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

/*
 * l859_dumpmem - Dump bytes to screen
 */
void l859_dumpmem(void *buf, int size) {

        int     i;

        fprintf(stderr, "\nMemory dump: size: %i, contents:\n", size);
        for (i = 0; i < size; i ++)
                fprintf(
                        stderr,
                        *((char*)buf + i) >= 32 &&
                        *((char*)buf + i) < 127 ? "%c" : "\\x%02x",
                        (u_int8_t)*((char*)buf + i)
                );
        fprintf(stderr, "\n\n");
}

void l859_debug(char *format, ...) {

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
int l859_sendcmd(l859_t *dsc, u_int8_t cmd) {

	l859_debug("Sending command: 0x%02x.", cmd);

	memset(dsc->buf, 0, 1);
	dsc->buf[0] = cmd;

	return gp_port_write(dsc->dev, dsc->buf, 1);

}

/*
 * l859_retrcmd - Packet received from camera
 */
int l859_retrcmd(l859_t *dsc) {

	int     s;

	if ((s = gp_port_read(dsc->dev, dsc->buf, 116)) == GP_ERROR)
		return GP_ERROR;

	if (s != 116)
		return GP_ERROR;

	dsc->size = 116;

	l859_debug("Retrieved Data");

	return GP_OK;
}

/*
 * l859_connect - try hand shake with camera and establish connection
 */
int l859_connect(l859_t *dsc, int speed) {

	u_int8_t	bps;

	l859_debug("Connecting to a camera.");

	l859_sendcmd(dsc, L859_CMD_CONNECT);
	if (l859_retrcmd(dsc) == GP_ERROR) {
		if (l859_sendcmd(dsc, L859_CMD_RESET) != GP_OK)
			return GP_ERROR;
		if (l859_sendcmd(dsc, L859_CMD_CONNECT)!= GP_OK)
			return GP_ERROR;
		if (l859_retrcmd(dsc) == GP_ERROR)
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

		if (l859_sendcmd(dsc, bps) != GP_OK)
			return GP_ERROR;

		dsc->settings.serial.speed = speed;
		if (gp_port_settings_set(dsc->dev, dsc->settings) != GP_OK)
			return GP_ERROR;

		if (l859_retrcmd(dsc) == GP_ERROR)
			return GP_ERROR;
	}

	if (l859_sendcmd(dsc, L859_CMD_INIT) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;

	l859_debug("Camera connected successfully.");

	return GP_OK;
}

/*
 * l859_disconnect - reset camera, free buffers and close files
 */
int l859_disconnect(l859_t *dsc) {

	l859_debug("Disconnecting the camera.");

	if (l859_sendcmd(dsc, L859_CMD_RESET) != GP_OK)
		return GP_ERROR;
	if (gp_port_read(dsc->dev, dsc->buf, 1) == GP_ERROR)
                return GP_ERROR;

	l859_debug("Camera disconnected.");

	return GP_OK;
}

/*
 *l859_delete - delete image #index from camera memory
 */
int l859_delete(l859_t *dsc, u_int8_t index) {

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

	if (l859_sendcmd(dsc, L859_CMD_DELETE_1) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, 0xa0 + value1) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, 0xb0 + value2) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, 0xc0 + value3) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, L859_CMD_DELETE_2) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, L859_CMD_DELETE_3) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, L859_CMD_DELETE_ACK) != GP_OK)
		return GP_ERROR;

	l859_debug("Image %i deleted.", index);

	return GP_OK;
}

/*
 * l859_selectimage - select image to download, return its size
 */
int l859_selectimage(l859_t *dsc, u_int8_t index) {

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

	if (l859_sendcmd(dsc, 0xa0 + value1) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, 0xb0 + value2) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, 0xc0 + value3) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, L859_CMD_IMAGE) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;

	byte1 = dsc->buf[5];
	byte2 = dsc->buf[6];
	byte3 = dsc->buf[7];

	size = (byte1 * 256 * 256) + (byte2 * 256) + byte3;

	l859_debug("Selected image: %i, size: %i.", index, size);

	return size;
}

/*
 * l859_selectimage_preview - select preview image to download,
 * return its size
 */
int l859_selectimage_preview(l859_t *dsc, u_int8_t index) {

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

	if (l859_sendcmd(dsc, 0xa0 + value1) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, 0xb0 + value2) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, 0xc0 + value3) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, L859_CMD_PREVIEW) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;

	byte1 = dsc->buf[5];
	byte2 = dsc->buf[6];
	byte3 = dsc->buf[7];

	size = (byte1 * 256 * 256) + (byte2 * 256) + byte3;

	l859_debug("Selected preview image: %i, size: %i.", index, size);

	return size;
}

/*
 * l859_readimageblock - read #block block (116 bytes) of an
 * image into buf
 */
int l859_readimageblock(l859_t *dsc, int block, char *buffer) {

	int     index;

	l859_debug("Reading image block: %i.", block);

	if (l859_sendcmd(dsc, L859_CMD_ACK) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;

	for (index = 3; index < 116; index++) {
		buffer[index - 3] = dsc->buf[index];
	}

	l859_debug("Block: %i read in.", block);

	return 112;

}

/******************************************************************************/
/* Library interface functions */

int camera_id (CameraText *id) {

	l859_debug("Camera ID");

	strcpy(id->text, "panasonic-l859");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities *a;

	l859_debug("Camera Abilities");

	if (!(a = gp_abilities_new()))
		return GP_ERROR;

	strcpy(a->model, "Panasonic PV-L859");
	a->port		= GP_PORT_SERIAL;
	a->speed[0] 	= 9600;
	a->speed[1] 	= 19200;
	a->speed[2] 	= 57600;
	a->speed[3] 	= 115200;
	a->speed[4] 	= 0;
	a->operations        = GP_OPERATION_NONE;
	a->file_operations   = GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW;
	a->folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

	if (gp_abilities_list_append(list, a) == GP_ERROR)
		return GP_ERROR;

	return GP_OK;
}

int camera_init (Camera *camera) {

	int		ret;
	l859_t	*dsc = NULL;

	l859_debug("Camera Init %s", camera->model);

	/* First, set up all the function pointers */
	camera->functions->id 					= camera_id;
	camera->functions->abilities 			= camera_abilities;
	camera->functions->init 				= camera_init;
	camera->functions->exit 				= camera_exit;
	camera->functions->folder_list_folders 	= camera_folder_list_folders;
	camera->functions->folder_list_files   	= camera_folder_list_files;
	camera->functions->folder_delete_all   	= camera_folder_delete_all;
	camera->functions->file_get 			= camera_file_get;
	camera->functions->file_get_preview 	= camera_file_get_preview;
	camera->functions->file_delete			= camera_file_delete;
	camera->functions->summary				= camera_summary;
	camera->functions->manual 				= camera_manual;
	camera->functions->about 				= camera_about;

	if (dsc && dsc->dev) {
		gp_port_close(dsc->dev);
		gp_port_free(dsc->dev);
	}
	free(dsc);

	/* first of all allocate memory for a dsc struct */
	if ((dsc = (l859_t*)malloc(sizeof(l859_t))) == NULL) {
		l859_debug("Unable to allocate memory for data structure");
		return GP_ERROR;
	}

	camera->camlib_data = dsc;

	if ((ret = gp_port_new(&(dsc->dev), GP_PORT_SERIAL)) < 0) {
		free(dsc);
		return (ret);
	}

	gp_port_timeout_set(dsc->dev, 2000);
	strcpy(dsc->settings.serial.port, camera->port->path);
	dsc->settings.serial.speed 	= 9600; /* hand shake speed */
	dsc->settings.serial.bits	= 8;
	dsc->settings.serial.parity	= 0;
	dsc->settings.serial.stopbits	= 1;

	gp_port_settings_set(dsc->dev, dsc->settings);

	gp_port_open(dsc->dev);

	/* allocate memory for a dsc read/write buffer */
	if ((dsc->buf = (char *)malloc(sizeof(char)*(L859_BUFSIZE))) == NULL) {
		l859_debug("Unable to allocate memory for read/write buffer");
		free(dsc);
		return GP_ERROR;
	}

	/* allocate memory for camera filesystem struct */
	if ((ret = gp_filesystem_new(&dsc->fs)) < 0) {
		l859_debug("Unable to allocate memory for camera filesystem");
		free(dsc);
		return ret;
	}

	return l859_connect(dsc, camera->port->speed);

}

int camera_exit (Camera *camera) {

	l859_t	*dsc = (l859_t *)camera->camlib_data;

	l859_debug("Camera Exit");

	l859_disconnect(dsc);

	if (dsc->dev) {
		gp_port_close(dsc->dev);
		gp_port_free(dsc->dev);
	}
	if (dsc->fs)
		gp_filesystem_free(dsc->fs);

	free(dsc);

	return (GP_OK);
}

int camera_folder_list_folders (Camera *camera, const char *folder,
				CameraList *list) {

	l859_debug("Camera List Folders");

	return GP_OK; 	/* folders are unsupported but it is OK */
}

int camera_folder_list_files (Camera *camera, const char *folder,
			      CameraList *list) {

	l859_t		*dsc = (l859_t *)camera->camlib_data;
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

	gp_filesystem_free(dsc->fs);
	gp_filesystem_new(&dsc->fs);

	if (l859_sendcmd(dsc, 0xa0) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, 0xb0) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, 0xc0) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, L859_CMD_PREVIEW) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;

	while (dsc->buf[13] == num) {

		byte1 = dsc->buf[8];
		byte2 = dsc->buf[9];
		width = (byte1 * 256) + byte2;
		byte1 = dsc->buf[22];
		year = byte1 + 1900;
		month = dsc->buf[23];
		day = dsc->buf[24];
		hour = dsc->buf[25];
		minute = dsc->buf[26];

		byte1 = dsc->buf[5];
		byte2 = dsc->buf[6];
		byte3 = dsc->buf[7];
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

		gp_filesystem_append(dsc->fs, "/", filename);
		gp_list_append (list, filename, NULL);
/*		l859_debug("Num %i Filename %s", num, gp_filesystem_name (dsc->fs, "/", num)); */

		num = num + 1;
		if (l859_sendcmd(dsc, L859_CMD_PREVIEW_NEXT) != GP_OK)
			return GP_ERROR;
		if (l859_retrcmd(dsc) == GP_ERROR)
			return GP_ERROR;

	}

	l859_debug("Camera List Files Done");

	return GP_OK;
}

int camera_file_get (Camera *camera, const char *folder, const char *filename,
		     CameraFile *file) {

	l859_t	*dsc = (l859_t *)camera->camlib_data;
	int		index;
	int		size;
	int		i;
	int		s;
	char	*buffer;
	int		bufIndex;

	l859_debug("Get File %s", filename);

	/* index is the 0-based image number on the camera */
	index = gp_filesystem_number (dsc->fs, folder, filename);
	if (index < 0)
		return (index);

	if ((size = l859_selectimage(dsc, index)) < 0)
		return (GP_ERROR);

	if (!(buffer = (char*)malloc(112))) {
		l859_debug("Unable to allocate memory for image data.");
		return (int) NULL;
	}

	gp_file_set_name (file, filename);
	gp_file_set_type (file, "image/jpeg");

	gp_frontend_progress (camera, file, 0.00);

	for (i = 0, s = 0; s < size; i++) {

		if (l859_sendcmd(dsc, L859_CMD_ACK) != GP_OK)
			return GP_ERROR;
		if (l859_retrcmd(dsc) == GP_ERROR)
			return GP_ERROR;

		for (bufIndex = 3; bufIndex < 115 && s < size; bufIndex++) {
			buffer[bufIndex - 3] = dsc->buf[bufIndex];
			s += 1;
		}
		l859_debug("Packet Size: %i Data Size: %i", bufIndex - 3, s);
		gp_file_append (file, buffer, bufIndex - 3);

		gp_frontend_progress (camera, file,
				(float)(s)/(float)size*100.0);
	}

	l859_debug("Camera Get File Done");

	return (GP_OK);
}

int camera_file_get_preview (Camera *camera, const char *folder,
			const char *filename, CameraFile *file) {

	l859_t	*dsc = (l859_t *)camera->camlib_data;
	int		index;
	int		size;
	int		i;
	int		s;
	char    *buffer;
	int     bufIndex;

	l859_debug("Get preview image %s.", filename);

	/* index is the 0-based image number on the camera */
	index = gp_filesystem_number (dsc->fs, folder, filename);
	if (index < 0)
		return (index);

	if ((size = l859_selectimage_preview(dsc, index)) < 0)
		return (GP_ERROR);

	if (!(buffer = (char*)malloc(112))) {
		l859_debug("Unable to allocate memory for preview image data.");
		return (int) NULL;
	}

	gp_file_set_name (file, filename);
	gp_file_set_type (file, "image/jpeg");

	gp_frontend_progress (camera, file, 0.00);

	for (i = 0, s = 0; s < size; i++) {

		if (l859_sendcmd(dsc, L859_CMD_ACK) != GP_OK)
			return GP_ERROR;
		if (l859_retrcmd(dsc) == GP_ERROR)
			return GP_ERROR;

		for (bufIndex = 3; bufIndex < 115 && s < size; bufIndex++) {
			buffer[bufIndex - 3] = dsc->buf[bufIndex];
			s += 1;
		}
		l859_debug("Packet Size: %i Data Size: %i", bufIndex - 3, s);
		gp_file_append (file, buffer, bufIndex - 3);

		gp_frontend_progress (camera, file,
					(float)(s)/(float)size*100.0);
	}

	l859_debug("Camera Get File Preview Done");

	return (GP_OK);
}

int camera_file_delete (Camera *camera, const char *folder,
			const char *filename) {

	l859_t	*dsc = (l859_t *)camera->camlib_data;
	int		index;
	int		result;

	l859_debug("Delete File %s", filename);

	/* index is the 0-based image number on the camera */
	index = gp_filesystem_number (dsc->fs, folder, filename);
	if (index < 0)
		return (index);

	result = l859_delete (dsc, index);

	l859_debug("Delete File Done");

	return result;
}

int camera_folder_delete_all(Camera *camera, const char *folder) {

	l859_t	*dsc = (l859_t *)camera->camlib_data;

	l859_debug("Delete all images");

	if (l859_sendcmd(dsc, L859_CMD_DELETE_ALL) != GP_OK)
		return GP_ERROR;
	if (l859_retrcmd(dsc) == GP_ERROR)
		return GP_ERROR;
	if (l859_sendcmd(dsc, L859_CMD_DELETE_ACK) != GP_OK)
		return GP_ERROR;

	l859_debug("Delete all images done.");

	return GP_OK;
}

int camera_summary (Camera *camera, CameraText *summary) {

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

int camera_manual (Camera *camera, CameraText *manual) {

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

int camera_about (Camera *camera, CameraText *about) {

	strcpy(about->text,
			_("Panasonic PV-L859-K/PV-L779-K Palmcorder Driver\n"
			"Andrew Selkirk <aselkirk@mailandnews.com>"));
	return (GP_OK);
}

/* End of l859.c */
