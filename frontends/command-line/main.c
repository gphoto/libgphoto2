#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2.h>

#define DEBUG

int main (int argc, char **argv) {

	CameraAbilities a;
	CameraPortSettings s;
	CameraFile *f;
	char buf[1024];
	int x, m, n, camnum;

	/* ----------------------------- */
	/* Initialize the gPhoto library */
	/* ----------------------------- */
	gp_init();

	strcpy(s.serial_port, "/dev/ttyS0");
	s.serial_baud = 57600;
	gp_port_set(s);

	/* ---------------------------------------- */
	/* Retrieve the number of cameras available */
	/* ---------------------------------------- */
	if ((n = gp_camera_count())==GP_ERROR)
		printf("cli: camera_count error!\n");
#ifdef DEBUG
	printf("cli: Number of cameras: %i\n", n);
#endif
	/* ----------------------------------- */
	/* Display a list of available cameras */
	/* ----------------------------------- */
	for (x=0; x<n; x++) {
		if (gp_camera_name(x, buf)==GP_ERROR)
			printf("cli: (ERROR) camera_name error! (%i)\n", x);

#ifdef DEBUG
		printf("cli: Camera #%i name: %s\n", x, buf);
#endif
	}

	/* -------------------------- */
	/* Choose which camera to use */
	/* -------------------------- */
	printf("Camera number to use? ");
	fflush(stdout);
	fgets(buf, 1023, stdin);
	camnum = atoi(buf);
#ifdef DEBUG
	printf("cli: Setting camera #%i active\n", camnum);
#endif
	/* ---------------------------------------------- */
	/* Set the camera they chose as the active camera */
	/* ---------------------------------------------- */
	if (gp_camera_set(camnum) == GP_ERROR)
		printf("cli: (ERROR) camera_set error!\n");

	/* ------------------------------- */
	/* Retrieve the camera's abilities */
	/* ------------------------------- */
	if (gp_camera_abilities(&a) == GP_ERROR)
		printf("cli: (ERROR) camera_abilities error!\n");
	
	/* --------------------------------------- */
	/* Get the number of folders on the camera */
	/* --------------------------------------- */
	n = gp_folder_count();
	if (n == GP_ERROR)
		printf("cli: (ERROR) folder_count error!\n");
#ifdef DEBUG
	printf("cli: Number of folders: %i\n", n);
#endif

	/* ------------------------------------ */
	/* Get the names of each of the folders */
	/* ------------------------------------ */
	for (x=0; x<n; x++) {
		gp_folder_name(x, buf);
#ifdef DEBUG
		printf("cli: Folder #%i name: %s\n", x, buf);
#endif
	}
	/* ------------------------------- */
	/* Set the currently active folder */
	/* ------------------------------- */
	if (gp_folder_set(0) == GP_ERROR)
		printf("Can't set folder!\n");

	/* ----------------------------------------- */
	/* Get the number of pictures in that folder */
	/* ----------------------------------------- */
	m=gp_file_count();
	if (m == GP_ERROR)
		printf("cli: (ERROR) file_count error!\n");
#ifdef DEBUG
	printf("cli: Number of files in folder #%i: %i\n", n, m);
#endif

	/* ------------------------------- */
	/* Get a thumbnail from the camera */
	/* ------------------------------- */
	f = gp_file_new();

	printf("cli: Getting preview 0\n");
	gp_file_get_preview(0, f);
	printf("cli: done!\n");

	gp_file_save_to_disk(f, "/home/scottf/test.jpg");

	/* ---------------------------------- */
	/* Display camera/library information */
	/* ---------------------------------- */
	if (gp_summary(buf) == GP_ERROR)
		printf("cli: (ERROR) summary error!\n");
	printf("cli: Summary:\n%s\n", buf);

	if (gp_manual(buf) == GP_ERROR)
		printf("cli: (ERROR) manual error!\n");
	printf("cli: Manual:\n%s\n", buf);

	if (gp_about(buf) == GP_ERROR)
		printf("cli: (ERROR) about error!\n");
	printf("cli: About:\n%s\n", buf);

	/* ---------------------------- */
	/* Close out the gPhoto library */
	/* ---------------------------- */	
	gp_exit();

	return (0);
}
