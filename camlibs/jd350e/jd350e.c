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
	CameraAbilities *a;

	gp_abilities_new(&a);

	strcpy(a->model, "Jenoptik JD350e");
	a->port     = GP_PORT_SERIAL;
	a->speed[0] = 115200;
	a->speed[1] = 0;
	a->operations        = GP_OPERATION_NONE;
	a->file_operations   = GP_FILE_OPERATION_PREVIEW;
	a->folder_operations = GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

int camera_exit (Camera *camera) {

	struct jd350e_s *device = camera->camlib_data;

	/* close serial port */
	gp_port_close(device->gpiod);

	/* free camera filesystem */
	gp_filesystem_free(device->fs);

	return (GP_OK);
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list) 
{
	/* jd350e_s has no folder support */

	return (GP_OK);
}

int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list) 
{
	struct jd350e_s *device = camera->camlib_data;
	int i, count, result;
	const char *name;

	result =  jd350e_file_count(device, &count);
	if (result != GP_OK)
		return result;

	gp_filesystem_populate(device->fs, "/", "image%02i.pnm", count);

	for(i = 0; i < gp_filesystem_count(device->fs, folder); ++i) {
		gp_filesystem_name(device->fs, folder, i, &name);
		gp_list_append(list, name, NULL);
	}

	return (GP_OK);
}

int camera_file_get (Camera *camera, const char *folder, const char *filename,
                     CameraFileType type, CameraFile *file ) 
{
	struct jd350e_s *device = camera->camlib_data;
	int image_no, count, result;
	char *data;
	long int size;

	result =  jd350e_file_count(device, &count);
	if (result != GP_OK)
		return result;

	gp_filesystem_populate(device->fs, "/", "image%02i.pnm", count);

	image_no = gp_filesystem_number(device->fs, folder, filename);

	if(image_no < 0)
		return image_no;

	switch (type) {
	case GP_FILE_TYPE_RAW:
		result =  jd350e_get_image_raw (device, image_no, &data,
					    (int*) &size);
		break;
	case GP_FILE_TYPE_NORMAL:
		result =  jd350e_get_image_full (device, image_no, &data,
					    (int*) &size);
		break;
	case GP_FILE_TYPE_PREVIEW:
		result =  jd350e_get_image_preview (device, image_no, &data,
						    (int*) &size);
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

int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) 
{
	/* XXX implement */

	return (GP_ERROR_NOT_SUPPORTED);
}

int camera_summary (Camera *camera, CameraText *summary) 
{
	strcpy(summary->text, _("No summary available."));

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) 
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

int camera_about (Camera *camera, CameraText *about) 
{
	strcpy (about->text, 
		_("JD350e\n"
		"Michael Trawny <trawny99@yahoo.com>\n"
		"Driver for Jenoptik JD350e cameras (uncompressed mode only!)\n"
		"Protocol reverse engineered using Portmon"));

	return (GP_OK);
}

char* camera_result_as_string (Camera *camera, int result) 
{
	if (result >= 0) return ("This is not an error...");
	if (-result < 100) return gp_result_as_string (result);
	return ("This is a template specific error.");
}

int camera_init (Camera *camera) 
{
        gp_port_settings gpiod_settings;
        int ret;
        struct jd350e_s *device;

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->folder_list_folders  = camera_folder_list_folders;
        camera->functions->folder_list_files    = camera_folder_list_files;
        camera->functions->file_get             = camera_file_get;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;
        camera->functions->result_as_string     = camera_result_as_string;

        if((device = malloc(sizeof(struct jd350e_s))) == NULL) {
                return GP_ERROR_NO_MEMORY;
        }

        camera->camlib_data = device;

        /* open and configure serial port */
        if ((ret = gp_port_new(&(device->gpiod), GP_PORT_SERIAL)) < 0)
            return (ret);

        gp_port_timeout_set(device->gpiod, 1000);

        strcpy(gpiod_settings.serial.port, camera->port_info->path);
        gpiod_settings.serial.speed = camera->port_info->speed;
        gpiod_settings.serial.bits = 8;
        gpiod_settings.serial.parity = 0;
        gpiod_settings.serial.stopbits = 1;

        gp_port_settings_set(device->gpiod, gpiod_settings);
        gp_port_open(device->gpiod);

        /* create camera filesystem */
        gp_filesystem_new(&device->fs);

        /* test camera */
        return  jd350e_ping(device);
}

