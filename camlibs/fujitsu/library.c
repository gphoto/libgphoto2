#include <string.h>
#include <gphoto2.h>
#include <gpio/gpio.h>

#include "fujitsu.h"

int 		glob_debug	=0;
gpio_device*	glob_dev;
char		glob_folder[128];

void debug_print(char *message) {

	if (glob_debug) {
		printf("FUJITSU: ");
		printf(message);
		printf("\n");
	}
}

int camera_id (char *id) {

	strcpy(id, "fujitsu-scottf");

	return (GP_OK);
}

int camera_debug_set (int value) {

	glob_debug=value;

        return (GP_OK);
}

int camera_abilities (CameraAbilities *abilities, int *count) {

	*count = 3;

	debug_print("getting camera abilities");

	/* Fill in each camera model's abilities */
	strcpy(abilities[0].model, "Olympus D-220L");
	abilities[0].serial    = 1;
	abilities[0].parallel  = 0;
	abilities[0].usb       = 0;
	abilities[0].ieee1394 = 0;
	abilities[0].speed[0] = 19200;
	abilities[0].speed[1] = 38400;
	abilities[0].speed[2] = 57600;
	abilities[0].speed[3] = 115200;
	abilities[0].speed[4] = 0;
	abilities[0].capture	= 1;
	abilities[0].config	= 1;
	abilities[0].file_delete  = 1;
	abilities[0].file_preview = 1;
	abilities[0].file_put   = 0;

	strcpy(abilities[1].model, "Olympus D-320L");
	abilities[1].serial   = 1;
	abilities[1].parallel = 0;
	abilities[1].usb      = 0;
	abilities[1].ieee1394 = 0;
	abilities[1].parallel = 0;
	abilities[1].speed[0] = 19200;
	abilities[1].speed[1] = 38400;
	abilities[1].speed[2] = 57600;
	abilities[1].speed[3] = 115200;
	abilities[1].speed[4] = 0;
	abilities[1].capture	= 1;
	abilities[1].config	= 1;
	abilities[1].file_delete  = 1;
	abilities[1].file_preview = 1;
	abilities[1].file_put   = 0;

	strcpy(abilities[2].model, "Olympus D-620L");
	abilities[2].serial   = 1;
	abilities[2].parallel = 0;
	abilities[2].usb      = 0;
	abilities[2].ieee1394 = 0;
	abilities[2].speed[0] = 19200;
	abilities[2].speed[1] = 38400;
	abilities[2].speed[2] = 57600;
	abilities[2].speed[3] = 115200;
	abilities[2].speed[4] = 0;
	abilities[2].capture	= 1;
	abilities[2].config	= 1;
	abilities[2].file_delete  = 1;
	abilities[2].file_preview = 1;
	abilities[2].file_put   = 0;

	return (GP_OK);
}

int camera_init (CameraInit *init) {

	char buf[4096];
	char *p;
	int l;
	gpio_device_settings settings;

	debug_print("Initializing camera");

	glob_dev = NULL;

	glob_dev = gpio_new(GPIO_DEVICE_SERIAL);
	if (!glob_dev)
		return (GP_ERROR);

        strcpy(settings.serial.port, init->port_settings.port);
	settings.serial.speed 	 = 19200;
	settings.serial.bits 	 = 8;
	settings.serial.parity 	 = 0;
	settings.serial.stopbits = 1;
	gpio_set_settings(glob_dev, settings);

	if (gpio_open(glob_dev)==GPIO_ERROR)
		return (GP_ERROR);

	if (fujitsu_ping(glob_dev)==GP_ERROR)
		return (GP_ERROR);

	/* Build the packet */
	p = fujitsu_build_packet(TYPE_COMMAND, SUBTYPE_COMMAND_FIRST,3);	

	/* Fill in the data */
	p[3] = 0x00;
	p[4] = 0x11;
	p[5] = 0x05;

	l = fujitsu_process_packet(p);
printf("length=%i\n");
	gpio_write(glob_dev, p, l);
	gpio_read(glob_dev, buf, 1);
printf("got: 0x%02x\n", buf[0]);

/*
	if (init->speed)
		settings.serial.speed = init->port_settings.speed;
	   else
		settings,serial.speed = 115200;
*/	
	return (GP_OK);
}

int camera_exit () {

	debug_print("Exiting camera");

	return (GP_OK);
}

int camera_folder_list(char *folder_name, CameraFolderInfo *list) {

	debug_print("Listing folders");

	strcpy(list[0].name, "<photos>");

	/* return only 1 folder (root) on camera */
	return (1);
}

int camera_folder_set(char *folder_name) {

	char buf[4096];

	sprintf(buf, "Setting folder to %s", folder_name);
	debug_print(buf);

	strcpy(glob_folder, folder_name);

	return (GP_OK);
}

int camera_file_count () {

	debug_print("Counting files");

	return (0);
}

int camera_file_get (int file_number, CameraFile *file) {

	char buf[4096];

	sprintf(buf, "Getting file #%i", file_number);
	debug_print(buf);

	return (GP_OK);
}

int camera_file_get_preview (int file_number, CameraFile *preview) {

	char buf[4096];

	sprintf(buf, "Getting file preview #%i", file_number);
	debug_print(buf);

	return (GP_OK);
}

int camera_file_put (CameraFile *file) {

	debug_print("Putting file on camera");

	return (GP_ERROR);
}

int camera_file_delete (int file_number) {

	char buf[4096];

	sprintf(buf, "Deleting file #%i", file_number);
	debug_print(buf);

	return (GP_ERROR);
}

int camera_config_get (CameraWidget *window) {

	debug_print("Building configuration window");

        return GP_ERROR;
}

int camera_config_set (CameraSetting *setting, int count) {

	debug_print("Setting configuration values");

	return (GP_ERROR);
}

int camera_capture (CameraFileType type) {

	debug_print("Capturing image");

	return (GP_ERROR);
}

int camera_summary (char *summary) {

	debug_print("Getting camera summary");

	strcpy(summary, "Summary Not Available");

	return (GP_OK);
}

int camera_manual (char *manual) {

	debug_print("Getting camera manual");

	strcpy(manual, "Manual Not Available");

	return (GP_OK);
}

int camera_about (char *about) {

	debug_print("Getting 'about' information");

	strcpy(about, 
"Fujitsu SPARClite library
Scott Fritzinger <scottf@unr.edu>
Support for Fujitsu-based digital cameras
including Olympus, Nikon, Epson, and a few
others.");

	return (GP_OK);
}
