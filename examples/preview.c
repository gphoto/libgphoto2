/* compile with gcc -Wall -o canon-capture -lgphoto2 canon-capture.c
 * This code released into the public domain 21 July 2008
 * 
 * This program does the equivalent of:
 * gphoto2 --shell
 *   > set-config capture=1
 *   > capture-image-and-download
 * compile with gcc -Wall -o canon-capture -lgphoto2 canon-capture.c
 *
 * Taken from: http://credentiality2.blogspot.com/2008/07/linux-libgphoto2-image-capture-from.html 
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

static void errordumper(GPLogLevel level, const char *domain, const char *str,
                 void *data) {
  printf("%s (data %p)\n", str,data);
}

/* This seems to have no effect on where images go
void set_capturetarget(Camera *canon, GPContext *canoncontext) {
	int retval;
	printf("Get root config.\n");
	CameraWidget *rootconfig; // okay, not really
	CameraWidget *actualrootconfig;

	retval = gp_camera_get_config(canon, &rootconfig, canoncontext);
	actualrootconfig = rootconfig;
	printf("  Retval: %d\n", retval);

	printf("Get main config.\n");
	CameraWidget *child;
	retval = gp_widget_get_child_by_name(rootconfig, "main", &child);
	printf("  Retval: %d\n", retval);

	printf("Get settings config.\n");
	rootconfig = child;
	retval = gp_widget_get_child_by_name(rootconfig, "settings", &child);
	printf("  Retval: %d\n", retval);

	printf("Get capturetarget.\n");
	rootconfig = child;
	retval = gp_widget_get_child_by_name(rootconfig, "capturetarget", &child);
	printf("  Retval: %d\n", retval);


	CameraWidget *capture = child;

	const char *widgetinfo;
	gp_widget_get_name(capture, &widgetinfo);
	printf("config name: %s\n", widgetinfo );

	const char *widgetlabel;
	gp_widget_get_label(capture, &widgetlabel);
	printf("config label: %s\n", widgetlabel);

	int widgetid;
	gp_widget_get_id(capture, &widgetid);
	printf("config id: %d\n", widgetid);

	CameraWidgetType widgettype;
	gp_widget_get_type(capture, &widgettype);
	printf("config type: %d == %d \n", widgettype, GP_WIDGET_RADIO);


	printf("Set value.\n");

	// capture to ram should be 0, although I think the filename also plays into
	// it
	int one=1;
	retval = gp_widget_set_value(capture, &one);
	printf("  Retval: %d\n", retval);

	printf("Enabling capture to CF.\n");
	retval = gp_camera_set_config(canon, actualrootconfig, canoncontext);
	printf("  Retval: %d\n", retval);
}
*/

#if 0
static void
capture_to_file(Camera *canon, GPContext *canoncontext, char *fn) {
	int fd, retval;
	CameraFile *canonfile;
	CameraFilePath camera_file_path;

	printf("Capturing.\n");

	retval = gp_camera_capture(canon, GP_CAPTURE_IMAGE, &camera_file_path, canoncontext);
	printf("  Retval: %d\n", retval);

	printf("Pathname on the camera: %s/%s\n", camera_file_path.folder, camera_file_path.name);

	fd = open(fn, O_CREAT | O_WRONLY, 0644);
	retval = gp_file_new_from_fd(&canonfile, fd);
	printf("  Retval: %d\n", retval);
	retval = gp_camera_file_get(canon, camera_file_path.folder, camera_file_path.name,
		     GP_FILE_TYPE_NORMAL, canonfile, canoncontext);
	printf("  Retval: %d\n", retval);

	printf("Deleting.\n");
	retval = gp_camera_file_delete(canon, camera_file_path.folder, camera_file_path.name,
			canoncontext);
	printf("  Retval: %d\n", retval);

	gp_file_free(canonfile);
}
#endif

int
main(int argc, char **argv) {
	Camera	*canon;
	int	i, retval;
	GPContext *canoncontext = sample_create_context();

	gp_log_add_func(GP_LOG_ERROR, errordumper, 0);
	gp_camera_new(&canon);

	/* When I set GP_LOG_DEBUG instead of GP_LOG_ERROR above, I noticed that the
	 * init function seems to traverse the entire filesystem on the camera.  This
	 * is partly why it takes so long.
	 * (Marcus: the ptp2 driver does this by default currently.)
	 */
	printf("Camera init.  Takes about 10 seconds.\n");
	retval = gp_camera_init(canon, canoncontext);
	if (retval != GP_OK) {
		printf("  Retval: %d\n", retval);
		exit (1);
	}
	canon_enable_capture(canon, TRUE, canoncontext);
	retval = camera_eosviewfinder(canon,canoncontext,1);
	if (retval != GP_OK) {
		fprintf(stderr,"camera_eosviewfinder(1): %d\n", retval);
		exit(1);
	}
	/*set_capturetarget(canon, canoncontext);*/
	printf("Taking 100 previews and saving them to snapshot-XXX.jpg ...\n");
	for (i=0;i<100;i++) {
		CameraFile *file;
		char output_file[32];

		fprintf(stderr,"preview %d\n", i);
		retval = gp_file_new(&file);
		if (retval != GP_OK) {
			fprintf(stderr,"gp_file_new: %d\n", retval);
			exit(1);
		}

		/* autofocus every 10 shots */
		if (i%10 == 9) {
			camera_auto_focus (canon, canoncontext, 1);
			/* FIXME: wait a bit and/or poll events ? */
			camera_auto_focus (canon, canoncontext, 0);
		} else {
			camera_manual_focus (canon, (i/10-5)/2, canoncontext);
		}
#if 0 /* testcase for EOS zooming */
		{
			char buf[20];
			if (i<10) set_config_value_string (canon, "eoszoom", "5", canoncontext);
			sprintf(buf,"%d,%d",(i&0x1f)*64,(i>>5)*64);
			fprintf(stderr, "%d - %s\n", i, buf);
			set_config_value_string (canon, "eoszoomposition", buf, canoncontext);
		}
#endif
		retval = gp_camera_capture_preview(canon, file, canoncontext);
		if (retval != GP_OK) {
			fprintf(stderr,"gp_camera_capture_preview(%d): %d\n", i, retval);
			exit(1);
		}
		sprintf(output_file, "snapshot-%03d.jpg", i);
		retval = gp_file_save(file, output_file);
		if (retval != GP_OK) {
			fprintf(stderr,"gp_camera_capture_preview(%d): %d\n", i, retval);
			exit(1);
		}
		gp_file_unref(file);
	/*
		sprintf(output_file, "image-%03d.jpg", i);
	        capture_to_file(canon, canoncontext, output_file);
	*/
	}
	retval = camera_eosviewfinder(canon,canoncontext,0);
	if (retval != GP_OK) {
		fprintf(stderr,"camera_eosviewfinder(0): %d\n", retval);
		exit(1);
	}

	sleep(10);
	gp_camera_exit(canon, canoncontext);
	return 0;
}
