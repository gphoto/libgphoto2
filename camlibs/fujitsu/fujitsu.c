#include <string.h>
#include <gphoto2.h>
#include <gpio.h>

int camera_abilities (CameraAbilities *abilities) {

	abilities->model[0] 	= strdup("Olympus D-220L");
	abilities->model[1] 	= strdup("Olympus D-320L");
	abilities->model[2] 	= strdup("Olympus D-620L");
	abilities->model[3] 	= NULL;

	abilities->serial	= 1;
	abilities->serial_baud[0] = 19200;
	abilities->serial_baud[1] = 38400;
	abilities->serial_baud[2] = 57600;
	abilities->serial_baud[3] = 115200;
	abilities->serial_baud[4] = 0;

	abilities->cancel	= 0;
	abilities->capture	= 1;
	abilities->config	= 1;
	abilities->delete_file  = 1;
	abilities->file_preview = 1;
	abilities->reset	= 1;
	abilities->sleep	= 1;

	return (GP_OK);
}

int camera_init (CameraInit *init) {

	return (GP_OK);
}

int camera_exit () {

	return (GP_OK);
}

int camera_open () {

	return (GP_OK);
}

int camera_close () {

	return (GP_OK);
}

int camera_folder_count (int *count) {

	return (0);
}

int camera_folder_name(int folder_number, char *folder_name) {

	return (GP_OK);
}

int camera_folder_set(int folder_number) {

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

int camera_file_delete (int file_number) {

	return (GP_ERROR);
}

int camera_file_lock (int file_number) {

	return (GP_ERROR);
}

int camera_file_unlock (int file_number) {

	return (GP_ERROR);
}

int camera_config_set (CameraConfig *conf) {

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
"Scott Fritzinger <scottf@unr.edu>
Support for Fujitsu-based digital cameras
including Olympus, Nikon, Epson, and a few
others.");

	return (GP_OK);
}
