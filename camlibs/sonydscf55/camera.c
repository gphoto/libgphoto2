/* Sony DSC-F55 & MSAC-SR1 - gPhoto2 camera library
 * Copyright (C) 2001 Raymond Penners <raymond@dotsphinx.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <stdlib.h>
#include <gphoto2.h>
#include <sys/types.h>

#include "sony.h"
#include "nls.h"


int camera_id(CameraText * id)
{
	strcpy(id->text, SONY_CAMERA_ID);
	return (GP_OK);
}

int camera_abilities(CameraAbilitiesList * list)
{
	static const char *models[] = {
		"Sony DSC-F55",
		SONY_MODEL_MSAC_SR1
	};
	int i;

	for (i = 0; i < sizeof(models) / sizeof(models[i]); i++) {
		CameraAbilities *a;

		gp_abilities_new(&a);
		strcpy(a->model, models[i]);
		a->port = GP_PORT_SERIAL;
		a->speed[0] = 0;
		a->operations = GP_OPERATION_NONE;
		a->file_operations = GP_FILE_OPERATION_PREVIEW;
		a->folder_operations = GP_FOLDER_OPERATION_NONE;
		gp_abilities_list_append(list, a);
	}

	return (GP_OK);
}

/**
 * De-initialises camera
 */
int camera_exit(Camera * camera)
{
	int rc = GP_OK;
	if (camera) {
		SonyData *data = sony_data(camera);
		if (data) {
			if (data->dev) {
				if (data->initialized) {
					sony_exit(camera);
					data->initialized = FALSE;
				}

				rc = gp_port_free(data->dev);
				if (rc == GP_OK)
					data->dev = NULL;
			}
			if (rc == GP_OK && data->fs) {
				rc = gp_filesystem_free(data->fs);
				if (rc == GP_OK)
					data->fs = NULL;
			}
			if (rc == GP_OK) {
				free(data);
				camera->camlib_data = NULL;
			}
		}
	}
	return rc;
}



int camera_about(Camera * camera, CameraText * about)
{
	strcpy(about->text,
	       _("Sony DSC-F55/505 gPhoto library\n"
		 "Supports Sony MSAC-SR1 and Memory Stick used by DCR-PC100\n"
		 "Originally written by Mark Davies <mdavies@dial.pipex.com>\n"
		 "gPhoto2 port by Raymond Penners <raymond@dotsphinx.com>"));

	return (GP_OK);
}

int
camera_folder_list_files(Camera * camera, const char *folder,
			 CameraList * list)
{
	int rc;
	SonyData *b = (SonyData *) camera->camlib_data;

	gp_debug_printf(GP_DEBUG_LOW, SONY_CAMERA_ID,
			"camera_folder_list_files()");
	rc = sony_image_count(camera);
	if (rc != GP_ERROR) {
		int count, x;

		count = rc;

		/* Populate the filesystem */
		gp_filesystem_populate(b->fs, "/", SONY_FILE_NAME_FMT,
				       count);
		for (x = 0; x < gp_filesystem_count(b->fs, folder); x++)
		{
			const char *name;
			gp_filesystem_name(b->fs, folder, x, &name);
			gp_list_append(list, name, NULL);
		}
		rc = GP_OK;
	}
	return rc;
}

int
camera_file_get(Camera * camera, const char *folder, const char *filename,
		CameraFileType type, CameraFile * file)
{
	int num;
	int rc = GP_ERROR;

	gp_debug_printf(GP_DEBUG_LOW, SONY_CAMERA_ID,
			"camera_file_get(\"%s/%s\")", folder, filename);
	num =
	    gp_filesystem_number(sony_data(camera)->fs, folder, filename);
	if (num >= 0) {
		num++;
		gp_debug_printf(GP_DEBUG_LOW, SONY_CAMERA_ID,
				"file %s has id %d", filename, num);

		switch (type) {
		case GP_FILE_TYPE_NORMAL:
			rc = sony_image_get(camera, num, file);
			break;
		case GP_FILE_TYPE_PREVIEW:
			rc = sony_thumbnail_get(camera, num, file);
			break;
		default:
			rc = GP_ERROR_NOT_SUPPORTED;
		}

		
		if (rc == GP_OK) {
			gp_file_set_name (file, filename);
		}
	}
	return rc;
}



int
camera_file_get_info(Camera * camera, const char *folder,
		     const char *filename, CameraFileInfo * info)
{
	int num, rc = GP_ERROR;

	num =
	    gp_filesystem_number(sony_data(camera)->fs, folder, filename);
	if (num >= 0) {
		num++;
		rc = sony_image_info(camera, num, info);
	}
	return rc;
}

/**
 * Initialises camera
 */
int camera_init(Camera * camera)
{
	gp_port_settings settings;
	int ret;

	gp_debug_printf(GP_DEBUG_LOW, SONY_CAMERA_ID, "Initialising %s",
			camera->model);

	camera->functions->exit = camera_exit;
	camera->functions->about = camera_about;
	camera->functions->folder_list_files = camera_folder_list_files;
	camera->functions->file_get = camera_file_get;
	camera->functions->file_get_info = camera_file_get_info;

	camera->camlib_data = NULL;

	camera->camlib_data = (SonyData *) malloc(sizeof(SonyData));
	if (camera->camlib_data) {
		SonyData *b = sony_data(camera);

		b->sequence_id = 0;
		b->dev = NULL;
		b->fs = NULL;
		b->initialized = FALSE;

		b->msac_sr1 = !strcmp(camera->model, SONY_MODEL_MSAC_SR1);

		if ((ret = gp_port_new(&(b->dev), GP_PORT_SERIAL)) >= 0) {
			gp_port_timeout_set(b->dev, 5000);
			strcpy(settings.serial.port, camera->port_info->path);

			settings.serial.speed = 9600;
			settings.serial.bits = 8;
			settings.serial.parity = 0;
			settings.serial.stopbits = 1;

			gp_port_settings_set(b->dev, settings);
			if (gp_port_open(b->dev) == GP_OK) {
				/* Create the filesystem */
				if (gp_filesystem_new (&b->fs) == GP_OK) {
					if (sony_init(camera) == GP_OK) {
						return GP_OK;
					}
				}
			}

		}
	}
	camera_exit(camera);
	return (GP_ERROR);
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
