#include <stdio.h>
#include <stdlib.h>
#include <gphoto2.h>
#include "util.h"

int gp_dump_abilities (CameraAbilities *abilities) {

	int x=0;

	printf("core: Abilities for \"%s\"\n", 
		abilities->model);
	printf("core: \tSerial support			: %s\n", 
		abilities->serial == 0? "no":"yes");
	if (abilities->serial == 1) {
		printf("core:\tSerial transfer speeds supported:\n");
		do {	
			printf("core:\t\t%i\n", abilities->serial_baud[x]);
			x++;
		} while (abilities->serial_baud[x]!=0);
	}
	printf("core: \tUSB support			: %s\n", 
		abilities->usb == 0? "no":"yes");
	printf("core: \tParallel support		: %s\n", 
		abilities->parallel == 0? "no":"yes");
	printf("core: \tIEEE1394 support		: %s\n", 
		abilities->ieee1394 == 0? "no":"yes");
	printf("core: \tCancel transfer support	   	: %s\n", 
		abilities->cancel == 0? "no":"yes");
	printf("core: \tCapture from computer support	: %s\n", 
		abilities->capture == 0? "no":"yes");
	printf("core: \tConfiguration  support		: %s\n", 
		abilities->config == 0? "no":"yes");
	printf("core: \tDelete files on camera support	: %s\n", 
		abilities->file_delete == 0? "no":"yes");
	printf("core: \tFile preview (thumnail) support	: %s\n", 
		abilities->file_preview == 0? "no":"yes");
	printf("core: \tFile upload support		: %s\n", 
		abilities->file_put == 0? "no":"yes");
	printf("core: \tProtect files support		: %s\n", 
		abilities->lock == 0? "no":"yes");
	printf("core: End of Abilities\n");

	return (GP_OK);
}
