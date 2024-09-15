/* qtkn-decoder.c
 *
   Copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net
 * Copyright 2023, Colin Leroy-Mira <colin@colino.net>
 *
 * QTKN (RADC) decoder heavily inspired from dcraw.c.
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

#define radc_token(tree, ptr) ((signed char) getbithuff(8, ptr, huff[tree]))
#define PREDICTOR (c ? (buf[c][y-1][x] + buf[c][y][x+1]) / 2 \
                     : (buf[c][y-1][x+1] + 2*buf[c][y-1][x] + buf[c][y][x+1]) / 4)

int qtkn_decode(unsigned char *raw, int width, int height, unsigned char **out) {
	static const char src[] = {
		1,1, 2,3, 3,4, 4,2, 5,7, 6,5, 7,6, 7,8,
		1,0, 2,1, 3,3, 4,4, 5,2, 6,7, 7,6, 8,5, 8,8,
		2,1, 2,3, 3,0, 3,2, 3,4, 4,6, 5,5, 6,7, 6,8,
		2,0, 2,1, 2,3, 3,2, 4,4, 5,6, 6,7, 7,5, 7,8,
		2,1, 2,4, 3,0, 3,2, 3,3, 4,7, 5,5, 6,6, 6,8,
		2,3, 3,1, 3,2, 3,4, 3,5, 3,6, 4,7, 5,0, 5,8,
		2,3, 2,6, 3,0, 3,1, 4,4, 4,5, 4,7, 5,2, 5,8,
		2,4, 2,7, 3,3, 3,6, 4,1, 4,2, 4,5, 5,0, 5,8,
		2,6, 3,1, 3,3, 3,5, 3,7, 3,8, 4,0, 5,2, 5,4,
		2,0, 2,1, 3,2, 3,3, 4,4, 4,5, 5,6, 5,7, 4,8,
		1,0, 2,2, 2,-2,
		1,-3, 1,3,
		2,-17, 2,-5, 2,5, 2,17,
		2,-7, 2,2, 2,9, 2,18,
		2,-18, 2,-9, 2,-2, 2,7,
		2,-28, 2,28, 3,-49, 3,-9, 3,9, 4,49, 5,-79, 5,79,
		2,-1, 2,13, 2,26, 3,39, 4,-16, 5,55, 6,-37, 6,76,
		2,-26, 2,-13, 2,1, 3,-39, 4,16, 5,-55, 6,-76, 6,37
	};
	static const unsigned short pt[] =
		{ 0,0, 1280,1344, 2320,3616, 3328,8000, 4095,16383, 65535,16383 };
	unsigned short curve[0x10000];
	unsigned short huff[19][256];
	int row, col, tree, nreps, rep, step, i, c, s, r, x, y, val, len;
	short last[3] = { 16,16,16 }, mul[3], buf[3][3][386];
	char *header;
	unsigned short *tmp;
	unsigned char *tmp_c, *ptr;

	header = qtk_ppm_header(width, height);
	if (header == NULL)
		return GP_ERROR_NO_MEMORY;

	len = qtk_ppm_size(width, height);

	tmp = malloc((size_t)width * (size_t)height * sizeof(unsigned short));
	if (tmp == NULL) {
		free(header);
		return GP_ERROR_NO_MEMORY;
	}

	tmp_c = malloc((size_t)width * (size_t)height);
	if (tmp_c == NULL) {
		free(header);
		free(tmp);
		return GP_ERROR_NO_MEMORY;
	}

	for (i=2; i < 12; i+=2) {
		for (c=pt[i-2]; c <= pt[i]; c++) {
			curve[c] = (float)(c-pt[i-2]) / (pt[i]-pt[i-2]) * (pt[i+1]-pt[i-1]) + pt[i-1] + 0.5;
		}
	}

	for (s=i=0; i < (int)sizeof src; i+=2) {
		for (c=0; c < 256 >> src[i]; c++) {
			((unsigned short *)huff)[s++] = src[i] << 8 | (unsigned char)src[i+1];
		}
	}
	s = 3;
	for (c=0; c < 256; c++) {
		huff[18][c] = (8-s) << 8 | c >> s << s | 1 << (s-1);
	}
	getbits(-1, &raw);
	for (i=0; i < (int)(sizeof(buf)/sizeof(short)); i++) {
		((short *)buf)[i] = 2048;
	}

	for (row=0; row < height; row+=4) {
		for (c=0; c < 3; c++) {
			mul[c] = getbits(6, &raw);
		}
		for (c=0; c < 3; c++) {
			val = ((0x1000000/last[c] + 0x7ff) >> 12) * mul[c];
			s = val > 65564 ? 10:12;
			x = ~(-1 << (s-1));
			val <<= 12-s;
			for (i=0; i < (int)(sizeof(buf[0])/sizeof(short)); i++) {
				((short *)buf[c])[i] = (((short *)buf[c])[i] * val + x) >> s;
			}
			last[c] = mul[c];
			for (r=0; r <= !c; r++) {
				buf[c][1][width/2] = buf[c][2][width/2] = mul[c] << 7;
				for (tree=1, col=width/2; col > 0; ) {
					if ((tree = radc_token(tree, &raw))) {
						col -= 2;
						if (tree == 8) {
							for (y=1; y < 3; y++) {
								for (x=col+1; x >= col; x--) {
									buf[c][y][x] = (unsigned char) radc_token(18, &raw) * mul[c];
								}
							}
						} else {
							for (y=1; y < 3; y++) {
								for (x=col+1; x >= col; x--) {
									buf[c][y][x] = radc_token(tree+10, &raw) * 16 + PREDICTOR;
								}
							}
						}
					} else
						do {
							nreps = (col > 2) ? radc_token(9, &raw) + 1 : 1;
							for (rep=0; rep < 8 && rep < nreps && col > 0; rep++) {
								col -= 2;
								for (y=1; y < 3; y++) {
									for (x=col+1; x >= col; x--) {
										buf[c][y][x] = PREDICTOR;
									}
								}
								if (rep & 1) {
									step = radc_token(10, &raw) << 4;
									for (y=1; y < 3; y++) {
										for (x=col+1; x >= col; x--) {
											buf[c][y][x] += step;
										}
									}
								}
							}
						} while (nreps == 9);
				}
				for (y=0; y < 2; y++) {
					for (x=0; x < width/2; x++) {
						val = (buf[c][y+1][x] << 4) / mul[c];
						if (val < 0) val = 0;
						if (c) RAW(tmp, row+y*2+c-1,x*2+2-c) = val;
						else   RAW(tmp, row+r*2+y,x*2+y) = val;
					}
				}
				memcpy (buf[c][0]+!c, buf[c][2], sizeof buf[c][0]-2*!c);
			}
		}
		for (y=row; y < row+4; y++) {
			for (x=0; x < width; x++) {
				if ((x+y) & 1) {
					r = x ? x-1 : x+1;
					s = x+1 < width ? x+1 : x-1;
					val = (RAW(tmp, y, x)-2048) + (RAW(tmp,y,r)+RAW(tmp,y,s))/2;
					if (val < 0) val = 0;
					RAW(tmp,y,x) = val;
				}
			}
		}
	}

	for (i=0; i < height*width; i++) {
		tmp_c[i] = LIM(curve[tmp[i]] >> 4, 0, 255);
	}

	*out = calloc(1, len);
	if (*out == NULL) {
		free(header);
		free(tmp);
		free(tmp_c);
		return GP_ERROR_NO_MEMORY;
	}

	strcpy((char *)*out, header);
	ptr = *out + strlen(header);
	free(header);

	gp_bayer_decode(tmp_c, width, height, ptr, BAYER_TILE_GBRG);
	free(tmp);
	free(tmp_c);

	return GP_OK;
}

#undef PREDICTOR
