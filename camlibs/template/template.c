#include <string.h>
#include <gphoto2.h>
#include <gpio.h>

int camera_id (char *id) {

	strcpy(id, "REPLACE WITH UNIQUE LIBRARY ID");

	return (GP_OK);
}

int camera_debug_set (int onoff) {

        return (GP_OK);
}

int camera_abilities (CameraAbilities *abilities, int *count) {

	*count = 0;

	/* Fill in each camera model's abilities */
	/* Make separate entries for each conneciton type (usb, serial, etc...)
	   if a camera supported multiple ways. */

	strcpy(abilities[0].model, "CAMERA MODEL");
	abilities[0].port_type	= GP_PORT_NONE;
	abilities[0].serial_speed[0] = 9600;
	abilities[0].serial_speed[1] = 0;
	abilities[0].capture	= 0;
	abilities[0].config	= 0;
	abilities[0].file_delete  = 0;
	abilities[0].file_preview = 0;
	abilities[0].file_put   = 0;

	return (GP_OK);
}

int camera_init (CameraInit *init) {

	return (GP_OK);
}

int camera_exit () {

	return (GP_OK);
}

int camera_folder_list(char *folder_name, CameraFolderList *list) {

	return (GP_OK);
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

int camera_config (CameraSetting *setting, int count) {

	return (GP_ERROR);
}

int camera_capture (int type) {

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
"Library Name
YOUR NAME <email@somewhere.com>
Quick description of the library.
No more than 5 lines if possible.");

	return (GP_OK);
}
