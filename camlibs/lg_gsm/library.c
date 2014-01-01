/* library.c
 *
 * Copyright (C) 2003 Theodore Kilgore <kilgota@auburn.edu>
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

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "lg_gsm.h"
#include <gphoto2/gphoto2-port.h>

#define GP_MODULE "lg_gsm"

struct _CameraPrivateLibrary {
	Model model;
	Info info[40];
};

static struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"LG T5100", GP_DRIVER_STATUS_EXPERIMENTAL, 0x1004, 0x6005},
	{NULL,0,0}
};

int
camera_id (CameraText *id)
{
    	strcpy (id->text, "LG GSM camera");
    	return GP_OK;
}

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].name; i++) {
		memset (&a, 0, sizeof(a));
		strcpy (a.model, models[i].name);
		a.status = models[i].status;
		a.port   = GP_PORT_USB;
		a.speed[0] = 0;
		a.usb_vendor = models[i].idVendor;
		a.usb_product= models[i].idProduct;
		a.operations = GP_OPERATION_NONE;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		a.file_operations   = GP_FILE_OPERATION_NONE;
		gp_abilities_list_append (list, a);
	}
	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{

	char firmware[20];
	char firmware_version[20];
	memcpy(firmware,&camera->pl->info[0],20);
	memcpy(firmware_version,&camera->pl->info[20],20);


	sprintf (summary->text,_("Your USB camera seems to be a LG GSM.\n"
			"Firmware: %s\n"
			"Firmware Version: %s\n"
			), firmware, firmware_version);

	return GP_OK;
}


static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
    	strcpy (about->text, _("LG GSM generic driver\n"
			    "Guillaume Bedot <littletux@zarb.org>\n"));
    	return GP_OK;
}

/*************** File and Downloading Functions *******************/

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
	Camera *camera = data;
	lg_gsm_list_files (camera->port, list);

	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data; 

        int k;
	char *data;
	int len;

	k = gp_filesystem_number(camera->fs, "/", filename, context);

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		return GP_ERROR_NOT_SUPPORTED;
	case GP_FILE_TYPE_NORMAL:
		len = lg_gsm_get_picture_size (camera->port, k);
		GP_DEBUG("len = %i\n", len);
		data = malloc(len);
		if (!data) {
			GP_DEBUG("malloc failed\n");
			return GP_ERROR_NO_MEMORY;
		}
		lg_gsm_read_picture_data (camera->port, data, len, k);
		gp_file_append (file, data, len);
		free (data);
		break;
	default:
		return GP_ERROR_NOT_SUPPORTED;

	}

	return GP_OK;
}

/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("LG GSM camera_exit");

	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func
};

int
camera_init(Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	int ret = 0;

	/* First, set up all the function pointers */
	camera->functions->summary      = camera_summary;
	camera->functions->about        = camera_about;
	camera->functions->exit	    = camera_exit;
   
	GP_DEBUG ("Initializing the camera\n");
	ret = gp_port_get_settings(camera->port,&settings);
	if (ret < 0) 
		return ret; 

	switch (camera->port->type) {
		case GP_PORT_SERIAL:
			return GP_ERROR;
		case GP_PORT_USB:
			settings.usb.config = 1;
			settings.usb.altsetting = 0;
			settings.usb.interface = 1;
			settings.usb.inep = 0x81;
			settings.usb.outep =0x02;
			break;
		default:
			return GP_ERROR;
	}

	ret = gp_port_set_settings(camera->port,settings);
	if (ret < 0) 
		return ret; 

	GP_DEBUG("interface = %i\n", settings.usb.interface);
	GP_DEBUG("inep = %x\n", settings.usb.inep);	
	GP_DEBUG("outep = %x\n", settings.usb.outep);

        /* Tell the CameraFilesystem where to get lists from */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl) 
		return GP_ERROR_NO_MEMORY;
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));

	/* Connect to the camera */
	lg_gsm_init (camera->port, &camera->pl->model, camera->pl->info);
	return GP_OK;
}
