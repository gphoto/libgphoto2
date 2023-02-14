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
#define _DEFAULT_SOURCE

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_LIBGD
#include <gd.h>
#endif

#include <gphoto2/gphoto2-result.h>
#include "tp6801.h"

static int
tp6801_send_cmd(Camera *camera, int to_dev, unsigned char cmd, int offset,
	char *data, int data_size)
{
	char cmd_buffer[16];
	char sense_buffer[32];

	/* The device firmware does not seem to wait for the last cmd to
	   finish when going from PP to READ, do this for it */
	if (camera->pl->last_cmd == TP6801_PROGRAM_PAGE &&
	    cmd == TP6801_READ) {
		usleep(5000); /* Max page program time for most spi flash */
	}
	camera->pl->last_cmd = cmd;

	memset (cmd_buffer, 0, sizeof (cmd_buffer));
	cmd_buffer[0] = cmd;
	cmd_buffer[1] = 0x11;
	cmd_buffer[2] = 0x31;
	cmd_buffer[3] = 0x0f;
	cmd_buffer[4] = 0x30;
	cmd_buffer[5] = 0x01;
	cmd_buffer[6] = (data_size >> 8) & 0xff;
	cmd_buffer[7] = data_size & 0xff;
	cmd_buffer[8] = (offset >> 16) & 0xff;
	cmd_buffer[9] = (offset >> 8) & 0xff;
	cmd_buffer[10] = offset & 0xff;

	return gp_port_send_scsi_cmd (camera->port, to_dev,
		cmd_buffer, sizeof(cmd_buffer),
		sense_buffer, sizeof(sense_buffer),
		data, data_size);
}

int
tp6801_set_time_and_date(Camera *camera, struct tm *t)
{
	char cmd_buffer[16];
	char sense_buffer[32];

	memset (cmd_buffer, 0, sizeof (cmd_buffer));

	cmd_buffer[0] = TP6801_SET_TIME;
	cmd_buffer[1] = 0x11;
	cmd_buffer[2] = 0x31;
	cmd_buffer[3] = 0x0f;
	cmd_buffer[4] = 0x30;
	cmd_buffer[5] = 0x01;
	cmd_buffer[6] = t->tm_hour;
	cmd_buffer[7] = t->tm_min;
	cmd_buffer[8] = t->tm_sec;
	cmd_buffer[9] = t->tm_year % 100;
	cmd_buffer[10] = t->tm_mon + 1;
	cmd_buffer[11]  = t->tm_mday;

	return gp_port_send_scsi_cmd (camera->port, 0,
		cmd_buffer, sizeof(cmd_buffer),
		sense_buffer, sizeof(sense_buffer),
		NULL, 0);
}

static int
tp6801_read(Camera *camera, int offset, char *buf, int size)
{
	int ret;
	if (camera->pl->mem_dump) {
		ret = fseek (camera->pl->mem_dump, offset, SEEK_SET);
		if (ret) {
			gp_log (GP_LOG_ERROR, "tp6801",
				"seeking in memdump: %s", strerror(errno));
			return GP_ERROR_IO_READ;
		}
		ret = fread (buf, 1, size, camera->pl->mem_dump);
		if (ret != size) {
			if (ret < 0)
				gp_log (GP_LOG_ERROR, "tp6801",
					"reading memdump: %s",
					strerror(errno));
			else
				gp_log (GP_LOG_ERROR, "tp6801",
					"short read reading from memdump");
			return GP_ERROR_IO_READ;
		}
	} else {
		CHECK (tp6801_send_cmd (camera, 0, TP6801_READ, offset,
					buf, size))
	}
	return GP_OK;
}

static int
tp6801_program_page(Camera *camera, int offset, char *buf)
{
	int ret;

	if (camera->pl->mem_dump) {
		ret = fseek (camera->pl->mem_dump, offset, SEEK_SET);
		if (ret) {
			gp_log (GP_LOG_ERROR, "tp6801",
				"seeking in memdump: %s", strerror(errno));
			return GP_ERROR_IO_WRITE;
		}
		ret = fwrite (buf, 1, TP6801_PAGE_SIZE, camera->pl->mem_dump);
		if (ret != TP6801_PAGE_SIZE) {
			gp_log (GP_LOG_ERROR, "tp6801",
				"writing memdump: %s", strerror(errno));
			return GP_ERROR_IO_WRITE;
		}
	} else {
		CHECK (tp6801_send_cmd (camera, 1, TP6801_PROGRAM_PAGE,
					offset, buf, TP6801_PAGE_SIZE))
	}
	return GP_OK;
}

static int
tp6801_erase_block(Camera *camera, int offset)
{
	int ret;
	char *buf;

	if (camera->pl->mem_dump) {
		buf = camera->pl->mem + offset;
		memset(buf, 0xff, TP6801_BLOCK_SIZE);
		ret = fseek (camera->pl->mem_dump, offset, SEEK_SET);
		if (ret) {
			gp_log (GP_LOG_ERROR, "tp6801",
				"seeking in memdump: %s", strerror(errno));
			return GP_ERROR_IO_WRITE;
		}
		ret = fwrite (buf, 1, TP6801_BLOCK_SIZE, camera->pl->mem_dump);
		if (ret != TP6801_BLOCK_SIZE) {
			gp_log (GP_LOG_ERROR, "tp6801",
				"writing memdump: %s", strerror(errno));
			return GP_ERROR_IO_WRITE;
		}
		return GP_OK;
	} else {
		CHECK (tp6801_send_cmd (camera, 0, TP6801_ERASE_BLOCK, offset,
					NULL, 0))
	}

	return GP_OK;
}

static int
tp6801_check_offset_len(Camera *camera, int offset, int len)
{
	if (offset < 0 || len < 0) {
		gp_log (GP_LOG_ERROR, "tp6801", "negative offset or len");
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (offset + len > camera->pl->mem_size) {
		gp_log (GP_LOG_ERROR, "tp6801", "access beyond end of memory");
		return GP_ERROR_CORRUPTED_DATA;
	}
	return GP_OK;
}

static int
tp6801_read_mem(Camera *camera, int offset, int len)
{
	int i, to_read, page = offset / TP6801_PAGE_SIZE;

	CHECK (tp6801_check_offset_len (camera, offset, len))

	/* Adjust len for us reading 1 page at a time */
	len += offset % TP6801_PAGE_SIZE;

	while (len > 0) {
		/* Skip already read pages */
		if (camera->pl->page_state[page] & TP6801_PAGE_READ) {
			len -= TP6801_PAGE_SIZE;
			page++;
			continue;
		}

		/* Try to read as much as possible in one go */
		to_read = 0;
		while (len > 0 && to_read < TP6801_MAX_READ &&
				!(camera->pl->page_state[page + to_read] &
							TP6801_PAGE_READ)) {
			len -= TP6801_PAGE_SIZE;
			to_read++;
		}

		offset = page * TP6801_PAGE_SIZE;
		CHECK (tp6801_read (camera, offset, camera->pl->mem + offset,
				    to_read * TP6801_PAGE_SIZE))
		for (i = 0; i < to_read; i++, page++)
			camera->pl->page_state[page] |= TP6801_PAGE_READ;
	}
	return GP_OK;
}

static int
tp6801_write_mem(Camera *camera, int offset,
	void *buf, int len)
{
	int i, first, last;

	CHECK (tp6801_check_offset_len (camera, offset, len))

	first = offset / TP6801_PAGE_SIZE;
	last  = (offset + len - 1) / TP6801_PAGE_SIZE;

	/* If we're not going to completely fill the first page, first read
	   it to ensure it is present in our device memory copy */
	if ((offset % TP6801_PAGE_SIZE || len < TP6801_PAGE_SIZE) &&
	    (camera->pl->page_state[first] & TP6801_PAGE_CONTAINS_DATA) &&
	    !(camera->pl->page_state[first] & TP6801_PAGE_READ)) {
		int o = first * TP6801_PAGE_SIZE;
		CHECK (tp6801_read (camera, o, camera->pl->mem + o,
				    TP6801_PAGE_SIZE))
		camera->pl->page_state[first] |= TP6801_PAGE_READ;
	}

	/* Likewise for the last page */
	if ((offset + len) % TP6801_PAGE_SIZE &&
	    (camera->pl->page_state[last] & TP6801_PAGE_CONTAINS_DATA) &&
	    !(camera->pl->page_state[last] & TP6801_PAGE_READ)) {
		int o = last * TP6801_PAGE_SIZE;
		CHECK (tp6801_read (camera, o, camera->pl->mem + o,
				    TP6801_PAGE_SIZE))
		camera->pl->page_state[last] |= TP6801_PAGE_READ;
	}

	memcpy(camera->pl->mem + offset, buf, len);
	for (i = first; i <= last; i++) {
		/* Note we mark the written pages as read too, to avoid
		   them getting overwritten by a read from the device later */
		camera->pl->page_state[i] |= TP6801_PAGE_DIRTY |
					     TP6801_PAGE_CONTAINS_DATA |
					     TP6801_PAGE_READ;
	}
	return GP_OK;
}

static int
tp6801_detect_mem(Camera *camera)
{
	int i;
	char *m;

	camera->pl->mem = malloc(TP6801_MAX_MEM_SIZE);
	if (!camera->pl->mem)
		return GP_ERROR_NO_MEMORY;
	camera->pl->mem_size = TP6801_MAX_MEM_SIZE;

	/* Note we read the PAT instead of some mem at offset 0, because:
	   1) This saves a read when reading the PAT later
	   2) The PAT contains reasonably unique data */
	CHECK (tp6801_read_mem (camera, TP6801_PAT_OFFSET, TP6801_PAT_SIZE))

	for (i = 0; (1048576 << i) != TP6801_MAX_MEM_SIZE; i++) {
		int offset = (1048576 << i) + TP6801_PAT_OFFSET;
		CHECK (tp6801_read_mem (camera, offset, TP6801_PAT_SIZE))
		if (memcmp(camera->pl->mem + TP6801_PAT_OFFSET,
			   camera->pl->mem + offset,
			   TP6801_PAT_SIZE) == 0)
			break;
	}

	camera->pl->mem_size = 1048576 << i;
	GP_DEBUG ("tp6801 detected %d bytes of memory", camera->pl->mem_size);

	m = realloc(camera->pl->mem, camera->pl->mem_size);
	if (!m)
		return GP_ERROR_NO_MEMORY;

	camera->pl->mem = m;
	return GP_OK;
}

int
tp6801_filesize(Camera *camera)
{
	/* The pictures are stored as rgb565be, so 2 bytes / pixel */
	return camera->pl->width * camera->pl->height * 2;
}

int
tp6801_max_filecount(Camera *camera)
{
	int free_mem, size;

	size = tp6801_filesize (camera);
	free_mem = camera->pl->mem_size - TP6801_PICTURE_OFFSET(0, size);
	free_mem -= TP6801_CONST_DATA_SIZE;

	return free_mem / size;
}

int
tp6801_file_present(Camera *camera, int idx)
{
	if (idx < 0) {
		gp_log (GP_LOG_ERROR, "tp6801",
			"file index < 0");
		return GP_ERROR_BAD_PARAMETERS;
	}

	if (idx >= tp6801_max_filecount (camera)) {
		gp_log (GP_LOG_ERROR, "tp6801",
			"file index beyond end of ABFS");
		return GP_ERROR_BAD_PARAMETERS;
	}

	if (camera->pl->pat[idx] >= 1 &&
	    camera->pl->pat[idx] <= camera->pl->picture_count)
		return 1;
	else switch (camera->pl->pat[idx]) {
	case TP6801_PAT_ENTRY_DELETED_FRAME:
	case TP6801_PAT_ENTRY_DELETED_WIN:
	case TP6801_PAT_ENTRY_PRE_ERASED:
		return 0;
	default:
		return GP_ERROR_CORRUPTED_DATA;
	}
}

static int
tp6801_check_file_present(Camera *camera, int idx)
{
	int r = tp6801_file_present (camera, idx);
	if (r < 0)
		return r;
	if (r == 0)
		return GP_ERROR_BAD_PARAMETERS;
	return GP_OK;
}

static int
tp6801_decode_image(Camera *camera, char *_src, int **dest)
{
#ifdef HAVE_LIBGD
	int x, y;
	unsigned char *src = (unsigned char *)_src;

	for (y = 0; y < camera->pl->height; y++) {
		for (x = 0; x < camera->pl->width; x++) {
			int r, g, b, rgb565;

			rgb565 = (src[0] << 8) | src[1];
			r = (rgb565 & 0xf800) >> 8;
			g = (rgb565 & 0x07e0) >> 3;
			b = (rgb565 & 0x001f) << 3;

			dest[y][x] = gdTrueColor (r, g, b);
			src += 2;
		}
	}
	return GP_OK;
#else
	gp_log(GP_LOG_ERROR,"tp6801", "GD decompression not supported - no libGD present during build");
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;
#endif
}

static int
tp6801_encode_image(Camera *camera, int **src, char *dest)
{
#ifdef HAVE_LIBGD
	int x, y;

	for (y = 0; y < camera->pl->height; y++) {
		for (x = 0; x < camera->pl->width; x++) {
			int r, g, b, rgb565;
			int p = src[y][x];

			r = gdTrueColorGetRed(p);
			g = gdTrueColorGetGreen(p);
			b = gdTrueColorGetBlue(p);
			rgb565 = ((r & 0xf8) << 8) |
				 ((g & 0xfc) << 3) |
				 ((b & 0xf8) >> 3);
			dest[0] = (rgb565 >> 8) & 0xff;
			dest[1] = (rgb565 >> 0) & 0xff;
			dest += 2;
		}
	}
	return GP_OK;
#else
	gp_log(GP_LOG_ERROR,"tp6801", "GD compression not supported - no libGD present during build");
	return GP_ERROR_NOT_SUPPORTED;
#endif
}

int
tp6801_read_raw_file(Camera *camera, int idx, char **raw)
{
	int size;

	*raw = NULL;
	size = tp6801_filesize (camera);

	CHECK (tp6801_check_file_present (camera, idx))
	CHECK (tp6801_read_mem (camera, TP6801_PICTURE_OFFSET(idx, size),
				size))

	*raw = malloc (size);
	if (!*raw) {
		gp_log (GP_LOG_ERROR, "tp6801", "allocating memory");
		return GP_ERROR_NO_MEMORY;
	}

	memcpy(*raw, camera->pl->mem + TP6801_PICTURE_OFFSET(idx, size), size);
	return GP_OK;
}

int
tp6801_read_file(Camera *camera, int idx, int **rgb24)
{
	int size = tp6801_filesize (camera);

	CHECK (tp6801_check_file_present (camera, idx))
	CHECK (tp6801_read_mem (camera, TP6801_PICTURE_OFFSET(idx, size),
				size))
	CHECK (tp6801_decode_image (camera,
			camera->pl->mem + TP6801_PICTURE_OFFSET(idx, size),
			rgb24))
	return GP_OK;
}

int
tp6801_write_file(Camera *camera, int **rgb24)
{
	const int size = tp6801_filesize (camera);
	int i, count = tp6801_max_filecount (camera);
	char buf[size];

	/* Pass 1 try to find a pre-erased slot in the PAT */
	for (i = 0; i < count; i++)
		if (camera->pl->pat[i] == TP6801_PAT_ENTRY_PRE_ERASED)
			break;

	if (i == count) {
		/* Pass 2 try to find a deleted slot in the PAT */
		for (i = 0; i < count; i++)
			if (TP6801_PAT_ENTRY_DELETED(camera->pl->pat[i]))
				break;
	}

	if (i == count) {
		gp_log (GP_LOG_ERROR, "tp6801",
			"not enough freespace to add file");
		return GP_ERROR_NO_SPACE;
	}

	CHECK (tp6801_encode_image (camera, rgb24, buf))
	CHECK (tp6801_write_mem (camera, TP6801_PICTURE_OFFSET(i, size),
				 buf, size))
	camera->pl->picture_count++;
	camera->pl->pat[i] = camera->pl->picture_count;
	camera->pl->page_state[TP6801_PAT_PAGE] |= TP6801_PAGE_DIRTY;

	return GP_OK;
}

int
tp6801_delete_file(Camera *camera, int idx)
{
	CHECK (tp6801_check_file_present (camera, idx))

	camera->pl->pat[idx] = TP6801_PAT_ENTRY_DELETED_WIN;
	camera->pl->page_state[TP6801_PAT_PAGE] |= TP6801_PAGE_DIRTY;

	return GP_OK;
}

int
tp6801_delete_all(Camera *camera)
{
	int i, start, end;

	/* Erase the entire picture memory */
	start = TP6801_PICTURE_OFFSET(0, 0);
	end   = camera->pl->mem_size - TP6801_CONST_DATA_SIZE;
	for (i = start; i < end; i += TP6801_BLOCK_SIZE)
		CHECK (tp6801_erase_block (camera, i))

	/* Update state of all picture memory pages */
	start /= TP6801_PAGE_SIZE;
	end   /= TP6801_PAGE_SIZE;
	for (i = start; i < end; i++)
		camera->pl->page_state[i] = 0;

	/* Update PAT */
	end = tp6801_max_filecount (camera);
	for (i = 0; i < end; i++)
		camera->pl->pat[i] = TP6801_PAT_ENTRY_PRE_ERASED;
	camera->pl->picture_count = 0;
	camera->pl->page_state[TP6801_PAT_PAGE] |= TP6801_PAGE_DIRTY;

	return GP_OK;
}

/* bsp = block start page */
static int
tp6801_program_block(Camera *camera, int bsp, char prog_flags)
{
	const int block_page_size = TP6801_BLOCK_SIZE / TP6801_PAGE_SIZE;
	int i, offset;

	for (i = 0, offset = bsp * TP6801_PAGE_SIZE;
	     i < block_page_size;
	     i++, offset += TP6801_PAGE_SIZE) {
		if (!(camera->pl->page_state[bsp + i] & prog_flags))
			continue;

		CHECK (tp6801_program_page (camera, offset,
					    camera->pl->mem + offset))
		camera->pl->page_state[bsp + i] &= ~TP6801_PAGE_DIRTY;
		camera->pl->page_state[bsp + i] |= TP6801_PAGE_NEEDS_ERASE;
	}
	return GP_OK;
}

static int
tp6801_read_erase_program_block(Camera *camera, int bsp)
{
	const int block_page_size = TP6801_BLOCK_SIZE / TP6801_PAGE_SIZE;
	int i, offset, to_read;

	/* Step 1 read in any pages which contain data before erasing! */
	i = 0;
	while (i < block_page_size) {
		/* Skip unused pages */
		if (!(camera->pl->page_state[bsp + i] &
				TP6801_PAGE_CONTAINS_DATA)) {
			i++;
			continue;
		}

		/* Read as many pages in one go as possible */
		to_read = 0;
		while ((i + to_read) < block_page_size &&
		       (camera->pl->page_state[bsp + i + to_read] &
					TP6801_PAGE_CONTAINS_DATA)) {
			to_read++;
		}
		offset = (bsp + i) * TP6801_PAGE_SIZE;
		CHECK (tp6801_read_mem (camera, offset,
					to_read * TP6801_PAGE_SIZE))
		i += to_read;
	}

	/* Step 2 erase the block */
	CHECK (tp6801_erase_block (camera, bsp * TP6801_PAGE_SIZE))
	for (i = 0; i < block_page_size; i++) {
		camera->pl->page_state[bsp + i] &=
			~TP6801_PAGE_NEEDS_ERASE;
	}

	/* Step 3 program all pages which are dirty or contain data */
	CHECK (tp6801_program_block (camera, bsp,
			TP6801_PAGE_DIRTY | TP6801_PAGE_CONTAINS_DATA))

	return GP_OK;
}

static int
tp6801_commit_block(Camera *camera, int bsp)
{
	const int block_page_size = TP6801_BLOCK_SIZE / TP6801_PAGE_SIZE;
	int i, dirty_pages = 0, needs_erase = 0;

	for (i = 0; i < block_page_size; i++) {
		char page_state = camera->pl->page_state[bsp + i];
		if (page_state & TP6801_PAGE_DIRTY) {
			dirty_pages++;
			if (page_state & TP6801_PAGE_NEEDS_ERASE)
				needs_erase++;
		}
	}

	if (!dirty_pages)
		return GP_OK;

	if (needs_erase)
		CHECK (tp6801_read_erase_program_block (camera, bsp))
	else
		/* No erase needed, just program dirty pages */
		CHECK (tp6801_program_block (camera, bsp,
					     TP6801_PAGE_DIRTY))

	return GP_OK;
}

int
tp6801_commit(Camera *camera)
{
	const int size = tp6801_filesize (camera);
	const int block_page_size = TP6801_BLOCK_SIZE / TP6801_PAGE_SIZE;
	int mem_page_size = camera->pl->mem_size / TP6801_PAGE_SIZE;
	int i, j, begin, end, count = tp6801_max_filecount (camera);

	/* Skip the first block as that contains the PAT */
	for (i = block_page_size;
	     i < mem_page_size;
	     i += block_page_size) {
		CHECK (tp6801_commit_block (camera, i))
	}

	/* Now see if we've completely erased the area of some deleted files
	   and if so mark there PAT entries as pre-erased, rather then just
	   deleted */
	for (i = 0; i < count; i++) {
		if (!TP6801_PAT_ENTRY_DELETED(camera->pl->pat[i]))
			continue;

		begin = TP6801_PICTURE_OFFSET(i, size) / TP6801_PAGE_SIZE;
		end   = TP6801_PICTURE_OFFSET(i + 1, size) / TP6801_PAGE_SIZE;
		for (j = begin; j < end; j++) {
			char page_state = camera->pl->page_state[j];
			if (page_state & TP6801_PAGE_NEEDS_ERASE)
				break;
		}
		if (j == end) { /* No page needs erase -> mark pre-erased */
			camera->pl->pat[i] = TP6801_PAT_ENTRY_PRE_ERASED;
			camera->pl->page_state[TP6801_PAT_PAGE] |=
				TP6801_PAGE_DIRTY;
		}
	}

	/* The picture numbering in the PAT can contain holes from us
	   (or the frame which is why wo do this here) deleting pictures.
	   These holes are a problem as if we keep deleting all but the
	   highest numbered picture and adding new pictures the picture
	   number could reach 254 / 255 which have special meaning.

	   So we renumber the pictures here to remove the holes */
	for (i = 1; i <= camera->pl->picture_count; i++) {
		/* Step 1 see if this number exists in the PAT */
		for (j = 0; j < count; j++)
			if (camera->pl->pat[j] == i)
				break;
		if (j != count)
			continue; /* Number exists no renumber needed */

		/* Step 2 decr. the number of all higher numbered picts */
		for (j = 0; j < count; j++) {
			if (camera->pl->pat[j] >= 1 &&
			    camera->pl->pat[j] <= camera->pl->picture_count &&
			    camera->pl->pat[j] > i) {
				camera->pl->pat[j]--;
			}
		}
		camera->pl->picture_count--;
		camera->pl->page_state[TP6801_PAT_PAGE] |= TP6801_PAGE_DIRTY;
	}

	/* And commit the block with the PAT */
	CHECK (tp6801_commit_block (camera, 0))

	return GP_OK;
}

static const struct tp6801_model_info {
	int vid;
	int pid;
	char model[TP6801_SCSI_MODEL_LEN + 1];
	int width;
	int height;
} tp6801_models[] = {
	{ 0x0168, 0x3011, "InsigniaNS-KEYXX09", 160, 128 }, /* Guessed */
	{ 0x0168, 0x3011, "InsigniaNS-KEYXX10", 160, 128 },
	{}
};

int
tp6801_open_device(Camera *camera)
{
	int i, j, count, size, begin, end, offset, vid, pid;
	char model[TP6801_SCSI_MODEL_LEN + 1];

	CHECK (tp6801_detect_mem (camera))

	/* Read the first 512 bytes of the const data area, this gives us
	   the usb id, scsi model and begin of the iso img */
	offset = camera->pl->mem_size - TP6801_CONST_DATA_SIZE;
	CHECK (tp6801_read_mem (camera, offset, 512))

	/* Sanity check, verify that the iso starts where we expect it, if it
	   does not we likely have the size of the const data wrong for this
	   model, and could overwrite part of the iso, not good! */
	offset += TP6801_ISO_OFFSET;
	if (memcmp (camera->pl->mem + offset, "\001CD001", 6)) {
		gp_log (GP_LOG_ERROR, "tp6801", "Could not find ISO header");
		return GP_ERROR_MODEL_NOT_FOUND;
	}
	offset -= TP6801_ISO_OFFSET;

	/* The 4st 4 bytes of the const data are pid:vid */
	pid = (camera->pl->mem[offset + 0] << 8) | camera->pl->mem[offset + 1];
	vid = (camera->pl->mem[offset + 2] << 8) | camera->pl->mem[offset + 3];

	offset += TP6801_SCSI_MODEL_OFFSET;
	CHECK (tp6801_read_mem (camera, offset, TP6801_SCSI_MODEL_LEN))
	memcpy (model, camera->pl->mem + offset, TP6801_SCSI_MODEL_LEN);
	model[TP6801_SCSI_MODEL_LEN] = 0;

	for (i = 0; tp6801_models[i].pid; i++) {
		if (tp6801_models[i].pid == pid &&
		    tp6801_models[i].vid == vid &&
		    strcmp(tp6801_models[i].model, model) == 0)
			break;
	}
	if (!tp6801_models[i].pid) {
		gp_log (GP_LOG_ERROR, "tp6801",
			"unknown model %04x:%04x %s", vid, pid, model);
		return GP_ERROR_MODEL_NOT_FOUND;
	}
	camera->pl->width  = tp6801_models[i].width;
	camera->pl->height = tp6801_models[i].height;
	GP_DEBUG("tp6801 detect %s model (%dx%d)", model,
		 camera->pl->width, camera->pl->height);

	/* One more sanity check */
	size = tp6801_filesize (camera);
	if (size % TP6801_PAGE_SIZE) {
		gp_log (GP_LOG_ERROR, "tp6801", "image size not page aligned");
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	/* Read PAT, verify signature */
	CHECK (tp6801_read_mem (camera, TP6801_PAT_OFFSET, TP6801_PAT_SIZE))
	if (memcmp (camera->pl->mem + TP6801_PAT_MAGIC_OFFSET,
		    TP6801_PAT_MAGIC, strlen(TP6801_PAT_MAGIC))) {
		gp_log (GP_LOG_ERROR, "tp6801", "invalid pat magic");
		return GP_ERROR_MODEL_NOT_FOUND;
	}
	camera->pl->pat = (unsigned char *)camera->pl->mem + TP6801_PAT_OFFSET;

	/* Set initial page state for all pages */
	for (i = 0; i < camera->pl->mem_size / TP6801_PAGE_SIZE; i++)
		camera->pl->page_state[i] |= TP6801_PAGE_CONTAINS_DATA |
					       TP6801_PAGE_NEEDS_ERASE;

	/* Parse and verify PAT and update page_state based on it */
	count = tp6801_max_filecount(camera);
	for (i = 0; i < count; i++) {
		int clear_flags = 0;

		if (camera->pl->pat[i] >= 1 &&
		    camera->pl->pat[i] <= count) {
			if (camera->pl->pat[i] > camera->pl->picture_count)
				camera->pl->picture_count = camera->pl->pat[i];
			continue;
		} else switch (camera->pl->pat[i]) {
		case TP6801_PAT_ENTRY_PRE_ERASED:
			clear_flags |= TP6801_PAGE_NEEDS_ERASE;
			/* fall through */
		case TP6801_PAT_ENTRY_DELETED_FRAME:
		case TP6801_PAT_ENTRY_DELETED_WIN:
			clear_flags |= TP6801_PAGE_CONTAINS_DATA;
			break;
		default:
			gp_log (GP_LOG_ERROR, "tp6801", "invalid pat entry");
			return GP_ERROR_CORRUPTED_DATA;
		}

		begin = TP6801_PICTURE_OFFSET(i, size) / TP6801_PAGE_SIZE;
		end   = TP6801_PICTURE_OFFSET(i + 1, size) / TP6801_PAGE_SIZE;
		for (j = begin; j < end; j++)
			camera->pl->page_state[j] &= ~clear_flags;
	}

	return GP_OK;
}

int
tp6801_open_dump(Camera *camera, const char *dump)
{
	camera->pl->mem_dump = fopen(dump, "r+");
	if (!camera->pl->mem_dump) {
		gp_log (GP_LOG_ERROR, "tp6801", "opening memdump file: %s: %s",
			dump, strerror(errno));
		return GP_ERROR_IO_INIT;
	}

	return tp6801_open_device (camera);
}

void tp6801_close(Camera *camera)
{
	free (camera->pl->mem);
	camera->pl->mem = NULL;
	if (camera->pl->mem_dump) {
		fclose (camera->pl->mem_dump);
		camera->pl->mem_dump = NULL;
	}
}

int
tp6801_get_mem_size(Camera *camera)
{
	return camera->pl->mem_size;
}

int
tp6801_get_free_mem_size(Camera *camera)
{
	return (tp6801_max_filecount (camera) - camera->pl->picture_count) *
	       tp6801_filesize (camera);
}
