/* pdc320.c
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
#include <gphoto2-library.h>
#include <gphoto2-core.h>
#include <stdlib.h>

#define PDC320_INIT   {0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0xf, 0xff, 0xff}
#define PDC320_IDENT    {0x6, 0x6, 0x6, 0x6, 0x1,       0xfe, 0xff}
#define PDC320_UNKNOWN1 {0x6, 0x6, 0x6, 0x6, 0x2,       0xfd, 0xff}
#define PDC320_NUM      {0x6, 0x6, 0x6, 0x6, 0x3,       0xfc, 0xff}
#define PDC320_SIZE     {0x6, 0x6, 0x6, 0x6, 0x4, 0x01, 0xfb, 0xfe}
#define PDC320_PIC      {0x6, 0x6, 0x6, 0x6, 0x5, 0x01, 0xfa, 0xfe}
#define PDC320_DELETE   {0x6, 0x6, 0x6, 0x6, 0x7,       0xf8, 0xff}
#define PDC320_UNKNOWN2 {0x6, 0x6, 0x6, 0x6, 0xa,       0xf5, 0xff}
#define PDC320_UNKNOWN3 {0x6, 0x6, 0x6, 0x6, 0xc,       0xf3, 0xf3}

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_RESULT_FREE(result, data) {int r = (result); if (r < 0) {free (data); return (r);}}

static int
pdc320_init (CameraPort *port)
{
	unsigned char cmd[] = PDC320_INIT;
	unsigned char buf[3];

	CHECK_RESULT (gp_port_write (port, cmd, sizeof (cmd)));
	CHECK_RESULT (gp_port_read (port, buf, 3));

	if ((buf[0] != 0x0f) ||
	    (buf[1] != 0xfa) ||
	    (buf[2] != 0xff))
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc320_num (CameraPort *port)
{
	int num;
	unsigned char cmd[] = PDC320_NUM;
	unsigned char buf[4];

	/* The first byte we get is the number of images on the camera */
	CHECK_RESULT (gp_port_write (port, cmd, sizeof (cmd)));
	CHECK_RESULT (gp_port_read (port, buf, 4));
	num = buf[0];

	if ((buf[1] != 0x01) ||
	    (buf[2] != 0xfc) ||
	    (buf[3] != 0xfe))
		return (GP_ERROR_CORRUPTED_DATA);

	gp_debug_printf (GP_DEBUG_LOW, "pdc320", "The camera contains %i "
			 "files.", num);

	return (num);
}

static int
pdc320_delete (CameraPort *port)
{
	unsigned char buf[7];
	unsigned char cmd[] = PDC320_DELETE;

	CHECK_RESULT (gp_port_write (port, cmd, sizeof (cmd)));
	CHECK_RESULT (gp_port_read (port, buf, 7));

	if ((buf[0] != 0x08) ||
	    (buf[1] != 0xf7) ||
	    (buf[2] != 0xff) ||
	    (buf[3] != 0x09) ||
	    (buf[4] != 0xf6) ||
	    (buf[5] != 0xff) ||
	    (buf[6] != 0x00))
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc320_size (CameraPort *port, int n)
{
	int size;
	unsigned char buf[7];
	unsigned char cmd[] = PDC320_SIZE;

	CHECK_RESULT (gp_port_write (port, cmd, sizeof (cmd)));
	CHECK_RESULT (gp_port_read (port, buf, 7));

	size = (buf[1] << 24) + (buf[2] << 16) + (buf[3] << 8) + buf[4];

	if ((buf[0] != 0x06))
		return (GP_ERROR_CORRUPTED_DATA);

	gp_debug_printf (GP_DEBUG_LOW, "pdc320", "Image %i has size %i.",
			 n, size);

	return (size);
}

static int
pdc320_pic (CameraPort *port, int n, unsigned char **data, int *size)
{
	unsigned char cmd[] = PDC320_PIC;
	unsigned char buf[2048];
	int remaining, f1, f2, i, len, checksum;

	/* Get the size of the picture and allocate the memory */
	CHECK_RESULT (*size = pdc320_size (port, n));
	*data = malloc (sizeof (char) * (*size));
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	cmd[5] += n;
	cmd[7] -= n;

	CHECK_RESULT_FREE (gp_port_write (port, cmd, sizeof (cmd)), *data);
	
	len = *size;
	for (i = 0; i < *size; i += 2000) {

		/* How many bytes do we read in this round? */
		remaining = *size - i;
		len = (remaining > 2000) ? 2000 : remaining;

		/* Read the frame number */
		usleep (10000);
		CHECK_RESULT_FREE (gp_port_read (port, buf, 5), *data);
		f1 = (buf[1] << 8) + buf[2];
		f2 = (buf[3] << 8) + buf[4];
		gp_debug_printf (GP_DEBUG_LOW, "pdc320", "Reading frame %d "
				 "(%d)...", f1, f2);

		/* Read the actual data */
		usleep(1000);
		CHECK_RESULT_FREE (gp_port_read (port, *data + i, len), *data);
		
		/* Read the checksum */
		CHECK_RESULT_FREE (gp_port_read (port, buf, 2), *data);
		checksum = (buf[0] << 8) + buf[1];
	}

	return (GP_OK);
}

int
camera_id (CameraText *id) 
{
	strcpy (id->text, "Polaroid DC320");

	return (GP_OK);
}

static struct {
	const char *model;
} models[] = {
	{"Polaroid DC320"},
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
		a->speed[0] = 0;
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

	if (type != GP_FILE_TYPE_RAW)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Get the number of the picture from the filesystem */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, filename));

	/* Get the file */
	CHECK_RESULT (pdc320_pic (camera->port, n, &data, &size));

	CHECK_RESULT (gp_file_set_data_and_size (file, data, size));
	CHECK_RESULT (gp_file_set_name (file, filename));
	CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_RAW));

	return (GP_OK);
}

static int
camera_folder_delete_all (Camera *camera, const char *folder)
{
	/* Delete and tell the filesyste */
	CHECK_RESULT (pdc320_delete (camera->port));
	CHECK_RESULT (gp_filesystem_format (camera->fs));

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about) 
{
	strcpy (about->text, "Download program for Polaroid DC320 POS camera. "
		"Originally written by Peter Desnoyers "
		"<pjd@fred.cambridge.ma.us>, and adapted for gphoto2 by "
		"Lutz Müller <urc8@rz.uni-karlsruhe.de>.");

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data)
{
	int n;
	Camera *camera = data;

	/* Fill the list */
	CHECK_RESULT (n = pdc320_num (camera->port));
	gp_list_populate (list, "PDC320%04i.raw", n);

	return (GP_OK);
}

int
camera_init (Camera *camera) 
{
	int result;
	gp_port_settings settings;

        /* First, set up all the function pointers */
        camera->functions->file_get          = camera_file_get;
        camera->functions->folder_delete_all = camera_folder_delete_all;
        camera->functions->about             = camera_about;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);

	/* Open the port and check if the camera is there */
	CHECK_RESULT (gp_port_settings_get (camera->port, &settings));
	strcpy (settings.serial.port, camera->port_info->path);
	if (camera->port_info->speed)
		settings.serial.speed = camera->port_info->speed;
	else
		settings.serial.speed = 115200;
	CHECK_RESULT (gp_port_settings_set (camera->port, settings));
	CHECK_RESULT (gp_port_timeout_set (camera->port, 5000));

	/* Check if the camera is really there */
	CHECK_RESULT (gp_port_open (camera->port));
	result = pdc320_init (camera->port);
	gp_port_close (camera->port);

	return (result);
}
