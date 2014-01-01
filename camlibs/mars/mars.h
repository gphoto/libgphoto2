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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __mars_H__
#define __mars_H__

#include <unistd.h>
#include <gphoto2/gphoto2-port.h>

typedef unsigned char Info;

/* Huffman table for decompressing the compressed mode */

typedef struct {
	int is_abs;
	int len;
	int val;
} code_table_t;

#define MARS_SLEEP	10000

int mars_init              (Camera *camera, GPPort *port, Info *info);
int mars_reset	     (GPPort *port);
int mars_get_num_pics   (Info *info);
int mars_get_pic_data_size (Info *info, int n);
int mars_read_picture_data (Camera *camera, Info *info,
				GPPort *port, char *data, int size, int n);

int mars_decompress (unsigned char *inp ,unsigned char *outp, int w, int h);
int histogram (unsigned char *data, unsigned int size, int *htable_r, 
                                        int *htable_g, int *htable_b);
int mars_white_balance (unsigned char *data, unsigned int size, float saturation,
                                        float image_gamma);
#endif

