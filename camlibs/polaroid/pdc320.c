/* pdc320.c
 *
 * Copyright © 2001 Lutz Müller
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

/* Originally written by Peter Desnoyers <pjd@fred.cambridge.ma.us>,
 * and adapted for gphoto2 by
 * Nathan Stenzel <nathanstenzel@users.sourceforge.net> and
 * Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 * Maintained by Nathan Stenzel <nathanstenzel@users.sourceforge.net>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

//#include "jpeghead.h"
#include "pdc320.h"
//#include <libgphoto2/jpeg.h>
#include <gphoto2-library.h>
#include <gphoto2-port-log.h>

#define GP_MODULE "pdc320"

/*******************************************************************************/
/* NOTICE: There is a 16x8 block of pixels after the image data.               */
/* All of them are about the same and I believe they are for image correction. */
/*******************************************************************************/

/* Notes on what needs work:
 * 	Make a better header that could correct color problems....
 *		or manage to decode, color correct, and save the image.
 *
 * 	Fix some bug where the pdc320_num returns a strange response after
 *		pdc320_delete is called. Perhaps it needs to reset or something
 */

/*
 * For the PDC640SE, the init sequence is INIT,ID,ENDINIT,STATE,NUM,SIZE,
 * handshake?, PIC.
 *
 * During handshake, if the first read byte is not 6 then it is not good. Read
 * the number of bytes mentioned +2 ("FE FF"?)and then INIT and try SIZE
 * again (this is a loop).
 *
 * It is possible that the PDC320 might do a similar handshaking sequence at
 * that same point.
 */

typedef enum {
	PDC320,
	PDC640SE
} PDCModel;

struct _CameraPrivateLibrary {
	PDCModel model;
};

static int
pdc320_id (GPPort *port, const char **model)
{
	int i;
	unsigned char buf[32];

	GP_DEBUG ("*** PDC320_ID ***");
	CR (gp_port_write (port, PDC320_ID, sizeof (PDC320_ID) - 1));
	CR (gp_port_read (port, buf, 14));
	if (model) {
		*model = "unknown";
		for (i = 0; models[i].model; i++)
			if (buf[1] == models[i].id) {
				*model = models[i].model;
				break;
			}
	}

	return (GP_OK);
}

static int
pdc320_init (GPPort *port)
{
	unsigned char buf[32];

	GP_DEBUG ("*** PDC320_INIT ***");
	CR (gp_port_write (port, PDC320_INIT,
				     sizeof (PDC320_INIT) - 1));
	CR (gp_port_read (port, buf, 3));
#if 0
	if ((buf[0] != 0x05) || //0x0f?
	    (buf[1] != 0xfa) ||
	    (buf[2] != 0xff))
		return (GP_ERROR_CORRUPTED_DATA);
#endif

	CR (pdc320_id (port, NULL));

	GP_DEBUG ("*** PDC320_STATE ***");
	CR (gp_port_write (port, PDC320_STATE,
				     sizeof (PDC320_STATE) - 1));
	CR (gp_port_read (port, buf, 16));

	GP_DEBUG ("*** PDC320_ENDINIT ***");
	CR (gp_port_write (port, PDC320_ENDINIT,
				     sizeof (PDC320_ENDINIT) - 1));
	CR (gp_port_read (port, buf, 8));
	return (GP_OK);
}

static int
pdc320_num (GPPort *port)
{
	int num;
	unsigned char buf[4];

	/* The first byte we get is the number of images on the camera */
	CR (gp_port_write (port, PDC320_NUM,
					   sizeof (PDC320_NUM) - 1));
	CR (gp_port_read (port, buf, 3));
	num = buf[1];

#if 0
	if ((buf[1] != 0x01) ||
	    (buf[2] != 0xfc) ||
	    (buf[3] != 0xfe))
		return (GP_ERROR_CORRUPTED_DATA);
#endif

	GP_DEBUG ("The camera contains %i "
			 "files.", num);

	return (num);
}

static int
pdc320_delete (GPPort *port)
{
	unsigned char buf[7];

	/*
	 * For the PDC320, we get 3 bytes (0x8 0xf7 0xff) back. It could
	 * well be that other cameras send other bytes.
	 */
	CR (gp_port_write (port, PDC320_DEL,
				     sizeof (PDC320_DEL) - 1));
/* Since the Polaroid 640SE times out here, I will read one byte at a time to
   find out how many bytes to read.
 * Now, let's see how many bytes it reads before it times out. : )
 */
/*	CR (gp_port_read (port, buf, 1));
	CR (gp_port_read (port, buf, 1));
	CR (gp_port_read (port, buf, 1));
*/
	CR (gp_port_read (port, buf, 3));
#if 0
	if ((buf[0] != 0x08) ||
	    (buf[1] != 0xf7) ||
	    (buf[2] != 0xff))
		return (GP_ERROR_CORRUPTED_DATA);
#endif

	return (GP_OK);
}

static int
pdc320_size (Camera *camera, int n)
{
	int size, i;
	unsigned char buf[256];
	unsigned char cmd[] = PDC320_SIZE;

	cmd[5] = n;		/* n is from 1 on up */
	cmd[7] = 0xff - n;

	for (i = 0; i <= RETRIES; i++) {

		/* Write the command */
		CR (gp_port_write (camera->port, cmd, sizeof (cmd)));

		/* Read one byte and check if we can continue */
		CR (gp_port_read (camera->port, buf, 1));
		if (buf[0] != ACK) {

			/*
			 * Do we need to dump some bytes here before trying
			 * again?
                         * Yes, but how many is not known for the PDC 320.
			 */
			switch (camera->pl->model) {
			case PDC640SE:
				CR (gp_port_read (camera->port, buf,
							    buf[0] + 2));
				CR (pdc320_init (camera->port));
				break;
			case PDC320:
				i = RETRIES;
				break;
			}
			continue;
		}

		/*
		 * Ok, everything is fine. Read 6 bytes containing the size
		 * and return.
		 */
		CR (gp_port_read (camera->port, buf, 6));
		size = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
		GP_DEBUG ("Image %i has size "
				 "%i.", n, size);
		return (size);
	}

	return (GP_ERROR_CORRUPTED_DATA);
}

static int
pdc320_pic (Camera *camera, int n, unsigned char **data, int *size)
{
	unsigned char cmd[] = PDC320_PIC;
	unsigned char buf[2048];
	int remaining, f1, f2, i, len, checksum;
	int chunksize=2000;

	/* Get the size of the picture and allocate the memory */
	GP_DEBUG ("Checking size of image %i...",
			 n);
	CR (*size = pdc320_size (camera, n));
	*data = malloc (sizeof (char) * (*size));
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	cmd[5] = n;
	cmd[7] = 0xff - n;

	CR_FREE (gp_port_write (camera->port, cmd, sizeof (cmd)),
			   *data);

	switch (camera->pl->model) {
	case PDC640SE:
		chunksize = 528;
		break;
	case PDC320:
		chunksize = 2000;
		break;
	}

	len = *size;
	for (i = 0; i < *size; i += chunksize) {

		/* How many bytes do we read in this round? */
		remaining = *size - i;
		len = (remaining > chunksize) ? chunksize : remaining;

		/* Read the frame number */
		usleep (10000);
		CR_FREE (gp_port_read (camera->port, buf, 5), *data);
		f1 = (buf[1] << 8) + buf[2];
		f2 = (buf[3] << 8) + buf[4];
		GP_DEBUG ("Reading frame %d "
				 "(%d)...", f1, f2);

		/* Read the actual data */
		usleep(1000);
		CR_FREE (gp_port_read (camera->port, *data + i, len), *data);
		
		/* Read the checksum */
		CR_FREE (gp_port_read (camera->port, buf, 2), *data);
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

int
camera_abilities (CameraAbilitiesList *list) 
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].model; i++) {
		memset(&a, 0, sizeof(a));
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port     = GP_PORT_SERIAL;
		a.speed[0] = 0;
		a.operations        = GP_OPERATION_NONE;
		a.file_operations   = GP_FILE_OPERATION_NONE;
		a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

		CR (gp_abilities_list_append (list, a));
	}

	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
	jpeg *myjpeg;
	chunk *tempchunk;
	int n, size, width, height;
	unsigned char *data;

	if ((type != GP_FILE_TYPE_RAW) && (type != GP_FILE_TYPE_NORMAL))
		return (GP_ERROR_NOT_SUPPORTED);

	/*
	 * Get the number of the picture from the filesystem and increment
	 * since we need a range starting with 1.
	 */
	GP_DEBUG ("Getting number from fs...");
	CR (n = gp_filesystem_number (camera->fs, folder, filename, context));
	n++;

	/* Get the file */
	GP_DEBUG ("Getting file %i...", n);
	CR (pdc320_pic (camera, n, &data, &size));

	/* Post-processing */
	switch (type) {
	case GP_FILE_TYPE_RAW:
		CR (gp_file_set_data_and_size (file, data, size));
		CR (gp_file_set_name (file, filename));
		CR (gp_file_set_mime_type (file, GP_MIME_RAW));
		break;
	case GP_FILE_TYPE_NORMAL:
	default:
	GP_DEBUG ("Using Nathan Stenzel's experimental jpeg.c\n");
	GP_DEBUG ("About to make jpeg header\n");
    width = data[4]*256 + data[5];
    height = data[2]*256 + data[3];
	GP_DEBUG ("Width=%i\tHeight=%i\n", width, height);
    myjpeg = gp_jpeg_header(width,height/2, 0x11,0x11,0x21, 1,0,0, &chrominance, &luminance,
            0,0,0, chunk_new_filled(HUFF_00), chunk_new_filled(HUFF_10), NULL, NULL);
	GP_DEBUG ("Turning the picture data into a chunk data type\n");
    tempchunk = chunk_new(size);
    tempchunk->data = data;
	GP_DEBUG ("Adding the picture data to the jpeg structure\n");
    gp_jpeg_add_marker(myjpeg, tempchunk, 6, size-1);
	GP_DEBUG ("Writing the jpeg file\n");
    gp_jpeg_write(file, filename, myjpeg);
	GP_DEBUG ("Cleaning up the mess\n");
    gp_jpeg_destroy(myjpeg);

    }
    return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;

	CR (pdc320_delete (camera->port));

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, "Download program for several Polaroid cameras. "
		"Originally written by Peter Desnoyers "
		"<pjd@fred.cambridge.ma.us>, and adapted for gphoto2 by "
		"Nathan Stenzel <nathanstenzel@users.sourceforge.net> and "
		"Lutz Müller <urc8@rz.uni-karlsruhe.de>.\n"
		"Polaroid 640SE testing was done by Michael Golden "
		"<naugrim@juno.com>.");

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	int n;
	Camera *camera = data;

	/* Fill the list */
	CR (n = pdc320_num (camera->port));
	gp_list_populate (list, "PDC320%04i.jpg", n);

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	const char *model;

	CR (pdc320_id (camera->port, &model));
	strcpy (summary->text, "Model: ");
	strcat (summary->text, model);

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	if (!camera)
		return (GP_ERROR_BAD_PARAMETERS);

	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	CameraAbilities abilities;
	int result;

        /* First, set up all the function pointers */
	camera->functions->exit		= camera_exit;
        camera->functions->about        = camera_about;
	camera->functions->summary      = camera_summary;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL,
				      camera);
	gp_filesystem_set_folder_funcs (camera->fs, NULL, delete_all_func,
					NULL, NULL, camera);

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);

	/* What model are we talking to? */
	gp_camera_get_abilities (camera, &abilities);
	if (!strcmp (abilities.model, "Polaroid Fun! 320"))
		camera->pl->model = PDC320;
	else if (!strcmp (abilities.model, "Polaroid 640SE"))
		camera->pl->model = PDC640SE;
	else {
		free (camera->pl);
		camera->pl = NULL;
		return (GP_ERROR_MODEL_NOT_FOUND);
	}

	/* Open the port and check if the camera is there */
	gp_port_get_settings (camera->port, &settings);
	if (!settings.serial.speed)
		settings.serial.speed = 115200;
	gp_port_set_settings (camera->port, settings);
	gp_port_set_timeout (camera->port, 30000);

	/* Check if the camera is really there */
	result = pdc320_init (camera->port);
	if (result < 0) {
		free (camera->pl);
		camera->pl = NULL;
		return (result);
	}

	return (GP_OK);
}

