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
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gphoto2/gphoto2-result.h>
#include "st2205.h"

struct image_table_entry {
	uint8_t present;  /* 1 when this image is present, 0 when deleted */
	uint32_t address; /* memory address where this image is stored */
	char name[ST2205_FILENAME_LENGTH + 1]; /* Image name (11 bytes) */
} __attribute__((packed));

struct image_header {
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

/* The st2205 port driver's write and read functions require page aligned
   buffers, as they use O_DIRECT. */
static char *st2205_malloc_page_aligned(int size)
{
	int fd;
	char *aligned;
	
	fd = open ("/dev/zero", O_RDWR);
	aligned = mmap (0, size, PROT_READ|PROT_WRITE,MAP_PRIVATE, fd, 0);
	if (aligned == MAP_FAILED)
		return NULL;

	return aligned;
}

static void st2205_free_page_aligned(char *aligned, int size)
{
	if (aligned != NULL)
		munmap(aligned, size);
}

static int
st2205_send_command(Camera *camera, int cmd, int arg1, int arg2)
{
	char *buf = camera->pl->buf;

	if (gp_port_seek (camera->port, ST2205_CMD_OFFSET, SEEK_SET) !=
	    ST2205_CMD_OFFSET)
		return GP_ERROR_IO;

	memset(buf, 0, 512);
	buf[0] = cmd;
	buf[1] = (arg1 >> 24) & 0xff;
	buf[2] = (arg1 >> 16) & 0xff;
	buf[3] = (arg1 >>  8) & 0xff;
	buf[4] = (arg1      ) & 0xff;
	buf[5] = (arg2 >> 24) & 0xff;
	buf[6] = (arg2 >> 16) & 0xff;
	buf[7] = (arg2 >>  8) & 0xff;
	buf[8] = (arg2      ) & 0xff;

	if (gp_port_write (camera->port, buf, 512) != 512)
		return GP_ERROR_IO_WRITE;

	return GP_OK;
}

static int
st2205_read_block(Camera *camera, int block, char *buf)
{
	int ret;
	if (camera->pl->mem_dump) {
		ret = fseek(camera->pl->mem_dump, block * ST2205_BLOCK_SIZE,
			    SEEK_SET);
		if (ret) {
			gp_log (GP_LOG_ERROR, "st2205",
				"seeking in memdump: %s", strerror(errno));
			return GP_ERROR_IO_READ;
		}
		ret = fread(buf, 1, ST2205_BLOCK_SIZE, camera->pl->mem_dump);
		if (ret != ST2205_BLOCK_SIZE) {
			if (ret < 0)
				gp_log (GP_LOG_ERROR, "st2205",
					"reading memdump: %s",
					strerror(errno));
			else
				gp_log (GP_LOG_ERROR, "st2205",
					"short read reading from memdump");
			return GP_ERROR_IO_READ;
		}
	} else {
		CHECK (st2205_send_command (camera, 4, block,
					    ST2205_BLOCK_SIZE))
		if (gp_port_seek (camera->port, ST2205_READ_OFFSET, SEEK_SET)
				!= ST2205_READ_OFFSET)
			return GP_ERROR_IO;

		if (gp_port_read (camera->port, buf, ST2205_BLOCK_SIZE)
				!= ST2205_BLOCK_SIZE)
			return GP_ERROR_IO_READ;
	}
	return GP_OK;
}

static int
st2205_write_block(Camera *camera, int block, char *buf)
{
	int ret;
	if (camera->pl->mem_dump) {
		ret = fseek(camera->pl->mem_dump, block * ST2205_BLOCK_SIZE,
			    SEEK_SET);
		if (ret) {
			gp_log (GP_LOG_ERROR, "st2205",
				"seeking in memdump: %s", strerror(errno));
			return GP_ERROR_IO_WRITE;
		}
		ret = fwrite(buf, 1, ST2205_BLOCK_SIZE, camera->pl->mem_dump);
		if (ret != ST2205_BLOCK_SIZE) {
			gp_log (GP_LOG_ERROR, "st2205",
				"writing memdump: %s", strerror(errno));
			return GP_ERROR_IO_WRITE;
		}
	} else {
		/* Prepare for write */
		CHECK (st2205_send_command (camera, 3, block,
					    ST2205_BLOCK_SIZE))
		/* Write */
		if (gp_port_seek (camera->port, ST2205_WRITE_OFFSET, SEEK_SET)
				!= ST2205_WRITE_OFFSET)
			return GP_ERROR_IO;

		if (gp_port_write (camera->port, buf, ST2205_BLOCK_SIZE)
				!= ST2205_BLOCK_SIZE)
			return GP_ERROR_IO_WRITE;
		/* Commit */
		CHECK (st2205_send_command (camera, 2, block,
					    ST2205_BLOCK_SIZE))
		/* Read commit response (ignored) */
		if (gp_port_seek (camera->port, ST2205_READ_OFFSET, SEEK_SET)
				!= ST2205_READ_OFFSET)
			return GP_ERROR_IO;

		if (gp_port_read (camera->port, camera->pl->buf, 512)
				!= 512)
			return GP_ERROR_IO_READ;
	}
	return GP_OK;
}

static int
st2205_check_block_present(Camera *camera, int block)
{
	int ret;

	if ((block + 1) * ST2205_BLOCK_SIZE > camera->pl->mem_size) {
		gp_log (GP_LOG_ERROR, "st2205", "read beyond end of memory");
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (camera->pl->block_is_present[block])
		return GP_OK;

	ret = st2205_read_block(camera, block, camera->pl->mem +
				block * ST2205_BLOCK_SIZE);
	if (ret == 0)
		camera->pl->block_is_present[block] = 1;

	return ret;
}

static int
st2205_detect_mem_size(Camera *camera)
{
	char *buf0, *buf1;
	int i, ret;

	buf0 = st2205_malloc_page_aligned(ST2205_BLOCK_SIZE);
	buf1 = st2205_malloc_page_aligned(ST2205_BLOCK_SIZE);
	if (!buf0 || !buf1) {
		st2205_free_page_aligned(buf0, ST2205_BLOCK_SIZE);
		st2205_free_page_aligned(buf1, ST2205_BLOCK_SIZE);
		return GP_ERROR_NO_MEMORY;
	}

	ret = st2205_read_block(camera, 0, buf0);
	if (ret)
		return ret;

	for (i = 0; i < 2; i++) {
		ret = st2205_read_block(camera,
					(524288 / ST2205_BLOCK_SIZE) << i,
					buf1);
		if (ret)
			return ret;
		if (memcmp(buf0, buf1, ST2205_BLOCK_SIZE) == 0)
			break;
	}

	camera->pl->mem_size = 524288 << i;	
	GP_DEBUG ("Detect %d bytes of flash memory", camera->pl->mem_size);

	st2205_free_page_aligned(buf0, ST2205_BLOCK_SIZE);
	st2205_free_page_aligned(buf1, ST2205_BLOCK_SIZE);

	return GP_OK;	
}

static int
st2205_read_mem(Camera *camera, int offset,
	void *buf, int len)
{
	int to_copy, block = offset / ST2205_BLOCK_SIZE;

	while (len) {
		CHECK (st2205_check_block_present (camera, block))

		to_copy = ST2205_BLOCK_SIZE - (offset % ST2205_BLOCK_SIZE);
		if (to_copy > len)
			to_copy = len;

		memcpy(buf, camera->pl->mem + offset, to_copy);
		buf += to_copy;
		len -= to_copy;
		offset += to_copy;
		block++;
	}
	return GP_OK;
}

static int
st2205_write_mem(Camera *camera, int offset,
	void *buf, int len)
{
	int to_copy, block = offset / ST2205_BLOCK_SIZE;

	/* Don't allow writing to the firmware space */
	if ((offset + len) >
	    (camera->pl->mem_size - camera->pl->firmware_size)) {
		gp_log (GP_LOG_ERROR, "st2205", "write beyond end of memory");
		return GP_ERROR_CORRUPTED_DATA;
	}

	while (len) {
		CHECK (st2205_check_block_present (camera, block))

		to_copy = ST2205_BLOCK_SIZE - (offset % ST2205_BLOCK_SIZE);
		if (to_copy > len)
			to_copy = len;

		memcpy(camera->pl->mem + offset, buf, to_copy);
		camera->pl->block_dirty[block] = 1;

		buf += to_copy;
		len -= to_copy;
		offset += to_copy;
		block++;
	}
	return GP_OK;
}

static int
st2205_read_file_count(Camera *camera)
{
	uint8_t count;

	CHECK (st2205_read_mem (camera, ST2205_COUNT_OFFSET, &count, 1))

	return count;
}

static int
st2205_write_file_count(Camera *camera, int count)
{
	uint8_t c = count;

	CHECK (st2205_write_mem (camera, ST2205_COUNT_OFFSET, &c, 1))

	return GP_OK;
}

static int
st2205_calc_fat_checksum(Camera *camera)
{
	int i, checksum = 0;

	CHECK (st2205_check_block_present(camera, 0))

	/* Calculate the "FAT" checksum, note that the present bits are skipped
	   (as is the checksum location itself). The picframe itself does not
	   care about this, but the windows software does! */
	for (i = 2; i < ST2205_FAT_SIZE; i++)
		if (i % 16)
			checksum += (uint8_t)camera->pl->mem[i];

	return checksum & 0xffff;
}

static int
st2205_check_fat_checksum(Camera *camera)
{
	int checksum, expected_checksum;

	CHECK (st2205_check_block_present (camera, 0))
	checksum = le16atoh ((uint8_t *)camera->pl->mem);

	expected_checksum = st2205_calc_fat_checksum (camera);
	if (expected_checksum < 0) return expected_checksum;

	if (checksum != expected_checksum) {
		gp_log (GP_LOG_ERROR, "st2205",
			"image table checksum mismatch");
		return GP_ERROR_CORRUPTED_DATA;
	}

	return GP_OK;
}

static int
st2205_update_fat_checksum(Camera *camera)
{
	int checksum;
	uint8_t buf[2];

	checksum = st2205_calc_fat_checksum(camera);
	if (checksum < 0) return checksum;

	htole16a(buf, checksum);
	return st2205_write_mem (camera, 0, buf, 2);
}

static int st2205_copy_fat(Camera *camera)
{
	int i;

	/* The "FAT" repeats itself on some frames, copy it over */
	CHECK (st2205_check_block_present (camera, 0))
	for (i = 1; i < camera->pl->no_fats; i++)
		CHECK (st2205_write_mem (camera, i * ST2205_FAT_SIZE,
					 camera->pl->mem, ST2205_FAT_SIZE))

	return GP_OK;
}

static int
st2205_add_picture(Camera *camera, int idx, const char *filename,
	int start, int shuffle, unsigned char *buf, int size)
{
	int count;
	struct image_table_entry entry;
	struct image_header header;

	count = st2205_read_file_count(camera);
	if (count < 0) return count;

	if (idx > count) {
		gp_log (GP_LOG_ERROR, "st2205",
			"adding picture beyond end of FAT");
		return GP_ERROR_BAD_PARAMETERS;
	}

	memset(&entry, 0, sizeof(entry));
	entry.present = 1;
	entry.address = htole32(start);
	snprintf(entry.name, sizeof(entry.name), "%s", filename);
	CHECK (st2205_write_mem (camera, ST2205_FILE_OFFSET (idx),
				 &entry, sizeof(entry)))

	if (idx == count) {
		/* update picture count */
		count++;
		CHECK (st2205_write_file_count (camera, count))

		/* Add a fake last entry, with no name pointing to beginning
		   of freespace, neither the windows software nor the picframe
		   seem to care about this, but the windows software does this,
		   so lets do it too. */
		memset (&entry, 0, sizeof(entry));
		entry.address = htole32 (start + size + sizeof(header));
		CHECK (st2205_write_mem (camera, ST2205_FILE_OFFSET (count),
					 &entry, sizeof(entry)))
	}

	CHECK (st2205_update_fat_checksum (camera))
	CHECK (st2205_copy_fat (camera))

	memset(&header, 0, sizeof(header));
	header.marker = ST2205_HEADER_MARKER;
	header.width  = htobe16(camera->pl->width);
	header.height = htobe16(camera->pl->height);
	header.blocks = htobe16((camera->pl->width * camera->pl->height) / 64);
	header.shuffle_table = shuffle;
	header.unknown2 = 0x04;
	header.unknown3 = camera->pl->unknown3[shuffle];
	header.length = htobe16(size);
	CHECK (st2205_write_mem (camera, start,
				 &header, sizeof(header)))
	CHECK (st2205_write_mem (camera, start + sizeof(header), buf, size))

	/* Let the caller know at which index we stored the table entry */
	return idx;
}

static int st2205_file_present(Camera *camera, int idx)
{
	struct image_table_entry entry;

	CHECK (st2205_read_mem (camera, ST2205_FILE_OFFSET (idx),
				&entry, sizeof(entry)))

	return entry.present;
}

/***************** Begin "public" functions *****************/

/* Note this function assumes the names array is filled with zeros
   before it gets called */
int
st2205_get_filenames(Camera *camera, st2205_filename *names)
{
	int i, count;
	struct image_table_entry entry;

	count = st2205_read_file_count(camera);
	if (count < 0) return count;

	if (count > ST2205_MAX_NO_FILES) {
		gp_log (GP_LOG_ERROR, "st2205", "file table count overflow");
		return GP_ERROR_CORRUPTED_DATA;
	}

	for (i = 0; i < count; i++) {
		CHECK (st2205_read_mem (camera, ST2205_FILE_OFFSET (i),
					&entry, sizeof(entry)))

		if (!entry.present)
			continue;

		/* Note we use memcpy as we don't want to depend on the names
		   on the picframe being 0-terminated. We always write 0
		   terminated names, as does windows, but better safe then
		   sorry. */
		memcpy(names[i], entry.name, ST2205_FILENAME_LENGTH);
		/* Make sure a file with no name gets a "name" */
		if (!names[i][0])
			names[i][0] = '?';
	}

	return GP_OK;
}

int
st2205_read_file(Camera *camera, int idx, int **rgb24)
{
	unsigned char *src;
	struct image_table_entry entry;
	struct image_header header;
	int ret, count;

	count = st2205_read_file_count(camera);
	if (count < 0) return count;

	if (idx >= count) {
		gp_log (GP_LOG_ERROR, "st2205",
			"read file beyond end of FAT");
		return GP_ERROR_BAD_PARAMETERS;
	}

	CHECK (st2205_read_mem (camera, ST2205_FILE_OFFSET (idx),
				&entry, sizeof(entry)))

	/* This should never happen */
	if (!entry.present) {
		gp_log (GP_LOG_ERROR, "st2205",
			"trying to read a deleted file");
		return GP_ERROR_BAD_PARAMETERS;
	}
	LE32TOH(entry.address);

	GP_DEBUG ("file: %d start at: %08x\n", idx, entry.address);

	CHECK (st2205_read_mem (camera, entry.address,
				&header, sizeof(header)))

	if (header.marker != ST2205_HEADER_MARKER) {
		gp_log (GP_LOG_ERROR, "st2205", "invalid header magic");
		return GP_ERROR_CORRUPTED_DATA;
	}

	BE16TOH(header.width);
	BE16TOH(header.height);
	BE16TOH(header.length);
	BE16TOH(header.blocks);

	if ((header.width != camera->pl->width) ||
	    (header.height != camera->pl->height)) {
		gp_log (GP_LOG_ERROR, "st2205",
			"picture size does not match frame size.");
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (((header.width / 8) * (header.height / 8)) != header.blocks) {
		gp_log (GP_LOG_ERROR, "st2205", "invalid block count");
		return GP_ERROR_CORRUPTED_DATA;
	}

	GP_DEBUG ("file: %d header read, size: %dx%d, length: %d bytes\n",
		  idx, header.width, header.height, header.length);

	src = malloc(header.length);
	if (!src) {
		gp_log (GP_LOG_ERROR, "st2205", "allocating memory");
		return GP_ERROR_NO_MEMORY;
	}

	ret = st2205_read_mem (camera, entry.address + sizeof(header), src,
			       header.length);
	if (ret < 0) { free(src); return ret; }

	ret = st2205_decode_image (camera->pl, src, header.length, rgb24,
				   header.shuffle_table);
	free(src);

	return ret;
}

static int
st2205_real_write_file(Camera *camera,
	const char *filename, int **rgb24, unsigned char *buf,
	int shuffle, int allow_uv_corr)
{
	int size, count;
	struct image_table_entry entry;
	struct image_header header;
	int i, start, end, hole_start = 0, hole_idx = 0;

	size = st2205_code_image (camera->pl, rgb24, buf, shuffle,
				  allow_uv_corr);
	if (size < 0) return size;

	count = st2205_read_file_count (camera);
	if (count < 0) return count;

	/* Try to find a large enough "hole" in the memory */
	end = camera->pl->picture_start;
	for (i = 0; i <= count; i++) {
		/* Fake a present entry at the end of picture mem */
		if (i == count) {
			entry.present = 1;
			start = camera->pl->mem_size -
				camera->pl->firmware_size;
			/* If the last entry in the "FAT" was present, we need
			   to set hole_start to the end of the last picture */
			if (!hole_start) {
				hole_start = end;
				hole_idx = i;
			}
		} else {
			CHECK (st2205_read_mem (camera, ST2205_FILE_OFFSET (i),
						(unsigned char *)&entry,
						sizeof(entry)))

			start = entry.address;
			if (entry.present) {
				CHECK (st2205_read_mem (camera, start,
							&header,
							sizeof(header)))

				BE16TOH(header.length);
				end = start + sizeof(header) + header.length;
			}
		}

		/* If we have a hole start address look for present entries (so a hole end
			 address), otherwise look for non present entries */
		if (hole_start) {
			if (entry.present) {
				int hole_size = start - hole_start;
				GP_DEBUG ("found a hole at: %08x, of %d bytes "
					  "(need %d)\n", hole_start, hole_size,
					  (int)(size + sizeof(header)));
				if (hole_size < (size + sizeof(header))) {
					/* Too small, start searching
					   for the next hole */
					hole_start = 0;
					continue;
				}

				/* bingo we have a large enough hole */
				return st2205_add_picture(camera, hole_idx,
						filename, hole_start, shuffle,
						buf, size);
			}
		} else {
			if (!entry.present) {
				/* We have a hole starting at the end of the
				   *previous* picture, note that end at this
				   moment points to the end of the last present
				   picture, as we don't set end for non present
				   pictures, which is exactly what we want. */
				hole_start = end;
				hole_idx = i;
			}
		}
	}
	
	/* No freespace found, try again with uv correction tables disabled */
	if (allow_uv_corr)
		return st2205_real_write_file (camera, filename, rgb24, buf,
					       shuffle, 0);

	gp_log (GP_LOG_ERROR, "st2205", "not enough freespace to add file %s",
		filename);
	return GP_ERROR_NO_SPACE;
}

int
st2205_write_file(Camera *camera,
	const char *filename, int **rgb24)
{
	/* The buffer must be large enough for the worst case scenario */
	unsigned char buf[(camera->pl->width * camera->pl->height / 64) * 64];
	int shuffle;

	shuffle = (long long)rand_r(&camera->pl->rand_seed) *
		   camera->pl->no_shuffles / (RAND_MAX + 1ll);

	return st2205_real_write_file (camera, filename, rgb24, buf,
				       shuffle, 1);
}

int
st2205_delete_file(Camera *camera, int idx)
{
	uint8_t c = 0;
	int i, present, count, new_count = 0;

	count = st2205_read_file_count(camera);
	if (count < 0) return count;

	if (idx >= count) {
		gp_log (GP_LOG_ERROR, "st2205",
			"delete file beyond end of FAT");
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Calculate new file count after the delete operation */
	for (i = 0; i < count; i++) {
		if (i == idx)
			continue;

		present = st2205_file_present (camera, i);
		if (present < 0) return present;
		if (present)
			new_count = i + 1;
	}

	CHECK (st2205_write_mem (camera, ST2205_FILE_OFFSET (idx), &c, 1))
	CHECK (st2205_write_file_count (camera, new_count))
	CHECK (st2205_update_fat_checksum (camera))
	CHECK (st2205_copy_fat (camera))

	return GP_OK;
}

int
st2205_delete_all(Camera *camera)
{
	CHECK (st2205_check_block_present(camera, 0))
	memset (camera->pl->mem + ST2205_FILE_OFFSET (0), 0,
		ST2205_FAT_SIZE - ST2205_FILE_OFFSET (0));
	/* Mark the memory block we've directly manipulated dirty. */
	camera->pl->block_dirty[0] = 1;

	CHECK (st2205_update_fat_checksum (camera))
	CHECK (st2205_copy_fat (camera))

	return GP_OK;
}

int
st2205_commit(Camera *camera)
{
	int i, j;
	int mem_block_size = (camera->pl->mem_size - camera->pl->firmware_size)
				/ ST2205_BLOCK_SIZE;
	int erase_block_size = ST2205_ERASE_BLOCK_SIZE / ST2205_BLOCK_SIZE;

	for (i = 0; i < mem_block_size; i += erase_block_size) {
		for (j = 0; j < erase_block_size; j++)
			if (camera->pl->block_dirty[i + j])
				break;

		/* If we have no dirty blocks in this erase block continue */
		if (j == erase_block_size)
			continue;

		/* Re-write all the data blocks in this erase block! */
		for (j = 0; j < erase_block_size; j++) {
			CHECK (st2205_write_block (camera, i + j,
						   camera->pl->mem +
						   (i + j) *
						   ST2205_BLOCK_SIZE))
			camera->pl->block_dirty[i + j] = 0;
		}
	}
	return GP_OK;
}

static int
st2205_init(Camera *camera)
{
	uint16_t *lookup_src, *dest;
	uint8_t *shuffle_src;
	int x, y, i, j, shuffle_size, checksum, expected_checksum;
	const struct {
		int width, height, no_tables, usable_tables;
		unsigned char unknown3[8];
		int checksum;
	} shuffle_info[] = {
		{ 128, 160, 8, 7, /* Last shuffle table does not work ?? */
		  { 0xff, 0xff, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02 },
		  0x0003fc00 },
		{ 128, 128, 7, 7,
		  { 0xff, 0xff, 0x01, 0x01, 0x01, 0x01, 0x01 },
		  0x00025800 },
		{ 120, 160, 7, 7,
		  /* FIXME unknown3 needs to be verified */
		  { 0xff, 0xff, 0x01, 0x01, 0x01, 0x01, 0x01 },
		  0x00030570 },
		{ 96, 64, 7, 7,
		  { 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00 },
		  0x00008700 },
		{ 0, 0, 0 }
	};
	const struct {
		int lookup_offset;
		int firmware_size;
		int picture_start;
		int no_fats;
	} version_info[] = {
		{ ST2205_V1_LOOKUP_OFFSET, ST2205_V1_FIRMWARE_SIZE,
		  ST2205_V1_PICTURE_START, 4 },
		{ ST2205_V2_LOOKUP_OFFSET, ST2205_V2_FIRMWARE_SIZE,
		  ST2205_V2_PICTURE_START, 1 },
		{ }
	};

	GP_DEBUG ("st2205_init called");

	CHECK (st2205_detect_mem_size(camera))

	if ((camera->pl->width % 8) || (camera->pl->height % 8)) {
		gp_log (GP_LOG_ERROR, "st2205",
			"lcd width and height must be a multiple of 8");
		return GP_ERROR_IO;
	}

	shuffle_size = (camera->pl->width / 8) * (camera->pl->height / 8);
	if (shuffle_size > ST2205_SHUFFLE_SIZE) {
		gp_log (GP_LOG_ERROR, "st2205",
			"shuffle table size too small!");
		return GP_ERROR_FIXED_LIMIT_EXCEEDED;
	}

	camera->pl->mem = st2205_malloc_page_aligned(camera->pl->mem_size);
	if (!camera->pl->mem)
		return GP_ERROR_NO_MEMORY;

	/* Get the lookup tables from the device */
	for (i = 0; version_info[i].lookup_offset; i++) {
		int lookup_offset = version_info[i].lookup_offset;
		checksum = 0;

		if ((lookup_offset + ST2205_LOOKUP_SIZE) >
		    camera->pl->mem_size)
			continue;

		CHECK (st2205_check_block_present(camera,
					lookup_offset / ST2205_BLOCK_SIZE))

		lookup_src = (uint16_t *)(camera->pl->mem + lookup_offset);
		dest = (uint16_t *)(&camera->pl->lookup[0][0][0]);
		for (j = 0; j < 3 * 256 * 8; j++) {
			*dest++ = le16toh(*lookup_src++);
			checksum += (uint8_t)
				(camera->pl->mem + lookup_offset)[j * 2];
			checksum += (uint8_t)
				(camera->pl->mem + lookup_offset)[j * 2 + 1];
		}

		if (checksum == ST2205_LOOKUP_CHECKSUM)
			break;
	}

	if (!version_info[i].lookup_offset) {
		gp_log (GP_LOG_ERROR, "st2205", "lookup tables not found");
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	camera->pl->firmware_size = version_info[i].firmware_size;
	camera->pl->picture_start = version_info[i].picture_start;
	camera->pl->no_fats	  = version_info[i].no_fats;

	/* Generate shuffle tables 0 and 1 */
	for (y = 0, i = 0; y < camera->pl->height; y += 8)
		for (x = 0; x < camera->pl->width; x += 8, i++) {
			camera->pl->shuffle[0][i].x = x;
			camera->pl->shuffle[0][i].y = y;
		}

	for (x = 0, i = 0; x < camera->pl->width; x += 8)
		for (y = 0; y < camera->pl->height; y += 8, i++) {
			camera->pl->shuffle[1][i].x = x;
			camera->pl->shuffle[1][i].y = y;
		}

	/* Get the other shuffle tables from the device (they start directly
	   after the chroma lookup table) */
	shuffle_src = (uint8_t *)lookup_src;

	/* Skip to the tables for the right resolution */
	for (i = 0; shuffle_info[i].no_tables; i++) {
		if (camera->pl->width	== shuffle_info[i].width &&
				camera->pl->height == shuffle_info[i].height)
			break;
		shuffle_src += (shuffle_info[i].width *
				shuffle_info[i].height * 2 / 64) *
			       (shuffle_info[i].no_tables - 2);
	}
	if (!shuffle_info[i].no_tables) {
		gp_log (GP_LOG_ERROR, "st2205",
			"unknown display resolution: %dx%d",
			camera->pl->width, camera->pl->height);
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	memcpy (camera->pl->unknown3, shuffle_info[i].unknown3,
		sizeof(camera->pl->unknown3));
	camera->pl->no_shuffles = shuffle_info[i].usable_tables;
	expected_checksum = shuffle_info[i].checksum;
	checksum = 0;
	for (j = 2; j < camera->pl->no_shuffles; j++)
		for (i = 0; i < shuffle_size; i++) {
			camera->pl->shuffle[j][i].x = *shuffle_src++;
			camera->pl->shuffle[j][i].y = *shuffle_src++;
			if (camera->pl->shuffle[j][i].x >= camera->pl->width ||
			    camera->pl->shuffle[j][i].y >= camera->pl->height){
				gp_log (GP_LOG_ERROR, "st2205",
				      "shuffle table coordinate out of range");
				return GP_ERROR_CORRUPTED_DATA;
			}
			checksum += camera->pl->shuffle[j][i].x;
			checksum += camera->pl->shuffle[j][i].y;
		}

	if (checksum != expected_checksum) {
		gp_log (GP_LOG_ERROR, "st2205",
			"shuffle table checksum mismatch");
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	CHECK (st2205_check_fat_checksum (camera))

	camera->pl->rand_seed = time(NULL);

	return GP_OK;
}

static void
st2205_exit(Camera *camera)
{
	st2205_free_page_aligned(camera->pl->mem, camera->pl->mem_size);
	camera->pl->mem = NULL;
}

int
st2205_open_device(Camera *camera)
{
	camera->pl->buf = st2205_malloc_page_aligned(512);
	if (!camera->pl->buf)
		return GP_ERROR_NO_MEMORY;

	/* Check this is a Sitronix frame */
	CHECK (gp_port_seek (camera->port, 0, SEEK_SET))
	if (gp_port_read (camera->port, camera->pl->buf, 512) != 512)
		return GP_ERROR_IO_READ;
	if (strcmp (camera->pl->buf, "SITRONIX CORP."))
		return GP_ERROR_MODEL_NOT_FOUND;

	/* Read LCD size from the device */
	CHECK (st2205_send_command (camera, 5, 0 ,0))

	if (gp_port_seek (camera->port, ST2205_READ_OFFSET, SEEK_SET) !=
	    ST2205_READ_OFFSET)
		return GP_ERROR_IO;

	if (gp_port_read (camera->port, camera->pl->buf, 512) != 512)
		return GP_ERROR_IO_READ;

	camera->pl->width  = be16atoh ((uint8_t *)camera->pl->buf);
	camera->pl->height = be16atoh ((uint8_t *)camera->pl->buf + 2);

	GP_DEBUG ("Sitronix picframe of %dx%d detected.",
		  camera->pl->width, camera->pl->height);

	return st2205_init (camera);
}

int
st2205_open_dump(Camera *camera, const char *dump,
		 int width, int height)
{
	camera->pl->mem_dump = fopen(dump, "r+");
	if (!camera->pl->mem_dump) {
		gp_log (GP_LOG_ERROR, "st2205", "opening memdump file: %s: %s",
			dump, strerror(errno));
		return GP_ERROR_IO_INIT;
	}

	camera->pl->width  = width;
	camera->pl->height = height;

	return st2205_init (camera);
}

void st2205_close(Camera *camera)
{
	st2205_exit (camera);
	if (camera->pl->mem_dump) {
		fclose (camera->pl->mem_dump);
		camera->pl->mem_dump = NULL;
	}
	st2205_free_page_aligned(camera->pl->buf, 512);
	camera->pl->buf = NULL;
}

int
st2205_get_mem_size(Camera *camera)
{
	return camera->pl->mem_size;
}
