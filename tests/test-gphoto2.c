#include <gphoto2.h>
#include <stdio.h>

#define CHECK(f) {int res = f; if (res < 0) {printf ("ERROR: %s\n", gp_result_as_string (res)); exit (1);}}

int
main (int argc, char *argv [])
{
	CameraText text;
	Camera *camera;

	printf ("Initializing gphoto2...\n");
	CHECK (gp_init (GP_DEBUG_NONE));

	printf ("Creating camera...\n");
	CHECK (gp_camera_new (&camera));

	strcpy (camera->model, "Directory Browse");

	printf ("Initializing camera...\n");
	CHECK (gp_camera_init (camera));

	printf ("Getting summary...\n");
	CHECK (gp_camera_get_summary (camera, &text));
	printf ("%s\n", text.text);

	printf ("Unrefing camera...\n");
	CHECK (gp_camera_unref (camera));

	return (0);
}
