/* fuji.c:
 * 
 * Fuji Camera library for the gphoto project.
 * 
 * (C) 2001 Matthew G. Martin <matt.martin@ieee.org>
 *     2002 Lutz Müller <lutz@users.sourceforge.net>
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
 *  Copyright (C) 1998 Matthew G. Martin

 *  Some of which was derived from get_ds7 , a Perl Language library
 *  Copyright (C) 1997 Mamoru Ohno
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <config.h>
#include "fuji.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gphoto2-port-log.h>

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

#define GP_MODULE "fuji"

#define CR(result)    {int r = (result); if (r < 0) return (r);}
#define CRF(result,d) {int r = (result); if (r < 0) {free (d); return (r);}}
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

#if 0

#define FUJI_GET_PICCOUNT	0x0b
#define FUJI_CMD_TAKE		0x27
#define FUJI_CMD_DELETE		0x19
#define FUJI_CHARGE_FLASH	0x34
#define FUJI_PREVIEW		0x64


/**
 * Gets a byte. Already gets rid of parity error generated sequences.
 */
static int
get_byte (GPPort *port)
{
	unsigned char c;

	CR(gp_port_read(port, &c, 1));
	if (c < 255)
		return c;
	CR(gp_port_read(port, &c, 1));
	if (c == 255)
		return c;	/* escaped '\377' */

	if (c != 0) {
		fprintf(stderr,"get_byte: impossible escape sequence following 0xFF\n");
	}
	/* Otherwise, it's a parity or framing error, drop 1 byte. */
	CR(gp_port_read(port, &c, 1));
	return GP_ERROR_IO;
}

static int
put_byte (GPPort *port, char c)
{
    return gp_port_write(port, &c, 1);
}

static int
send_packet (GPPort *port, unsigned char *data, int len, int last)
{
	unsigned char *p, *end, buff[3];
	int check;

	last = last ? ETX : ETB;
	check = last;
	end = data + len;
	for (p = data; p < end; p++)
		check ^= *p;

	/* Start of frame */
	buff[0] = ESC;
	buff[1] = STX;

	CR(gp_port_write(port, buff, 2));

	/* Data */
	/* This is not so good if we have more than one esc. char */
	for (p = data; p < end; p++)
		if (*p == ESC) {
		    	char c;
			/* Escape the last character */
		        CR(gp_port_write(port,data,p-data+1)); /* send up to the ESC*/
			data = p+1;

			c = ESC;/* send another esc*/
			CR(gp_port_write(port,&c,1));
		}
	CR(gp_port_write(port, data, end-data));
	/* End of frame */
	buff[1] = last;
	buff[2] = check;
	CR(gp_port_write(port, buff, 3));
	return GP_OK;
}

static int
read_packet (GPPort *port, char **packet, int *size )
{
	unsigned char answerheader[4];
	unsigned char *p = answerheader;
	int c, cnt, check, incomplete, answerlen = 0;

	if ((get_byte(port) != ESC) || (get_byte(port) != STX)) {
bad_frame:
		/* drain input */
		while (get_byte(port) >= 0)
			continue;
		return GP_ERROR_IO;
	}

	check = 0;cnt = 0;
	while(1) {
		if ((c = get_byte(port)) < 0)
			goto bad_frame;
		if (c == ESC) {
			if ((c = get_byte(port)) < 0)
				goto bad_frame;
			if (c == ETX || c == ETB) {
				incomplete = (c == ETB);
				break;
			}
		}
		*p++ = c;
		cnt++;
		if (cnt == 4) {
			answerlen = (answerheader[2] | (answerheader[3]<<8));
			*packet = malloc(answerlen+4);
			*size = answerlen+4;
			memcpy(*packet,answerheader, 4);
			p = (*packet)+4;
		}
		if ((cnt > 4) && (cnt > answerlen+4)) {
			fprintf(stderr,"Too much data (only expecting %d byte)!\n",cnt);
			goto bad_frame;
		}
		check ^= c;
	}
	/* Append a sentry '\0' at the end of the buffer, for the convenience
	   of C programmers */
	*p = '\0';
	check ^= c;
	c = get_byte(port);
	if (c != check)
		return -1;
	/* Return 0 for the last packet, 1 otherwise */
	return incomplete;
}

static int
cmd (	Camera *camera, GPContext *context,
	unsigned char *cmd, int cmdlen, 
	char **data, int *len
) {
	int i, c, timeout=50;
	int	xsize=0;
	char	*xdata = NULL;

	fprintf(stderr,"CMD $%X called ",cmd[1]);	

	/* Some commands require large timeouts */
	switch (cmd[1]) {
	  case FUJI_CMD_TAKE:	/* Take picture */
	  case FUJI_CHARGE_FLASH:	/* Recharge the flash */
	  case FUJI_PREVIEW:	/* Take preview */
	    timeout = 12;
	    break;
	  case FUJI_CMD_DELETE:	/* Erase a picture */
	    timeout = 1;
	    break;
	}

	for (i = 0; i < 3; i++) { /* Three tries to send cmd. */
		send_packet(camera->port, cmd, cmdlen, 1);
		c = get_byte(camera->port);
		if (c == ACK)
			goto rd_pkt;
		if (c != NAK){
			if(fuji_ping(camera, context))
			    return(GP_ERROR_IO);
		};
	}
	fprintf(stderr,"Command error, aborting.\n");
	return(-1);

rd_pkt:
	/*wait_for_input(timeout);*/
	for (i = 0; i < 3; i++) {
	    	char *answer;
		int answerlen;
		if (i) put_byte(camera->port,NAK);
		c = read_packet(camera->port, &answer, &answerlen);
		if (c < 0)
			continue; /* go and retry */
		put_byte(camera->port,ACK);

		/* Skip 4 byte */
		answerlen-=4;
		xdata = realloc(xdata, xsize+answerlen);
		memcpy(xdata+xsize, answer, answerlen);
		xsize+=answerlen;
		free(answer);answer=NULL;

		/* More packets ? */
		if (c != 0)
			goto rd_pkt;
		*len = xsize;
		*data = xdata;
		return 0;
	}
	fprintf(stderr,"Cannot receive answer, aborting.");
	return(GP_ERROR_IO);
}

/* Zero byte command */
static int
cmd0(	
	Camera *camera, GPContext *context,
	int c0, int c1, char **data, int *len
) {
	unsigned char b[4];

	b[0] = c0; b[1] = c1;
	b[2] = b[3] = 0;
	return cmd(camera, context, b, 4, data, len);
}


/* One byte command */
static int
cmd1(
	Camera *camera, GPContext *context,
	int c0, int c1, int arg,
	char **data, int *len
) {
	unsigned char b[5];

	b[0] = c0; b[1] = c1;
	b[2] =  1; b[3] =  0;
	b[4] = arg;
	return cmd(camera, context, b, 5,  data, len);
}

/* Two byte command */
static int cmd2(
	Camera *camera, GPContext *context, int c0, int c1, int arg,
	char **data, int *len
) {
	unsigned char b[6];

	b[0] = c0; b[1] = c1;
	b[2] =  2; b[3] =  0;
	b[4] = arg; b[5] = arg>>8;
	return cmd(camera, context, b, 6, data, len);
}
#endif

int
fuji_ping (Camera *camera, GPContext *context)
{
	unsigned char b;
	unsigned int i;
	int r;

	GP_DEBUG ("Pinging camera...");

	/* Drain input */
	while (gp_port_read (camera->port, &b, 1) >= 0);

	i = 0;
	while (1) {
		b = ENQ;
		CR (gp_port_write (camera->port, &b, 1));
		r = gp_port_read (camera->port, &b, 1);
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
        CR (gp_port_write (camera->port, b, 2));

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
        CR (gp_port_write (camera->port, b, cmd_len));

        /* Send footer */
        b[0] = ESC;
        b[1] = (last ? ETX : ETB);
        b[2] = check;
        CR (gp_port_write (camera->port, b, 3));

	return (GP_OK);
}

static int
fuji_recv (Camera *camera, unsigned char *buf, unsigned int *buf_len,
	   unsigned char *last, GPContext *context)
{
	unsigned char b[1024], check = 0;
	int i;

	/* Read the header. The checksum covers the size, too. */
	CR (gp_port_read (camera->port, b, 4));
	if ((b[0] != ESC) || (b[1] != STX)) {
		gp_context_error (context, _("Received unexpected data "
			"(0x%02x, 0x%02x)."), b[0], b[1]);
		return (GP_ERROR_CORRUPTED_DATA);
	}
	*buf_len = ((b[2] << 8) | b[3]) - 2;
	check ^= b[2];
	check ^= b[3];
	GP_DEBUG ("We are expecting %i byte(s) data (excluding ESC quotes). "
		  "Let's read them...", *buf_len);

	/* Read the data. Unescape it. Calculate the checksum. */
	for (i = 0; i < *buf_len; i++) {
		CR (gp_port_read (camera->port, buf + i, 1));
		if (buf[i] == ESC) {
			CR (gp_port_read (camera->port, buf + i, 1));
			if (buf[i] != ESC) {
				gp_context_error (context,
					_("Wrong escape sequence: "
					"expected 0x%02x, got 0x%02x."),
					ESC, buf[i]);

				/* Dump the remaining data */
				while (gp_port_read (camera->port, b, 1) >= 0);

				return (GP_ERROR_CORRUPTED_DATA);
			}
		}
		check ^= buf[i];
	}

	/* Read the footer */
	CR (gp_port_read (camera->port, b, 3));
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
#if 0
		return (GP_ERROR_CORRUPTED_DATA);
#endif
	}

	return (GP_OK);
}

static int
fuji_transmit (Camera *camera, unsigned char *cmd, unsigned int cmd_len,
	       unsigned char *buf, unsigned int *buf_len, GPContext *context)
{
	unsigned char last = 0, c;
	unsigned int b_len = 1024;
	int r, retries = 0;

	/* Send the command */
	retries = 0;
	while (1) {

		/* Give the user the possibility to cancel. */
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);

		CR (fuji_send (camera, cmd, cmd_len, 1, context));

		/* Receive ACK (hopefully) */
		CR (gp_port_read (camera->port, &c, 1));
		switch (c) {
		case ACK:
			break;
		case NAK:

			/* Camera didn't like the command */
			if (++retries > 2) {
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

	/* Read the response */
	*buf_len = 0;
	retries = 0;
	while (!last) {
		r = fuji_recv (camera, buf + *buf_len, &b_len, &last, context);
		if (r < 0) {
			retries++;
			while (gp_port_read (camera->port, &c, 1) >= 0);
			if (++retries > 2)
				return (r);
			GP_DEBUG ("Retrying...");
			c = NAK;
			CR (gp_port_write (camera->port, &c, 1));
			continue;
		}

		/* Acknowledge this packet. */
		c = ACK;
		CR (gp_port_write (camera->port, &c, 1));
		*buf_len += b_len;
	}

	return (GP_OK);
}

int
fuji_get_cmds (Camera *camera, unsigned char *cmds, GPContext *context)
{
	unsigned char cmd[4], buf[1024];
	unsigned int i, buf_len;

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
	unsigned int buf_len;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_COUNT;
	cmd[2] = 0;
	cmd[3] = 0;
	CR (fuji_transmit (camera, cmd, 4, buf, &buf_len, context));

	CLEN (buf_len, 4);

	/* The first two bytes are the number of pictures. */
	*n = (buf[1] << 8) | buf[0];

	/*
	 * The next two bytes we don't know the meaning of. We have seen
	 * 0x02 0x00.
	 */

	return (GP_OK);

#if 0
    int numpics, gpr, len = 0;
    char *data = NULL;

    gpr = cmd0 (camera, context, 0, FUJI_GET_PICCOUNT, &data, &len);
    if (gpr < GP_OK) {
	if (data) free(data);
	return gpr;
    }
    numpics = data[0] + (data[1]<<8);
    if (data) free(data);
    return numpics;
#endif
}

int
fuji_pic_name (Camera *camera, unsigned int i, const char **name,
	       GPContext *context)
{
	static unsigned char buf[1025];
	unsigned char cmd[6];
	unsigned int buf_len = 1024;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_NAME;
	cmd[2] = 2;
	cmd[4] = i;
	cmd[5] = (i >> 8);

	memset (buf, 0, sizeof (buf));
	CR (fuji_transmit (camera, cmd, 6, buf, &buf_len, context));
	*name = buf;

	return (GP_OK);
}

int
fuji_pic_del (Camera *camera, unsigned int i, GPContext *context)
{
	unsigned char cmd[6], buf[1024];
	unsigned int buf_len;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_NAME;
	cmd[2] = 2;
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
	unsigned int buf_len;

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_SIZE;
	cmd[2] = 2;
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
	cmd[4] = i;
	cmd[5] = (i >> 8);

	CRF (fuji_transmit (camera, cmd, 6, *data, size, context), data);

	return (GP_OK);
}

int
fuji_pic_get (Camera *camera, unsigned int i, unsigned char **data, 
	      unsigned int *size, GPContext *context)
{
	unsigned char cmd[6];

	/*
	 * First, get the size of the picture and allocate the necessary
	 * memory.
	 */
	CR (fuji_pic_size (camera, i, size, context));
	*data = malloc (sizeof (char) * *size);
	if (!*data) {
		gp_context_error (context, _("Could not allocate "
			"%i byte(s) for downloading the picture."), *size);
		return (GP_ERROR_NO_MEMORY);
	}

	cmd[0] = 0;
	cmd[1] = FUJI_CMD_PIC_GET;
	cmd[2] = 2;
	cmd[4] = i;
	cmd[5] = (i >> 8);

	CRF (fuji_transmit (camera, cmd, 6, *data, size, context), data);

	return (GP_OK);
}

int
fuji_set_speed (Camera *camera, FujiSpeed speed, GPContext *context)
{
	unsigned char cmd[5], buf[1024];
	unsigned int buf_len;

	cmd[0] = 1;
	cmd[1] = FUJI_CMD_SPEED;
	cmd[2] = 1;
	cmd[3] = 0;
	cmd[4] = speed;
	CR (fuji_transmit (camera, cmd, 5, buf, &buf_len, context));
	CLEN (buf_len, 1);

	/* Check the response */
	if (buf[0] != 0) {
		gp_context_error (context, _("Could not set speed to "
			"%i (camera responded with %i)."), speed, buf[0]);
		return (GP_ERROR);
	}

	/* Reset the connection */
	CR (fuji_reset (camera, context));

	return (GP_OK);
}

int
fuji_reset (Camera *camera, GPContext *context)
{
	unsigned char c = EOT;

	CR (gp_port_write (camera->port, &c, 1));
	
	return (GP_OK);
}

#if 0
#define serial_port 0

#warning Do not use globals - they break pretty much every GUI for gphoto2,
#warning including Konqueror (KDE) and Nautilus (GNOME).
static gp_port *thedev;

static int pictures;
static int interrupted = 0;
static int pending_input = 0;
static struct pict_info *pinfo = NULL;

#define STX 0x2  /* Start of data */
#define ETX 0x3  /* End of data */
#define EOT 0x4  /* End of session */
#define ENQ 0x5  /* Enquiry */
#define ACK 0x6
#define ESC 0x10
#define ETB 0x17 /* End of transmission block */
#define NAK 0x15

#define FUJI_MAXBUF_DEFAULT 9000000

#define DBG(x) GP_DEBUG(x)
#define DBG2(x,y) GP_DEBUG(x,y)
#define DBG3(x,y,z) GP_DEBUG(x,y,z)
#define DBG4(w,x,y,z) GP_DEBUG(w,x,y,z)

static char *fuji_buffer;
static long fuji_maxbuf=FUJI_MAXBUF_DEFAULT;
static int answer_len = 0;
static Camera *curcam=NULL;
static CameraFile *curcamfile=NULL;

static int fuji_initialized=0; 
static int fuji_count;
static int fuji_size;
static int devfd = -1;
static int maxnum;


struct pict_info {
	char *name;
	int number;
	int size;
	short ondisk;
	short transferred;
};

static int get_raw_byte (void)
{
	static unsigned char buffer[128];
	static unsigned char *bufstart;
	int ret;

	while (!pending_input) {
		/* Refill the buffer */
	        ret=gp_port_read(thedev,buffer,1);
		/*ret = read(devfd, buffer, 128);*/
		DBG2("GOT %d",ret);
		if (ret == GP_ERROR_TIMEOUT)
			return -1;  /* timeout */
		if (ret == GP_ERROR) {
		  return -1;  /* error */
		}
		if (ret <0 ) {
		  return -1;  /* undocumented error */
		}
		pending_input = ret;
		bufstart = buffer;
	}
	pending_input--;
	return *bufstart++;
}

static int wait_for_input (int seconds)
{
	fd_set rfds;
	struct timeval tv;
	DBG2("Wait for input,devfd=%d",devfd);
	if (pending_input)
		return 1;
	if (!seconds)
		return 0;
	DBG("Wait for input, running cmds");
	FD_ZERO(&rfds);
	FD_SET(devfd, &rfds);
	DBG("Wait for input,cmds complete");
	tv.tv_sec = seconds;
	tv.tv_usec = 0;

	return select(1+devfd, &rfds, NULL, NULL, &tv);
}

static int put_bytes (int n, unsigned char* buff)
{
	int ret;

	ret=gp_port_write(thedev,buff,n);
	if (ret==GP_OK) {
	  buff+=n;
	  return -1;
	};
	return 0;
	while (n > 0) {
	  /*ret = write(devfd, buff, n);*/
	  CR (ret = gp_port_write(thedev,buff,n)); 
	  n -= ret;
	  buff += ret;
	}
	return 0;
}


static int cmd (int len, unsigned char *data)
{
	int i, c, timeout=50;
	fuji_count=0;

	DBG2("CMD $%X called ",data[1]);	

	/* Some commands require large timeouts */
	switch (data[1]) {
	  case FUJI_CMD_TAKE:	/* Take picture */
	  case FUJI_CHARGE_FLASH:	/* Recharge the flash */
	  case FUJI_PREVIEW:	/* Take preview */
	    timeout = 12;
	    break;
	  case FUJI_CMD_DELETE:	/* Erase a picture */
	    timeout = 1;
	    break;
	}

	for (i = 0; i < 3; i++) { /* Three tries */
		send_packet(len, data, 1);
		c = get_byte();
		if (c == ACK)
			goto rd_pkt;
		if (c != NAK){
			if(fuji_ping(camera, context)) return(-1);
		};
	}
	DBG2("Command error, aborting. %d bytes recd",answer_len);
	return(-1);

rd_pkt:
	/*wait_for_input(timeout);*/

	for (i = 0; i < 3; i++) {
		if (i) put_byte(NAK);
		c = read_packet();
		if (c < 0)
			continue;
		if (c && interrupted) {
		        DBG("interrupted");
			exit(1);
		}

		put_byte(ACK);

		if (fuji_buffer != NULL) {
		  if ((fuji_count+answer_len-4)<fuji_maxbuf){
		    memcpy(fuji_buffer+fuji_count,answer+4,answer_len-4);
		    fuji_count+=answer_len-4;
		  } else DBG("buffer overflow");
		    
		  DBG3("Recd %d of %d\n",fuji_count,fuji_size);

		  //if (curcamfile)
		  //  gp_frontend_progress(curcam,curcamfile,
		  //		 (1.0*fuji_count/fuji_size>1.0)?1.0:1.0*fuji_count/fuji_size);

		};
		/* More packets ? */
		if (c != 0)
			goto rd_pkt;
		//gp_frontend_progress("","",0); /* Clean up the indicator */
		return 0;
	}		
	DBG("Cannot receive answer, aborting.");
	return(-1);
}

static char* fuji_version_info (void)
{
	cmd0 (0, FUJI_CMD_VERSION);
	return answer+4;
}

static char* fuji_camera_type (void)
{
	cmd0 (0, 0x29);
	return answer+4;
}

static char* fuji_camera_id (void)
{
	cmd0 (0, 0x80);
	return answer+4;
}

static int fuji_set_camera_id (const char *id)
{
	unsigned char b[14];
	int n = strlen(id);

	if (n > 10)
		n = 10;
	b[0] = 0;
	b[1] = 0x82;
	b[2] = n;
	b[3] = 0;
	memcpy(b+4, id, n);
	return cmd(n+4, b);
}

static char* fuji_get_date (void)
{
	char *fmtdate = answer+50;
	
	cmd0 (0, 0x84);
	strcpy(fmtdate, "YYYY/MM/DD HH:MM:SS");
	memcpy(fmtdate,    answer+4,   4);	/* year */
	memcpy(fmtdate+5,  answer+8,   2);	/* month */
	memcpy(fmtdate+8,  answer+10,  2);	/* day */
	memcpy(fmtdate+11, answer+12,  2);	/* hour */
	memcpy(fmtdate+14, answer+14,  2);	/* minutes */
	memcpy(fmtdate+17, answer+16,  2);	/* seconds */

	return fmtdate;
}

static int fuji_set_date (struct tm *pt)
{
	unsigned char b[50];

	sprintf(b+4, "%04d%02d%02d%02d%02d%02d", 1900 + pt->tm_year, pt->tm_mon+1, pt->tm_mday,
		pt->tm_hour, pt->tm_min, pt->tm_sec);
	b[0] = 0;
	b[1] = 0x86;
	b[2] = strlen(b+4);	/* should be 14 */
	b[3] = 0;
	return cmd(4+b[2], b);
}

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

int
fuji_pic_count (Camera *camera, GPContext *context)
{
	if (cmd0 (0, 0x0b)) return(-1);
	return answer[4] + (answer[5]<<8);
}

int
fuji_pic_name (Camera *camera, unsigned int i, const char **name,
	       GPContext *context)
{
	cmd2 (0, 0x0a, i);
	*name = answer+4;

	return (GP_OK);
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
/*
void list_pictures (void)
{
	int i;
	struct pict_info* pi;
	char ex;

	for (i = 1; i <= pictures; i++) {
		pi = &pinfo[i];
		ex = pi->ondisk ? '*' : ' ';
		DBG4(stderr,"%3d%c  %12s  %7d\n", i, ex,pi->name, pi->size);
	}
}
*/
static void fuji_close_connection (void)
{
	put_byte(0x04);
	usleep(50000);
}

static void reset_serial (void)
{
	if (devfd >= 0) {
	  fuji_close_connection();
		/*tcsetattr(devfd, TCSANOW, &oldt);*/
	}
	devfd = -1; /* shouldn't be needed*/
}

static int fuji_set_max_speed (int newspeed)
{
  int speed,i,res;
  int speedlist[]={115200,57600,38400,19200,9600,0};  /* Serial Speeds */
  int numlist[]={8,7,6,5,0,0}; /* Rate codes for camera */
  gp_port_settings settings;

  gp_port_settings_get(thedev, &settings);
  
  DBG2("Speed currently set to %d",settings.serial.speed);

  DBG("Begin set_max_speed");

  speed=9600; /* The default */

  /* Search for speed =< that specified which is supported by the camera*/
  for (i=0;speedlist[i]>0;i+=1) {

    if (speedlist[i]>newspeed) continue;

    res=cmd1(1,7,numlist[i]); /* Check if supported */

    DBG4("Speed change to %d answer was %X, res=%d",speedlist[i],answer[4],res);

    if (answer[4]==0 && res>-1) {
      speed=speedlist[i];
      break; /* Use this one !!! */
    };
  };

  fuji_close_connection();
  settings.serial.speed=speed;

  DBG2("Setting speed to %d",speed);
  gp_port_settings_set(thedev, settings);
  gp_port_settings_get(thedev, &settings);
  DBG2("Speed set to %d",settings.serial.speed);
  return(fuji_ping (camera, context));
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
	else fuji_size=10500;  /* Probly not same for all cams, better way ? */
	
	DBG2("calling download for %d bytes",fuji_size);

	t1 = times(0);
	if (cmd2(0, (thumb?0:0x02), n)==-1) return(GP_ERROR);

	DBG3("Download :%4d actual bytes vs expected %4d bytes\n", 
	     fuji_count ,fuji_size);

	//file->bytes_read=fuji_count;
	//	file->size=fuji_count+(thumb?128:0);/*add room for thumb-decode*/
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
	  //gp_frontend_status(NULL,"Short picture file--disk full or quota exceeded\n");
	    return(GP_ERROR);
	  };
	};
	return(GP_OK);

}

static int fuji_free_memory (void)
{
	cmd0 (0, 0x1B);
	return answer[5] + (answer[6]<<8) + (answer[7]<<16) + (answer[8]<<24);
}


static int delete_pic (const char *picname,FujiData *fjd)
{
	int i, ret;

	for (i = 1; i <= pictures; i++)
	        if (!strcmp(pinfo[i].name, picname)) {
	                 if ((ret = del_frame(i)) == 0)
			          get_picture_list(fjd);
			 return ret;
		}
	return -1;
}


static char* auto_rename (void)
{
	static char buffer[13];
	
	if (maxnum < 99999)
		maxnum++;

	sprintf(buffer, "DSC%05d.JPG", maxnum);
	return buffer;
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
	  //update_status("Cannot open file for upload");
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


/***********************************************************************
     gphoto lib calls
***********************************************************************/


static int fuji_configure(){
  /* 
     DS7 can't be configured, looks like other Fuji cams can...
     expansion needed here.
  */
  // open_fuji_config_dialog();

  return(1);
};

static int fuji_get_picture (int picture_number,int thumbnail,CameraFile *cfile,CameraPrivateLibrary *fjd){

  const char *buffer;
  long int file_size;

  exifparser exifdat;

  DBG3("fuji_get_picture called for #%d %s\n",
       picture_number, thumbnail?"thumb":"photo");

  //if (fuji_init()) return(GP_ERROR);
  DBG2("buffer is %x",fuji_buffer);

  //  if (thumbnail)
  //  sprintf(tmpfname, "%s/gphoto-thumbnail-%i-%i.tiff",

  DBG("test 1");

  if (download_picture(picture_number,thumbnail,cfile,fjd)!=GP_OK) {
    return(GP_ERROR);
  };

  DBG("test 2");

  //buffer=cfile->data;
  gp_file_get_data_and_size(cfile,&buffer,&file_size);

  /* Work on the tags...*/
  exifdat.header=buffer;
  exifdat.data=buffer+12;

  if (exif_parse_data(&exifdat)>0){
    stat_exif(&exifdat);  /* pre-parse the exif data */

    //newimage->image_info_size=exifdat.ifdtags[thumbnail?1:0]*2;

    //if (thumbnail) newimage->image_info_size+=6; /* just a little bigger...*/

    //DBG2("Image info size is %d",cfile->size);

    //newimage->image_info=malloc(sizeof(char*)*infosize);

    if (buffer==NULL) {
      DBG("BIG TROUBLE!!! Bad image memory allocation");
      return(GP_ERROR);
    };

    if (thumbnail) {/* This SHOULD be done by tag id */
      //      togphotostr(&exifdat,0,0,newimage->image_info,newimage->image_info+1);
      //      togphotostr(&exifdat,0,1,newimage->image_info+2,newimage->image_info+3);
      //      togphotostr(&exifdat,0,2,newimage->image_info+4,newimage->image_info+5);
      //      newimage->image_info_size+=6;
    };

    //exif_add_all(&exifdat,thumbnail?1:0,newimage->image_info+(thumbnail?6:0));

    //DBG("====================EXIF TAGS================");
    //for(i=0;i<cfile->size/2;i++)
    // DBG3("%12s = %12s \n",cfile->data[i*2],cfile->data[i*2+1]);
    //DBG("=============================================");

    /* Tiff info in thumbnail must be converted to be viewable */
    if (thumbnail) {
      fuji_exif_convert(&exifdat,cfile);
      gp_file_get_data_and_size(cfile,&buffer,&file_size);
      
      //  if (cfile->data==NULL) {
      if (buffer==NULL) {
	DBG("Thumbnail conversion error");
	return(GP_ERROR);
      };
    }
    else  { /* Real image ... */
      //cfile->data=buffer;  /* ok, use original data...*/
      //strcpy(cfile->type,"image/jpg");
    };
  } else    { /* Wasn't recognizable exif data */
	DBG("Exif data parsing error");
  };

  return(GP_OK/*newimage*/);

};

static int fuji_delete_image (FujiData *fjd, int picture_number){
  int err_status;

  if (!fjd->has_cmd[FUJI_CMD_DELETE]) {
    return(0);
  };

  err_status = del_frame(picture_number);
  reset_serial();

  /* Warning, does the index need to be re-counted after each delete?*/
  return(!err_status);
};

static int fuji_number_of_pictures (){
  int numpix;

  numpix=fuji_nb_pictures();

  reset_serial(); /* Wish this wasn't necessary...*/
  return(numpix);

};

static int camera_exit (Camera *camera) {
  DBG("Camera exit");
  fuji_close_connection();
  if (camera->port) gp_port_close(camera->port);
  DBG("Done");
  return (GP_OK);
}

static int camera_folder_list	(Camera *camera, char *folder, CameraList *list) {
  DBG2("Camera folder list,%s",folder);
  if (folder==NULL) gp_list_append(list,"/",NULL);
  return (GP_OK);
}

static int camera_file_list (Camera *camera, const char *folder, CameraList *list) {
  int i,n;
  char* fn=NULL;
  //FujiData *cs;
  CameraPrivateLibrary *cs;

  //cs=(FujiData*)camera->camlib_data;
  cs=(CameraPrivateLibrary *)camera->pl;

  DBG("Camera file list");
  n=fuji_nb_pictures();
  DBG3("Getting file list for %s, %d files",folder,n);
  for (i=0;i<n;i++) {
    fn=strdup(fuji_picture_name(i+1));
    DBG3("File %s is number %d",fn,i+1);
    //gp_filesystem_append(camera->fs,"/",fn);
    gp_list_append (list, fn, NULL);
  };
  return (GP_OK);
  }


static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
	       void *data)
{
	Camera *camera = data;
	DBG("file_list_func");
	return (camera_file_list (camera, folder, list));
}

static int get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,CameraFileInfo *info, void *data)
{
  DBG2("Info Func %s",folder);

  info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE |
    GP_FILE_INFO_NAME;
  info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;

  /* Name of image */
  strncpy (info->file.name, "test\0", 5);
  
  /* Get the size of the current image */
  info->file.size = 1;
  
  /* Type of image? */
  /* if (strstr (filename, ".MOV") != NULL) {
     strcpy (info->file.type, GP_MIME_QUICKTIME);
     strcpy (info->preview.type, GP_MIME_JPEG);
     } else if (strstr (filename, ".TIF") != NULL) {
     strcpy (info->file.type, GP_MIME_TIFF);
     strcpy (info->preview.type, GP_MIME_TIFF);
     } else {*/
     strcpy (info->file.type, GP_MIME_JPEG);
     strcpy (info->preview.type, GP_MIME_JPEG);
     //     }


  return(GP_OK);
};

static int
get_file_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, CameraFileType type, CameraFile *file,
	       void *data) { 

  Camera *camera = data;
  int num;
  //FujiData *fjd;
  CameraPrivateLibrary *fjd;

  fjd=camera->pl;


  curcam=camera;
  curcamfile=file;

  DBG3("Camera file type %d get %s",type,filename);

  num=gp_filesystem_number   (camera->fs, "/" ,filename)+1;


  DBG3("File %s,%d\n",filename,num);

	return (fuji_get_picture(num,type==0?1:0,file,fjd));
}

int camera_init (Camera *camera) {
        char idstring[256];
	CameraPrivateLibrary *fjd;

	gp_port_settings settings;

	DBG("Camera init");

	/* First, set up all the function pointers */
	camera->functions->exit 	= camera_exit;
	//camera->functions->file_get 	= camera_file_get;
	//	camera->functions->file_get_preview =  camera_file_get_preview;
	//	camera->functions->file_put 	= camera_file_put;
	//camera->functions->file_delete 	= camera_file_delete;
	/*	camera->functions->config_get   = camera_config_get;
		camera->functions->config_set   = camera_config_set;*/
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	//camera->camlib_data=malloc(sizeof( FujiData));
	camera->pl=malloc(sizeof( CameraPrivateLibrary ));
	fjd=camera->pl;//(FujiData*)camera->camlib_data;

	/* Set up the CameraFilesystem */
        gp_filesystem_set_list_funcs (camera->fs,
                                        file_list_func, folder_list_func,
                                        camera);
        gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL,
	                              camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	/* Set up the port */
	thedev=camera->port;
	gp_port_settings_get (camera->port, &settings);
	gp_port_timeout_set(thedev,1000);
	settings.serial.speed 	 = 9600;
	settings.serial.bits 	 = 8;
	settings.serial.parity 	 = GP_PORT_SERIAL_PARITY_EVEN;
	settings.serial.stopbits = 1;

	gp_port_settings_set(camera->port,settings);
	//gp_port_open(thedev);

	//DBG2("Initialized port %s",settings.serial.port);

	fuji_maxbuf=FUJI_MAXBUF_DEFAULT; /* This should be camera dependent */

	//fuji_set_max_speed(57600);

	//DBG("Max speed set");

	if (!fuji_initialized){
	  fuji_buffer=malloc(fuji_maxbuf);
	  if (fuji_buffer==NULL) {
	    DBG("Memory allocation error");
	    return(-1);
	  } else {
	    DBG2("Allocated %ld bytes to main buffer",fuji_maxbuf);
	  };
	  
	  /* could ID camera here and set the relevent variables... */
	  get_command_list(camera->pl);
	  /* For now assume that the user wil not use 2 different fuji cams
	     in one session */
	  strcpy(idstring,"Identified ");
	  strncat(idstring,fuji_version_info(),100);
	  //gp_frontend_status(NULL,idstring);

	  /* load_fuji_options() */
	  fuji_initialized=1;  
	};

	return (GP_OK);
}

#endif
