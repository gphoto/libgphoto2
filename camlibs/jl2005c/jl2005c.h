/* jl2005c.h
 *
 * Copyright (C) 2006-2010 Theodore Kilgore <kilgota@auburn.edu>
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

#ifndef __jl2005c_H__
#define __jl2005c_H__

#include <unistd.h>
#include <gphoto2/gphoto2-port.h>

#define MAX_DLSIZE 0xfa00

typedef unsigned char Info;

struct _CameraPrivateLibrary {
	unsigned char model;
	unsigned char init_done;
	int can_do_capture;
	int blocksize;
	int nb_entries;
	int data_reg_opened;
	unsigned long total_data_in_camera;
	unsigned long data_to_read;
	unsigned char *data_cache;
	unsigned long bytes_read_from_camera;
	unsigned long bytes_put_away;
	Info table[0x4000];
};


int jl2005c_init (Camera *camera, GPPort *port, CameraPrivateLibrary *priv);
int jl2005c_open_data_reg (Camera *camera, GPPort *port);
int jl2005c_get_pic_data_size (CameraPrivateLibrary *priv, Info *table, int n);
unsigned long jl2005c_get_start_of_photo (CameraPrivateLibrary *priv,
						Info *table, unsigned int n);

int set_usb_in_endpoint       (Camera *camera, int inep);
int jl2005c_read_data  (GPPort *port, char *data, int size);
int jl2005c_reset (Camera *camera, GPPort *port);
int jl2005c_delete_all (Camera *camera, GPPort *port);




#endif

