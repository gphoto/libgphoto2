/****************************************************************/
/* library.c - Gphoto2 library for cameras with a sunplus       */
/*             spca504 chip and flash memory                    */
/*                                                              */
/*                                                              */
/* Authors: Till Adam <till@adam-lilienthal.de>                 */
/*                                                              */
/* based on work done by:                                       */
/*          Mark A. Zimmerman <mark@foresthaven.com>            */
/*                                                              */
/* This library is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU Library General  */
/* Public License as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any     */
/* later version.                                               */
/*                                                              */
/* This library is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU Library General Public License for     */
/* more details.                                                */
/*                                                              */
/* You should have received a copy of the GNU Library General   */
/* Public License along with this library; if not, write to the */
/* Free Software Foundation, Inc., 59 Temple Place - Suite 330, */
/* Boston, MA 02111-1307, USA.                                  */
/****************************************************************/

#include "config.h"

#include "spca504_flash.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2.h>
#include <gphoto2-port.h>

#define GP_MODULE "spca504_flash"

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

struct models
{
	char *name;
	unsigned short idVendor;
	unsigned short idProduct;
	char serial;
}
models[] =
{
	{"Aiptek Pencam", 0x04fc, 0x504a, 0},
	{"Medion MD 5319", 0x04fc, 0x504a, 0},
	{"nisis Quickpix Qp3", 0x04fc, 0x504a, 0}
	, {
	NULL, 0, 0, 0}
};

int
camera_id (CameraText *id)
{
	strcpy (id->text, "spca504_flash");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].name; i++) {
		memset (&a, 0, sizeof (CameraAbilities));
		strcpy (a.model, models[i].name);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port = GP_PORT_USB;
		a.speed[0] = 0;
		a.usb_vendor = models[i].idVendor;
		a.usb_product = models[i].idProduct;
		a.operations = GP_OPERATION_NONE;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		a.file_operations = GP_FILE_OPERATION_PREVIEW;
		gp_abilities_list_append (list, a);
	}
	return GP_OK;
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	int ret;

	ret = spca504_flash_close (camera->port, context);
	return ret;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder,
		CameraList *list, void *data, GPContext *context)
{

	Camera *camera = data;
	int i = 0, filecount = 0;
	char temp_file[14];

	CHECK (spca504_flash_get_TOC(camera->pl, &filecount));
	for (i=0; i<filecount; i++)
	{
		CHECK(spca504_flash_get_file_name (camera->pl, i, temp_file));	
		gp_list_append (list, temp_file, NULL);
	}
	
	return GP_OK;
}


static int
get_file_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, CameraFileType type,
	       CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
	unsigned char *data = NULL;
	int size, number;

	CHECK (number =
	       gp_filesystem_number (camera->fs, folder, filename, context));
	switch (type) {
		case GP_FILE_TYPE_NORMAL:
			CHECK (spca504_flash_get_file (camera->pl, context, 
						&data, &size, number, 0));		
			break;
		case GP_FILE_TYPE_PREVIEW:
			CHECK (spca504_flash_get_file (camera->pl, context, 
						&data, &size, number, 1));
			CHECK (gp_file_set_mime_type (file, GP_MIME_BMP));
			break;
		default:
			return GP_ERROR_NOT_SUPPORTED;
	}

	if (!data)
		return GP_ERROR;

	CHECK (gp_file_set_data_and_size (file, data, size));
	CHECK (gp_file_set_name (file, filename));

	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	return GP_ERROR_NOT_SUPPORTED;
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	strcpy (manual->text, _("Manual Not Implemented Yet"));
	return GP_OK;
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
        /* Translators: please translate "Krauss" to "Krauß" if possible. */
	strcpy (about->text,
		_
		("spca504_flash camlib\n Authors: Till Adam\n <till@adam-lilienthal.de>\nbased on work by Matthias Krauss\nand Mark A. Zimmerman <mark@foresthaven.com>"));
	return GP_OK;
}
static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
		CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n, filecount;
	int w, h;
	char name[14];

	/* Get the file number from the CameraFileSystem */
	CHECK (n =
		gp_filesystem_number (camera->fs, folder, filename, context));

	CHECK (spca504_flash_get_TOC(camera->pl, &filecount));
	CHECK (spca504_flash_get_file_name(camera->pl, n, name));
	CHECK (spca504_flash_get_file_dimensions(camera->pl, n, &w, &h));

	info->file.fields =
		GP_FILE_INFO_NAME | GP_FILE_INFO_TYPE | GP_FILE_INFO_WIDTH |
		GP_FILE_INFO_HEIGHT;
	strncpy (info->file.name, name, sizeof (info->file.name));
	strcpy (info->file.type, GP_MIME_JPEG);
	info->file.width = w;
	info->file.height = h;

	info->preview.fields =
		GP_FILE_INFO_TYPE | GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;
	info->preview.width = w/8;
	info->preview.height = w/8;
	strcpy (info->preview.type, GP_MIME_BMP);
	return GP_OK;
}


int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	int ret = 0;

	camera->functions->exit = camera_exit;
	camera->functions->summary = camera_summary;
	camera->functions->manual = camera_manual;
	camera->functions->about = camera_about;
	switch (camera->port->type) {
		case GP_PORT_USB:
			ret = gp_port_get_settings (camera->port, &settings);
			if (ret < 0)
				return ret;
			settings.usb.inep = 0x82;
			settings.usb.outep = 0x03;
			settings.usb.config = 1;
			settings.usb.interface = 1;
			settings.usb.altsetting = 0;
			ret = gp_port_set_settings (camera->port, settings);
			if (ret < 0)
				return ret;
			break;
		case GP_PORT_SERIAL:
			return GP_ERROR_IO_SUPPORTED_SERIAL;
		default:
			return GP_ERROR_NOT_SUPPORTED;
	}
	CHECK (spca504_flash_init (camera->port, context));

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
        if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	       
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
	camera->pl->gpdev = camera->port;
	camera->pl->dirty = 1;
	
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);
	return GP_OK;
}
