/* ricoh.c
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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

#include <config.h>
#include "ricoh.h"

#include <stdlib.h>
#include <string.h>

#include <gphoto2-port-log.h>

#include "crctab.h"

#define GP_MODULE "ricoh"

#define STX 0x02 /* start of text */
#define ETX 0x03 /* end of text */
#define ACK 0x06 /* acknowledge */
#define NAK 0x15 /* negative acknowledge */
#define ETB 0x17 /* end of transmission block */
#define DLE 0x10 /* datalink escape */

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
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

#define CR(result)       {int r=(result); if (r<0) return r;}
#define CRF(result,data) {int r=(result); if (r<0) {free (data); return r;}}

#define C_CMD(context,cmd,target) 			\
{							\
	if (cmd != target) {				\
		gp_context_error (context, _("Expected %i, got %i. Please "\
			"report this error to <gphoto-devel@gphoto.org>."),\
			cmd, target);			\
		return (GP_ERROR_CORRUPTED_DATA);	\
	}						\
}
#define C_LEN(context,len,target)			\
{							\
	if (len != target) {				\
		gp_context_error (context, _("Expected "\
			"%i bytes, got %i. "		\
			"Please report this error to "	\
			"<gphoto-devel@gphoto.org>."),	\
			len, target);			\
		return (GP_ERROR_CORRUPTED_DATA);	\
	}						\
}

static int
ricoh_send (Camera *camera, GPContext *context, unsigned char cmd,
	    unsigned char number,
	    const unsigned char *data, unsigned char len)
{
	char buf[6];
	unsigned int i, w, crc = 0;

	/* Write header */
	buf[0] = DLE;
	buf[1] = STX;
	buf[2] = cmd;
	buf[3] = len;
	CR (gp_port_write (camera->port, buf, 4));
	crc = updcrc (cmd, crc);
	crc = updcrc (len, crc);

	/*
	 * Write data (escape DLE).
	 *
	 * w ... bytes written
	 */
	w = 0;
	while (w < len) {
		for (i = w; i < len; i++) {
			crc = updcrc (data[i], crc);
			if (data[i] == 0x10)
				break;
		}
		CR (gp_port_write (camera->port, data + w, i - w + 1));
		if (data[i] == 0x10)
			CR (gp_port_write (camera->port, "\x10", 1));
		w = i + 1;
	}

	/* Write footer */
	buf[0] = DLE;
	buf[1] = ETX;
	buf[2] = crc >> 0;
	buf[3] = crc >> 8;
	buf[4] = len + 2;
	buf[5] = number;
	CR (gp_port_write (camera->port, buf, 6));

	return (GP_OK);
}

#if 0
static int
ricoh_send_ack (Camera *camera, GPContext *context)
{
	CR (gp_port_write (camera->port, "\x10\x06", 2));
	return (GP_OK);
}
#endif

static int
ricoh_send_nack (Camera *camera, GPContext *context)
{
	CR (gp_port_write (camera->port, "\x10\x06", 2));
	return (GP_OK);
}

static int
ricoh_recv (Camera *camera, GPContext *context, unsigned char *cmd,
            unsigned char *number, unsigned char *data, unsigned char *len)
{
        char buf[6];
	unsigned char r, i;
	unsigned int crc = 0;

	/* Get header */
	CR (gp_port_read (camera->port, buf, 2));
	if (buf[0] != DLE)
		return (GP_ERROR_CORRUPTED_DATA);
	switch (buf[1]) {
	case ACK:
		return (GP_OK);
	case STX:
		break;
	case NAK:
	default:
		return (GP_ERROR_CORRUPTED_DATA);
	}
	CR (gp_port_read (camera->port, cmd, 1));
	CR (gp_port_read (camera->port, len, 1));
	crc = updcrc (*cmd, crc);
	crc = updcrc (*len, crc);

	/*
	 * Get data (check for DLEs).
	 * r ... number of bytes received
	 */
	r = 0;
	while (r < *len) {
		CR (gp_port_read (camera->port, data + r, *len - r));
		for (i = r; i < *len; i++) {
			crc = updcrc (data[i], crc);
			if (data[i] == 0x10) {
				if (data[i + 1] != 0x10)
					return (GP_ERROR_CORRUPTED_DATA);
				memmove (&data[i], &data[i + 1], *len - i - 1);
				i++;
			}
		}
	}

        /* Get footer */
        CR (gp_port_read (camera->port, buf, 6));

        if ((buf[0] != DLE) ||
            (buf[1] != ETX))
                return (GP_ERROR_CORRUPTED_DATA);

        /* CRC correct? */
        if ((buf[2] != crc >> 0) ||
            (buf[3] |= crc >> 8) ||
	    (buf[4] != *len + 2)) {
                GP_DEBUG ("CRC error. Retrying...");
		CR (ricoh_send_nack (camera, context));
		CR (ricoh_recv (camera, context, cmd, number, data, len));
	}

	/* Sequence number */
	if (number)
		*number = buf[5];
	else if (buf[5])
		return (GP_ERROR_CORRUPTED_DATA);

        return (GP_OK);
}

int
ricoh_get_mode (Camera *camera, GPContext *context, RicohMode *mode)
{
	unsigned char p[2], cmd, buf[0xff], len;

	GP_DEBUG ("Getting mode...");

	p[0] = 0x12;
	p[1] = 0x00;
	CR (ricoh_send (camera, context, 0x51, 0, p, 2));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x51);
	C_LEN (context, len, 4);

	/* No idea what buf[0] and buf[1] is for. */

	/* Mode */
	*mode = buf[2];

	/* What is buf[3]? */

	return (GP_OK);
}

int
ricoh_set_mode (Camera *camera, GPContext *context, RicohMode mode)
{
	unsigned char p[2], cmd, buf[0xff], len;

	GP_DEBUG ("Setting mode to %i...", mode);

	p[0] = 0x12;
	p[1] = mode;
	CR (ricoh_send (camera, context, 0x50, 0, p, 2));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x50);
	C_LEN (context, len, 0);

	return (GP_OK);
}

int
ricoh_get_size (Camera *camera, GPContext *context, unsigned int n,
		unsigned long *size)
{
	unsigned char p[3], cmd, buf[0xff], len;

	GP_DEBUG ("Getting size of picture %i...", n);

	p[0] = 0x04;
	p[1] = n >> 0;
	p[2] = n >> 8;
	CR (ricoh_send (camera, context, 0x95, 0, p, 3));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x95);
	C_LEN (context, len, 4);

	*size = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[1] << 0;

	return (GP_OK);
}

int
ricoh_get_date (Camera *camera, GPContext *context, unsigned int n,
		time_t *date)
{
	unsigned char p[3], cmd, buf[0xff], len;

	GP_DEBUG ("Getting date of picture %i...", n);

	p[0] = 0x03;
	p[1] = n >> 0;
	p[2] = n >> 8;
	CR (ricoh_send (camera, context, 0x95, 0, p, 3));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x95);
	C_LEN (context, len, 6);

	return (GP_OK);
}

int
ricoh_del_pic (Camera *camera, GPContext *context, unsigned int n)
{
	unsigned char p[2], cmd, buf[0xff], len;

	GP_DEBUG ("Deleting picture %i...", n);

	/* Put camera in delete mode */
	CR (ricoh_send (camera, context, 0x97, 0, NULL, 0));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x97);
	C_LEN (context, len, 0);

	/* Find and send picture to delete */
	p[0] = n >> 0;
	p[1] = n >> 8;
	CR (ricoh_send (camera, context, 0x93, 0, p, 2));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x93);
	C_LEN (context, len, 0);
	CR (ricoh_send (camera, context, 0x92, 0, p, 2));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x92);
	C_LEN (context, len, 0);

	return (GP_OK);
}

int
ricoh_get_num (Camera *camera, GPContext *context, unsigned int *n)
{
	unsigned char p[2], cmd, buf[0xff], len;

	GP_DEBUG ("Getting number of pictures...");

	p[0] = 0x00;
	p[1] = 0x01;
	CR (ricoh_send (camera, context, 0x51, 0, p, 2));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x51);
	C_LEN (context, len, 1);

	*n = buf[0];

	return (GP_OK);
}

int
ricoh_set_speed (Camera *camera, GPContext *context, RicohSpeed speed)
{
	unsigned char p[1], cmd, buf[0xff], len;

	p[0] = speed;
	CR (ricoh_send (camera, context, 0x32, 0, p, 1));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x32);
	C_LEN (context, len, 0);

	return (GP_OK);
}

int
ricoh_ping (Camera *camera, GPContext *context, RicohModel *model)
{
	unsigned char p[3], cmd, buf[0xff], len;

	p[0] = 0x00;
	p[1] = 0x00;
	p[2] = 0x00;
	CR (ricoh_send (camera, context, 0x31, 0, p, 3));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x31);
	C_LEN (context, len, 6);

	/* No idea what buf[0] and buf[1] contain. */

	/* Model */
	if (model)
		*model = buf[2] << 8 | buf[3];

	/* No idea what buf[4] and buf[5] contain. */

	return (GP_OK);
}

int
ricoh_bye (Camera *camera, GPContext *context)
{
	unsigned char cmd, buf[0xff], len;

	CR (ricoh_send (camera, context, 0x37, 0, NULL, 0));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x37);
	C_LEN (context, len, 0);

	return (GP_OK);
}

int
ricoh_get_pic (Camera *camera, GPContext *context, unsigned int n,
	       unsigned char **data, unsigned int *size)
{
	unsigned char p[2], cmd, buf[0xff], len, r;

	/* Put camera into play mode. */
	CR (ricoh_set_mode (camera, context, RICOH_MODE_PLAY));

	/* Send picture number */
	p[0] = n >> 0;
	p[1] = n >> 8;
	CR (ricoh_send (camera, context, 0xa0, 0, p, 2));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0xa0);
	C_LEN (context, len, 18);

	*size = buf[17] << 24 | buf[16] << 16 | buf[15] << 8 | buf[14];
	*data = malloc (*size);
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	/*
	 * Receive data.
	 * r ... number of bytes received
	 */
	for (r = 0; r < *size; r += len) {
		CRF (ricoh_recv (camera, context, &cmd, NULL,
				 *data + r, &len), data);
		C_CMD (context, cmd, 0xa0);
	}

	return (GP_OK);
}
