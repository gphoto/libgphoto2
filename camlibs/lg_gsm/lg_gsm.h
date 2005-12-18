/* lg_gsm.h
 *
 * Copyright (C) 2005 Guillaume Bedot <littletux@zarb.org>
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

#ifndef __LG_GSM_H__
#define __LG_GSM_H__

#include <libgphoto2_port/gphoto2-port.h>

typedef unsigned char Info;

typedef enum {
	MODEL_LG_T5100
} Model;

int lg_gsm_init              (GPPort *port, Model *model, Info *info);
int lg_gsm_get_picture_size  (GPPort *port, int pic);
int lg_gsm_read_picture_data (GPPort *port, char *data, int size, int n);
int lg_gsm_list_files (GPPort *port, CameraList *list);

#endif

