/****************************************************************/
/* library.c - Gphoto2 library for the Mustek gSmart mini 2     */
/*                                                              */
/* Copyright (C) 2002 Till Adam                                 */
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

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2.h>

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

#include "gsmart.h"

#define GP_MODULE "gsmartmini2"
#define TIMEOUT	      5000

#define GSMART_VERSION "0.2"
#define GSMART_LAST_MOD "08/09/02 - 23:14:11"

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
static struct
{
	char *model;
	int usb_vendor;
	int usb_product;
}
models[] =
{
	{
	"Mustek gSmart mini 2", 0x055f, 0xc420}
	, {
	NULL, 0, 0}
};

int
camera_id (CameraText *id)
{
	strcpy (id->text, "gsmartmini2");
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

		a.operations = GP_OPERATION_CAPTURE_IMAGE;
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
camera_capture (Camera *camera, CameraCaptureType type,
		CameraFilePath * path, GPContext *context)
{
	struct GsmartFile *file;

	CHECK (gsmart_capture (camera->pl));
	CHECK (gsmart_get_info (camera->pl));
	CHECK (gsmart_get_file_info (camera->pl, camera->pl->num_files - 1, &file));

	/* Now tell the frontend where to look for the image */
	strncpy (path->folder, "/", sizeof (path->folder) - 1);
	path->folder[sizeof (path->folder) - 1] = '\0';
	strncpy (path->name, file->name, sizeof (path->name) - 1);
	path->name[sizeof (path->name) - 1] = '\0';

	CHECK (gp_filesystem_append (camera->fs, path->folder, path->name, context));

	return GP_OK;
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

	/* possibly get # pics, mem free, etc. if needed */
	if (camera->pl->dirty)
		CHECK (gsmart_get_info (camera->pl));

	snprintf (tmp, sizeof (tmp),
		  "Files: %d\n  Images: %4d\n  Movies: %4d\nSpace used: %8d\nSpace free: %8d\n",
		  camera->pl->num_files, camera->pl->num_images,
		  camera->pl->num_movies, camera->pl->size_used, camera->pl->size_free);
	strcat (summary->text, tmp);

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text,
		_("gsmart library v" GSMART_VERSION
		  " " GSMART_LAST_MOD "\n"
		  "Till Adam <till@adam-lilienthal.de>\n"
		  "Support for Mustek gSmart mini 2 digital cameras\n"
		  "based on several other gphoto2 camlib modules and "
		  "the windows driver source kindly provided by Mustek.\n" "\n"));

	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context)
{
	int ret;
	GPPortSettings settings;

	/* First, set up all the function pointers */
	camera->functions->exit = camera_exit;
	camera->functions->summary = camera_summary;
	camera->functions->about = camera_about;
	camera->functions->capture = camera_capture;

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
					  _("Unsupported port type: %d."
					    "This driver only works with USB"
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

	ret = gsmart_reset (camera->pl);

	if (ret < 0) {
		gp_context_error (context, _("Could not reset camera.\n"));
		free (camera->pl);
		camera->pl = NULL;

		return (ret);
	}

	/* Set up the CameraFilesystem */
	CHECK (gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera));
	CHECK (gp_filesystem_set_file_funcs (camera->fs, get_file_func,
					     delete_file_func, camera));
	CHECK (gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera));
	CHECK (gp_filesystem_set_folder_funcs (camera->fs,
					       NULL, delete_all_func, NULL, NULL, camera));

	return (GP_OK);
}


static int
file_list_func (CameraFilesystem *fs, const char *folder,
		CameraList *list, void *data, GPContext *context)
{

	Camera *camera = data;
	int i;
	char temp_file[14];

	if (camera->pl->dirty)
		CHECK (gsmart_get_info (camera->pl));

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
	unsigned char *data = NULL;
	int size, number;

	CHECK (number = gp_filesystem_number (camera->fs, folder, filename, context));

	switch (type) {
		case GP_FILE_TYPE_NORMAL:
			CHECK (gsmart_request_file (camera->pl, &data, &size, number));
			break;
		case GP_FILE_TYPE_PREVIEW:
			CHECK (gsmart_request_thumbnail (camera->pl, &data, &size, number));
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
get_info_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;
	struct GsmartFile *file;

	/* Get the file number from the CameraFileSystem */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename, context));

	CHECK (gsmart_get_file_info (camera->pl, n, &file));

	info->file.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;
	if (file->mime_type == FILE_TYPE_IMAGE)
		strcpy (info->file.type, GP_MIME_JPEG);
	else if (file->mime_type == FILE_TYPE_AVI)
		strcpy (info->file.type, GP_MIME_AVI);
	info->file.width = file->width;
	info->file.height = file->height;

	info->preview.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;
	strcpy (info->preview.type, GP_MIME_BMP);
	info->preview.width = 160;
	info->preview.height = 120;

	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int index;

	/* Get the file number from the CameraFileSystem */
	CHECK (index = gp_filesystem_number (camera->fs, folder, filename, context));
	CHECK (gsmart_delete_file (camera->pl, index));

	return GP_OK;
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data, GPContext *context)
{
	Camera *camera = data;

	CHECK (gsmart_delete_all (camera->pl));
	return GP_OK;
}
