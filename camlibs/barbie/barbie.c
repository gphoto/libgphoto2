#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "barbie.h"

extern char packet_1[];

static char *models[] = {
	"Barbie",
	"Nick Click",
	"WWF",
	"Hot Wheels",
	NULL
};

int camera_id (CameraText *id) {

	strcpy(id->text, "barbie");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	int x=0;
	CameraAbilities *a;

	while (models[x]) {

		/* Allocate the new abilities */
		a = gp_abilities_new();

		/* Fill in the appropriate flags/data */
		strcpy(a->model, models[x]);
		a->port      = GP_PORT_SERIAL;
		a->speed[0]  = 57600;
		a->speed[1]  = 0;
		a->operations        = GP_OPERATION_NONE;
		a->file_operations   = GP_FILE_OPERATION_PREVIEW;
		a->folder_operations = GP_FOLDER_OPERATION_NONE;

		/* Append it to the list */
		gp_abilities_list_append(list, a);

		x++;
	}	
	return (GP_OK);
}

int camera_init(Camera *camera) {

	gp_port_settings settings;
	BarbieStruct *b;
        int ret;

	/* First, set up all the function pointers */
	camera->functions->id 		= camera_id;
	camera->functions->abilities 	= camera_abilities;
	camera->functions->init 	= camera_init;
	camera->functions->exit 	= camera_exit;
	camera->functions->folder_list_folders  = camera_folder_list_folders;
	camera->functions->folder_list_files    = camera_folder_list_files;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	b = (BarbieStruct*)malloc(sizeof(BarbieStruct));
	camera->camlib_data = b;

        if ((ret = gp_port_new(&(b->dev), GP_PORT_SERIAL)) < 0) {
            return (ret);
        }
	gp_port_timeout_set(b->dev, 5000);
	strcpy(settings.serial.port, camera->port->path);

	settings.serial.speed	= 57600;
	settings.serial.bits	= 8;
	settings.serial.parity	= 0;
	settings.serial.stopbits= 1;

	gp_port_settings_set(b->dev, settings);
	gp_port_open(b->dev);

	/* Create the filesystem */
	b->fs = gp_filesystem_new();

	return (barbie_ping(b));
}

int camera_exit(Camera *camera) {

	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	gp_port_close(b->dev);
	gp_filesystem_free(b->fs);

	return GP_OK;
}


int camera_folder_list_folders (Camera *camera, const char *folder, CameraList *list) {

	/* there are never any subfolders */

	return GP_OK;
}

int camera_folder_list_files (Camera *camera, const char *folder, CameraList *list) 
{

	int count, x;
	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	count = barbie_file_count(b);

	/* Populate the filesystem */
	gp_filesystem_populate (b->fs, "/", "mattel%02i.ppm", count);

	for (x = 0; x < gp_filesystem_count (b->fs, folder); x++) 
		gp_list_append (list, 
				gp_filesystem_name (b->fs, folder, x), 
				GP_LIST_FILE);

	return GP_OK;
}

int camera_file_get (Camera *camera, const char *folder, const char *filename, 
		     CameraFile *file) 
{

	int size, num;
	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	gp_frontend_progress(camera, NULL, 0.00);
	
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

int camera_file_get_preview (Camera *camera, const char *folder, 
			     const char *filename, CameraFile *file) 
{

	int size, num;
	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	gp_frontend_progress(camera, NULL, 0.00);

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

#if 0
int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) {

	char cmd[4], resp[4];
	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

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

	return (GP_ERROR);
}	
#endif

int camera_summary (Camera *camera, CameraText *summary) {

	int num;
	char *firm;
	BarbieStruct *b = (BarbieStruct*)camera->camlib_data;

	num = barbie_file_count(b);
	firm = barbie_read_firmware(b);

	sprintf(summary->text, _("Number of pictures: %i\nFirmware Version: %s"), num,firm);

	free(firm);

	return GP_OK;
}

int camera_manual (Camera *camera, CameraText *manual) {

	strcpy(manual->text, _("No manual available."));

	return GP_OK;
}
int camera_about (Camera *camera, CameraText *about) {

	strcpy(about->text,_("Barbie/HotWheels/WWF\nScott Fritzinger <scottf@unr.edu>\nAndreas Meyer <ahm@spies.com>\nPete Zaitcev <zaitcev@metabyte.com>\n\nReverse engineering of image data by:\nJeff Laing <jeffl@SPATIALinfo.com>\n\nImplemented using documents found on\nthe web. Permission given by Vision."));
	return GP_OK;
}
