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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ST2205_H__
#define __ST2205_H__
#include "config.h"

#include <stdio.h>
#ifdef HAVE_ICONV
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
#define ST2205_COUNT_OFFSET 0x6
#define ST2205_LOOKUP_OFFSET 0x8477
#define ST2205_LOOKUP_CHECKSUM 0x0016206f
#define ST2205_SHUFFLE_SIZE (128 * 160 / 64)
#define ST2205_HEADER_MARKER 0xf5
/* The "FAT" cannot contain more then 510 entries */
#define ST2205_MAX_NO_FILES 510
#define ST2205_FILENAME_LENGTH 10

/* We prefix all names with there idx in the FAT table, to make sure they are
   all unique and don't change when files with the same name (after converting
   to ascii and truncating) are added / deleted. */
#define ST2205_SET_FILENAME(dest, name, idx) \
	snprintf(dest, sizeof(st2205_filename), "%04d-%s.png", (idx) + 1, name)

struct st2205_coord {
	uint8_t x;
	uint8_t y;
};

#define CHECK(result) {int r=(result); if (r<0) return (r);}

/* The + 5 is space for the "####-" prefix we add, + 4 for .png postfix. */
typedef char st2205_filename[ST2205_FILENAME_LENGTH + 5 + 4 + 1];
typedef int16_t st2205_lookup_row[8];

struct _CameraPrivateLibrary {
	/* Used by library.c glue code */
#ifdef HAVE_ICONV
	iconv_t	cd;
#endif
	st2205_filename filenames[ST2205_MAX_NO_FILES];

	/* Used by st2205.c / st2205_decode.c */
	int width;
	int height;
	FILE *mem_dump;
	char *mem;
	char *buf; /* 512 bytes aligned buffer (for sending / reading cmds) */
	int mem_size;
	int block_is_present[2097152 / ST2205_BLOCK_SIZE];
	int block_dirty[2097152 / ST2205_BLOCK_SIZE];
	st2205_lookup_row lookup[3][256];
	struct st2205_coord shuffle[7][ST2205_SHUFFLE_SIZE];
	int no_shuffles;
	unsigned char unknown3[8];
	unsigned int rand_seed;
};

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

/* functions in st2205_decode.c */
int
st2205_decode_image(CameraPrivateLibrary *pl, unsigned char *src,
	int src_length, int **dest, uint8_t shuffle_pattern);

int
st2205_code_image(CameraPrivateLibrary *pl, int **src,
	unsigned char *dest, uint8_t shuffle_pattern, int allow_uv_corr);

#endif
