#include <stdio.h>
#include <stdlib.h>

#include <gphoto2.h>

extern int glob_stdout;
extern int glob_quiet;

int gp_interface_status (Camera *camera, char *status) {

	if (glob_quiet)
		return (GP_OK);

	printf("Status: %s\n", status);
	return (GP_OK);
}

int gp_interface_message (Camera *camera, char *message) {

	if (glob_quiet)
		return (GP_OK);

	printf("Message: %s\n", message);
	return (GP_OK);
}

int gp_interface_confirm (Camera *camera, char *message) {

	char c[1024];

	if (glob_quiet)
		return (GP_CONFIRM_YES);	/* dangerous? */

	do {
		printf("%s [y/n] ", message);
		fflush(stdout);
		fgets(c, 1023, stdin);
	} while ((c[0]!='y')&&(c[0]!='Y')&&(c[0]!='n')&&(c[0]!='N'));

	return ( ((c[0]=='y') || (c[0]=='Y'))? GP_CONFIRM_YES: GP_CONFIRM_NO);
}

int gp_interface_progress (Camera *camera, CameraFile *file, float percentage) {

	if (glob_quiet)
		return GP_OK;

	if (percentage >= 0) {
		printf("Percent completed: %02.01f\r", percentage);
		fflush(stdout);
	}
	return (GP_OK);
}
