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
#include <gphoto2-debug.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libgphoto2/bayer.h"

#define PDC640_PING  "\x01"
#define PDC640_SPEED "\x69\x0b"

#define PDC640_FILESPEC "pdc640%04i.ppm"

#define PDC640_MAXTRIES 3

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

	/* Calculate the checksum */
	for (i = 0, checksum = 0; i < buf_size; i++)
		buf[i] = 0;

	/* Read the packet */
	CHECK_RESULT (gp_port_read (port, buf, buf_size));

	/* Calculate the checksum */
	for (i = 0, checksum = 0; i < buf_size; i++)
		checksum += buf[i];

	/* Read the checksum */
	CHECK_RESULT (gp_port_read (port, &c, 1));

	gp_debug_printf(GP_DEBUG_LOW, "pdc640", 
			"*** (%d = %d)", checksum, c);

	if (checksum != c)
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc640_transmit (CameraPort *port, char *cmd, int cmd_size,
				   char *buf, int buf_size)
{
	char c;
	int tries, r;

	/* In event of a checksum or timing failure, retry */
	for (tries = 0; tries < PDC640_MAXTRIES; tries++) {
		/*
		 * The first byte returned is always the same as the first byte
		 * of the command.
		 */
		CHECK_RESULT (gp_port_write (port, cmd, cmd_size));
		r = gp_port_read (port, &c, 1);
		if ((r < 0) || (c != cmd[0]))
			continue;

		if (buf) {
			r = pdc640_read_packet (port, buf, buf_size);
			if (r < 0)
				continue;
		}

		return (GP_OK);
	}

	return (GP_ERROR_CORRUPTED_DATA);
}

static int
pdc640_transmit_pic (CameraPort *port, char cmd, int width, int thumbnail,
		     char *buf, int buf_size)
{
	char cmd1[] = {0x61, cmd};
	char cmd2[] = {0x15, 0x00, 0x00, 0x00, 0x00};
	int i, packet_size, result, size, ofs;
	char *data;

	/* First send the command ... */
	CHECK_RESULT (pdc640_transmit (port, cmd1, 2, NULL, 0));

/*
	if (thumbnail) {
		cmd2[4] = 0x01;
	} else {
		cmd2[4] = 0x06;
	}
*/

	/* Set how many scanlines worth of data to get at once */
	cmd2[4] = 0x06;

	/* Packet size is a multiple of the image width */
	packet_size = width * cmd2[4];

	/* Allocate memory to temporarily store received data */
	data = malloc (packet_size);
	if (!data)
		return (GP_ERROR_NO_MEMORY);

	/* Now get the packets */
	ofs = 0;
	result = GP_OK;
	for (i = 0; i < buf_size; i += packet_size) {
		/* Read the packet */
		result = pdc640_transmit (port, cmd2, 5,
					data, packet_size);
		if (result < 0)
			break;

		/* Copy from temp buffer -> actual */
		size = packet_size;
		if (size > buf_size - i)
			size = buf_size - i;
		memcpy(buf + i, data, size);

		/* Move to next offset */
		ofs += cmd2[4];
		cmd2[2] = ofs & 0xFF;
		cmd2[1] = (ofs >> 8) & 0xFF;
	}

	free (data);

	return (result);
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

#if 0
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
#endif

#if 0
static int
pdc640_unknown20 (CameraPort* port)
{
	char buf[128];

	CHECK_RESULT (pdc640_transmit_packet (port, 0x20, buf, 128));

	return (GP_OK);
}
#endif

static int
pdc640_caminfo (CameraPort *port, int *numpic)
{
	char buf[1280];

	CHECK_RESULT (pdc640_transmit_packet (port, 0x40, buf, 1280));
	*numpic = buf[2];

	return (GP_OK);
}

static int
pdc640_setpic (CameraPort *port, char n)
{
	char cmd[2] = {0xf6, n};
	char buf[8];

	CHECK_RESULT (pdc640_transmit (port, cmd, 2, buf, 7));

	return (GP_OK);
}

static int
pdc640_picinfo (CameraPort *port, char n,
		int *size_pic,   int *width_pic,   int *height_pic,
		int *size_thumb, int *width_thumb, int *height_thumb)
{
	unsigned char buf[32];

	CHECK_RESULT (pdc640_setpic (port, n));
	CHECK_RESULT (pdc640_transmit_packet (port, 0x80, buf, 32));

	/* Check image number matches */
	if (buf[0] != n)
		return (GP_ERROR_CORRUPTED_DATA);

	/* Picture size, width and height */
	*size_pic   = buf[2] | (buf[3] << 8) | (buf[4] << 16);
	*width_pic  = buf[5] | (buf[6] << 8);
	*height_pic = buf[7] | (buf[8] << 8);

	/* Thumbnail size, width and height */
	*size_thumb   = buf[25] | (buf[26] << 8) | (buf[27] << 16);
	*width_thumb  = buf[28] | (buf[29] << 8);
	*height_thumb  = buf[30] | (buf[31] << 8);

	return (GP_OK);
}

static int
pdc640_processtn (int width, int height, char **data, int size) {
	char *newdata;
	int y;

	/* Sanity checks */
	if ((data == NULL) || (size < width * height))
		return (GP_ERROR_CORRUPTED_DATA);

	/* Allocate a new buffer */
	newdata = malloc(size);
	if (!newdata)
		return (GP_ERROR_NO_MEMORY);

	/* Flip the thumbnail */
	for (y = 0; y < height; y++) {
		memcpy(&newdata[(height - y - 1) * width],
			&((*data)[y * width]), width);
	}

	/* Set new buffer */
	free (*data);
	*data = newdata;

	return (GP_OK);
}

static int
pdc640_getbit (char *data, int *ofs, int size, int *bit) {
	static char c;
	int b;

	/* Check if next byte required */
	if (*bit == 0) {
		if (*ofs >= size)
			return (-1);

		c = data[*ofs];
		(*ofs)++;
	}

	/* Get current bit value */
	b = (c >> *bit) & 1;

	/* Then move onto the next bit */
	(*bit)++;
	if (*bit >= 8)
		*bit = 0;

	return (b);
}

static int
pdc640_deltadecode (int width, int height, char **rawdata, int *rawsize)
{
	char col1, col2;
	char *data;
	int rawofs, x, y, ofs, bit, ones;
	int size;
	int e, d, o, val;

	gp_debug_printf(GP_DEBUG_LOW, "pdc640", "*** deltadecode");

	/* Create a buffer to store RGB data in */
	size = width * height;
	data = malloc (size * sizeof (char));
	if (!data)
		return (GP_ERROR_NO_MEMORY);

	/* Delta decode scanline by scanline */
	rawofs = 0;
	for (y = height-1; y >= 0; y--) {
		/* Word alignment */
		if (rawofs & 1)
			rawofs++;

		/* Sanity check */
		if (rawofs >= *rawsize) {
			free (data);
			return (GP_ERROR_CORRUPTED_DATA);
		}

		/* Offset into the uncompressed data */
		ofs = y * width;

		/* Get the first two pixel values */
		col1 = (*rawdata)[rawofs];
		col2 = (*rawdata)[rawofs + 1];
		rawofs += 2;
		data[ofs + 0] = col1 << 1;
		data[ofs + 1] = col2 << 1;

		/* Work out the remaining pixels */
		bit = 0;
		for (x = 2; x < width; x++) {
			/* Count number of ones */
			ones = 0;
			while (pdc640_getbit(*rawdata, &rawofs, *rawsize, &bit) == 1)
				ones++;

			/* 
			 * Get the delta value
			 * (size dictated by number of ones)
			 */
			val = 0;
			for (o = 0, e = 0, d = 1; o < ones; o++, d <<= 1) {
				e = pdc640_getbit(*rawdata, &rawofs, *rawsize, &bit);
				if (e == 1) val += d;
			}
			if (e == 0) val += 1 - d; /* adjust for negatives */

			/* Adjust the corresponding pixel value */
			if (x & 1)
				val = (col2 += val);
			else
				val = (col1 += val);

			data[ofs + x] = val << 1;
		}		
	}

	/* Set new buffer */
	free (*rawdata);
	*rawdata = data;
	*rawsize = size;

	return (GP_OK);
}

static int
pdc640_getpic (CameraPort *port, int n, int thumbnail, int justraw,
		char **data, int *size)
{
	char cmd, ppmheader[100];
	int size_pic, width_pic, height_pic;
	int size_thumb, width_thumb, height_thumb;
	int height, width, outsize, result;
	char *outdata;

	/* Get the size of the picture */
	CHECK_RESULT (pdc640_picinfo (port, n,
				&size_pic,   &width_pic,   &height_pic,
				&size_thumb, &width_thumb, &height_thumb));

	/* Evaluate parameters */
	if (thumbnail) {
		gp_debug_printf(GP_DEBUG_LOW, "pdc640", 
				"*** Size: %d Width: %d Height: %d", 
				size_thumb, width_thumb, height_thumb);

		*size = size_thumb;
		width = width_thumb;
		height = height_thumb;
		cmd = 0x03;
	} else {
		gp_debug_printf(GP_DEBUG_LOW, "pdc640", 
				"*** Size: %d Width: %d Height: %d", 
				size_pic, width_pic, height_pic);

		*size = size_pic;
		width = width_pic;
		height = height_pic;
		cmd = 0x10;
	}

	/* Sanity check */
	if ((*size <= 0) || (width <= 0) || (height <= 0))
		return (GP_ERROR_CORRUPTED_DATA);

	/* Allocate the memory */
	*data = malloc (*size * sizeof (char));
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	/* Get the raw picture */
	CHECK_RESULT (pdc640_setpic (port, n));
	CHECK_RESULT (pdc640_transmit_pic (port, cmd, width, thumbnail, 
					*data, *size));

	/* Just wanted the raw camera data */
	if (justraw)
		return(GP_OK);

	if (thumbnail) {
		/* Process thumbnail data */
		CHECK_RESULT (pdc640_processtn (width, height,
						data, *size));
	} else {
		/* Image data is delta encoded so decode it */
		CHECK_RESULT (pdc640_deltadecode (width, height, 
						  data, size));
	}

	gp_debug_printf(GP_DEBUG_LOW, "pdc640", "*** Bayer decode");

	sprintf(ppmheader, "P6\n"
			   "# CREATOR: gphoto2, pdc640 library\n"
			   "%d %d\n"
			   "255\n", width, height);

	/* Allocate memory for Interpolated ppm image */
	result = strlen(ppmheader);
	outsize = width * height * 3 + result + 1;
	outdata = malloc(outsize);
	if (!outdata)
		return (GP_ERROR_NO_MEMORY);

	/* Set header */
	strcpy(outdata, ppmheader);

	/* Decode and interpolate the Bayer Mask */
	result = gp_bayer_decode(*data, width, height,
				&outdata[result], BAYER_TILE_RGGB);
	if (result < 0) {
		free (outdata);
		return (result);
	}

	/* Fix up data pointers */
	free (*data);
	*data = outdata;
	*size = outsize;

	return (GP_OK);
}

static int
pdc640_delpic (CameraPort *port)
{
	char cmd[2] = {0x59, 0x01};

	CHECK_RESULT (pdc640_transmit (port, cmd, 2, NULL, 0));

	return (GP_OK);
}

static int
pdc640_takepic (CameraPort *port)
{
	char cmd[2] = {0x2D, 0x00};

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
	char *data, *p;

	/*
	 * Get the number of the picture from the filesystem and increment
	 * since we need a range starting with 1.
	 */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, filename));
	n++;

	CHECK_RESULT (gp_file_set_name (file, filename));

	/* Get the picture */
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		CHECK_RESULT (pdc640_getpic (camera->port, n, 0, 0, &data, &size));
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_PPM));
		break;
	case GP_FILE_TYPE_RAW:
		CHECK_RESULT (pdc640_getpic (camera->port, n, 0, 1, &data, &size));
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_RAW));

		/* Change extension to raw */
		n = strlen(filename);
		p = malloc (n + 1);
		if (p) {
			strcpy (p, filename);
			p[n-3] = 'r';
			p[n-2] = 'a';
			p[n-1] = 'w';
			CHECK_RESULT (gp_file_set_name (file, p));
			free (p);
		}
		break;
	case GP_FILE_TYPE_PREVIEW:
		CHECK_RESULT (pdc640_getpic (camera->port, n, 1, 0, &data, &size));
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_PPM));
		break;
	}

	CHECK_RESULT (gp_file_set_data_and_size (file, data, size));

	return (GP_OK);
}

static int
camera_folder_delete_all (Camera *camera, const char *folder)
{
	char cmd[2] = {0x59, 0x00};

	CHECK_RESULT (pdc640_transmit (camera->port, cmd, 2, NULL, 0));

	return (GP_OK);
}

static int
camera_file_delete (Camera *camera, const char *folder, const char *file)
{
	int n, count;

	/* We can only delete the last picture */
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
camera_capture (Camera *camera, int capture_type, CameraFilePath *path)
{
	int num, numpic;

	/* First get the current number of images */
	CHECK_RESULT (pdc640_caminfo (camera->port, &numpic));

	/* Take a picture */
	CHECK_RESULT (pdc640_takepic (camera->port));

	/* Wait a bit for the camera */
	sleep(4);

	/* Picture will be the last one in the camera */
	CHECK_RESULT (pdc640_caminfo (camera->port, &num));
	if (num <= numpic)
		return (GP_ERROR);

	/* Set the filename */
        sprintf (path->name, PDC640_FILESPEC, num);
        strcpy (path->folder, "/");

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
	CHECK_RESULT (gp_list_populate (list, PDC640_FILESPEC, n));

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
/*	
	info->file.size   = size_pic;
*/
	info->file.size   = width_pic * height_pic * 3;
	strcpy (info->file.type, GP_MIME_PPM);

	info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_WIDTH |
			       GP_FILE_INFO_HEIGHT | GP_FILE_INFO_TYPE;
	info->preview.width  = width_thumb;
	info->preview.height = height_thumb;
	info->preview.size   = size_thumb * 3;
	strcpy (info->preview.type, GP_MIME_PPM);

	return (GP_OK);
}

int
camera_init (Camera *camera)
{
	int result;
	gp_port_settings settings;

	camera->functions->file_get   = camera_file_get;
	camera->functions->file_delete = camera_file_delete;
	camera->functions->folder_delete_all = camera_folder_delete_all;
	camera->functions->about      = camera_about;
	camera->functions->capture      = camera_capture;

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

	/* Start with a low timeout (so we don't have to wait if already initialized) */
	CHECK_RESULT (gp_port_timeout_set (camera->port, 1000));

	/* Is the camera at 9600? */
	result = pdc640_ping_low (camera->port);
	if (result == GP_OK)
		CHECK_RESULT (pdc640_speed (camera->port, 115200));

	/* Switch to 115200 */
	settings.serial.speed = 115200;
	CHECK_RESULT (gp_port_settings_set (camera->port, settings));

	/* Is the camera at 115200? */
	result = pdc640_ping_high (camera->port);
	if (result != GP_OK)
		return(GP_ERROR_NO_CAMERA_FOUND);

	/* Switch to a higher timeout */
	CHECK_RESULT (gp_port_timeout_set (camera->port, 5000));

	return (GP_OK);
}
