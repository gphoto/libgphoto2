/* lowlevel.c
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sourceforge.net>
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
#include "lowlevel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>

#include "konica.h"

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

#define GP_MODULE "konica"

#define DEFAULT_TIMEOUT	1000

#define LESSER_ESCAPES

/* ESC quotes */
#define STX	0x02
#define ETX	0x03
#define ENQ	0x05
#define ACK	0x06
#define XOFF	0x11
#define XON	0x13
#define NACK	0x15
#define ETB	0x17
#define ESC	0x1b

/* Not an ESC quote */
#define EOT	0x04

#define CHECK(r)        {int ret = (r); if (ret < 0) return (ret);}
#define CHECK_NULL(r)   {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_FREE(r,f) {int ret = (r); if (ret < 0) {free (f); return (ret);}}

static int
l_ping_rec (GPPort *p, unsigned int level)
{
	unsigned char c;

	/* Write ENQ and read the response. */
	c = ENQ;
	CHECK (gp_port_write (p, (char *)&c, 1));
	CHECK (gp_port_read (p, (char *)&c, 1));
	switch (c) {
	case ACK:
		return (GP_OK);
	case NACK:

		/*
		 * Uh, lets try again a couple of times, but
		 * make sure we don't recurse forever.
		 */
		if (level < 30)
			return (l_ping_rec (p, level + 1));
		else
			return (GP_ERROR_CORRUPTED_DATA);
	case ENQ:

		/*
		 * ENQ received. It seems that the camera would like
		 * to send us data, but we do not want it and
		 * therefore simply reject it. The camera will try
		 * two more times with ENQ to get us to receive data
		 * before finally giving up and sending us ACK.
		 */

		/* Write NACK.  */
		c = NACK;
		CHECK (gp_port_write (p, (char *)&c, 1));
		for (;;) {
			CHECK (gp_port_read (p, (char *)&c, 1));
			switch (c) {
			case ENQ:

				/* The camera has not yet given up. */
				continue;

			case ACK:

				/* ACK received. We can proceed. */
				return (GP_OK);
			default:

				/* This should not happen. */
				return (GP_ERROR_CORRUPTED_DATA);
			}
			break;
		}
		break;
	default:

		/*
		 * The camera seems to send us data. We'll
		 * simply ignore it and try again (but make
		 * sure that we don't loop forever).
		 */
		CHECK (gp_port_flush (p, 0));
		CHECK (gp_port_flush (p, 1));
		if (level > 50)
			return (GP_ERROR_CORRUPTED_DATA);
		else
			return (l_ping_rec (p, level + 1));
	}

	return (GP_OK);
}

int
l_ping (GPPort *p, GPContext *c)
{
	return (l_ping_rec (p, 0));
}

int
l_init (GPPort *p, GPContext *c)
{
	unsigned int i;
	int result = GP_OK;
	CHECK_NULL (p);

	CHECK (gp_port_set_timeout (p, DEFAULT_TIMEOUT));
	for (i = 0; i < 3; i++) {
		result = l_ping (p, c);
		if (result != GP_ERROR_TIMEOUT)
			break;
	}
	return (result);
}

static int
l_esc_read (GPPort *p, unsigned char *c)
{
	CHECK_NULL (p && c);

	CHECK (gp_port_read (p, (char *)c, 1));

	/*
	 * STX, ETX, ENQ, ACK, XOFF, XON, NACK, and ETB have to be masked by
	 * ESC. If we receive one of those (except ETX and ETB) without mask,
	 * we will not report an error, as it will be recovered automatically
	 * later. If we receive ETX or ETB, we reached the end of the packet
	 * and report a transmission error, so that the error can be
	 * recovered.
	 * If the camera sends us ESC (the mask), we will not count this byte
	 * and read a second one. This will be reverted and processed. It
	 * then must be one of STX, ETX, ENQ, ACK, XOFF, XON, NACK, ETB, or
	 * ESC. As before, if it is not one of those, we'll not report an
	 * error, as it will be recovered automatically later.
	 */
	 /* The HP PhotoSmart does not escape every special code, only
	  * some and it gets confused if we do this checks. By relaxing
	  * these, it now downloads images etc.
	  */
#ifndef LESSER_ESCAPES
	if ((*c == STX ) || (*c == ETX) || (*c == ENQ ) || (*c == ACK) ||
	    (*c == XOFF) || (*c == XON) || (*c == NACK) || (*c == ETB)) {
#else /* LESSER_ESCAPES */
	if ((*c == STX ) || (*c == XOFF) || (*c == XON)) {
#endif /* LESSER_ESCAPES */
		GP_DEBUG ("Wrong ESC masking!");
		if ((*c == ETX) || (*c == ETB))
			return (GP_ERROR_CORRUPTED_DATA);
	} else if (*c == ESC) {
		CHECK (gp_port_read (p, (char *)c, 1));
		*c = (~*c & 0xff);
		if ((*c != STX ) && (*c != ETX ) && (*c != ENQ) &&
		    (*c != ACK ) && (*c != XOFF) && (*c != XON) &&
		    (*c != NACK) && (*c != ETB ) && (*c != ESC))
			GP_DEBUG ("Wrong ESC masking!");
	}
	return (GP_OK);
}


static int
l_send (GPPort *p, GPContext *context, unsigned char *send_buffer,
	unsigned int send_buffer_size)
{
	unsigned char c;
	unsigned char checksum;
	/**********************************************************************/
	/*  sb: A pointer to the buffer that we will send.                    */
	/* sbs: Its size.		                                      */
	/**********************************************************************/
	unsigned char *sb;
	unsigned int sbs;
	int i = 0;

	CHECK_NULL (p && send_buffer);

	/* We need to ping the camera first */
	CHECK (l_ping (p, context));

	/********************************************************/
	/* We will write:			 		*/
	/*  - STX						*/
	/*  - low order byte of (send_buffer_size + 5)		*/
	/*  - high order byte of (send_buffer_size + 5)		*/
	/*  - 'send_buffer_size' bytes data plus ESC quotes	*/
	/*  - ETX						*/
	/*  - 1 byte for checksum plus ESC quotes		*/
	/*							*/
	/* The checksum covers all bytes after STX and before	*/
	/* the checksum byte(s).				*/
	/********************************************************/
	sbs = send_buffer_size + 5;
	sb = malloc (sizeof (char) * sbs);
	sb[0] = STX;
	sb[1] = send_buffer_size;
	sb[2] = send_buffer_size >> 8;
	checksum  = sb[1];
	checksum += sb[2];
	for (i = 3; i < (sbs - 2); i++) {
		checksum += *send_buffer;
		if (	(*send_buffer == STX ) ||
			(*send_buffer == ETX ) ||
			(*send_buffer == ENQ ) ||
			(*send_buffer == ACK ) ||
			(*send_buffer == XOFF) ||
			(*send_buffer == XON ) ||
			(*send_buffer == NACK) ||
			(*send_buffer == ETB ) ||
			(*send_buffer == ESC )) {
			sb = realloc (sb, ++sbs * sizeof (char));
			sb[  i] = ESC;
			sb[++i] = ~*send_buffer;
		} else  sb[  i] =  *send_buffer;
		send_buffer++;
	}
	sb[sbs - 2] = ETX;
	checksum += ETX;
	if (	(checksum == STX ) ||
		(checksum == ETX ) ||
		(checksum == ENQ ) ||
		(checksum == ACK ) ||
		(checksum == XOFF) ||
		(checksum == XON ) ||
		(checksum == NACK) ||
		(checksum == ETB ) ||
		(checksum == ESC )) {
		sb = realloc (sb, ++sbs * sizeof (char));
		sb[sbs - 2] = ESC;
		sb[sbs - 1] = ~checksum;
	} else  sb[sbs - 1] =  checksum;
	for (i = 0; ; i++) {

		/* Write data as above.	*/
		CHECK_FREE (gp_port_write (p, (char *)sb, sbs), sb);
		CHECK_FREE (gp_port_read (p, (char *)&c, 1), sb);
		switch (c) {
		case ACK:

			/* ACK received. We can proceed. */
			free (sb);

			/* Write EOT.	*/
			c = EOT;
			CHECK (gp_port_write (p, (char *)&c, 1));
			return (GP_OK);

		case NACK:

			/* NACK received. We'll try up to three times. */
			if (i == 2) {
				free (sb);
				return (GP_ERROR_CORRUPTED_DATA);
			} else break;
		default:

			/* Should not happen. */
			return (GP_ERROR_CORRUPTED_DATA);
		}
	}
}


static int
l_receive (GPPort *p, GPContext *context,
	   unsigned char **rb, unsigned int *rbs,
	   unsigned int timeout)
{
	unsigned char c, d, progress = 0;
	int error_flag;
	unsigned int i, j, rbs_internal, id = 0;
	unsigned char checksum;
	int result;
	KCommand command;

	CHECK_NULL (p && rb && rbs);

	for (i = 0; ; ) {
		CHECK (gp_port_set_timeout (p, timeout));
		CHECK (gp_port_read (p, (char *)&c, 1));
		CHECK (gp_port_set_timeout (p, DEFAULT_TIMEOUT));
		switch (c) {
		case ENQ:

			/* ENQ received. We can proceed. */
			break;

		case ACK:

			/* ACK received. We'll try again at most ten times. */
			if (i == 9) {

				/*
				 * The camera hangs! Although it could be
				 * that the camera accepts one of the
				 * commands
				 *  - KONICA_CANCEL,
				 *  - KONICA_GET_IO_CAPABILITY, and
				 *  - KONICA_SET_IO_CAPABILITY,
				 * we can not get the camera back to working
				 * correctly.
				 */
				return (GP_ERROR_CORRUPTED_DATA);

			}
			i++;
			break;
		default:
			/*
			 * We'll dump this data until we get ENQ (then, we'll
			 * proceed) or nothing any more (then, we'll report
			 * error).
			 */
			for (;;) {
				CHECK (gp_port_read (p, (char *)&c, 1));
				if (c == ENQ)
					break;
			}
			break;
		}
		if (c == ENQ)
			break;
	}

	/* Start progress */
	if (*rbs > 1000) {
		id = gp_context_progress_start (context, *rbs,
						_("Downloading..."));
		progress = 1;
	}

	/* Write ACK. */
	CHECK (gp_port_write (p, "\6", 1));
	for (*rbs = 0; ; ) {
		for (j = 0; ; j++) {
			CHECK (gp_port_read (p, (char *)&c, 1));
			switch (c) {
			case STX:

				/* STX received. We can proceed. */
				break;

			default:

				/* We'll dump this data and try again. */
				continue;

			}

			/*
			 * Read 2 bytes for size (low order byte, high order
			 * byte) plus ESC quotes.
			 */
			CHECK (l_esc_read (p, &c));
			checksum = c;
			CHECK (l_esc_read (p, &d));
			checksum += d;
			rbs_internal = (d << 8) | c;
			if (*rbs == 0)
				*rb = malloc (rbs_internal * sizeof (char));
			else
				*rb = realloc (*rb,
					sizeof (char) * (*rbs + rbs_internal));

			/* Read 'rbs_internal' bytes data plus ESC quotes. */
			error_flag = 0;
{
unsigned int read = 0, r = 0;
while (read < rbs_internal) {

	/*
	 * Read the specified amount of bytes. We will probably read more
	 * because some bytes will be quoted.
	 */
	GP_DEBUG ("Reading %i bytes (%i of %i already read)...",
		  rbs_internal - read, read, rbs_internal);
	result = gp_port_read (p, (char *)&((*rb)[*rbs + read]),
			       rbs_internal - read);
	if (result < 0) {
		error_flag = 1;
		break;
	}
	r = rbs_internal - read;

	/* Unescape the data we just read */
	for (i = read; i < read + r; i++) {
		unsigned char *c = &(*rb)[*rbs + i];

		 /* The HP PhotoSmart does not escape every special code, only
		  * some and it gets confused if we do this checks. By relaxing
		  * these, it now downloads images etc.
		  */
#ifndef LESSER_ESCAPES
	        if ((*c == STX) || (*c == ETX) || (*c == ENQ ) ||
		    (*c == ACK) || (*c == XOFF) || (*c == XON) ||
		    (*c == NACK) || (*c == ETB)) {
#else /* LESSER_ESCAPES */
	        if ((*c == STX) ||  (*c == XOFF) || (*c == XON)) {
#endif /* LESSER_ESCAPES */
			GP_DEBUG ("Wrong ESC masking!");
			error_flag = 1;
			break;
		} else if (*c == ESC) {
			if (i == read + r - 1) {
				/* ESC is last char of packet */
				CHECK (gp_port_read (p, (char *)c, 1));
			} else {
				memmove (c, c + 1, read + r - i - 1);
				r--;
			}
			*c = ~*c & 0xff;
			if ((*c != STX ) && (*c != ETX ) && (*c != ENQ) &&
			    (*c != ACK ) && (*c != XOFF) && (*c != XON) &&
			    (*c != NACK) && (*c != ETB ) && (*c != ESC)) {
				GP_DEBUG ("Wrong ESC masking!");
				error_flag = 1;
				break;
			}
		}
		checksum += (*rb)[*rbs + i];
	}
	if (error_flag)
		break;
	read += r;
}}
			if (!error_flag) {
				CHECK (gp_port_read (p, (char *)&d, 1));
				switch (d) {
				case ETX:

					/*
					 * ETX received. This is the last
					 * packet.
					 */
					GP_DEBUG ("Last packet.");
					break;

				case ETB:

					/*
					 * ETB received. There are more
					 * packets to come.
					 */
					GP_DEBUG ("More packets coming.");
					break;

				default:

					/*
					 * We get more bytes than expected.
					 * Nothing serious, as we will simply
					 * dump all bytes until we receive
					 * ETX or ETB. Later, we'll read the
					 * checksum with ESC quotes and
					 * reject the packet.
					 */
					while ((d != ETX) && (d != ETB)) {
						CHECK (gp_port_read (p,
								     (char *)&d, 1));
					}
					error_flag = 1;
					break;
				}
			}
			checksum += d;

			/* Read 1 byte for checksum plus ESC quotes. */
			CHECK (l_esc_read (p, &c));
			if ((c == checksum) && (!error_flag)) {
				*rbs += rbs_internal;

				/* Write ACK. */
				CHECK (gp_port_write (p, "\6", 1));
				break;

			} else {

				/*
				 * Checksum wrong or transmission error. The
				 * camera will send us the data at the most
				 * three times.
				 */
				GP_DEBUG ("Checksum wrong: expected %i, "
					  "got %i.", c, checksum);
				if (j == 2)
					return (GP_ERROR_CORRUPTED_DATA);

				/* Write NACK. */
				c = NACK;
				CHECK (gp_port_write (p, (char *)&c, 1));
				continue;
			}
		}
		CHECK (gp_port_read (p, (char *)&c, 1));
		switch (c) {
			case EOT:

				/* EOT received. We can proceed. */
				break;

			default:

				/* Should not happen. */
				return (GP_ERROR_CORRUPTED_DATA);
		}
		/*
		 * Depending on d, we will either continue to receive data or
		 * stop.
		 *
		 *  - ETX:  We are all done.
		 *  - ETB:  We expect more data.
		 *  - else: Should not happen.
		 */
		switch (d) {
		case ETX:

			/* We are all done. */
			if (progress)
				gp_context_progress_stop (context, id);
			return (GP_OK);

		case ETB:

			/* We expect more data. Read ENQ. */
			CHECK (gp_port_read (p, (char *)&c, 1));
			switch (c) {
			case ENQ:

				/* ENQ received. We can proceed. */
				break;

			default:

				/* Should not happen. */
				return (GP_ERROR_CORRUPTED_DATA);
			}

			if (gp_context_cancel (context) ==
						GP_CONTEXT_FEEDBACK_CANCEL) {
				GP_DEBUG ("Trying to cancel operation...");
				CHECK (k_cancel (p, context, &command));
				GP_DEBUG ("Operation 0x%x cancelled.",
					  command);
				return (GP_ERROR_CANCEL);
			}

			/* Write ACK. */
			CHECK (gp_port_write (p, "\6", 1));
			break;

		default:

			/* Should not happen. */
			return (GP_ERROR_CORRUPTED_DATA);
		}
		if (progress)
			gp_context_progress_update (context, id, *rbs);
	}
}

int
l_send_receive (
	GPPort *p, GPContext *c,
	unsigned char *send_buffer, unsigned int send_buffer_size,
	unsigned char **receive_buffer, unsigned int *receive_buffer_size,
	unsigned int timeout,
	unsigned char **image_buffer, unsigned int *image_buffer_size)
{
	if (!timeout)
		timeout = DEFAULT_TIMEOUT;

	/* Send data. */
	CHECK (l_send (p, c, send_buffer, send_buffer_size));

        /* Receive data. */
	if (image_buffer_size)
		*receive_buffer_size = *image_buffer_size;
	CHECK (l_receive (p, c, receive_buffer, receive_buffer_size,
			  timeout));

	/* Check if we've received the control data. */
	if ((*receive_buffer_size > 1) &&
	    (((*receive_buffer)[0] == send_buffer[0]) &&
	     ((*receive_buffer)[1] == send_buffer[1])))
	    return (GP_OK);

	/* We didn't receive control data yet. */
	*image_buffer = *receive_buffer;
	*image_buffer_size = *receive_buffer_size;
	*receive_buffer = NULL;

	/* Receive control data. */
	CHECK (l_receive (p, c, receive_buffer, receive_buffer_size,
			  DEFAULT_TIMEOUT));

	/* Sanity check: Did we receive the right control data? */
	if (((*receive_buffer)[0] != send_buffer[0]) ||
	    ((*receive_buffer)[1] != send_buffer[1]))
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}
