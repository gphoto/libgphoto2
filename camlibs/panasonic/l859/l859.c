/*
	Copyright 2001 Andrew Selkirk <aselkirk@mailandnews.com>

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
#include "config.h"
#include "l859.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "gphoto2-endian.h"
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-library.h>

#ifndef __FILE__
#  define __FILE__ "l859.c"
#endif

#define GP_MODULE "l859"

/******************************************************************************/
/* Internal utility functions */

/*
 * l859_sendcmd - Send command to camera
 */
static int l859_sendcmd(Camera *camera, uint8_t cmd) {

	GP_DEBUG ("Sending command: 0x%02x.", cmd);

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

	GP_DEBUG ("Retrieved Data");

	return GP_OK;
}

/*
 * l859_connect - try hand shake with camera and establish connection
 */
static int l859_connect(Camera *camera) {

	GPPortSettings settings;
	uint8_t	bps;
	int ret;

	GP_DEBUG ("Connecting to a camera.");

	ret = l859_sendcmd(camera, L859_CMD_CONNECT);
	if (ret < GP_OK)
		return ret;
	if (l859_retrcmd(camera) == GP_ERROR) {
		if (l859_sendcmd(camera, L859_CMD_RESET) != GP_OK)
			return GP_ERROR;
		if (l859_sendcmd(camera, L859_CMD_CONNECT)!= GP_OK)
			return GP_ERROR;
		if (l859_retrcmd(camera) == GP_ERROR)
			return GP_ERROR;
	}

	switch (camera->pl->speed) {
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
			break;
	}

	if (bps != L859_CMD_SPEED_DEFAULT) {

		if (l859_sendcmd(camera, bps) != GP_OK)
			return GP_ERROR;

		gp_port_get_settings(camera->port, &settings);
		settings.serial.speed = camera->pl->speed;
		gp_port_set_settings(camera->port, settings);

		if (l859_retrcmd(camera) == GP_ERROR)
			return GP_ERROR;
	}

	if (l859_sendcmd(camera, L859_CMD_INIT) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;

	GP_DEBUG ("Camera connected successfully.");

	return GP_OK;
}

/*
 * l859_disconnect - reset camera, free buffers and close files
 */
static int l859_disconnect(Camera *camera) {

	GP_DEBUG ("Disconnecting the camera.");

	if (l859_sendcmd(camera, L859_CMD_RESET) != GP_OK)
		return GP_ERROR;
	if (gp_port_read(camera->port, camera->pl->buf, 1) == GP_ERROR)
                return GP_ERROR;

	GP_DEBUG ("Camera disconnected.");

	return GP_OK;
}

/*
 *l859_delete - delete image #index from camera memory
 */
static int l859_delete(Camera *camera, uint8_t index) {

	int		value0;
	int		value1;
	int		value2;
	int		value3;

	GP_DEBUG ("Deleting image: %i.", index);

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

	GP_DEBUG ("Image %i deleted.", index);

	return GP_OK;
}

/*
 * l859_selectimage - select image to download, return its size
 */
static int l859_selectimage(Camera *camera, uint8_t index) {

	int			size = 0;
	uint8_t	byte1 = 0;
	uint8_t	byte2 = 0;
	uint8_t	byte3 = 0;
	int		value0;
	int		value1;
	int		value2;
	int		value3;

	GP_DEBUG ("Selecting image: %i.", index);

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

	GP_DEBUG ("Selected image: %i, size: %i.", index, size);

	return size;
}

/*
 * l859_selectimage_preview - select preview image to download,
 * return its size
 */
static int l859_selectimage_preview(Camera *camera, uint8_t index) {

	int			size = 0;
	uint8_t	byte1 = 0;
	uint8_t	byte2 = 0;
	uint8_t	byte3 = 0;
	int		value0;
	int		value1;
	int		value2;
	int		value3;

	GP_DEBUG ("Selected preview image: %i.", index);

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

	GP_DEBUG ("Selected preview image: %i, size: %i.", index, size);

	return size;
}

#if 0
/*
 * l859_readimageblock - read #block block (116 bytes) of an
 * image into buf
 */
static int l859_readimageblock(Camera *camera, int block, char *buffer) {

	int     index;

	GP_DEBUG ("Reading image block: %i.", block);

	if (l859_sendcmd(camera, L859_CMD_ACK) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;

	for (index = 3; index < 116; index++) {
		buffer[index - 3] = camera->pl->buf[index];
	}

	GP_DEBUG ("Block: %i read in.", block);

	return 112;

}
#endif

/******************************************************************************/
/* Library interface functions */

int camera_id (CameraText *id) {

	/* GP_DEBUG ("Camera ID"); */

	strcpy(id->text, "panasonic-l859");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities a;

	memset (&a, 0, sizeof(a));
	a.status = GP_DRIVER_STATUS_PRODUCTION;
	a.port		= GP_PORT_SERIAL;
	a.speed[0] 	= 9600;
	a.speed[1] 	= 19200;
	a.speed[2] 	= 57600;
	a.speed[3] 	= 115200;
	a.speed[4] 	= 0;
	a.operations        = GP_OPERATION_NONE;
	a.file_operations   = GP_FILE_OPERATION_DELETE |
			      GP_FILE_OPERATION_PREVIEW;
	a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

	strncpy (a.model, "Panasonic:PV-L691", sizeof (a.model));
	gp_abilities_list_append (list, a);
	strncpy (a.model, "Panasonic:PV-L859", sizeof (a.model));
	gp_abilities_list_append (list, a);

	return GP_OK;
}

static int camera_exit (Camera *camera, GPContext *context) {

	GP_DEBUG ("Camera Exit");

	if (camera->pl) {
		l859_disconnect(camera);
		free(camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context) {

	Camera *camera = data;
	int	num = 0;
	int	width;
	int	year;
	int	size;
	uint8_t	month;
	uint8_t	day;
	uint8_t	hour;
	uint8_t	minute;
	uint8_t	byte1;
	uint8_t	byte2;
	uint8_t	byte3;
	char	*filename;

	GP_DEBUG ("Camera List Files");

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

		/* Check for no images */
		if (size == 0)
			return GP_OK;

		if (!(filename = (char*)malloc(30))) {
			GP_DEBUG ("Unable to allocate memory for filename.");
			return GP_ERROR_NO_MEMORY;
		}

		sprintf(filename, "%.4i%s%i-%i-%i(%i-%i).jpg", num + 1,
			width == 640 ? "F" : "N", year, month, day, hour, minute);
		GP_DEBUG ("Filename: %s.", filename);

		gp_list_append (list, filename, NULL);
/*		GP_DEBUG ("Num %i Filename %s", num, filename); */
		free (filename);
		num = num + 1;
		if (l859_sendcmd(camera, L859_CMD_PREVIEW_NEXT) != GP_OK)
			return GP_ERROR;
		if (l859_retrcmd(camera) == GP_ERROR)
			return GP_ERROR;
	}
	GP_DEBUG ("Camera List Files Done");
	return GP_OK;
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *data, GPContext *context) {

	Camera *camera = data;
	int		index;
	int		size;
	int		i;
	int		s;
	char	buffer[112];
	int		bufIndex;
	unsigned int id;

	GP_DEBUG ("Get File %s", filename);

	/* index is the 0-based image number on the camera */
	index = gp_filesystem_number (camera->fs, folder, filename, context);
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

	id = gp_context_progress_start (context, size,
		_("Downloading '%s'..."), filename);
	for (i = 0, s = 0; s < size; i++) {

		if (l859_sendcmd(camera, L859_CMD_ACK) != GP_OK)
			return GP_ERROR;
		if (l859_retrcmd(camera) == GP_ERROR)
			return GP_ERROR;

		for (bufIndex = 3; bufIndex < 115 && s < size; bufIndex++) {
			buffer[bufIndex - 3] = camera->pl->buf[bufIndex];
			s += 1;
		}
		GP_DEBUG ("Packet Size: %i Data Size: %i", bufIndex - 3, s);

		/* Append data to the file */
		gp_file_append (file, buffer, bufIndex - 3);

		/* Check for cancellation */
		gp_context_progress_update (context, id, s);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			l859_disconnect (camera);
			l859_connect    (camera);
			return (GP_ERROR_CANCEL);
		}
	}

	gp_file_set_mime_type (file, GP_MIME_JPEG); 
	GP_DEBUG ("Camera Get File Done");
	return (GP_OK);
}

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data,
			     GPContext *context) {

	Camera *camera = data;
	int		index;
	int		result;

	GP_DEBUG ("Delete File %s", filename);

	/* index is the 0-based image number on the camera */
	index = gp_filesystem_number (camera->fs, folder, filename, context);
	if (index < 0)
		return (index);

	result = l859_delete (camera, index);
	if (result < 0)
		return (result);

	GP_DEBUG ("Delete File Done");

	return GP_OK;
}

static int delete_all_func (CameraFilesystem *fs, const char *folder,
			    void *data, GPContext *context) {

	Camera *camera = data;

	GP_DEBUG ("Delete all images");

	if (l859_sendcmd(camera, L859_CMD_DELETE_ALL) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(camera) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(camera, L859_CMD_DELETE_ACK) != GP_OK)
		return GP_ERROR;

	GP_DEBUG ("Delete all images done.");

	return GP_OK;
}

static int camera_summary (Camera *camera, CameraText *summary,
			   GPContext *context) {

	strcpy (summary->text,
		_("Panasonic PV-L859-K/PV-L779-K Palmcorder\n"
		  "\n"
		  "Panasonic introduced image capturing technology "
		  "called PHOTOSHOT for the first time, in this series "
		  "of Palmcorders.  Images are stored in JPEG format on "
		  "an internal flashcard and can be transferred to a "
		  "computer through the built-in serial port.  Images "
		  "are saved in one of two resolutions; NORMAL is "
		  "320x240 and FINE is 640x480.  The CCD device which "
		  "captures the images from the lens is only 300K and "
		  "thus produces only low quality pictures."));
	return (GP_OK);
}

static int camera_manual (Camera *camera, CameraText *manual,
			  GPContext *context) {

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

static int camera_about (Camera *camera, CameraText *about,
			 GPContext *context) {

	strcpy(about->text,
			_("Panasonic PV-L859-K/PV-L779-K Palmcorder Driver\n"
			"Andrew Selkirk <aselkirk@mailandnews.com>"));
	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.delete_all_func = delete_all_func,
	.del_file_func = delete_file_func
};

int camera_init (Camera *camera, GPContext *context) {

        int ret;
	GPPortSettings settings;

        /* First, set up all the function pointers */
        camera->functions->exit    = camera_exit;
        camera->functions->summary = camera_summary;
        camera->functions->manual  = camera_manual;
	camera->functions->about   = camera_about;

	/* Allocate memory for a read/write buffer */
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);

        gp_port_set_timeout (camera->port, 2000);
	gp_port_get_settings(camera->port, &settings);
	camera->pl->speed = settings.serial.speed;
	settings.serial.speed      = 9600; /* hand shake speed */
	settings.serial.bits       = 8;
	settings.serial.parity     = 0;
	settings.serial.stopbits   = 1;
        gp_port_set_settings(camera->port, settings);

	/* Set up the filesystem */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

        ret = l859_connect (camera);
	if (ret < 0) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return (ret);
}

/* End of l859.c */
