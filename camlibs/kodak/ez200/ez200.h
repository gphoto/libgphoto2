/* ez200.h
 *
 * Copyright (C) 2004 Bucas Jean-François <jfbucas@tuxfamily.org>
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

#ifndef __EZ200_H__
#define __EZ200_H__

#include <libgphoto2_port/gphoto2-port.h>

#define PING 		0x05
#define STATUS		0x06
#define PICTURE		0x08
#define PICTURE_HEAD	0x0b
#define ACTIVE		0xe0

#define JPG_HEADER_SIZE 0x24D   /* 589 */
#define HEADER_SIZE 0x26F
#define DATA_HEADER_SIZE 0x200

typedef unsigned char Info;

typedef enum {
	MODEL_MINI
} Model;

int ez200_init                (GPPort *port, Model *model, Info *info);
int ez200_exit                (GPPort *port);
int ez200_get_num_pics        (Info *info);
int ez200_read_data           (GPPort *port, char *data, int size);
int ez200_get_picture_size    (GPPort *port, int n);
int ez200_read_picture_data   (GPPort *port, char *data, int size, int n);
int ez200_wait_status_ok      (GPPort *port);
int ez200_read_picture_header (GPPort *port, char *data);

#endif

