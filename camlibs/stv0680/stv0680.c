/*
 * STV0680 Vision Camera Chipset Driver
 * Copyright (C) 2000 Adam Harrison <adam@antispin.org> 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA   
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <gphoto2.h>
#include <gphoto2-port.h>
#include <gphoto2-library.h>

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

#include "stv0680.h"
#include "library.h"

struct camera_to_usb {
	  char *name;
	  unsigned short idVendor;
	  unsigned short idProduct;
} camera_to_usb[] = {
	{ "STM USB Dual-mode camera", 0x0553, 0x0202 },
	{ "Aiptek PenCam Trio", 0x0553, 0x0202 },
	{ "Micromaxx Digital Camera", 0x553, 0x0202 },
	{ "STV0680", 0x0, 0x0 }				
};

int camera_id (CameraText *id) 
{
	strcpy(id->text, "STV0680");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;
	int i;

	/* 10/11/01/cw rewritten to add cameras via array */

	for (i = 0; i < sizeof(camera_to_usb) / sizeof(struct camera_to_usb); i++) {

		strcpy(a.model, camera_to_usb[i].name);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;

		if (!camera_to_usb[i].idVendor) {
			a.port     = GP_PORT_SERIAL;
			a.speed[0] = 115200;
			a.speed[1] = 0;
			a.operations        = GP_OPERATION_CAPTURE_IMAGE;
			a.file_operations   = GP_FILE_OPERATION_PREVIEW;
			a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;
		} else {
			a.port     = GP_PORT_USB;
			a.speed[0] = 0;
			a.operations        = GP_OPERATION_CAPTURE_PREVIEW | 
			    			GP_OPERATION_CAPTURE_IMAGE;
			a.file_operations   = GP_FILE_OPERATION_PREVIEW;
			a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;
			a.usb_vendor  = camera_to_usb[i].idVendor;		
			a.usb_product = camera_to_usb[i].idProduct;
		}

		gp_abilities_list_append(list, a);
	}

	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder, 
			   CameraList *list, void *data) 
{
	Camera *camera = data;
	int count, result;

	result = stv0680_file_count(camera->port, &count);
	if (result != GP_OK)
		return result;

	gp_list_populate(list, "image%03i.pnm", count);

	return (GP_OK);
}



static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data)
{
	Camera *camera = user_data;
	int image_no, result;
	char *data;
	long int size;

	image_no = gp_filesystem_number(camera->fs, folder, filename);
	if(image_no < 0)
		return image_no;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		result = stv0680_get_image (camera->port, image_no, &data,
					    (int*) &size);
		break;
	case GP_FILE_TYPE_PREVIEW:
		result = stv0680_get_image_preview (camera->port, image_no,
						&data, (int*) &size);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	if (result < 0)
		return result;

	gp_file_set_name (file, filename);
	gp_file_set_mime_type (file, "image/pnm"); 
	gp_file_set_data_and_size (file, data, size);

	return (GP_OK);
}

static int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path)
{
	int result;
	int count,oldcount;

	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);
	result = stv0680_file_count(camera->port,&oldcount);

	result = stv0680_capture(camera->port);
	if (result < 0)
		return result;

	/* Just added a new picture... */
	result = stv0680_file_count(camera->port,&count);
	if (count == oldcount)
	    return GP_ERROR; /* unclear what went wrong  ... hmm */
	strcpy(path->folder,"/");
	sprintf(path->name,"image%03i.pnm",count);
	return (GP_OK);
}

static int camera_capture_preview (Camera *camera, CameraFile *file)
{
	char *data;
	int result;
	long int size;

	result = stv0680_capture_preview (camera->port, &data, (int*) &size);
	if (result < 0)
		return result;

	gp_file_set_name (file, "capture.pnm");
	gp_file_set_mime_type (file, "image/pnm"); 
	gp_file_set_data_and_size (file, data, size);
	
	return (GP_OK);
}

static int camera_summary (Camera *camera, CameraText *summary) 
{
	stv0680_summary(camera->port,summary->text);
	return (GP_OK);
}

static int camera_manual (Camera *camera, CameraText *manual) 
{
	strcpy(manual->text, _("No manual available."));

	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about) 
{
	strcpy (about->text, 
		_("STV0680\n"
		"Adam Harrison <adam@antispin.org>\n"
		"Driver for cameras using the STV0680 processor ASIC.\n"
		"Protocol reverse engineered using CommLite Beta 5\n"
		"Carsten Weinholz <c.weinholz@netcologne.de>\n"
		"Extended for Aiptek PenCam and other STM USB Dual-mode cameras."));

	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char* folder, void *data)
{
        Camera *camera = data;
	if (strcmp (folder, "/"))
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	return stv0680_delete_all(camera->port);
}

int camera_init (Camera *camera) 
{
	GPPortSettings settings;
        int ret;

        /* First, set up all the function pointers */
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;
	camera->functions->capture_preview	= camera_capture_preview;
	camera->functions->capture		= camera_capture;

	gp_port_get_settings(camera->port, &settings);
	switch(camera->port->type) {
	case GP_PORT_SERIAL:		
        	gp_port_set_timeout(camera->port, 1000);
        	settings.serial.bits = 8;
        	settings.serial.parity = 0;
        	settings.serial.stopbits = 1;
		break;
	case GP_PORT_USB:
    		settings.usb.inep = 0x82;
    		settings.usb.outep = 0x00;
    		settings.usb.config = 1;
    		settings.usb.interface = 0;
    		settings.usb.altsetting = 1;
		break;
	default:
		return (GP_ERROR_UNKNOWN_PORT);
		break;
	}
	gp_port_set_settings(camera->port, settings);

	/* Set up the filesystem */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	gp_filesystem_set_folder_funcs (camera->fs, NULL, delete_all_func, NULL, NULL, camera);

        /* test camera */
        ret = stv0680_ping(camera->port);
	return (ret);
}
