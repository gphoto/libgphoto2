/* Tenx tp6801 picframe access library
 *
 *   Copyright (c) 2011 Hans de Goede <hdegoede@redhat.com>
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
#ifndef __TP6801_H__
#define __TP6801_H__
#include "config.h"

#include <stdio.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2-endian.h>

#define GP_MODULE "tp6801"

#define TP6801_PAT_MAGIC		"erutangiS metsyS eliF egamI 1086PT xneT"
#define TP6801_PAT_MAGIC_OFFSET		0x1e80
#define TP6801_PAT_OFFSET		0x1e00
#define TP6801_PAT_PAGE			(TP6801_PAT_OFFSET / TP6801_PAGE_SIZE)
#define TP6801_PAT_SIZE			256 /* Including the magic */
#define TP6801_PAT_ENTRY_PRE_ERASED	0xff
/* Windows software uses 0xfe to mark as deleted, the frame itself 0x00 */
#define TP6801_PAT_ENTRY_DELETED_FRAME	0x00
#define TP6801_PAT_ENTRY_DELETED_WIN	0xfe
#define TP6801_PAT_ENTRY_DELETED(x)	((x) == 0xfe || (x) == 0x00)
#define TP6801_PICTURE_OFFSET(i, size)	(0x10000 + (i) * (size))
#define TP6801_READ			0xC1
#define TP6801_ERASE_BLOCK		0xC6
#define TP6801_SET_TIME			0xCA
#define TP6801_PROGRAM_PAGE		0xCB
#define TP6801_BLOCK_SIZE		65536
#define TP6801_PAGE_SIZE		256
/* USB bulk transfers are 32k max */
#define TP6801_MAX_READ			(32768 / TP6801_PAGE_SIZE)
#define TP6801_MAX_MEM_SIZE		4194304
#define TP6801_CONST_DATA_SIZE		393216
#define TP6801_SCSI_MODEL_OFFSET	32
#define TP6801_SCSI_MODEL_LEN		32
#define TP6801_ISO_OFFSET		256
/* page_state flags */
#define TP6801_PAGE_READ		0x01
#define TP6801_PAGE_DIRTY		0x02
#define TP6801_PAGE_CONTAINS_DATA	0x04
#define TP6801_PAGE_NEEDS_ERASE		0x08

#define CHECK(result) {int r=(result); if (r<0) return (r);}

struct _CameraPrivateLibrary {
	FILE *mem_dump;
	char *mem;
	unsigned char *pat;
	char page_state[TP6801_MAX_MEM_SIZE / TP6801_PAGE_SIZE];
	unsigned char last_cmd;
	int picture_count;
	/* LCD display attributes */
	int width;
	int height;
	/* EEPROM attributes */
	int mem_size;
	/* Driver configuration settings */
	int syncdatetime;
};

struct tp6801_devinfo {
	unsigned short vendor_id;
	unsigned short product_id;
};

/* functions in tp6801.c */
int
tp6801_open_device(Camera *camera);

int
tp6801_open_dump(Camera *camera, const char *dump);

void tp6801_close(Camera *camera);

int
tp6801_max_filecount(Camera *camera);

int
tp6801_file_present(Camera *camera, int idx);

int
tp6801_read_raw_file(Camera *camera, int idx, char **raw);

int
tp6801_read_file(Camera *camera, int idx, int **rgb24);

int
tp6801_write_file(Camera *camera, int **rgb24);

int
tp6801_delete_file(Camera *camera, int idx);

int
tp6801_delete_all(Camera *camera);

int
tp6801_commit(Camera *camera);

int
tp6801_get_mem_size(Camera *camera);

int
tp6801_get_free_mem_size(Camera *camera);

int
tp6801_set_time_and_date(Camera *camera, struct tm *t);

int
tp6801_filesize(Camera *camera);

#endif
