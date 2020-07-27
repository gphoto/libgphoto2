/* Sony DSC-F55 & MSAC-SR1 - gPhoto2 camera library
 * Copyright 2001, 2002, 2004 Raymond Penners <raymond@dotsphinx.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <gphoto2/gphoto2.h>
#include <sys/types.h>

#include "sony.h"
#include "nls.h"
#include <gphoto2/gphoto2-camera.h>

#define GP_MODULE "sonydscf55"

struct ModelInfo {
	SonyModel model_id;
	const char *model_str;
};

static const struct ModelInfo models[] = {
	{ SONY_MODEL_MSAC_SR1, "Sony:MSAC-SR1" },
	{ SONY_MODEL_DCR_PC100, "Sony:DCR-PC100" },
	{ SONY_MODEL_TRV_20E,   "Sony:TRV-20E" },
	{ SONY_MODEL_DSC_F55, "Sony:DSC-F55" }
};

int
camera_id(CameraText * id)
{
	strcpy(id->text, SONY_CAMERA_ID);
	return (GP_OK);
}

int
camera_abilities(CameraAbilitiesList * list)
{
	int i;
	CameraAbilities a;

	for (i = 0; i < sizeof(models) / sizeof(models[i]); i++) {
		memset(&a, 0, sizeof(a));
		strcpy(a.model, models[i].model_str);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port = GP_PORT_SERIAL;
		a.speed[0] = 0;
		a.operations = GP_OPERATION_NONE;
		a.file_operations = GP_FILE_OPERATION_PREVIEW | GP_FILE_OPERATION_EXIF;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		gp_abilities_list_append(list, a);
	}

	return (GP_OK);
}

/**
 * De-initialises camera
 */
static int
camera_exit(Camera * camera, GPContext *context)
{
	int rc;

	GP_DEBUG(
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



static int
camera_about(Camera * camera, CameraText * about, GPContext *context)
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
	int mpeg, rc = GP_OK;

	GP_DEBUG("camera_folder_list_files()");

	for (mpeg = 0; mpeg <= 1 && rc == GP_OK; mpeg++) {
		int i, count;
		SonyFileType file_type;

		file_type = mpeg ? SONY_FILE_MPEG : SONY_FILE_IMAGE;

		rc = sony_file_count(camera, file_type, &count);
		if (rc != GP_OK) {
			break;
		}

		for (i = 1; i <= count; i++) {
			char buf[13];
			rc = sony_file_name_get(camera, i, file_type, buf);
			if (rc != GP_OK) {
				break;
			}
			gp_list_append(list, buf, NULL);

			if (gp_context_cancel(context)
			    == GP_CONTEXT_FEEDBACK_CANCEL) {
				rc = GP_ERROR_CANCEL;
			}
		}
	}
	return rc;
}


static int
get_sony_file_id(Camera *camera, const char *folder,
		 const char *filename, GPContext *context,
		 int *sony_id, SonyFileType *sony_type)
{
	int num = gp_filesystem_number(camera->fs, folder, filename, context);
	if (num < 0)
		return (num);

	num++;

	if (sony_is_mpeg_file_name(filename)) {
		const char *name_found;
		int mpeg_num = 0;
		do {
			mpeg_num++;
			gp_filesystem_name(camera->fs, folder, num-mpeg_num, &name_found, context);
		}
		while (sony_is_mpeg_file_name(name_found)
		       && (mpeg_num<=num));
		mpeg_num--;

		*sony_type = SONY_FILE_MPEG;
		*sony_id = mpeg_num;
	}
	else {
		*sony_type = SONY_FILE_IMAGE;
		*sony_id = num;
	}
	return GP_OK;
}


static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile * file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int num;
	SonyFileType file_type;
	int rc = GP_ERROR;

	GP_DEBUG("camera_file_get(\"%s/%s\")", folder, filename);

	rc = get_sony_file_id(camera, folder, filename, context,
			      &num, &file_type);
	if (rc != GP_OK)
		return rc;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		if (file_type == SONY_FILE_MPEG) {
			rc = sony_mpeg_get(camera, num, file, context);
                }
		else {
			rc = sony_image_get(camera, num, file, context);
		}
		break;
	case GP_FILE_TYPE_PREVIEW:
		if (file_type == SONY_FILE_MPEG) {
			rc = GP_OK;
		}
		else {
			rc = sony_thumbnail_get(camera, num, file, context);
		}
		break;
	case GP_FILE_TYPE_EXIF:
		if (file_type == SONY_FILE_MPEG) {
			rc = GP_OK;
		}
		else {
			rc = sony_exif_get(camera, num, file, context);
		}
		break;
	default:
		rc = GP_ERROR_NOT_SUPPORTED;
	}
	return rc;
}



static int
get_info_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, CameraFileInfo *info, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int num, rc;
	SonyFileType file_type;

	rc = get_sony_file_id(camera, folder, filename, context,
			      &num, &file_type);
	if (rc != GP_OK)
		return rc;

	rc = sony_image_info(camera, num, file_type, info, context);
	return rc;
}


static int
model_compare(const char *a, const char *b)
{
	const char *amod;
	const char *bmod;
	int alen, blen;

	alen = strlen(a);
	blen = strlen(b);
	if (alen != blen)
		return 0;
	amod = strchr(a, ':');
	bmod = strchr(b, ':');
	if ((amod == NULL && bmod == NULL)
	    || (amod != NULL && bmod != NULL)) {
		return !strcasecmp(a, b);
	}
	if (amod != NULL) {
		int aidx = amod - a;
		return (!strncasecmp(a, b, aidx))
			&& (!strcasecmp(a+aidx+1, b+aidx+1));
	}
	if (bmod != NULL) {
		int bidx = bmod - b;
		return (!strncasecmp(a, b, bidx))
			&& (!strcasecmp(a+bidx+1, b+bidx+1));
	}
	/* can't really get here */
	return 42;
}

static int
get_camera_model(Camera *camera, SonyModel *model)
{
	CameraAbilities a;
	int rc;

	rc = gp_camera_get_abilities (camera, &a);
	if (rc == GP_OK) {
		int i;
		rc = GP_ERROR;
		for (i = 0; i < sizeof(models) / sizeof(models[i]); i++) {
			if (model_compare(models[i].model_str,
						 a.model)) {
				rc = GP_OK;
				*model = models[i].model_id;
				break;
			}
		}
	}
	return rc;
}


static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func
};

/**
 * Initialises camera
 */
int
camera_init(Camera * camera, GPContext *context)
{
	int rc;
	SonyModel model;

	rc = get_camera_model(camera, &model);
	if (rc != GP_OK)
		return rc;

	camera->functions->exit = camera_exit;
	camera->functions->about = camera_about;
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);

	rc = sony_init (camera, model);
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
