/*
 * Jenopt JD11 Camera Driver
 * Copyright 1999-2001 Marcus Meissner <marcus@jet.franken.de>
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

#ifndef _JD11_SERIAL_H
#define _JD11_SERIAL_H

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

extern int jd11_index_reader(GPPort *port, CameraFilesystem *fs, GPContext *context);
extern int jd11_get_image_full(Camera *camera,CameraFile *file,int nr,int raw, GPContext *context);
extern int jd11_erase_all(GPPort *port);
extern int jd11_ping(GPPort *port);
extern int jd11_float_query(GPPort *port);
extern int jd11_select_index(GPPort *port);
extern int jd11_select_image(GPPort *port, int nr);
extern int jd11_set_bulb_exposure(GPPort *port, int nr);
extern int jd11_set_rgb(GPPort *port, float red, float green, float blue);
extern int jd11_get_rgb(GPPort *port, float *red, float *green, float *blue);


#define IMGHEADER "P6\n# gPhoto2 JD11 thumbnail image\n640 480 255\n"
#define THUMBHEADER "P5\n# gPhoto2 JD11 thumbnail image\n64 48 255\n"

#endif
