#include <string.h>
#include <gphoto2.h>
#include <gpio.h>

int camera_id (char *id) {

	strcpy(id, "REPLACE WITH UNIQUE LIBRARY ID");

	return (GP_OK);
}

int camera_abilities (CameraAbilities *abilities, int *count) {

	*count = 0;

	/* Fill in each camera model's abilities */
	/* Make separate entries for each conneciton type (usb, serial, etc...)
	   if a camera supported multiple ways. */

	strcpy(abilities[0].model, "CAMERA MODEL");
	abilities[0].serial   = 0;
	abilities[0].parallel = 0;
	abilities[0].usb      = 0;
	abilities[0].ieee1394 = 0;
	abiltiies[0].network  = 0;

	abilities[0].speed[0] = 0;
	abilities[0].capture  = 0;
	abilities[0].config   = 0;
	abilities[0].file_delete  = 0;
	abilities[0].file_preview = 0;
	abilities[0].file_put = 0;

	return (GP_OK);
}

int camera_init (Camera *camera, CameraInit *init) {

	/* First, set up all the function pointers */
	camera->functions->id 		= camera_id;
	camera->functions->abilities 	= camera_abilities;
	camera->functions->init 	= camera_init;
	camera->functions->exit 	= camera_exit;
	camera->functions->folder_list  = camera_folder_list;
	camera->functions->folder_set 	= camera_folder_set;
	camera->functions->file_count 	= camera_file_count;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->file_put 	= camera_file_put;
	camera->functions->file_delete 	= camera_file_delete;
	camera->functions->config_get   = camera_config_get;
	camera->functions->config_set   = camera_config_set;
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	return (GP_OK);
}

int camera_exit (Camera *camera) {

	return (GP_OK);
}

int camera_folder_list(Camera *camera, char *folder_name, CameraFolderInfo *list) {

	return (GP_OK);
}

int camera_folder_set(Camera *camera, char *folder_name) {

	return (GP_OK);
}

int camera_file_count (Camera *camera) {

	return (0);
}

int camera_file_get (Camera *camera, CameraFile *file, int file_number) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/

	return (GP_OK);
}

int camera_file_get_preview (Camera *camera, CameraFile *preview, int file_number) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/

	return (GP_OK);
}

int camera_file_put (Camera *camera, CameraFile *file) {

	return (GP_ERROR);
}

int camera_file_delete (Camera *camera, int file_number) {

	return (GP_ERROR);
}

int camera_config_get (Camera *camera, CameraWidget *window) {

        return GP_ERROR;
}

int camera_config_set (Camera *camera, CameraSetting *setting, int count) {

	return (GP_ERROR);
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	return (GP_ERROR);
}

int camera_summary (Camera *camera, CameraText *summary) {

	strcpy(summary->text, "Summary Not Available");

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) {

	strcpy(manual->text, "Manual Not Available");

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

	strcpy(about->text, 
"Library Name
YOUR NAME <email@somewhere.com>
Quick description of the library.
No more than 5 lines if possible.");

	return (GP_OK);
}
