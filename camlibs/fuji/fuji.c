/* fuji.c:
 *
 * Fuji Camera library for the gphoto project.
 *
 * © 2001 Matthew G. Martin <matt.martin@ieee.org>
 *     2002 Lutz Mueller <lutz@users.sourceforge.net>
 *
 * This routine works for Fuji DS-7 and DX-5,7,10 and
 * MX-500,600,700,1200,1700,2700,2900,  Apple QuickTake 200,
 * Samsung Kenox SSC-350N,Leica Digilux Zoom cameras and possibly others.
 *
 * Preview and take_picture fixes and preview conversion integrated
 * by Michael Smith <michael@csuite.ns.ca>.
 *
 * This driver was reworked from the "fujiplay" package:
 *    * A program to control Fujifilm digital cameras, like
 *    * the DS-7 and MX-700, and their clones.
 *    * Written by Thierry Bousch <bousch@topo.math.u-psud.fr>
 *    * and released in the public domain.
 *
 *  Portions of this code were adapted from
 *  GDS7 v0.1 interactive digital image transfer software for DS-7 camera
 *  Copyright 1998 Matthew G. Martin

 *  Some of which was derived from get_ds7 , a Perl Language library
 *  Copyright 1997 Mamoru Ohno
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301  USA
 */
#include "config.h"
#include "fuji.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gphoto2/gphoto2-port-log.h>

#include "libgphoto2/i18n.h"


#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

#define GP_MODULE "fuji"

#define CR(result)    {int __r = (result); if (__r < 0) return (__r);}
#define CRF(result,d) {int __r = (result); if (__r < 0) {free (d); return (__r);}}
#define CLEN(buf_len,required)					\
{								\
	if (buf_len < required) {				\
		gp_context_error (context, _("The camera sent "	\
			"only %i byte(s), but we need at "	\
			"least %i."), buf_len, required);	\
		return (GP_ERROR);				\
	}							\
}


#define STX 0x02 /* Start of data */
#define ETX 0x03 /* End of data */
#define EOT 0x04 /* End of session */
#define ENQ 0x05 /* Enquiry */
#define ACK 0x06
#define ESC 0x10
#define ETB 0x17 /* End of transmission block */
#define NAK 0x15

#define FUJI_ACK 0x00
#define FUJI_NAK 0x01

static int fuji_reset (Camera *camera, GPContext *context);

int
fuji_ping (Camera *camera, GPContext *context)
{
	unsigned char b;
	unsigned int i;
	int r;

	GP_DEBUG ("Pinging camera...");

	/* Drain input */
	while (gp_port_read (camera->port, (char *)&b, 1) >= 0);

	i = 0;
	while (1) {
		b = ENQ;
		CR (gp_port_write (camera->port, (char *)&b, 1));
		r = gp_port_read (camera->port, (char *)&b, 1);
		if ((r >= 0) && (b == ACK))
			return (GP_OK);
		i++;
		if (i >= 3) {
			gp_context_error (context, _("Could not contact "
				"camera."));
			return (GP_ERROR);
		}
	}
}

static int
fuji_send (Camera *camera, unsigned char *cmd, unsigned int cmd_len,
	   unsigned char last, GPContext *context)
{
        unsigned char b[1024], check;
        unsigned int i;

        /* Send header */
        b[0] = ESC;
        b[1] = STX;
        CR (gp_port_write (camera->port, (char *)b, 2));

        /*
	 * Escape the data we are going to send.
	 * Calculate the checksum.
	 */
	check = (last ? ETX : ETB);
        memcpy (b, cmd, cmd_len);
        for (i = 0; i < cmd_len; i++) {
		check ^= b[i];
                if (b[i] == ESC) {
                        memmove (b + i + 1, b + i, cmd_len - i);
                        b[i] = ESC;
                        i++;
                        cmd_len++;
                }
        }

        /* Send data */
        CR (gp_port_write (camera->port, (char *)b, cmd_len));

        /* Send footer */
        b[0] = ESC;
        b[1] = (last ? ETX : ETB);
        b[2] = check;
        CR (gp_port_write (camera->port, (char *)b, 3));

	return (GP_OK);
}

static int
fuji_recv (Camera *camera, unsigned char *buf, unsigned int *buf_len,
	   unsigned char *last, GPContext *context)
{
	unsigned char b[1024], check = 0;
	unsigned int i;

	/*
	 * Read the header. The checksum covers all bytes between
	 * ESC STX and ESC ET[X,B].
	 */
	CR (gp_port_read (camera->port, (char *)b, 6));

	/* Verify the header */
	if ((b[0] != ESC) || (b[1] != STX)) {
		gp_context_error (context, _("Received unexpected data "
			"(0x%02x, 0x%02x)."), b[0], b[1]);
		return (GP_ERROR_CORRUPTED_DATA);
	}

	/*
	 * We don't know the meaning of b[2] and b[3].
	 * We have seen 0x00 0x4d and 0x00 0x06.
	 */
	check ^= b[2];
	check ^= b[3];

	/* Determine the length of the packet. */
	*buf_len = ((b[5] << 8) | b[4]);
	check ^= b[4];
	check ^= b[5];
	GP_DEBUG ("We are expecting %i byte(s) data (excluding ESC quotes). "
		  "Let's read them...", *buf_len);

	/* Read the data. Unescape it. Calculate the checksum. */
	for (i = 0; i < *buf_len; i++) {
		CR (gp_port_read (camera->port, (char *)buf + i, 1));
		if (buf[i] == ESC) {
			CR (gp_port_read (camera->port, (char *)buf + i, 1));
			if (buf[i] != ESC) {
				gp_context_error (context,
					_("Wrong escape sequence: "
					"expected 0x%02x, got 0x%02x."),
					ESC, buf[i]);

				/* Dump the remaining data */
				while (gp_port_read (camera->port, (char *)b, 1) >= 0);

				return (GP_ERROR_CORRUPTED_DATA);
			}
		}
		check ^= buf[i];
	}

	/* Read the footer */
	CR (gp_port_read (camera->port, (char *)b, 3));
	if (b[0] != ESC) {
		gp_context_error (context,
			_("Bad data - got 0x%02x, expected 0x%02x."),
			b[0], ESC);
		return (GP_ERROR_CORRUPTED_DATA);
	}
	switch (b[1]) {
	case ETX:
		*last = 1;
		break;
	case ETB:
		*last = 0;
		break;
	default:
		gp_context_error (context,
			_("Bad data - got 0x%02x, expected 0x%02x or "
			  "0x%02x."), b[1], ETX, ETB);
		return (GP_ERROR_CORRUPTED_DATA);
	}
	check ^= b[1];
	if (check != b[2]) {
		gp_context_error (context,
			_("Bad checksum - got 0x%02x, expected 0x%02x."),
			b[2], check);
		return (GP_ERROR_CORRUPTED_DATA);
	}

	return (GP_OK);
}

static int
fuji_transmit (Camera *camera, unsigned char *cmd, unsigned int cmd_len,
	       unsigned char *buf, unsigned int *buf_len, GPContext *context)
{
	unsigned char last = 0, c, p;
	unsigned int b_len = 1024;
	int r, retries = 0, id = 0;

	/*
	 * Send the command. If we fail the first time, we only try once more.
	 * After the third time, the camera would reset itself automatically.
	 */
	retries = 0;
	while (1) {

		/* Give the user the possibility to cancel. */
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);

		CR (fuji_send (camera, cmd, cmd_len, 1, context));

		/* Receive ACK (hopefully) */
		CR (gp_port_read (camera->port, (char *)&c, 1));
		switch (c) {
		case ACK:
			break;
		case NAK:

			/* Camera didn't like the command */
			if (++retries > 1) {
				gp_context_error (context, _("Camera rejected "
					"the command."));
				return (GP_ERROR);
			}
			continue;

		case EOT:

			/* Camera got fed up and reset itself. */
			gp_context_error (context, _("Camera reset itself."));
			return (GP_ERROR);

		default:
			gp_context_error (context, _("Camera sent unexpected "
				"byte 0x%02x."), c);
			return (GP_ERROR_CORRUPTED_DATA);
		}

		break;
	}

	/*
	 * Read the response. If we expect more than 1024 bytes, show
	 * progress information.
	 */
	p = ((*buf_len > 1024) ? 1 : 0);
	if (p)
		id = gp_context_progress_start (context, *buf_len,
					        _("Downloading..."));
	*buf_len = 0;
	retries = 0;
	while (!last) {
		r = fuji_recv (camera, buf + *buf_len, &b_len, &last, context);
		if (r < 0) {
			retries++;
			while (gp_port_read (camera->port, (char *)&c, 1) >= 0);
			if (++retries > 2)
			    return (r);
			GP_DEBUG ("Retrying...");
			c = NAK;
			CR (gp_port_write (camera->port, (char *)&c, 1));
			continue;
		}

		/* Give the user the possibility to cancel. */
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			CR (fuji_reset (camera, context));
			return (GP_ERROR_CANCEL);
		}

		/* Acknowledge this packet. */
		c = ACK;
		CR (gp_port_write (camera->port, (char *)&c, 1));
		*buf_len += b_len;

		if (p)
			gp_context_progress_update (context, id, *buf_len);
	}
	if (p)
		gp_context_progress_stop (context, id);

	return (GP_OK);
}

int
fuji_get_cmds (Camera *camera, unsigned char *cmds, GPContext *context)
{
	unsigned char cmd[4], buf[1024];
	unsigned int i, buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_CMDS_VALID;
	cmd[2] = 0;
	cmd[3] = 0;
	CR (fuji_transmit (camera, cmd, 4, buf, &buf_len, context));

	memset (cmds, 0, 0xff);
	for (i = 0; i < buf_len; i++)
		cmds[buf[i]] = 1;

	return (GP_OK);
}

int
fuji_pic_count (Camera *camera, unsigned int *n, GPContext *context)
{
	unsigned char cmd[4], buf[1024];
	unsigned int buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_COUNT;
	cmd[2] = 0;
	cmd[3] = 0;
	CR (fuji_transmit (camera, cmd, 4, buf, &buf_len, context));

	CLEN (buf_len, 2);

	*n = (buf[1] << 8) | buf[0];

	return (GP_OK);
}

int
fuji_pic_name (Camera *camera, unsigned int i, const char **name,
	       GPContext *context)
{
	static unsigned char buf[1025];
	unsigned char cmd[6];
	unsigned int buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_NAME;
	cmd[2] = 2;
	cmd[3] = 0;
	cmd[4] = i;
	cmd[5] = (i >> 8);

	memset (buf, 0, sizeof (buf));
	CR (fuji_transmit (camera, cmd, 6, buf, &buf_len, context));
	*name = (char *)buf;

	return (GP_OK);
}

int
fuji_version (Camera *camera, const char **version, GPContext *context)
{
	static unsigned char buf[1025];
	unsigned char cmd[4];
	unsigned int buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_VERSION;
	cmd[2] = 0;
	cmd[3] = 0;

	memset (buf, 0, sizeof (buf));
	CR (fuji_transmit (camera, cmd, 4, buf, &buf_len, context));
	*version = (char *)buf;

	return (GP_OK);
}

int
fuji_model (Camera *camera, const char **model, GPContext *context)
{
	static unsigned char buf[1025];
	unsigned char cmd[4];
	unsigned int buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_MODEL;
	cmd[2] = 0;
	cmd[3] = 0;

	memset (buf, 0, sizeof (buf));
	CR (fuji_transmit (camera, cmd, 4, buf, &buf_len, context));
	*model = (char *)buf;

	return (GP_OK);
}

int
fuji_id_get (Camera *camera, const char **id, GPContext *context)
{
	static unsigned char buf[1025];
	unsigned char cmd[4];
	unsigned int buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_ID_GET;
	cmd[2] = 0;
	cmd[3] = 0;

	memset (buf, 0, sizeof (buf));
	CR (fuji_transmit (camera, cmd, 4, buf, &buf_len, context));
	*id = (char *)buf;

	return (GP_OK);
}

int
fuji_id_set (Camera *camera, const char *id, GPContext *context)
{
	unsigned char cmd[14], buf[1025];
	unsigned int buf_len = 0;

	/* It seems that the ID must be 10 chars or less */
	cmd[0] = 0;
	cmd[1] = FUJI_CMD_ID_SET;
	cmd[2] = 0x0a;
	cmd[3] = 0x00;
	memcpy (cmd + 4, id, MIN (strlen (id) + 1, 10));

	CR (fuji_transmit (camera, cmd, 14, buf, &buf_len, context));

	return (GP_OK);
}

int
fuji_pic_del (Camera *camera, unsigned int i, GPContext *context)
{
	unsigned char cmd[6], buf[1024];
	unsigned int buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_NAME;
	cmd[2] = 2;
	cmd[3] = 0;
	cmd[4] = i;
	cmd[5] = (i >> 8);

	CR (fuji_transmit (camera, cmd, 6, buf, &buf_len, context));

	return (GP_OK);
}

int
fuji_pic_size (Camera *camera, unsigned int i, unsigned int *size,
	       GPContext *context)
{
	unsigned char cmd[6], buf[1024];
	unsigned int buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_SIZE;
	cmd[2] = 2;
	cmd[3] = 0;
	cmd[4] = i;
	cmd[5] = (i >> 8);

	CR (fuji_transmit (camera, cmd, 6, buf, &buf_len, context));
	CLEN (buf_len, 4);

	*size = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

	return (GP_OK);
}

int
fuji_pic_get_thumb (Camera *camera, unsigned int i, unsigned char **data,
		    unsigned int *size, GPContext *context)
{
	unsigned char cmd[6];

	/* It seems that all thumbnails have the size of 60 x 175. */
	*size = 60 * 175;
	*data = malloc (sizeof (char) * *size);
	if (!*data) {
		gp_context_error (context, _("Could not allocate "
			"%i byte(s) for downloading the thumbnail."), *size);
		return (GP_ERROR_NO_MEMORY);
	}

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_GET_THUMB;
	cmd[2] = 2;
	cmd[3] = 0;
	cmd[4] = i;
	cmd[5] = (i >> 8);

	CRF (fuji_transmit (camera, cmd, 6, *data, size, context), *data);
	GP_DEBUG ("Download of thumbnail completed.");

	return (GP_OK);
}

int
fuji_pic_get (Camera *camera, unsigned int i, unsigned char **data,
	      unsigned int *size, GPContext *context)
{
	unsigned char cmd[6];

	/*
	 * First, get the size of the picture and allocate the necessary
	 * memory. Some cameras don't support the FUJI_CMD_PIC_SIZE command.
	 * We will then assume 66000 bytes.
	 */
	if (fuji_pic_size (camera, i, size, context) < 0)
		*size = 66000;

	*data = malloc (sizeof (char) * *size);
	if (!*data) {
		gp_context_error (context, _("Could not allocate "
			"%i byte(s) for downloading the picture."), *size);
		return (GP_ERROR_NO_MEMORY);
	}

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_GET;
	cmd[2] = 2;
	cmd[3] = 0;
	cmd[4] = i;
	cmd[5] = (i >> 8);

	CRF (fuji_transmit (camera, cmd, 6, *data, size, context), *data);
	GP_DEBUG ("Download of picture completed (%i byte(s)).", *size);

	return (GP_OK);
}

int
fuji_date_get (Camera *camera, FujiDate *date, GPContext *context)
{
	unsigned char cmd[4], buf[1024];
	unsigned int buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_DATE_GET;
	cmd[2] = 0;
	cmd[3] = 0;

	CR (fuji_transmit (camera, cmd, 4, buf, &buf_len, context));
	CLEN (buf_len, 14);

	date->year  = buf[0] * 1000 + buf[1] * 100 + buf[ 2] * 10 + buf[ 3];
	date->month =                                buf[ 4] * 10 + buf[ 5];
	date->day   =                                buf[ 6] * 10 + buf[ 7];
	date->hour  =                                buf[ 8] * 10 + buf[ 9];
	date->min   =                                buf[10] * 10 + buf[11];
	date->sec   =                                buf[12] * 10 + buf[13];

	return (GP_OK);
}

int
fuji_avail_mem (Camera *camera, unsigned int *avail_mem, GPContext *context)
{
	unsigned char cmd[4], buf[1024];
	unsigned int buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_AVAIL_MEM;
	cmd[2] = 0;
	cmd[3] = 0;

	CR (fuji_transmit (camera, cmd, 4, buf, &buf_len, context));
	CLEN (buf_len, 4);

	*avail_mem = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

	return (GP_OK);
}

int
fuji_date_set (Camera *camera, FujiDate date, GPContext *context)
{
	unsigned char cmd[1024], buf[1024];
	unsigned int buf_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_DATE_SET;
	cmd[2] = 14;
	cmd[3] = 0;
	sprintf ((char*)cmd + 4, "%04d%02d%02d%02d%02d%02d", date.year, date.month,
		 date.day, date.hour, date.min, date.sec);

	CR (fuji_transmit (camera, cmd, 4, buf, &buf_len, context));

	return (GP_OK);
}

int
fuji_upload_init (Camera *camera, const char *name, GPContext *context)
{
	unsigned char cmd[1024], buf[1024];
	unsigned int buf_len = 0, cmd_len = 0;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_UPLOAD_INIT;
	cmd[2] = strlen (name);
	cmd[3] = 0;
	memcpy (cmd + 4, name, strlen (name));
	cmd_len = strlen (name) + 4;
	CR (fuji_transmit (camera, cmd, cmd_len, buf, &buf_len, context));
	CLEN (buf_len, 1);

	/* Check the response */
	switch (buf[0]) {
	case FUJI_ACK:
		break;
	case FUJI_NAK:
		gp_context_error (context, _("The camera does not "
			"accept '%s' as filename."), name);
		return (GP_ERROR);
	default:
		gp_context_error (context, _("Could not initialize upload "
			"(camera responded with 0x%02x)."), buf[0]);
		return (GP_ERROR);
	}

	return (GP_OK);
}

#define CHUNK_SIZE 512

int
fuji_upload (Camera *camera, const unsigned char *data,
	     unsigned int size, GPContext *context)
{
	unsigned char cmd[1024], c;
	unsigned int cmd_len, chunk_size, retries, i;

	/*
	 * The old driver did upload in chunks of 512 bytes. I don't know
	 * if this is optimal.
	 */
	cmd[0] = 0;
	cmd[1] = FUJI_CMD_UPLOAD;
	for (i = 0; i < size; i+= CHUNK_SIZE) {
		chunk_size = MIN (CHUNK_SIZE, size - i);
		cmd[2] = chunk_size;
		cmd[3] = (chunk_size >> 8);
		memcpy (cmd + 4, data + i, chunk_size);
		cmd_len = chunk_size + 4;

		retries = 0;
		while (1) {

			/* Give the user the possibility to cancel. */
			if (gp_context_cancel (context) ==
					GP_CONTEXT_FEEDBACK_CANCEL) {
				CR (fuji_reset (camera, context));
				return (GP_ERROR_CANCEL);
			}

			CR (fuji_send (camera, cmd, cmd_len,
				(i + CHUNK_SIZE < size) ? 0 : 1, context));

			/* Receive ACK (hopefully) */
			CR (gp_port_read (camera->port, (char *)&c, 1));
			switch (c) {
			case ACK:
				break;
			case NAK:

				/* Camera didn't like the command */
				if (++retries > 1) {
					gp_context_error (context,
						_("Camera rejected "
							"the command."));
					return (GP_ERROR);
				}
				continue;

			case EOT:

				/* Camera got fed up and reset itself. */
				gp_context_error (context, _("Camera reset "
							"itself."));
				return (GP_ERROR);

			default:
				gp_context_error (context, _("Camera sent "
					"unexpected byte 0x%02x."), c);
				return (GP_ERROR_CORRUPTED_DATA);
			}
			break;
		}
	}

	return (GP_OK);
}

int
fuji_set_speed (Camera *camera, FujiSpeed speed, GPContext *context)
{
	unsigned char cmd[5], buf[1024];
	unsigned int buf_len = 0;

	GP_DEBUG("Attempting to set speed to %i",speed);
	cmd[0] = 1;
	cmd[1] = FUJI_CMD_SPEED;
	cmd[2] = 1;
	cmd[3] = 0;
	cmd[4] = speed;
	CR (fuji_transmit (camera, cmd, 5, buf, &buf_len, context));
	CLEN (buf_len, 1);

	/* Check the response */
	switch (buf[0]) {
	case FUJI_ACK:
		break;
	case FUJI_NAK:
		gp_context_error (context, _("The camera does not "
			"support speed %i."), speed);
		return (GP_ERROR_NOT_SUPPORTED);
	default:
		gp_context_error (context, _("Could not set speed to "
			"%i (camera responded with %i)."), speed, buf[0]);
		return (GP_ERROR);
	}

	GP_DEBUG("Success with speed %i.",speed);

	/* Reset the connection */
	CR (fuji_reset (camera, context));

	return (GP_OK);
}

static int
fuji_reset (Camera *camera, GPContext *context)
{
	unsigned char c = EOT;

	CR (gp_port_write (camera->port, (char *)&c, 1));

	return (GP_OK);
}

#if 0

static int fuji_get_flash_mode (void)
{
	cmd0 (0, 0x30);
	return answer[4];
}

static int fuji_set_flash_mode (int mode)
{
	cmd1 (0, 0x32, mode);
	return answer[4];
}

static int fuji_picture_size (int i)
{
	cmd2 (0, FUJI_SIZE, i);
	return answer[4] + (answer[5] << 8) + (answer[6] << 16) + (answer[7] << 24);
}

static int charge_flash (void)
{
	cmd2 (0, FUJI_CHARGE_FLASH, 200);
	return answer[4];
}

static int take_picture (void)
{
	cmd0 (0, FUJI_CMD_TAKE);
	return answer[4] + (answer[5] << 8) + (answer[6] << 16) + (answer[7] << 24);
}

static int del_frame (int i)
{
	cmd2 (0, FUJI_CMD_DELETE, i);
	return answer[4];
}

int
fuji_get_cmds (Camera *camera, unsigned char *cmds, GPContext *context)
{
	unsigned char answer[0xff];
	unsigned int answer_len = 0xff;

	memset (cmds, 0, 0xff);

	CR (fuji_cmd0 (camera, 0, FUJI_CMD_CMDS_VALID, answer, &answer_len,
		       context));
	int i;
	DBG("Get command list");
	memset(fjd->has_cmd, 0, 256);
	if (cmd0 (0, FUJI_CMDS_VALID)) return ;
	for (i = 4; i < answer_len; i++)
	  fjd->has_cmd[answer[i]] = 1;
}

static int get_picture_info(int num,char *name,CameraPrivateLibrary *camdata){

          DBG("Getting name...");

	  strncpy(name,fuji_picture_name(num),100);

	  DBG2("%s\n",name);

	  /*
	   * To find the picture number, go to the first digit. According to
	   * recent Exif specs, n_off can be either 3 or 4.
	   */
	  if (camdata->has_cmd[FUJI_SIZE])   fuji_size=fuji_picture_size(num);
	  else {
	    fuji_size=70000;  /* this is an overestimation for DS7 */
	    DBG2("Image size not obtained, guessing %d",fuji_size);
	  };
	  return (fuji_size);
};

static void get_picture_list (FujiData *fjd)
{
	int i, n_off;
	char *name;
	struct stat st;

	pictures = fuji_nb_pictures();
	maxnum = 100;
	free(pinfo);
	pinfo = calloc(pictures+1, sizeof(struct pict_info));
	if (pinfo==NULL) {
	  DBG("Allocation error");
	};

	for (i = 1; i <= pictures; i++) {
	        DBG("Getting name...");

	        name = strdup(fuji_picture_name(i));
	        pinfo[i].name = name;

		DBG2("%s\n",name);

		/*
		 * To find the picture number, go to the first digit. According to
		 * recent Exif specs, n_off can be either 3 or 4.
		 */
		n_off = strcspn(name, "0123456789");
		if ((pinfo[i].number = atoi(name+n_off)) > maxnum)
			maxnum = pinfo[i].number;
		if (fjd->has_cmd[FUJI_SIZE])	pinfo[i].size = fuji_picture_size(i);
		else pinfo[i].size=65000;
		pinfo[i].ondisk = !stat(name, &st);
	}
}

static int download_picture(int n,int thumb,CameraFile *file,CameraPrivateLibrary *fjd)
{
	clock_t t1, t2;
	char name[100];
	long int file_size;
	char *data;

	DBG3("download_picture: %d,%s",n,thumb?"thumb":"pic");

	if (!thumb) {
	        fuji_size=get_picture_info(n,name,fjd);
		DBG3("Info %3d   %12s ", n, name);
	}
	else fuji_size=10500;  /* Probably not same for all cams, better way ? */

	DBG2("calling download for %d bytes",fuji_size);

	t1 = times(0);
	if (cmd2(0, (thumb?0:0x02), n)==-1) return(GP_ERROR);

	DBG3("Download :%4d actual bytes vs expected %4d bytes\n",
	     fuji_count ,fuji_size);

	/* file->bytes_read=fuji_count; */
	/* add room for thumb-decode */
	/* file->size=fuji_count+(thumb?128:0); */
	file_size=fuji_count+(thumb?128:0);

	data=malloc(file_size);

	if (data==NULL) return(GP_ERROR);

	memcpy(data,fuji_buffer,fuji_count);

	gp_file_set_data_and_size(file,data,file_size);

	t2 = times(0);

	DBG2("%3d seconds, ", (int)(t2-t1) / CLK_TCK);
	DBG2("%4d bytes/s\n",
		fuji_count * CLK_TCK / (int)(t2-t1));

	if (fjd->has_cmd[17]&&!thumb){
	if (!thumb&&(fuji_count != fuji_size)){
	/* gp_frontend_status(NULL,"Short picture file--disk full or quota exceeded\n"); */
	    return(GP_ERROR);
	  };
	};
	return(GP_OK);

}

static int fuji_free_memory (void)
{
	cmd0 (0, FUJI_CMD_FREE_MEM);
	return answer[5] + (answer[6]<<8) + (answer[7]<<16) + (answer[8]<<24);
}

static int upload_pic (const char *picname)
{
	unsigned char buffer[516];
	const char *p;
	struct stat st;
	FILE *fd;
	int c, last, len, free_space;

	fd = fopen(picname, "r");
	if (fd == NULL) {
	  /* update_status("Cannot open file for upload"); */
		return 0;
	}
	if (fstat(fileno(fd), &st) < 0) {
		DBG("fstat");
		return 0;
	}
	free_space = fuji_free_memory();
	fprintf(stderr, "Uploading %s (size %d, available %d bytes)\n",
		picname, (int) st.st_size, free_space);
	if (st.st_size > free_space) {
		fprintf(stderr, "  not enough space\n");
		return 0;
	}
	if ((p = strrchr(picname, '/')) != NULL)
		picname = p+1;
	if (strlen(picname) != 12 || memcmp(picname,"DSC",3) || memcmp(picname+8,".JPG",4)) {
		picname = auto_rename();
		fprintf(stderr, "  file renamed %s\n", picname);
	}
	buffer[0] = 0;
	buffer[1] = 0x0F;
	buffer[2] = 12;
	buffer[3] = 0;
	memcpy(buffer+4, picname, 12);
	cmd(16, buffer);
	if (answer[4] != 0) {
		fprintf(stderr, "  rejected by the camera\n");
		return 0;
	}
	buffer[1] = 0x0E;
	while(1) {
		len = fread(buffer+4, 1, 512, fd);
		if (!len) break;
		buffer[2] = len;
		buffer[3] = (len>>8);
		last = 1;
		if ((c = getc(fd)) != EOF) {
			last = 0;
			ungetc(c, fd);
		}
		if (!last && interrupted) {
			fprintf(stderr, "Interrupted!\n");
			exit(1);
		}
again:
		send_packet(4+len, buffer, last);
		/*wait_for_input(1);*/
		if (get_byte() == 0x15)
			goto again;
	}
	fclose(fd);
	fprintf(stderr, "  looks ok\n");
	return 1;
}

#endif
