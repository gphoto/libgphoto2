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

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include <gphoto2.h>
#include <sys/types.h>

#include "sony.h"
#include "nls.h"
#include "gphoto2-camera.h"

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
	CameraAbilities a;

	for (i = 0; i < sizeof(models) / sizeof(models[i]); i++) {
		memset(&a, 0, sizeof(a));
		strcpy(a.model, models[i]);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port = GP_PORT_SERIAL;
		a.speed[0] = 0;
		a.operations = GP_OPERATION_NONE;
		a.file_operations = GP_FILE_OPERATION_PREVIEW;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		gp_abilities_list_append(list, a);
	}

	return (GP_OK);
}

/**
 * De-initialises camera
 */
static int camera_exit(Camera * camera)
{
	int rc;

	gp_debug_printf(GP_DEBUG_LOW, SONY_CAMERA_ID,
			"camera_exit()");

	if (camera->pl) {
		rc = sony_exit (camera);
		if (rc < 0)
			return (rc);
		free (camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}



static int camera_about(Camera * camera, CameraText * about)
{
	strcpy(about->text,
	       _("Sony DSC-F55/505 gPhoto library\n"
		 "Supports Sony MSAC-SR1 and Memory Stick used by DCR-PC100\n"
		 "Originally written by Mark Davies <mdavies@dial.pipex.com>\n"
		 "gPhoto2 port by Raymond Penners <raymond@dotsphinx.com>"));

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int count;

	gp_debug_printf(GP_DEBUG_LOW, SONY_CAMERA_ID,
			"camera_folder_list_files()");

	count = sony_image_count(camera);
	if (count < 0)
		return (count);

	/* Populate the filesystem */
	gp_list_populate(list, SONY_FILE_NAME_FMT, count);

	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile * file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int num;
	int rc = GP_ERROR;

	gp_debug_printf(GP_DEBUG_LOW, SONY_CAMERA_ID,
			"camera_file_get(\"%s/%s\")", folder, filename);

	num = gp_filesystem_number(camera->fs, folder, filename, context);
	if (num < 0)
		return (num);

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

	return rc;
}



static int
get_info_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, CameraFileInfo *info, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int num;

	num = gp_filesystem_number(camera->fs, folder, filename, context);
	if (num < 0)
		return (num);

	num++;
	return (sony_image_info(camera, num, info));
}

/**
 * Initialises camera
 */
int camera_init(Camera * camera)
{
	CameraAbilities a;
	int is_msac, rc;

	camera->functions->exit = camera_exit;
	camera->functions->about = camera_about;

	gp_camera_get_abilities (camera, &a);
	is_msac = !strcmp (a.model, SONY_MODEL_MSAC_SR1);

	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);

	rc = sony_init (camera, is_msac);
	if (rc < 0) {
		free (camera->pl);
		camera->pl = NULL;
		return (rc);
	}

	return (GP_OK);
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
