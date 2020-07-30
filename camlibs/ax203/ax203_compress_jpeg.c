/* Appotech ax203 picframe JPEG-ish compression code
 * For ax203 picture frames with firmware version 3.5.x
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

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_LIBGD
#include <gd.h>
#endif

#include "ax203.h"
#ifdef HAVE_LIBJPEG
#include "jpeg_memsrcdest.h"
#endif

#if defined(HAVE_LIBGD) && defined(HAVE_LIBJPEG)
static int
locate_tables_n_write(JOCTET *jpeg, int jpeg_size, JOCTET table_type,
	uint8_t *outbuf, int *outc)
{
	int i, size, found, orig_outc = *outc;

	/* Reserve space for writing table size */
	*outc += 2;

	/* Locate tables and write them to outbuf */
	for (i = 2; i < jpeg_size; i += size) {
		if (jpeg[i] != 0xff) {
			gp_log (GP_LOG_ERROR, "ax203",
				"marker does not start with ff?");
			return GP_ERROR_CORRUPTED_DATA;
		}
		if (jpeg[i + 1] == 0xda)
			break;

		found = jpeg[i + 1] == table_type;
		i += 2;
		size = ((jpeg[i] << 8) | jpeg[i + 1]) - 2;
		i += 2;

		if (found) {
			memcpy(outbuf + *outc, jpeg + i, size);
			*outc += size;
		}
	}

	size = *outc - orig_outc;
	outbuf[orig_outc    ] = size >> 8;
	outbuf[orig_outc + 1] = size;

	return GP_OK;
}

static int
copy_huffman(uint8_t *dst, JOCTET *src, int n)
{
	int i, copied = 0;

	for (i = 0; i < n; i++) {
		/* Skip 0xff escaping 0x00's (ax203 special) */
		if (i && src[i - 1] == 0xff && src[i] == 0x00)
			continue;

		dst[copied++] = src[i];
	}

	return copied;
}

/* Store MCU info needed by the picture frame (probably so that it
   can start decompression with any block to allow transition effects) */
static void
add_mcu_info(uint8_t *outbuf, int block_nr, int last_Y, int last_Cb,
	int last_Cr, int huffman_addr)
{
	int info_addr = block_nr * 8;

	/* skip header */
	huffman_addr -= 16;
	info_addr += 16;

	/* This is crazy, but it is what the pictframe wants */
	huffman_addr -= 8 * block_nr;

	/* Store last DC vals + huffman data address */
	outbuf[info_addr + 0] = last_Y;
	outbuf[info_addr + 1] = last_Y >> 8;
	outbuf[info_addr + 2] = last_Cb;
	outbuf[info_addr + 3] = last_Cb >> 8;
	outbuf[info_addr + 4] = last_Cr;
	outbuf[info_addr + 5] = last_Cr >> 8;
	outbuf[info_addr + 6] = huffman_addr;
	outbuf[info_addr + 7] = huffman_addr >> 8;
}

int
ax206_compress_jpeg(Camera *camera, int **in, uint8_t *outbuf, int out_size,
	int width, int height)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_decompress_struct dinfo;
	struct jpeg_error_mgr jcerr, jderr;
	JSAMPROW row_pointer[1];
	JOCTET *buf = NULL, *regular_jpeg = NULL;
	jvirt_barray_ptr *in_coefficients;
	int i, x, y, stop, size, ret, outc;
	unsigned long regular_jpeg_size = 0, buf_size = 0;
	int last_dc_val[3] = { 0, 0, 0 };

	/* We have a rgb24bit image in the desired dimensions, first we
	   compress it into a regular jpeg, which we use as a base for
	   creating the ax203 JPEG-ish format */
	cinfo.err = jpeg_std_error (&jcerr);
	jpeg_create_compress (&cinfo);
	jpeg_mem_dest (&cinfo, &regular_jpeg, &regular_jpeg_size);
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults (&cinfo);
	cinfo.comp_info[0].h_samp_factor = 2;
	cinfo.comp_info[0].v_samp_factor = 2;

	jpeg_start_compress (&cinfo, TRUE);
	while( cinfo.next_scanline < cinfo.image_height ) {
		JOCTET row[width * 3];
		for (i = 0; i < width; i++) {
			int p = in[cinfo.next_scanline][i];
			row[i * 3 + 0] = gdTrueColorGetRed(p);
			row[i * 3 + 1] = gdTrueColorGetGreen(p);
			row[i * 3 + 2] = gdTrueColorGetBlue(p);
		}
		row_pointer[0] = row;
		jpeg_write_scanlines (&cinfo, row_pointer, 1);
	}
	jpeg_finish_compress (&cinfo);
	jpeg_destroy_compress (&cinfo);

	/* Write image header to outbuf */
	outbuf[0] = width >> 8;
	outbuf[1] = width;
	outbuf[2] = height >> 8;
	outbuf[3] = height;
	outbuf[4] = 3; /* 2x uv sub-sampling */

	outbuf[5] = 0;
	outbuf[6] = 1;
	outbuf[7] = 1;

	outbuf[8] = 0;
	outbuf[9] = 1;
	outbuf[10] = 1;

	outbuf[11] = 0;
	outbuf[12] = 1;
	outbuf[13] = 1;

	outbuf[14] = 0;
	outbuf[15] = 0;

	/* Make outc point to after the MCU info table, so to the start of
	   the quantisation tables */
	outc = 16 + ((width + 15) / 16) * ((height + 15) / 16) * 8;

	/* Locate quant tables and write them to outbuf */
	ret = locate_tables_n_write (regular_jpeg, regular_jpeg_size, 0xdb,
				     outbuf, &outc);
	if (ret < 0) return ret;

	/* Locate huffman tables and write them to outbuf */
	ret = locate_tables_n_write (regular_jpeg, regular_jpeg_size, 0xc4,
				     outbuf, &outc);
	if (ret < 0) return ret;

	/* The ax203 has 2 perculiarities in the huffman data for the JPEG
	   DCT coefficients:
	   1) It wants the MCU huffman data for each new MCU to start on a byte
	      boundary
	   2) The component order in an MCU is Cb Cr Y, rather then Y Cb Cr

	   We solve both these issues by decompressing the regular jpeg we've
	   just created into its raw coefficients, following by creating
	   mini JPEGs with the size of one MCU block, using
	   the raw-coefficients from the regular JPEG we first created.

	   This allows us to both shuffle the component order, and to byte
	   align the start of each MCU. */

	/* Get the raw coefficients from the regular JPEG */
	dinfo.err = jpeg_std_error (&jderr);
	jpeg_create_decompress (&dinfo);
	jpeg_mem_src (&dinfo, regular_jpeg, regular_jpeg_size);
	jpeg_read_header (&dinfo, TRUE);
	in_coefficients = jpeg_read_coefficients (&dinfo);

	/* Create a JPEG compression object for our mini JPEGs */
	cinfo.err = jpeg_std_error (&jcerr);
	jpeg_create_compress (&cinfo);
	cinfo.image_width = 16;
	cinfo.image_height = 16;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

#if JPEG_LIB_VERSION >= 80
	cinfo.min_DCT_h_scaled_size = dinfo.min_DCT_h_scaled_size;
	cinfo.min_DCT_v_scaled_size = dinfo.min_DCT_h_scaled_size;
	cinfo.jpeg_width = 16;
	cinfo.jpeg_height = 16;
#endif
	jpeg_set_defaults (&cinfo);
	/* We will write Cb values as comp. 0, so give it chroma settings */
	cinfo.comp_info[0].h_samp_factor = 1;
	cinfo.comp_info[0].v_samp_factor = 1;
	cinfo.comp_info[0].quant_tbl_no = 1;
	cinfo.comp_info[0].dc_tbl_no = 1;
	cinfo.comp_info[0].ac_tbl_no = 1;
	/* We will write Y values as comp. 2, so give it luma settings */
	cinfo.comp_info[2].h_samp_factor = 2;
	cinfo.comp_info[2].v_samp_factor = 2;
	cinfo.comp_info[2].quant_tbl_no = 0;
	cinfo.comp_info[2].dc_tbl_no = 0;
	cinfo.comp_info[2].ac_tbl_no = 0;

	/* And create a mini JPEG for each MCU with the shuffled component order
	   and extract its huffman data. This fixes our component order problem and
	   gives us the needed byte aligning for free. */
	for (y = 0; y < (height + 15) / 16; y++) {
		JBLOCKARRAY in_row[3], out_row[3];
		jvirt_barray_ptr out_barray[3];

		/* Notice the shuffling of components happening here !! */
		in_row[0] = dinfo.mem->access_virt_barray((j_common_ptr)&dinfo,
							  in_coefficients[1],
							  y, 1, 0);
		in_row[1] = dinfo.mem->access_virt_barray((j_common_ptr)&dinfo,
							  in_coefficients[2],
							  y, 1, 0);
		in_row[2] = dinfo.mem->access_virt_barray((j_common_ptr)&dinfo,
							   in_coefficients[0],
							   y * 2, 2, 0);

		for (x = 0; x < (width + 15) / 16; x++) {
			/* (Re)init our destination buffer */
			jpeg_mem_dest (&cinfo, &buf, &buf_size);

			/* Add MCU info block to output */
			add_mcu_info (outbuf, y * ((width + 15) / 16) + x,
				      last_dc_val[2], last_dc_val[0],
				      last_dc_val[1], outc);

			/* Allocate virtual arrays to store the coefficients
			   for the mini jpg we are making */
			out_barray[0] = cinfo.mem->request_virt_barray(
						(j_common_ptr)&cinfo,
						JPOOL_IMAGE, 0, 1, 1, 1);
			out_barray[1] = cinfo.mem->request_virt_barray(
						(j_common_ptr)&cinfo,
						JPOOL_IMAGE, 0, 1, 1, 1);
			out_barray[2] = cinfo.mem->request_virt_barray(
						(j_common_ptr)&cinfo,
						JPOOL_IMAGE, 0, 2, 2, 2);
			jpeg_write_coefficients (&cinfo, out_barray);

			/* Copy over our 3 (or 6) coefficient blocks, and
			   apply last_dc_val */
			out_row[0] = cinfo.mem->access_virt_barray(
						(j_common_ptr)&cinfo,
						out_barray[0], 0, 1, 1);
			out_row[1] = cinfo.mem->access_virt_barray(
						(j_common_ptr)&cinfo,
						out_barray[1], 0, 1, 1);
			out_row[2] = cinfo.mem->access_virt_barray(
						(j_common_ptr)&cinfo,
						out_barray[2], 0, 2, 1);

			for (i = 0; i < 2; i++) {
				memcpy (out_row[i][0][0],
					in_row[i][0][x],
					sizeof(JBLOCK));
				out_row[i][0][0][0] -= last_dc_val[i];
				last_dc_val[i] = in_row[i][0][x][0];
			}

			memcpy (out_row[2][0][0],
				in_row[2][0][x * 2 + 0],
				sizeof(JBLOCK));
			memcpy (out_row[2][0][1],
				in_row[2][0][x * 2 + 1],
				sizeof(JBLOCK));
			memcpy (out_row[2][1][0],
				in_row[2][1][x * 2 + 0],
				sizeof(JBLOCK));
			memcpy (out_row[2][1][1],
				in_row[2][1][x * 2 + 1],
				sizeof(JBLOCK));
			out_row[2][0][0][0] -= last_dc_val[2];
			out_row[2][0][1][0] -= last_dc_val[2];
			out_row[2][1][0][0] -= last_dc_val[2];
			out_row[2][1][1][0] -= last_dc_val[2];
			last_dc_val[2] = in_row[2][1][x * 2 + 1][0];

			jpeg_finish_compress (&cinfo);

			stop = 0;
			for (i = 2; i < buf_size && !stop; i += size) {
				stop = buf[i] == 0xff && buf[i + 1] == 0xda;
				i += 2;
				size = (buf[i] << 8) | buf[i + 1];
			}
			if (i >= buf_size) {
				gp_log (GP_LOG_ERROR, "ax203",
					"missing in ff da marker?");
				return GP_ERROR_CORRUPTED_DATA;
			}

			size = buf_size - i - 2;
			if ((outc + size) > out_size) {
				gp_log (GP_LOG_ERROR, "ax203",
					"jpeg output buffer overflow");
				return GP_ERROR_FIXED_LIMIT_EXCEEDED;
			}
			outc += copy_huffman(outbuf + outc, buf + i, size);

			/* Cleanup our memory dest */
			free (buf);
			buf = NULL;
			buf_size = 0;
		}
	}

	/* We're done with the jpeg decompress (input) and
	   compress (output) objs */
	jpeg_destroy_decompress (&dinfo);
	jpeg_destroy_compress (&cinfo);
	free(regular_jpeg);

	return outc;
}
#endif
