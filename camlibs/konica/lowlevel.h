/* lowlevel.h
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

#ifndef __KONICA_LOWLEVEL_H__
#define __KONICA_LOWLEVEL_H__

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-context.h>

int l_ping (GPPort *, GPContext *);
int l_init (GPPort *, GPContext *);

/*
 * Some comments on the structure of send_buffer, image_buffer,
 * and receive_buffer:
 *
 * send_buffer:	The first two bytes are low order and high
 * 		byte of the command, followed by at least two
 *		more bytes. The number of sent bytes has to be
 *		even.
 *
 * image_buffer: 	This buffer will contain raw data like
 *			jpeg or exif data.
 *
 * receive_buffer:	The first two bytes will be the same as
 *			in send_buffer. This will be checked
 *			here. Then will follow low and high
 *			order byte of the return status. After
 *			that, depending on command and return
 *			status, other bytes will follow.
 */
int l_send_receive (GPPort *, GPContext *,
	unsigned char *send_buffer, unsigned int send_buffer_size,
	unsigned char **receive_buffer, unsigned int *receive_buffer_size,
	unsigned int timeout,
	unsigned char **image_buffer, unsigned int *image_buffer_size);

#endif /* __KONICA_LOWLEVEL_H__ */
