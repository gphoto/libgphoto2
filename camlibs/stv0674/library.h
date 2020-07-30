/*
 * STV0674 Vision Camera Chipset Driver
 * Copyright 2002 Vincent Sanders <vince@kyllikki.org>
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

#ifndef LIBRARY_H
#define LIBRARY_H

#include <gphoto2/gphoto2-port.h>

int stv0674_ping              (GPPort *);
int stv0674_summary           (GPPort *, char*);
int stv0674_file_count        (GPPort *, int *count);
int stv0674_get_image         (GPPort *, int image_no, CameraFile *file);
int stv0674_get_image_raw     (GPPort *, int image_no, CameraFile *file);
int stv0674_get_image_preview (GPPort *, int image_no, CameraFile *file);

int stv0674_capture_preview   (GPPort *device, char **data, int *size);
int stv0674_capture	      (GPPort *port);
int stv0674_delete_all	      (GPPort *port);

#endif
