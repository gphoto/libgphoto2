/* library.c
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2-library.h>
#include <gphoto2-port-log.h>

#include "ricoh.h"

#define GP_MODULE "ricoh"

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
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CR(result) {int r=(result); if (r<0) return r;}

static struct {
	RicohModel id;
	const char *model;
} models[] = {
	{RICOH_MODEL_300,  "Ricoh RDC-300"},
	{RICOH_MODEL_300Z, "Ricoh RDC-300Z"},
	{RICOH_MODEL_4300, "Ricoh RDC-4300"},
	{0, NULL}
};

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	memset (&a, 0, sizeof (CameraAbilities));
	for (i = 0; models[i].model; i++) {
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port = GP_PORT_SERIAL;
		a.operations = GP_OPERATION_NONE;
		a.file_operations = GP_FILE_OPERATION_NONE;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		CR (gp_abilities_list_append (list, a));
	}

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	if (camera->pl) {
		ricoh_bye (camera, context);
		free (camera->pl);
		camera->pl = NULL;
	}

	return GP_OK;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned int n;

	CR (ricoh_get_num (camera, context, &n));
	CR (gp_list_populate (list, "rdc%04i.jpg", n));

	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
	int n;
	unsigned int size;
	unsigned char *data;

	CR (n = gp_filesystem_number (fs, folder, filename, context));
	n++;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		CR (ricoh_get_pic (camera, context, n, &data, &size));
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	gp_file_set_data_and_size (file, data, size);
	gp_file_set_mime_type (file, GP_MIME_JPEG);

	return (GP_OK);
}

static int
del_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       void *user_data, GPContext *context)
{
	Camera *camera = user_data;
	int n;

	CR (n = gp_filesystem_number (fs, folder, filename, context));
	n++;

	CR (ricoh_del_pic (camera, context, n));

	return (GP_OK);
}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "Ricoh");

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *about, GPContext *context)
{
	unsigned int i;
	int avail_mem, total_mem;
	time_t camtime;
	char cam_id[128];
	RicohModel model;
	const char *model_string = N_("Unknown");

	CR (ricoh_ping (camera, context, &model));
	for (i = 0; models[i].model; i++)
		if (models[i].id == model)
			break;
	if (models[i].model)
		model_string = models[i].model;
	CR (ricoh_get_cam_id   (camera, context, cam_id));
	CR (ricoh_get_cam_amem (camera, context, &avail_mem));
	CR (ricoh_get_cam_mem  (camera, context, &total_mem));
	CR (ricoh_get_cam_date (camera, context, &camtime));

	sprintf(about->text, _("Camera model: %s\n"
			       "Camera ID: %s\n"
			       "Memory: %d byte(s) of %d available\n"
			       "Camera time: %s\n"),
		_(model_string), cam_id, avail_mem, total_mem,
		ctime (&camtime));

	return (GP_OK);
}

static struct {
	unsigned int speed;
	RicohSpeed rspeed;
} speeds[] = {
	{  2400, RICOH_SPEED_2400},
	{115200, RICOH_SPEED_115200},
	{  4800, RICOH_SPEED_4800},
	{ 19200, RICOH_SPEED_19200},
	{ 38400, RICOH_SPEED_38400},
	{ 57600, RICOH_SPEED_57600},
	{     0, 0}
};

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	unsigned int speed, i;
	int result;
	RicohModel model;

	/* Try to contact the camera */
	CR (gp_port_set_timeout (camera->port, 5000));
	CR (gp_port_get_settings (camera->port, &settings));
	speed = (settings.serial.speed ? settings.serial.speed : 115200);
	for (i = 0; speeds[i].speed; i++) {
		GP_DEBUG ("Trying speed %i...", speeds[i].speed);
		settings.serial.speed = speeds[i].speed;
		CR (gp_port_set_settings (camera->port, settings));
		result = ricoh_ping (camera, NULL, NULL);
		if (result == GP_OK)
			break;
	}

	/* Contact made? If not, report error. */
	if (!speeds[i].speed) {
		gp_context_error (context, _("Could not contact camera."));
		return (GP_ERROR);
	}

	/* Contact made. Do we need to change the speed? */
	if (settings.serial.speed != speed) {
		for (i = 0; speeds[i].speed; i++)
			if (speeds[i].speed == speed)
				break;
		if (!speeds[i].speed) {
			gp_context_error (context, _("Speed %i is not "
				"supported!"), speed);
			return (GP_ERROR);
		}
		CR (ricoh_set_speed (camera, context, speeds[i].rspeed));
		settings.serial.speed = speed;
		CR (gp_port_set_settings (camera->port, settings));
		CR (ricoh_ping (camera, context, &model));
	}

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));

	/* save the mode the camera is in */
	CR (ricoh_get_mode (camera, context, &(camera->pl->mode)));

	/* setup the function calls */
	camera->functions->exit = camera_exit;
	camera->functions->summary = camera_summary;
	CR (gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL,
					  camera));
	CR (gp_filesystem_set_file_funcs (camera->fs, get_file_func,
					  del_file_func, camera));

	return (GP_OK);
}
