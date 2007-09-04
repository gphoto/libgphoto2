/* jl2005c.h
 *
 * Copyright (C) 2006 Theodore Kilgore <kilgota@auburn.edu>
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

#ifndef __jl2005c_H__
#define __jl2005c_H__

#include <unistd.h>
#include <gphoto2/gphoto2-port.h>

typedef unsigned char Info;

struct _CameraPrivateLibrary {
	unsigned char *catalog;
	int nb_entries;
	int last_fetched_entry;
	unsigned long total_data_in_camera;
	unsigned long data_to_read;
	unsigned char *data_cache;
	unsigned long bytes_read_from_camera;
	int data_used_from_block;
	unsigned long bytes_put_away;	
	Info info[0xe000];
};


int jl2005c_init              (Camera *camera, GPPort *port, 
					    CameraPrivateLibrary *priv);
int jl2005c_reset	     (Camera *camera, GPPort *port);
int jl2005c_get_num_pics   (Info *info);
int jl2005c_get_resolution      (Info *info, int n);

int jl2005c_get_compression (Info *info, int n);
int jl2005c_get_width (Info *info, int n);
int jl2005c_get_pic_data_size (Info *info, int n);
unsigned long jl2005c_get_start_of_photo(Info *info, unsigned int n);
int set_usb_in_endpoint	     (Camera *camera, int inep);
int jl2005c_get_picture_data (GPPort *port, char *data, int size);
#endif

