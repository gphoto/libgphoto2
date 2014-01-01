/* Appotech ax203 picframe decompression and compression code
 * For ax203 picture frames with firmware version 3.3.x
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

#include "ax203.h"

#endif

#define CLAMP_U8(x) (((x) > 255) ? 255 : (((x) < 0) ? 0 : (x)))

#ifdef HAVE_LIBGD
static void
ax203_decode_block_yuv(char *src, int **dest, int dest_x, int dest_y)
{
	int x, y, r, g, b;
	uint8_t Y[4];
	int8_t U, V;

	/* The compressed data consists of blocks of 2x2 pixels, each encoded
	   in 4 bytes The highest 5 bits of each byte encode an Y value, the
	   lowest 3 bits of the first 2 bytes are combined to form U, and of
	   the last 2 bytes to form V */
	for (x = 0; x < 4; x++)
		Y[x] = src[x] & 0xF8;

	U = ((src[0] & 7) << 5) | ((src[1] & 7) << 2);
	V = ((src[2] & 7) << 5) | ((src[3] & 7) << 2);

	/* The Y components are for a 2x2 pixels block like this:
	   1 2
	   3 4  */
	for (y = 0; y < 2; y ++) {
		for (x = 0; x < 2; x ++) {
			r = 1.164 * (Y[y * 2 + x] - 16) + 1.596 * V;
			g = 1.164 * (Y[y * 2 + x] - 16) - 0.391 * U - 0.813 * V;
			b = 1.164 * (Y[y * 2 + x] - 16) + 2.018 * U;
			dest[dest_y + y][dest_x + x] =
				gdTrueColor (CLAMP_U8 (r),
					     CLAMP_U8 (g),
					     CLAMP_U8 (b));
		}
	}
}

void
ax203_decode_yuv(char *src, int **dest, int width, int height)
{
	int x, y;

	for (y = 0; y < height; y += 2) {
		for (x = 0; x < width; x += 2) {
			ax203_decode_block_yuv(src, dest, x, y);
			src += 4;
		}
	}
}

static void
ax203_encode_block_yuv(int **src, int src_x, int src_y, char *dest)
{
	uint8_t Y[4];
	int8_t U, V;
	int x, y;

	/* Step 1 convert to YUV */
	for (y = 0; y < 2; y ++) {
		for (x = 0; x < 2; x ++) {
			int p = src[src_y + y][src_x + x];
			Y[y * 2 + x] = gdTrueColorGetRed(p)   * 0.257 +
				       gdTrueColorGetGreen(p) * 0.504 +
				       gdTrueColorGetBlue(p)  * 0.098 + 16;
		}
	}

	{
		int p1 = src[src_y    ][src_x    ];
		int p2 = src[src_y    ][src_x + 1];
		int p3 = src[src_y + 1][src_x    ];
		int p4 = src[src_y + 1][src_x + 1];

		int r = (gdTrueColorGetRed(p1) + gdTrueColorGetRed(p2) +
			 gdTrueColorGetRed(p3) + gdTrueColorGetRed(p4)) / 4;
		int g = (gdTrueColorGetGreen(p1) + gdTrueColorGetGreen(p2) +
			 gdTrueColorGetGreen(p3) + gdTrueColorGetGreen(p4)) / 4;
		int b = (gdTrueColorGetBlue(p1) + gdTrueColorGetBlue(p2) +
			 gdTrueColorGetBlue(p3) + gdTrueColorGetBlue(p4)) / 4;

		U = 0.439 * b - 0.291 * g - 0.148 * r;
		V = 0.439 * r - 0.368 * g - 0.071 * b;
	}

	for (x = 0; x < 4; x++)
		dest[x] = Y[x] & 0xf8;

	dest[0] |= (U & 0xe0) >> 5;
	dest[1] |= (U & 0x1c) >> 2;
	dest[2] |= (V & 0xe0) >> 5;
	dest[3] |= (V & 0x1c) >> 2;
}

void
ax203_encode_yuv(int **src, char *dest, int width, int height)
{
	int x, y;

	for (y = 0; y < height; y += 2) {
		for (x = 0; x < width; x += 2) {
			ax203_encode_block_yuv(src, x, y, dest);
			dest += 4;
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
	gdImagePtr im_in = NULL, im = NULL;
	char *buf = NULL;
	int bufsize, ret = 0;
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

	bufsize = width * height;
	buf = malloc(bufsize);
	if (!buf) {
		fprintf (stderr, "Error allocating memory\n");
		ret = 1;
		goto exit;
	}

	im = gdImageCreateTrueColor(width, height);
	if (!im) {
		fprintf (stderr, "Error allocating memory\n");
		ret = 1;
		goto exit;
	}

	if (!strcmp(argv[1], "-d")) {
		if (fread(buf, 1, bufsize, fin) != bufsize) {
			fprintf (stderr, "Error reading: %s: %s\n", argv[2],
				 strerror(errno));
			ret = 1;
			goto exit;
		}
		ax203_decode2(buf, im->tpixels, width, height);
		gdImagePng (im, fout);
	} else if (!strcmp(argv[1], "-c")) {
		im_in = gdImageCreateFromPng(fin);
		if (im_in == NULL) {
			rewind(fin);
			im_in = gdImageCreateFromGif(fin);
		}
		if (im_in == NULL) {
			rewind(fin);
			im_in = gdImageCreateFromWBMP(fin);
		}
		/* gdImageCreateFromJpegPtr is chatty on error,
		   so call it last */
		if (im_in == NULL) {
			rewind(fin);
			im_in = gdImageCreateFromJpeg(fin);
		}
		if (im_in == NULL) {
			fprintf (stderr,
			       "Error unrecognized file format for file: %s\n",
			       argv[2]);
			ret = 1;
			goto exit;
		}

		gdImageCopyResampled (im, im_in, 0, 0, 0, 0,
				      im->sx, im->sy,
				      im_in->sx, im_in->sy);
		gdImageSharpen(im, 100);
		ax203_encode2(im->tpixels, buf, width, height);

		if (fwrite (buf, 1, bufsize, fout) != bufsize) {
			fprintf (stderr, "Error writing: %s: %s\n", argv[3],
				 strerror(errno));
			ret = 1;
			goto exit;
		}
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
	if (buf)
		free (buf);
	if (im)
		gdImageDestroy (im);
	if (im_in)
		gdImageDestroy (im_in);
	return ret;
}

#endif
