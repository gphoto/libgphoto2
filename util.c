#include <stdio.h>
#include <stdlib.h>
#include <gphoto2.h>
#include "util.h"

int gp_dump_abilities (CameraAbilities *abilities) {

	int x=0;

	printf("core: Abilities for camera                  : %s\n", 
		abilities->model);
	printf("core: Serial port support                   : %s\n", 
		abilities->serial == 0? "no":"yes");
	printf("core: Parallel port support                 : %s\n", 
		abilities->parallel == 0? "no":"yes");
	printf("core: USB support                           : %s\n", 
		abilities->usb == 0? "no":"yes");
	printf("core: IEEE1394 support                      : %s\n", 
		abilities->ieee1394 == 0? "no":"yes");
	if (abilities->speed[0] != 0) {
	printf("core: Transfer speeds supported             :\n");
		do {	
	printf("core:                                       : %i\n", abilities->speed[x]);
			x++;
		} while (abilities->speed[x]!=0);
	}
	printf("core: Capture from computer support         : %s\n", 
		abilities->capture == 0? "no":"yes");
	printf("core: Configuration  support                : %s\n", 
		abilities->config == 0? "no":"yes");
	printf("core: Delete files on camera support        : %s\n", 
		abilities->file_delete == 0? "no":"yes");
	printf("core: File preview (thumnail) support       : %s\n", 
		abilities->file_preview == 0? "no":"yes");
	printf("core: File upload support                   : %s\n", 
		abilities->file_put == 0? "no":"yes");

	return (GP_OK);
}
