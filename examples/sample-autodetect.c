#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2-camera.h>

#include "samples.h"

/* Sample autodetection program.
 *
 * This program can autodetect multiple cameras and then calls a
 * simple function in each of them (summary).
 */

int main(int argc, char **argv) {
	CameraList	*list;
	Camera		**cams;
	int		ret, i, count;
	const char	*name, *value;
	GPContext	*context;

	context = sample_create_context (); /* see context.c */

	/* Detect all the cameras that can be autodetected... */
	ret = gp_list_new (&list);
	if (ret < GP_OK) return 1;
	count = sample_autodetect (list, context);

	/* Now open all cameras we autodected for usage */
	printf("Number of cameras: %d\n", count);
	cams = calloc (sizeof (Camera*),count);
        for (i = 0; i < count; i++) {
                gp_list_get_name  (list, i, &name);
                gp_list_get_value (list, i, &value);
		ret = sample_open_camera (&cams[i], name, value);
		if (ret < GP_OK) fprintf(stderr,"Camera %s on port %s failed to open\n", name, value);
        }
	/* Now call a simple function in each of those cameras. */
	for (i = 0; i < count; i++) {
		CameraText	text;
	        ret = gp_camera_get_summary (cams[i], &text, context);
		if (ret < GP_OK) {
			fprintf (stderr, "Failed to get summary.\n");
			continue;
		}

                gp_list_get_name  (list, i, &name);
                gp_list_get_value (list, i, &value);
                printf("%-30s %-16s\n", name, value);
		printf("Summary:\n%s\n", text.text);
	}
	return 0;
}
