/****************************************************************/
/*library.c - Gphoto2 library for Benq DC1300 cameras and       */
/*            friends                                           */
/*                                                              */
/* Author: Till Adam <till@adam-lilienthal.de>                  */
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

#include "benq.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2.h>
#include <gphoto2-port.h>

#define GP_MODULE "benq"

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
}
models[] =
{
	{"Benq:DC1300", 0x04a5, 0x3003},
	{ NULL, 0, 0}
};

static int delete_all_func (CameraFilesystem *fs, const char *folder,
			    void *data, GPContext *context);


int
camera_id (CameraText *id)
{
	strcpy (id->text, "benq");
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
		a.file_operations = GP_FILE_OPERATION_DELETE;
		gp_abilities_list_append (list, a);
	}
	return GP_OK;
}


static int
file_list_func (CameraFilesystem *fs, const char *folder,
		CameraList *list, void *data, GPContext *context)
{

	Camera *camera = data;
	int i = 0, filecount = 0;
	char temp_file[14];

	CHECK (benq_get_TOC(camera->pl, &filecount));
	for (i=0; i<filecount; i++)
	{
		CHECK(benq_get_file_name (camera->pl, i, temp_file));	
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
			CHECK (benq_get_file (camera->pl, context, 
						&data, &size, number, 0));		
			break;
		case GP_FILE_TYPE_PREVIEW:
			CHECK (benq_get_file (camera->pl, context, 
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
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text,
		("benq camlib\n"
		 "Author: Till Adam\n <till@adam-lilienthal.de>\n"));
	return GP_OK;
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
		CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n, filecount;
	unsigned int size;
	char name[14];

	/* Get the file number from the CameraFileSystem */
	CHECK (n =
		gp_filesystem_number (camera->fs, folder, filename, context));

	CHECK (benq_get_TOC(camera->pl, &filecount));
	CHECK (benq_get_file_name(camera->pl, n, name));
	CHECK (benq_get_file_size(camera->pl, n, &size));

	info->file.fields = GP_FILE_INFO_NAME | GP_FILE_INFO_SIZE;
	strncpy (info->file.name, name, sizeof (info->file.name));
	info->file.size = size;
	
	return GP_OK;
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;

	CHECK (benq_delete_all (camera->port));
	return GP_OK;
}

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	int ret = 0;

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
	CHECK (benq_init (camera->port, context));

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
        if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	       
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
	camera->pl->gpdev = camera->port;
	
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);
	gp_filesystem_set_folder_funcs
	       (camera->fs, NULL, delete_all_func, NULL, NULL, camera);


	return GP_OK;
}
