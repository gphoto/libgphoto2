/* exif-utils.c
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

#include <config.h>
#include "exif-utils.h"

typedef signed short ExifSShort;

static ExifSShort
exif_get_sshort (const unsigned char *buf, ExifByteOrder order)
{
        switch (order) {
        case EXIF_BYTE_ORDER_MOTOROLA:
                return ((buf[0] << 8) | buf[1]);
        case EXIF_BYTE_ORDER_INTEL:
                return ((buf[1] << 8) | buf[0]);
        }

	/* Won't be reached */
	return (0);
}

ExifShort
exif_get_short (const unsigned char *buf, ExifByteOrder order)
{
	return (exif_get_sshort (buf, order) & 0xffff);
}

void
exif_set_short (unsigned char *b, ExifByteOrder order, ExifShort value)
{
	switch (order) {
	case EXIF_BYTE_ORDER_MOTOROLA:
		b[0] = value >> 8;
		b[1] = value;
		break;
	case EXIF_BYTE_ORDER_INTEL:
		b[0] = value;
		b[1] = value >> 8;
		break;
	}
}

ExifSLong
exif_get_slong (const unsigned char *b, ExifByteOrder order)
{
        switch (order) {
        case EXIF_BYTE_ORDER_MOTOROLA:
                return ((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
        case EXIF_BYTE_ORDER_INTEL:
                return ((b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0]);
        }

	/* Won't be reached */
	return (0);
}

void
exif_set_slong (unsigned char *b, ExifByteOrder order, ExifSLong value)
{
	switch (order) {
	case EXIF_BYTE_ORDER_MOTOROLA:
		b[0] = value >> 24;
		b[1] = value >> 16;
		b[2] = value >> 8;
		b[3] = value;
		break;
	case EXIF_BYTE_ORDER_INTEL:
		b[3] = value >> 24;
		b[2] = value >> 16;
		b[1] = value >> 8;
		b[0] = value;
		break;
	}
}

ExifLong
exif_get_long (const unsigned char *buf, ExifByteOrder order)
{
        return (exif_get_slong (buf, order) & 0xffffffff);
}

void
exif_set_long (unsigned char *b, ExifByteOrder order, ExifLong value)
{
	exif_set_slong (b, order, value);
}

ExifSRational
exif_get_srational (const unsigned char *buf, ExifByteOrder order)
{
	ExifSRational r;

	r.numerator   = exif_get_slong (buf, order);
	r.denominator = exif_get_slong (buf + 4, order);

	return (r);
}

ExifRational
exif_get_rational (const unsigned char *buf, ExifByteOrder order)
{
	ExifRational r;

	r.numerator   = exif_get_long (buf, order);
	r.denominator = exif_get_long (buf + 4, order);

	return (r);
}

void
exif_set_rational (unsigned char *buf, ExifByteOrder order,
		   ExifRational value)
{
	exif_set_long (buf, order, value.numerator);
	exif_set_long (buf + 4, order, value.denominator);
}

void
exif_set_srational (unsigned char *buf, ExifByteOrder order,
		    ExifSRational value)
{
	exif_set_slong (buf, order, value.numerator);
	exif_set_slong (buf + 4, order, value.denominator);
}
