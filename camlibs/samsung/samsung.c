/* samsung.c
 *
 * Copyright (C) 2000 James McKenzie
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
 * 2001-08-31  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 * Although I ported this library to the gphoto2 architecture, I cannot test
 * it because I don't own this camera. If you own such a camera, it would 
 * be great if you could test this library and report success or failure
 * either directly to me or to gphoto-devel@gphoto.org.
 */

#include <stdlib.h>
#include <string.h>
#include <gphoto2-library.h>

/* The commands */
#define SDSC_START      0x43
#define SDSC_NEXT     0x53
#define SDSC_BINARY     0x06
#define SDSC_RETRANSMIT 0x15

#define SDSC_BLOCKSIZE 0x400
#define SDSC_INFOSIZE  0x080

#define SDSC_TIMEOUT    500
#define SDSC_RETRIES      3

/* Our own error codes */
#define SDSC_ERROR_EOF -1001

#define CHECK_RESULT(result) {int r = result; if (r < 0) return (r);}

static int
SDSC_send (GPPort *port, unsigned char command)
{
	CHECK_RESULT (gp_port_write (port, &command, 1));

	return (GP_OK);
}

static int
SDSC_receive (GPPort *port, unsigned char *buf, int length)
{
	char tmp[3];
	int i, result = GP_OK;

	for (i = 0; i < SDSC_RETRIES; i++) {

		/* Read the header (3 bytes) and check for EOF */
		CHECK_RESULT (gp_port_read (port, tmp, 1));
		if (tmp[0] == 0x04)
			return (SDSC_ERROR_EOF);
		result = gp_port_read (port, tmp, 2);
		if (result < 0) {
			CHECK_RESULT (SDSC_send (port, SDSC_RETRANSMIT));
			continue;
		}

		/* Read data */
		result = gp_port_read (port, buf, length);
		if (result < 0) {
			CHECK_RESULT (SDSC_send (port, SDSC_RETRANSMIT));
			continue;
		}

		/* Read footer (2 bytes) */
		result = gp_port_read (port, tmp, 2);
		if (result < 0) {
			CHECK_RESULT (SDSC_send (port, SDSC_RETRANSMIT));
			continue;
		}
	}

	return (result);
}

static int
is_null (unsigned char *buf)
{
	int n = 128;
	
	while (n--)
		if (*(buf++))
			return (0);

	return (1);
}

static int
SDSC_initialize (GPPort *port)
{
	unsigned char buffer[SDSC_INFOSIZE];

	/* Read out a header */
	do {
		CHECK_RESULT (SDSC_send (port, SDSC_NEXT));
		CHECK_RESULT (SDSC_send (port, SDSC_START));
		CHECK_RESULT (SDSC_receive (port, buffer, SDSC_INFOSIZE));
	} while (!is_null (buffer));

	return (GP_OK);
}

int
camera_id (CameraText *id) 
{
	strcpy (id->text, "samsung");

	return (GP_OK);
}

static struct {
	const char *model;
} models[] = {
	{"Samsung digimax 800k"},
	{NULL}
};

int
camera_abilities (CameraAbilitiesList *list) 
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].model; i++) {
		memset (&a, 0, sizeof(a));

		memset(&a, 0, sizeof(a));
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port     = GP_PORT_SERIAL;
		a.speed[0] = 0;
		a.operations        = GP_OPERATION_NONE;
		a.file_operations   = GP_FILE_OPERATION_NONE;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		
		CHECK_RESULT (gp_abilities_list_append (list, a));
	}
	
	return (GP_OK);
}

static int
camera_exit (Camera *camera) 
{
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data)
{
	Camera *camera = user_data;
	int i, n, result;
	unsigned char buffer[SDSC_BLOCKSIZE];
	long int size;
	unsigned char *data;

	if (type != GP_FILE_TYPE_NORMAL)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Get the picture number from the filesystem (those start at 0!) */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, filename));

	/* Rewind */
	CHECK_RESULT (SDSC_initialize (camera->port));

	/* Seek the header of our file */
	for (i = 0; i <= n; i++) {
		CHECK_RESULT (SDSC_send (camera->port, SDSC_NEXT));
		CHECK_RESULT (SDSC_send (camera->port, SDSC_START));
		CHECK_RESULT (SDSC_receive (camera->port, buffer, SDSC_INFOSIZE));
	}

	/* Extract the size of the file and allocate the memory */
	size = (3 * SDSC_BLOCKSIZE) + buffer[12] - 1;
	data = malloc (sizeof (char) * size);
	if (!data)
		return (GP_ERROR_NO_MEMORY);
	
	/* Put the camera into image mode */
	CHECK_RESULT (SDSC_send (camera->port, SDSC_BINARY));
	CHECK_RESULT (SDSC_send (camera->port, SDSC_START));

	/* Read data */
	for (i = 0; ; i++) {

		/* Read data and check for EOF */
		result = SDSC_receive (camera->port, buffer, SDSC_BLOCKSIZE);
		if (result == SDSC_ERROR_EOF)
			break;
		else if (result < 0) {
			free (data);
			return (result);
		}

		/* Copy the data */
		memcpy (data + (i * SDSC_BLOCKSIZE), buffer, SDSC_BLOCKSIZE);

		CHECK_RESULT (SDSC_send (camera->port, SDSC_BINARY));
	}

	CHECK_RESULT (gp_file_set_data_and_size (file, data, size));
	CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_JPEG));

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about) 
{
	strcpy (about->text, "The Samsung digimax 800k driver has "
		"been written by James Mckenzie "
		"<james@fishsoup.dhs.org> for gphoto. "
		"Lutz Müller <urc8@rz.uni-karlsruhe.de> ported it to "
		"gphoto2 but has not been able to test it. Please "
		"report success or failure on gphoto-devel@gphoto.org.");

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data)
{
	int n;
	unsigned char buffer[SDSC_INFOSIZE];
	Camera *camera = data;

	/* Rewind */
	CHECK_RESULT (SDSC_initialize (camera->port)); 

	/* Count the pictures */
	for (n = 0; ; n++) {
		CHECK_RESULT (SDSC_send (camera->port, SDSC_NEXT));
		CHECK_RESULT (SDSC_send (camera->port, SDSC_START));
		CHECK_RESULT (SDSC_receive (camera->port,
				buffer, SDSC_INFOSIZE));
		if (is_null (buffer))
			break;
	}

	/* Populate the list */
	gp_list_populate (list, "SDSC%04i.jpg", n);

	return (GP_OK);
}

int
camera_init (Camera *camera) 
{
	GPPortSettings settings;

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	/* Some settings */
	CHECK_RESULT (gp_port_settings_get (camera->port, &settings));
	/* Should we adjust anything here? */
	CHECK_RESULT (gp_port_settings_set (camera->port, settings));
	CHECK_RESULT (gp_port_timeout_set (camera->port, SDSC_TIMEOUT));

	/* Open the port and check if the camera is there */
	CHECK_RESULT (SDSC_initialize (camera->port));

	return (GP_OK);
}
