/*
 * Jenoptik JD11 Driver
 * Copyright (C) 1999-2001 Marcus Meissner <marcus@jet.franken.de>
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

#include "serial.h"

int camera_id (CameraText *id) 
{
	strcpy(id->text, "JD11");
	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	strcpy(a.model, "Jenoptik JD11");
	a.status		= GP_DRIVER_STATUS_TESTING;
	a.port			= GP_PORT_SERIAL;
	a.speed[0]		= 115200;
	a.speed[1]		= 0;
	a.operations		= GP_OPERATION_NONE ;
	a.file_operations	= GP_FILE_OPERATION_PREVIEW;
	a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data) 
{
	Camera *camera = data;
	int count, result;

	result =  jd11_file_count(camera->port, &count);
	if (result != GP_OK)
		return result;

	gp_list_populate(list, "image%02i.pnm", count);

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

	image_no = gp_filesystem_number(fs, folder, filename);

	if(image_no < 0)
		return image_no;

	switch (type) {
	    /*
	case GP_FILE_TYPE_RAW:
		result =  jd11_get_image_raw (camera->port, image_no, &data,
					    (int*) &size);
		break;
		*/
	case GP_FILE_TYPE_NORMAL:
		result =  jd11_get_image_full (camera->port, image_no, &data,
					    (int*) &size);
		break;
	case GP_FILE_TYPE_PREVIEW:
		result =  jd11_get_image_preview (camera->port, image_no,
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

static int
delete_all_func (CameraFilesystem *fs, const char* folder, void *data)
{
    Camera *camera = data;
    if (strcmp (folder, "/"))
	return (GP_ERROR_DIRECTORY_NOT_FOUND);
    return jd11_erase_all(camera->port);
}

static int camera_summary (Camera *camera, CameraText *summary) 
{
	strcpy(summary->text, _("No summary available."));
	return (GP_OK);
}

static int camera_manual (Camera *camera, CameraText *manual) 
{
	strcpy(manual->text, 
	_(
	"The JD11 camera works rather well with this driver.\n"
	"It gets a bit confused if some data is left over on the serial line,\n"
	"and will report I/O errors on startup. Just switch the camera off and\n"
	"on again and it will no longer do that.\n"
	"An RS232 interface @ 115 kbit is required for image transfer.\n"
	"The driver allows you to get\n\n"
	"   - thumbnails (64x48 PGM format)\n"
	"   - full images (640x480 PPM format)\n"
	"No color correction and no interpolation is applied as of this time.\n"
	"\n\n")); 

	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about) 
{
	strcpy (about->text, 
		_("JD11\n"
		"Marcus Meissner <marcus@jet.franken.de>\n"
		"Driver for the Jenoptik JD11 camera.\n"
		"Protocol reverse engineered using WINE and IDA."));

	return (GP_OK);
}

int camera_init (Camera *camera) 
{
        gp_port_settings settings;
	int ret;

        /* First, set up all the function pointers */
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

        /* Configure port */
        gp_port_set_timeout(camera->port, 1000);
	gp_port_get_settings(camera->port, &settings);
        settings.serial.speed	= 115200;
        settings.serial.bits	= 8;
        settings.serial.parity	= 0;
        settings.serial.stopbits= 1;
        gp_port_set_settings(camera->port, settings);

	/* Set up the filesystem */
	gp_filesystem_set_list_funcs(camera->fs, file_list_func, NULL,camera);
	gp_filesystem_set_file_funcs(camera->fs,get_file_func,NULL,camera);
	gp_filesystem_set_folder_funcs(camera->fs,NULL,delete_all_func, NULL,NULL,camera);
        /* test camera */
        ret = jd11_ping(camera->port);
	return (ret);
}

