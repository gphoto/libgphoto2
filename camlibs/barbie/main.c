#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "barbie.h"

char serial_port[32];

int main (int argc, char *argv[]) {

	int size, x;
	char *s;
	char fname[16];
	FILE *fp;
	sprintf(serial_port, "/dev/ttyS0");

	if (barbie_initialize() != 1)
		printf("initialization failed\n");
	   else
		printf("initialization succeeded\n");

	for (x=3; x<=3; x++) {
		s = barbie_read_picture(x, 0,&size);
		if (s) {
			sprintf(fname, "temp-%i.ppm", x);
			fp = fopen(fname, "w");
			fwrite(s, size, 1, fp);
			fclose(fp);
			free(s);
		} else {
			printf("error!\n");
		}
	}


/* display number of pictures
	printf("%i\n", barbie_number_of_pictures());
*/

/* take a picture
	printf("%i\n", barbie_take_picture());
*/

/* firmware revision
	s = barbie_read_firmware();
	if (s) {
		printf("%s\n", s);
		free(s);
	}  else {
		printf("0\n");
	}
*/
	return (0);
}
