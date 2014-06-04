/* library.c
 *
 * Copyright (c) 2000 Scott Fritzinger <scottf@unr.edu>
 * Copyright (c) 2000 Andreas Meyer <ahm@spies.com>
 * Copyright (c) 2000 Pete Zaitcev <zaitcev@metabyte.com>
 * Copyright (c) 2000 Jeff Laing <jeffl@SPATIALinfo.com>
 * Copyright (c) 2007 Marcus Meissner <marcus@jet.franken.de>
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
#define _POSIX_C_SOURCE 199309L

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bayer.h>
#include <unistd.h>
#include <gphoto2/gphoto2.h>

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

#define MAX_PICTURES	6

#define ACK		0x06
#define NAK		0x15

#define COMMAND_BYTE	1
#define RESPONSE_BYTE	1
#define DATA1_BYTE	2
#define DATA2_BYTE	3

#define BARBIE_DATA_FIRMWARE	0
#define BARBIE_DATA_THUMBNAIL	1
#define BARBIE_DATA_PICTURE	2

#define GP_MODULE "barbie"

/* Some simple packet templates */
static const char packet_1[4]                = {0x02, 0x01, 0x01, 0x03};

/* Utility Functions */
static int
barbie_read_response(GPPort *port, unsigned char *response, unsigned int size) {
	int x;
	char ack = 0;

	/* Read the ACK */
	x=gp_port_read(port, &ack, 1);
	if ((ack != ACK)||(x<0))
		return GP_ERROR_IO_READ;
	/* Read the Response */
	memset(response, 0, size);
	return gp_port_read(port, (char*)response, size);
}

static
int barbie_exchange (GPPort *port, unsigned char *cmd, unsigned int cmd_size, unsigned char *resp, unsigned int resp_size) {
	int count = 0;
	while (count++ < 10) {
		if (gp_port_write(port, (char*)cmd, cmd_size) < GP_OK)
			return (0);
		if (barbie_read_response(port, resp, resp_size) < GP_OK)
			return (0);
		/* if it's not busy, return */
		if (resp[RESPONSE_BYTE] != '!')
			return (1);
		/* if busy, sleep 2 seconds */
		sleep(2);
	}
	return (0);
}

static int
barbie_ping(GPPort *port) {
	unsigned char cmd[4], resp[4];

	GP_DEBUG( "Pinging the camera...");

	memcpy(cmd, packet_1, 4);
	cmd[COMMAND_BYTE] = 'E';
	cmd[DATA1_BYTE]   = 'x';

	if (barbie_exchange(port, cmd, 4, resp, 4) == 0)
		return (0);

	if (resp[DATA1_BYTE] != 'x')
		return (0);
	GP_DEBUG ("Ping answered!");
	return (1);
}

static int
barbie_file_count (GPPort *port) {
        unsigned char cmd[4], resp[4];

        GP_DEBUG ("Getting the number of pictures...");

        memcpy(cmd, packet_1, 4);
        cmd[COMMAND_BYTE] = 'I';
        cmd[DATA1_BYTE]   = 0;

        if (barbie_exchange(port, cmd, 4, resp, 4) != 1)
                return (0);
        return (resp[DATA1_BYTE]);
}

static unsigned char *
barbie_read_firmware(GPPort *port) {

	unsigned char cmd[4];
	unsigned char resp[4];
	unsigned int size;
	char c;
	unsigned char *s = NULL;

	GP_DEBUG ("Getting Firmware...");

	memcpy(cmd, packet_1, 4);
	cmd[COMMAND_BYTE] = 'V';
	cmd[DATA1_BYTE]   = '0';
	if (barbie_exchange(port, cmd, 4, resp, 4) != 1)
		return (0);
	/* we're getting the firmware revision */
	size = resp[2];
	s = malloc(size);
	memset(s, 0, size);
	s[0] = resp[3];
	if (gp_port_read(port, (char*)&s[1], size-1) < 0) {
		free(s);
		return (NULL);
	}
	/* read the footer */
	if (gp_port_read(port, &c, 1) < 0) {
		free(s);
		return (0);
	}
	return s;
}

static int
barbie_read_picture(GPPort *port, int picture_number, int get_thumbnail, CameraFile *file) {
	unsigned char cmd[4], resp[4];
	unsigned int size;
	char c;
	int cols, visrows, blackrows, statusbytes;
	int x, y, z;
	unsigned char *s = NULL, *us = NULL, *rg = NULL, *t = NULL;
	char ppmhead[64];
	unsigned char uc;

	memcpy(cmd, packet_1, 4);
	cmd[COMMAND_BYTE] = 'A';
	cmd[DATA1_BYTE]   = picture_number;

	if (barbie_exchange(port, cmd, 4, resp, 4) != 1)
		return GP_ERROR_IO_READ;

	memcpy(cmd, packet_1, 4);
	if (get_thumbnail)
		cmd[COMMAND_BYTE] = 'M';
	   else
		cmd[COMMAND_BYTE] = 'U';
	cmd[DATA1_BYTE] = 0;

	if (barbie_exchange(port, cmd, 4, resp, 4) != 1)
		return (0);
	GP_DEBUG( "Getting Picture...");
	/* we're getting a picture */
	cols = resp[2];
	blackrows = resp[3];
	if (gp_port_read(port, (char*)&uc, 1) < 0)
		return GP_ERROR_IO_READ;
	visrows = uc;
	if (gp_port_read(port, (char*)&uc, 1) < 0)
		return GP_ERROR_IO_READ;
	statusbytes = uc;
	size = cols*(blackrows+visrows)+statusbytes;
	sprintf(ppmhead, "P6\n# test.ppm\n%i %i\n255\n", cols-4, visrows);
	gp_file_append (file, ppmhead, strlen(ppmhead));

	us = malloc(size);
	rg = malloc(size);
	s  = malloc(cols*(blackrows+visrows)*3);
	t  = malloc((cols-4)*visrows*3);
	memset(us, 0, size);
	memset(rg, 0, size);
	memset(s , 0, size);
	memset(t , 0, size);
	if (gp_port_read(port, (char*)us, size)<0) {
		free(us);
		free(rg);
		free(s);
		free(t);
		return GP_ERROR_IO_READ;
	}
	/* Unshuffle the data */
	for (x=0; x<blackrows+visrows; x++) {
		for (y=0; y<cols-4; y++) {
			z = x*cols + y/2 + (y%2)*(cols/2+2);
			rg[x*cols+(y^1)] = us[z];
		}
	}
	free (us);
	/* Camera uses a Bayer CCD array:
	 *		gb  gb   ...
	 *		rg  rg   ...
	 */
	gp_bayer_decode (rg, cols, blackrows+visrows, s, BAYER_TILE_GBRG);
	free (rg);
	/* Shrink it a bit to avoid junk cols and black rows */
	for (x=0; x<visrows; x++)
		memcpy (t+x*(cols-4)*3,s+cols*(x+blackrows)*3,(cols-4)*3);
	gp_file_append (file, (char*)t, visrows*(cols-4)*3);
	free (s);
	free (t);

	/* read the footer */
	if (gp_port_read(port, &c, 1) < 0)
		return GP_ERROR_IO_READ;
	return GP_OK;
}


/* GPhoto specific stuff */
static char *models[] = {
        "Barbie",
        "Nick Click",
        "WWF",
        "Hot Wheels",
        NULL
};

int
camera_id (CameraText *id)
{
        strcpy(id->text, "barbie");
        return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
        int x=0;
        CameraAbilities a;

        while (models[x]) {
                /* Fill in the appropriate flags/data */
		memset (&a, 0, sizeof(a));
                strcpy(a.model, models[x]);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
                a.port      = GP_PORT_SERIAL;
                a.speed[0]  = 57600;
                a.speed[1]  = 0;
                a.operations        = GP_OPERATION_NONE;
                a.file_operations   = GP_FILE_OPERATION_PREVIEW;
                a.folder_operations = GP_FOLDER_OPERATION_NONE;

                /* Append it to the list */
                gp_abilities_list_append(list, a);

                x++;
        }
        return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
        Camera *camera = data;

        return gp_list_populate (list, "mattel%02i.ppm", barbie_file_count (camera->port));
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
        int num;

        /* Retrieve the number of the photo on the camera */
        num = gp_filesystem_number (camera->fs, "/", filename, context);
	if (num < GP_OK)
		return num;
        gp_file_set_mime_type (file, GP_MIME_PPM);
        switch (type) {
        case GP_FILE_TYPE_NORMAL:
                return barbie_read_picture (camera->port, num, 0, file);
                break;
        case GP_FILE_TYPE_PREVIEW:
                return barbie_read_picture (camera->port, num, 1, file);
                break;
        default:
                return GP_ERROR_NOT_SUPPORTED;
        }
        return GP_ERROR_NOT_SUPPORTED;
}

#if 0
int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) {

        char cmd[4], resp[4];

        memcpy(cmd, packet_1, 4);

        cmd[COMMAND_BYTE] = 'G';
        cmd[DATA1_BYTE]   = 0x40;
        if (barbie_exchange(cmd, 4, resp, 4) == 0)
                return (0);

        cmd[COMMAND_BYTE] = 'Y';
        cmd[DATA1_BYTE]   = 0;
        if (barbie_exchange(cmd, 4, resp, 4) == 0)
                return (0);

        return(resp[DATA1_BYTE] == 0? GP_OK: GP_ERROR);

        return (GP_ERROR);
}
#endif

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
        int num;
        char *firm;

        num = barbie_file_count (camera->port);
        firm = (char*)barbie_read_firmware (camera->port);
        sprintf(summary->text, _("Number of pictures: %i\nFirmware Version: %s"), num,firm);
        free(firm);
        return GP_OK;
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
        strcpy (about->text,_("Barbie/HotWheels/WWF\nScott Fritzinger <scottf@unr.edu>\nAndreas Meyer <ahm@spies.com>\nPete Zaitcev <zaitcev@metabyte.com>\n\nReverse engineering of image data by:\nJeff Laing <jeffl@SPATIALinfo.com>\n\nImplemented using documents found on\nthe web. Permission given by Vision."));
        return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func
};

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
        int res;

        /* First, set up all the function pointers */
        camera->functions->summary      = camera_summary;
        camera->functions->about        = camera_about;

        /* Set up the CameraFilesystem */
        gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	/* Set up the port */
        gp_port_set_timeout (camera->port, 5000);
	gp_port_get_settings (camera->port, &settings);
        settings.serial.speed   = 57600;
        settings.serial.bits    = 8;
        settings.serial.parity  = 0;
        settings.serial.stopbits= 1;
        gp_port_set_settings (camera->port, settings);

        res = barbie_ping (camera->port);
        if (res)
                return (GP_OK);
        return (GP_ERROR);
}
