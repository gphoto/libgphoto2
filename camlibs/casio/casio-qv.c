/* casio-qv.c
 *
 * Copyright (C) 2001 Lutz Müller
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * 2001/08/31 Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 * I cannot test this stuff - I simply don't have this camera. If you have
 * one, please contact me so that we can get this stuff up and running.
 */

#include <gphoto2-library.h>
#include <gphoto2-core.h>
#include <stdlib.h>
#include <stdio.h>

#include "casio-qv-commands.h"

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

int
camera_id (CameraText *id) 
{
	strcpy (id->text, "Casio QV");

	return (GP_OK);
}

static struct {
	const char       *model;
	int               public;
	unsigned long int revision;
} models[] = {
	{"Casio QV10",  1, 0x00538b8f},
	{"Casio QV10",  0, 0x00531719},
	{"Casio QV10A", 1, 0x00800003},
	{"Casio QV70",  1, 0x00835321},
	{"Casio QV100", 1, 0x0103ba90},
	{"Casio QV300", 1, 0x01048dc0},
	{"Casio QV700", 1, 0x01a0e081},
	{"Casio QV770", 1, 0x01a10000},
	{NULL}
};

int
camera_abilities (CameraAbilitiesList *list) 
{
	int i;
	CameraAbilities *a;

	for (i = 0; models[i].model; i++) {
		if (!models[i].public)
			continue;

		CHECK_RESULT (gp_abilities_new (&a));

		strcpy (a->model, models[i].model);
		a->status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a->port     = GP_PORT_SERIAL;
		a->speed[0] = 9600;
		a->speed[1] = 19200;
		a->speed[2] = 38400;
		a->speed[3] = 57600;
		a->operations        = GP_OPERATION_NONE;
		a->file_operations   = GP_FILE_OPERATION_NONE;
		a->folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

		CHECK_RESULT (gp_abilities_list_append (list, a));
	}
	
	return (GP_OK);
}

static int
camera_file_get (Camera *camera, const char *folder, const char *filename, 
		 CameraFileType type, CameraFile *file)
{
	int n;
	unsigned char *data = NULL;
	long int size = 0;

	/* Get the number of the picture from the filesystem */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, filename));

	/* Prepare the transaction */
	CHECK_RESULT (QVshowpic (camera, n));
	CHECK_RESULT (QVsetpic (camera));

	switch (type) {
	case GP_FILE_TYPE_RAW:
		break;
	case GP_FILE_TYPE_PREVIEW:
		break;
	case GP_FILE_TYPE_NORMAL:
	default:
		CHECK_RESULT (QVgetpic (camera, data, size));
		break;
	}

	CHECK_RESULT (gp_file_set_data_and_size (file, data, size));
	CHECK_RESULT (gp_file_set_name (file, filename));
	CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_JPEG));

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about) 
{
	strcpy (about->text, "Download program for Casio QV cameras. "
		"Originally written for gphoto-0.4. Adapted for gphoto2 by "
		"Lutz Müller <urc8@rz.uni-karlsruhe.de>.");

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *about)
{
	float battery;
	long int revision;

	CHECK_RESULT (QVbattery  (camera, &battery));
	CHECK_RESULT (QVrevision (camera, &revision));

	sprintf (about->text, "Battery level: %.1f Volts. "
			      "Revision: %i.", battery, (int) revision);

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data)
{
	int num;
	Camera *camera = data;

	/* Fill the list */
	CHECK_RESULT (num = QVnumpic (camera));
	gp_list_populate (list, "CASIO_QV_%04i.jpg", num);

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data)
{
	int n, size_thumb, size_pic;
	Camera *camera = data;

	/* Get the picture number from the CameraFilesystem */
	CHECK_RESULT (n = gp_filesystem_number (fs, folder, file));

	/* Get some information */
	camera = NULL;
	size_thumb = size_pic = 0;

	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	strcpy (info->file.type, GP_MIME_JPEG);
	strcpy (info->preview.type, GP_MIME_JPEG);
	info->file.size = size_pic;
	info->preview.size = size_thumb;

	return (GP_OK);
}

static int
camera_capture (Camera *camera, int type, CameraFilePath *path)
{
	/* Capture the image */
	CHECK_RESULT (QVcapture (camera));

	/* Tell libgphoto2 that the filesystem changed */
	CHECK_RESULT (gp_filesystem_format (camera->fs));

	/* Tell libgphoto2 where to look for the new image */
	strcpy (path->folder, "/");
	sprintf (path->name, "CASIO_QV_%04i.jpg",
		 gp_filesystem_count (camera->fs, "/"));

	return (GP_OK);
}

int
camera_init (Camera *camera) 
{
	int result = GP_OK, i;
	gp_port_settings settings;
	int speeds[] = {9600, 57600, 19200, 38400};

        /* First, set up all the function pointers */
        camera->functions->file_get     = camera_file_get;
	camera->functions->summary	= camera_summary;
	camera->functions->capture      = camera_capture;
        camera->functions->about        = camera_about;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);

	/* Open the port and check if the camera is there */
	CHECK_RESULT (gp_port_settings_get (camera->port, &settings));
	strcpy (settings.serial.port, camera->port_info->path);
	CHECK_RESULT (gp_port_settings_set (camera->port, settings));
	CHECK_RESULT (gp_port_timeout_set (camera->port, 1000));

	/* Check if the camera is really there */
	CHECK_RESULT (gp_port_open (camera->port));
	for (i = 0; i < 4; i++) {
		settings.serial.speed = speeds[i];
		CHECK_RESULT (gp_port_settings_set (camera->port, settings));
		result = QVping (camera);
		if (result == GP_OK)
			break;
	}
	if (i == 4)
		return (result);

	return (GP_OK);
}
