/* Appotech ax203 picframe decompression and compression code
 * For ax203 picture frames with firmware version 3.4.x
 *
 *   Copyright (c) 2010 Hans de Goede <hdegoede@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the 
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifdef STANDALONE_MAIN

#include <stdio.h>
#include <_stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <gd.h>

#else

#include "config.h"

#ifdef HAVE_LIBGD
#include <gd.h>
#endif
#include <stdlib.h>

#include "ax203.h"

#endif

#define CLAMP_U8(x) (((x) > 255) ? 255 : (((x) < 0) ? 0 : (x)))

static const int corr_tables[4][8] = {
	/* Table 0 depends on wrap around to get negative
	   corrections!! */
	{ 0, 32, 64, 96, 128, 160, 192, 224 },
	{ 0, 16, 32, 48, -64, -48, -32, -16 },
	{ 0,  8, 16, 24, -32, -24, -16, -8 },
	{ 0,  4,  8, 12, -16, -12,  -8, -4 },
};

#ifdef HAVE_LIBGD
/* With in a compressed 4x4 block, the data is stored 4 component values at a
   time, compressed into 2 bytes, the first 5 bits are a starting value,
   then 2 bits encoding which correction/delta table to use and
   then 3x3 bits code deltas from the previous pixel. */
static void
ax203_decode_component_values(char *src, char *dest)
{
	int i, table, corr;
	dest[0] = src[0] & ~0x07;
       	table = (src[0] >> 1) & 3;
       	
	for (i = 1; i < 4; i++) {
		switch (i) {
		case 1:
			corr =  (src[1] >> 5) & 7;
			break;
		case 2:
			corr =  (src[1] >> 2) & 7;
			break;
		case 3:
			corr = ((src[1] << 1) & 6) | (src[0] & 1);
			break;
		}
		dest[i] = dest[i - 1] + corr_tables[table][corr];
	}
}

/* The compressed data consists of blocks of 4x4 pixels, each encoded
   in 12 bytes */
static void
ax203_decode_block_yuv_delta(char *src, int **dest, int dest_x, int dest_y)
{
	int x, y, r, g, b;
	uint8_t Y[16];
	int8_t U[4], V[4];

	/* The compressed component order is: 4 Pixels U, 4 Pixels V,
	   16 pixels Y */
	ax203_decode_component_values(src, (char *)U);
	src += 2;
	ax203_decode_component_values(src, (char *)V);
	src += 2;

	/* The Y components are stored 4 at a time in a pattern like this:
	   1 2  1 2
	   3 4  3 4
	   
	   1 2  1 2
	   3 4  3 4 */
	for (y = 0; y < 4; y += 2) {
		for (x = 0; x < 4; x += 2) {
			char buf[4];
			ax203_decode_component_values(src, buf);
			Y[ y      * 4 + x    ] = buf[0];
			Y[ y      * 4 + x + 1] = buf[1];
			Y[(y + 1) * 4 + x    ] = buf[2];
			Y[(y + 1) * 4 + x + 1] = buf[3];
			src += 2;
		}
	}

	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++) {
			r = 1.164 * (Y[y * 4 + x] - 16) +
			    1.596 *  V[(y / 2) * 2 + x / 2];
			g = 1.164 * (Y[y * 4 + x] - 16) -
			    0.391 *  U[(y / 2) * 2 + x / 2] -
			    0.813 *  V[(y / 2) * 2 + x / 2];
			b = 1.164 * (Y[y * 4 + x] - 16) +
			    2.018 *  U[(y / 2) * 2 + x / 2];
			dest[dest_y + y][dest_x + x] =
				gdTrueColor (CLAMP_U8(r),
					     CLAMP_U8(g),
					     CLAMP_U8(b));
		}
	}
}

void ax203_decode_yuv_delta(char *src, int **dest, int width, int height)
{
	int x, y;

	for (y = 0; y < height; y += 4) {
		for (x = 0; x < width; x += 4) {
			ax203_decode_block_yuv_delta(src, dest, x, y);
			src += 12;
		}
	}
}

static int
ax203_find_closest_correction_signed(int8_t base, int8_t val, int table)
{
	int closest_delta = 256;
	int i, delta, closest_idx = 0;
	int8_t corrected_val;

	for (i = 0; i < 8; i++) {
		/* Don't allow wrap around for tables other then table 0 */
		if (table &&
		    ((base + corr_tables[table][i]) > 127 ||
		     (base + corr_tables[table][i]) < -128))
			continue;

		/* We are used for U and V values skip correction vals which
		   would lead to invalid U / V values */
		corrected_val = base + corr_tables[table][i];
		if (corrected_val < -112 || corrected_val > 111)
			continue;

		delta = abs(corrected_val - val);
		if (delta < closest_delta) {
			closest_delta = delta;
			closest_idx = i;
		}
	}
	return closest_idx;
}

static int
ax203_find_closest_correction_unsigned(uint8_t base, uint8_t val, int table)
{
	int closest_delta = 256;
	int i, delta, closest_idx = 0;
	uint8_t corrected_val;

	for (i = 0; i < 8; i++) {
		/* Don't allow wrap around for tables other then table 0 */
		if (table &&
		    ((base + corr_tables[table][i]) > 255 ||
		     (base + corr_tables[table][i]) < 0))
			continue;

		/* We are used for Y values skip correction vals which
		   would lead to invalid Y values */
		corrected_val = base + corr_tables[table][i];
		if (corrected_val < 16 || corrected_val > 235)
			continue;

		delta = abs(corrected_val - val);
		if (delta < closest_delta) {
			closest_delta = delta;
			closest_idx = i;
		}
	}
	return closest_idx;
}

static void
ax203_encode_signed_component_values(int8_t *src, char *dest)
{
	int i, j, table, corr;
	int8_t base;

	/* Select a correction table */
	for (i = 3; i > 0; i--) {
		base = src[0] & ~0x07;
		for (j = 1; j < 4; j++) {
			if ((base + corr_tables[i][3] + 4) < src[j] ||
			    (base + corr_tables[i][4] - 4) > src[j])
				break;
			corr = ax203_find_closest_correction_signed
							(base, src[j], i);
			/* Calculate the base value for the next pixel */
			base = base + corr_tables[i][corr];
		}
		/* If we did not break out the above loop the min / max
		   correction in the current table is enough */
		if (j == 4) 
			break;
	}
	table = i;

	/* And store the pixels */
	base = src[0] & ~0x07;
	dest[0] = base;
	dest[0] |= table << 1;
	dest[1] = 0;
	for (i = 1; i < 4; i++) {
		corr = ax203_find_closest_correction_signed (base, src[i],
							     table);
		switch (i) {
		case 1:
			dest[1] |= corr << 5;
			break;
		case 2:
			dest[1] |= corr << 2;
			break;
		case 3:
			dest[0] |= corr & 1;
			dest[1] |= corr >> 1;
			break;
		}
		/* Calculate the base value for the next pixel */
		base = base + corr_tables[table][corr];
	}
}

static void
ax203_encode_unsigned_component_values(uint8_t *src, char *dest)
{
	int i, j, table, corr;
	uint8_t base;

	/* Select a correction table */
	for (i = 3; i > 0; i--) {
		base = src[0] & ~0x07;
		for (j = 1; j < 4; j++) {
			if ((base + corr_tables[i][3] + 4) < src[j] ||
			    (base + corr_tables[i][4] - 4) > src[j])
				break;
			corr = ax203_find_closest_correction_unsigned
							(base, src[j], i);
			/* Calculate the base value for the next pixel */
			base = base + corr_tables[i][corr];
		}
		/* If we did not break out the above loop the min / max
		   correction in the current table is enough */
		if (j == 4) 
			break;
	}
	table = i;

	/* And store the pixels */
	base = src[0] & ~0x07;
	dest[0] = base;
	dest[0] |= table << 1;
	dest[1] = 0;
	for (i = 1; i < 4; i++) {
		corr = ax203_find_closest_correction_unsigned (base, src[i],
							       table);
		switch (i) {
		case 1:
			dest[1] |= corr << 5;
			break;
		case 2:
			dest[1] |= corr << 2;
			break;
		case 3:
			dest[0] |= corr & 1;
			dest[1] |= corr >> 1;
			break;
		}
		/* Calculate the base value for the next pixel */
		base = base + corr_tables[table][corr];
	}
}

static void
ax203_encode_block_yuv_delta(int **src, int src_x, int src_y, char *dest)
{
	uint8_t Y[16];
	int8_t U[4], V[4];
	int x, y;

	/* Convert to YUV */
	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++) {
			int p = src[src_y + y][src_x + x];
			Y[y * 4 + x] = gdTrueColorGetRed(p)   * 0.257 +
				       gdTrueColorGetGreen(p) * 0.504 +
				       gdTrueColorGetBlue(p)  * 0.098 + 16;
		}
	}
	for (y = 0; y < 4; y += 2) {
		for (x = 0; x < 4; x += 2) {
			int p1 = src[src_y + y    ][src_x + x    ];
			int p2 = src[src_y + y    ][src_x + x + 1];
			int p3 = src[src_y + y + 1][src_x + x    ];
			int p4 = src[src_y + y + 1][src_x + x + 1];

			int r = (gdTrueColorGetRed(p1) +
				 gdTrueColorGetRed(p2) +
				 gdTrueColorGetRed(p3) +
				 gdTrueColorGetRed(p4)) / 4;
			int g = (gdTrueColorGetGreen(p1) +
				 gdTrueColorGetGreen(p2) +
				 gdTrueColorGetGreen(p3) +
				 gdTrueColorGetGreen(p4)) / 4;
			int b = (gdTrueColorGetBlue(p1) +
				 gdTrueColorGetBlue(p2) +
				 gdTrueColorGetBlue(p3) +
				 gdTrueColorGetBlue(p4)) / 4;

			U[y + x / 2] = 0.439 * b - 0.291 * g - 0.148 * r;
			V[y + x / 2] = 0.439 * r - 0.368 * g - 0.071 * b;
		}
	}

	/* The compressed component order is: 4 Pixels U, 4 Pixels V,
	   16 pixels Y */
	ax203_encode_signed_component_values(U, dest);
	dest += 2;
	ax203_encode_signed_component_values(V, dest);
	dest += 2;

	/* The Y components are stored 4 at a time in a pattern like this:
	   1 2  1 2
	   3 4  3 4
	   
	   1 2  1 2
	   3 4  3 4 */
	for (y = 0; y < 4; y += 2) {
		for (x = 0; x < 4; x += 2) {
			uint8_t buf[4];
			buf[0] = Y[ y      * 4 + x    ];
			buf[1] = Y[ y      * 4 + x + 1];
			buf[2] = Y[(y + 1) * 4 + x    ];
			buf[3] = Y[(y + 1) * 4 + x + 1];
			ax203_encode_unsigned_component_values(buf, dest);
			dest += 2;
		}
	}
}

void
ax203_encode_yuv_delta(int **src, char *dest, int width, int height)
{
	int x, y;

	for (y = 0; y < height; y += 4) {
		for (x = 0; x < width; x += 4) {
			ax203_encode_block_yuv_delta(src, x, y, dest);
			dest += 12;
		}
	}
}

#endif

#ifdef STANDALONE_MAIN

int
main(int argc, char *argv[])
{
	FILE *fin = NULL;
	FILE *fout = NULL;
	gdImagePtr im = NULL;
	int ret = 0;
	/* FIXME get these from the cmdline */
	int width = 128, height = 128;

	if (argc != 4) {
		fprintf (stderr,
			 "Usage: %s <-d|-c> <inputfile> <outputfile>\n",
			 argv[0]);
		ret = 1;
		goto exit;
	}

	fin = fopen(argv[2], "r");
	if (!fin) {
		fprintf (stderr, "Error opening: %s: %s\n", argv[2],
			 strerror(errno));
		ret = 1;
		goto exit;
	}

	fout = fopen(argv[3], "w");
	if (!fout) {
		fprintf (stderr, "Error opening: %s: %s\n", argv[3],
			 strerror(errno));
		ret = 1;
		goto exit;
	}

	if (!strcmp(argv[1], "-d")) {
		const int bufsize = width * height * 3 / 4;
		char buf[bufsize];
		if (fread(buf, 1, bufsize, fin) != bufsize) {
			fprintf (stderr, "Error reading: %s: %s\n", argv[2],
				 strerror(errno));
			ret = 1;
			goto exit;
		}
		im = gdImageCreateTrueColor(width, height);
		if (!im) {
			fprintf (stderr, "Error allocating memory\n");
			ret = 1;
			goto exit;
		}
		ax203_decode(buf, im->tpixels, width, height);
		gdImagePng (im, fout);
	} else {
		fprintf (stderr, "%s: unkown option: %s\n", argv[0], argv[1]);
		ret = 1;
		goto exit;
	}

exit:
	if (fin)
		fclose (fin);
	if (fout)
		fclose (fout);
	if (im)
		gdImageDestroy (im);
	return ret;
}

#endif
