/* samsung.c
 *
 * Copyright 2000 James McKenzie
 * Copyright 2001 Lutz Mueller
 * Copyright 2002 Marcus Meissner
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
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>

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
#  define _(String) (String)
#  define N_(String) (String)
#endif

static int
SDSC_send (GPPort *port, unsigned char command)
{
	CHECK_RESULT (gp_port_write (port, (char *)&command, 1));

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
		result = gp_port_read (port, (char *)buf, length);
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
		break;
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

static const struct {
	const char *model;
} models[] = {
	{"Samsung:digimax 800k"},
	{"Dynatron:Dynacam 800"},
	{"Jenoptik:JD12 800ff"},
	{"Praktica:QD800"},
	{NULL}
};

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].model; i++) {
		memset (&a, 0, sizeof(a));
		strcpy (a.model, models[i].model);
		a.status		= GP_DRIVER_STATUS_PRODUCTION;
		a.port     		= GP_PORT_SERIAL;
		a.speed[0]		= 115200;
		a.speed[1]		= 0;
		a.operations		= GP_OPERATION_NONE;
		a.file_operations	= GP_FILE_OPERATION_NONE;
		a.folder_operations	= GP_FOLDER_OPERATION_NONE;
		CHECK_RESULT (gp_abilities_list_append (list, a));
	}
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
	int result;
	int havefirst = 0;
	unsigned char buffer[SDSC_BLOCKSIZE], first[SDSC_BLOCKSIZE];
	long int size, curread;
	unsigned int pid;

	if (type != GP_FILE_TYPE_NORMAL)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Seek the header of our file */
	while (1) {
		CHECK_RESULT (SDSC_send (camera->port, SDSC_NEXT));
		CHECK_RESULT (SDSC_send (camera->port, SDSC_START));
		CHECK_RESULT (SDSC_receive (camera->port, buffer, SDSC_INFOSIZE));
		if (!strcmp((char*)buffer,filename))
		    break;
		if (is_null(buffer)) { /* skipped to the end of the camera? */
		    /* Since we start at a random position, we wrap around. */
		    continue;
	        }
		/* We are at the first item again, so break. */
		if (havefirst && !strcmp((char*)first,(char*)buffer))
			return GP_ERROR_BAD_PARAMETERS;
		if (!havefirst) {
			havefirst = 1;
			strcpy((char*)first,(char*)buffer);
		}
	}
	/* The buffer header has
	 * filename (8.3 DOS format and \0)
	 * filesize (as ascii number) and \0
	 */
	/* Extract the size of the file */
	sscanf((char *)buffer+12,"%ld",&size);
	/* Put the camera into image mode */
	CHECK_RESULT (SDSC_send (camera->port, SDSC_BINARY));
	CHECK_RESULT (SDSC_send (camera->port, SDSC_START));

	pid = gp_context_progress_start(context,size,_("Downloading image..."));

	curread = 0;
	/* Read data */
	while (1) {
		/* Read data and check for EOF */
		result = SDSC_receive (camera->port, buffer, SDSC_BLOCKSIZE);
		if (result == SDSC_ERROR_EOF)
			break;
		if (result < 0)
			return result;
		gp_file_append(file,(char *)buffer,SDSC_BLOCKSIZE);
		curread += SDSC_BLOCKSIZE;
	        gp_context_progress_update(context, pid, curread);
		if (gp_context_cancel(context) == GP_CONTEXT_FEEDBACK_CANCEL)
		    return GP_ERROR_CANCEL;
		CHECK_RESULT (SDSC_send (camera->port, SDSC_BINARY));
	}
	gp_context_progress_stop(context, pid);
	CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_JPEG));
	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	/* Translators: please write 'M"uller' and 'Mei"sner' (that
	   is, with u-umlaut and eszett resp.) if your charset
	   allows it.  If not, use "Mueller" and "Meissner".  */
	strcpy (about->text, _("The Samsung digimax 800k driver has "
		"been written by James McKenzie "
		"<james@fishsoup.dhs.org> for gphoto. "
		"Lutz Mueller <lutz@users.sourceforge.net> ported it to "
		"gphoto2. Marcus Meissner <marcus@jet.franken.de> fixed "
		"and enhanced the port.")
		);

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	unsigned char buffer[SDSC_INFOSIZE];
	Camera *camera = data;

	/* Rewind */
	CHECK_RESULT (SDSC_initialize (camera->port));

	/* Count the pictures */
	while (1) {
		CHECK_RESULT (SDSC_send (camera->port, SDSC_NEXT));
		CHECK_RESULT (SDSC_send (camera->port, SDSC_START));
		CHECK_RESULT (SDSC_receive (camera->port,
				buffer, SDSC_INFOSIZE));
		if (is_null (buffer))
			break;
		gp_list_append(list, (char *)buffer, NULL);
	}
	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
               CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int havefirst = 0;
	unsigned char buffer[SDSC_INFOSIZE], first[SDSC_INFOSIZE];

	info->file.fields	= GP_FILE_INFO_NONE;
	/* Don't rewind, just go forward until we find it.
	 * Works way better for linear queries.
	 */
	while (1) {
		CHECK_RESULT (SDSC_send (camera->port, SDSC_NEXT));
		CHECK_RESULT (SDSC_send (camera->port, SDSC_START));
		CHECK_RESULT (SDSC_receive (camera->port,buffer,SDSC_INFOSIZE));
		if (is_null (buffer))
			continue;
		if (!strcmp((char*)buffer,filename)) {
			info->file.fields	= GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT | GP_FILE_INFO_SIZE;
			info->file.width	= 1024;
			info->file.height	= 768;
			strcpy(info->file.type,GP_MIME_JPEG);
			sscanf((char *)buffer+12,"%lld",(long long int *)&info->file.size);
			return GP_OK;
	        }
		/* We are at the first item again */
		if (havefirst && !strcmp((char*)first,(char*)buffer))
			break;
		if (!havefirst) {
			havefirst = 1;
			strcpy((char*)first,(char*)buffer);
		}
	}
	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func
};

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;

        /* First, set up all the function pointers */
        camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	/* Some settings */
	CHECK_RESULT (gp_port_get_settings (camera->port, &settings));
	settings.serial.speed   = 115200;
	settings.serial.bits    = 8;
	settings.serial.parity  = 0;
	settings.serial.stopbits= 1;
	CHECK_RESULT (gp_port_set_settings (camera->port, settings));
	CHECK_RESULT (gp_port_set_timeout (camera->port, SDSC_TIMEOUT));
	/* Open the port and check if the camera is there */
	CHECK_RESULT (SDSC_initialize (camera->port));
	return (GP_OK);
}
