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

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_RESULT_FREE(result, data) {int r = (result); if (r < 0) {free (data); return (r);}}

static int
calc_checksum (unsigned char *cmd, int len)
{
	int loop;
	unsigned char checksum;

	for (checksum = 0, loop = 3; loop < len; loop++)
		checksum += cmd[loop];

	return (checksum);
}

static int
pdc700_read (CameraPort *port, unsigned char *buf, int *len)
{
	unsigned char header[3], c;
	int checksum;

	/* Read the header */
	CHECK_RESULT (gp_port_read (port, header, 3));
	if (header[0] != 0x40)
		return (GP_ERROR_CORRUPTED_DATA);
	*len = (header[2] << 8) | header [1];
	*len += 2;

	/* Read the actual data */
	CHECK_RESULT (gp_port_read (port, buf, *len));

	/* Read the checksum */
	CHECK_RESULT (gp_port_read (port, &c, 1));
	checksum = calc_checksum (buf, *len);
	if (checksum != c)
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc700_baud (CameraPort *port, int baud)
{
	unsigned char b;
	unsigned char cmd[6];
	unsigned char buf[2];
	int len;

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

	cmd[0] = 0x40;
	cmd[1] = (6 - 3) >> 8;
	cmd[2] = (6 - 3) & 0xff;
	cmd[3] = 0x04;				/* Change baud rate */
	cmd[4] = b;
	cmd[5] = calc_checksum (cmd, 5);
	CHECK_RESULT (gp_port_write (port, cmd, 6));
	CHECK_RESULT (pdc700_read (port, buf, &len));

	if ((buf[0] != 0x84) ||
	    (buf[1] != 0x01))
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc700_init (CameraPort *port)
{
	int len;
	unsigned char cmd[5];
	unsigned char buf[2];

	cmd[0] = 0x40;
	cmd[1] = (5 - 3) >> 8;
	cmd[2] = (5 - 3) & 0xff;
	cmd[3] = 0x01;			/* Init */
	cmd[4] = calc_checksum (cmd, 4);
	CHECK_RESULT (gp_port_write (port, cmd, sizeof (cmd)));
	CHECK_RESULT (pdc700_read (port, buf, &len));

	if ((buf[0] != 0x81) ||
	    (buf[1] != 0x01))
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc700_picinfo (CameraPort *port, int n, int *size_thumb, int *size_pic)
{
	int len;
	unsigned char cmd[7];
	unsigned char buf[24];

	cmd[0] = 0x40;
	cmd[1] = (7 - 3) >> 8;			/* Size of command */
	cmd[2] = (7 - 3) & 0xff;
	cmd[3] = 0x05;				/* Picture info */
	cmd[4] = n && 0xff;			/* Picture number */
	cmd[5] = n >> 8;
	cmd[6] = calc_checksum (cmd, 6);	/* Checksum */

	CHECK_RESULT (gp_port_write (port, cmd, sizeof (cmd)));
	CHECK_RESULT (pdc700_read (port, buf, &len));

	if ((buf[0] != 0x85))
		return (GP_ERROR_CORRUPTED_DATA);

	*size_pic = buf[6] | (buf[7] << 8) | (buf[8] << 16) | (buf[9] << 24);
	*size_thumb = buf[20] | (buf[21] <<  8) | (buf[22] << 16) |
						  (buf[23] << 24);

	return (GP_OK);
}

static int
pdc700_num (CameraPort *port, int *num, int *num_free)
{
	int len;
	unsigned char buf[22];
	unsigned char cmd[5];

	cmd[0] = 0x40;
	cmd[1] = (5 - 3) >> 8;
	cmd[2] = (5 - 3) & 0xff;
	cmd[3] = 0x02;				/* Get camera info */
	cmd[4] = calc_checksum (cmd, 4);
	CHECK_RESULT (gp_port_write (port, cmd, sizeof (cmd)));
	CHECK_RESULT (pdc700_read (port, buf, &len));

	if ((buf[0] != 0x82) ||
	    (buf[1] != 0x01))
		return (GP_ERROR_CORRUPTED_DATA);

	if (num)
		*num = buf[18] | (buf[19] << 8);
	if (num_free)
		*num_free = buf[20] | (buf[21] << 8);

	return (GP_OK);
}

static int
pdc700_pic (CameraPort *port, int n, unsigned char **data, int *size,
	    int thumb)
{
	unsigned char cmd[8];
	unsigned char buf[2048];
	int len;
	int size_thumb, size_pic, done, i;
	unsigned char sequence_num;

	cmd[0] = 0x40;
	cmd[1] = (8 - 3) >> 8;
	cmd[2] = (8 - 3) & 0xff;
	cmd[3] = (thumb) ? 0x06 : 0x07;		/* Preview / Picture */
	cmd[4] = 0x00;
	cmd[5] = n & 0xff;
	cmd[6] = n >> 8;
	cmd[7] = calc_checksum (cmd, 7);
	CHECK_RESULT (gp_port_write (port, cmd, 8));

	/* Picture size? */
	CHECK_RESULT (pdc700_picinfo (port, n, &size_thumb, &size_pic));

	/* Allocate the memory */
	*data = malloc (sizeof (char) * ((thumb) ? size_thumb : size_pic));
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	/* Get picture data */
	done = 0;
	for (i = 0; !done;) {
		CHECK_RESULT_FREE (pdc700_read (port, buf, &len), *data);
		if (buf[0] != (0x80 | ((thumb) ? 0x06 : 0x07))) {
			free (*data);
			return (GP_ERROR_CORRUPTED_DATA);
		}

		/* Is this the last packet? */
		if (buf[1] == 0x02)
			done = 1;
		sequence_num = buf[2];
		memcpy (*data + i, &buf[3], len - 4);
		i += (len - 4);

		/* Request the next packet */
		cmd[0] = 0x40;
		cmd[1] = (7 - 3) >> 8;
		cmd[2] = (7 - 3) & 0xff;
		cmd[3] = (thumb) ? 0x06 : 0x07;		/* Preview / Picture */
		cmd[4] = (done) ? 0x02 : 0x01;		/* Stop / Cont. */
		cmd[5] = sequence_num;
		cmd[6] = calc_checksum (cmd, 6);
		CHECK_RESULT_FREE (gp_port_write (port, cmd, 7), *data);
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
