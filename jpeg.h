/* jpeg.h
 * This code was written by Nathan Stenzel for gphoto
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GPHOTO2_JPEG_H__
enum jpegmarker {
    JPEG_START=0xD8,    JPEG_APPO=0xE0,         JPEG_QUANTIZATION=0xDB,
    JPEG_HUFFMAN=0xC4,  JPEG_SSSEAHAL=0xDA,     JPEG_SOFC0=0xC0
};

const JPEG_MARKERS[] = {
    JPEG_START,         JPEG_APPO,              JPEG_QUANTIZATION,
    JPEG_HUFFMAN,       JPEG_SSSEAHAL,          JPEG_SOFC0
};

const char *JPEG_MARKERNAMES[] = {
    "Start",            "APPO",                 "Quantization table",
    "Huffman table",    "SsSeAhAl",             "SOFC0"
};

typedef struct chunk{
    int size;
    unsigned char *data;
} chunk;

void chunk_new(chunk *mychunk, int length);

void chunk_print(chunk *mychunk);

void chunk_destroy(chunk *mychunk);

typedef struct jpeg {
    int count;
    struct chunk marker[20]; /* I think this should be big enough */
}jpeg;

void jpeg_init(jpeg *myjpeg);

void jpeg_destroy(jpeg *myjpeg);

char jpeg_findff(int *location, chunk *picture);

char jpeg_findactivemarker(char *id, int *location, chunk *picture);

void jpeg_add_marker(jpeg *myjpeg, chunk *picture, int start, int end);

void jpeg_parse(jpeg *myjpeg, chunk *picture);

void jpeg_print(jpeg *myjpeg);

char *jpeg_markername(int c);

#endif
