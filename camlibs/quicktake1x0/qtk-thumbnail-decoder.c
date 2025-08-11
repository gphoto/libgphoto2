/* qtkt-decoder.c
 *
 * Copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net
 * Copyright 2023, Colin Leroy-Mira <colin@colino.net>
 *
 * QTKT decoder heavily inspired from dcraw.c.
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
#include "config.h"

#include "quicktake1x0.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>

static int qt100_thumbnail_decode(unsigned char *raw, unsigned char **out) {
	int s, v, p1, p2, r, g, b, len;
	char *header;
	unsigned char *ptr;

	header = qtk_ppm_header(QT1X0_THUMB_WIDTH, QT1X0_THUMB_HEIGHT);

	len = qtk_ppm_size(QT1X0_THUMB_WIDTH, QT1X0_THUMB_HEIGHT);
	*out = ptr = calloc(1, len);
	if (ptr == NULL) {
		free(header);
		return GP_ERROR_NO_MEMORY;
	}

	strcpy((char *)ptr, header);
	ptr += strlen(header);
	free(header);

	/* The 2400 bytes buffer represent 80x60 pixels at 4bpp.
	 * It is very logical to decode as each half-byte represents
	 * the next pixel.
	 */
	for (s = 0; s < QT1X0_THUMB_SIZE; s++) {
		v = raw[s];

		p1 = ((v >> 4) & 0x0F) << 4;
		p2 = ((v >> 0) & 0x0F) << 4;

		/* FIXME do color thumbnails */
		r = g = b = p1;
		*ptr++ = r;
		*ptr++ = g;
		*ptr++ = b;

		r = g = b = p2;
		*ptr++ = r;
		*ptr++ = g;
		*ptr++ = b;
	}

	return GP_OK;
}

static int qt150_thumbnail_decode(unsigned char *raw, unsigned char **out) {
	int len;
	char *header;
	unsigned char *ptr;
	unsigned char *cur_in;
	unsigned char line[QT1X0_THUMB_WIDTH*2], *cur_out;
	int i, a, b, c, d, y;

	header = qtk_ppm_header(QT1X0_THUMB_WIDTH, QT1X0_THUMB_HEIGHT);

	len = qtk_ppm_size(QT1X0_THUMB_WIDTH, QT1X0_THUMB_HEIGHT);
	*out = ptr = calloc(1, len);
	if (ptr == NULL) {
		free(header);
		return GP_ERROR_NO_MEMORY;
	}

	strcpy((char *)ptr, header);
	ptr += strlen(header);
	free(header);

	/* The 2400 bytes buffer represent 80x60 pixels at 4bpp.
	 * It is harder to decode as the half-bytes do not
	 * represent one pixel after the other.
	 * Every 80 bytes represent 2 lines. The first 60 bytes
	 * (120 half-bytes) represent pixels at:
	 * 0,y ; 1,y ; 0,y+1 ; 2,y ; 3,y ; 2,y+1 ; etc
	 * The last 20 bytes (40 half-bytes) represent the pixels
	 * at 1,y+1 ; 3,y+1 ; 5,y+1 ; etc
	 */
	cur_in = raw;
	for (y = 0; y < QT1X0_THUMB_HEIGHT; y+=2) {
		cur_out = line;
		for (i = 0; i < QT1X0_THUMB_WIDTH; i++) {
			c = *cur_in++;
			a   = (((c>>4) & 0x0F) << 4);
			b   = (((c)    & 0x0F) << 4);
			*cur_out++ = a;
			*cur_out++ = b;
		}
		cur_out = line;

		for (i = 0; i < QT1X0_THUMB_WIDTH * 2; ) {
			if (i < QT1X0_THUMB_WIDTH*3/2) {
				a = *cur_out++;
				b = *cur_out++;
				c = *cur_out++;

				*(ptr) = a;
				*(ptr + 3) = b;
				*(ptr + 3*QT1X0_THUMB_WIDTH) = c;
				ptr++;

				*(ptr) = a;
				*(ptr + 3) = b;
				*(ptr + 3*QT1X0_THUMB_WIDTH) = c;
				ptr++;

				*(ptr) = a;
				*(ptr + 3) = b;
				*(ptr + 3*QT1X0_THUMB_WIDTH) = c;
				ptr++;

				i+=3;
				ptr+=3;
			} else {
				i++;
				ptr += 3;

				d = *cur_out++;
				*(ptr++) = d;
				*(ptr++) = d;
				*(ptr++) = d;
			}
		}
	}

	return GP_OK;
}

int qtk_thumbnail_decode(unsigned char *raw, unsigned char **out, Quicktake1x0Model model) {
	if (model == QUICKTAKE_MODEL_100) {
		return qt100_thumbnail_decode(raw, out);
	} else {
		return qt150_thumbnail_decode(raw, out);
	}
}
