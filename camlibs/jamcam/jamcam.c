/****************************************************************/
/* jamcam.c - Gphoto2 library for the KBGear JamCam v2 and v3   */
/*                                                              */
/* Copyright (C) 2001 Chris Pinkham                             */
/*                                                              */
/* Author: Chris Pinkham <cpinkham@infi.net>                    */
/*                                                              */
/* This program is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU General Public   */
/* License as published by the Free Software Foundation; either */
/* version 2 of the License, or (at your option) any later      */
/* version.                                                     */
/*                                                              */
/* This program is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU General Public License for more        */
/* details.                                                     */
/*                                                              */
/* You should have received a copy of the GNU General Public    */
/* License along with this program; if not, write to the Free   */
/* Software Foundation, Inc., 59 Temple Place, Suite 330,       */
/* Boston, MA 02111-1307, USA.                                  */
/****************************************************************/

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2.h>
#include <libgphoto2/bayer.h>
#include <libgphoto2/gamma.h>

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

#include "library.h"

#define TIMEOUT	      2000

#define JAMCAM_VERSION "0.5"
#define JAMCAM_LAST_MOD "11/08/2001 08:13 EST"

/* define what cameras we support */
static struct {
	char *model;
	int usb_vendor;
	int usb_product;
} models[] = {
	{ "KBGear JamCam", 0x084E, 0x0001 },
	{ NULL, 0, 0 }
};

int camera_id (CameraText *id)
{
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* camera_id");

	strcpy (id->text, "jamcam");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list)
{
	int x = 0;
	char *ptr;
	CameraAbilities a;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* camera_abilities");

	ptr = models[x].model;
	while (ptr) {
		strcpy (a.model, ptr );
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port     = GP_PORT_SERIAL | GP_PORT_USB;
		a.speed[0] = 57600;
		a.speed[1] = 0;

		/* fixme, need to set these operations lists to correct values */
		a.operations        = 	GP_OPERATION_NONE;
		a.file_operations   = 	GP_FILE_OPERATION_PREVIEW;

		a.folder_operations = 	GP_FOLDER_OPERATION_NONE;

		a.usb_vendor = models[x].usb_vendor;
		a.usb_product = models[x].usb_product;

		gp_abilities_list_append (list, a);

		ptr = models[++x].model;
	}

	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			      CameraList *list, void *data)
{
	Camera *camera = data;
	int count;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* file_list_func");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** folder: %s", folder);

	CHECK (count = jamcam_file_count (camera));
	CHECK (gp_list_populate (list, "pic_%04i.ppm", count));

	return (GP_OK);
}

static int get_info_func (CameraFilesystem *fs, const char *folder,
		const char *filename, CameraFileInfo *info, void *data)
{
	Camera *camera = data;
	int n;
	struct jamcam_file *jc_file;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* get_info_func");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** filename: %s",filename);

	/* Get the file number from the CameraFileSystem */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename));

	jc_file = jamcam_file_info( camera, n );

	/* fixme, get file size also */
	info->file.fields = GP_FILE_INFO_TYPE;
	strcpy (info->file.type, GP_MIME_PPM);
	info->file.width = jc_file->width;
	info->file.height = jc_file->height;

	info->preview.fields = GP_FILE_INFO_TYPE;
	strcpy (info->preview.type, GP_MIME_PPM);
	info->preview.width = 80;
	info->preview.height = 60;

	return (GP_OK);
}

static int camera_exit (Camera *camera)
{
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* camera_exit");

	return (GP_OK);
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data)
{
	Camera *camera = user_data;
	char raw[265000];
	char ppm[265000 * 3];
	char tmp_filename[128];
	unsigned char gtable[256];
	char *ptr;
	int size = 0, n = 0;
	int width, height;
	struct jamcam_file *jc_file;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* camera_file_get");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** filename: %s",filename);
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** type: %d", type);

	CHECK (n = gp_filesystem_number (camera->fs, folder, filename));

	gp_camera_progress( camera, 0 );

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		CHECK (jamcam_request_thumbnail (camera, raw, &size, n));

		width = 80;
		height = 60;

		sprintf( ppm,
			"P6\n"
			"# CREATOR: gphoto2, jamcam library\n"
			"%d %d\n"
			"255\n", width, height );

		ptr = ppm + strlen( ppm );

		size = strlen( ppm ) + ( height * width * 3 );

		gp_bayer_decode(raw, width, height, ptr, BAYER_TILE_GBRG );
		gp_gamma_fill_table( gtable, 0.5 );
		gp_gamma_correct_single( gtable, ptr, height * width );

		CHECK (gp_file_set_mime_type (file, GP_MIME_PPM));
		CHECK (gp_file_set_name (file, filename));
		CHECK (gp_file_append (file, ppm, size));
		break;

	case GP_FILE_TYPE_NORMAL:
		CHECK (jamcam_request_image (camera, raw, &size, n));

		jc_file = jamcam_file_info (camera, n);

		sprintf( ppm,
			"P6\n"
			"# CREATOR: gphoto2, jamcam library\n"
			"%d %d\n"
			"255\n", jc_file->width, jc_file->height );

		ptr = ppm + strlen( ppm );

		size = strlen( ppm ) + ( jc_file->width * jc_file->height * 3 );

		gp_bayer_decode( raw, jc_file->width, jc_file->height, ptr,
			BAYER_TILE_GBRG );
		gp_gamma_fill_table( gtable, 0.5 );
		gp_gamma_correct_single( gtable, ptr, jc_file->width * jc_file->height );

		CHECK (gp_file_set_mime_type (file, GP_MIME_PPM));
		CHECK (gp_file_set_name (file, filename));
		CHECK (gp_file_append (file, ppm, size));
		break;

	case GP_FILE_TYPE_RAW:
		CHECK (jamcam_request_image (camera, raw, &size, n));
		CHECK (gp_file_set_mime_type (file, GP_MIME_RAW));
		strcpy( tmp_filename, filename );
		tmp_filename[strlen(tmp_filename)-3] = 'r';
		tmp_filename[strlen(tmp_filename)-2] = 'a';
		tmp_filename[strlen(tmp_filename)-1] = 'w';
		CHECK (gp_file_set_name (file, tmp_filename));
		CHECK (gp_file_append (file, raw, size));
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	return (GP_OK);
}

static int camera_summary (Camera *camera, CameraText *summary)
{
	int count;
	char tmp[1024];

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* camera_summary");

	/* possibly get # pics, mem free, etc. */
	count = jamcam_file_count (camera);

	sprintf (tmp, "Frames Taken     : %4d\n", count);
	strcat (summary->text, tmp );

	return (GP_OK);
}

static int camera_manual (Camera *camera, CameraText *manual)
{
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* camera_manual");

	strcpy (manual->text, _("Some notes:\n"
		"    No notes here yet.\n"));
	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about)
{
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* camera_about");

	strcpy (about->text,
		_("jamcam library v" JAMCAM_VERSION
		" " JAMCAM_LAST_MOD "\n"
		"Chris Pinkham <cpinkham@infi.net>\n"
		"Support for KBGear JamCam v2.0 & v3.0 digital cameras\n"
		"based on reverse engineering serial protocol.\n"
		"\n"));

	return (GP_OK);
}

int camera_init (Camera *camera)
{
	int count;
	GPPortSettings settings;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* camera_init");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam",
		"   * jamcam library for Gphoto2 by Chris Pinkham <cpinkham@infi.net>");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam",
		"   * jamcam library v%s, %s", JAMCAM_VERSION, JAMCAM_LAST_MOD );

	/* First, set up all the function pointers */
	camera->functions->exit 	= camera_exit;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	CHECK (gp_port_get_settings (camera->port, &settings));
	switch( camera->port->type ) {
	case GP_PORT_SERIAL:
		settings.serial.speed 	 = 57600;
		settings.serial.bits 	 = 8;
		settings.serial.parity 	 = 0;
		settings.serial.stopbits = 1;
		break;
	case GP_PORT_USB:
		settings.usb.inep        = 0x81;
		settings.usb.outep       = 0x02;
		settings.usb.config      = 1;
		settings.usb.interface   = 0;
		settings.usb.altsetting  = 0;
		break;
	default:
		fprintf(stderr, "Unknown port type: %d\n", camera->port->type );
		return ( GP_ERROR );
		break;
	}
	CHECK (gp_port_set_settings (camera->port, settings));

	CHECK (gp_port_set_timeout (camera->port, TIMEOUT));

	/* check to see if camera is really there */
	CHECK (jamcam_enq (camera));

	/* get number of images */
	CHECK (count = jamcam_file_count (camera));

	/* Set up the CameraFilesystem */
	CHECK (gp_filesystem_set_list_funcs (camera->fs,
		file_list_func, NULL, camera));
	CHECK (gp_filesystem_set_info_funcs (camera->fs,
		get_info_func, NULL, camera));
	CHECK (gp_filesystem_set_file_funcs (camera->fs, get_file_func,
		NULL, camera));

	return (GP_OK);
}

