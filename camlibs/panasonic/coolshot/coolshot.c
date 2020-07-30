/****************************************************************/
/* coolshot.c - Gphoto2 library for accessing the Panasonic     */
/*              Coolshot KXL-600A & KXL-601A digital cameras.   */
/*                                                              */
/* Copyright 2001 Chris Pinkham                                 */
/*                                                              */
/* Author: Chris Pinkham <cpinkham@infi.net>                    */
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gphoto2/gphoto2.h>

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

#include "library.h"

#define GP_MODULE "coolshot"

#define TIMEOUT	      2000
#define DEFAULT_SPEED 9600

#define COOLSHOT_VERSION "0.4"
#define COOLSHOT_LAST_MOD "09/06/2001 02:28 EST"

/* define what cameras we support */
static char *coolshot_cameras[] = {
	"Panasonic:Coolshot KXL-600A",
	"Panasonic:Coolshot KXL-601A",
	""
};

struct _CameraPrivateLibrary {
	int speed;
};

int camera_id (CameraText *id)
{
	/* GP_DEBUG ("* camera_id"); */

	strcpy (id->text, "coolshot");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list)
{
	int x = 0;
	char *ptr;
	CameraAbilities a;

	/* GP_DEBUG ("* camera_abilities"); */

	ptr = coolshot_cameras[x++];
	while (*ptr) {
		memset(&a, 0, sizeof(a));
		strcpy (a.model, ptr );
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port     = GP_PORT_SERIAL;
		a.speed[0] = 9600;
		a.speed[1] = 19200;
		a.speed[2] = 38400;
		a.speed[3] = 57600;
		a.speed[4] = 115200;
		a.speed[5] = 0;

		/* fixme, need to set these operations lists to correct values */
		a.operations        = 	GP_OPERATION_NONE;
		a.file_operations   = 	GP_FILE_OPERATION_PREVIEW;

		/* fixme, believe supports GP_FOLDER_PUT_FILE */
		a.folder_operations = 	GP_FOLDER_OPERATION_NONE;

		gp_abilities_list_append (list, a);

		ptr = coolshot_cameras[x++];
	}

	return (GP_OK);
}

static int camera_start (Camera *camera)
{
	GP_DEBUG ("* camera_start");

	/* coolshot_sb sets to default speed if speed == 0 */
	CHECK (coolshot_sb (camera, camera->pl->speed));
	return( GP_OK );
}

static int camera_stop (Camera *camera)
{
	GP_DEBUG ("* camera_stop");

	CHECK (coolshot_sb( camera, DEFAULT_SPEED));
	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context)
{
	Camera *camera = data;
	int count;

	GP_DEBUG ("* file_list_func");
	GP_DEBUG ("*** folder: %s", folder);

	CHECK (camera_start (camera));
	CHECK (count = coolshot_file_count (camera));
	CHECK (gp_list_populate (list, "pic_%04i.jpg", count));

	return (camera_stop (camera));
}

static int get_info_func (CameraFilesystem *fs, const char *folder,
		const char *filename, CameraFileInfo *info, void *data,
		GPContext *context)
{
	Camera *camera = data;
	int n;

	GP_DEBUG ("* get_info_func");
	GP_DEBUG ("*** folder: %s", folder);
	GP_DEBUG ("*** filename: %s",filename);

	CHECK (camera_start (camera));

	/* Get the file number from the CameraFileSystem */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename,
					 context));

	/* fixme, get file size also */
	info->file.fields = GP_FILE_INFO_TYPE;
	strcpy (info->file.type, GP_MIME_JPEG );

	info->preview.fields = GP_FILE_INFO_TYPE;
	strcpy (info->preview.type, GP_MIME_JPEG);

	return (camera_stop (camera));
}

static int camera_exit (Camera *camera, GPContext *context)
{
	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
	char data[128000];
	int size, n;

	GP_DEBUG ("* camera_file_get");
	GP_DEBUG ("*** folder: %s", folder);
	GP_DEBUG ("*** filename: %s",filename);
	GP_DEBUG ("*** type: %d", type);

	CHECK (camera_start (camera));

	if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
		camera_stop (camera);
		return (GP_ERROR_CANCEL);
	}

	/*
	 * Get the file number from the CameraFileSystem (and increment
	 * because we need numbers starting with 1)
	 */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename,
					 context));
	n++;

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		CHECK (coolshot_request_thumbnail (camera, file, data, &size, n, context));
		CHECK (coolshot_build_thumbnail (data, &size));
		CHECK (gp_file_set_mime_type (file, GP_MIME_PPM));
		break;

	case GP_FILE_TYPE_NORMAL:
		CHECK (coolshot_request_image (camera, file, data, &size, n, context));
		CHECK (gp_file_set_mime_type (file, GP_MIME_JPEG));
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK (gp_file_append (file, data, size));

	return (camera_stop (camera));
}

static int camera_summary (Camera *camera, CameraText *summary,
			   GPContext *context)
{
	int count;
	char tmp[1024];

	GP_DEBUG ("* camera_summary");

	CHECK (camera_start (camera));

	/* possibly get # pics, mem free, etc. */
	count = coolshot_file_count (camera);

	sprintf (tmp, "Frames Taken     : %4d\n", count);
	strcat (summary->text, tmp );

	return (camera_stop (camera));
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	GP_DEBUG ("* camera_about");

	strcpy (about->text,
		_("coolshot library v" COOLSHOT_VERSION
		" " COOLSHOT_LAST_MOD "\n"
		"Chris Pinkham <cpinkham@infi.net>\n"
		"Support for Panasonic Coolshot digital cameras\n"
		"based on reverse engineering serial protocol.\n"
		"\n"));

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func,
};

int camera_init (Camera *camera, GPContext *context)
{
	int count;
	GPPortSettings settings;

	/* First, set up all the function pointers */
	camera->functions->exit 	= camera_exit;
	camera->functions->summary	= camera_summary;
	camera->functions->about 	= camera_about;

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);

	/* Set up the port and remember the requested speed */
	CHECK (gp_port_get_settings (camera->port, &settings));
	camera->pl->speed = settings.serial.speed;
	settings.serial.speed 	 = DEFAULT_SPEED;
	settings.serial.bits 	 = 8;
	settings.serial.parity 	 = 0;
	settings.serial.stopbits = 1;
	CHECK (gp_port_set_settings (camera->port, settings));
	CHECK (gp_port_set_timeout (camera->port, TIMEOUT));

	/* check to see if camera is really there */
	CHECK (coolshot_enq (camera));

	coolshot_sm (camera);

	/* get number of images */
	CHECK (count = coolshot_file_count (camera));

	CHECK (camera_start (camera));
	CHECK (gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera));

	/* coolshot_sb sets to default speed if speed == 0 */
	CHECK (coolshot_sb (camera, camera->pl->speed));

	return (camera_stop (camera));
}
