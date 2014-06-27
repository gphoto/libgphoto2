/* largan.c
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sourceforge.net>
 * Copyright 2002 Hubert Figuiere <hfiguiere@teaser.fr>
 * Code largely borrowed to lmini-0.1 by Steve O Connor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * Currently only largan lmini is supported
 */

#define _BSD_SOURCE

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>

#include "lmini.h"


#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define TIMEOUT 1500

/* internal functions prototypes */
static uint8_t convert_name_to_index (const char * name);

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "largan-lmini");

	return (GP_OK);
}


static const struct largan_cameras {
	char * name;
	unsigned short idVendor;
	unsigned short idProduct;
	char hasSerial;
} largan_cameras [] = {
	{ "Largan:Lmini", 0, 0, 1 },
/* these are other largan camera. I'm not sure they use the same
 * protocol */
/*	{ "Largan:Chameleon Slim", 0, 0, 0 },
	{ "Largan:Chameleon Mega", 0, 0, 0 },
	{ "Largan:Chameleon", 0, 0, 0 },
	{ "Largan:Easy 800", 0, 0, 1 }, */
	{ NULL, 0, 0, 0 }
};

int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;
	int i;

	for (i = 0; largan_cameras[i].name; i++) {
		memset(&a, 0, sizeof(a));
		strcpy(a.model, largan_cameras[i].name);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		if (largan_cameras[i].hasSerial) {
			a.port |= GP_PORT_SERIAL;
		}
		if (largan_cameras[i].idVendor && largan_cameras[i].idProduct) {
			a.port |= GP_PORT_USB;
		}
		if (largan_cameras[i].hasSerial) {
			a.speed[0] = 4800;
			a.speed[1] = 9600;
			a.speed[2] = 19200;
			a.speed[3] = 38400;
			a.speed[4] = 0;

		}
		a.operations        = 	GP_OPERATION_CAPTURE_IMAGE;
		a.file_operations   = 	GP_FILE_OPERATION_DELETE | 
					GP_FILE_OPERATION_PREVIEW;
		a.folder_operations = 	GP_FOLDER_OPERATION_NONE;
	
		if (a.port) {
			/* only append if port is defined.*/
			gp_abilities_list_append(list, a);
		}
	}
	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context) 
{
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;

	/*
	 * Get the file from the camera. Use gp_file_set_mime_type,
	 * gp_file_set_data_and_size, etc.
	 */
	int ret;
	largan_pict_info *pict;
	uint8_t index;	/* the index of the file to fetch */
	largan_pict_type pict_type;

	index = convert_name_to_index (filename);
	
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		pict_type = LARGAN_PICT;
		break;
	case GP_FILE_TYPE_PREVIEW:
		pict_type = LARGAN_THUMBNAIL;
		break;
	default:
		return GP_ERROR_NOT_SUPPORTED;
	}
		
	pict = largan_pict_new ();
	ret = largan_get_pict (camera, pict_type, (uint8_t)index, pict);
	if (ret == GP_OK) {
		gp_file_append (file, pict->data , pict->data_size);
		if (pict->type == LARGAN_THUMBNAIL) {
			gp_file_set_mime_type (file, GP_MIME_BMP);
		} else {
			gp_file_set_mime_type (file, GP_MIME_JPEG);
		}
	}

	largan_pict_free (pict);

	return ret;
}


static uint8_t 
convert_name_to_index (const char * name)
{
	long index;
	/* extract the index from the file name */
	char * myFile = strdup (name);
	*strstr (myFile, ".jpg") = 0; /* strip the extension */
	index = strtol(myFile, (char **)NULL, 10);
	/* TODO check index bounds */
	free (myFile);

	return (uint8_t)index;
}


static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int index;
	
	/* Delete one file from the camera. 
	(only the last file can be deleted)*/
	
	index = convert_name_to_index (filename);
	return largan_erase (camera, index);
}


static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;
	/*
	 * Delete all files in the given folder. If your camera doesn't have
	 * such a functionality, just don't implement this function.
	 */

	return largan_erase (camera, 0xff);
}


static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	return largan_capture (camera);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Largan driver\n"
			       "Hubert Figuiere <hfiguiere@teaser.fr>\n"));

	return (GP_OK);
}


static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int num, i;
	char name[32];

	num = largan_get_num_pict (camera);
	if (num < 0) {
		/* error */
		return num;
	}
	/* List your files here */
	for (i = 1; i <= num; i++) {
		snprintf (name, sizeof(name), "%08d.jpg", i);
		gp_list_append (list, name, NULL);
	}

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func,
};

int
camera_init (Camera *camera, GPContext *context) 
{
	int ret;
	GPPortSettings settings;

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->capture              = camera_capture;
        camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
	/*
	 * The port is already provided with camera->port (and
	 * already open). You just have to use functions like
	 * gp_port_timeout_set, gp_port_settings_get, gp_port_settings_set.
	 */
	ret = gp_port_get_settings (camera->port, &settings);
	if (ret < 0)
		return (ret);
	switch (camera->port->type) {
	case GP_PORT_SERIAL:

		/* Remember the selected speed */

		settings.serial.speed    = 19200;/* initial speed is 19200 */
		settings.serial.bits     = 8;
		settings.serial.parity   = 0;
		settings.serial.stopbits = 1;
		ret = gp_port_set_timeout (camera->port, TIMEOUT);
		if (ret < 0) {
			return (ret);
		}
		break;
	case GP_PORT_USB:
		/* TODO check with a USB camera.... */
		settings.usb.inep       = 0x82;
		settings.usb.outep      = 0x01;
		settings.usb.config     = 1;
		settings.usb.interface  = 0;
		settings.usb.altsetting = 0;
		break;
	default:
		return (GP_ERROR_UNKNOWN_PORT);
	}
	
	ret = gp_port_set_settings (camera->port, settings);
	if (ret < 0)
		return (ret);

	/*
	 * Once you have configured the port, you should check if a 
	 * connection to the camera can be established.
	 */
	return largan_open (camera);
}
