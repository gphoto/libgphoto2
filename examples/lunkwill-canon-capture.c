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
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gphoto2/gphoto2.h>

#include "samples.h"

static void errordumper(GPLogLevel level, const char *domain, const char *str,
                 void *data) {
  struct timeval tv;

  gettimeofday (&tv, NULL);
  fprintf(stdout, "%d.%d: %s\n", (int)tv.tv_sec, (int)tv.tv_usec, str);
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

static void
capture_to_file(Camera *canon, GPContext *canoncontext, char *fn) {
	int fd, retval;
	CameraFile *canonfile;
	CameraFilePath camera_file_path;

	printf("Capturing.\n");

	/* NOP: This gets overridden in the library to /capt0000.jpg */
	strcpy(camera_file_path.folder, "/");
	strcpy(camera_file_path.name, "foo.jpg");

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

int
main(int argc, char **argv) {
	Camera	*canon;
	int	retval;
	GPContext *canoncontext = sample_create_context();

	gp_log_add_func(GP_LOG_DEBUG, errordumper, NULL);
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
	/*set_capturetarget(canon, canoncontext);*/
	capture_to_file(canon, canoncontext, "foo.jpg");
	capture_to_file(canon, canoncontext, "foo1.jpg");
	capture_to_file(canon, canoncontext, "foo2.jpg");
	capture_to_file(canon, canoncontext, "foo3.jpg");
	capture_to_file(canon, canoncontext, "foo4.jpg");
	capture_to_file(canon, canoncontext, "foo5.jpg");
	capture_to_file(canon, canoncontext, "foo6.jpg");
	gp_camera_exit(canon, canoncontext);
	return 0;
}
