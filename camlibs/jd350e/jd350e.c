/*
 * Jenoptik JD350e Driver
 * Copyright (C) 2001 Michael Trawny <trawny99@yahoo.com>
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

#include "jd350e.h"
#include "library.h"

int camera_id (CameraText *id) 
{
	strcpy(id->text, "JD350e");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	strcpy(a.model, "Jenoptik JD350e");
	a.status = GP_DRIVER_STATUS_PRODUCTION;
	a.port     = GP_PORT_SERIAL;
	a.speed[0] = 115200;
	a.speed[1] = 0;
	a.operations        = GP_OPERATION_NONE;
	a.file_operations   = GP_FILE_OPERATION_PREVIEW;
	a.folder_operations = GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data) 
{
	Camera *camera = data;
	int count, result;

	result =  jd350e_file_count(camera->port, &count);
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
	case GP_FILE_TYPE_RAW:
		result =  jd350e_get_image_raw (camera->port, image_no, &data,
					    (int*) &size);
		break;
	case GP_FILE_TYPE_NORMAL:
		result =  jd350e_get_image_full (camera->port, image_no, &data,
					    (int*) &size);
		break;
	case GP_FILE_TYPE_PREVIEW:
		result =  jd350e_get_image_preview (camera->port, image_no,
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

static int camera_summary (Camera *camera, CameraText *summary) 
{
	strcpy(summary->text, _("No summary available."));

	return (GP_OK);
}

static int camera_manual (Camera *camera, CameraText *manual) 
{
	strcpy(manual->text, 
	_("The JD350e camera uses a proprietary compression method. This driver is\n"
	"not yet able to decode compressed images. So please make sure that your pictures\n"
	"are taken in \"best\" mode if you want to use this driver.\n\n"
	"An RS232 interface @ 115 kbit is required for image transfer.\n"
	"USB is not yet supported.\n\n"
	"The driver allows you to get\n\n"
	"   - interpolated thumbnails (80x60 PPM format)\n"
	"   - interpolated images (640x480 PPM format)\n"
	"   - raw images (640x480 PGM format, Bayer-RGB coded)\n\n"
	"The pictures are interpolated using the pattrec algorithm by Erik Brombaugh \n"
	"and Adam Harrison\n\n"
	"A simple color correction is applied to the interpolated images. It works fine\n"
	"for daylight but not for indoor pictures.\n\n")); 

	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about) 
{
	strcpy (about->text, 
		_("JD350e\n"
		"Michael Trawny <trawny99@yahoo.com>\n"
		"Driver for Jenoptik JD350e cameras (uncompressed mode only!)\n"
		"Protocol reverse engineered using Portmon"));

	return (GP_OK);
}

static const char* camera_result_as_string (Camera *camera, int result) 
{
	if (result >= 0) return ("This is not an error...");
	if (-result < 100) return gp_result_as_string (result);
	return ("This is a template specific error.");
}

int camera_init (Camera *camera) 
{
        gp_port_settings settings;
	int ret;

        /* First, set up all the function pointers */
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;
        camera->functions->result_as_string     = camera_result_as_string;

        /* Configure port */
        gp_port_timeout_set(camera->port, 1000);

	gp_port_settings_get(camera->port, &settings);
        settings.serial.speed = camera->port_info->speed;
        settings.serial.bits = 8;
        settings.serial.parity = 0;
        settings.serial.stopbits = 1;
        gp_port_settings_set(camera->port, settings);

	/* Set up the filesystem */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL,
				      camera);
	gp_filesystem_set_file_funcs  (camera->fs, get_file_func, NULL, camera);

        /* test camera */
        ret = jd350e_ping(camera->port);

	return (ret);
}

