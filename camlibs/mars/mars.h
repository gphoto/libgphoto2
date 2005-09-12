/* mars.h
 *
 * Copyright (C) 2003 Theodore Kilgore <kilgota@auburn.edu>
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

#ifndef __mars_H__
#define __mars_H__

#include <libgphoto2_port/gphoto2-port.h>

typedef unsigned char Info;

/* Huffman table for decompressing the compressed mode */

typedef struct {
	int is_abs;
	int len;
	int val;
} code_table_t;



int mars_init              (Camera *camera, GPPort *port, Info *info);
int mars_reset	     (GPPort *port);
int mars_get_num_pics   (Info *info);
int mars_chk_compression   (Info *info, int n);
int mars_get_picture_width   (Info *info, int n);
int mars_get_pic_data_size (Info *info, int n);
int set_usb_in_endpoint	     (Camera *camera, int inep);
int set_usb_out_endpoint	     (Camera *camera, int outep);
int mars_read_data         (Camera *camera, GPPort *port, char *data, int size);
int mars_read_picture_data (Camera *camera, Info *info,
				GPPort *port, char *data, int size, int n);
int M_READ (GPPort *port, char *data, int size);
int M_COMMAND (GPPort *port, char *command, int size, char *response);
int mars_routine (Info *info, GPPort *port, 
					char param, int n); 

/* Some basic color correction */
int mars_postprocess(CameraPrivateLibrary *priv, int width, 
			int height, int is_compressed, unsigned char *rgb, int n);

/* The following are used for decompression of compressed-mode photos */
void precalc_table(code_table_t *table);
unsigned char get_bits(unsigned char *inp, int bitpos);
int mars_decompress (unsigned char *inp ,unsigned char *outp, int w, int h);

#endif

