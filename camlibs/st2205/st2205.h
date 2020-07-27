/* Sitronix st2205 picframe access library
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
#ifndef __ST2205_H__
#define __ST2205_H__
#include "config.h"

#include <stdio.h>
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
#include <iconv.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2-endian.h>

#define GP_MODULE "st2205"

#define ST2205_CMD_OFFSET 0x6200
#define ST2205_WRITE_OFFSET 0x6600
#define ST2205_READ_OFFSET 0xb000
/* 2 block sizes, first is bytes per transfer, second is bytes per erase /
   program cycle */
#define ST2205_BLOCK_SIZE 32768
#define ST2205_ERASE_BLOCK_SIZE 65536
#define ST2205_FAT_SIZE 8192
#define ST2205_COUNT_OFFSET 0x6
/* 16 bytes info per file + a 16 bytes header */
#define ST2205_FILE_OFFSET(x) (((x) + 1) * 16)
#define ST2205_LOOKUP_SIZE (0x8000 - 0x0477)
#define ST2205_V1_LOOKUP_OFFSET 0x8477
#define ST2205_V2_LOOKUP_OFFSET 0xd8477
#define ST2205_V1_FIRMWARE_SIZE 65536
#define ST2205_V2_FIRMWARE_SIZE 262144
#define ST2205_V1_PICTURE_START 65536
#define ST2205_V2_PICTURE_START 8192
#define ST2205_LOOKUP_CHECKSUM 0x0016206f
#define ST2205_SHUFFLE_SIZE (240 * 320 / 64)
#define ST2205_HEADER_MARKER 0xf5
/* The "FAT" cannot contain more then 510 entries */
#define ST2205_MAX_NO_FILES 510
#define ST2205_FILENAME_LENGTH 10

enum {
	ORIENTATION_AUTO,
	ORIENTATION_LANDSCAPE,
	ORIENTATION_PORTRAIT,
};

/* We prefix all names with there idx in the FAT table, to make sure they are
   all unique and don't change when files with the same name (after converting
   to ascii and truncating) are added / deleted. */
#define ST2205_SET_FILENAME(dest, name, idx) \
	snprintf(dest, sizeof(st2205_filename), "%04d-%s.png", (idx) + 1, name)

struct st2205_coord {
	uint16_t x;
	uint16_t y;
};

struct st2205_image_header {
	uint8_t marker;   /* Always 0xF5 */
	uint16_t width;	  /* big endian */
	uint16_t height;  /* big endian */
	uint16_t blocks;  /* number of 8x8 blocks in the image, big endian */
	uint8_t shuffle_table; /* shuffle table idx (for transition effects) */
	uint8_t unknown2; /* Unknown usually 0x04 */
	uint8_t unknown3; /* shuffle related, must have a special value depend
			     on the pattern see the table in st2205_init */
	uint16_t length;  /* length of the data *following* the header (be) */
	uint8_t unknown4[4]; /* Always 4x 0x00 (padding ?) */
} __attribute__((packed));

#define CHECK(result) {int r=(result); if (r<0) return (r);}

/* The + 5 is space for the "####-" prefix we add, + 4 for .png postfix. */
typedef char st2205_filename[ST2205_FILENAME_LENGTH + 5 + 4 + 1];
typedef int16_t st2205_lookup_row[8];

struct _CameraPrivateLibrary {
	/* Used by library.c glue code */
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
	iconv_t	cd;
#endif
	st2205_filename filenames[ST2205_MAX_NO_FILES];

	/* Driver configuration settings */
	int syncdatetime;
	int orientation;

	/* Used by st2205.c / st2205_decode.c */
	int width;
	int height;
	int compressed; /* Is the image data compressed or rgb565 ? */
	FILE *mem_dump;
	char *mem;
	char *buf; /* 512 bytes aligned buffer (for sending / reading cmds) */
	int mem_size;
	int firmware_size;
	int picture_start;
	int no_fats;
	int block_is_present[2097152 / ST2205_BLOCK_SIZE];
	int block_dirty[2097152 / ST2205_BLOCK_SIZE];
	struct st2205_coord shuffle[8][ST2205_SHUFFLE_SIZE];
	int no_shuffles;
	unsigned char unknown3[8];
	unsigned int rand_seed;
};

/* tables in st2205_tables.c */
extern const st2205_lookup_row st2205_lookup[3][256];
extern const uint8_t st2205_shuffle_data[10360];

/* functions in st2205.c */
int
st2205_open_device(Camera *camera);

int
st2205_open_dump(Camera *camera, const char *dump,
		 int width, int height);

void st2205_close(Camera *camera);

int
st2205_get_filenames(Camera *camera, st2205_filename *names);

int
st2205_read_file(Camera *camera, int idx, int **rgb24);

int
st2205_read_raw_file(Camera *camera, int idx, unsigned char **raw);

int
st2205_set_time_and_date(Camera *camera, struct tm *t);

/* This function returns the index used to save the file at on success */
int
st2205_write_file(Camera *camera,
	const char *filename, int **rgb24);

int
st2205_delete_file(Camera *camera, int idx);

int
st2205_delete_all(Camera *camera);

int
st2205_commit(Camera *camera);

int
st2205_get_mem_size(Camera *camera);

int
st2205_get_free_mem_size(Camera *camera);

/* functions in st2205_decode.c */
int
st2205_decode_image(CameraPrivateLibrary *pl, unsigned char *src, int **dest);

int
st2205_code_image(CameraPrivateLibrary *pl, int **src,
	unsigned char *dest, uint8_t shuffle_pattern, int allow_uv_corr);

int
st2205_rgb565_to_rgb24(CameraPrivateLibrary *pl, unsigned char *src,
	int **dest);

int
st2205_rgb24_to_rgb565(CameraPrivateLibrary *pl, int **src,
	unsigned char *dest);

#endif
