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

#define CR(result)       {int r_mac=(result); if (r_mac<0) return r_mac;}
#define CRF(result,data) {int r_mac=(result); \
  if (r_mac<0) {free (data); return r_mac;}}

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
			target, len);			\
		return (GP_ERROR_CORRUPTED_DATA);	\
	}						\
}

static int
ricoh_send (Camera *camera, GPContext *context, unsigned char cmd,
	    unsigned char number,
	    const unsigned char *data, unsigned char len)
{
	unsigned char buf[6];
	unsigned int i, w, crc = 0;
	int timeout;

	/* First, make sure there is no data coming from the camera. */
	CR (gp_port_get_timeout (camera->port, &timeout));
	CR (gp_port_set_timeout (camera->port, 20));
	while (gp_port_read (camera->port, buf, 1) >= 0);
	CR (gp_port_set_timeout (camera->port, timeout));

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
			if (data[i] == 0x10) {
				i++;
				break;
			}
		}
		CR (gp_port_write (camera->port, data + w, i - w));
		if (data[i - 1] == 0x10)
			CR (gp_port_write (camera->port, "\x10", 1));
		w = i;
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

static int
ricoh_send_ack (Camera *camera, GPContext *context)
{
	CR (gp_port_write (camera->port, "\x10\x06", 2));
	return (GP_OK);
}

static int
ricoh_send_nack (Camera *camera, GPContext *context)
{
	CR (gp_port_write (camera->port, "\x10\x15", 2));
	return (GP_OK);
}

static int
ricoh_recv (Camera *camera, GPContext *context, unsigned char *cmd,
            unsigned char *number, unsigned char *data, unsigned char *len)
{
        unsigned char buf[6];
	unsigned char r, i, ii, last_dle;
	unsigned int crc = 0;

	for (ii = 0; ; ii++) {
		crc = 0;

		/*
		 * Get header (DLE,STX). If we receive (DLE,ACK), just
		 * drop that and read on.
		 */
		for (i = 0, buf[1] = ACK; i < 4; i++) {
			CR (gp_port_read (camera->port, buf, 2));
			if (buf[0] != DLE) {
				gp_context_error (context, _("We expected "
					"0x%x but received 0x%x. Please "
					"contact <gphoto-devel@gphoto.org>."),
						DLE, buf[0]);
				return (GP_ERROR_CORRUPTED_DATA);
			}
			if (buf[1] != ACK)
				break;
		}
		switch (buf[1]) {
		case STX:
			break;
		case NAK:
		default:
			gp_context_error (context, _("We expected "
				"0x%x but received 0x%x. Please "
				"contact <gphoto-devel@gphoto.org>."),
					STX, buf[1]);
			return GP_ERROR_CORRUPTED_DATA;
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
		last_dle = 0;
		while (r < *len) {
			CR (gp_port_read (camera->port, data + r, *len - r));
			if (last_dle) {
				r++;
				last_dle = 0;
			}
			for (i = r; i < *len; i++) {
				if (data[r] == DLE) {
					if (i + 1 != *len &&
					    data[r + 1] != DLE) {
						gp_context_error (context,
							_("Bad characters "
							"(0x%x, 0x%x). Please "
							"contact <gphoto-devel"
							"@gphoto.org>."),
							data[r], data[r + 1]);
						return (GP_ERROR_CORRUPTED_DATA);
					}
					memmove (&data[r], &data[r +1],
						 *len - i - 1);
					i++;
				}
				crc = updcrc (data[r], crc);
				if (i != *len)
					r++;
				else
					last_dle = 1;
			}
		}

		/* Get footer */
		CR (gp_port_read (camera->port, buf, 6));

		if ((buf[0] != DLE) || (buf[1] != ETX && buf[1] != ETB))
			return (GP_ERROR_CORRUPTED_DATA);

		/* CRC correct? If not, retry. */
		if ((buf[2] != (crc & 0xff)) ||
		    (buf[3] != (crc >> 8 & 0xff)) ||
		    (buf[4] != *len + 2)) {
			GP_DEBUG ("CRC error. Retrying...");
			CR (ricoh_send_nack (camera, context));
			continue;
		}

		/* Acknowledge the packet. */
		CR (ricoh_send_ack (camera, context));

		/* If camera is busy, try again (but at most 4 times). */
		if ((*len == 3) && (data[0] == 0x00) &&
				   (data[1] == 0x04) &&
				   (data[2] == 0xff)) {
			if (ii >= 4) {
				gp_context_error (context, _("Camera busy. "
					"If the problem persists, please "
					"contact <gphoto-devel@gphoto.org>."));
				return (GP_ERROR);
			}
			continue;
		}

		/* Everything is ok. Break out of loop. */
		break;
	}

	/* Sequence number */
	if (number)
		*number = buf[5];

	return (GP_OK);
}

static int
ricoh_transmit (Camera *camera, GPContext *context, unsigned char cmd,
                const unsigned char *data, unsigned char len,
                unsigned char *ret_data, unsigned char *ret_len)
{
        unsigned char ret_cmd;
	unsigned int r = 0;
	int result;
 
        while (1) {
                CR (ricoh_send (camera, context, cmd, 0, data, len));
                result = ricoh_recv (camera, context, &ret_cmd, NULL,
				     ret_data, ret_len);
		switch (result) {
		case GP_ERROR_TIMEOUT:
			if (++r > 2) {
				gp_context_error (context, _("Timeout "
					"even after 2 retries. Please "
					"contact <gphoto-devel@gphoto.org>."));
				return (GP_ERROR_TIMEOUT);
			}
			GP_DEBUG ("Timeout! Retrying...");
			continue;
		default:
			CR (result);
		}

		/* Check if the answer is for our command. */
		if (cmd != ret_cmd) {
			GP_DEBUG ("Commands differ (expected 0x%02x, "
				  "got 0x%02x)!", cmd, ret_cmd);
			if (++r > 2) {
				gp_context_error (context, _("Communication "
					"error even after 2 retries. Please "
					"contact <gphoto-devel@gphoto.org>."));
				return (GP_ERROR);
			}
			continue;
		}

		/* Check if the camera reported success. */
		if ((*ret_len >= 2) && (ret_data[0] == 0x00) &&
				       (ret_data[1] == 0x00))
			break;

		/* If camera is busy, try again (but at most 4 times). */
		if ((*ret_len == 3) && (ret_data[0] == 0x00) &&
				       (ret_data[1] == 0x04) &&
				       (ret_data[2] == 0xff)) {
			if (++r >= 4) {
				gp_context_error (context, _("Camera busy. "
					"If the problem persists, please "
					"contact <gphoto-devel@gphoto.org>."));
				return (GP_ERROR);
			}
			continue;
		}

		/*
		 * It could be that the camera is in the wrong mode to
		 * execute this command.
		 */
		if ((*ret_len == 2) && (ret_data[0] == 0x06) &&
				       (ret_data[1] == 0x00)) {
			gp_context_error (context, _("Camera is in wrong "
				"mode. Please contact "
				"<gphoto-devel@gphoto.org>."));
			return (GP_ERROR);
		}

		/* Invalid parameters? */
		if ((*ret_len == 2) && (ret_data[0] == 0x04) &&
				       (ret_data[1] == 0x00)) {
			gp_context_error (context, _("Camera did not "
				"accept the parameters. Please contact "
				"<gphoto-devel@gphoto.org>."));
			return (GP_ERROR);
		}

		gp_context_error (context, _("An unknown error occurred. "
			"Please contact <gphoto-devel@gphoto.org>."));
		return (GP_ERROR);
        }

	/* Success! We don't need the first two bytes any more. */
	*ret_len -= 2;
	if (*ret_len > 0)
		memmove (ret_data, ret_data + 2, *ret_len);

        return (GP_OK);
}

/* query what mode(play/record) the camera is in */
int
ricoh_get_mode (Camera *camera, GPContext *context, RicohMode *mode)
{
	unsigned char p[1], buf[0xff], len;

	GP_DEBUG ("Getting mode...");

	p[0] = 0x12;
	CR (ricoh_transmit (camera, context, 0x51, p, 1, buf, &len));
	C_LEN (context, len, 1);

	/* Mode */
	if (mode)
		*mode = buf[0];

	return (GP_OK);
}

int
ricoh_set_mode (Camera *camera, GPContext *context, RicohMode mode)
{
	unsigned char p[2], buf[0xff], len;

	GP_DEBUG ("Setting mode to %i...", mode);

	p[0] = 0x12;
	p[1] = mode;
	CR (ricoh_transmit (camera, context, 0x50, p, 2, buf, &len));
	C_LEN (context, len, 0);

	return (GP_OK);
}

int
ricoh_get_pic_size (Camera *camera, GPContext *context, unsigned int n,
		    unsigned long *size)
{
	unsigned char p[3], buf[0xff], len;

	GP_DEBUG ("Getting size of picture %i...", n);

	p[0] = 0x04;
	p[1] = n >> 0;
	p[2] = n >> 8;
	CR (ricoh_transmit (camera, context, 0x95, p, 3, buf, &len));
	C_LEN (context, len, 2);

	if (size)
		*size = (buf[0] << 8) | buf[1] << 0;

	return (GP_OK);
}

int
ricoh_get_pic_date (Camera *camera, GPContext *context, unsigned int n,
		    time_t *date)
{
	unsigned char p[3], buf[0xff], len;
	struct tm time;

	GP_DEBUG ("Getting date of picture %i...", n);

	p[0] = 0x03;
	p[1] = n >> 0;
	p[2] = n >> 8;
	CR (ricoh_transmit (camera, context, 0x95, p, 3, buf, &len));
	C_LEN (context, len, 6);

	if (date) {
		time.tm_year = buf[0] + 2000;
		time.tm_mon  = buf[1];
		time.tm_mday = buf[2];
		time.tm_hour = buf[3];
		time.tm_min  = buf[4];
		time.tm_sec  = buf[5];
		time.tm_isdst = -1;
		*date = mktime (&time);
	}

	return (GP_OK);
}

int
ricoh_get_pic_name (Camera *camera, GPContext *context, unsigned int n,
		    const char **name)
{
	unsigned char p[3], len;
	static unsigned char buf[0xff];

	GP_DEBUG ("Getting name of picture %i...", n);

	p[0] = 0x00;
	p[1] = n >> 0;
	p[2] = n >> 8;
	CR (ricoh_transmit (camera, context, 0x95, p, 3, buf, &len));

	if (name && *name) {
		*name = buf;
		buf[len] = '\0';
	}

	return (GP_OK);
}

int
ricoh_get_pic_memo (Camera *camera, GPContext *context, unsigned int n,
		    const char **memo)
{
	unsigned char p[3], len;
	static unsigned char buf[0xff];

	GP_DEBUG ("Getting memo of picture %i...", n);

	p[0] = 0x02;
	p[1] = n >> 0;
	p[2] = n >> 8;
	CR (ricoh_transmit (camera, context, 0x95, p, 3, buf, &len));

	if (memo && *memo) {
		*memo = buf;
		buf[len] = '\0';
	}

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
	unsigned char p[2], buf[0xff], len;

	GP_DEBUG ("Getting number of pictures...");

	p[0] = 0x00;
	p[1] = 0x01;
	CR (ricoh_transmit (camera, context, 0x51, p, 2, buf, &len));
	C_LEN (context, len, 2);

	if (n)
		*n = (buf[1] << 8) | buf[0];

	return (GP_OK);
}

int
ricoh_set_speed (Camera *camera, GPContext *context, RicohSpeed speed)
{
	unsigned char p[1], buf[0xff], len;

	p[0] = speed;
	CR (ricoh_transmit (camera, context, 0x32, p, 1, buf, &len));
	C_LEN (context, len, 0);

	/* Wait for camera to switch speed. */
	sleep (1);

	return (GP_OK);
}

int
ricoh_connect (Camera *camera, GPContext *context, RicohModel *model)
{
	unsigned char p[3], buf[0xff], len;

	p[0] = 0x00;
	p[1] = 0x00;
	p[2] = 0x00;
	CR (ricoh_transmit (camera, context, 0x31, p, 3, buf, &len));
	C_LEN (context, len, 4);

	/* Model */
	if (model)
		*model = (buf[0] << 8) | buf[1];

	/* No idea what buf[2] and buf[3] contain. */

	return (GP_OK);
}

int
ricoh_disconnect (Camera *camera, GPContext *context)
{
	unsigned char cmd, buf[0xff], len;

	CR (ricoh_send (camera, context, 0x37, 0, NULL, 0));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, 0x37);
	C_LEN (context, len, 2);

	return (GP_OK);
}

unsigned char header[236] = {
	0x49,0x49,0x2a,0x00,0x08,0x00,0x00,0x00,0x0c,0x00,0x00,0x01,0x03,0x00,
	0x01,0x00,0x00,0x00,0x50,0x00,0x00,0x00,0x01,0x01,0x03,0x00,0x01,0x00,
	0x00,0x00,0x3c,0x00,0x00,0x00,0x02,0x01,0x03,0x00,0x03,0x00,0x00,0x00,
	0x9e,0x00,0x00,0x00,0x03,0x01,0x03,0x00,0x01,0x00,0x00,0x00,0x01,0x00,
	0x00,0x00,0x06,0x01,0x03,0x00,0x01,0x00,0x00,0x00,0x06,0x00,0x00,0x00,
	0x11,0x01,0x04,0x00,0x01,0x00,0x00,0x00,0xec,0x00,0x00,0x00,0x15,0x01,
	0x03,0x00,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x17,0x01,0x04,0x00,
	0x01,0x00,0x00,0x00,0x80,0x25,0x00,0x00,0x1c,0x01,0x03,0x00,0x01,0x00,
	0x00,0x00,0x01,0x00,0x00,0x00,0x11,0x02,0x05,0x00,0x03,0x00,0x00,0x00,
	0xa4,0x00,0x00,0x00,0x12,0x02,0x03,0x00,0x02,0x00,0x00,0x00,0x02,0x00,
	0x01,0x00,0x14,0x02,0x05,0x00,0x06,0x00,0x00,0x00,0xbc,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0xe0,0xd0,0x22,0x13,
	0x00,0x00,0x00,0x40,0x80,0x68,0x91,0x25,0x00,0x00,0x00,0x40,0xa8,0xc6,
	0x4b,0x07,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
	0x00,0x00,0xe0,0x1f,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x10,0x00,0x00,
	0x20,0x00,0x00,0x00,0xe0,0x1f,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x20,0x00,0x00,0x00,0xe0,0x1f,0x00,0x00,0x20,0x00
};

int
ricoh_get_pic (Camera *camera, GPContext *context, unsigned int n,
	       RicohFileType type, unsigned char **data, unsigned int *size)
{
	unsigned char p[2], cmd, buf[0xff], len;
	unsigned int r, header_len;
	RicohMode mode;

	GP_DEBUG( "Getting image %i as %i...", n, type );
	
	/* Put camera into play mode, if not already */
	CR (ricoh_get_mode (camera, context, &mode));
	if (mode != RICOH_MODE_PLAY)
	    CR (ricoh_set_mode (camera, context, RICOH_MODE_PLAY));

	/* Send picture number */
	p[0] = n >> 0;
	p[1] = n >> 8;
	CR (ricoh_send (camera, context, (unsigned char) type, 0, p, 2));
	CR (ricoh_recv (camera, context, &cmd, NULL, buf, &len));
	C_CMD (context, cmd, (unsigned char) type);
	C_LEN (context, len, 18);

	/*
	 * Allocate the necessary memory. Note that previews are 80x60 pixels
	 * raw TIFF data, we therefore need to allocate some extra memory
	 * for the TIFF header.
	 */
	header_len = (type == RICOH_FILE_TYPE_PREVIEW) ? sizeof (header) : 0;
	*size = buf[17] << 24 | buf[16] << 16 | buf[15] << 8 | buf[14];
	*size += header_len;
	*data = malloc (*size);
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	/*
	 * Receive data.
	 * r ... number of bytes received
	 */
	for (r = 0; r < *size; r += len) {
		CRF (ricoh_recv (camera, context, &cmd, NULL,
				 *data + header_len + r, &len), *data);
		C_CMD (context, cmd, 0xa2);
	}

	/* In case of previews, copy over the TIFF header. */
	if (type == RICOH_FILE_TYPE_PREVIEW)
		memcpy (data, header, header_len);

	return (GP_OK);
}

/* get the date the camera is set to */
int
ricoh_get_cam_date (Camera *camera, GPContext *context, time_t *date)
{
	unsigned char p[1], buf[0xff], len;
	struct tm time;

	p[0] = 0xa;
	CR (ricoh_transmit (camera, context, 'Q', p, 1, buf, &len));

	/* the camera only supplies 2 digits for year, so I will
	 * make the assumption that if less than 90, then it is
	 * year 2000 or greater */
	 time.tm_year = ((buf[1] & 0xf0) >> 4) * 10 + (buf[1] & 0xf);
	 if(time.tm_year < 90) time.tm_year += 100;
	 time.tm_mon =  ((buf[2] & 0xf0) >> 4) * 10 + (buf[2] & 0xf) - 1;
	 time.tm_mday = ((buf[3] & 0xf0) >> 4) * 10 + (buf[3] & 0xf);
	 time.tm_hour = ((buf[4] & 0xf0) >> 4) * 10 + (buf[4] & 0xf);
	 time.tm_min =  ((buf[5] & 0xf0) >> 4) * 10 + (buf[5] & 0xf);
	 time.tm_sec =  ((buf[6] & 0xf0) >> 4) * 10 + (buf[6] & 0xf);
	 time.tm_isdst = -1;
	 *date = mktime(&time);

	 return (GP_OK);
}

/* get the cameras memory size */
int
ricoh_get_cam_mem (Camera *camera, GPContext *context, int *size)
{
	unsigned char p[2], buf[0xff], len;

	p[0] = 0x00;
	p[1] = 0x05;
	CR (ricoh_transmit (camera, context, 'Q', p, 2, buf, &len));

	if (size)
		*size = buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0];

	return (GP_OK);
}

/* get the cameras available memory size */
int
ricoh_get_cam_amem (Camera *camera, GPContext *context, int *size)
{
	unsigned char p[2], buf[0xff], len;

	p[0] = 0x00;
	p[1] = 0x06;
	CR (ricoh_transmit (camera, context, 'Q', p, 2, buf, &len));

	if (size)
		*size = buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0];

	return (GP_OK);
}

/* get the camera ID aka copyright message */ 
int
ricoh_get_cam_id (Camera *camera, GPContext *context, const char **cam_id)
{
	unsigned char p[1], len;
	static char buf[1024];

	p[0] = 0xf;
	CR (ricoh_transmit (camera, context, 'Q', p, 1, buf, &len));

	if (cam_id && *cam_id) {
		*cam_id = buf;
		buf[len] = '\0';
	}

	return (GP_OK);
}

int
ricoh_take_pic (Camera *camera, GPContext *context)
{
	unsigned char p[1];
	RicohMode mode;

	/* Put camera into record mode, if not already */
	CR (ricoh_get_mode (camera, context, &mode));
	if (mode != RICOH_MODE_RECORD)
		CR (ricoh_set_mode (camera, context, RICOH_MODE_RECORD));

	p[0] = 0x01;
	CR (ricoh_send (camera, context, 0x60, 0x00, p, 1));

	return (GP_OK);
}
