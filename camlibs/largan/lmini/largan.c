/* largan.c
 *
 * Copyright © 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
 * Copyright © 2002 Hubert Figuiere <hfiguiere@teaser.fr>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Currently only largan lmini is supported
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gphoto2-library.h>
#include <gphoto2-result.h>

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

/* internal functions prototypes */
static uint8_t convert_name_to_index (const char * name);

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "largan");

	return (GP_OK);
}


struct largan_cameras {
	char * name;
	unsigned short idVendor;
	unsigned short idProduct;
	char hasSerial;
} largan_cameras [] = {
	{ "Largan Lmini", 0, 0, 1 },
/* these are other largan camera. I'm not sure they use the same
 * protocol */
/*	{ "Largan Chameleon Slim", 0, 0, 0 },
	{ "Largan Chameleon Mega", 0, 0, 0 },
	{ "Largan Chameleon", 0, 0, 0 },
	{ "Largan Easy 800", 0, 0, 1 }, */
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
			// only append if port is defined.
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
		gp_file_set_name (file, filename);
		if (pict->type == LARGAN_THUMBNAIL) {
			gp_file_set_mime_type (file, GP_MIME_BMP);
		}
		else {
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
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;
	int ret;
	/*
	 * Delete all files in the given folder. If your camera doesn't have
	 * such a functionality, just don't implement this function.
	 */

	ret = largan_erase (camera, 1);
	
	return ret;
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	int ret;
	/*
	 * Capture a preview and return the data in the given file (again,
	 * use gp_file_set_data_and_size, gp_file_set_mime_type, etc.).
	 * libgphoto2 assumes that previews are NOT stored on the camera's 
	 * disk. If your camera does, please delete it from the camera.
	 */
	ret = largan_capture (camera);
	if (ret != GP_OK) {
		return ret;
	}


	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	int ret;
	/*
	 * Capture an image and tell libgphoto2 where to find it by filling
	 * out the path.
	 */
	ret = largan_capture (camera);
	if (ret != GP_OK) {
		return ret;
	}
	

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	/*
	 * Fill out the summary with some information about the current 
	 * state of the camera (like pictures taken, etc.).
	 */
	strcpy (summary->text, _("There is nothing to summarize for this camera."));

	return (GP_OK);
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	/*
	 * If you would like to tell the user some information about how 
	 * to use the camera or the driver, this is the place to do.
	 */
	strcpy (manual->text, _("No manual"));

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Largan driver\n"
			       "Hubert Figuiere <hfiguiere@teaser.fr>\n\n"
			       "Handles Largan Lmini camera.\n"
			       ""));

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	/* Camera *camera = data; */

	/* Get the file info here and write it into <info> */

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

int
camera_init (Camera *camera, GPContext *context) 
{
	int ret, selected_speed = 0;
	GPPortSettings settings;

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->capture              = camera_capture;
        camera->functions->capture_preview      = camera_capture_preview;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func,
				      NULL, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL,
				      camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      NULL, camera);
	gp_filesystem_set_folder_funcs (camera->fs, NULL,
					delete_all_func, NULL, NULL, camera);

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
		selected_speed = settings.serial.speed;

		settings.serial.speed    = 19200;// initial speed is 19200
		settings.serial.bits     = 8;
		settings.serial.parity   = 0;
		settings.serial.stopbits = 1;
		break;
	case GP_PORT_USB:
		// TODO check with a USB camera....
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
	ret = gp_port_set_timeout (camera->port, TIMEOUT);
	if (ret < 0)
		return (ret);
*/
	/*
	 * Once you have configured the port, you should check if a 
	 * connection to the camera can be established.
	 */
	ret = largan_open (camera);
	if (ret < 0)
		return (ret);
	

	return (GP_OK);
}
