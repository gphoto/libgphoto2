/* exif-utils.h
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

#ifndef __EXIF_UTILS_H__
#define __EXIF_UTILS_H__

typedef enum _ExifByteOrder ExifByteOrder;
enum _ExifByteOrder {
        EXIF_BYTE_ORDER_MOTOROLA,
        EXIF_BYTE_ORDER_INTEL
};

typedef char		ExifByte;          /* 1 byte  */
typedef char *		ExifAscii;
typedef unsigned short	ExifShort;         /* 2 bytes */
typedef unsigned long	ExifLong;          /* 4 bytes */
typedef struct {ExifLong numerator; ExifLong denominator;} ExifRational;
typedef char		ExifUndefined;     /* 1 byte  */
typedef signed long	ExifSLong;         /* 4 bytes */
typedef struct {ExifSLong numerator; ExifSLong denominator;} ExifSRational;


ExifShort     exif_get_short     (const unsigned char *b, ExifByteOrder order);
ExifLong      exif_get_long      (const unsigned char *b, ExifByteOrder order);
ExifSLong     exif_get_slong     (const unsigned char *b, ExifByteOrder order);
ExifRational  exif_get_rational  (const unsigned char *b, ExifByteOrder order);
ExifSRational exif_get_srational (const unsigned char *b, ExifByteOrder order);

void exif_set_short     (unsigned char *b, ExifByteOrder order,
			 ExifShort value);
void exif_set_long      (unsigned char *b, ExifByteOrder order,
			 ExifLong value);
void exif_set_slong     (unsigned char *b, ExifByteOrder order,
			 ExifSLong value);
void exif_set_rational  (unsigned char *b, ExifByteOrder order,
			 ExifRational value);
void exif_set_srational (unsigned char *b, ExifByteOrder order,
			 ExifSRational value);

#endif /* __EXIF_UTILS_H__ */
