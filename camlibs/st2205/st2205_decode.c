/* Sitronix st2205 picframe decompression and compression code
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
#include "config.h"

#include <string.h>
#include <stdlib.h>
#ifdef HAVE_LIBGD
# include <gd.h>
#endif

#include "st2205.h"

#ifdef HAVE_LIBGD
#define CLAMP256(x) (((x) > 255) ? 255 : (((x) < 0) ? 0 : (x)))
#define CLAMP64S(x) (((x) > 63) ? 63 : (((x) < -64) ? -64 : (x)))

static const int16_t st2205_corr_table[16] = {
	-26,-22,-18,-14,-11,-7,-4,-1,1,4,7,11,14,18,22,26
};

static int
st2205_decode_block(CameraPrivateLibrary *pl, unsigned char *src,
	int src_length, int **dest, int dest_x, int dest_y)
{
	const st2205_lookup_row *luma_table, *chroma_table;
	int y_base, uv_base[2], uv_corr[2];
	int16_t Y[64], UV[2][16];
	int x, y, r, g, b, uv;
	if (src_length < 4) {
		gp_log (GP_LOG_ERROR, "st2205", "short image block");
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (src[0] & 0x80) {
		gp_log (GP_LOG_ERROR, "st2205",
			"2 luma bits per pixel pictures are not supported");
		return GP_ERROR_CORRUPTED_DATA;
	}
	y_base = src[1] & 0x7f;
	luma_table = st2205_lookup[src[1] >> 7];
	chroma_table = st2205_lookup[2];

	uv_base[0] = src[2] & 0x7f;
	uv_corr[0] = src[2] & 0x80;
	uv_base[1] = src[3] & 0x7f;
	uv_corr[1] = src[3] & 0x80;

	if (src_length != (4 + (uv_corr[0]? 10:2) + (uv_corr[1]? 10:2) + 40)) {
		GP_DEBUG ("src_length: %d, u_corr: %x v_corr: %x\n",
			  src_length, uv_corr[0], uv_corr[1]);
		gp_log (GP_LOG_ERROR, "st2205", "invalid block length");
		return GP_ERROR_CORRUPTED_DATA;
	}

	src += 4;

	for (uv = 0; uv < 2; uv++) {
		for (y = 0; y < 4; y++)
			for (x = 0; x < 4; x++)
				UV[uv][y * 4 + x] = uv_base[uv] - 64 +
				    chroma_table[src[y / 2]][(y & 1) * 4 + x];
		src += 2;

		if (uv_corr[uv]) {
			for (x = 0; x < 16; x+= 2) {
				uint8_t corr = src[x / 2];
				UV[uv][x    ] += st2205_corr_table[corr >> 4];
				UV[uv][x + 1] += st2205_corr_table[corr & 0x0f];
			}
			src += 8;
		}
	}

	for (y = 0; y < 8; y++) {
		memcpy(Y + y * 8, luma_table[src[y]], 8 * sizeof(uint16_t));
		for (x = 0; x < 8; x+= 2) {
			uint8_t corr = src[8 + y * 4 + x / 2];
			Y[y * 8 + x    ] +=
				y_base + st2205_corr_table[corr >> 4];
			Y[y * 8 + x + 1] +=
				y_base + st2205_corr_table[corr & 0x0f];
		}
	}

	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x++) {
			r = Y[y * 8 + x] + UV[1][(y / 2) * 4 + x / 2];
			g = Y[y * 8 + x] - UV[0][(y / 2) * 4 + x / 2] -
			    UV[1][(y / 2) * 4 + x / 2];
			b = Y[y * 8 + x] + UV[0][(y / 2) * 4 + x / 2];
			dest[dest_y + y][dest_x + x] =
				gdTrueColor (CLAMP256(2 * r),
					     CLAMP256(2 * g),
					     CLAMP256(2 * b));
		}
	}

	return 0;
}

int
st2205_decode_image(CameraPrivateLibrary *pl, unsigned char *src, int **dest)
{
	int ret, block = 0, block_length;
	struct st2205_coord *shuffle_table;
	struct st2205_image_header *header = (struct st2205_image_header*)src;
	uint8_t shuffle_pattern = header->shuffle_table;
	int src_length = be16toh (header->length);

	/* Skip the header */
	src += sizeof(*header);

	if (shuffle_pattern >= pl->no_shuffles) {
		gp_log (GP_LOG_ERROR, "st2205", "invalid shuffle pattern");
		return GP_ERROR_CORRUPTED_DATA;
	}
	shuffle_table = pl->shuffle[shuffle_pattern];

	while (src_length && block < (pl->width * pl->height / 64)) {
		block_length = (src[0] & 0x7f) + 1;
		if (block_length > src_length) {
			gp_log (GP_LOG_ERROR, "st2205", "block %d goes outside of image buffer", block);
			return GP_ERROR_CORRUPTED_DATA;
		}

		ret = st2205_decode_block(pl, src, block_length, dest,
					  shuffle_table[block].x,
					  shuffle_table[block].y);
		if (ret < 0)
			return ret;
		src += block_length;
		src_length -= block_length;
		block++;
	}

	if (src_length) {
		gp_log (GP_LOG_ERROR, "st2205", "data remaining after decoding %d blocks", block);
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (block != (pl->width * pl->height / 64)) {
		gp_log (GP_LOG_ERROR, "st2205",
			"image only contained %d of %d blocks",
			block, pl->width * pl->height / 64);
		return GP_ERROR_CORRUPTED_DATA;
	}

	return 0;
}

static uint8_t st2205_find_closest_match(const st2205_lookup_row *table,
	  int16_t *row, int *smallest_diff_ret)
{
	int i, j;
	uint8_t closest_match = 0;
	unsigned int diff, smallest_diff = -1;

	for (i = 0; i < 256; i++) {
		diff = 0;
		for (j = 0; j < 8; j++)
			diff += (row[j] - table[i][j]) * (row[j] - table[i][j]);
		if (diff < smallest_diff) {
			smallest_diff = diff;
			closest_match = i;
		}
	}

	if (smallest_diff_ret)
		*smallest_diff_ret = smallest_diff;

	return closest_match;
}

static uint8_t st2205_closest_correction(int16_t corr)
{
	int i, diff, smallest_diff;
	uint8_t closest = 0;

	smallest_diff = abs(st2205_corr_table[0] - corr);
	for (i = 1; i < 16; i++) {
		diff = abs(st2205_corr_table[i] - corr);
		if (diff < smallest_diff) {
			smallest_diff = diff;
			closest = i;
		}
	}

	return closest;
}

static int
st2205_code_block(CameraPrivateLibrary *pl, int **src,
	int src_x, int src_y, unsigned char *dest, int allow_uv_corr)
{
	const st2205_lookup_row *luma_table;
	int y_base, uv_base[2];
	int16_t Y[64], UV[2][16];
	uint8_t corr1, corr2, *pattern;
	int x, y, r, g, b, uv, diff1, diff2, used = 0;

	/* Step 1 convert to "YUV" */
	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x++) {
			int p = src[src_y + y][src_x + x];
			Y[y * 8 + x] = (gdTrueColorGetRed(p) +
					gdTrueColorGetGreen(p) +
					gdTrueColorGetBlue(p)) / 6;
		}
	}

	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++) {
			int p1 = src[src_y + y    ][src_x + x    ];
			int p2 = src[src_y + y    ][src_x + x + 1];
			int p3 = src[src_y + y + 1][src_x + x    ];
			int p4 = src[src_y + y + 1][src_x + x + 1];

			r = gdTrueColorGetRed(p1) + gdTrueColorGetRed(p2) +
			    gdTrueColorGetRed(p3) + gdTrueColorGetRed(p4);
			g = gdTrueColorGetGreen(p1) + gdTrueColorGetGreen(p2) +
			    gdTrueColorGetGreen(p3) + gdTrueColorGetGreen(p4);
			b = gdTrueColorGetBlue(p1) + gdTrueColorGetBlue(p2) +
			    gdTrueColorGetBlue(p3) + gdTrueColorGetBlue(p4);
			uv = (b * 3 - (r + g + b)) / 24;
			UV[0][y * 4 + x] = CLAMP64S(uv);
			uv = (r * 3 - (r + g + b)) / 24;
			UV[1][y * 4 + x] = CLAMP64S(uv);
		}
	}

	/* Step 2a calculate base values */
	y_base = 0;
	for (x = 0; x < 64; x++)
		y_base += Y[x];
	y_base = y_base / 64;

	for (uv = 0; uv < 2; uv++) {
		uv_base[uv] = 0;
		for (x = 0; x < 16; x++)
			uv_base[uv] += UV[uv][x];
		uv_base[uv] = uv_base[uv] / 16;
	}

	dest[1] = y_base;
	dest[2] = uv_base[0] + 64;
	dest[3] = uv_base[1] + 64;
	used = 4;

	/* Step 2b adjust values for base values and normalize */
	for (x = 0; x < 64; x++)
		Y[x] -= y_base;

	for (uv = 0; uv < 2; uv++)
		for (x = 0; x < 16; x++)
			UV[uv][x] -= uv_base[uv];

	/* Step 3 encode chroma values */
	for (uv = 0; uv < 2; uv++) {
		pattern = dest + used;
		dest[used++] = st2205_find_closest_match (st2205_lookup[2],
							  &UV[uv][0], &diff1);
		dest[used++] = st2205_find_closest_match (st2205_lookup[2],
							  &UV[uv][8], &diff2);
		if ((diff1 > 64 || diff2 > 64) && allow_uv_corr) {
			dest[2 + uv] |= 0x80;
			for (x = 0; x < 16; x+= 2) {
				corr1 = st2205_closest_correction(UV[uv][x] -
				  st2205_lookup[2][pattern[x / 8]][x % 8]);
				corr2 = st2205_closest_correction(UV[uv][x + 1] -
				- st2205_lookup[2][pattern[x / 8]][x % 8 + 1]);
				dest[used++] = (corr1 << 4) | corr2;
			}
		}
	}

	/* Step 4a encode luma values, choose luma table */
	diff1 = 0;
	diff2 = 0;
	for (y = 0; y < 8; y++) {
		st2205_find_closest_match(st2205_lookup[0], &Y[y * 8], &x);
		diff1 += x;
		st2205_find_closest_match(st2205_lookup[1], &Y[y * 8], &x);
		diff2 += x;
	}

	if (diff1 <= diff2) {
		luma_table = st2205_lookup[0];
		dest[1] |= 0x00;
	} else {
		luma_table = st2205_lookup[1];
		dest[1] |= 0x80;
	}

	/* Step 4b encode luma values, choose luma patterns */
	pattern = dest + used;
	for (y = 0; y < 8; y++)
		dest[used++] = st2205_find_closest_match (luma_table,
							  &Y[y * 8], NULL);

	/* Step 4c encode luma values, add luma correction values */
	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x += 2) {
			corr1 = st2205_closest_correction (Y[y * 8 + x] -
						luma_table[pattern[y]][x]);
			corr2 = st2205_closest_correction(Y[y * 8 + x + 1] -
						luma_table[pattern[y]][x + 1]);
			dest[used++] = (corr1 << 4) | corr2;
		}
	}

	dest[0] = used - 1;

	return used;
}

int
st2205_code_image(CameraPrivateLibrary *pl, int **src,
	unsigned char *dest, uint8_t shuffle_pattern, int allow_uv_corr)
{
	int block = 0, used = 0, ret;
	struct st2205_coord *shuffle_table;
	struct st2205_image_header *header = (struct st2205_image_header*)dest;

	/* Make room for the header */
	dest += sizeof(*header);

	if (shuffle_pattern >= pl->no_shuffles) {
		gp_log (GP_LOG_ERROR, "st2205", "invalid shuffle pattern");
		return GP_ERROR_BAD_PARAMETERS;
	}

	shuffle_table = pl->shuffle[shuffle_pattern];

	while (block < (pl->width * pl->height / 64)) {
		ret = st2205_code_block (pl, src, shuffle_table[block].x,
					 shuffle_table[block].y,
					 dest + used, allow_uv_corr);
		if (ret < 0)
			return ret;
		used += ret;
		block++;
	}

	/* Write the header now that we know the size */
	memset(header, 0, sizeof(*header));
	header->marker = ST2205_HEADER_MARKER;
	header->width  = htobe16(pl->width);
	header->height = htobe16(pl->height);
	header->blocks = htobe16((pl->width * pl->height) / 64);
	header->shuffle_table = shuffle_pattern;
	header->unknown2 = 0x04;
	header->unknown3 = pl->unknown3[shuffle_pattern];
	header->length = htobe16(used);

	return used + sizeof(*header);
}

int
st2205_rgb565_to_rgb24(CameraPrivateLibrary *pl, unsigned char *src,
	int **dest)
{
	int x,y;

	for (y = 0; y < pl->height; y++) {
		for (x = 0; x < pl->width; x++) {
			unsigned short w = src[0] << 8 | src[1];

			dest[y][x] = gdTrueColor ((w >> 8) & 0xf8,
						  (w >> 3) & 0xfb,
						  (w << 3) & 0xf8);
			src += 2;
		}
	}

	return GP_OK;
}

int
st2205_rgb24_to_rgb565(CameraPrivateLibrary *pl, int **src,
	unsigned char *dest)
{
	int x,y;

	for (y = 0; y < pl->height; y++) {
		for (x = 0; x < pl->width; x++) {
			unsigned short w;

			int r = gdTrueColorGetRed(src[y][x]);
			int g = gdTrueColorGetGreen(src[y][x]);
			int b = gdTrueColorGetBlue(src[y][x]);

			w = ((r <<  8) & 0xf100) |
			    ((g <<  3) & 0x07e0) |
			    ((b >>  3) & 0x001f);

			*dest++ = w >> 8;
			*dest++ = w & 0xff;
		}
	}

	return pl->height * pl->width * 2;
}

#else
int
st2205_decode_image(CameraPrivateLibrary *pl, unsigned char *src, int **dest)
{
	return GP_ERROR_NOT_SUPPORTED;
}

int
st2205_code_image(CameraPrivateLibrary *pl, int **src,
	unsigned char *dest, uint8_t shuffle_pattern, int allow_uv_corr)
{
	return GP_ERROR_NOT_SUPPORTED;
}

int
st2205_rgb565_to_rgb24(CameraPrivateLibrary *pl, unsigned char *src,
	int **dest)
{
	return GP_ERROR_NOT_SUPPORTED;
}

int
st2205_rgb24_to_rgb565(CameraPrivateLibrary *pl, int **src,
	unsigned char *dest)
{
	return GP_ERROR_NOT_SUPPORTED;
}

#endif
