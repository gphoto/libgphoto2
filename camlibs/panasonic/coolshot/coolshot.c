/****************************************************************/
/* coolshot.c - Gphoto2 library for accessing the Panasonic     */
/*              Coolshot KXL-600A & KXL-601A digital cameras.   */
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
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "library.h"

#define TIMEOUT	      2000
#define DEFAULT_SPEED 9600

#define COOLSHOT_VERSION "0.4"
#define COOLSHOT_LAST_MOD "09/06/2001 02:28 EST"

/* define what cameras we support */
static char *coolshot_cameras[] = {
	"Panasonic Coolshot KXL-600A",
	"Panasonic Coolshot KXL-601A",
	""
};

int camera_id (CameraText *id)
{
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* camera_id");

	strcpy (id->text, "coolshot");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list)
{
	int x = 0;
	char *ptr;
	CameraAbilities *a;

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* camera_abilities");

	ptr = coolshot_cameras[x++];
	while (*ptr) {
		gp_abilities_new( &a );
		strcpy (a->model, ptr );
		a->port     = GP_PORT_SERIAL;
		a->speed[0] = 9600;
		a->speed[1] = 19200;
		a->speed[2] = 38400;
		a->speed[3] = 57600;
		a->speed[4] = 115200;
		a->speed[5] = 0;

		/* fixme, need to set these operations lists to correct values */
		a->operations        = 	GP_OPERATION_NONE;
		a->file_operations   = 	GP_FILE_OPERATION_PREVIEW;

		/* fixme, believe supports GP_FOLDER_PUT_FILE */
		a->folder_operations = 	GP_FOLDER_OPERATION_NONE;

		gp_abilities_list_append (list, a);

		ptr = coolshot_cameras[x++];
	}

	return (GP_OK);
}

static int camera_start (Camera *camera)
{
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* camera_start");

	/* coolshot_sb sets to default speed if speed == 0 */
	CHECK (coolshot_sb (camera, camera->port_info->speed));
	return( GP_OK );
}

static int camera_stop (Camera *camera)
{
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* camera_stop");

	CHECK (coolshot_sb( camera, DEFAULT_SPEED));
	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			      CameraList *list, void *data)
{
	Camera *camera = data;
	int count;

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* file_list_func");
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "*** folder: %s", folder);

	CHECK (camera_start (camera));
	CHECK (count = coolshot_file_count (camera));
	CHECK (gp_list_populate (list, "pic_%04i.jpg", count));

	return (camera_stop (camera));
}

static int get_info_func (CameraFilesystem *fs, const char *folder,
		const char *filename, CameraFileInfo *info, void *data)
{
	Camera *camera = data;
	int n;

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* get_info_func");
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "*** filename: %s",filename);

	CHECK (camera_start (camera));

	/* Get the file number from the CameraFileSystem */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename));

	/* fixme, get file size also */
	info->file.fields = GP_FILE_INFO_TYPE;
	strcpy (info->file.type, GP_MIME_JPEG );

	info->preview.fields = GP_FILE_INFO_TYPE;
	strcpy (info->preview.type, GP_MIME_JPEG);

	return (camera_stop (camera));
}

static int camera_exit (Camera *camera)
{
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* camera_exit");

	return (GP_OK);
}

static int camera_file_get (Camera *camera, const char *folder,
			    const char *filename, CameraFileType type,
			    CameraFile *file)
{
	char data[128000];
	char ppm_filename[128];
	int size, n;

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* camera_file_get");
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "*** filename: %s",filename);
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "*** type: %d", type);

	CHECK (camera_start (camera));

	/*
	 * Get the file number from the CameraFileSystem (and increment 
	 * because we need numbers starting with 1)
	 */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename));
	n++;

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		CHECK (coolshot_request_thumbnail (camera, data, &size, n));
		CHECK (coolshot_build_thumbnail (data, &size));
		CHECK (gp_file_set_mime_type (file, GP_MIME_PPM));

		strcpy( ppm_filename, filename );
		ppm_filename[strlen(ppm_filename)-3] = 'p';
		ppm_filename[strlen(ppm_filename)-2] = 'p';
		ppm_filename[strlen(ppm_filename)-1] = 'm';
		CHECK (gp_file_set_name (file, ppm_filename));
		break;

	case GP_FILE_TYPE_NORMAL:
		CHECK (coolshot_request_image (camera, data, &size, n));
		CHECK (gp_file_set_mime_type (file, GP_MIME_JPEG));
		CHECK (gp_file_set_name (file, filename));
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK (gp_file_append (file, data, size));

	return (camera_stop (camera));
}

static int camera_summary (Camera *camera, CameraText *summary)
{
	int count;
	char tmp[1024];

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* camera_summary");

	CHECK (camera_start (camera));

	/* possibly get # pics, mem free, etc. */
	count = coolshot_file_count (camera);

	sprintf (tmp, "Frames Taken     : %4d\n", count);
	strcat (summary->text, tmp );

	return (camera_stop (camera));
}

static int camera_manual (Camera *camera, CameraText *manual)
{
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* camera_manual");

	strcpy (manual->text, _("Some notes:\n"
		"    No notes here yet.\n"));
	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about)
{
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* camera_about");

	strcpy (about->text,
		_("coolshot library v" COOLSHOT_VERSION
		" " COOLSHOT_LAST_MOD "\n"
		"Chris Pinkham <cpinkham@infi.net>\n"
		"Support for Panasonic Coolshot digital cameras\n"
		"based on reverse engineering serial protocol.\n"
		"\n"));

	return (GP_OK);
}

int camera_init (Camera *camera)
{
	int count;
	gp_port_settings settings;

	/* First, set up all the function pointers */
	camera->functions->exit 	= camera_exit;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	strcpy (settings.serial.port, camera->port_info->path);
	settings.serial.speed 	 = DEFAULT_SPEED;
	settings.serial.bits 	 = 8;
	settings.serial.parity 	 = 0;
	settings.serial.stopbits = 1;

	CHECK (gp_port_settings_set (camera->port, settings));

	CHECK (gp_port_timeout_set (camera->port, TIMEOUT));
	CHECK (gp_port_open (camera->port));

	/* check to see if camera is really there */
	CHECK (coolshot_enq (camera));

	coolshot_sm (camera);

	/* get number of images */
	CHECK (count = coolshot_file_count (camera));

	CHECK (camera_start (camera));
	CHECK (gp_filesystem_set_list_funcs (camera->fs,
		file_list_func, NULL, camera));
	CHECK (gp_filesystem_set_info_funcs (camera->fs,
		get_info_func, NULL, camera));

	/* coolshot_sb sets to default speed if speed == 0 */
	CHECK (coolshot_sb (camera, camera->port_info->speed));

	return (camera_stop (camera));
}

