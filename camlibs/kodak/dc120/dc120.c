/* dc120.c
 *
 * Copyright (C) Scott Fritzinger
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
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 199309L

#include "config.h"

/* Must be included before including unistd.h to define _XOPEN_SOURCE
   before unistd.h on FreeBSD. */
#include <gphoto2/gphoto2-port-portability.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#include "libgphoto2/i18n.h"

#include "dc120.h"
#include "library.h"


#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

int camera_id (CameraText *id) {
	strcpy(id->text, "kodak-dc120");
	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities a;

	memset (&a, 0, sizeof(a));
	strcpy(a.model, "Kodak:DC120");
	a.status = GP_DRIVER_STATUS_PRODUCTION;
	a.port     = GP_PORT_SERIAL;
	a.speed[0] = 9600;
	a.speed[1] = 19200;
	a.speed[2] = 38400;
	a.speed[3] = 57600;
	a.speed[4] = 115200;
	a.speed[5] = 0;
	a.operations        = 	GP_OPERATION_CAPTURE_IMAGE;
	a.file_operations   = 	GP_FILE_OPERATION_DELETE |
				GP_FILE_OPERATION_PREVIEW;
	a.folder_operations = 	GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

/** Parses a path for a folder and returns folder number and card */
static int find_folder( Camera *camera, const char *folder,
			int *from_card, int *folder_nr, GPContext *context)
{
	CameraList *albums = NULL;
	const char* album_name;
	size_t folder_len;
	int i;
	char *dc120_folder_card   = _("CompactFlash Card");

	if (folder[0] != '/') {
		return (GP_ERROR);
	}

	folder++;

	if (folder[0] == '\0') {
		/* From memory */
		*from_card = FALSE;
		*folder_nr = 0;
		return (GP_OK);
	} else if (strncmp(folder, dc120_folder_card, strlen(dc120_folder_card))==0) {
		/* From card */
		*from_card = TRUE;
		folder += strlen(dc120_folder_card);
	} else {
		/* Subfolder in memory */
		*from_card = FALSE;
		folder--; /* step back to slash */
	}

	if ((folder[0] == 0) ||
		(folder[0] == '/' && folder[1] == 0)) { /* ok, finished */
		*folder_nr = 0;
		return (GP_OK);
	} else if (folder[0] != '/')
		return (GP_ERROR);

	folder++; /* remove slash */

	/* Have trailing slash */
	folder_len = strlen(folder);
	if (folder[folder_len-1] == '/') {
		folder_len--;
	}

	/* ok, now we have a album. first get all albums */
	if (gp_list_new(&albums) != (GP_OK)) {
		return (GP_ERROR);
	}

	if (dc120_get_albums(camera, *from_card, albums, context) != (GP_OK)) {
		gp_list_free(albums);
		return (GP_ERROR);
	}

	/* no check if such a album exist */
	for (i = 0; i<gp_list_count(albums); i++)
	{
		gp_list_get_name(albums, i, &album_name);
		if (strlen(album_name) == folder_len &&
			strncmp(album_name, folder, folder_len) == 0)
		{
			*folder_nr = i+1;
			gp_list_free(albums); /* ok, we found it. */
			return (GP_OK);
		}
	}

	/* oh, we did not find the folder. bummer. */
	gp_list_free(albums);
	return (GP_ERROR);
}

static int folder_list_func (CameraFilesystem *fs, const char *folder,
			     CameraList *list, void *data, GPContext *context)
{
	int res;
	int from_card;
	int folder_nr;
	Camera *camera = data;
	char *dc120_folder_card   = _("CompactFlash Card");

	res = find_folder(camera, folder, &from_card, &folder_nr, context);
	if (res != (GP_OK)) {
		return res;
	}

	if (!from_card && folder_nr==0) {
		gp_list_append(list, dc120_folder_card, NULL);
		return (dc120_get_albums(camera, from_card, list, context));
	} else if (from_card && folder_nr==0) {
		return (dc120_get_albums(camera, from_card, list, context));
	} else {
		return (GP_OK);
	}
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context)
{
	int res;
	int from_card;
	int folder_nr;
	Camera *camera = data;

	res = find_folder(camera, folder, &from_card, &folder_nr, context);
	if (res != (GP_OK)) {
		return res;
	}

	return dc120_get_filenames(camera, from_card, folder_nr, list, context);

	/* Save the order of the pics (wtf: no filename access on dc120???) */
}

static int camera_file_action (Camera *camera, int action, CameraFile *file,
			       const char *folder, const char *filename,
			       GPContext *context)
{
	CameraList *files = NULL;
	const char* file_name;
	int file_nr;
	int i;
	char *dot;
	int picnum=0;
	int result = GP_OK;

	/*  first find the file */
	int from_card;
	int folder_nr;

	result = find_folder( camera, folder, &from_card, &folder_nr, context );
	if( result != (GP_OK) ) {
		return result;
	}

	result = gp_list_new( &files );
	if( result != GP_OK ) {
		goto fail;
	}

	result = dc120_get_filenames(camera, from_card, folder_nr, files, context);
	if( result != GP_OK ) {
		goto fail;
	}


	/* now we have the list, search for the file. */
	file_nr = -1;
	for( i = 0; i<gp_list_count( files ); i++ ) {
		gp_list_get_name( files, i, &file_name );
		if( strcmp( file_name, filename ) == 0 ) {
			file_nr = i;  /* ok, we found it. */
			break;
		}
	}
	gp_list_free( files );


	if( file_nr == -1 ) { /* not found */
		return GP_ERROR;
	}


	picnum = gp_filesystem_number(camera->fs, folder, filename, context);
	if (picnum < 0)
		return picnum;


	if (action == DC120_ACTION_PREVIEW) { /* FIXME: marcus, fix type */
		dot = strrchr(filename, '.');
		if( dot && strlen( dot )>3 ) {
			strcpy( dot+1, "ppm");
		}
	}
	return (dc120_file_action(camera, action, from_card, folder_nr, picnum+1, file, context));
	/* yes, after that it is to handle failures. */
 fail:
	if (files)
		gp_list_free( files );
	return result;

}


static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *data, GPContext *context)
{
	Camera *camera = data;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		return (camera_file_action (camera, DC120_ACTION_IMAGE, file,
					    folder, filename, context));
	case GP_FILE_TYPE_PREVIEW:
		return (camera_file_action (camera, DC120_ACTION_PREVIEW, file,
					    folder, filename, context));
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
}

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data,
			     GPContext *context)
{
	Camera *camera = data;
	int retval;

	retval = camera_file_action (camera, DC120_ACTION_DELETE, NULL,
				     folder, filename, context);

	return (retval);
}

static int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context) {
	CameraList *list;
	int   count;
	const char* name;

	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	/* capture a image to flash (note: we do not check if full */
	CHECK_RESULT (dc120_capture(camera, path, context));

	/* Get the last picture in the Flash memory */
	gp_list_new(&list);

	dc120_get_filenames (camera, 0, 0, list, context);

	count = gp_list_count(list);
	gp_list_get_name (list, count - 1, &name);
	gp_list_free(list);

	/* Set the filename */
	strcpy(path->folder, "/");

	CHECK_RESULT (gp_filesystem_append (camera->fs,
					    path->folder,
					    path->name, context));
	return (GP_OK);

}

static int camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	static char summary_string[2048] = "";
	char buff[1024];
	Kodak_dc120_status status;

	if (dc120_get_status (camera, &status, context))
	{
		strcpy(summary_string, "Kodak DC120\n");

		snprintf(buff, 1024, "Camera Identification: %s\n", status.camera_id);
		strcat(summary_string, buff);

		snprintf(buff, 1024, "Camera Type: %d\n", status.camera_type_id);
		strcat(summary_string, buff);

		snprintf(buff, 1024, "Firmware: %d.%d\n", status.firmware_major, status.firmware_minor);
		strcat(summary_string, buff);

		snprintf(buff, 1024, "Battery Status: %d\n", status.batteryStatusId);
		strcat(summary_string, buff);

		snprintf(buff, 1024, "AC Status: %d\n", status.acStatusId);
		strcat(summary_string, buff);

		strftime(buff, 1024, "Time: %a, %d %b %Y %T\n", localtime((time_t *)&status.time));
		strcat(summary_string, buff);

		snprintf(buff, 1024, "Total Pictures Taken: %d\n",
			status.taken_pict_mem + status.taken_pict_card);
		strcat(summary_string, buff);

	}

	strcpy(summary->text, summary_string);

	return (GP_OK);
}


static int camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	strcpy (manual->text,
		_("The Kodak DC120 camera uses the KDC file format "
		"for storing images. If you want to view the images you "
		"download from your camera, you will need to download "
		"the \"kdc2tiff\" program. "
		"It is available from http://kdc2tiff.sourceforge.net"));

	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy(about->text,
		_("Kodak DC120 Camera Library\n"
		"Scott Fritzinger <scottf@gphoto.net>\n"
		"Camera Library for the Kodak DC120 camera.\n"
		"(by popular demand)."));

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
};

int camera_init (Camera *camera, GPContext *context) {

	GPPortSettings settings;
	int speed;

	/* First, set up all the function pointers */
	camera->functions->capture 	= camera_capture;
	camera->functions->summary 	= camera_summary;
	camera->functions->manual       = camera_manual;
	camera->functions->about        = camera_about;

	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	/* Configure the port (remember the speed) */
	gp_port_get_settings (camera->port, &settings);
	speed = settings.serial.speed;
	settings.serial.speed    = 9600;
	settings.serial.bits     = 8;
	settings.serial.parity   = 0;
	settings.serial.stopbits = 1;
	gp_port_set_settings (camera->port, settings);
	gp_port_set_timeout (camera->port, TIMEOUT);

	/* Reset the camera to 9600 */
	gp_port_send_break (camera->port, 2);

	/* Wait for it to update */
	usleep(1500 * 1000);

	if (dc120_set_speed (camera, speed) == GP_ERROR) {
		return (GP_ERROR);
	}

	/* Try to talk after speed change */
	if (dc120_get_status(camera, NULL, context) == GP_ERROR) {
		return (GP_ERROR);
	}

	return (GP_OK);
}
