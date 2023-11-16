/* qtkt-decoder.c
 *
   Copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net
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

int qtk_thumbnail_decode(unsigned char *raw, unsigned char **out) {
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

	for (s = 0; s < QT1X0_THUMB_SIZE; s++) {
		v = raw[s];

		p1 = ((v >> 4) & 0b00001111) << 4;
    p2 = ((v >> 0) & 0b00001111) << 4;

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
