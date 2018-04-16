/* compile with gcc -Wall -o sample-photobooth -lgphoto2 sample-photobooth.c
 * 
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <gphoto2/gphoto2.h>

#include "samples.h"

#define CONFIG_FILE	"config.txt"
#define PREVIEW		"preview.jpg"

static void errordumper(GPLogLevel level, const char *domain, const char *str, void *data) {
/*	printf("%s (data %p)\n", str,data);*/
}

/* set by signal handler */
static int capture_now = 0;
static int read_config = 0;

static int
capture_to_file(Camera *camera, GPContext *context, char *fn) {
	int fd, retval;
	CameraFile *file;
	CameraFilePath camera_file_path;
	char	*s, *t;

	retval = gp_camera_capture(camera, GP_CAPTURE_IMAGE, &camera_file_path, context);
	if (retval < GP_OK) return retval;

	s = strrchr (fn, '.');
	t = strrchr (camera_file_path.name, '.');
	/* replace the suffix by the one sent by the camera .. for RAW capture */
	if (t && s && (strlen(t+1) == 3) && (strlen(s+1) == 3)) {
		strcpy (s+1, t+1);
	}

	fd = open (fn, O_CREAT | O_WRONLY | O_BINARY, 0644);
	if (fd == -1)
		return GP_ERROR;

	retval = gp_file_new_from_fd(&file, fd);
	if (retval < GP_OK) {
		close(fd);
		unlink(fn);
		return retval;
	}

	retval = gp_camera_file_get(camera, camera_file_path.folder, camera_file_path.name,
		     GP_FILE_TYPE_NORMAL, file, context);
	if (retval < GP_OK) {
		gp_file_free(file);
		unlink(fn);
		return retval;
	}

	retval = gp_camera_file_delete(camera, camera_file_path.folder, camera_file_path.name, context);
	gp_file_free(file);
	return GP_OK;
}

static void
sig_handler_capture_now (int sig_num)
{
#if !defined (WIN32)
	signal (SIGUSR1, sig_handler_capture_now);
#endif
	capture_now = 1;
}

static void
sig_handler_read_config (int sig_num)
{
#if !defined (WIN32)
	signal (SIGUSR2, sig_handler_read_config);
#endif
	read_config = 1;
}

int
main(int argc, char **argv) {
	Camera	*camera;
	int	retval;
	int	capturecnt = 0;
	GPContext *context = sample_create_context();

	printf("Sample photobooth.\n");
	printf("Continously stores previews in 'preview.jpg'.\n");
	printf("kill -USR1 %d to take a capture.\n", getpid());
	printf("kill -USR2 %d to read the 'config.txt'.\n", getpid());
	printf("kill -TERM %d to finish.\n", getpid());

#if !defined (WIN32)
	signal (SIGUSR1, sig_handler_capture_now);
	signal (SIGUSR2, sig_handler_read_config);
#endif

	gp_log_add_func(GP_LOG_ERROR, errordumper, 0);
	gp_camera_new(&camera);

	retval = gp_camera_init(camera, context);
	if (retval != GP_OK) {
		printf("  Retval: %d\n", retval);
		exit (1);
	}

	while (1) {
		CameraFile *file;
		char output_file[32];

		/*
		 * Capture a full picture on demand. Use unique filenames.
		 */
		if (capture_now) {
			capture_now = 0;
			sprintf(output_file, "image-%04d.jpg", capturecnt++);
			retval = capture_to_file(camera, context, output_file);
			if (retval == GP_OK)
				fprintf (stderr, "captured to %s\n", output_file);
		}

		/*
		 * Read configuration changes from "config.txt".
		 * Expected are key=value pairs where value is the value seen in "--get-config key", e.g.
		 * 	iso=Auto
		 * 	iso=200
		 * 	shutterspeed=1/200
		 */
		if (read_config) {
			FILE	*config;
			char	buf[512];

			read_config = 0;

			config = fopen (CONFIG_FILE, "r");
			while (fgets (buf, sizeof(buf), config)) {
				char	*s;

				/* kill linefeeds */
				s = strchr(buf,'\r');
				if (s)
					*s=0;
				s = strchr(buf,'\n');
				if (s)
					*s=0;

				s = strchr(buf,'=');
				if (!s) continue;

				*s=0;
				retval = set_config_value_string (camera, buf, s+1, context);
				if (retval < GP_OK)
					fprintf (stderr, "setting configuration '%s' to '%s' failed with %d.\n", buf, s+1, retval);
				else
					fprintf (stderr, "changed configuration '%s' to '%s'\n", buf, s+1);
			}
			fclose (config);
		}

		/*
		 * Capture a preview on every loop. Save as preview.jpg.
		 */
		retval = gp_file_new(&file);
		if (retval != GP_OK) {
			fprintf(stderr,"gp_file_new: %d\n", retval);
			exit(1);
		}

		retval = gp_camera_capture_preview(camera, file, context);
		if (retval != GP_OK) {
			fprintf(stderr,"gp_camera_capture_preview failed: %d\n", retval);
			exit(1);
		}
		retval = gp_file_save(file, PREVIEW);
		if (retval != GP_OK) {
			fprintf(stderr,"saving preview failed: %d\n", retval);
			exit(1);
		}
		gp_file_unref(file);
		/*
		 * Check and drain the event queue.
		 * 	Download newly captured images.
		 *	Ignore the rest events.
		 */
		while (1) {
			CameraEventType	evttype;
			CameraFilePath	*path;
			void    	*evtdata;
			int		fd;

			evtdata = NULL;
			retval = gp_camera_wait_for_event (camera, 1, &evttype, &evtdata, context);
			if (retval != GP_OK) break;
			switch (evttype) {
			case GP_EVENT_FILE_ADDED: {
				char	*t;

				path = (CameraFilePath*)evtdata;
				t = strrchr (path->name, '.');
				if (t && (strlen(t+1) == 3)) {
					sprintf(output_file, "image-%04d.%s", capturecnt++, t+1);
				} else {
					sprintf(output_file, "image-%04d.jpg", capturecnt++);
				}

				fd = open (output_file, O_CREAT | O_WRONLY | O_BINARY, 0644);
				retval = gp_file_new_from_fd(&file, fd);
				retval = gp_camera_file_get(camera, path->folder, path->name,
					     GP_FILE_TYPE_NORMAL, file, context);

				retval = gp_camera_file_delete(camera, path->folder, path->name, context);
				gp_file_free(file);
				free (evtdata);
				fprintf (stderr, "saved to %s\n", output_file);
				break;
			}
			case GP_EVENT_FILE_CHANGED:
				path = (CameraFilePath*)evtdata;
				printf("File changed on camera: %s / %s\n", path->folder, path->name);
				free (evtdata);
				break;
			case GP_EVENT_FOLDER_ADDED:
				path = (CameraFilePath*)evtdata;
				printf("Folder added on camera: %s / %s\n", path->folder, path->name);
				free (evtdata);
				break;
			case GP_EVENT_CAPTURE_COMPLETE:
				printf("Capture Complete.\n");
				break;
			case GP_EVENT_TIMEOUT:
				break;
			case GP_EVENT_UNKNOWN:
				if (evtdata) {
					printf("Unknown event: %s.\n", (char*)evtdata);
					free (evtdata);
				} else {
					printf("Unknown event.\n");
				}
				break;
			default:
				printf("Type %d?\n", evttype);
				break;
			}
			if (evttype == GP_EVENT_TIMEOUT)
				break;
		}
	}
	gp_camera_exit(camera, context);
	return 0;
}
