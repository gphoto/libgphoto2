/*
 * STV0680 Vision Camera Chipset Driver
 * Copyright 2000 Adam Harrison <adam@antispin.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBRARY_H
#define LIBRARY_H

#include <gphoto2/gphoto2-port.h>

int stv0680_ping              (GPPort *);
int stv0680_summary           (GPPort *, char*);
int stv0680_file_count        (GPPort *, int *count);
int stv0680_get_image         (GPPort *, int image_no, CameraFile *file);
int stv0680_get_image_raw     (GPPort *, int image_no, CameraFile *file);
int stv0680_get_image_preview (GPPort *, int image_no, CameraFile *file);

int stv0680_capture_preview   (GPPort *device, char **data, int *size);
int stv0680_capture	      (GPPort *port);
int stv0680_delete_all	      (GPPort *port);

#endif
