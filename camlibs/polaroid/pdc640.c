/* pdc640.c
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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

#include <gphoto2-library.h>
#include <gphoto2-core.h>
#include <stdlib.h>
#include <string.h>

#define PDC640_PING  "\x01"
#define PDC640_SPEED "\x69\x0b"

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

static struct {
	const char *model;
} models[] = {
	{"Polaroid Fun Flash 640"},
	{NULL}
};

static int
pdc640_read_packet (CameraPort *port, char *buf, int buf_size)
{
	int i;
	char checksum, c;

	/* Read the packet */
	CHECK_RESULT (gp_port_read (port, buf, buf_size));

	/* Calculate the checksum */
	for (i = 0, checksum = 0; i < buf_size; i++)
		checksum += buf[i];

	/* Read the checksum */
	CHECK_RESULT (gp_port_read (port, &c, 1));
	if (checksum != c)
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc640_transmit (CameraPort *port, char *cmd, int cmd_size,
				   char *buf, int buf_size)
{
	char c;

	/*
	 * The first byte returned is always the same as the first byte
	 * of the command.
	 */
	CHECK_RESULT (gp_port_write (port, cmd, cmd_size));
	CHECK_RESULT (gp_port_read (port, &c, 1));
	if (c != cmd[0])
		return (GP_ERROR_CORRUPTED_DATA);

	if (buf)
		CHECK_RESULT (pdc640_read_packet (port, buf, buf_size));

	return (GP_OK);
}

static int
pdc640_transmit_pic (CameraPort *port, char cmd, int thumbnail,
		     char *buf, int buf_size)
{
	char cmd1[] = {0x61, cmd};
	char cmd2[] = {0x15, 0x00, 0x00, 0x00, 0x00};
	int i, packet_size;

	/* First send the command ... */
	CHECK_RESULT (pdc640_transmit (port, cmd1, 2, NULL, 0));

	if (thumbnail) {
		packet_size = 80;
		cmd2[4] = 0x01;
	} else {
		packet_size = 3840;
		cmd2[4] = 0x06;
	}

	/* ... then get the packets */
	for (i = 0; i < buf_size; i += packet_size) {

		/* Read the packet */
		CHECK_RESULT (pdc640_transmit (port, cmd2, 5,
					       buf + i, packet_size));
		cmd2[2] += cmd1[4];
	}

	return (GP_OK);
}

static int
pdc640_transmit_packet (CameraPort *port, char cmd, char *buf, int buf_size)
{
	char cmd1[] = {0x61, cmd};
	char cmd2[] = {0x15, 0x00, 0x00, 0x00, 0x01};

	/* Send the command and get the packet */
	CHECK_RESULT (pdc640_transmit (port, cmd1, 2, NULL, 0));
	CHECK_RESULT (pdc640_transmit (port, cmd2, 5, buf, buf_size));

	return (GP_OK);
}

static int
pdc640_ping_low (CameraPort *port)
{
	char cmd[] = {0x01};

	CHECK_RESULT (pdc640_transmit (port, cmd, 1, NULL, 0));

	return (GP_OK);
}

static int
pdc640_ping_high (CameraPort *port)
{
	char cmd[] = {0x41};

	CHECK_RESULT (pdc640_transmit (port, cmd, 1, NULL, 0));

	return (GP_OK);
}

static int
pdc640_speed (CameraPort *port, int speed)
{
	char cmd[] = {0x69, 0x00};

	cmd[1] = (speed / 9600) - 1;
	CHECK_RESULT (pdc640_transmit (port, cmd, 2, NULL, 0));

	return (GP_OK);
}

static int
pdc640_unknown5 (CameraPort *port)
{
	char cmd[] = {0x05};
	char buf[3];

	CHECK_RESULT (pdc640_transmit (port, cmd, 1, buf, 3));
	if ((buf[0] != 0x33) || (buf[1] != 0x02) || (buf[2] != 0x35))
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc640_unknown20 (CameraPort* port)
{
	char buf[128];

	CHECK_RESULT (pdc640_transmit_packet (port, 0x20, buf, 128));

	return (GP_OK);
}

static int
pdc640_caminfo (CameraPort *port, int *numpic)
{
	char buf[1280];

	CHECK_RESULT (pdc640_transmit_packet (port, 0x40, buf, 1280));
	*numpic = buf[3];

	return (GP_OK);
}

static int
pdc640_setpic (CameraPort *port, char n)
{
	char cmd[2] = {0xf6, n};
	char buf[8];

	CHECK_RESULT (pdc640_transmit (port, cmd, 2, buf, 8));

	return (GP_OK);
}

static int
pdc640_picinfo (CameraPort *port, char n,
		int *size_pic,   int *width_pic,   int *height_pic,
		int *size_thumb, int *width_thumb, int *height_thumb)
{
	char buf[32];

	CHECK_RESULT (pdc640_setpic (port, n));
	CHECK_RESULT (pdc640_transmit_packet (port, 0x80, buf, 32));

	/* Image number */
	if (buf[0] != n)
		return (GP_ERROR_CORRUPTED_DATA);

	/* Picture size, width and height */
	*size_pic   = buf[2] | (buf[3] << 8) | (buf[4] << 16);
	*width_pic  = buf[5] | (buf[6] << 8);
	*height_pic = buf[7] | (buf[8] << 8);

	/* Thumbnail size, width and height */
	*size_thumb   = buf[25] | (buf[26] << 8) | (buf[27] << 16);
	*width_thumb  = buf[28] | (buf[29] << 8) | (buf[30] << 16);

	return (GP_OK);
}

static int
pdc640_getpic (CameraPort *port, int n, int thumbnail, char **data, int *size)
{
	char cmd;
	int size_pic, size_thumb;
	int width_pic, width_thumb, height_pic, height_thumb; 

	/* Get the size of the picture */
	CHECK_RESULT (pdc640_picinfo (port, n,
				&size_pic,   &width_pic,   &height_pic,
				&size_thumb, &width_thumb, &height_thumb));

	/* Allocate the memory */
	if (thumbnail) {
		*size = 4720;
		cmd = 0x03;
	} else {
		*size = 138240;
		cmd = 0x10;
	}
	*data = malloc (*size * sizeof (char));
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	/* Get the picture */
	CHECK_RESULT (pdc640_setpic (port, n));
	CHECK_RESULT (pdc640_transmit_pic (port, cmd, thumbnail, *data, *size));

	return (GP_OK);
}

static int
pdc640_delpic (CameraPort *port)
{
	char cmd[2] = {0x59, 0x01};

	CHECK_RESULT (pdc640_transmit (port, cmd, 2, NULL, 0));

	return (GP_OK);
}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "Polaroid Fun Flash 640");

	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities *a;

	for (i = 0; models[i].model; i++) {
		CHECK_RESULT (gp_abilities_new (&a));

		strcpy (a->model, models[i].model);
		a->status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a->port = GP_PORT_SERIAL;
		a->speed[0] = 0;
		a->operations        = GP_OPERATION_NONE;
		a->file_operations   = GP_FILE_OPERATION_DELETE;
		a->folder_operations = GP_FOLDER_OPERATION_NONE;

		CHECK_RESULT (gp_abilities_list_append (list, a));
	}

	return (GP_OK);
}

static int
camera_file_get (Camera *camera, const char *folder, const char *filename,
		 CameraFileType type, CameraFile *file)
{
	int n, size;
	char *data;

	if (type == GP_FILE_TYPE_NORMAL)
		return (GP_ERROR_NOT_SUPPORTED);

	/*
	 * Get the number of the picture from the filesystem and increment
	 * since we need a range starting with 1.
	 */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, filename));
	n++;

	/* Get the picture */
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		return (GP_ERROR_NOT_SUPPORTED);
	case GP_FILE_TYPE_RAW:
		CHECK_RESULT (pdc640_getpic (camera->port, n, 0, &data, &size));
		break;
	case GP_FILE_TYPE_PREVIEW:
		CHECK_RESULT (pdc640_getpic (camera->port, n, 1, &data, &size));
		break;
	}

	CHECK_RESULT (gp_file_set_data_and_size (file, data, size));
	CHECK_RESULT (gp_file_set_name (file, filename));
	CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_RAW));

	return (GP_OK);
}

static int
camera_file_delete (Camera *camera, const char *folder, const char *file)
{
	int n, count;

	/* We can only delete the last picture. (Is this really the case?) */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, file));
	n++;

	CHECK_RESULT (pdc640_caminfo (camera->port, &count));
	if (count != n)
		return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT (pdc640_delpic (camera->port));

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about)
{
	strcpy (about->text, "Download program for Polaroid Fun Flash 640. "
		"Originally written by Chris Byrne <adapt@ihug.co.nz>, "
		"and adapted for gphoto2 by Lutz Müller "
		"<urc8@rz.uni-karlsruhe.de>.");

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data)
{
	int n;
	Camera *camera = data;

	/* Fill the list */
	CHECK_RESULT (pdc640_caminfo (camera->port, &n));
	CHECK_RESULT (gp_list_populate (list, "PDC640%04i.raw", n));

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data)
{
	Camera *camera = data;
	int n;
	int size_pic, size_thumb;
	int width_pic, width_thumb, height_pic, height_thumb;

	CHECK_RESULT (n = gp_filesystem_number (fs, folder, file));
	n++;

	CHECK_RESULT (pdc640_picinfo (camera->port, n,
				&size_pic,   &width_pic,   &height_pic,
				&size_thumb, &width_thumb, &height_thumb));

	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_WIDTH |
			    GP_FILE_INFO_HEIGHT | GP_FILE_INFO_TYPE;
	info->file.width  = width_pic;
	info->file.height = height_pic;
	info->file.size   = size_pic;
	strcpy (info->file.type, GP_MIME_RAW);

	info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_WIDTH |
			       GP_FILE_INFO_HEIGHT | GP_FILE_INFO_TYPE;
	info->preview.width  = width_thumb;
	info->preview.height = height_thumb;
	info->preview.size   = size_thumb;
	strcpy (info->preview.type, GP_MIME_RAW);

	return (GP_OK);
}

int
camera_init (Camera *camera)
{
	int result;
	gp_port_settings settings;

	camera->functions->file_get   = camera_file_get;
	camera->functions->file_delete = camera_file_delete;
	camera->functions->about      = camera_about;

	/* Tell the filesystem where to get lists and info */
	CHECK_RESULT (gp_filesystem_set_list_funcs (camera->fs, file_list_func,
						    NULL, camera));
	CHECK_RESULT (gp_filesystem_set_info_funcs (camera->fs, get_info_func,
						    NULL, camera));

	/* Open the port */
	CHECK_RESULT (gp_port_settings_get (camera->port, &settings));
	strcpy (settings.serial.port, camera->port_info->path);
	settings.serial.speed = 9600;
	CHECK_RESULT (gp_port_settings_set (camera->port, settings));
	CHECK_RESULT (gp_port_timeout_set (camera->port, 5000));

	/* Is the camera at 9600? */
	result = pdc640_ping_low (camera->port);
	if (result == GP_OK)
		CHECK_RESULT (pdc640_speed (camera->port, 115200));

	/* Is the camera at 115200? */
	CHECK_RESULT (pdc640_ping_high (camera->port));

	return (GP_OK);
}
