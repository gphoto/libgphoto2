#include <stdio.h>
#include <stdlib.h>

#include <gphoto2.h>

extern int glob_stdout;

int gp_interface_status (Camera *camera, char *status) {

	printf("Status: %s\n", status);
	return (GP_OK);
}

int gp_interface_message (Camera *camera, char *message) {

	printf("Message: %s\n", message);
	return (GP_OK);
}

int gp_interface_confirm (Camera *camera, char *message) {

	char c;

	printf("%s [y/n] ", message);
	fflush(stdout);
	c = getchar();
	fflush(stdin);
	return ((c=='y') || (c=='Y'));
}

int gp_interface_progress (Camera *camera, CameraFile *file, float percentage) {

	if (glob_stdout)
		return GP_OK;

	if (percentage >= 0) {
		printf("Percent completed: %02.01f\r", percentage);
		fflush(stdout);
	}
	return (GP_OK);
}
