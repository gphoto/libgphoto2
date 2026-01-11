/*
 * Jenopt JD11 Camera Driver
 * Copyright 2025 Nicholas Sherlock <n.sherlock@gmail.com>
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

#include "dng.h"

#include <libgphoto2/gamma.h>

#include <math.h>
#include <tiffio.h>
#include <stdlib.h>
#include <string.h>

/* Growable memory sink for TIFFClientOpen */
typedef struct {
	uint8_t *buf;
	toff_t   size; // Current data size
	toff_t   cap;  // Allocated capacity
	toff_t   pos;  // Current write position
} TiffMem;

static int tiffmem_grow(TiffMem *m, toff_t need)
{
	if (need <= m->cap) return 1;
	toff_t newcap = m->cap ? m->cap : 64 * 1024;
	while (newcap < need) {
		newcap *= 2;
		if (newcap < m->cap) return 0; // Overflow
	}
	uint8_t *nb = realloc(m->buf, newcap);
	if (!nb) return 0;
	m->buf = nb;
	m->cap = newcap;
	return 1;
}

static tsize_t tiffmem_write(thandle_t h, tdata_t data, tsize_t len)
{
	TiffMem *m = (TiffMem*)h;
	toff_t end = m->pos + (toff_t)len;
	if (!tiffmem_grow(m, end)) return 0;
	memcpy(m->buf + m->pos, data, (size_t)len);
	m->pos = end;
	if (m->pos > m->size) m->size = m->pos;
	return len;
}

static tsize_t tiffmem_read(thandle_t h, tdata_t data, tsize_t len)
{
	(void)h; (void)data; (void)len;
	return 0;
}

static toff_t tiffmem_seek(thandle_t h, toff_t off, int whence)
{
	TiffMem *m = (TiffMem*)h;
	toff_t npos = 0;

	switch (whence) {
	case SEEK_SET: npos = off; break;
	case SEEK_CUR: npos = m->pos + off; break;
	case SEEK_END: npos = m->size + off; break;
	default: return (toff_t)-1;
	}

	// Allow seeking past end; grow on write
	m->pos = npos;
	return m->pos;
}

static int tiffmem_close(thandle_t h)
{
	(void)h;
	return 0;
}

static toff_t tiffmem_size(thandle_t h)
{
	TiffMem *m = (TiffMem*)h;
	return m->size;
}

static int tiffmem_map(thandle_t h, tdata_t *data, toff_t *size)
{
	(void)h; (void)data; (void)size;
	return 0;
}

static void tiffmem_unmap(thandle_t h, tdata_t data, toff_t size)
{
	(void)h; (void)data; (void)size;
}

/**
 * Simple downscaling+debayering+gamma correction to create an RGB thumb.
 * 
 * @return 1 on success 
 */
static int demosaic_to_thumbnail(
	const uint8_t *bayer,
	int width, int height,
	const uint8_t * cfa_pattern,
	int thumb_shift,
	TIFF *tif
) {
	uint8_t gamma[256];

	gp_gamma_fill_table(gamma, 0.45);
	
	/* Compute the byte offsets to the color subpixels within Bayer
	 * clusters on the source image.
	 * For duplicate colors (e.g. two greens) we just take the
	 * last instance
	 */
	size_t red_offset = 0, green_offset = 0, blue_offset = 0;
	for (int y = 0; y < 2; y++) {
		for (int x = 0; x < 2; x++) {
			size_t subpixel_offset = y * width + x;
			
			switch (cfa_pattern[y * 2 + x]) {
				case CFA_R:
					red_offset = subpixel_offset;
					break;
				case CFA_G:
					green_offset = subpixel_offset;
					break;
				case CFA_B:
					blue_offset = subpixel_offset;
					break;
				default:
					;
			}
		}
	}
	
	uint8_t *thumb_line = malloc((width >> thumb_shift) * 3);
	if (!thumb_line) {
		return 0;
	}

	for (int row = 0; row < height >> thumb_shift; row++) {
		/* Source is always an even-numbered row (so we correctly pick up the
		 * top left corner of the CFA pattern)
		 */
		uint8_t *dest = thumb_line;
		for (int col = 0; col < width >> thumb_shift; col++) {
			size_t red_sum = 0, green_sum = 0, blue_sum = 0;
			/* The first level of shift just samples from a single 2x2 Bayer 
			 * cluster, with additional shift averaging multiple clusters
			 * together
			 */
			const uint8_t * cluster_start = 
					bayer
					+ (row << thumb_shift) * width
					+ (col << thumb_shift);
			
			for (int y = 0; y < 1 << (thumb_shift - 1); y++) {
				const uint8_t *source = cluster_start + (y * 2) * width;

				for (int x = 0; x < 1 << (thumb_shift - 1); x++, source += 2) {
					red_sum   += source[red_offset];
					green_sum += source[green_offset];
					blue_sum  += source[blue_offset];
				}
			}
			*dest++ = red_sum   >> (2 * (thumb_shift - 1));
			*dest++ = green_sum >> (2 * (thumb_shift - 1));
			*dest++ = blue_sum  >> (2 * (thumb_shift - 1));
		}

		gp_gamma_correct_single(gamma, thumb_line, width >> thumb_shift);
		
		if (TIFFWriteScanline (tif, (tdata_t) thumb_line, row, 0) < 0) {
			free(thumb_line);
			return 0;
		}
	}
	free(thumb_line);
	return 1;
}

/**
 * params:
 *    uint8_t *bayer      - 8-bit Bayer array
 *    int width, height   - image size
 *    int cfa_pattern[4]  - array of CFA_* constants to encode the colors
 *                          of the 4 subpixels within a Bayer unit
 *    uint8_t black_level - smallest and largest possible pixel values
 *    uint8_t white_level
 *    char *make, *model  - camera make and model
 *    char *unique_model  - (optional) a string that uniquely represents the
 *                          camera, for identifying it to apply image editing
 *                          profiles to. Should include maker's name and a
 *                          non-localized unique product name. 
 *    int thumb_shift     - image dimensions are shifted right by this much
 *                          to reduce the size for the thumbnail (i.e.
 *                          shift of 1 = 2x smaller), must be >=1
 *    uint8_t **out_buf   - resulting buffer is stored here iff successful,
 *                          and you must free this
 *    size_t   *out_size
 * 
 * Writes an 8-bit Bayer array as a DNG into a memory buffer.
 *
 * return value: 1 on success 
 */
int write_dng_cfa8_to_memory(
	const uint8_t *bayer,
	int width, int height,
	const uint8_t cfa_pattern[4],
	uint8_t black_level, uint8_t white_level,
	const char *make, const char *model, const char *unique_model,
	int thumb_shift,
	uint8_t **out_buf, size_t *out_size)
{
	static const float    BaselineExposure = 0.0f;
	static const uint16_t CFARepeatPatternDim[2] = { 2, 2 };
	/* Adobe's DNG SDK has a bug in it that we must avoid to have our DNGs
	 * open successfully in Photoshop.
	 * 
	 * If the DNG specifies black-levels as a 2x2 array, the SDK tries to
	 * precompute a pixel-brightness mapping table for a full 16-bit input
	 * range, even though our DNG is 8-bit. This triggers a fatal overflow
	 * in the SDK tool dng_validate: "Overflow in Round_int32"
	 *
	 * They have a different codepath for 1x1 black-levels which has explicit
	 * support for 8-bit RAW, so we take that path by setting our black-level
	 * to 1x1 instead of 2x2:
	 */
	static const uint16_t BlackLevelRepeatDim[2] = { 1, 1 };
	// Simple XYZ to sRGB matrix
	static const float    ColorMatrix[9] = {
		3.1338561f, -1.6168667f, -0.4906146f,
		-.9787684f,  1.9161415f,  0.0334540f,
		0.0719453f, -0.2289914f,  1.4052427f,
	};
	static const float    AsShotNeutral[] = { 1.f, 1.f, 1.f };

	TIFF *tif;
	TiffMem mem = {0};
	uint64_t sub_offset = 0;

	if (thumb_shift < 1)
		thumb_shift = 1;

	tif = TIFFClientOpen(
		"mem.dng", "w",
		(thandle_t)&mem,
		tiffmem_read,
		tiffmem_write,
		tiffmem_seek,
		tiffmem_close,
		tiffmem_size,
		tiffmem_map,
		tiffmem_unmap
	);
	if (!tif) {
		free(mem.buf);
		return 0;
	}

	TIFFSetField(tif, TIFFTAG_DNGVERSION,         "\001\004\0\0"); // 1.4
	TIFFSetField(tif, TIFFTAG_DNGBACKWARDVERSION, "\001\000\0\0"); // 1.0
	TIFFSetField(tif, TIFFTAG_SUBFILETYPE, 1);
	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,  width >> thumb_shift);
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height >> thumb_shift);
	// Put whole thumb into 1 strip:
	TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height >> thumb_shift); 
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
	TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
	TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(tif, TIFFTAG_SUBIFD, 1, &sub_offset);
	TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField(tif, TIFFTAG_BASELINEEXPOSURE, BaselineExposure);
	TIFFSetField(tif, TIFFTAG_COLORMATRIX1, 9, ColorMatrix);
	TIFFSetField(tif, TIFFTAG_ASSHOTNEUTRAL, 3, AsShotNeutral);
	TIFFSetField(tif, TIFFTAG_CALIBRATIONILLUMINANT1, 21); // D65
	TIFFSetField(tif, TIFFTAG_MAKE, make);
	TIFFSetField(tif, TIFFTAG_MODEL, model);
	if (unique_model)
		TIFFSetField(tif, TIFFTAG_UNIQUECAMERAMODEL, unique_model);

	TIFFCheckpointDirectory(tif);

	if (!demosaic_to_thumbnail(
		bayer,
		width, height,
		cfa_pattern,
		thumb_shift,
		tif))
	{
		TIFFClose(tif);
		free(mem.buf);
		return 0;
	}

	TIFFWriteDirectory(tif);

	// Now the main image
	TIFFSetField(tif, TIFFTAG_SUBFILETYPE, 0);
	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,  (uint32_t)width);
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (uint32_t)height);
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
	TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 64);
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_CFA);
	TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(tif, TIFFTAG_CFAREPEATPATTERNDIM, CFARepeatPatternDim);
	if (TIFFLIB_VERSION < 20201219) {
		TIFFSetField(tif, TIFFTAG_CFAPATTERN, cfa_pattern);
	} else {
		TIFFSetField(tif, TIFFTAG_CFAPATTERN, 4, cfa_pattern);
	}
	
	float black_level_f = black_level;
	uint32_t white_level_32 = white_level;
	TIFFSetField(tif, TIFFTAG_BLACKLEVELREPEATDIM, BlackLevelRepeatDim);
	TIFFSetField(tif, TIFFTAG_BLACKLEVEL, 1, &black_level_f);
	TIFFSetField(tif, TIFFTAG_WHITELEVEL, 1, &white_level_32);

	for (int row = 0; row < height; row++) {
		const uint8_t *line = bayer + (size_t)row * (size_t)width;
		if (TIFFWriteScanline(tif, (tdata_t) line, row, 0) < 0) {
			TIFFClose(tif);
			free(mem.buf);
			return 0;
		}
	}
	
	TIFFClose(tif);

	*out_buf  = mem.buf; // Caller takes ownership
	*out_size = mem.size;
	return 1;
}
