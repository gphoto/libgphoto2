#include <stdio.h>
#include <stdlib.h>

#include <gphoto2.h>

int interface_update_status (char *status) {

	printf("Status: %s\n", status);
	return (GP_OK);
}

int interface_message (char *message) {

	printf("Message: %s\n", message);
	return (GP_OK);
}

int interface_confirm (char *message) {

	char c;

	printf("%s [y/n] ", message);
	fflush(stdout);
	c = getchar();
	fflush(stdin);
	return ((c=='y') || (c=='Y'));
}

int interface_update_progress (float percentage) {

	if (percentage >= 0)
		printf("Percent completed: %f\n", percentage);
	return (GP_OK);
}
