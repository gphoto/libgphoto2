/* aox.h
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

#ifndef __AOX_H__
#define __AOX_H__

#include <libgphoto2_port/gphoto2-port.h>

typedef unsigned char Info;

typedef enum {
	MODEL_MINI
} Model;

int aox_init              (GPPort *port, Model *model, Info *info);
int aox_get_num_lo_pics   (Info *info);
int aox_get_num_hi_pics   (Info *info);
int aox_read_data         (GPPort *port, char *data, int size);
int aox_get_picture_size  (GPPort *port, int lo, int hi, int n, int k);
int aox_read_picture_data (GPPort *port, char *data, int size, int n);

#endif

