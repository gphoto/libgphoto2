/****************************************************************/
/* library.c - Gphoto2 library for the Mustek gSmart 300        */
/*                                                              */
/* Copyright (C) 2002 Jérôme Lodewyck                           */
/*                                                              */
/* Author: Jérôme Lodewyck <jerome.lodewyck@ens.fr>             */
/*                                                              */
/* based on code by: Till Adam <till@adam-lilienthal.de>        */
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
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/

#define _BSD_SOURCE

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

#include "gsmart300.h"

#define GP_MODULE "gsmart300"
#define TIMEOUT	      5000


/* forward declarations */
static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context);
static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data, GPContext *context);

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data, GPContext *context);
static int delete_all_func (CameraFilesystem *fs, const char *folder,
			    void *data, GPContext *context);
static int get_info_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileInfo *info,
			  void *data, GPContext *context);

/* define what cameras we support */
static const struct
{
	char *model;
	int usb_vendor;
	int usb_product;
}
models[] =
{
	{
	"Mustek:gSmart 300", 0x055f, 0xc200}
	, {
	"Casio:LV 10", 0x055f, 0xc200}
	, {

	NULL, 0, 0}
};

int
camera_id (CameraText *id)
{
	strcpy (id->text, "gsmart300");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	int x = 0;
	char *ptr;
	CameraAbilities a;

	ptr = models[x].model;
	while (ptr) {
		memset (&a, 0, sizeof (a));
		strcpy (a.model, ptr);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port = GP_PORT_USB;
		a.speed[0] = 0;

		a.file_operations = GP_FILE_OPERATION_PREVIEW | GP_FILE_OPERATION_DELETE;

		a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

		a.usb_vendor = models[x].usb_vendor;
		a.usb_product = models[x].usb_product;

		gp_abilities_list_append (list, a);

		ptr = models[++x].model;
	}

	return (GP_OK);
}


static int
camera_exit (Camera *camera, GPContext *context)
{
	if (camera->pl) {
		if (camera->pl->fats) {
			free (camera->pl->fats);
			camera->pl->fats = NULL;
		}
		if (camera->pl->files) {
			free (camera->pl->files);
			camera->pl->files = NULL;
		}

		free (camera->pl);
		camera->pl = NULL;
	}
	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	char tmp[1024];

	if (camera->pl->dirty)
		CHECK (gsmart300_get_info (camera->pl));

	snprintf (tmp, sizeof (tmp), "Files: %d\n\n", camera->pl->num_files);
	strcat (summary->text, tmp);

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text,
		_("gsmart300 library \n"
		  "Till Adam <till@adam-lilienthal.de>\n"
		  "Jerome Lodewyck <jerome.lodewyck@ens.fr>\n"
		  "Support for Mustek gSmart 300 digital cameras\n"
		  "based on several other gphoto2 camlib modules and "
		  "the specifications kindly provided by Mustek.\n" "\n"));

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func
};

int
camera_init (Camera *camera, GPContext *context)
{
	int ret;
	GPPortSettings settings;

	/* First, set up all the function pointers */
	camera->functions->exit = camera_exit;
	camera->functions->summary = camera_summary;
	camera->functions->about = camera_about;

	CHECK (gp_port_get_settings (camera->port, &settings));
	switch (camera->port->type) {
		case GP_PORT_USB:
			settings.usb.inep = 0x82;
			settings.usb.outep = 0x03;
			settings.usb.config = 1;
			settings.usb.interface = 0;
			settings.usb.altsetting = 0;

			CHECK (gp_port_set_settings (camera->port, settings));
			CHECK (gp_port_set_timeout (camera->port, TIMEOUT));

			break;
		default:
			gp_context_error (context,
					  _("Unsupported port type: %d. "
					    "This driver only works with USB "
					    "cameras.\n"), camera->port->type);
			return (GP_ERROR);
			break;
	}

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
	camera->pl->gpdev = camera->port;
	camera->pl->dirty = 1;

	ret = gsmart300_reset (camera->pl);

	if (ret < 0) {
		gp_context_error (context, _("Could not reset camera.\n"));
		free (camera->pl);
		camera->pl = NULL;

		return (ret);
	}
	/* Set up the CameraFilesystem */
	return gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
}


static int
file_list_func (CameraFilesystem *fs, const char *folder,
		CameraList *list, void *data, GPContext *context)
{
	Camera *camera = data;
	int i;
	char temp_file[14];

	if (camera->pl->dirty)
		CHECK (gsmart300_get_info (camera->pl));

	for (i = 0; i < camera->pl->num_files; i++) {
		strncpy (temp_file, camera->pl->files[i].name, 12);
		temp_file[12] = 0;
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
	int number, filetype;

	CHECK (number = gp_filesystem_number (camera->fs, folder, filename, 
				              context));
	switch (type) {
		case GP_FILE_TYPE_NORMAL:
			CHECK (gsmart300_request_file (camera->pl, file, number));
			break;
		case GP_FILE_TYPE_PREVIEW:
			CHECK (gsmart300_request_thumbnail
			       (camera->pl, file, number, &filetype));
			if (filetype == GSMART_FILE_TYPE_IMAGE) {
				CHECK (gp_file_set_mime_type (file, GP_MIME_BMP));
			}
			break;
		default:
			return GP_ERROR_NOT_SUPPORTED;
	}
	return GP_OK;
}

static int
get_info_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, CameraFileInfo *info, void *data, 
	       GPContext *context)
{
	Camera *camera = data;
	int n;
	struct GsmartFile *file;

	/* Get the file number from the CameraFileSystem */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename, 
				         context));

	CHECK (gsmart300_get_file_info (camera->pl, n, &file));

	info->file.fields = GP_FILE_INFO_TYPE 
		| GP_FILE_INFO_WIDTH 
		| GP_FILE_INFO_HEIGHT;
	if (file->mime_type == GSMART_FILE_TYPE_IMAGE) {
		strcpy (info->file.type, GP_MIME_JPEG);
		info->preview.width = 80;
		info->preview.height = 60;
	}
	info->file.width = file->width;
	info->file.height = file->height;

	info->preview.fields = GP_FILE_INFO_TYPE 
		| GP_FILE_INFO_WIDTH 
		| GP_FILE_INFO_HEIGHT;
	strcpy (info->preview.type, GP_MIME_BMP);

	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int n, c;

	/* Get the file number from the CameraFileSystem */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename, 
				         context));
	CHECK (c = gp_filesystem_count (camera->fs, folder, context));
	if (n + 1 != c) {
		const char *name;

		gp_filesystem_name (fs, "/", c - 1, &name, context);
		gp_context_error (context,
											_("Your camera only supports deleting " 
												"the last file on the camera. In this "
												"case, this is file '%s'."), name);
		return (GP_ERROR);
	}
	CHECK (gsmart300_delete_file (camera->pl, n));
	return GP_OK;
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data, 
		 GPContext *context)
{
	Camera *camera = data;

	CHECK (gsmart300_delete_all (camera->pl));
	return GP_OK;
}
