#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gphoto2/gphoto2-camera.h>

#include "samples.h"

/* Sample for AFL usage.
 *
 */

static void errordumper(GPLogLevel level, const char *domain, const char *str,
                 void *data) {
	/* Do not log ... but let it appear here so we discover debug paths */
	fprintf(stderr, "%s\n", str);
}


int main(int argc, char **argv) {
	Camera		*camera = NULL;
	int		ret;
	GPContext	*context;
	CameraWidget	*rootwidget;
	char		buf[200];
	CameraText	summary;

        gp_log_add_func(GP_LOG_DEBUG, errordumper, NULL);

	context = sample_create_context (); /* see context.c */

	strcpy(buf,"usb:");
	if (argc > 1)
		strcat (buf, argv[1]);

	fprintf(stderr,"setting path %s.\n", buf);

	ret = sample_open_camera (&camera, "USB PTP Class Camera", buf, context);
        if (ret < GP_OK) {
		fprintf(stderr,"camera %s not found.\n", buf);
		goto out;
	}

	ret = gp_camera_init (camera, context);
	if (ret < GP_OK) {
		fprintf(stderr,"No camera auto detected.\n");
		goto out;
	}

	/* AFL PART STARTS HERE */
	ret = gp_camera_get_summary (camera, &summary, context);
	if (ret < GP_OK) {
		printf ("Could not get summary.\n");
		goto out;
	}

#if 1
	ret = gp_camera_get_config (camera, &rootwidget, context);
	if (ret < GP_OK) {
		fprintf (stderr,"Could not get config.\n");
		goto out;
	}
#endif
	printf ("OK, %s\n", summary.text);
	while (1) {
		CameraEventType evttype;
		void *data = NULL;

		ret = gp_camera_wait_for_event(camera, 1, &evttype, &data, context);
		if (ret < GP_OK) break;
		if (data) free (data);
		if (evttype == GP_EVENT_TIMEOUT) break;
	}

	/* AFL PART ENDS HERE */
out:
	gp_camera_exit (camera, context);
	gp_camera_free (camera);
	return 0;
}
