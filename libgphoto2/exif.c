/** \file
 * \brief Old EXIF file library for GPHOTO package

 * \author Copyright 2017 Marcus Meissner

 * \par License
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \par
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
*/

/* Contained some EXIF functions previously. We now defer to libexif usage. */

#include "exif.h"

#include <stdio.h>

/*
 * Gets the thumbnail of an EXIF image.
 * The thumbnail size is provided
 * 
 * No longer provided, use libexif directly please.
 */
unsigned char *gpi_exif_get_thumbnail_and_size(void *exifdat, long *size) { return NULL; }

/* No longer provided */
int gpi_exif_stat(void *exifdata) { return -1; }
