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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __LG_GSM_H__
#define __LG_GSM_H__

#include <gphoto2/gphoto2-port.h>

typedef unsigned char Info;

typedef enum {
	MODEL_LG_T5100
} Model;

int lg_gsm_init              (GPPort *port, Model *model, Info *info);
unsigned int lg_gsm_get_picture_size  (GPPort *port, int pic);
int lg_gsm_read_picture_data (GPPort *port, char *data, int size, int n);
int lg_gsm_list_files (GPPort *port, CameraList *list);

#endif

