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
#include <libgphoto2/bayer.h>

int qtkt_decode(unsigned char *raw, int width, int height, unsigned char **out) {
	char *header;
	unsigned char *tmp, *ptr;
	unsigned char pixel[484][644];
	static const short gstep[16] =
	{ -89,-60,-44,-32,-22,-15,-8,-2,2,8,15,22,32,44,60,89 };
	static const short rstep[6][4] =
	{ {  -3,-1,1,3  }, {  -5,-1,1,5  }, {  -8,-2,2,8  },
		{ -13,-3,3,13 }, { -19,-4,4,19 }, { -28,-6,6,28 } };
	int rb, row, col, sharp, val=0, len;

	header = qtk_ppm_header(width, height);
	if (header == NULL)
		return GP_ERROR_NO_MEMORY;

	len = qtk_ppm_size(width, height);

	getbits(-1, 0);
	memset (pixel, 0x80, sizeof pixel);
	for (row=2; row < height+2; row++) {
		for (col=2+(row & 1); col < width+2; col+=2) {
			val = ((pixel[row-1][col-1] + 2*pixel[row-1][col+1] +
						pixel[row][col-2]) >> 2) + gstep[getbits(4, &raw)];
			pixel[row][col] = val = LIM(val,0,255);
			if (col < 4)
				pixel[row][col-2] = pixel[row+1][~row & 1] = val;
			if (row == 2)
				pixel[row-1][col+1] = pixel[row-1][col+3] = val;
		}
		pixel[row][col] = val;
	}

	for (rb=0; rb < 2; rb++) {
		for (row=2+rb; row < height+2; row+=2) {
			for (col=3-(row & 1); col < width+2; col+=2) {
				if (row < 4 || col < 4) {
					sharp = 2;
				} else {
					val = ABS(pixel[row-2][col] - pixel[row][col-2])
						+ ABS(pixel[row-2][col] - pixel[row-2][col-2])
						+ ABS(pixel[row][col-2] - pixel[row-2][col-2]);
					sharp = val <  4 ? 0 : val <  8 ? 1 : val < 16 ? 2 :
										val < 32 ? 3 : val < 48 ? 4 : 5;
				}
				val = ((pixel[row-2][col] + pixel[row][col-2]) >> 1)
							+ rstep[sharp][getbits(2, &raw)];
				pixel[row][col] = val = LIM(val,0,255);
				if (row < 4) pixel[row-2][col+2] = val;
				if (col < 4) pixel[row+2][col-2] = val;
			}
		}
	}

	for (row=2; row < height+2; row++) {
		for (col=3-(row & 1); col < width+2; col+=2) {
			val = ((pixel[row][col-1] + (pixel[row][col] << 2) +
						pixel[row][col+1]) >> 1) - 0x100;
			pixel[row][col] = LIM(val,0,255);
		}
	}

	tmp = malloc((size_t)(width + 4) * (size_t)(height + 4));
	if (tmp == NULL) {
		free(header);
		return GP_ERROR_NO_MEMORY;
	}

	for (row=0; row < height; row++) {
		for (col=0; col < width; col++) {
			RAW(tmp, row, col) = pixel[row+2][col+2];
		}
	}

	*out = calloc(1, len);
	if (*out == NULL) {
		free(header);
		free(tmp);
		return GP_ERROR_NO_MEMORY;
	}

	strcpy((char *)*out, header);
	ptr = *out + strlen(header);
	free(header);

	gp_bayer_decode(tmp, width, height, ptr, BAYER_TILE_GBRG);
	free(tmp);

	return GP_OK;
}
