#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2.h>

int test_gphoto() {
	/* This will let the user check to see the status of their
	   gPhoto installation and test to make sure everything is
	   in working order */

	return (GP_OK);
}

#if 0
	/* Example code for using gPhoto2 */

	CameraAbilities a;
	CameraPortSettings s;
	CameraFile *f;
	char buf[1024];
	int x, m, n, camnum;

	/* ----------------------------- */
	/* Initialize the gPhoto library */
	/* ----------------------------- */
	gp_init(0);

	/* ---------------------------------------- */
	/* Retrieve the number of cameras available */
	/* ---------------------------------------- */
	if ((n = gp_camera_count())==GP_ERROR)
		printf("cli: camera_count error!\n");
	printf("cli: Number of cameras: %i\n", n);
	/* ----------------------------------- */
	/* Display a list of available cameras */
	/* ----------------------------------- */
	for (x=0; x<n; x++) {
		if (gp_camera_name(x, buf)==GP_ERROR)
			printf("cli: ERROR: camera_name error! (%i)\n", x);

		printf("cli: Camera #%i name: %s\n", x, buf);
	}

	/* -------------------------- */
	/* Choose which camera to use */
	/* -------------------------- */
	printf("Camera number to use? ");
	fflush(stdout);
	fgets(buf, 1023, stdin);
	camnum = atoi(buf);
	printf("cli: Setting camera #%i active\n", camnum);

	/* ---------------------------------------------- */
	/* Set the camera they chose as the active camera */
	/* ---------------------------------------------- */
	strcpy(s.path, "/dev/ttyS0");
	s.speed = 57600;
	if (gp_camera_set(camnum, &s) == GP_ERROR)
		printf("cli: ERROR: camera_set error!\n");

	/* ------------------------------- */
	/* Retrieve the camera's abilities */
	/* ------------------------------- */
	if (gp_camera_abilities(camnum, &a) == GP_ERROR)
		printf("cli: ERROR: camera_abilities error!\n");
	
	/* ------------------------------- */
	/* Set the currently active folder */
	/* ------------------------------- */
	if (gp_folder_set("/") == GP_ERROR)
		printf("cli: ERROR: Can't set folder!\n");

	/* ----------------------------------------- */
	/* Get the number of pictures in that folder */
	/* ----------------------------------------- */
	m=gp_file_count();
	if (m == GP_ERROR)
		printf("cli: ERROR: file_count error!\n");
	   else
		printf("cli: Number of files: %i\n", n);

	/* ------------------------------- */
	/* Get a thumbnail from the camera */
	/* ------------------------------- */
	f = gp_file_new();

	printf("cli: Getting preview 0\n");
	if (gp_file_get_preview(0, f) == GP_ERROR)
		printf("cli: ERROR: get_preview error!\n");
	   else {
		printf("cli: done!\n");
		gp_file_save(f, "/home/scottf/test.jpg");
	}

	/* ---------------------------------- */
	/* Display camera/library information */
	/* ---------------------------------- */
	if (gp_summary(buf) == GP_ERROR)
		printf("cli: ERROR: summary error!\n");
	   else
		printf("cli: Summary:\n%s\n", buf);

	if (gp_manual(buf) == GP_ERROR)
		printf("cli: ERROR: manual error!\n");
	   else
		printf("cli: Manual:\n%s\n", buf);

	if (gp_about(buf) == GP_ERROR)
		printf("cli: ERROR: about error!\n");
	   else
		printf("cli: About:\n%s\n", buf);

	/* ---------------------------- */
	/* Close out the gPhoto library */
	/* ---------------------------- */	
	gp_exit();

	return (0);
#endif
