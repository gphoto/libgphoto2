#include <stdio.h>
#include <stdlib.h>
#include <gphoto2.h>
#include "util.h"

int gp_dump_abilities (CameraAbilities *abilities) {

	int x=0;
	char buf[32];

	printf("Abilities for camera:                   : %s\n", 
		abilities->model);
	switch (abilities->port_type) {
		case GP_PORT_SERIAL:
			strcpy(buf, "serial");
			break;
		case GP_PORT_PARALLEL:
			strcpy(buf, "parallel");
			break;
		case GP_PORT_USB:
			strcpy(buf, "usb");
			break;
		case GP_PORT_IEEE1394:
			strcpy(buf, "ieee1394");
			break;
		case GP_PORT_IRDA:
			strcpy(buf, "irda");
			break;
		case GP_PORT_SOCKET:
			strcpy(buf, "socket");
			break;
		default:
			strcpy(buf, "none");
			break;
	}
	printf("Connection type:                      : %s\n", buf);

	if (abilities->speed[0] != 0) {
	printf("Transfer speeds supported             :\n");
		do {	
	printf("                                        :%i\n", abilities->speed[x]);
			x++;
		} while (abilities->speed[x]!=0);
	}
	printf("Capture from computer support         : %s\n", 
		abilities->capture == 0? "no":"yes");
	printf("Configuration  support                : %s\n", 
		abilities->config == 0? "no":"yes");
	printf("Delete files on camera support        : %s\n", 
		abilities->file_delete == 0? "no":"yes");
	printf("File preview (thumnail) support       : %s\n", 
		abilities->file_preview == 0? "no":"yes");
	printf("File upload support                   : %s\n", 
		abilities->file_put == 0? "no":"yes");

	return (GP_OK);
}
