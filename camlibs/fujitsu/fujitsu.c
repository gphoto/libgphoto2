#include <string.h>
#include <gphoto2.h>
#include <gpio/gpio.h>

int camera_id (char *id) {

	strcpy(id, "fujitsu-scottf");

	return (GP_OK);
}

int camera_debug_set (int onoff) {

        return (GP_OK);
}

int camera_abilities (CameraAbilities *abilities, int *count) {

	*count = 3;

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

	return (GP_OK);
}

int camera_exit () {

	return (GP_OK);
}

int camera_folder_list(char *folder_name, CameraFolderInfo *list) {

	return (GP_ERROR);
}

int camera_folder_set(char *folder_name) {

	return (GP_OK);
}

int camera_file_count () {

	return (0);
}

int camera_file_get (int file_number, CameraFile *file) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/

	return (GP_OK);
}

int camera_file_get_preview (int file_number, CameraFile *preview) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/

	return (GP_OK);
}

int camera_file_put (CameraFile *file) {

	return (GP_ERROR);
}

int camera_file_delete (int file_number) {

	return (GP_ERROR);
}

int camera_config_get (char *filename) {

        return GP_ERROR;
}

int camera_config_set (CameraSetting *setting, int count) {

	return (GP_ERROR);
}

int camera_capture (CameraFileType type) {

	return (GP_ERROR);
}

int camera_summary (char *summary) {

	strcpy(summary, "Summary Not Available");

	return (GP_OK);
}

int camera_manual (char *manual) {

	strcpy(manual, "Manual Not Available");

	return (GP_OK);
}

int camera_about (char *about) {

	strcpy(about, 
"Fujitsu SPARClite library
Scott Fritzinger <scottf@unr.edu>
Support for Fujitsu-based digital cameras
including Olympus, Nikon, Epson, and a few
others.");

	return (GP_OK);
}
