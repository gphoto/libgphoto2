/* lowlevel.c
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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
#include "lowlevel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2-result.h>
#include <gphoto2-debug.h>
#include <gphoto2-port-log.h>

#define GP_MODULE "konica"

#define DEFAULT_TIMEOUT	500

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

int 
l_init (GPPort *device)
{
	unsigned char c;
	int i, result;

	CHECK_NULL (device);

	CHECK (gp_port_set_timeout (device, DEFAULT_TIMEOUT));
	for (i = 0; ; i++) {

		/* Write ENQ. 	*/
		CHECK (gp_port_write (device, "\5", 1));
		if ((result = gp_port_read (device, &c, 1)) < 1) {

			/*
			 * We didn't receive anything. We'll try up to four
			 * times.
			 */
			if (i == 4)
				return (result);
			continue;
		}
		switch (c) {
		case ACK:

			/* ACK received. We can proceed. */
			return (GP_OK);

		default:
			/*
			 * The camera seems to be sending something. We'll
			 * dump all bytes we get and try again.
			 */
			while (gp_port_read (device, &c, 1) > 0);
			i--;
			break;
		}
	}
}

static int 
l_esc_read (GPPort *device, unsigned char *c)
{
	CHECK_NULL (device && c);

	CHECK (gp_port_read (device, c, 1));

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
	if ((*c == STX ) || (*c == ETX) || (*c == ENQ ) || (*c == ACK) ||
	    (*c == XOFF) || (*c == XON) || (*c == NACK) || (*c == ETB)) {
		gp_debug_printf (GP_DEBUG_HIGH, "konica", "Wrong ESC masking!");
		if ((*c == ETX) || (*c == ETB))
			return (GP_ERROR_CORRUPTED_DATA);
	} else if (*c == ESC) {
		CHECK (gp_port_read (device, c, 1));
		*c = (~*c & 0xff);
		if ((*c != STX ) && (*c != ETX ) && (*c != ENQ) &&
		    (*c != ACK ) && (*c != XOFF) && (*c != XON) &&
		    (*c != NACK) && (*c != ETB ) && (*c != ESC))
			gp_debug_printf (GP_DEBUG_HIGH, "konica",
					 "Wrong ESC masking!");
	}
	return (GP_OK);
}


static int
l_send (GPPort *device, unsigned char *send_buffer,
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
	int result, i = 0;

	CHECK_NULL (device && send_buffer);

	for (;;) {

		/* Write ENQ. */
		CHECK (gp_port_write (device, "\5", 1));

		/* Read. */
		if ((result = gp_port_read (device, &c, 1)) < 1) {

			/* Let's try for a couple of times. */
			i++;
			if (i == 5)
				return (result);
			continue;
		}
		switch (c) {
		case ACK:

			/* ACK received. We can proceed. */
			break;

		case NACK:

			/* NACK received. We'll start from the beginning. */
			continue;

		case ENQ:

			/*
			 * ENQ received. It seems that the camera would like 
			 * to send us data, but we do not want it and
			 * therefore simply reject it. The camera will try
			 * two more times with ENQ to get us to receive data 
			 * before finally giving up and sending us ACK.
			 */

			/* Write NACK.	*/
			c = NACK;
			CHECK (gp_port_write (device, &c, 1));
			for (;;) {
				CHECK (gp_port_read (device, &c, 1));
				switch (c) {
				case ENQ:

					/* The camera has not yet given up. */
					continue;

				case ACK:

					/* ACK received. We can proceed. */
					break;

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
			 * simply dump it and try again.
			 */
			continue;
		}
		break;
	}
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
		CHECK_FREE (gp_port_write (device, sb, sbs), sb);
		CHECK_FREE (gp_port_read (device, &c, 1), sb);
		switch (c) {
		case ACK:

			/* ACK received. We can proceed. */
			free (sb);

			/* Write EOT.	*/
			c = EOT;
			CHECK (gp_port_write (device, &c, 1));
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
l_receive (GPPort *device, unsigned char **rb, unsigned int *rbs,
	   unsigned int timeout)
{
	unsigned char c, d;
	int error_flag;
	unsigned int i, j, rbs_internal;
	unsigned char checksum;
	int result;

	CHECK_NULL (device && rb && rbs);

	for (i = 0; ; ) {
		CHECK (gp_port_set_timeout (device, timeout));
		CHECK (gp_port_read (device, &c, 1));
		CHECK (gp_port_set_timeout (device, DEFAULT_TIMEOUT));
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
				CHECK (gp_port_read (device, &c, 1));
				if (c == ENQ)
					break;
			}
			break;
		}
		if (c == ENQ)
			break;
	}

	/* Write ACK. */
	CHECK (gp_port_write (device, "\6", 1));
	for (*rbs = 0; ; ) {
		for (j = 0; ; j++) {
			CHECK (gp_port_read (device, &c, 1));
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
			CHECK (l_esc_read (device, &c));
			checksum = c;
			CHECK (l_esc_read (device, &d));
			checksum += d;
			rbs_internal = (d << 8) | c;
			if (*rbs == 0)
				*rb = malloc (rbs_internal * sizeof (char));
			else
				*rb = realloc (*rb,
					sizeof (char) * (*rbs + rbs_internal));

			/* Read 'rbs_internal' bytes data plus ESC quotes. */
			error_flag = 0;
#if 1
{
unsigned int read = 0, r = 0;
while (read < rbs_internal) {

	/*
	 * Read the specified amount of bytes. We will probably read more
	 * because some bytes will be quoted.
	 */
	GP_DEBUG ("Reading %i bytes (%i of %i already read)...", 
		  rbs_internal - read, read, rbs_internal);
	result = gp_port_read (device, &((*rb)[*rbs + read]),
			       rbs_internal - read);
	if (result < 0) {
		error_flag = 1;
		break;
	}
	r = rbs_internal - read;

	/* Unescape the data we just read */
	for (i = read; i < read + r; i++) {
		unsigned char *c = &(*rb)[*rbs + i];

	        if ((*c == STX) || (*c == ETX) || (*c == ENQ ) ||
		    (*c == ACK) || (*c == XOFF) || (*c == XON) ||
		    (*c == NACK) || (*c == ETB)) {
			GP_DEBUG ("Wrong ESC masking!");
			error_flag = 1;
			break;
		} else if (*c == ESC) {
			if (i == read + r - 1) {
				CHECK (gp_port_read (device,
						&((*rb)[*rbs + i + 1]), 1));
				r++;
			}
			*c = (~*(c + 1) & 0xff);
			if ((*c != STX ) && (*c != ETX ) && (*c != ENQ) &&
			    (*c != ACK ) && (*c != XOFF) && (*c != XON) &&
			    (*c != NACK) && (*c != ETB ) && (*c != ESC)) {
				GP_DEBUG ("Wrong ESC masking!");
				error_flag = 1;
				break;
			}
			memmove (c + 1, c + 2, read + r - i);
			r--;
		}
		checksum += (*rb)[*rbs + i];
	}
	if (error_flag)
		break;
	read += r;
}}
#else
			for (i = 0; i < rbs_internal; i++) {
				result = l_esc_read (device, &((*rb)[*rbs + i]));
				switch (result) {
				case GP_ERROR_CORRUPTED_DATA:
					/**************************************/
					/* We already received ETX or ETB.    */
					/* Later, we'll read the checksum     */
					/* plus ESC quotes and reject the     */
					/* packet.                            */
					/**************************************/
					error_flag = 1;
        	                        break;
				case GP_OK:

					/* We can proceed. */
					checksum += (*rb)[*rbs + i];
					break;

				default:
					return (result);
				}
				if (error_flag)
					break;
			}
#endif
			if (!error_flag) {
				CHECK (gp_port_read (device, &d, 1));
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
						CHECK (gp_port_read (device,
								     &d, 1));
					}
					error_flag = 1;
					break;
				}
			}
			checksum += d;

			/* Read 1 byte for checksum plus ESC quotes. */
			CHECK (l_esc_read (device, &c));
			if ((c == checksum) && (!error_flag)) {
				*rbs += rbs_internal;

				/* Write ACK. */
				CHECK (gp_port_write (device, "\6", 1));
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
				CHECK (gp_port_write (device, &c, 1));
				continue;
			}
		}
		CHECK (gp_port_read (device, &c, 1));
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
			return (GP_OK);

		case ETB:

			/* We expect more data. Read ENQ. */
			CHECK (gp_port_read (device, &c, 1));
			switch (c) {
			case ENQ:

				/* ENQ received. We can proceed. */
				break;

			default:

				/* Should not happen. */
				return (GP_ERROR_CORRUPTED_DATA);
			}

			/* Write ACK. */
			CHECK (gp_port_write (device, "\6", 1));
			break;

		default:

			/* Should not happen. */
			return (GP_ERROR_CORRUPTED_DATA);
		}
	}
}

int 
l_send_receive (
	GPPort *device,
	unsigned char *send_buffer, unsigned int send_buffer_size, 
	unsigned char **receive_buffer, unsigned int *receive_buffer_size,
	unsigned int timeout,
	unsigned char **image_buffer, unsigned int *image_buffer_size)
{
	if (!timeout)
		timeout = DEFAULT_TIMEOUT;

	/* Send data. */
	CHECK (l_send (device, send_buffer, send_buffer_size));

        /* Receive data. */
	CHECK (l_receive (device, receive_buffer, receive_buffer_size,
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
	CHECK (l_receive (device, receive_buffer, receive_buffer_size,
			  DEFAULT_TIMEOUT));

	/* Sanity check: Did we receive the right control data? */
	if (((*receive_buffer)[0] != send_buffer[0]) ||
	    ((*receive_buffer)[1] != send_buffer[1]))
		return (GP_ERROR_CORRUPTED_DATA);
	
	return (GP_OK);
}
