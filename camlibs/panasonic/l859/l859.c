/*
	$Id$

	Copyright (c) 2001 Andrew Selkirk <aselkirk@mailandnews.com>

	This file is part of the gPhoto project and may only be used,  modified,
	and distributed under the terms of the gPhoto project license,  COPYING.
	By continuing to use, modify, or distribute  this file you indicate that
	you have read the license, understand and accept it fully.

	THIS  SOFTWARE IS PROVIDED AS IS AND COME WITH NO WARRANTY  OF ANY KIND,
	EITHER  EXPRESSED OR IMPLIED.  IN NO EVENT WILL THE COPYRIGHT  HOLDER BE
	LIABLE FOR ANY DAMAGES RESULTING FROM THE USE OF THIS SOFTWARE.

	Note:

	This is the Panasonic PV-L859 camera gPhoto library source code.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

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

#include "l859.h"

#ifndef __FILE__
#  define __FILE__ "l859.c"
#endif

/******************************************************************************/
/* Internal utility functions */

static void l859_debug(char *format, ...) {

	va_list		pvar;
	char		msg[512];

	va_start(pvar, format);
	vsprintf(msg, format, pvar);
	va_end(pvar);

	gp_debug_printf(GP_DEBUG_LOW, "l859", "%s\n", msg);

}

/*
 * l859_sendcmd - Send command to camera
 */
static int l859_sendcmd(Camera *camera, u_int8_t cmd) {

	l859_debug("Sending command: 0x%02x.", cmd);

	memset(camera->pl->buf, 0, 1);
	camera->pl->buf[0] = cmd;

	return gp_port_write(camera->port, camera->pl->buf, 1);

}

/*
 * l859_retrcmd - Packet received from camera
 */
static int l859_retrcmd(Camera *camera) {

	int     s;

	if ((s = gp_port_read(camera->port, camera->pl->buf, 116)) == GP_ERROR)
		return GP_ERROR;

	if (s != 116)
		return GP_ERROR;

	camera->pl->size = 116;

	l859_debug("Retrieved Data");

	return GP_OK;
}

/*
 * l859_connect - try hand shake with camera and establish connection
 */
static int l859_connect(Camera *camera, int speed) {

	GPPortSettings settings;
	u_int8_t	bps;

	l859_debug("Connecting to a camera.");

	l859_sendcmd(camera, L859_CMD_CONNECT);
	if (l859_retrcmd(camera) == GP_ERROR) {
		if (l859_sendcmd(camera, L859_CMD_RESET) != GP_OK)
			return GP_ERROR;
		if (l859_sendcmd(camera, L859_CMD_CONNECT)!= GP_OK)
			return GP_ERROR;
		if (l859_retrcmd(camera) == GP_ERROR)
			return GP_ERROR;
	}

	switch (speed) {
		case 19200:
			bps = L859_CMD_SPEED_19200;
			break;
		case 57600:
			bps = L859_CMD_SPEED_57600;
			break;
		case 115200:
			bps = L859_CMD_SPEED_115200;
			break;
		default:
			bps = L859_CMD_SPEED_DEFAULT;
	}

	if (bps != L859_CMD_SPEED_DEFAULT) {

		if (l859_sendcmd(camera, bps) != GP_OK)
			return GP_ERROR;

		gp_port_settings_get(camera->port, &settings);
		settings.serial.speed = speed;
		gp_port_settings_set(camera->port, settings);

		if (l859_retrcmd(camera) == GP_ERROR)
			return GP_ERROR;
	}

	if (l859_sendcmd(camera, L859_CMD_INIT) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;

	l859_debug("Camera connected successfully.");

	return GP_OK;
}

/*
 * l859_disconnect - reset camera, free buffers and close files
 */
static int l859_disconnect(Camera *camera) {

	l859_debug("Disconnecting the camera.");

	if (l859_sendcmd(camera, L859_CMD_RESET) != GP_OK)
		return GP_ERROR;
	if (gp_port_read(camera->port, camera->pl->buf, 1) == GP_ERROR)
                return GP_ERROR;

	l859_debug("Camera disconnected.");

	return GP_OK;
}

/*
 *l859_delete - delete image #index from camera memory
 */
static int l859_delete(Camera *camera, u_int8_t index) {

	int		value0;
	int		value1;
	int		value2;
	int		value3;

	l859_debug("Deleting image: %i.", index);

	value0 = index;
	value1 = value0 % 10;
	value2 = (value0 - value1) % 100;
	value3 = (value0 - value1 - value2) / 100;
	value2 = value2 / 10;

	if (l859_sendcmd(camera, L859_CMD_DELETE_1) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, 0xa0 + value1) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, 0xb0 + value2) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, 0xc0 + value3) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, L859_CMD_DELETE_2) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, L859_CMD_DELETE_3) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, L859_CMD_DELETE_ACK) != GP_OK)
		return GP_ERROR;

	l859_debug("Image %i deleted.", index);

	return GP_OK;
}

/*
 * l859_selectimage - select image to download, return its size
 */
static int l859_selectimage(Camera *camera, u_int8_t index) {

	int			size = 0;
	u_int8_t	byte1 = 0;
	u_int8_t	byte2 = 0;
	u_int8_t	byte3 = 0;
	int		value0;
	int		value1;
	int		value2;
	int		value3;

	l859_debug("Selecting image: %i.", index);

	value0 = index;
	value1 = value0 % 10;
	value2 = (value0 - value1) % 100;
	value3 = (value0 - value1 - value2) / 100;
	value2 = value2 / 10;

	if (l859_sendcmd(camera, 0xa0 + value1) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, 0xb0 + value2) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, 0xc0 + value3) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, L859_CMD_IMAGE) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;

	byte1 = camera->pl->buf[5];
	byte2 = camera->pl->buf[6];
	byte3 = camera->pl->buf[7];

	size = (byte1 * 256 * 256) + (byte2 * 256) + byte3;

	l859_debug("Selected image: %i, size: %i.", index, size);

	return size;
}

/*
 * l859_selectimage_preview - select preview image to download,
 * return its size
 */
static int l859_selectimage_preview(Camera *camera, u_int8_t index) {

	int			size = 0;
	u_int8_t	byte1 = 0;
	u_int8_t	byte2 = 0;
	u_int8_t	byte3 = 0;
	int		value0;
	int		value1;
	int		value2;
	int		value3;

	l859_debug("Selected preview image: %i.", index);

	value0 = index;
	value1 = value0 % 10;
	value2 = (value0 - value1) % 100;
	value3 = (value0 - value1 - value2) / 100;
	value2 = value2 / 10;

	if (l859_sendcmd(camera, 0xa0 + value1) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, 0xb0 + value2) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, 0xc0 + value3) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, L859_CMD_PREVIEW) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;

	byte1 = camera->pl->buf[5];
	byte2 = camera->pl->buf[6];
	byte3 = camera->pl->buf[7];

	size = (byte1 * 256 * 256) + (byte2 * 256) + byte3;

	l859_debug("Selected preview image: %i, size: %i.", index, size);

	return size;
}

#if 0
/*
 * l859_readimageblock - read #block block (116 bytes) of an
 * image into buf
 */
static int l859_readimageblock(Camera *camera, int block, char *buffer) {

	int     index;

	l859_debug("Reading image block: %i.", block);

	if (l859_sendcmd(camera, L859_CMD_ACK) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;

	for (index = 3; index < 116; index++) {
		buffer[index - 3] = camera->pl->buf[index];
	}

	l859_debug("Block: %i read in.", block);

	return 112;

}
#endif

/******************************************************************************/
/* Library interface functions */

int camera_id (CameraText *id) {

	l859_debug("Camera ID");

	strcpy(id->text, "panasonic-l859");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities a;

	l859_debug("Camera Abilities");

	strcpy(a.model, "Panasonic PV-L859");
	a.status = GP_DRIVER_STATUS_PRODUCTION;
	a.port		= GP_PORT_SERIAL;
	a.speed[0] 	= 9600;
	a.speed[1] 	= 19200;
	a.speed[2] 	= 57600;
	a.speed[3] 	= 115200;
	a.speed[4] 	= 0;
	a.operations        = GP_OPERATION_NONE;
	a.file_operations   = GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW;
	a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

	if (gp_abilities_list_append(list, a) == GP_ERROR)
		return GP_ERROR;

	return GP_OK;
}

static int camera_exit (Camera *camera) {

	l859_debug("Camera Exit");

	if (camera->pl) {
		l859_disconnect(camera);
		free(camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data) {

	Camera *camera = data;
	int			num = 0;
	int			width;
	int			year;
	int			size;
	u_int8_t	month;
	u_int8_t	day;
	u_int8_t	hour;
	u_int8_t	minute;
	u_int8_t	byte1;
	u_int8_t	byte2;
	u_int8_t	byte3;
	char		*filename;

	l859_debug("Camera List Files");

	if (l859_sendcmd(camera, 0xa0) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, 0xb0) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, 0xc0) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, L859_CMD_PREVIEW) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;

	while (camera->pl->buf[13] == num) {

		byte1 = camera->pl->buf[8];
		byte2 = camera->pl->buf[9];
		width = (byte1 * 256) + byte2;
		byte1 = camera->pl->buf[22];
		year = byte1 + 1900;
		month = camera->pl->buf[23];
		day = camera->pl->buf[24];
		hour = camera->pl->buf[25];
		minute = camera->pl->buf[26];

		byte1 = camera->pl->buf[5];
		byte2 = camera->pl->buf[6];
		byte3 = camera->pl->buf[7];
		size = (byte1 * 256 * 256) + (byte2 * 256) + byte3;

		// Check for no images
		if (size == 0)
			return GP_OK;

		if (!(filename = (char*)malloc(30))) {
			l859_debug("Unable to allocate memory for filename.");
			return (int) NULL;
		}

		sprintf(filename, "%.4i%s%i-%i-%i(%i-%i).jpg", num + 1,
			width == 640 ? "F" : "N", year, month, day, hour, minute);
		l859_debug("Filename: %s.", filename);

		gp_list_append (list, filename, NULL);
/*		l859_debug("Num %i Filename %s", num, filename); */

		num = num + 1;
		if (l859_sendcmd(camera, L859_CMD_PREVIEW_NEXT) != GP_OK)
			return GP_ERROR;
		if (l859_retrcmd(camera) == GP_ERROR)
			return GP_ERROR;

	}

	l859_debug("Camera List Files Done");

	return GP_OK;
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *data) {

	Camera *camera = camera;
	int		index;
	int		size;
	int		i;
	int		s;
	char	buffer[112];
	int		bufIndex;

	l859_debug("Get File %s", filename);

	/* index is the 0-based image number on the camera */
	index = gp_filesystem_number (camera->fs, folder, filename);
	if (index < 0)
		return (index);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		size = l859_selectimage(camera, index);
		break;
	case GP_FILE_TYPE_PREVIEW:
		size = l859_selectimage_preview(camera, index);
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
	if (size < 0)
		return (size);

	for (i = 0, s = 0; s < size; i++) {

		if (l859_sendcmd(camera, L859_CMD_ACK) != GP_OK)
			return GP_ERROR;
		if (l859_retrcmd(camera) == GP_ERROR)
			return GP_ERROR;

		for (bufIndex = 3; bufIndex < 115 && s < size; bufIndex++) {
			buffer[bufIndex - 3] = camera->pl->buf[bufIndex];
			s += 1;
		}
		l859_debug("Packet Size: %i Data Size: %i", bufIndex - 3, s);
		gp_file_append (file, buffer, bufIndex - 3);

		gp_camera_progress (camera, (float)(s)/(float)size);
	}

	gp_file_set_name (file, filename);
	gp_file_set_mime_type (file, GP_MIME_JPEG); 

	l859_debug("Camera Get File Done");

	return (GP_OK);
}

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data) {

	Camera *camera = data;
	int		index;
	int		result;

	l859_debug("Delete File %s", filename);

	/* index is the 0-based image number on the camera */
	index = gp_filesystem_number (camera->fs, folder, filename);
	if (index < 0)
		return (index);

	result = l859_delete (camera, index);
	if (result < 0)
		return (result);

	l859_debug("Delete File Done");

	return GP_OK;
}

static int delete_all_func (CameraFilesystem *fs, const char *folder,
			    void *data) {

	Camera *camera = data;

	l859_debug("Delete all images");

	if (l859_sendcmd(camera, L859_CMD_DELETE_ALL) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, L859_CMD_DELETE_ACK) != GP_OK)
		return GP_ERROR;

	l859_debug("Delete all images done.");

	return GP_OK;
}

static int camera_summary (Camera *camera, CameraText *summary) {

	strcpy (summary->text,
			_("Panasonic PV-L859-K/PV-L779-K Palmcorder\n"
				"\n"
				"Panasonic introduced image capturing technology "
				"called PHOTOSHOT for the first time to this series "
				"of Palmcorders.  Images are stored in JPEG format on "
				"an internal flashcard and can be transfered to a "
				"computer through the built-in serial port.  Images "
				"are saved in one of two resolutions; NORMAL is "
				"320x240 and FINE is 640x480.  The CCD device which "
				"captures the images from the lens is only 300K and "
				"thus produces only low quality pictures."));
	return (GP_OK);
}

static int camera_manual (Camera *camera, CameraText *manual) {

	strcpy (manual->text,
			_("Known problems:\n"
				"\n"
				"If communications problems occur, reset the camera "
				"and restart the application.  The driver is not robust "
				"enough yet to recover from these situations, especially "
				"if a problem occurs and the camera is not properly shutdown "
				"at speeds faster than 9600."));
	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about) {

	strcpy(about->text,
			_("Panasonic PV-L859-K/PV-L779-K Palmcorder Driver\n"
			"Andrew Selkirk <aselkirk@mailandnews.com>"));
	return (GP_OK);
}

int camera_init (Camera *camera) {

        int ret, speed;
	GPPortSettings settings;

        /* First, set up all the function pointers */
        camera->functions->exit    = camera_exit;
        camera->functions->summary = camera_summary;
        camera->functions->manual  = camera_manual;
	camera->functions->about   = camera_about;

        gp_port_timeout_set(camera->port, 2000);
	gp_port_settings_get(camera->port, &settings);
	speed = settings.serial.speed;
	settings.serial.speed      = 9600; /* hand shake speed */
	settings.serial.bits       = 8;
	settings.serial.parity     = 0;
	settings.serial.stopbits   = 1;
        gp_port_settings_set(camera->port, settings);

        /* allocate memory for a read/write buffer */
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);

	/* Set up the filesystem */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      delete_file_func, camera);
	gp_filesystem_set_folder_funcs (camera->fs, NULL, delete_all_func,
					camera);

        ret = l859_connect (camera, speed);
	if (ret < 0) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return (ret);
}

/* End of l859.c */
