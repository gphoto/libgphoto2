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

#include <gphoto2-library.h>
#include <gphoto2-core.h>
#include <stdlib.h>
#include <stdio.h>

#define PDC700_INIT	0x01
#define PDC700_INFO	0x02

#define PDC700_BAUD	0x04
#define PDC700_PICINFO	0x05
#define PDC700_THUMB	0x06
#define PDC700_PIC	0x07


#define PDC700_FIRST	0x00
#define PDC700_DONE	0x01
#define PDC700_LAST	0x02

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_RESULT_FREE(result, data) {int r = (result); if (r < 0) {free (data); return (r);}}

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
pdc700_read (CameraPort *port, unsigned char *cmd, int cmd_len,
	     unsigned char *buf, int *buf_len, int *status)
{
	unsigned char header[3], c;
	int checksum;

	/* Finish the command and send it */
	cmd[0] = 0x40;
	cmd[1] = (cmd_len - 3) >> 8;
	cmd[2] = (cmd_len - 3) & 0xff;
	cmd[cmd_len - 1] = calc_checksum (cmd + 3, cmd_len - 1 - 3);
	CHECK_RESULT (gp_port_write (port, cmd, cmd_len));

	/*
	 * Read the header (0x40 plus 2 bytes indicating how many bytes
	 * will follow besides checksum, command confirmation and last packet
	 * identifier)
	 */
	CHECK_RESULT (gp_port_read (port, header, 3));
	if (header[0] != 0x40)
		return (GP_ERROR_CORRUPTED_DATA);
	*buf_len = (header[2] << 8) | header [1];

	/* Read one byte and check if the response is for our command */
	CHECK_RESULT (gp_port_read (port, &c, 1));
	if (c != (0x80 | cmd[3]))
		return (GP_ERROR_CORRUPTED_DATA);

	/*
	 * Read another byte which indicates if other packets will follow 
	 * (in case of picture transmission)
	 */
	CHECK_RESULT (gp_port_read (port, &c, 1));
	*status = c;

	/* Read the actual data */
	if (*buf_len)
		CHECK_RESULT (gp_port_read (port, buf, *buf_len));

	/* Read the checksum */
	CHECK_RESULT (gp_port_read (port, &c, 1));
	checksum = calc_checksum (buf, *buf_len - 1);
	if (checksum != c)
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc700_baud (CameraPort *port, int baud)
{
	unsigned char b;
	unsigned char cmd[6];
	unsigned char buf[2048];
	int status, buf_len;

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
	CHECK_RESULT (pdc700_read (port, cmd, 6, buf, &buf_len, &status));
	if (status != PDC700_DONE)
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc700_init (CameraPort *port)
{
	int status, buf_len;
	unsigned char cmd[5];
	unsigned char buf[2048];

	cmd[3] = PDC700_INIT;
	CHECK_RESULT (pdc700_read (port, cmd, 5, buf, &buf_len, &status));
	if (status != PDC700_DONE)
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc700_picinfo (CameraPort *port, int n, int *size_thumb, int *size_pic)
{
	int status, buf_len;
	unsigned char cmd[7];
	unsigned char buf[2048];

	cmd[3] = PDC700_PICINFO;
	cmd[4] = n && 0xff;
	cmd[5] = n >> 8;
	CHECK_RESULT (pdc700_read (port, cmd, 7, buf, &buf_len, &status));
	if (status != PDC700_DONE)
		return (GP_ERROR_CORRUPTED_DATA);

	*size_pic = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
	*size_thumb = buf[18] | (buf[19] <<  8) | (buf[20] << 16) |
						  (buf[21] << 24);

	return (GP_OK);
}

static int
pdc700_num (CameraPort *port, int *num, int *num_free)
{
	int status, buf_len;
	unsigned char buf[2048];
	unsigned char cmd[5];

	cmd[3] = PDC700_INFO;
	CHECK_RESULT (pdc700_read (port, cmd, 5, buf, &buf_len, &status));
	if (status != PDC700_DONE)
		return (GP_ERROR_CORRUPTED_DATA);

	if (num)
		*num = buf[16] | (buf[17] << 8);
	if (num_free)
		*num_free = buf[18] | (buf[19] << 8);

	return (GP_OK);
}

static int
pdc700_pic (CameraPort *port, int n, unsigned char **data, int *size,
	    int thumb)
{
	unsigned char cmd[8];
	unsigned char buf[2048];
	int len, status;
	int size_thumb, size_pic, i;
	unsigned char sequence_num;

	/* Picture size? Allocate the memory */
	CHECK_RESULT (pdc700_picinfo (port, n, &size_thumb, &size_pic));
	*data = malloc (sizeof (char) * ((thumb) ? size_thumb : size_pic));
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	/* Get first packet and copy the data (without sequence number) */
	cmd[3] = (thumb) ? PDC700_THUMB : PDC700_PIC;
	cmd[4] = PDC700_FIRST;
	cmd[5] = n & 0xff;
	cmd[6] = n >> 8;
	CHECK_RESULT_FREE (pdc700_read (port, cmd, 8,
					buf, &len, &status), *data);
	sequence_num = buf[0];
	memcpy (*data, buf + 1, len - 1);

	/* Get the following packets */
	i = len - 1;
	while (status != PDC700_LAST) {
		cmd[4] = PDC700_DONE;
		cmd[5] = sequence_num;
		CHECK_RESULT_FREE (pdc700_read (port, cmd, 7,
						buf, &len, &status), *data);
		sequence_num = buf[0];
		memcpy (*data + i, buf + 1, len - 1);
		i += (len - 1);
	}

	/* Terminate transfer */
	cmd[4] = PDC700_LAST;
	cmd[5] = sequence_num;
	CHECK_RESULT_FREE (pdc700_read (port, cmd, 7, buf, &len, &status),
			   *data);

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
	CameraAbilities *a;

	for (i = 0; models[i].model; i++) {
		CHECK_RESULT (gp_abilities_new (&a));

		strcpy (a->model, models[i].model);
		a->status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a->port     = GP_PORT_SERIAL;
		a->speed[0] = 9600;
		a->speed[1] = 19200;
		a->speed[2] = 38400;
		a->speed[3] = 57600;
		a->operations        = GP_OPERATION_NONE;
		a->file_operations   = GP_FILE_OPERATION_NONE;
		a->folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

		CHECK_RESULT (gp_abilities_list_append (list, a));
	}
	
	return (GP_OK);
}

static int
camera_file_get (Camera *camera, const char *folder, const char *filename, 
		 CameraFileType type, CameraFile *file)
{
	int n, size;
	unsigned char *data;

	if (type == GP_FILE_TYPE_RAW)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Get the number of the picture from the filesystem */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, filename));

	/* Get the file */
	CHECK_RESULT (pdc700_pic (camera->port, n, &data, &size, 
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
	int num, num_free;

	CHECK_RESULT (pdc700_num (camera->port, &num, &num_free));

	sprintf (about->text, "There are %i pictures on the camera. "
		 "There is place for another %i one(s).", num, num_free);

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data)
{
	int num;
	Camera *camera = data;

	/* Fill the list */
	CHECK_RESULT (pdc700_num (camera->port, &num, NULL));
	gp_list_populate (list, "PDC700%04i.jpg", num);

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data)
{
	int n, size_thumb, size_pic;
	Camera *camera = data;

	/* Get the picture number from the CameraFilesystem */
	CHECK_RESULT (n = gp_filesystem_number (fs, folder, file));

	CHECK_RESULT (pdc700_picinfo (camera->port, n, &size_thumb, &size_pic));
	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	strcpy (info->file.type, GP_MIME_JPEG);
	strcpy (info->preview.type, GP_MIME_JPEG);
	info->file.size = size_pic;
	info->preview.size = size_thumb;

	return (GP_OK);
}

int
camera_init (Camera *camera) 
{
	int result = GP_OK, i;
	gp_port_settings settings;
	int speeds[] = {9600, 57600, 19200, 38400};

        /* First, set up all the function pointers */
        camera->functions->file_get     = camera_file_get;
	camera->functions->summary	= camera_summary;
        camera->functions->about        = camera_about;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);

	/* Open the port and check if the camera is there */
	CHECK_RESULT (gp_port_settings_get (camera->port, &settings));
	strcpy (settings.serial.port, camera->port_info->path);
	CHECK_RESULT (gp_port_settings_set (camera->port, settings));
	CHECK_RESULT (gp_port_timeout_set (camera->port, 1000));

	/* Check if the camera is really there */
	CHECK_RESULT (gp_port_open (camera->port));
	for (i = 0; i < 4; i++) {
		settings.serial.speed = speeds[i];
		CHECK_RESULT (gp_port_settings_set (camera->port, settings));
		result = pdc700_init (camera->port);
		if (result == GP_OK)
			break;
	}
	if (i == 4)
		return (result);

	/* Set the speed to the highest one */
	if (speeds[i] != 57600)
		CHECK_RESULT (pdc700_baud (camera->port, 57600));

	return (GP_OK);
}
