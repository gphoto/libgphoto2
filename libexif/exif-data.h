/* exif-data.h
 *
 * Copyright (C) 2001 Lutz Müller <lutz@users.sourceforge.net>
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

#ifndef __EXIF_DATA_H__
#define __EXIF_DATA_H__

#include <libexif/exif-tag.h>
#include <libexif/exif-content.h>

typedef struct _ExifData        ExifData;
typedef struct _ExifDataPrivate ExifDataPrivate;

struct _ExifData
{
	ExifContent *ifd0;
	ExifContent *ifd1;
	ExifContent *ifd_exif;
	ExifContent *ifd_gps;
	ExifContent *ifd_interoperability;

	unsigned char *data;
	unsigned int size;

	ExifDataPrivate *priv;
};

ExifData *exif_data_new   (void);
ExifData *exif_data_new_from_file (const char *path);
ExifData *exif_data_new_from_data (const unsigned char *data,
				   unsigned int size);

void      exif_data_load_data (ExifData *data, const unsigned char *d, 
			       unsigned int size);
void      exif_data_save_data (ExifData *data, unsigned char **d,
			       unsigned int *size);

void      exif_data_ref   (ExifData *data);
void      exif_data_unref (ExifData *data);
void      exif_data_free  (ExifData *data);

void      exif_data_dump  (ExifData *data);



#endif /* __EXIF_DATA_H__ */
