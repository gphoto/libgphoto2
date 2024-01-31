#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
	if (count < GP_OK) {
		printf("No cameras detected.\n");
		return 1;
	}

	/* Now open all cameras we autodected for usage */
	printf("Number of cameras: %d\n", count);
	cams = calloc (sizeof (Camera*),count);
        for (i = 0; i < count; i++) {
                gp_list_get_name  (list, i, &name);
                gp_list_get_value (list, i, &value);
		ret = sample_open_camera (&cams[i], name, value, context);
		if (ret < GP_OK) fprintf(stderr,"Camera %s on port %s failed to open\n", name, value);
        }
	/* Now call a simple function in each of those cameras. */
	for (i = 0; i < count; i++) {
		CameraText	text;
		char 		*owner;
	        ret = gp_camera_get_summary (cams[i], &text, context);
		if (ret < GP_OK) {
			fprintf (stderr, "Failed to get summary.\n");
			continue;
		}

                gp_list_get_name  (list, i, &name);
                gp_list_get_value (list, i, &value);
                printf("%-30s %-16s\n", name, value);
		printf("Summary:\n%s\n", text.text);

		/* Query a simple string configuration variable. */
		ret = get_config_value_string (cams[i], "owner", &owner, context);
		if (ret >= GP_OK) {
			printf("Owner: %s\n", owner);
			free (owner);
		} else {
			printf("Owner: No owner found.\n");
		}

	}
	return 0;
}
