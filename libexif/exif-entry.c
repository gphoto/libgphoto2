/* exif-entry.c
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
#include "exif-entry.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

//#define DEBUG

#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

static const unsigned char ExifHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};

struct _ExifEntryPrivate
{
	unsigned int ref_count;
};

ExifEntry *
exif_entry_new (void)
{
	ExifEntry *entry;

	entry = malloc (sizeof (ExifEntry));
	if (!entry)
		return (NULL);
	memset (entry, 0, sizeof (ExifEntry));
	entry->priv = malloc (sizeof (ExifEntryPrivate));
	if (!entry->priv) {
		free (entry);
		return (NULL);
	}
	memset (entry->priv, 0, sizeof (ExifEntryPrivate));
	entry->priv->ref_count = 1;

	return (entry);
}

void
exif_entry_ref (ExifEntry *entry)
{
	entry->priv->ref_count++;
}

void
exif_entry_unref (ExifEntry *entry)
{
	entry->priv->ref_count--;
	if (!entry->priv->ref_count)
		exif_entry_free (entry);
}

void
exif_entry_free (ExifEntry *entry)
{
	if (entry->data)
		free (entry->data);
	free (entry->priv);
	free (entry);
}

void
exif_entry_dump (ExifEntry *entry, unsigned int indent)
{
	char buf[1024];
	unsigned int i;

	for (i = 0; i < 2 * indent; i++)
		buf[i] = ' ';
	buf[i] = '\0';

	if (!entry)
		return;

	printf ("%sTag: 0x%x ('%s')\n", buf, entry->tag,
		exif_tag_get_name (entry->tag));
	printf ("%s  Format: %i ('%s')\n", buf, entry->format,
		exif_format_get_name (entry->format));
	printf ("%s  Components: %i\n", buf, (int) entry->components);
	printf ("%s  Size: %i\n", buf, entry->size);
	printf ("%s  Value: %s\n", buf, exif_entry_get_value (entry));
}

const char *
exif_entry_get_value (ExifEntry *entry)
{
	unsigned int i;
	ExifByte v_byte;
	ExifShort v_short;
	ExifLong v_long;
	ExifSLong v_slong;
	ExifRational v_rat;
	ExifSRational v_srat;
	static char v[1024], b[1024];

	memset (v, 0, sizeof (v));
	switch (entry->tag) {
	case EXIF_TAG_EXIF_VERSION:
		if (!memcmp (entry->data, "0200", 4))
			strncpy (v, "Exif Version 2.0", sizeof (v));
		else if (!memcmp (entry->data, "0210", 4))
			strncpy (v, "Exif Version 2.1", sizeof (v));
		else
			strncpy (v, "Unknown Exif Version", sizeof (v));
		break;
	case EXIF_TAG_FLASH_PIX_VERSION:
		if (!memcmp (entry->data, "0100", 4))
			strncpy (v, "FlashPix Version 1.0", sizeof (v));
		else
			strncpy (v, "Unknown FlashPix Version", sizeof (v));
		break;
	case EXIF_TAG_COPYRIGHT:
		if (strlen (entry->data))
			strncpy (v, entry->data, sizeof (v));
		else
			strncpy (v, "[None]", sizeof (v));
		strncat (v, " (Photographer) - ", sizeof (v));
		if (strlen (entry->data + strlen (entry->data) + 1))
			strncat (v, entry->data + strlen (entry->data) + 1,
				 sizeof (v));
		else
			strncat (v, "[None]", sizeof (v));
		strncat (v, " (Editor)", sizeof (v));
		break;
	case EXIF_TAG_APERTURE_VALUE:
		v_rat = exif_get_rational (entry->data, entry->order);
		snprintf (b, sizeof (b), "%i/%i", (int) v_rat.numerator,
						  (int) v_rat.denominator);
		snprintf (v, sizeof (v), "%s (APEX: f%.01f)", b,
			  pow (2 , ((float) v_rat.numerator /
				    (float) v_rat.denominator) / 2.));
		break;
	case EXIF_TAG_SHUTTER_SPEED_VALUE:
		v_srat = exif_get_srational (entry->data, entry->order);
		snprintf (b, sizeof (b), "%.0f/%.0f sec.",
			  (float) v_srat.numerator, (float) v_srat.denominator);
		snprintf (v, sizeof (v), "%s (APEX: %i)", b,
			  (int) pow (sqrt(2), (float) v_srat.numerator /
					      (float) v_srat.denominator));
		break;
	case EXIF_TAG_BRIGHTNESS_VALUE:
		v_srat = exif_get_srational (entry->data, entry->order);
		snprintf (v, sizeof (v), "%i/%i", (int) v_srat.numerator, 
						  (int) v_srat.denominator);
		//FIXME: How do I calculate the APEX value?
		break;
	default:
		switch (entry->format) {
		case EXIF_FORMAT_UNDEFINED:
			break;
		case EXIF_FORMAT_BYTE:
			v_byte = entry->data[0];
			snprintf (v, sizeof (v), "%i", v_byte);
			break;
		case EXIF_FORMAT_SHORT:
			v_short = exif_get_short (entry->data, entry->order);
			snprintf (v, sizeof (v), "%i", v_short);
			break;
		case EXIF_FORMAT_LONG:
			v_long = exif_get_long (entry->data, entry->order);
			snprintf (v, sizeof (v), "%i", (int) v_long);
			break;
		case EXIF_FORMAT_SLONG:
			v_slong = exif_get_slong (entry->data, entry->order);
			snprintf (v, sizeof (v), "%i", (int) v_slong);
			break;
		case EXIF_FORMAT_ASCII:
			strncpy (v, entry->data, MIN (sizeof (v), entry->size));
			break;
		case EXIF_FORMAT_RATIONAL:
			v_rat = exif_get_rational (entry->data, entry->order);
			snprintf (v, sizeof (v), "%i/%i",
				  (int) v_rat.numerator,
				  (int) v_rat.denominator);
			for (i = 1; i < entry->components; i++) {
				v_rat = exif_get_rational (
					entry->data + 8 * i, entry->order);
				snprintf (b, sizeof (b), "%i/%i",
					  (int) v_rat.numerator,
					  (int) v_rat.denominator);
				strncat (v, ", ", sizeof (v));
				strncat (v, b, sizeof (v));
			}
			break;
		case EXIF_FORMAT_SRATIONAL:
			v_srat = exif_get_srational (entry->data, entry->order);
			snprintf (v, sizeof (v), "%i/%i",
				  (int) v_srat.numerator,
				  (int) v_srat.denominator);
			for (i = 1; i < entry->components; i++) {
				v_srat = exif_get_srational (
					entry->data + 8 * i, entry->order);
				snprintf (b, sizeof (b), "%i/%i",
					  (int) v_srat.numerator,
					  (int) v_srat.denominator);
				strncat (v, ", ", sizeof (v));
				strncat (v, b, sizeof (v));
			}
			break;
		}
	}

	return (v);
}

void
exif_entry_initialize (ExifEntry *entry, ExifTag tag)
{
	time_t t;
	struct tm *tm;
	ExifRational r;

	if (!entry)
		return;
	if (!entry->parent || entry->data)
		return;

	entry->order = entry->parent->order;
	entry->tag = tag;
	switch (tag) {
	case EXIF_TAG_EXIF_IFD_POINTER:
	case EXIF_TAG_GPS_INFO_IFD_POINTER:
	case EXIF_TAG_INTEROPERABILITY_IFD_POINTER:
		entry->components = 1;
		entry->format = EXIF_FORMAT_LONG;
		entry->size = sizeof (ExifLong);
		entry->data = malloc (entry->size);
		exif_set_long (entry->data, entry->order, 0);
		break;
	case EXIF_TAG_IMAGE_WIDTH:
	case EXIF_TAG_IMAGE_LENGTH:
		entry->components = 1;
		entry->format = EXIF_FORMAT_SHORT;
		entry->size = sizeof (ExifShort);
		entry->data = malloc (entry->size);
		exif_set_short (entry->data, entry->order, 0);
		break;
	case EXIF_TAG_BITS_PER_SAMPLE:
		entry->components = 3;
		entry->format = EXIF_FORMAT_SHORT;
		entry->size = sizeof (ExifShort) * entry->components;
		entry->data = malloc (entry->size);
		exif_set_short (entry->data + 0, entry->order, 8);
		exif_set_short (entry->data + 2, entry->order, 8);
		exif_set_short (entry->data + 4, entry->order, 8);
		break;
	case EXIF_TAG_YCBCR_SUB_SAMPLING:
		entry->components = 2;
		entry->size = sizeof (ExifShort) * entry->components;
		entry->data = malloc (entry->size);
		exif_set_short (entry->data + 0, entry->order, 2);
		exif_set_short (entry->data + 2, entry->order, 1);
		break;
	case EXIF_TAG_COMPRESSION:
	case EXIF_TAG_ORIENTATION:
	case EXIF_TAG_PLANAR_CONFIGURATION:
	case EXIF_TAG_YCBCR_POSITIONING:
		entry->components = 1;
		entry->format = EXIF_FORMAT_SHORT;
		entry->size = sizeof (ExifShort);
		entry->data = malloc (entry->size);
		exif_set_short (entry->data, entry->order, 1);
		break;
	case EXIF_TAG_PHOTOMETRIC_INTERPRETATION:
	case EXIF_TAG_RESOLUTION_UNIT:
		entry->components = 1;
		entry->format = EXIF_FORMAT_SHORT;
		entry->size = sizeof (ExifShort);
		entry->data = malloc (entry->size);
		exif_set_short (entry->data, entry->order, 2);
		break;
	case EXIF_TAG_SAMPLES_PER_PIXEL:
		entry->components = 1; 
		entry->format = EXIF_FORMAT_SHORT; 
		entry->size = sizeof (ExifShort);
		entry->data = malloc (entry->size);
		exif_set_short (entry->data, entry->order, 3);
		break;
	case EXIF_TAG_X_RESOLUTION:
	case EXIF_TAG_Y_RESOLUTION:
		entry->components = 1;
		entry->format = EXIF_FORMAT_RATIONAL;
		entry->size = sizeof (ExifRational);
		entry->data = malloc (entry->size);
		r.numerator = 72;
		r.denominator = 1;
		exif_set_rational (entry->data, entry->order, r);
		break;
	case EXIF_TAG_REFERENCE_BLACK_WHITE:
		entry->components = 6;
		entry->format = EXIF_FORMAT_RATIONAL;
		entry->size = sizeof (ExifRational) * entry->components;
		entry->data = malloc (entry->size);
		r.denominator = 1;
		r.numerator = 0;
		exif_set_rational (entry->data +   0, entry->order, r);
		r.numerator = 255;
		exif_set_rational (entry->data +   8, entry->order, r);
		r.numerator = 0;
		exif_set_rational (entry->data +  16, entry->order, r);
		r.numerator = 255;
		exif_set_rational (entry->data +  32, entry->order, r);
		r.numerator = 0;
		exif_set_rational (entry->data +  64, entry->order, r);
		r.numerator = 255;
		exif_set_rational (entry->data + 128, entry->order, r);
		break;
	case EXIF_TAG_DATE_TIME:
	case EXIF_TAG_DATE_TIME_ORIGINAL:
	case EXIF_TAG_DATE_TIME_DIGITIZED:
		t = time (NULL);
		tm = localtime (&t);
		entry->components = 20;
		entry->format = EXIF_FORMAT_ASCII;
		entry->size = sizeof (ExifByte) * entry->components;
		entry->data = malloc (entry->size);
		snprintf (entry->data, entry->size, 
			  "%04i:%02i:%02i %02i:%02i:%02i",
			  tm->tm_year + 1900, tm->tm_mon, tm->tm_mday,
			  tm->tm_hour, tm->tm_min, tm->tm_sec);
		break;
	case EXIF_TAG_IMAGE_DESCRIPTION:
	case EXIF_TAG_MAKE:
	case EXIF_TAG_MODEL:
	case EXIF_TAG_SOFTWARE:
	case EXIF_TAG_ARTIST:
		entry->components = strlen ("[None]") + 1;
		entry->format = EXIF_FORMAT_ASCII;
		entry->size = sizeof (ExifByte) * entry->components;
		entry->data = malloc (entry->size);
		strncpy (entry->data, "[None]", entry->size);
		break;
	case EXIF_TAG_COPYRIGHT:
		entry->components = (strlen ("[None]") + 1) * 2;
		entry->format = EXIF_FORMAT_ASCII;
		entry->size = sizeof (ExifByte) * entry->components;
		entry->data = malloc (entry->size);
		strcpy (entry->data +                     0, "[None]");
		strcpy (entry->data + strlen ("[None]") + 1, "[None]");
		break;
	case EXIF_TAG_EXIF_VERSION:
		entry->components = 4;
		entry->format = EXIF_FORMAT_UNDEFINED;
		entry->size = sizeof (ExifUndefined) * entry->components;
		entry->data = malloc (entry->size);
		memcpy (entry->data, "0210", 4);
		break;
	case EXIF_TAG_FLASH_PIX_VERSION:
		entry->components = 4;
		entry->format = EXIF_FORMAT_UNDEFINED;
		entry->size = sizeof (ExifUndefined) * entry->components;
		entry->data = malloc (entry->size);
		memcpy (entry->data, "0100", 4);
		break;
	default:
		break;
	}
}
