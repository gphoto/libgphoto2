#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <gphoto2-port.h>

#include "dc120.h"
#include "library.h"

char *dc120_folder_memory = "Internal Memory";
char *dc120_folder_card   = "CompactFlash Card";

int camera_id (CameraText *id) {

	strcpy(id->text, "kodak-dc120");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities *a;

	a = gp_abilities_new();

	strcpy(a->model, "Kodak DC120");
	a->port     = GP_PORT_SERIAL;
	a->speed[0] = 9600;
	a->speed[1] = 19200;
	a->speed[2] = 38400;
	a->speed[3] = 57600;
	a->speed[4] = 115200;
	a->speed[5] = 0;
	a->capture  = GP_CAPTURE_IMAGE;
	a->config   = 0;
	a->file_delete  = 1;
	a->file_preview = 1;
	a->file_put = 0;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

int camera_init (Camera *camera) {

	gp_port_settings settings;
	DC120Data *dd;
        int ret;

	if (!camera)
		return (GP_ERROR);

	dd = (DC120Data*)malloc(sizeof(DC120Data));
	if (!dd)
		return (GP_ERROR);

	/* First, set up all the function pointers */
	camera->functions->id 		= camera_id;
	camera->functions->abilities 	= camera_abilities;
	camera->functions->init 	= camera_init;
	camera->functions->exit 	= camera_exit;
	camera->functions->folder_list  = camera_folder_list;
	camera->functions->file_list	= camera_file_list;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->file_put 	= camera_file_put;
	camera->functions->file_delete 	= camera_file_delete;
	camera->functions->config       = camera_config;
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

        if ((ret= gp_port_new(&(dd->dev), GP_PORT_SERIAL)) < 0) {
		free(dd);
		return (ret);
	}

	strcpy(settings.serial.port, camera->port->path);
	settings.serial.speed    = 9600;
	settings.serial.bits     = 8;
	settings.serial.parity   = 0;
	settings.serial.stopbits = 1;

	if (gp_port_settings_set(dd->dev, settings) == GP_ERROR) {
		gp_port_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	if (gp_port_open(dd->dev) == GP_ERROR) {
		gp_port_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	gp_port_timeout_set(dd->dev, TIMEOUT);

	/* Reset the camera to 9600 */
	gp_port_send_break(dd->dev, 2);

	/* Wait for it to update */
	GP_SYSTEM_SLEEP(1500);

	if (dc120_set_speed(dd, camera->port->speed) == GP_ERROR) {
		gp_port_close(dd->dev);
		gp_port_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	/* Try to talk after speed change */
	if (dc120_get_status(dd) == GP_ERROR) {
		gp_port_close(dd->dev);
		gp_port_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	/* Everything went OK. Save the data*/
	dd->fs = gp_filesystem_new();
	camera->camlib_data = dd;

	return (GP_OK);
}

int camera_exit (Camera *camera) {

	DC120Data *dd = camera->camlib_data;

	if (!dd)
		return (GP_OK);

	gp_filesystem_free(dd->fs);

	if (dd->dev) {
		if (gp_port_close(dd->dev) == GP_ERROR)
			{ /* camera did a bad, bad thing */ }
		gp_port_free(dd->dev);
	}
	free(dd);

	return (GP_OK);
}

int camera_folder_list	(Camera *camera, CameraList *list, char *folder) {

	DC120Data *dd = camera->camlib_data;
	char buf[32];

	if (strcmp(folder, "/")==0) {
		gp_list_append(list, dc120_folder_memory, GP_LIST_FOLDER);
		gp_list_append(list, dc120_folder_card, GP_LIST_FOLDER);
		return (GP_OK);
	}

	/* Chop trailing slash */
	if (folder[strlen(folder)-1] == '/')
		folder[strlen(folder)-1] = 0;

	sprintf(buf, "/%s", dc120_folder_memory);
	if (strcmp(folder, buf)==0)
		/* From memory */
		return (dc120_get_albums(dd, 0, list));

	sprintf(buf, "/%s", dc120_folder_card);
	if (strcmp(folder, buf)==0)
		/* From cf card */
		return (dc120_get_albums(dd, 1, list));
	return (GP_ERROR);
}

int camera_file_list (Camera *camera, CameraList *list, char *folder) {

	DC120Data *dd = camera->camlib_data;
	CameraListEntry *entry;
	char buf[32];
	int retval = GP_OK;
	int x;

	/* Chop trailing slash */
	if (folder[strlen(folder)-1] == '/')
		folder[strlen(folder)-1] = 0;

	sprintf(buf, "/%s", dc120_folder_memory);
	if (strcmp(folder, buf)==0)
		/* From memory */
		retval = dc120_get_filenames(dd, 0, 0, list);

	sprintf(buf, "/%s", dc120_folder_card);
	if (strcmp(folder, buf)==0)
		/* From cf card */
		retval = dc120_get_filenames(dd, 1, 0, list);

	sprintf(buf, "/%s/ALBUM", dc120_folder_card);
	if (strncmp(folder, buf, strlen(dc120_folder_card)+7)==0)
		retval = dc120_get_filenames(dd, 1, folder[strlen(dc120_folder_card)+8] - '0', list);

	/* Save the order of the pics (wtf: no filename access on dc120???) */
	/* Use the filesystem helpers to maintain picture list */
	if (retval == GP_OK) {
		for (x=0; x<gp_list_count(list); x++) {
			entry = gp_list_entry(list, x);
			gp_filesystem_append(dd->fs, folder, entry->name);
		}
	}

	return (retval);
}

int camera_file_action (Camera *camera, int action, CameraFile *file, char *folder, char *filename) {

	DC120Data *dd = camera->camlib_data;
	int picnum=0, album_num=-1, from_card=0;
	char buf[32];
	char *dot;

	picnum = gp_filesystem_number(dd->fs, folder, filename);

	if (picnum == GP_ERROR)
		return (GP_ERROR);
	
	sprintf(buf, "/%s", dc120_folder_memory);
	if (strcmp(folder, buf)==0) {
		from_card = 0;
		album_num = 0;
	}

	sprintf(buf, "/%s", dc120_folder_card);
	if (strcmp(folder, buf)==0) {
		from_card = 1;
		album_num = 0;
	}

	sprintf(buf, "/%s/ALBUM", dc120_folder_card);
	if (strncmp(folder, buf, strlen(dc120_folder_card)+7)==0) {
		from_card = 1; 
		album_num = folder[12] - '0';
	}

	if (album_num == -1)
		return (GP_ERROR);

	if (action == DC120_ACTION_PREVIEW) {
		dot = strrchr(filename, '.');
		if (dot)
			strcpy(&dot[1], "ppm");
	}

	if (file)
		strcpy(file->name, filename);

	return (dc120_file_action(dd, action, from_card, album_num, picnum+1, file));
}

int camera_file_get (Camera *camera, CameraFile *file, char *folder, char *filename) { 

	return (camera_file_action(camera, DC120_ACTION_IMAGE, file, folder, filename));
}

int camera_file_get_preview (Camera *camera, CameraFile *file,
			     char *folder, char *filename) {

	return (camera_file_action(camera, DC120_ACTION_PREVIEW, file, folder, filename));
}

int camera_file_put (Camera *camera, CameraFile *file, char *folder) {

/*	DC120Data *dd = camera->camlib_data; */

	return (GP_ERROR);
}

int camera_file_delete (Camera *camera, char *folder, char *filename) {

	DC120Data *dd = camera->camlib_data;
	int retval;

	retval = camera_file_action(camera, DC120_ACTION_DELETE, NULL, folder, filename);

	if (retval == GP_OK)
		gp_filesystem_delete(dd->fs, folder, filename);

	return (retval);
}

int camera_config (Camera *camera) {

/*	DC120Data *dd = camera->camlib_data; */

	return (GP_ERROR);
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	DC120Data *dd = camera->camlib_data;

	return (dc120_capture(dd, file));
}

int camera_summary (Camera *camera, CameraText *summary) {

/*	DC120Data *dd = camera->camlib_data; */

	strcpy(summary->text, "No summary information yet");

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) {

/*	DC120Data *dd = camera->camlib_data; */

	strcpy(manual->text, 
"The Kodak DC120 camera uses the KDC file format \
for storing images. If you want to view the images you \
download from your camera, you will need to download \
the \"kdc2tiff\" program. \
It is available from http://kdc2tiff.sourceforge.net");

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

/*	DC120Data *dd = camera->camlib_data; */

	strcpy(about->text, 
"Kodak DC120 Camera Library \
Scott Fritzinger <scottf@gphoto.net> \
Camera Library for the Kodak DC120 camera. \
(by popular demand).");

	return (GP_OK);
}
