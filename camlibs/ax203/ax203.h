/* Appotech ax203 picframe access library
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
#ifndef __AX203_H__
#define __AX203_H__
#include "config.h"

#include <stdio.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2-endian.h>

#define GP_MODULE "ax203"

#define AX203_ABFS_MAGIC		"ABFS"
#define AX203_ABFS_START		0x50000
#define AX203_ABFS_COUNT_OFFSET		0x05
#define AX203_ABFS_FILE_OFFSET(idx)     (0x20 + 2 * (idx))
#define AX203_ABFS_SIZE			0x2000
#define AX203_PICTURE_START		0x52000

#define AX203_TO_DEV		0xCB
#define AX203_FROM_DEV		0xCD
#define AX203_EEPROM_CMD	0x00
#define AX203_GET_VERSION	0x01
#define AX203_GET_LCD_SIZE	0x02

/* Note not all SPI EEPROM's actually have 4k sectors, some have
   64k sectors, ax203_commit() takes care if this. */
#define SPI_EEPROM_SECTOR_SIZE	4096
#define SPI_EEPROM_BLOCK_SIZE	65536
#define SPI_EEPROM_PP		0x02
#define SPI_EEPROM_READ		0x03
#define SPI_EEPROM_RDSR		0x05 /* ReaD Status Register */
#define SPI_EEPROM_WREN		0x06
#define SPI_EEPROM_ERASE_4K	0x20
#define SPI_EEPROM_RDID		0x9f /* Read Device ID */
#define SPI_EEPROM_RDP		0xab /* Release from Deep Powerdown */
#define SPI_EEPROM_ERASE_64K	0xd8

#define CHECK(result) {int r=(result); if (r<0) return (r);}

enum ax203_firmware {
        AX203_FIRMWARE_3_3_x,
        AX203_FIRMWARE_3_4_x,
        AX203_FIRMWARE_3_5_x,
};

enum ax203_compression {
        AX203_COMPRESSION_YUV,
        AX203_COMPRESSION_YUV_DELTA,
        AX203_COMPRESSION_UNKNOWN,
};

struct _CameraPrivateLibrary {
	FILE *mem_dump;
	char *mem;
	int sector_is_present[2097152 / SPI_EEPROM_SECTOR_SIZE];
	int sector_dirty[2097152 / SPI_EEPROM_SECTOR_SIZE];
	int fs_start;
	/* LCD display attributes */
	int width;
	int height;
	/* USB "bridge" / firmware attributes */
        enum ax203_firmware firmware_version;
        enum ax203_compression compression_version;
	/* EEPROM attributes */
	int mem_size;
	int has_4k_sectors;
};

struct ax203_devinfo {
	unsigned short vendor_id;
	unsigned short product_id;
        int firmware_version;
        int compression_version;
};

struct ax203_fileinfo {
        int address;
        int present;
        int size;
};

/* functions in ax203.c */
int
ax203_open_device(Camera *camera);

int
ax203_open_dump(Camera *camera, const char *dump);

void ax203_close(Camera *camera);

int
ax203_read_filecount(Camera *camera);

int
ax203_file_present(Camera *camera, int idx);

int
ax203_read_file(Camera *camera, int idx, int **rgb24);

int
ax203_write_file(Camera *camera, int **rgb24);

int
ax203_delete_file(Camera *camera, int idx);

int
ax203_delete_all(Camera *camera);

int
ax203_commit(Camera *camera);

int
ax203_get_mem_size(Camera *camera);

/* functions in ax203_decode_*.c */

void
ax203_decode_yuv(char *src, int **dest, int width, int height);

void
ax203_encode_yuv(int **src, char *dest, int width, int height);

void
ax203_decode_yuv_delta(char *src, int **dest, int width, int height);

void
ax203_encode_yuv_delta(int **src, char *dest, int width, int height);

#endif
