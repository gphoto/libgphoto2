/* exif-format.h
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

#ifndef __EXIF_FORMAT_H__
#define __EXIF_FORMAT_H__

typedef enum _ExifFormat ExifFormat;
enum _ExifFormat {
        EXIF_FORMAT_BYTE       =  1,
        EXIF_FORMAT_ASCII      =  2,
        EXIF_FORMAT_SHORT      =  3,
        EXIF_FORMAT_LONG       =  4,
        EXIF_FORMAT_RATIONAL   =  5,
        EXIF_FORMAT_UNDEFINED  =  7,
        EXIF_FORMAT_SLONG      =  9,
        EXIF_FORMAT_SRATIONAL  = 10,
};

const char   *exif_format_get_name (ExifFormat format);
unsigned char exif_format_get_size (ExifFormat format);

#endif /* __EXIF_FORMAT_H__ */
