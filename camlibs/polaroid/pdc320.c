/* pdc320.c
 *
 * Copyright 2001 Lutz Mueller
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/* Originally written by Peter Desnoyers <pjd@fred.cambridge.ma.us>,
 * and adapted for gphoto2 by
 * Nathan Stenzel <nathanstenzel@users.sourceforge.net> and
 * Lutz Mueller <lutz@users.sourceforge.net>
 *
 * Maintained by Nathan Stenzel <nathanstenzel@users.sourceforge.net>
 *
 * Original windows drivers available at:
 *	http://www.polaroid.com/service/software/index.html
 */

#define _BSD_SOURCE

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>

#include "pdc320.h"

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
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define GP_MODULE "pdc320"

/*******************************************************************************/
/* NOTICE: There is a 16x8 block of pixels after the image data.               */
/* All of them are about the same and I believe they are for image correction. */
/*******************************************************************************/

/* Notes on what needs work:
 * 	Make a better header that could correct color problems....
 *		or manage to decode, color correct, and save the image.
 * The quantization tables are incorrect for the PDC320.
 */

/*
 * For the PDC640SE, the init sequence is INIT,ID,ENDINIT,STATE,NUM,SIZE,PIC.
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
pdc320_escape(
	const unsigned char *inbuf, int inbuflen,
	unsigned char *outbuf
) {
	int i,j;

	j = 0;
	for (i=0;i<inbuflen;i++) {
		switch (inbuf[i]) {
		case 0xe3:
			outbuf[j++] = 0xe5;outbuf[j++] = 3;
			break;
		case 0xe4:
			outbuf[j++] = 0xe5;outbuf[j++] = 2;
			break;
		case 0xe5:
			outbuf[j++] = 0xe5;outbuf[j++] = 1;
			break;
		case 0xe6:
			outbuf[j++] = 0xe5;outbuf[j++] = 0;
			break;
		default:outbuf[j++] = inbuf[i];
			break;
		}
	}
	return j;
}

static int
pdc320_calc_checksum(const unsigned char *buf, int buflen) {
	int checksum, j;

	checksum = 0;
	j = 0;
	for (j=0;j<buflen/2;j++) {
		checksum += buf[j*2];
		checksum += buf[j*2+1]<<8;
	}
	if (buflen & 1)
		checksum += buf[buflen-1];

	while (checksum > 0xffff) {
		checksum =
			(checksum & 0xffff) + 
			((checksum >> 16) & 0xffff);
	}
	return 0xffff-checksum; /* neg checksum actually */
}

static int
pdc320_command(
	GPPort *port,
	const unsigned char *cmd, int cmdlen
) {
	unsigned char csum[2];
	int checksum, off, ret;
	unsigned char *newcmd;

	checksum = pdc320_calc_checksum (cmd, cmdlen);
	csum[0] = checksum & 0xff;
	csum[1] = (checksum >> 8) & 0xff;
	/* 4 times 0xe6, checksum and command, both might be escaped */
	newcmd = malloc (2*(sizeof(csum)+cmdlen)+4);
	if (!newcmd)
		return GP_ERROR_NO_MEMORY;
	memset (newcmd, 0xe6, 4); off = 4;
	off += pdc320_escape (cmd, cmdlen, newcmd + off);
	off += pdc320_escape (csum, 2, newcmd + off);
	ret = gp_port_write (port, (char *)newcmd, off);
	free(newcmd);
	return ret;
}

static int
pdc320_simple_command (GPPort *port, const unsigned char cmd) {
	return pdc320_command(port, &cmd, 1);
}

static int
pdc320_simple_reply (GPPort *port, const unsigned char expcode,
	unsigned int replysize, unsigned char *reply
) {
	unsigned char csum[2];
	int calccsum;

	CR (gp_port_read (port, (char *)reply, 1));
	if (reply[0] != expcode) {
		GP_DEBUG("*** reply got 0x%02x, expected 0x%02x\n",
			reply[0], expcode
		);
		return GP_ERROR;
	}
	CR (gp_port_read (port, (char *)reply+1, replysize-1));
	CR (gp_port_read (port, (char *)csum, 2));
	calccsum = pdc320_calc_checksum (reply, replysize);
	if (calccsum != ((csum[1] << 8) | csum[0])) {
		GP_DEBUG("csum %x vs %x\n",calccsum,((csum[0]<<8)|csum[1]));
		return GP_ERROR;
	}
	return GP_OK;
}

static int
pdc320_simple_command_reply (GPPort *port,
	const unsigned char cmd, const unsigned char expcode,
	unsigned int replysize, unsigned char *reply
) {
	CR (pdc320_simple_command (port, cmd));
	CR (pdc320_simple_reply (port, expcode, replysize, reply));
	return GP_OK;
}

static int
pdc320_init (GPPort *port)
{
	unsigned char buf[32];
	unsigned char e6[4];
	int i;

	GP_DEBUG ("*** PDC320_INIT ***");
	
	/* The initial command is prefixed by 4 raw E6. */
	memset(e6,0xe6,sizeof(e6));
	CR (gp_port_write (port, (char *)e6, sizeof (e6) ));

	GP_DEBUG ("*** PDC320_INIT ***");
	CR (pdc320_simple_command_reply (port, PDC320_INIT, 5, 1, buf));
	GP_DEBUG ("*** PDC320_ID ***");
	CR (pdc320_simple_command_reply (port, PDC320_ID, 0, 12, buf));
	GP_DEBUG ("*** PDC320_STATE ***");
	CR (pdc320_simple_command_reply (port, PDC320_STATE, 2, 22, buf));

	for (i=0;i<9;i++) {
		GP_DEBUG ("%d: %d (0x%x)",i,((buf[2+i*2]<<8)|buf[2+2*i+1]), ((buf[2+i*2]<<8)|buf[2+2*i+1]));
	}
	GP_DEBUG ("*** PDC320_ENDINIT ***");
	return pdc320_simple_command_reply (port, PDC320_ENDINIT, 9, 1, buf);
}

static int
pdc320_num (GPPort *port)
{
	unsigned char buf[2];

	GP_DEBUG ("*** PDC320_NUM ***");
	CR (pdc320_simple_command_reply (port, PDC320_NUM, 3, 2, buf));
	GP_DEBUG ("The camera contains %i files.", buf[1]);
	return buf[1];
}

static int
pdc320_delete (GPPort *port)
{
	unsigned char buf[1];

	GP_DEBUG ("*** PDC320_DELETE ***");
	return pdc320_simple_command_reply(port, PDC320_DEL, 8, 1, buf);
}

static int
pdc320_size (Camera *camera, int n)
{
	int size;
	unsigned char buf[5];
	unsigned char cmd[2];

	GP_DEBUG ("*** PDC320_SIZE ***");
	cmd[0] = PDC320_SIZE;
	cmd[1] = n;
	CR (pdc320_command (camera->port, cmd, sizeof (cmd)));
	CR (pdc320_simple_reply (camera->port, 6, 5, buf));
	size = (buf[1] << 24) + (buf[2] << 16) + (buf[3] << 8) + buf[4];
	GP_DEBUG ("Image %i has size %i.", n, size);
	return (size);
}

/* Unclear. */
static int
pdc320_0c (Camera *camera, int n)
{
	int size, i;
	unsigned char buf[3], *xbuf;
	unsigned char cmd[2];
	
	cmd[0] = 0x0c;
	cmd[1] = n;		/* n is from 1 on up */

	/* Write the command */
	GP_DEBUG ("*** PDC320_0c ***");
	CR (pdc320_command (camera->port, cmd, sizeof (cmd)));
	CR (gp_port_read (camera->port, (char *)buf, 3));
	if (buf[0] != 7)
		return GP_ERROR;
	size = (buf[1] << 8) | buf[2];
	xbuf = malloc (size);
	CR (gp_port_read (camera->port, (char *)xbuf, size));
	for (i=0; i<size; i++) {
		GP_DEBUG ("buf[%d]=0x%02x", i, xbuf[i]);
	}
	CR (gp_port_read (camera->port, (char *)buf, 2));
	/* checksum is calculated from both, but i am not clear how. */
	free (xbuf);
	return GP_OK;
}

static int
pdc320_pic (Camera *camera, int n, unsigned char **data, int *size)
{
	unsigned char cmd[2];
	unsigned char buf[2048];
	int remaining, f1, f2, i, len;
	int chunksize=2000;

	/* Get the size of the picture and allocate the memory */
	GP_DEBUG ("Checking size of image %i...", n);
	CR (*size = pdc320_size (camera, n));
	*data = malloc (sizeof (char) * (*size));
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	cmd[0] = PDC320_PIC;
	cmd[1] = n;

	CR_FREE (pdc320_command (camera->port, cmd, sizeof (cmd)), *data);
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
		CR_FREE (gp_port_read (camera->port, (char *)buf, 5), *data);
		f1 = (buf[1] << 8) + buf[2];
		f2 = (buf[3] << 8) + buf[4];
		GP_DEBUG ("Reading frame %d "
				 "(%d)...", f1, f2);

		/* Read the actual data */
		usleep(1000);
		CR_FREE (gp_port_read (camera->port, (char *)*data + i, len), *data);
		
		/* Read the checksum */
		CR_FREE (gp_port_read (camera->port, (char *)buf, 2), *data);
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
		a.status		= GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port			= GP_PORT_SERIAL;
		a.speed[0]		= 115200;
		a.speed[1]		= 0;
		a.operations		= GP_OPERATION_NONE;
		a.file_operations	= GP_FILE_OPERATION_NONE;
		a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;
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
	int r, n, size, width, height;
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
	r = pdc320_0c (camera, n);
	if (r < GP_OK) {
		free (data);
		return r;
	}

	/* Post-processing */
	switch (type) {
	case GP_FILE_TYPE_RAW:
		CR (gp_file_set_data_and_size (file, (char *)data, size));
		CR (gp_file_set_mime_type (file, GP_MIME_RAW));
		break;
	case GP_FILE_TYPE_NORMAL:
	default:
		GP_DEBUG ("Using Nathan Stenzel's experimental jpeg.c\n");
		GP_DEBUG ("About to make jpeg header\n");
		width = data[4]*256 + data[5];
		height = data[2]*256 + data[3];
		GP_DEBUG ("Width=%i\tHeight=%i\n", width, height);
		myjpeg = gpi_jpeg_header(width,height/2, 0x11,0x11,0x21, 1,0,0, &chrominance, &luminance,
		    0,0,0, gpi_jpeg_chunk_new_filled(HUFF_00),
		    gpi_jpeg_chunk_new_filled(HUFF_10), NULL, NULL);
		GP_DEBUG ("Turning the picture data into a chunk data type\n");
		tempchunk = gpi_jpeg_chunk_new(size);
		tempchunk->data = data;
		GP_DEBUG ("Adding the picture data to the jpeg structure\n");
		gpi_jpeg_add_marker(myjpeg, tempchunk, 6, size-1);
		GP_DEBUG ("Writing the jpeg file\n");
		gpi_jpeg_write(file, filename, myjpeg);
		GP_DEBUG ("Cleaning up the mess\n");
		gpi_jpeg_destroy(myjpeg);
		free (tempchunk);
		break;
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
	strcpy (about->text, _("Download program for several Polaroid cameras. "
		"Originally written by Peter Desnoyers "
		"<pjd@fred.cambridge.ma.us>, and adapted for gphoto2 by "
		"Nathan Stenzel <nathanstenzel@users.sourceforge.net> and "
		"Lutz Mueller <lutz@users.sf.net>.\n"
		"Polaroid 640SE testing was done by Michael Golden "
		"<naugrim@juno.com>."));
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
	char buf[12];

	GP_DEBUG ("*** PDC320_ID ***");
	CR (pdc320_simple_command_reply (camera->port, PDC320_ID, 0, 12, (unsigned char *)buf));
	sprintf (summary->text, _("Model: %x, %x, %x, %x"), buf[8],buf[9],buf[10],buf[11]);
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

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.delete_all_func = delete_all_func
};

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
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);

	/* What model are we talking to? */
	gp_camera_get_abilities (camera, &abilities);
	if (	!strcmp (abilities.model, "Polaroid:Fun! 320") ||
		!strcmp (abilities.model, "Polaroid Fun! 320")
	)
		camera->pl->model = PDC320;
	else if (!strcmp (abilities.model, "Polaroid:640SE") ||
		 !strcmp (abilities.model, "Polaroid 640SE")
	)
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
