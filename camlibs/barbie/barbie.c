#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gphoto2.h>
#include <gpio.h>
#include "barbie.h"

extern char packet_1[];

int camera_id (char *id) {

	strcpy(id, "barbie");

	return (GP_OK);
}

int camera_abilities (CameraAbilities *abilities, int *count) {

	*count = 3;

	/* What models do we support? */
	strcpy(abilities[0].model, "Barbie");
	abilities[0].port = GP_PORT_SERIAL;
	abilities[0].speed[0] = 57600;
	abilities[0].speed[1] = 0;

	abilities[0].capture   = 1;
	abilities[0].config    = 0;
	abilities[0].file_delete  = 0;
	abilities[0].file_preview = 1;
	abilities[0].file_put  = 0;

	memcpy(&abilities[1], &abilities[0], sizeof(abilities[0]));
	strcpy(abilities[1].model, "Hot Wheels");

	memcpy(&abilities[2], &abilities[0], sizeof(abilities[0]));
	strcpy(abilities[2].model, "WWF");

	return (GP_OK);
}

int camera_init(Camera *camera, CameraInit *init) {

	gpio_device_settings settings;
	BarbieStruct *b;

	/* First, set up all the function pointers */
	camera->functions->id 		= camera_id;
	camera->functions->abilities 	= camera_abilities;
	camera->functions->init 	= camera_init;
	camera->functions->exit 	= camera_exit;
	camera->functions->folder_list  = camera_folder_list;
	camera->functions->file_list    = camera_file_list;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->file_put 	= NULL;
	camera->functions->file_delete 	= camera_file_delete;
	camera->functions->config_get   = camera_config_get;
	camera->functions->config_set   = camera_config_set;
	camera->functions->capture 	= NULL;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	b = (BarbieStruct*)malloc(sizeof(BarbieStruct));
	camera->camlib_data = b;

	b->debug = camera->debug;

	if (b->debug)
		printf("barbie: Initializing the camera\n");

	b->dev = gpio_new(GPIO_DEVICE_SERIAL);
	gpio_set_timeout(b->dev, 5000);
	strcpy(settings.serial.port, init->port_settings.path);

	settings.serial.speed	= 57600;
	settings.serial.bits	= 8;
	settings.serial.parity	= 0;
	settings.serial.stopbits= 1;

	gpio_set_settings(b->dev, settings);
	gpio_open(b->dev);

	/* Create the filesystem */
	b->fs = gp_filesystem_new();

	return (barbie_ping(b));
}

int camera_exit(Camera *camera) {

	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	gpio_close(b->dev);
	gp_filesystem_free(b->fs);

	return GP_OK;
}


int camera_folder_list (Camera *camera, CameraList *list, char *folder) {

	/* there are never any subfolders */

	return GP_OK;
}

int camera_file_list (Camera *camera, CameraList *list, char *folder) {

	int count, x;
	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	count = barbie_file_count(b);

	/* Populate the filesystem */
	gp_filesystem_populate(b->fs, "/", "mattel%02i.ppm", count);

	for (x=0; x<gp_filesystem_count(b->fs, folder); x++) 
		gp_list_append(list, gp_filesystem_name(b->fs, folder, x), GP_LIST_FILE);

	return GP_OK;
}

int camera_file_get (Camera *camera, CameraFile *file, char *folder, char *filename) {

	int size, num;
	char name[16];
	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	if (b->debug)
		printf("barbie: Getting a picture\n");

	gp_camera_progress(camera, NULL, 0.00);
	
	strcpy(file->name, filename);
	strcpy(file->type, "image/ppm");

	/* Retrieve the number of the photo on the camera */
	num = gp_filesystem_number(b->fs, "/", filename);

	file->data = barbie_read_picture(b, num, 0, &size);
	if (!file->data)
		return GP_ERROR;
	file->size = size;

	return GP_OK;
}

int camera_file_get_preview (Camera *camera, CameraFile *file, char *folder, char *filename) {

	int size, num;
	char name[24];
	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	if (b->debug)
		printf("barbie: Getting a preview\n");

	gp_camera_progress(camera, NULL, 0.00);

	strcpy(file->name, filename);
	strcpy(file->type, "image/ppm");

	/* Retrieve the number of the photo on the camera */
	num = gp_filesystem_number(b->fs, "/", filename);

	file->data = barbie_read_picture(b, num, 1, &size);;
	if (!file->data)
		return GP_ERROR;
	file->size = size;

	return GP_OK;
}

int camera_file_put (Camera *camera, CameraFile *file, char *folder) {

	return GP_ERROR;
}


int camera_file_delete (Camera *camera, char *folder, char *filename) {

	return GP_ERROR;
}

int camera_config_get (Camera *camera, CameraWidget *window) {

	return GP_ERROR;
}

int camera_config_set (Camera *camera, CameraSetting *setting, int count) {

	return GP_OK;
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {
/*
	char cmd[4], resp[4];
	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	if (b->debug)
		printf("barbie: Taking a picture\n");

	memcpy(cmd, packet_1, 4);

	cmd[COMMAND_BYTE] = 'G';
	cmd[DATA1_BYTE]   = 0x40;
	if (barbie_exchange(cmd, 4, resp, 4) == 0)
		return (0);

	cmd[COMMAND_BYTE] = 'Y';
	cmd[DATA1_BYTE]   = 0;
	if (barbie_exchange(cmd, 4, resp, 4) == 0)
		return (0);

	return(resp[DATA1_BYTE] == 0? GP_OK: GP_ERROR);
*/
	return (GP_ERROR);
}	

int camera_summary (Camera *camera, CameraText *summary) {

	int num;
	char *firm;
	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	num = barbie_file_count(b);
	firm = barbie_read_firmware(b);

	sprintf(summary->text, 
"Number of pictures: %i
Firmware Version: %s", num,firm);

	free(firm);

	return GP_OK;
}

int camera_manual (Camera *camera, CameraText *manual) {

	strcpy(manual->text, "No manual available.");

	return GP_OK;
}
int camera_about (Camera *camera, CameraText *about) {

	strcpy(about->text,
"Barbie/HotWheels/WWF
Scott Fritzinger <scottf@unr.edu>
Andreas Meyer <ahm@spies.com>
Pete Zaitcev <zaitcev@metabyte.com>

Reverse engineering of image data by:
Jeff Laing <jeffl@SPATIALinfo.com>

Implemented using documents found on
the web. Permission given by Vision.");
	return GP_OK;
}
