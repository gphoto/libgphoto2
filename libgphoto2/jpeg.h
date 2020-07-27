/** \file
 *
 * \author This code was written by Nathan Stenzel for gphoto
 *
 * \note
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \note
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \note
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __GPHOTO2_JPEG_H__
#define __GPHOTO2_JPEG_H__

#include <gphoto2/gphoto2-file.h>

typedef enum {
    JPEG_START=0xD8,        JPEG_COMMENT=0xFE,      JPEG_APPO=0xE0,
    JPEG_QUANTIZATION=0xDB, JPEG_HUFFMAN=0xC4,      JPEG_SOFC0=0xC0,
    JPEG_SSSEAHAL=0xDA,     JPEG_EOI=0xD9
} jpegmarker;

typedef struct chunk{
    int size;
    unsigned char *data;
} chunk;

typedef char jpeg_quantization_table[64];

typedef struct jpeg {
    int count;
    struct chunk *marker[20]; /* I think this should be big enough */
}jpeg;

chunk *gpi_jpeg_chunk_new(int length);
chunk *gpi_jpeg_chunk_new_filled(int length, char *data);
void gpi_jpeg_chunk_destroy(chunk *mychunk);
void gpi_jpeg_chunk_print(chunk *mychunk);

char  gpi_jpeg_findff(int *location, chunk *picture);
char  gpi_jpeg_findactivemarker(char *id, int *location, chunk *picture);
char *gpi_jpeg_markername(unsigned int c);

jpeg *gpi_jpeg_new        (void);
void  gpi_jpeg_destroy    (jpeg *myjpeg);
void  gpi_jpeg_add_marker (jpeg *myjpeg, chunk *picture, int start, int end);
void  gpi_jpeg_add_chunk  (jpeg *myjpeg, chunk *source);
void  gpi_jpeg_parse      (jpeg *myjpeg, chunk *picture);
void  gpi_jpeg_print      (jpeg *myjpeg);

chunk *gpi_jpeg_make_start   (void);
chunk *gpi_jpeg_make_SOFC    (int width, int height,
			     char vh1, char vh2, char vh3,
			     char q1, char q2, char q3);
chunk *gpi_jpeg_makeSsSeAhAl (int huffset1, int huffset2, int huffset3);

void gpi_jpeg_print_quantization_table(jpeg_quantization_table *table);
chunk *gpi_jpeg_make_quantization(const jpeg_quantization_table * table, char number);
jpeg_quantization_table *gpi_jpeg_quantization2table(chunk *qmarker);

jpeg *gpi_jpeg_header(int width, int height,
    char vh1, char vh2, char vh3,
    char q1, char q2, char q3,
    const jpeg_quantization_table *quant1, const jpeg_quantization_table *quant2,
    char huffset1, char huffset2, char huffset3,
    chunk *huff1, chunk *huff2, chunk *huff3, chunk *huff4);

char gpi_jpeg_write(CameraFile *file, const char *name, jpeg *myjpeg);
#endif
