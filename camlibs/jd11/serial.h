/*
 * Jenopt JD11 Camera Driver
 * Copyright (C) 1999-2001 Marcus Meissner <marcus@jet.franken.de> 
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

#ifndef _JD11_SERIAL_H
#define _JD11_SERIAL_H

#include <gphoto2.h>
#include <gphoto2-port.h>

extern int jd11_file_count(GPPort *port, int *count);
extern int jd11_get_image_preview(Camera *camera,CameraFile *file,int nr, char **data, int *size, GPContext *context);
extern int jd11_get_image_full(Camera *camera,CameraFile *file,int nr, char **data, int *size,int raw, GPContext *context);
extern int jd11_erase_all(GPPort *port);
extern int jd11_ping(GPPort *port);
extern int jd11_float_query(GPPort *port);
extern int jd11_select_index(GPPort *port);
extern int jd11_select_image(GPPort *port, int nr);
extern int jd11_set_bulb_exposure(GPPort *port, int nr);
extern int jd11_set_rgb(GPPort *port, float red, float green, float blue);
extern int jd11_get_rgb(GPPort *port, float *red, float *green, float *blue);
#endif
