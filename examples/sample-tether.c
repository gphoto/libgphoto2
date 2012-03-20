/* 
 * This program does the equivalent of:
 * gphoto2 --wait-event-and-download
 *
 * compile with: gcc -Wall -o sample-tether sample-tether.c context.c -lgphoto2
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

static void errordumper(GPLogLevel level, const char *domain, const char *str,
                 void *data) {
  fprintf(stdout, "%s\n", str);
}

static void
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
			break;
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
			break;
		case GP_EVENT_UNKNOWN:
			if (evtdata) {
				printf("Unknown event: %s.\n", (char*)evtdata);
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
	int	retval;
	GPContext *context = sample_create_context();

	gp_log_add_func(GP_LOG_ERROR, errordumper, NULL);
	gp_camera_new(&camera);

	/* When I set GP_LOG_DEBUG instead of GP_LOG_ERROR above, I noticed that the
	 * init function seems to traverse the entire filesystem on the camera.  This
	 * is partly why it takes so long.
	 * (Marcus: the ptp2 driver does this by default currently.)
	 */
	printf("Camera init.  Takes about 10 seconds.\n");
	retval = gp_camera_init(camera, context);
	if (retval != GP_OK) {
		printf("  Retval: %d\n", retval);
		exit (1);
	}

	camera_tether(camera, context);

	gp_camera_exit(camera, context);
	return 0;
}
