#include <stdio.h>
#include <stdlib.h>

#include <gphoto2.h>

int gp_interface_status (char *status) {

	printf("Status: %s\n", status);
	return (GP_OK);
}

int gp_interface_message (char *message) {

	printf("Message: %s\n", message);
	return (GP_OK);
}

int gp_interface_confirm (char *message) {

	char c;

	printf("%s [y/n] ", message);
	fflush(stdout);
	c = getchar();
	fflush(stdin);
	return ((c=='y') || (c=='Y'));
}

int gp_interface_progress (float percentage) {

	if (percentage >= 0) {
		printf("Percent completed: %02.01f\r", percentage*100);
		fflush(stdout);
	}
	return (GP_OK);
}
