/****************************************************************/
/* library.c - Gphoto2 library for cameras with sunplus spca50x */
/*             chips                                            */
/* Copyright (C) 2002, 2003 Till Adam                           */
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
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/
#define _BSD_SOURCE

#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

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

#include "spca50x.h"
#include "spca50x-flash.h"
#include "spca50x-sdram.h"

#define GP_MODULE "spca50x"
#define TIMEOUT	      5000

#define SPCA50X_VERSION "0.1"
#define SPCA50X_LAST_MOD "02/09/03 - 23:14:11"

/* forward declarations */
static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context);
static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data,
			  GPContext *context);

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data,
			     GPContext *context);
static int delete_all_func (CameraFilesystem *fs, const char *folder,
			    void *data, GPContext *context);
static int get_info_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileInfo *info,
			  void *data, GPContext *context);
static int cam_has_sdram (CameraPrivateLibrary *pl);
static int cam_has_flash (CameraPrivateLibrary *pl);
static int cam_has_card (CameraPrivateLibrary *pl);

/* define what cameras we support */
static const struct
{
	char *model;
	int usb_vendor;
	int usb_product;
	SPCA50xBridgeChip bridge;
	int storage_media_mask;
}
models[] =
{
	/* firmware version 1 cams. */
	{"Mustek:gSmart mini", 0x055f, 0xc220,
		BRIDGE_SPCA500, SPCA50X_SDRAM },
	{"Mustek:gSmart mini 2", 0x055f, 0xc420,
		BRIDGE_SPCA504, SPCA50X_SDRAM },
	{"Mustek:gSmart mini 3", 0x055f, 0xc520,
		BRIDGE_SPCA504, SPCA50X_SDRAM },
	{"So.:Show 301", 0x0ec7, 0x1008,
		BRIDGE_SPCA504, SPCA50X_SDRAM },
	{"Aiptek:Pencam", 0x04fc, 0x504a,
		BRIDGE_SPCA504, SPCA50X_SDRAM | SPCA50X_FLASH },
	/* same ids, different cam *sigh* */
	{"Aiptek:Pencam without flash", 0x04fc, 0x504a,
		BRIDGE_SPCA504, SPCA50X_SDRAM },
	{"Medion:MD 5319", 0x04fc, 0x504a,
		BRIDGE_SPCA504, SPCA50X_SDRAM | SPCA50X_FLASH },
	{"nisis:Quickpix Qp3", 0x04fc, 0x504a,
		BRIDGE_SPCA504, SPCA50X_SDRAM | SPCA50X_FLASH },
	{"Trust:Spyc@m 500F FLASH", 0x04fc, 0x504a,
		BRIDGE_SPCA504, SPCA50X_SDRAM | SPCA50X_FLASH },
	/* The firmware 2 cams. Those can autodetect their storage type */
	{"Aiptek:1.3 mega PocketCam", 0x04fc, 0x504b,
		BRIDGE_SPCA504, 0 },
	{"Maxell:Max Pocket", 0x04fc, 0x504b,
		BRIDGE_SPCA504, 0 },
	{"Aiptek:Smart Megacam", 0x04fc, 0x504b,
		BRIDGE_SPCA504, 0 },
	{"Benq:DC1300", 0x04a5, 0x3003,
	        BRIDGE_SPCA504, 0 },
	/* Some other 500a cams with flash */
	{"Trust:Familycam 300", 0x084d, 0x0003,
		BRIDGE_SPCA500, SPCA50X_FLASH},
	{"D-Link:DSC 350+", 0x084d, 0x0003,
		BRIDGE_SPCA500, SPCA50X_FLASH},
        {"Minton:S-Cam F5", 0x084d, 0x0003,
	        BRIDGE_SPCA500, SPCA50X_FLASH},	
	{"PureDigital:Ritz Disposable", 0x04fc, 0xffff,
		BRIDGE_SPCA504B_PD, SPCA50X_FLASH},
	{NULL, 0, 0, 0, 0}
};

int
camera_id (CameraText *id)
{
	strcpy (id->text, "spca50x");
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
		a.port = GP_PORT_USB;
		a.speed[0] = 0;
		a.status = GP_DRIVER_STATUS_TESTING;

		a.file_operations = GP_FILE_OPERATION_PREVIEW
			          | GP_FILE_OPERATION_DELETE;

		a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

		a.usb_vendor = models[x].usb_vendor;
		a.usb_product = models[x].usb_product;

		if (models[x].bridge == BRIDGE_SPCA504) {
			/* FIXME which cams can do it? */
			if (a.usb_product == 0xc420
			 || a.usb_product == 0xc520)
				a.operations = GP_OPERATION_CAPTURE_IMAGE;
		}
		if (models[x].bridge == BRIDGE_SPCA504B_PD) {
                            a.operations = GP_OPERATION_CAPTURE_IMAGE;
                }
		if (models[x].bridge == BRIDGE_SPCA500) {
			/* TEST enable capture for the DSC-350 style cams */
			if (a.usb_vendor == 0x084d) {
				a.operations = GP_OPERATION_CAPTURE_IMAGE;
			}
		}
		gp_abilities_list_append (list, a);

		ptr = models[++x].model;
	}

	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type,
		CameraFilePath * path, GPContext *context)
{
	struct SPCA50xFile *file;
	CameraAbilities a;

	/* Not all our cameras support capture */
	gp_camera_get_abilities (camera, &a);
	if (!(a.operations & GP_OPERATION_CAPTURE_IMAGE))
		return GP_ERROR_NOT_SUPPORTED;

	if (cam_has_flash(camera->pl))
	{
		int fc;
		char tmp [14];

		CHECK(spca500_flash_capture (camera->pl));
		CHECK(spca50x_flash_get_TOC (camera->pl, &fc));
		/* assume new pic is the last one in the cam...*/
		CHECK(spca50x_flash_get_file_name (camera->pl, (fc - 1), tmp));

		/* Add new image name to file list */
		/* NOTE: these lines moved from below */
		strncpy (path->name, tmp, sizeof (path->name) - 1);
		path->name[sizeof (path->name) - 1] = '\0';
	}
	else
	{
		CHECK (spca50x_capture (camera->pl));
		CHECK (spca50x_sdram_get_info (camera->pl));
		CHECK (spca50x_sdram_get_file_info
			(camera->pl, camera->pl->num_files_on_sdram - 1, &file));

		/* Add new image name to file list */
		/* NOTE: these lines moved from below */
		strncpy (path->name, file->name, sizeof (path->name) - 1);
		path->name[sizeof (path->name) - 1] = '\0';
	}
	/* Now tell the frontend where to look for the image */
	strncpy (path->folder, "/", sizeof (path->folder) - 1);
	path->folder[sizeof (path->folder) - 1] = '\0';

	CHECK (gp_filesystem_append
	       (camera->fs, path->folder, path->name, context));
	return GP_OK;
}


static int
camera_exit (Camera *camera, GPContext *context)
{
	if (camera->pl) {
		if (cam_has_flash (camera->pl) || cam_has_card (camera->pl))
			spca50x_flash_close (camera->pl, context);

		if (camera->pl->fats) {
			free (camera->pl->fats);
			camera->pl->fats = NULL;
		}
		if (camera->pl->files) {
			free (camera->pl->files);
			camera->pl->files = NULL;
		}
		if (camera->pl->flash_toc) {
			free (camera->pl->flash_toc);
			camera->pl->flash_toc = NULL;
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
	int flash_file_count;

	if (cam_has_flash(camera->pl)  || cam_has_card (camera->pl)) {
		spca50x_flash_get_filecount(camera->pl, &flash_file_count);
		snprintf (tmp, sizeof (tmp),
			_("FLASH:\n Files: %d\n"), flash_file_count);
		strcat (summary->text, tmp);
	}

	/* possibly get # pics, mem free, etc. if needed */
	if (cam_has_sdram(camera->pl) && camera->pl->dirty_sdram) {
		CHECK (spca50x_sdram_get_info (camera->pl));

		snprintf (tmp, sizeof (tmp),
				_("SDRAM:\n Files: %d\n  Images: %4d\n  Movies: %4d\nSpace used: %8d\nSpace free: %8d\n"),
				camera->pl->num_files_on_sdram,
				camera->pl->num_images,
				camera->pl->num_movies,
				camera->pl->size_used,
				camera->pl->size_free);
		strcat (summary->text, tmp);
	}

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text,
		_("spca50x library v" SPCA50X_VERSION
		  " " SPCA50X_LAST_MOD "\n"
		  "Till Adam <till@adam-lilienthal.de>\n"
		  "Support for digital cameras with a sunplus spca50x chip "
		  "based on several other gphoto2 camlib modules and "
		  "the information kindly provided by Mustek.\n"
		  "\n"));

	return (GP_OK);
}




static int
file_list_func (CameraFilesystem *fs, const char *folder,
		CameraList *list, void *data, GPContext *context)
{

	Camera *camera = data;
	int i = 0, filecount = 0;
	char temp_file[14];

	if (cam_has_flash(camera->pl) || cam_has_card(camera->pl) )
	{
		CHECK (spca50x_flash_get_TOC(camera->pl, &filecount));
		for (i=0; i<filecount; i++)
		{
			CHECK(spca50x_flash_get_file_name (camera->pl, i,
						temp_file));
			gp_list_append (list, temp_file, NULL);
		}
	}
	if (cam_has_sdram(camera->pl)) {
		if (camera->pl->dirty_sdram)
			CHECK (spca50x_sdram_get_info (camera->pl));

		for (i = 0; i < camera->pl->num_files_on_sdram; i++) {
			strncpy (temp_file, camera->pl->files[i].name, 12);
			temp_file[12] = 0;
			gp_list_append (list, temp_file, NULL);
		}
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
	int number, filetype, flash_file_count = 0;
	unsigned int size;

	CHECK (number =
	       gp_filesystem_number (camera->fs, folder, filename, context));

	if (cam_has_flash(camera->pl) || cam_has_card(camera->pl) ) {
		CHECK (spca50x_flash_get_filecount
					(camera->pl, &flash_file_count));
	}

	switch (type) {
		case GP_FILE_TYPE_NORMAL:
			if ( number < flash_file_count) {
				CHECK (spca50x_flash_get_file (camera->pl,
							context, &data, &size,
							number, 0));
				CHECK (gp_file_set_mime_type
						(file, GP_MIME_JPEG));

			} else {
				CHECK (spca50x_sdram_request_file
						(camera->pl, &data, &size,
						 number-flash_file_count,
						 &filetype));
				if (filetype == SPCA50X_FILE_TYPE_IMAGE) {
					CHECK (gp_file_set_mime_type
							(file, GP_MIME_JPEG));
				} else if (filetype == SPCA50X_FILE_TYPE_AVI) {
					CHECK (gp_file_set_mime_type
							(file, GP_MIME_AVI));
				}
			}
			break;
		case GP_FILE_TYPE_PREVIEW:
		       	 if ( number < flash_file_count) {
				CHECK (spca50x_flash_get_file (camera->pl,
							context, &data, &size,
							number, 1));
				CHECK (gp_file_set_mime_type (file,
							GP_MIME_BMP));

			} else {
				CHECK (spca50x_sdram_request_thumbnail
						(camera->pl, &data, &size,
						 number-flash_file_count,
						 &filetype));
				if (filetype == SPCA50X_FILE_TYPE_IMAGE) {
					CHECK (gp_file_set_mime_type
							(file, GP_MIME_BMP));
				} else if (filetype == SPCA50X_FILE_TYPE_AVI) {
					CHECK (gp_file_set_mime_type
							(file, GP_MIME_JPEG));
				}
			}

			break;
		default:
			return GP_ERROR_NOT_SUPPORTED;
	}
	if (!data)
		return GP_ERROR;
	return gp_file_set_data_and_size (file, (char *)data, size);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, CameraFileInfo *info, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int n, flash_file_count = 0;
	struct SPCA50xFile *file;
	char name[14];
	int w,h;

	/* Get the file number from the CameraFileSystem */
	CHECK (n =
	       gp_filesystem_number (camera->fs, folder, filename, context));

	if (cam_has_flash(camera->pl) || cam_has_card(camera->pl) ) {
		CHECK (spca50x_flash_get_TOC(camera->pl,
					&flash_file_count));
	}
	if (n < flash_file_count) {
		CHECK (spca50x_flash_get_file_name(camera->pl,
					n, name));
		CHECK (spca50x_flash_get_file_dimensions(
					camera->pl, n, &w, &h));
		strcpy (info->file.type, GP_MIME_JPEG);
		info->file.width = w;
		info->file.height = h;
		info->preview.width = w/8;
		info->preview.height = h/8;
	}
	if (cam_has_sdram (camera->pl) && n >= flash_file_count ){
		CHECK (spca50x_sdram_get_file_info (camera->pl,
					n-flash_file_count, &file));
		if (file->mime_type == SPCA50X_FILE_TYPE_IMAGE) {
			strcpy (info->file.type, GP_MIME_JPEG);
			info->preview.width = 160;
			info->preview.height = 120;
		} else if (file->mime_type == SPCA50X_FILE_TYPE_AVI) {
			strcpy (info->file.type, GP_MIME_AVI);
			info->preview.width = 320;
			info->preview.height = 240;
		}
		info->file.width = file->width;
		info->file.height = file->height;

	}
	info->file.fields =
		GP_FILE_INFO_TYPE
		| GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;

	info->file.mtime = 0;
	info->file.fields |= GP_FILE_INFO_MTIME;

	info->preview.fields =
		GP_FILE_INFO_TYPE | GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;
	strcpy (info->preview.type, GP_MIME_BMP);
	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int n, c, flash_file_count;

	/* FIXME deleting a single file for flash/card cams should work */
	/* Get the file number from the CameraFileSystem */
	CHECK (n =
	       gp_filesystem_number (camera->fs, folder, filename, context));

	if (cam_has_flash(camera->pl) || cam_has_card(camera->pl) ) {
		CHECK (spca50x_flash_get_filecount
					(camera->pl, &flash_file_count));
	} else {
		/* should not happen really */
		return GP_ERROR;
	}
	if (n < flash_file_count) {
		return spca500_flash_delete_file (camera->pl, n);
	}

	CHECK (c = gp_filesystem_count (camera->fs, folder, context));
	if (n + 1 != c) {
		const char *name;

		gp_filesystem_name (fs, "/", c - 1, &name, context);
		gp_context_error (context,
				  _("Your camera only supports deleting the "
				   "last file on the camera. In this case, this "
				   "is file '%s'."), name);
		return (GP_ERROR);
	}
	CHECK (spca50x_sdram_delete_file (camera->pl, n));
	return GP_OK;
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;

	if (cam_has_sdram (camera->pl))
		CHECK (spca50x_sdram_delete_all (camera->pl));
	if (cam_has_flash(camera->pl) || cam_has_card(camera->pl) )
		CHECK (spca50x_flash_delete_all (camera->pl, context));

	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func,
};

int
camera_init (Camera *camera, GPContext *context)
{
	int ret = 0;
	int x = 0;
	char *model;

	GPPortSettings settings;
	CameraAbilities abilities;

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
	camera->pl->dirty_sdram = 1;
	camera->pl->dirty_flash = 1;

	/* What bridge chip is inside the camera? The gsmart mini is spca500
	 * based, while the others have a spca50xa */
	gp_camera_get_abilities (camera, &abilities);
	model = models[x].model;
	while (model) {
		if (abilities.usb_vendor == models[x].usb_vendor
		 && abilities.usb_product == models[x].usb_product) {
			int same;
			char *m = strdup( models[x].model );
			char *p = strchr (m, ':' );

			if (p) *p = ' ';
			same = !strcmp (m, abilities.model);
			free (m);
			if (same) {
				camera->pl->bridge = models[x].bridge;
				camera->pl->storage_media_mask =
					models[x].storage_media_mask;
				break;
			}
		}
		model = models[++x].model;
	}

	CHECK (spca50x_get_firmware_revision (camera->pl));
	if (camera->pl->fw_rev > 1) {
		CHECK (spca50x_detect_storage_type (camera->pl));
	}
   
	if (cam_has_flash(camera->pl) || cam_has_card(camera->pl) ) {
		if ((camera->pl->bridge == BRIDGE_SPCA504) ||
		    (camera->pl->bridge == BRIDGE_SPCA504B_PD))
			CHECK (spca50x_flash_init (camera->pl, context));
	}

	if ((camera->pl->bridge == BRIDGE_SPCA504) ||
	    (camera->pl->bridge == BRIDGE_SPCA504B_PD)) {
/*		if (abilities.usb_vendor != 0x04fc && abilities.usb_product != 0x504a ) */
		if (!(abilities.usb_vendor == 0x04fc && abilities.usb_product == 0x504a ))
			ret = spca50x_reset (camera->pl);
	}

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
cam_has_sdram (CameraPrivateLibrary *pl)
{
	return pl->storage_media_mask & SPCA50X_SDRAM;
}

static int
cam_has_flash (CameraPrivateLibrary *pl)
{
	return pl->storage_media_mask & SPCA50X_FLASH;
}

static int
cam_has_card (CameraPrivateLibrary *pl)
{
	return pl->storage_media_mask & SPCA50X_CARD;
}
