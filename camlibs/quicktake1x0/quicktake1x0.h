/* quicktake1x0.h
 *
 * Copyright 2023 Colin Leroy-Mira <colin@colino.net>
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

#ifndef CAMLIBS_QUICKTAKE_1X0_H
#define CAMLIBS_QUICKTAKE_1X0_H

#define CHECK_RESULT(result) {int r = result; if (r < 0) return (r);}

typedef enum {
	QUICKTAKE_MODEL_UNKNOWN = 0,
	QUICKTAKE_MODEL_100,
	QUICKTAKE_MODEL_150,
} Quicktake1x0Model;

typedef enum {
	QUALITY_HIGH     = 0x10,
	QUALITY_STANDARD = 0x20
} QuickTake1x0Quality;

typedef enum {
	FLASH_AUTO = 0,
	FLASH_OFF,
	FLASH_ON
} Quicktake1x0FlashMode;

struct _CameraPrivateLibrary {
	Quicktake1x0Model model;

	int info_fetched;

	Quicktake1x0FlashMode flash_mode;
	QuickTake1x0Quality quality_mode;

	int num_pictures;
	char name[33];
	int num_pics;
	int free_pics;
	int battery_level;
	int day;
	int month;
	int year;
	int hour;
	int minute;
};

#define QT1X0_THUMB_WIDTH 80
#define QT1X0_THUMB_HEIGHT 60
#define QT1X0_THUMB_SIZE (QT1X0_THUMB_WIDTH * QT1X0_THUMB_HEIGHT / 2)

/* Decoders */
char *qtk_ppm_header(int width, int height);
void qtk_raw_header(unsigned char *data, const char *pic_format);

int qtk_ppm_size(int width, int height);
int qtk_thumbnail_decode(unsigned char *raw, unsigned char **out, Quicktake1x0Model model);
int qtkt_decode(unsigned char *raw, int width, int height, unsigned char **out);
int qtkn_decode(unsigned char *raw, int width, int height, unsigned char **out);
unsigned char getbithuff (int nbits, unsigned char **raw, unsigned short *huff);

#define ABS(x) (((int)(x) ^ ((int)(x) >> 31)) - ((int)(x) >> 31))
#define LIM(x,min,max) MAX(min,MIN(x,max))
#define RAW(ptr, row,col) (ptr)[(row)*width+(col)]
#define getbits(n, raw) getbithuff(n, raw, 0)

#endif /* !defined(CAMLIBS_QUICKTAKE_1X0_H) */
