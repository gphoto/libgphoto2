/* pdc700.c
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

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gphoto2-library.h>
#include <gphoto2-port-log.h>

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

#define GP_MODULE "pdc700"

#define PDC700_INIT	0x01
#define PDC700_INFO	0x02

#define PDC700_BAUD	0x04
#define PDC700_PICINFO	0x05
#define PDC700_THUMB	0x06
#define PDC700_PIC	0x07


#define PDC700_FIRST	0x00
#define PDC700_DONE	0x01
#define PDC700_LAST	0x02

typedef struct _PDCDate PDCDate;
struct _PDCDate {
	unsigned char year, month, day;
	unsigned char hour, minute, second;
};

typedef struct _PDCInfo PDCInfo;
struct _PDCInfo {
	unsigned int num_taken, num_free;
	char version[6];
	PDCDate date;
};

typedef struct _PDCPicInfo PDCPicInfo;
struct _PDCPicInfo {
	char version[6];
	unsigned int pic_size, thumb_size;
	unsigned char flash;
};

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

/*
 * Every command sent to the camera begins with 0x40 followed by two bytes
 * indicating the number of following bytes. Then follows the byte that
 * indicates the command (see above defines), perhaps some parameters. The
 * last byte is the checksum, the sum of all bytes between length bytes
 * (3rd one) and the last one (checksum).
 */
static int
calc_checksum (unsigned char *cmd, int len)
{
	int i;
	unsigned char checksum;

	for (checksum = 0, i = 0; i < len; i++)
		checksum += cmd[i];

	return (checksum);
}

static int
pdc700_send (Camera *camera, unsigned char *cmd, int cmd_len)
{
	/* Finish the command and send it */
	cmd[0] = 0x40;
	cmd[1] = (cmd_len - 3) >> 8;
	cmd[2] = (cmd_len - 3) & 0xff;
	cmd[cmd_len - 1] = calc_checksum (cmd + 3, cmd_len - 1 - 3);
	CHECK_RESULT (gp_port_write (camera->port, cmd, cmd_len));

	return (GP_OK);
}

static int
pdc700_read (Camera *camera, unsigned char *cmd,
	     unsigned char *b, int *b_len,
	     char *status, char *sequence_number)
{
	unsigned char header[3], checksum;
	unsigned int i;

	/*
	 * Read the header (0x40 plus 2 bytes indicating how many bytes
	 * will follow)
	 */
	CHECK_RESULT (gp_port_read (camera->port, header, 3));
	if (header[0] != 0x40)
		return (GP_ERROR_CORRUPTED_DATA);
	*b_len = (header[2] << 8) | header [1];

	/* Read the remaining bytes */
	CHECK_RESULT (gp_port_read (camera->port, b, *b_len));

	/*
	 * The first byte indicates if this the response for our command.
	 */
	if (b[0] != (0x80 | cmd[3])) {
		gp_camera_set_error (camera, _("Received unexpected response"));
		return (GP_ERROR_CORRUPTED_DATA);
	}

	/* Will other packets follow? */
	*status = b[1];

	/* Then follows the sequence number (number of next packet if any)
	 * only in case of picture transmission */
	if (sequence_number)
		*sequence_number = b[2];

	/* Check the checksum */
	for (checksum = i = 0; i < *b_len - 1; i++)
		checksum += b[i];
	if (checksum != b[*b_len - 1]) {
		gp_camera_set_error (camera, _("Checksum error"));
		return (GP_ERROR_CORRUPTED_DATA);
	}

	/* Preserve only the actual data */
	*b_len -= (sequence_number ? 4 : 3);
	memmove (b, b + (sequence_number ? 3 : 2), *b_len);

	return (GP_OK);
}

static int
pdc700_transmit (Camera *camera, unsigned char *cmd, int cmd_len,
		 unsigned char *buf, unsigned int *buf_len)
{
	unsigned char status, b[2048], sequence_number;
	unsigned int b_len;
	unsigned int target = *buf_len;

	CHECK_RESULT (pdc700_send (camera, cmd, cmd_len));
	CHECK_RESULT (pdc700_read (camera, cmd, b, &b_len, &status,
			(cmd[4] == PDC700_FIRST) ? &sequence_number : NULL));

	/* Copy over the data */
	*buf_len = b_len;
	memcpy (buf, b, b_len);

	/* Check if other packets are to follow */
	if (cmd[4] == PDC700_FIRST) {

		/* Get those other packets */
		while (status != PDC700_LAST) {
			cmd[4] = PDC700_DONE;
			cmd[5] = sequence_number;
			GP_DEBUG ("Fetching sequence %i...", sequence_number);
			CHECK_RESULT (pdc700_send (camera, cmd, 7));
			CHECK_RESULT (pdc700_read (camera, cmd, b, &b_len,
						&status, &sequence_number));

			/*
			 * Sanity check: We should never read more bytes than
			 * targeted
			 */
			if (*buf_len + b_len > target) {
				gp_camera_set_error (camera, _("The camera "
					"sent more bytes than expected (%i)"),
					target);
				return (GP_ERROR_CORRUPTED_DATA);
			}

			/* Copy over the data */
			memcpy (buf + *buf_len, b, b_len);
			*buf_len += b_len;
			gp_camera_progress (camera,
					    (float) *buf_len / (float) target);
		}

		/* Acknowledge last packet */
		cmd[4] = PDC700_LAST;
		cmd[5] = sequence_number;
		CHECK_RESULT (pdc700_send (camera, cmd, 7));
	}

	return (GP_OK);
}

static int
pdc700_baud (Camera *camera, int baud)
{
	unsigned char b;
	unsigned char cmd[6];
	unsigned char buf[2048];
	int buf_len;

	switch (baud) {
	case 57600:
		b = 0x03;
		break;
	case 38400:
		b = 0x02;
		break;
	case 19200:
		b = 0x01;
		break;
	case 9600:
	default:
		b = 0x00;
		break;
	}

	cmd[3] = PDC700_BAUD;
	cmd[4] = b;
	CHECK_RESULT (pdc700_transmit (camera, cmd, 6, buf, &buf_len));

	return (GP_OK);
}

static int
pdc700_init (Camera *camera)
{
	int buf_len;
	unsigned char cmd[5];
	unsigned char buf[2048];

	cmd[3] = PDC700_INIT;
	CHECK_RESULT (pdc700_transmit (camera, cmd, 5, buf, &buf_len));

	return (GP_OK);
}

static int
pdc700_picinfo (Camera *camera, int n, PDCPicInfo *info)
{
	int buf_len;
	unsigned char cmd[7];
	unsigned char buf[2048];

	cmd[3] = PDC700_PICINFO;
	cmd[4] = n && 0xff;
	cmd[5] = n >> 8;
	CHECK_RESULT (pdc700_transmit (camera, cmd, 7, buf, &buf_len));

	/* We don't know about the meaning of buf[0-1] */

	/* Check if this information is about the right picture */
	if (n != (buf[2] | (buf[3] << 8)))
		return (GP_ERROR_CORRUPTED_DATA);

	/* Picture size */
	info->pic_size = buf[4] | (buf[5] << 8) |
			(buf[6] << 16) | (buf[7] << 24);
	GP_DEBUG ("Size of picture: %i", info->pic_size);

	/* Flash used? */
	info->flash = buf[8];
	GP_DEBUG ("This picture has been taken with%s flash.",
		  buf[8] ? "" : "out");

	/* The meaning of buf[9-17] is unknown */

	/* Thumbnail size */
	info->thumb_size = buf[18] | (buf[19] <<  8) | (buf[20] << 16) |
			  (buf[21] << 24);
	GP_DEBUG ("Size of thumbnail: %i", info->thumb_size);

	/* The meaning of buf[22] is unknown */

	/* Version info */
	strncpy (info->version, &buf[23], 6);

	/*
	 * Now follows some picture data we have yet to reverse
	 * engineer (buf[24-63]).
	 */

	return (GP_OK);
}

static int
pdc700_info (Camera *camera, PDCInfo *info)
{
	int buf_len;
	unsigned char buf[2048];
	unsigned char cmd[5];

	cmd[3] = PDC700_INFO;
	CHECK_RESULT (pdc700_transmit (camera, cmd, 5, buf, &buf_len));

	/*
	 * buf[0-7]: We don't know. The following has been seen:
	 * 01 12 04 01 00 0a 01 00
	 * 01 20 04 02 01 05 01 00
	 * 01 20 04 02 01 05 00 00
	 */

	/* Protocol version */
	strncpy (info->version, &buf[8], 6);

	/* buf[14-15]: We don't know. Seems to be always 00 00 */

	/* Pictures */
	info->num_taken = buf[16] | (buf[17] << 8);
	info->num_free = buf[18] | (buf[19] << 8);

	/* Date */
	info->date.year   = buf[20];
	info->date.month  = buf[21];
	info->date.day    = buf[22];
	info->date.hour   = buf[23];
	info->date.minute = buf[24];
	info->date.second = buf[25];

	/*
	 * buf[26-63]: We don't know:
	 * 
	 * 03 00 f8 b2 64 03 00 00 01 00 00 00 00 00 00 00 00 00 00 00
	 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	 *
	 * 03 00 c6 03 86 28 00 00 01 00 00 00 00 00 00 00 00 00 00 00
	 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	 *
	 * 03 00 3a 7f 65 83 00 00 01 00 00 00 00 00 00 00 00 00 00 00 
	 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	 *
	 * 03 00 23 25 66 83 00 00 01 00 00 00 00 00 00 00 00 00 00 00
	 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	 */

	return (GP_OK);
}

static int
pdc700_pic (Camera *camera, int n, unsigned char **data, unsigned int *size,
	    int thumb)
{
	unsigned char cmd[8];
	int r;
	PDCPicInfo info;

	/* Picture size? Allocate the memory */
	CHECK_RESULT (pdc700_picinfo (camera, n, &info));
	*size = thumb ? info.thumb_size : info.pic_size;
	*data = malloc (sizeof (char) * *size);
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	/* Get picture data */
	cmd[3] = (thumb) ? PDC700_THUMB : PDC700_PIC;
	cmd[4] = PDC700_FIRST;
	cmd[5] = n & 0xff;
	cmd[6] = n >> 8;
	r = pdc700_transmit (camera, cmd, 8, *data, size);
	if (r < 0) {
		free (*data);
		return (r);
	}

	return (GP_OK);
}

int
camera_id (CameraText *id) 
{
	strcpy (id->text, "Polaroid DC700");

	return (GP_OK);
}

static struct {
	const char *model;
} models[] = {
	{"Polaroid DC700"},
	{NULL}
};

int
camera_abilities (CameraAbilitiesList *list) 
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].model; i++) {
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port     = GP_PORT_SERIAL;
		a.speed[0] = 9600;
		a.speed[1] = 19200;
		a.speed[2] = 38400;
		a.speed[3] = 57600;
		a.operations        = GP_OPERATION_NONE;
		a.file_operations   = GP_FILE_OPERATION_NONE;
		a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

		CHECK_RESULT (gp_abilities_list_append (list, a));
	}
	
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename, 
	       CameraFileType type, CameraFile *file, void *user_data)
{
	Camera *camera = user_data;
	int n, size;
	unsigned char *data;

	if (type == GP_FILE_TYPE_RAW)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Get the number of the picture from the filesystem */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, filename));

	/* Get the file */
	CHECK_RESULT (pdc700_pic (camera, n + 1, &data, &size, 
				  (type == GP_FILE_TYPE_NORMAL) ? 0 : 1));

	CHECK_RESULT (gp_file_set_data_and_size (file, data, size));
	CHECK_RESULT (gp_file_set_name (file, filename));
	CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_JPEG));

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about) 
{
	strcpy (about->text, "Download program for Polaroid DC700 camera. "
		"Originally written by Ryan Lantzer "
		"<rlantzer@umr.edu> for gphoto-4.x. Adapted for gphoto2 by "
		"Lutz Müller <urc8@rz.uni-karlsruhe.de>.");

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *about)
{
	PDCInfo info;

	CHECK_RESULT (pdc700_info (camera, &info));

	sprintf (about->text, _(
		"Date: %i/%i/%i %i:%i:%i\n"
		"Pictures taken: %i\n"
		"Free pictures: %i\n"
		"Software version: %s"),
		info.date.year, info.date.month, info.date.day,
		info.date.hour, info.date.minute, info.date.second,
		info.num_taken, info.num_free, info.version);

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data)
{
	Camera *camera = data;
	PDCInfo info;

	/* Fill the list */
	CHECK_RESULT (pdc700_info (camera, &info));
	gp_list_populate (list, "PDC700%04i.jpg", info.num_taken);

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data)
{
	int n;
	Camera *camera = data;
	PDCPicInfo pic_info;

	/* Get the picture number from the CameraFilesystem */
	CHECK_RESULT (n = gp_filesystem_number (fs, folder, file));

	CHECK_RESULT (pdc700_picinfo (camera, n + 1, &pic_info));
	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	strcpy (info->file.type, GP_MIME_JPEG);
	strcpy (info->preview.type, GP_MIME_JPEG);
	info->file.size = pic_info.pic_size;
	info->preview.size = pic_info.thumb_size;

	return (GP_OK);
}

int
camera_init (Camera *camera) 
{
	int result = GP_OK, i;
	GPPortSettings settings;
	int speeds[] = {9600, 57600, 19200, 38400};

        /* First, set up all the function pointers */
	camera->functions->summary	= camera_summary;
        camera->functions->about        = camera_about;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	/* Check if the camera is really there */
	CHECK_RESULT (gp_port_get_settings (camera->port, &settings));
	CHECK_RESULT (gp_port_set_timeout (camera->port, 1000));
	for (i = 0; i < 4; i++) {
		settings.serial.speed = speeds[i];
		CHECK_RESULT (gp_port_set_settings (camera->port, settings));
		result = pdc700_init (camera);
		if (result == GP_OK)
			break;
	}
	if (i == 4)
		return (result);

	/* Set the speed to the highest one */
	if (speeds[i] < 57600) {
		CHECK_RESULT (pdc700_baud (camera, 57600));
		settings.serial.speed = 57600;
		CHECK_RESULT (gp_port_set_settings (camera->port, settings));
	}

	return (GP_OK);
}
