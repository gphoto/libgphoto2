#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gphoto2/gphoto2-camera.h>

#include "samples.h"

/* Set the owner of the first camera.
 *
 * This program can autodetect a single camera and then sets its
 * owner to the string passed on the cmdline.
 *
 * Same as:
 *	gphoto2 --get-config ownername
 *	gphoto2 --set-config ownername="Owner Name"
 */

int main(int argc, char **argv) {
	Camera		*camera;
	int		ret;
	char		*owner;
	GPContext	*context;

	context = sample_create_context (); /* see context.c */
	gp_camera_new (&camera);

	/* This call will autodetect cameras, take the
	 * first one from the list and use it. It will ignore
	 * any others... See the *multi* examples on how to
	 * detect and use more than the first one.
	 */
	ret = gp_camera_init (camera, context);
	if (ret < GP_OK) {
		printf("No camera auto detected.\n");
		gp_camera_free (camera);
		return 0;
	}

	ret = get_config_value_string (camera, "ownername", &owner, context);
	if (ret < GP_OK) {
		printf ("Could not query owner.\n");
		goto out;
	}
	printf("Current owner: %s\n", owner);
	if (argc > 1) {
		ret = set_config_value_string (camera, "ownername", argv[1], context);
		if (ret < GP_OK) {
			fprintf (stderr, "Failed to set camera owner to %s; %d\n", argv[1], ret);
		} else 
			printf("New owner: %s\n", argv[1]);
	}
out:
	gp_camera_exit (camera, context);
	gp_camera_free (camera);
	return 0;
}
