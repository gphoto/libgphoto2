/* qtk-helpers.c
 *
   Copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net
 * Copyright 2023, Colin Leroy-Mira <colin@colino.net>
 *
 * getbithuff() heavily inspired from dcraw.c.
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

/* Write a basic .qtk header. This is imperfect and may not allow to open the
 * raw files we generate with the official, vintage Quicktake software, but it
 * is enough for dcraw to open and convert it.
 */
void
qtk_raw_header(unsigned char *data, const char *pic_format)
{
	char hdr[] = {0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x04,0x00,0x00,0x73,0xE4,0x00,0x01};

	memcpy(hdr, pic_format, 4);
	memcpy(data, hdr, sizeof hdr);
}

char *qtk_ppm_header(int width, int height) {
	char *header = malloc(128);
	if (header == NULL)
		return NULL;

	snprintf(header, 127,
					 "P6\n%d %d\n%d\n",
					 width, height, 255);

	return header;
}

int qtk_ppm_size(int width, int height) {
	char *header;
	int len;

	header = qtk_ppm_header(width, height);
	if (header == NULL) {
		return GP_ERROR_NO_MEMORY;
	}

	len = (width * height * 3) + strlen(header);
	free(header);

	return len;
}

unsigned char getbithuff (int nbits, unsigned char **raw, ushort *huff)
{
	static unsigned bitbuf = 0;
	static int vbits = 0;
	unsigned char c;
	int h;
	unsigned char *ptr;

	if (nbits == -1) {
		bitbuf = 0;
		vbits = 0;
		return 0;
	}

	ptr = *raw;
	if (vbits < nbits) {
		c = *ptr;
		ptr++; (*raw)++;
		bitbuf = (bitbuf << 8) + c;
		vbits += 8;
  }
	c = bitbuf << (32-vbits) >> (32-nbits);

	if (!huff)
		vbits -= nbits;
	else {
		h = huff[c];
		vbits -= h >> 8;
		c = (unsigned char) h;
	}

	return c;
}
