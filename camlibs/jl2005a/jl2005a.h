/* jl2005a.h
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __jl2005a_H__
#define __jl2005a_H__

#include <unistd.h>
#include <gphoto2/gphoto2-port.h>

struct _CameraPrivateLibrary {
	unsigned char *catalog;
	int nb_entries;
	int last_fetched_entry;
	int data_reg_accessed;
	unsigned long data_to_read;
	unsigned char *data_cache;
	int data_used_from_block;
};


int jl2005a_init              (Camera *camera, GPPort *port, 
					    CameraPrivateLibrary *priv);
int jl2005a_get_pic_data_size (GPPort *port, int n);
int jl2005a_get_pic_width (GPPort *port);
int jl2005a_get_pic_height (GPPort *port);
int set_usb_in_endpoint	     (Camera *camera, int inep);
int jl2005a_read_picture_data ( Camera *camera,
				GPPort *port, unsigned char *data, 
				unsigned int size);
int jl2005a_reset	     (Camera *camera, GPPort *port);
int jl2005a_read_info_byte(GPPort *port, int n);
int jl2005a_shortquery(GPPort *port, int n);
int jl2005a_decompress (unsigned char *inp, unsigned char *outp, int width,
				int height);

#endif

