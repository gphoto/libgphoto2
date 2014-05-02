/* 
 * This program tries various ISOs for best ISO with a shutterspeed limit
 *
 * compile with: gcc -Wall -o best-iso best-iso.c context.c config.c -lgphoto2 -lgphoto2_port
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gphoto2/gphoto2.h>

#include "samples.h"

static int aperture;
static float shutterspeed;

static void errordumper(GPLogLevel level, const char *domain, const char *str,
                 void *data) {
  fprintf(stdout, "%s\n", str);
}

static int
camera_tether(Camera *camera, GPContext *context) {
	int fd, retval;
	CameraFile *file;
	CameraEventType	evttype;
	CameraFilePath	*path;
	void	*evtdata;

	printf("Tethering...\n");

	while (1) {
		retval = gp_camera_wait_for_event (camera, 1000, &evttype, &evtdata, context);
		if (retval != GP_OK)
			return retval;
		switch (evttype) {
		case GP_EVENT_FILE_ADDED:
			path = (CameraFilePath*)evtdata;
			printf("File added on the camera: %s/%s\n", path->folder, path->name);

			fd = open(path->name, O_CREAT | O_WRONLY, 0644);
			retval = gp_file_new_from_fd(&file, fd);
			printf("  Downloading %s...\n", path->name);
			retval = gp_camera_file_get(camera, path->folder, path->name,
				     GP_FILE_TYPE_NORMAL, file, context);

			printf("  Deleting %s on camera...\n", path->name);
			retval = gp_camera_file_delete(camera, path->folder, path->name, context);
			gp_file_free(file);
			break;
		case GP_EVENT_FOLDER_ADDED:
			path = (CameraFilePath*)evtdata;
			printf("Folder added on camera: %s / %s\n", path->folder, path->name);
			break;
		case GP_EVENT_CAPTURE_COMPLETE:
			printf("Capture Complete.\n");
			break;
		case GP_EVENT_TIMEOUT:
			printf("Timeout.\n");
			return GP_OK;
		case GP_EVENT_UNKNOWN:
			if (evtdata) {
				char *val;
				int ret;

				printf("Unknown event: %s.\n", (char*)evtdata);
				/* 3 eos properties hardcoded */
				if (strstr(evtdata,"d103")) {
					ret = get_config_value_string(camera,"iso",&val,context);
					if (ret == GP_OK)
						printf("ISO is %s\n", val);
				}
				if (strstr(evtdata,"d102")) {
					ret = get_config_value_string(camera,"shutterspeed",&val,context);
					if (ret == GP_OK) {
						if (strchr(val,'/')) {
							int zaehler,nenner;
							sscanf(val,"%d/%d",&zaehler,&nenner);
							shutterspeed = 1.0*zaehler/nenner;
						} else {
							sscanf(val,"%g",&shutterspeed);
						}
						printf("Shutterspeed is %s (%g)\n", val, shutterspeed);
					}
				}
				if (strstr(evtdata,"d101")) {
					ret = get_config_value_string(camera,"aperture",&val,context);
					if (ret == GP_OK) {
						float ap;

						sscanf(val,"%g",&ap);
						aperture = 10*ap;
						printf("Aperture is %s (%d)\n", val, aperture);
					}
				}
			} else {
				printf("Unknown event.\n");
			}
			break;
		default:
			printf("Type %d?\n", evttype);
			break;
		}
	}
}

int
main(int argc, char **argv) {
	Camera	*camera;
	int	retval, iso;
	char	buf[20];
	GPContext *context = sample_create_context();

	gp_log_add_func(GP_LOG_ERROR, errordumper, NULL);
	gp_camera_new(&camera);

	retval = gp_camera_init(camera, context);
	if (retval != GP_OK) {
		printf("  camera failed to initialize: %d\n", retval);
		exit (1);
	}

	retval = camera_tether(camera, context);
	if (retval != GP_OK) {
		printf("Tether error: %d\n", retval);
		exit (1);
	}

	iso = 100;

	shutterspeed = 0.0;
	aperture = 0;

	while (1) {
		sprintf(buf,"%d",iso);
		printf("Setting ISO to %d\n",iso);

		retval = set_config_value_string(camera, "iso", buf, context);
		if (retval != GP_OK) {
			printf("  failed setting ISO to %d: %d\n", iso, retval);

			/* usually also happens if too dark */
			exit (1);
		}

		printf("eosremoterelease half press\n");
		retval = set_config_value_string(camera,"eosremoterelease", "Press Half", context);
		if (retval != GP_OK) {
			printf("  failed pressing shutter to half: %d\n", retval);
			exit (1);
		}

		retval = camera_tether(camera, context);
		if (retval != GP_OK) {
			printf("Tether error: %d\n", retval);
			exit (1);
		}

		printf("eosremoterelease release\n");
		retval = set_config_value_string(camera,"eosremoterelease", "Release Half", context);
		if (retval != GP_OK) {
			printf("  failed releasing shutter button from half: %d\n", retval);
			exit (1);
		}

		if (shutterspeed < 30) {
			printf("ISO %d has Aperture %g and Shutterspeed %g\n", iso, aperture/10.0, shutterspeed);
			/*break;*/
		}
		iso = iso*2;
	}

	gp_camera_exit(camera, context);
	return 0;
}
