/* sonix.h
 *
 * Copyright (C) 2005 Theodore Kilgore <kilgota@auburn.edu>
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

#ifndef __SONIX_H__
#define __SONIX_H__

#include <gphoto2/gphoto2-port.h>

#define DECOMP	1
#define REVERSE	2

struct _CameraPrivateLibrary {
/*	Model model; */ 	/* "model" and "fwversion" functionally equal */
	int num_pics;
	unsigned char size_code[512];
	unsigned char fwversion[4];
	unsigned char full;
	unsigned char avitype;
	unsigned char post;
	unsigned char offset;
	unsigned char avi_offset;
	unsigned char can_do_capture;
	int sonix_init_done;
};


int sonix_init              	(GPPort *port, CameraPrivateLibrary *priv);
int sonix_get_picture_size   	(GPPort *port, int n);
int sonix_read_data_size 	(GPPort *port, int n);
int sonix_delete_all_pics      	(GPPort *port);
int sonix_delete_last      	(GPPort *port);
int sonix_capture_image      	(GPPort *port);
int sonix_exit		      	(GPPort *port);
int sonix_decode		(unsigned char * dst, unsigned char * src, 
				    int width, int height);
int sonix_byte_reverse (unsigned char *imagedata, int datasize);
int sonix_rows_reverse (unsigned char *imagedata, int width, int height);
int sonix_cols_reverse (unsigned char *imagedata, int width, int height);
int white_balance (unsigned char *data, unsigned int size, float saturation);

#endif 

