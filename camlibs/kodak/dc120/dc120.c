#include <string.h>
#include <gphoto2.h>
#include <gpio.h>

#include "dc120.h"
#include "library.h"

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
	a->capture  = 1;
	a->config   = 0;
	a->file_delete  = 1;
	a->file_preview = 1;
	a->file_put = 0;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

int camera_init (Camera *camera, CameraInit *init) {

	gpio_device_settings settings;
	DC120Data *dd;

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
	camera->functions->config_get   = camera_config_get;
	camera->functions->config_set   = camera_config_set;
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	dd->dev = gpio_new(GPIO_DEVICE_SERIAL);
	if (!dd->dev) {
		free(dd);
		return (GP_ERROR);
	}

	strcpy(settings.serial.port, init->port_settings.path);
	settings.serial.speed    = 9600;
	settings.serial.bits     = 8;
	settings.serial.parity   = 0;
	settings.serial.stopbits = 1;

	if (gpio_set_settings(dd->dev, settings) == GPIO_ERROR) {
		gpio_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	if (gpio_open(dd->dev) == GPIO_ERROR) {
		gpio_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	gpio_set_timeout(dd->dev, TIMEOUT);

	/* Reset the camera to 9600 */
	gpio_send_break(dd->dev, 1);

	/* Wait for it to update */
	GPIO_SLEEP(1500);

	if (dc120_set_speed(dd, init->port_settings.speed) == GP_ERROR) {
		gpio_close(dd->dev);
		gpio_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	/* Try to talk after speed change */
	if (dc120_get_status(dd) == GP_ERROR) {
		gpio_close(dd->dev);
		gpio_free(dd->dev);
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
		if (gpio_close(dd->dev) == GPIO_ERROR)
			{ /* camera did a bad, bad thing */ }
		gpio_free(dd->dev);
	}
	free(dd);

	return (GP_OK);
}

int camera_folder_list	(Camera *camera, CameraList *list, char *folder) {

	DC120Data *dd = camera->camlib_data;

	if (strcmp(folder, "/")==0) {
		gp_list_append(list, "Built-In", GP_LIST_FOLDER);
		gp_list_append(list, "Card", GP_LIST_FOLDER);
		return (GP_OK);
	}

	/* Chop trailing slash */
	if (folder[strlen(folder)-1] == '/')
		folder[strlen(folder)-1] = 0;

	if (strcmp(folder, "/Build-In")==0)
		/* From memory */
		return (dc120_get_albums(dd, 0, list));

	if (strcmp(folder, "/Card")==0)
		/* From cf card */
		return (dc120_get_albums(dd, 1, list));

	return (GP_ERROR);
}

int camera_file_list (Camera *camera, CameraList *list, char *folder) {

	DC120Data *dd = camera->camlib_data;
	CameraListEntry *entry;
	int retval = GP_ERROR;
	int x;

	/* Chop trailing slash */
	if (folder[strlen(folder)-1] == '/')
		folder[strlen(folder)-1] = 0;

	if (strcmp(folder, "/Built-In")==0)
		/* From memory */
		retval = dc120_get_filenames(dd, 0, 0, list);

	if (strcmp(folder, "/Card")==0)
		/* From cf card */
		retval = dc120_get_filenames(dd, 1, 0, list);

	if (strncmp(folder, "/Card/ALBUM", 11)==0)
		retval = dc120_get_filenames(dd, 1, folder[12] - '0', list);

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

int camera_file_get_common (Camera *camera, CameraFile *file, char *folder, char *filename,
	int get_preview) {

	DC120Data *dd = camera->camlib_data;
	int picnum, album_num, from_card;

	picnum = gp_filesystem_number(dd->fs, folder, filename);

	if (picnum == GP_ERROR)
		return (GP_ERROR);

	if (strcmp(folder, "/Built-In")==0) {
		from_card = 0;
		album_num = 0;
	}

	if (strcmp(folder, "/Card")==0) {
		from_card = 1;
		album_num = 0;
	}

	if (strncmp(folder, "/Card/ALBUM", 11)==0) {
		from_card = 1;
		album_num = folder[12] - '0';
	}

	return (dc120_get_file(dd, get_preview, from_card, album_num, picnum+1, file));
}

int camera_file_get (Camera *camera, CameraFile *file, char *folder, char *filename) { 

	return (camera_file_get_common(camera, file, folder, filename, 0));
}

int camera_file_get_preview (Camera *camera, CameraFile *file,
			     char *folder, char *filename) {

	return (camera_file_get_common(camera, file, folder, filename, 1));
}

int camera_file_put (Camera *camera, CameraFile *file, char *folder) {

	DC120Data *dd = camera->camlib_data;

	return (GP_OK);
}

int camera_file_delete (Camera *camera, char *folder, char *filename) {

	DC120Data *dd = camera->camlib_data;

	return (GP_OK);
}

int camera_config_get (Camera *camera, CameraWidget *window) {

	DC120Data *dd = camera->camlib_data;

	return (GP_OK);
}

int camera_config_set (Camera *camera, CameraSetting *conf, int count) {

	DC120Data *dd = camera->camlib_data;

	return (GP_OK);
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	DC120Data *dd = camera->camlib_data;

	return (GP_OK);
}

int camera_summary (Camera *camera, CameraText *summary) {

	DC120Data *dd = camera->camlib_data;

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) {

	DC120Data *dd = camera->camlib_data;

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

	DC120Data *dd = camera->camlib_data;

	strcpy(about->text, 
"Kodak DC120 Camera Library
Scott Fritzinger <scottf@gphoto.net>
Camera Library for the Kodak DC120 camera.
(by popular demand).");

	return (GP_OK);
}
