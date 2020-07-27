/****************************************************************/
/* library.c - Gphoto2 library for the Creative PC-CAM 300      */
/*                                                              */
/*                                                              */
/* Authors: Till Adam <till@adam-lilienthal.de>                 */
/*          Miah Gregory <mace@darksilence.net>                 */
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

#include <config.h>

#include "pccam300.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#define GP_MODULE "pccam300"

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

#define GP_MODULE "pccam300"

static const struct models
{
	char *name;
	unsigned short idVendor;
	unsigned short idProduct;
}
models[] =
{
	{ "Creative PC-CAM 300",    0x041e, 0x400a},
	{ "Intel Pocket PC Camera", 0x8086, 0x0630},
	{ NULL, 0, 0}
};

static int delete_all_func (CameraFilesystem *fs, const char *folder,
			    void *data, GPContext *context);


int
camera_id (CameraText *id)
{
	strcpy (id->text, "Creative PC-CAM 300");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].name; i++) {
		memset (&a, 0, sizeof (CameraAbilities));
		strcpy (a.model, models[i].name);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port = GP_PORT_USB;
		a.usb_vendor = models[i].idVendor;
		a.usb_product = models[i].idProduct;
		a.operations = GP_OPERATION_NONE;
		a.file_operations = GP_FILE_OPERATION_DELETE;
		a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;
		gp_abilities_list_append (list, a);
	}
	return GP_OK;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder,
		CameraList *list, void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned int i, id, size, type;
	int filecount;
	CameraFile *file;
	CameraFileInfo info;
	unsigned char *buffer = NULL;
	int ret, n_img=0, n_avi=0, n_wav=0;
	char fn[100];

	CHECK (pccam300_get_filecount (camera->port, &filecount));

	id = gp_context_progress_start (context, filecount,
			_("Getting file list..."));

	for (i = 0; i < filecount; i++) {
		/* Get information */
		gp_file_new (&file);

		ret = pccam300_get_file (camera->port, context, i,
		                          &buffer, &size, &type);
		if (ret < GP_OK) {
			gp_file_free (file);
			return ret;
		}

		info.audio.fields = GP_FILE_INFO_NONE;
		info.preview.fields = GP_FILE_INFO_NONE;

		info.file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
		info.file.size = size;

		switch (type) {
			case PCCAM300_MIME_JPEG:
				strcpy (info.file.type, GP_MIME_JPEG);
				sprintf (fn, "Image%03i.jpeg", n_img++);
				break;
			case PCCAM300_MIME_AVI:
				strcpy (info.file.type, GP_MIME_AVI);
				sprintf (fn, "Movie%03i.UNUSABLE", n_avi++);
				break;
			case PCCAM300_MIME_WAV:
				strcpy (info.file.type, GP_MIME_WAV);
				sprintf (fn, "Audio%03i.UNUSABLE", n_wav++);
				break;
			default:
				break;
		}

		if (file)
			gp_file_set_data_and_size (file, (char *)buffer, size);
		else
			free (buffer);

		/*
		 * Append directly to the filesystem instead of to the list,
		 * because we have additional information.
		 * */
		gp_filesystem_append (camera->fs, folder, fn, context);
		gp_filesystem_set_info_noop (camera->fs, folder, fn, info, context);
		gp_filesystem_set_file_noop (camera->fs, folder, fn, GP_FILE_TYPE_NORMAL,
					file, context);
		gp_file_unref (file);

		gp_context_idle (context);
		gp_context_progress_update (context, id, i + 1);
		if (gp_context_cancel(context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
	}
	gp_context_progress_stop (context, id);
	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder,
               const char *filename, CameraFileType type,
               CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
	unsigned char *data = NULL;
	unsigned int size, mimetype;
	int index;

	size = 0;
	index = gp_filesystem_number (fs, folder, filename, context);
	if (index < 0)
		return index;
	switch (type) {
		case GP_FILE_TYPE_NORMAL:
			CHECK (pccam300_get_file (camera->port, context,
			                          index, &data, &size,
			                          &mimetype));
			break;
		default:
			return GP_ERROR_NOT_SUPPORTED;
	}
	return gp_file_set_data_and_size (file, (char *)data, size);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	int totalmem;
	int freemem;
	int filecount;
	char summary_text[256];

	CHECK (pccam300_get_mem_info (camera->port, context, &totalmem,
				      &freemem));
	CHECK (pccam300_get_filecount (camera->port, &filecount));
	snprintf (summary_text, sizeof (summary_text),
		  _(" Total memory is %8d bytes.\n"
		   " Free memory is  %8d bytes.\n"
		   " Filecount: %d"),
		  totalmem, freemem, filecount);
	strcat (summary->text, summary_text);
	return GP_OK;
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text,
		_("Creative PC-CAM 300\n Authors: Till Adam\n"
		 "<till@adam-lilienthal.de>\n"
		 "and: Miah Gregory\n <mace@darksilence.net>"));
	return GP_OK;
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{

	return GP_OK;
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	int index;
	Camera *camera = data;

	index = gp_filesystem_number (fs, folder, filename, context);
	gp_log (GP_LOG_DEBUG, "pccam", "deleting '%s' in '%s'.. index:%d",
		filename, folder, index);
	CHECK (pccam300_delete_file (camera->port, context, index));
	return GP_OK;
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
                 GPContext *context)
{
	Camera *camera = data;

	CHECK (pccam300_delete_all (camera->port, context));
	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func,
};

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	int ret = 0;

	camera->functions->summary = camera_summary;
	camera->functions->about = camera_about;
	gp_log (GP_LOG_DEBUG, "pccam 300", "Initializing the camera\n");
	switch (camera->port->type) {
		case GP_PORT_USB:
			ret = gp_port_get_settings (camera->port, &settings);
			if (ret < 0)
				return ret;
			settings.usb.inep = 0x82;
			settings.usb.outep = 0x03;
			settings.usb.config = 1;
			settings.usb.interface = 0;
			settings.usb.altsetting = 0;
			ret = gp_port_set_settings (camera->port, settings);
			if (ret < 0)
				return ret;
			break;
		case GP_PORT_SERIAL:
			return GP_ERROR_IO_SUPPORTED_SERIAL;
		default:
			return GP_ERROR_NOT_SUPPORTED;
	}
	CHECK (pccam300_init (camera->port, context));
	return gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
}
