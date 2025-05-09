/* casio-qv.c
 *
 * Copyright © 2001 Lutz Müller
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _DARWIN_C_SOURCE
#include "config.h"

#include <gphoto2/gphoto2-library.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "casio-qv-commands.h"
#include "camtojpeg.h"
#include "ycctoppm.h"

#define THUMBNAIL_WIDTH 52
#define THUMBNAIL_HEIGHT 36
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

#include "libgphoto2/i18n.h"


static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data, GPContext *context);

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
	{"Casio:QV10",  1, 0x00538b8f},
	{"Casio:QV10",  0, 0x00531719},
	{"Casio:QV10A", 1, 0x00800003},
	{"Casio:QV70",  1, 0x00835321},
	{"Casio:QV100", 1, 0x0103ba90},
	{"Casio:QV300", 1, 0x01048dc0},
	{"Casio:QV700", 1, 0x01a0e081},
	{"Casio:QV770", 1, 0x01a10000},
	{NULL, 0, 0}
};

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].model; i++) {

		if (!models[i].public)
			continue;

		memset (&a, 0, sizeof(a));
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port     = GP_PORT_SERIAL;
		a.speed[0] = 9600;
		a.speed[1] = 19200;
		a.speed[2] = 38400;
		a.speed[3] = 57600;
		a.speed[4] = 115200;
		a.operations        = GP_OPERATION_CAPTURE_IMAGE |
		                      GP_OPERATION_CONFIG;
		a.file_operations   = GP_FILE_OPERATION_DELETE |
		                      GP_FILE_OPERATION_PREVIEW;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;

		CHECK_RESULT (gp_abilities_list_append (list, a));
	}

	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
	int n;
	unsigned char attr;
	unsigned char *data = NULL;
	long int size = 0;
	unsigned char *cam = NULL;
	long int camSize = 0;

	/* Get the number of the picture from the filesystem */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, filename, context));

	/* Prepare the transaction */
	CHECK_RESULT (QVpicattr (camera, n, &attr));
	CHECK_RESULT (QVshowpic (camera, n));
	CHECK_RESULT (QVsetpic (camera));

	switch (type) {
	case GP_FILE_TYPE_RAW:
		CHECK_RESULT (QVgetCAMpic (camera, &data, (long unsigned *)&size, attr&2));
		CHECK_RESULT (gp_file_set_mime_type(file, GP_MIME_RAW));
		break;
	case GP_FILE_TYPE_PREVIEW:
		CHECK_RESULT (QVgetYCCpic (camera, &cam, (long unsigned *)&camSize));
		CHECK_RESULT (QVycctoppm(cam, camSize, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, 2, &data, &size));
		free(cam);
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_PPM));
		break;
	case GP_FILE_TYPE_NORMAL:
		CHECK_RESULT (QVgetCAMpic (camera, &cam, (long unsigned *)&camSize, attr&2));
		CHECK_RESULT ((attr&2 ? QVfinecamtojpeg : QVcamtojpeg)(cam, camSize, &data, &size));
		free(cam);
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_JPEG));
		break;
	default:
		gp_context_error(context, _("Image type %d not supported"), type);
		return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT (gp_file_set_data_and_size (file, (char*)data, size));
	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
                  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int nr ;
	CameraFileInfo info;

	nr = gp_filesystem_number(fs, folder, filename, context);
	if (nr < 0)
		return nr;

	CHECK_RESULT (get_info_func(fs, folder, filename, &info, data, context));

	if (info.file.permissions == GP_FILE_PERM_READ) {
		gp_context_error(context, _("Image %s is delete protected."), filename);
		return (GP_ERROR);
	}

	CHECK_RESULT (QVdelete (camera, nr));

	return GP_OK;
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Driver framework written by Lutz Mueller "
		"<lutz@users.sf.net>.\n"
		"This software has QVplay's source code, written by Ken-ichi HAYASHI "
		"<xg2k-hys@asahi-net.or.jp> and Jun-ichiro \"itojun\" Itoh "
		"<itojun@itojun.org>.\n"
		"Integration of QVplay by Michael Haardt <michael@moria.de>."));

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *about, GPContext *context)
{
	float battery;
	long int revision;

	CHECK_RESULT (QVbattery  (camera, &battery));
	CHECK_RESULT (QVrevision (camera, &revision));

	sprintf (about->text, _("Battery level: %.1f Volts. "
			      "Revision: %08x."), battery, (int) revision);

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	int num;
	Camera *camera = data;

	/* Fill the list */
	CHECK_RESULT (num = QVnumpic (camera));
	gp_list_populate (list, "CASIO_QV_%03i.jpg", num);

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	int n, size_thumb, size_pic;
	unsigned char attr;
	Camera *camera = data;

	/* Get the picture number from the CameraFilesystem */
	CHECK_RESULT (n = gp_filesystem_number (fs, folder, file, context));

	/* Get some information */
	size_thumb = size_pic = 0;

	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE | GP_FILE_INFO_PERMISSIONS;
	info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	strcpy (info->file.type, GP_MIME_JPEG);
	strcpy (info->preview.type, GP_MIME_PPM);
	info->file.size = size_pic;
	info->preview.size = size_thumb;

	CHECK_RESULT (QVpicattr (camera, n, &attr));
	if (attr&1)
		info->file.permissions = GP_FILE_PERM_READ;
	else
		info->file.permissions = GP_FILE_PERM_ALL;

	return (GP_OK);
}

static int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *child;
	float battery;
	char status[2];
	char t[1024];

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);

	CHECK_RESULT (QVbattery (camera, &battery));
	gp_widget_new (GP_WIDGET_TEXT, _("Battery"), &child);
	gp_widget_set_name (child, "battery");
	snprintf(t,sizeof(t),"%.1f V",battery);
	gp_widget_set_value (child, t);
	gp_widget_append (*window, child);

	CHECK_RESULT (QVstatus (camera, status));
	gp_widget_new (GP_WIDGET_RADIO, _("Brightness"), &child);
	gp_widget_set_name (child, "brightness");
	gp_widget_add_choice (child, _("Too bright"));
	gp_widget_add_choice (child, _("Too dark"));
	gp_widget_add_choice (child, _("OK"));
	if (status[0]&0x80) strcpy (t, _("Too bright"));
	else if (status[0]&0x40) strcpy (t, _("Too dark"));
	else strcpy (t, _("OK"));
	gp_widget_set_value (child, t);
	gp_widget_append (*window, child);

	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT (QVcapture (camera));

	strcpy (path->folder, "/");
	sprintf (path->name, "CASIO_QV_%03i.jpg", QVnumpic (camera));

	CHECK_RESULT (gp_filesystem_append (camera->fs, "/", path->name, context));

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	CHECK_RESULT (QVsetspeed (camera,9600));

	CHECK_RESULT (QVreset (camera));

	/* power down interface */
	gp_port_set_pin (camera->port, GP_PIN_RTS, GP_LEVEL_LOW);
	gp_port_set_pin (camera->port, GP_PIN_DTR, GP_LEVEL_LOW);
	gp_port_set_pin (camera->port, GP_PIN_CTS, GP_LEVEL_LOW);

	sleep(1);

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func
};

int
camera_init (Camera *camera, GPContext *context)
{
	gp_port_settings settings;
	int selected_speed;

	/* First, set up all the function pointers */
	camera->functions->get_config   = camera_config_get;
	camera->functions->capture      = camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->exit		= camera_exit;
	camera->functions->about        = camera_about;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	CHECK_RESULT (gp_port_get_settings (camera->port, &settings));

	/* Taking pictures takes over 5 seconds, so use 7 seconds to be safe */
	CHECK_RESULT (gp_port_set_timeout (camera->port, 7000));
	selected_speed = settings.serial.speed;
	if (!selected_speed) selected_speed = 115200;

	/* Protocol always starts with 9600 */
	settings.serial.speed = 9600;
	CHECK_RESULT (gp_port_set_settings (camera->port, settings));

	/* Power up interface */
	gp_port_set_pin (camera->port, GP_PIN_RTS, GP_LEVEL_HIGH);
	gp_port_set_pin (camera->port, GP_PIN_DTR, GP_LEVEL_LOW);
	gp_port_set_pin (camera->port, GP_PIN_CTS, GP_LEVEL_LOW);

	/*
	 * There may be one junk character at this point, but QVping
	 * takes care of that.
	 */
	CHECK_RESULT (QVping (camera));
	CHECK_RESULT (QVsetspeed (camera,selected_speed));

	return (GP_OK);
}
