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
#ifdef HAVE_GD
#include <gd.h>
#endif

#include <gphoto2/gphoto2-result.h>
#include "ax203.h"

static const struct eeprom_info {
	const char *name;
	uint32_t id;
	int mem_size;
	int has_4k_sectors;
} ax203_eeprom_info[] = {
	{ "AMIC A25L040", 0x00133037,  524288, 1 },
	{ "AMIC A25L080", 0x00143037, 1048576, 1 },
	{ "AMIC A25L40P", 0x1320377f,  524288, 0 },
	{ "AMIC A25L80P", 0x1420377f, 1048576, 0 },
	{ "AMIC A25L16P", 0x1520377f, 2097152, 0 },

	/* Note the ATmel AT26DF041 id:0x0000441f is fsck-ed up. It doesn't
	   support ERASE_64K, (only 4K) and SPI_EEPROM_PP is 0x11 rather then
	   0x02 (0x02 only programs a single byte). */
	/* I cannot find a datasheet for the ATmel AT26DF081 id:0x0000451f */
	{ "ATmel AT26DF161",  0x0000461f, 2097152, 1 },
	{ "ATmel AT26DF081A", 0x0001451f, 1048576, 1 },
	{ "ATmel AT26DF161A", 0x0001461f, 2097152, 1 },
	{ "ATmel AT25DF081",  0x0002451f, 1048576, 1 },
	{ "ATmel AT25DF161",  0x0002461f, 2097152, 1 },

	{ "EON EN25B16", 0x1c15201c, 2097152, 0 },
	{ "EON EN25B32", 0x1c16201c, 4194304, 0 },
	{ "EON EN25F80", 0x1c14311c, 1048576, 1 },
	{ "EON EN25F16", 0x1c15311c, 2097152, 1 },

	{ "ESI ES25P80", 0x0014204a, 1048576, 0 },
	{ "ESI ES25P16", 0x0015204a, 2097152, 0 },

	{ "ESMT F25L008 (top)",    0x8c14208c, 1048576, 1 },
	{ "ESMT F25L008 (bottom)", 0x8c14218c, 1048576, 1 },

	{ "MXIC MX25L4005A", 0xc21320c2,  524288, 1 },
	{ "MXIC MX25L8005A", 0xc21420c2, 1048576, 1 },
	{ "MXIC MX25L1605A", 0xc21520c2, 2097152, 1 },

	{ "PMC Pm25LV010", 0x007e9d7f, 524288, 0 },

	{ "Spansion S25FL004A", 0x00120201,  524288, 0 },
	{ "Spansion S25FL008A", 0x00130201, 1048576, 0 },
	{ "Spansion S25FL016A", 0x00140201, 2097152, 0 },

	/* The SST25VF080 and SST25VF016 (id:0xbf8e25bf & 0xbf4125bf) PP
	   instruction can only program a single byte at a time. Thus they
	   are not supported */

	{ "Winbond W25P80", 0x001420ef, 1048576, 0 },
	{ "Winbond W25P16", 0x001420ef, 2097152, 0 },

	{ "Winbond W25X40", 0x001330ef,  524288, 1 },
	{ "Winbond W25X80", 0x001430ef, 1048576, 1 },
	{ "Winbond W25X16", 0x001530ef, 2097152, 1 },
	{ "Winbond W25X32", 0x001630ef, 4194304, 1 },
	{ "Winbond W25X64", 0x001730ef, 8388608, 1 },

	{ }
};

static int
ax203_send_cmd(Camera *camera, int to_dev, char *cmd, int cmd_size,
	char *data, int data_size)
{
	char sense_buffer[32];

	return gp_port_send_scsi_cmd (camera->port, to_dev, cmd, cmd_size,
		sense_buffer, sizeof(sense_buffer), data, data_size);
}

static int
ax203_send_eeprom_cmd(Camera *camera, int to_dev,
	char *eeprom_cmd, int eeprom_cmd_size,
	char *data, int data_size)
{
	char cmd_buffer[16];
	int i;

	memset (cmd_buffer, 0, sizeof (cmd_buffer));
	if (to_dev)
		cmd_buffer[0] = AX203_TO_DEV;
	else
		cmd_buffer[0] = AX203_FROM_DEV;

	cmd_buffer[5] = AX203_EEPROM_CMD;
	cmd_buffer[6] = eeprom_cmd_size;
	cmd_buffer[7] = (data_size >> 16) & 0xff;
	cmd_buffer[8] = (data_size >> 8) & 0xff;
	cmd_buffer[9] = (data_size) & 0xff;

	for (i = 0; i < eeprom_cmd_size; i++)
		cmd_buffer[10 + i] = eeprom_cmd[i];

	return ax203_send_cmd (camera, to_dev, cmd_buffer, sizeof(cmd_buffer),
			       data, data_size);
}

static int
ax203_get_version(Camera *camera, char *buf)
{
	int ret;
	char cmd_buffer[16];

	memset (cmd_buffer, 0, sizeof (cmd_buffer));

	cmd_buffer[0] = AX203_FROM_DEV;
	cmd_buffer[5] = AX203_GET_VERSION;
	cmd_buffer[6] = 1;
	cmd_buffer[10] = AX203_GET_VERSION;

	ret = ax203_send_cmd (camera, 0, cmd_buffer, sizeof(cmd_buffer),
			      buf, 64);
	buf[63] = 0; /* ensure 0 termination */

	return ret;
}

/* This would be very nice, if only not all devices would answer that they
   are a 128x128 pixels device */
#if 0
static int
ax203_get_lcd_size(Camera *camera)
{
	char cmd_buffer[16];
	uint8_t data_buffer[4];

	memset (cmd_buffer, 0, sizeof (cmd_buffer));

	cmd_buffer[0] = AX203_FROM_DEV;
	cmd_buffer[5] = AX203_GET_LCD_SIZE;
	cmd_buffer[6] = 0x01;
	cmd_buffer[10] = AX203_GET_LCD_SIZE;

	CHECK (ax203_send_cmd (camera, 0, cmd_buffer, sizeof (cmd_buffer),
			       (char *)data_buffer, sizeof (data_buffer)))

	camera->pl->width  = le16atoh (data_buffer);
	camera->pl->height = le16atoh (data_buffer + 2);

	return GP_OK;
}
#endif

static int
ax203_eeprom_device_identification(Camera *camera, char *buf)
{
	char cmd = SPI_EEPROM_RDID;

	return ax203_send_eeprom_cmd (camera, 0, &cmd, 1, buf, 4);
}

static int
ax203_eeprom_release_from_deep_powerdown(Camera *camera)
{
	char cmd = SPI_EEPROM_RDP;

	return ax203_send_eeprom_cmd (camera, 1, &cmd, 1, NULL, 0);
}

static int
ax203_eeprom_read(Camera *camera, int address, char *buf, int buf_size)
{
	char cmd[4];
    
	cmd[0] = SPI_EEPROM_READ;
	cmd[1] = (address >> 16) & 0xff;
	cmd[2] = (address >> 8) & 0xff;
	cmd[3] = (address) & 0xff;
		    
	return ax203_send_eeprom_cmd (camera, 0, cmd, sizeof(cmd), buf,
				      buf_size);
}

static int
ax203_eeprom_program_page(Camera *camera, int address, char *buf, int buf_size)
{
	char cmd[4];

	cmd[0] = SPI_EEPROM_PP;
	cmd[1] = (address >> 16) & 0xff;
	cmd[2] = (address >> 8) & 0xff;
	cmd[3] = (address) & 0xff;
		    
	return ax203_send_eeprom_cmd (camera, 1, cmd, sizeof(cmd), buf,
				      buf_size);
}

static int
ax203_eeprom_write_enable(Camera *camera)
{
	char cmd = SPI_EEPROM_WREN;

	return ax203_send_eeprom_cmd (camera, 1, &cmd, 1, NULL, 0);
}

static int
ax203_eeprom_erase_4k_sector(Camera *camera, int address)
{
	char cmd[4];

	cmd[0] = SPI_EEPROM_ERASE_4K;
	cmd[1] = (address >> 16) & 0xff;
	cmd[2] = (address >> 8) & 0xff;
	cmd[3] = (address) & 0xff;
		    
	return ax203_send_eeprom_cmd (camera, 1, cmd, sizeof(cmd), NULL, 0);
}

static int
ax203_eeprom_erase_64k_sector(Camera *camera, int address)
{
	char cmd[4];

	cmd[0] = SPI_EEPROM_ERASE_64K;
	cmd[1] = (address >> 16) & 0xff;
	cmd[2] = (address >> 8) & 0xff;
	cmd[3] = (address) & 0xff;
		    
	return ax203_send_eeprom_cmd (camera, 1, cmd, sizeof(cmd), NULL, 0);
}

static int
ax203_eeprom_wait_ready(Camera *camera)
{
	char cmd = SPI_EEPROM_RDSR; /* Read status */
	char buf[64]; /* Do as windows does, read the status word 64 times,
			 then check */
	while (1) {
		CHECK (ax203_send_eeprom_cmd (camera, 0, &cmd, 1, buf, 64))
		/* We only need to check the last read */
		if (!(buf[63] & 0x01)) /* Check write in progress bit */
			break; /* No write in progress, done waiting */
	}
	return GP_OK;
}

static int
ax203_read_sector(Camera *camera, int sector, char *buf)
{
	int ret;
	if (camera->pl->mem_dump) {
		ret = fseek (camera->pl->mem_dump,
			     sector * SPI_EEPROM_SECTOR_SIZE, SEEK_SET);
		if (ret) {
			gp_log (GP_LOG_ERROR, "ax203",
				"seeking in memdump: %s", strerror(errno));
			return GP_ERROR_IO_READ;
		}
		ret = fread (buf, 1, SPI_EEPROM_SECTOR_SIZE,
			     camera->pl->mem_dump);
		if (ret != SPI_EEPROM_SECTOR_SIZE) {
			if (ret < 0)
				gp_log (GP_LOG_ERROR, "ax203",
					"reading memdump: %s",
					strerror(errno));
			else
				gp_log (GP_LOG_ERROR, "ax203",
					"short read reading from memdump");
			return GP_ERROR_IO_READ;
		}
	} else {
		CHECK (ax203_eeprom_read (camera,
					  sector * SPI_EEPROM_SECTOR_SIZE,
					  buf, SPI_EEPROM_SECTOR_SIZE))
	}
	return GP_OK;
}

static int
ax203_write_sector(Camera *camera, int sector, char *buf)
{
	int ret;
	if (camera->pl->mem_dump) {
		ret = fseek (camera->pl->mem_dump,
			     sector * SPI_EEPROM_SECTOR_SIZE, SEEK_SET);
		if (ret) {
			gp_log (GP_LOG_ERROR, "ax203",
				"seeking in memdump: %s", strerror(errno));
			return GP_ERROR_IO_WRITE;
		}
		ret = fwrite (buf, 1, SPI_EEPROM_SECTOR_SIZE,
			      camera->pl->mem_dump);
		if (ret != SPI_EEPROM_SECTOR_SIZE) {
			gp_log (GP_LOG_ERROR, "ax203",
				"writing memdump: %s", strerror(errno));
			return GP_ERROR_IO_WRITE;
		}
	} else {
		int i, base = sector * SPI_EEPROM_SECTOR_SIZE;

		for (i = 0; i < SPI_EEPROM_SECTOR_SIZE; i += 256) {
			CHECK (ax203_eeprom_write_enable (camera))
			CHECK (ax203_eeprom_program_page (camera, base + i,
							  buf + i, 256))
			CHECK (ax203_eeprom_wait_ready (camera))
		}
	}
	return GP_OK;
}

static int
ax203_erase4k_sector(Camera *camera, int sector)
{
	if (camera->pl->mem_dump)
		return GP_OK;

	CHECK (ax203_eeprom_write_enable (camera))
	CHECK (ax203_eeprom_erase_4k_sector (camera,
					     sector * SPI_EEPROM_SECTOR_SIZE))
	CHECK (ax203_eeprom_wait_ready (camera))

	return GP_OK;
}

static int
ax203_erase64k_sector(Camera *camera, int sector)
{
	if (camera->pl->mem_dump)
		return GP_OK;

	CHECK (ax203_eeprom_write_enable (camera))
	CHECK (ax203_eeprom_erase_64k_sector (camera,
					     sector * SPI_EEPROM_SECTOR_SIZE))
	CHECK (ax203_eeprom_wait_ready (camera))

	return GP_OK;
}

static int
ax203_check_sector_present(Camera *camera, int sector)
{
	int ret;

	if ((sector + 1) * SPI_EEPROM_SECTOR_SIZE > camera->pl->mem_size) {
		gp_log (GP_LOG_ERROR, "ax203", "access beyond end of memory");
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (camera->pl->sector_is_present[sector])
		return GP_OK;

	ret = ax203_read_sector(camera, sector, camera->pl->mem +
				sector * SPI_EEPROM_SECTOR_SIZE);
	if (ret == 0)
		camera->pl->sector_is_present[sector] = 1;

	return ret;
}

static int
ax203_read_mem(Camera *camera, int offset,
	void *buf, int len)
{
	int to_copy, sector = offset / SPI_EEPROM_SECTOR_SIZE;

	while (len) {
		CHECK (ax203_check_sector_present (camera, sector))

		to_copy = SPI_EEPROM_SECTOR_SIZE -
			  (offset % SPI_EEPROM_SECTOR_SIZE);
		if (to_copy > len)
			to_copy = len;

		memcpy(buf, camera->pl->mem + offset, to_copy);
		buf += to_copy;
		len -= to_copy;
		offset += to_copy;
		sector++;
	}
	return GP_OK;
}

static int
ax203_write_mem(Camera *camera, int offset,
	void *buf, int len)
{
	int to_copy, sector = offset / SPI_EEPROM_SECTOR_SIZE;

	while (len) {
		CHECK (ax203_check_sector_present (camera, sector))

		to_copy = SPI_EEPROM_SECTOR_SIZE -
			  (offset % SPI_EEPROM_SECTOR_SIZE);
		if (to_copy > len)
			to_copy = len;

		memcpy(camera->pl->mem + offset, buf, to_copy);
		camera->pl->sector_dirty[sector] = 1;

		buf += to_copy;
		len -= to_copy;
		offset += to_copy;
		sector++;
	}
	return GP_OK;
}

/* <sigh> the get_lcd_size command does not seem to be reliable, so instead
   read the parameter block at the beginning of the eeprom. This function
   also checks some hopefully constant bytes before and after the
   lcd size to make sure it is not reading garbage */
static int ax203_detect_lcd_size(Camera *camera)
{
	uint8_t buf[8];
	uint8_t expect[8];
	int i, param_offset, resolution_offset;
	const int valid_resolutions[][2] = {
		{ 120, 160 },
		{ 128, 128 },
		{ 128, 160 },
		{ 160, 120 },
		{ 160, 128 },
		{ 240, 320 },
		{ 320, 240 },
		{ }
	};
	const uint8_t expect_33x[8] = {
		0x13, 0x15, 0, 0, 0x02, 0x01, 0x02, 0x01 };
	const uint8_t expect_34x[8] = {
		0x13, 0x15, 0, 0, 0, 0, 0x03, 0x01 };
	const uint8_t expect_35x[8] = {
		0x00, 0x00, 0x007, 0, 0, 0, 0, 0xd8 };

	switch (camera->pl->firmware_version) {
	case AX203_FIRMWARE_3_3_x:
		param_offset = 0x50;
		resolution_offset = 2;
		memcpy (expect, expect_33x, 8);
		break;
	case AX203_FIRMWARE_3_4_x:
		param_offset = 0x50;
		resolution_offset = 2;
		memcpy (expect, expect_34x, 8);
		break;
	case AX203_FIRMWARE_3_5_x:
		param_offset = 0x20;
		resolution_offset = 3;
		memcpy (expect, expect_35x, 8);
		break;
	}

	CHECK (ax203_read_mem (camera, param_offset, buf, 8))

	switch (camera->pl->firmware_version) {
	case AX203_FIRMWARE_3_3_x:
		camera->pl->width  = buf[resolution_offset    ];
		camera->pl->height = buf[resolution_offset + 1];
		expect[resolution_offset    ] = camera->pl->width;
		expect[resolution_offset + 1] = camera->pl->height;
		break;
	case AX203_FIRMWARE_3_4_x:
	case AX203_FIRMWARE_3_5_x:
		camera->pl->width  = le16atoh (buf + resolution_offset);
		camera->pl->height = le16atoh (buf + resolution_offset + 2);
		htole16a(expect + resolution_offset    , camera->pl->width);
		htole16a(expect + resolution_offset + 2, camera->pl->height);
		break;
	}

	if (memcmp (buf, expect, 8)) {
		gp_log (GP_LOG_ERROR, "ax203",
			"unexpected contents of parameter block");
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	for (i = 0; valid_resolutions[i][0]; i++)
		if (valid_resolutions[i][0] == camera->pl->width &&
		    valid_resolutions[i][1] == camera->pl->height)
			break;

	if (!valid_resolutions[i][0]) {
		gp_log (GP_LOG_ERROR, "ax203", "unknown resolution: %dx%d",
			camera->pl->width, camera->pl->height);
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	return GP_OK;
}

static int ax203_check_fs_signature(Camera *camera)
{
	char buf[4];
	int fs_start;

	switch (camera->pl->firmware_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		fs_start = AX203_ABFS_START;
		break;
	case AX203_FIRMWARE_3_5_x:
		fs_start = AX206_ABFS_START;
		break;
	}

	CHECK (ax203_read_mem (camera, fs_start, buf, 4))
	if (memcmp (buf, AX203_ABFS_MAGIC, 4)) {
		gp_log (GP_LOG_ERROR, "ax203", "ABFS magic missing");
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	return GP_OK;
}

/* Note this funtions returns the file count on success */
static int ax203_check_file_index(Camera *camera, int idx)
{
	int count;

	if (idx < 0) {
		gp_log (GP_LOG_ERROR, "ax203",
			"file index < 0");
		return GP_ERROR_BAD_PARAMETERS;
	}

	count = ax203_read_filecount (camera);
	if (count < 0) return count;

	if (idx >= count) {
		gp_log (GP_LOG_ERROR, "ax203",
			"file index beyond end of FAT");
		return GP_ERROR_BAD_PARAMETERS;
	}

	return count;
}

static int
ax203_read_v3_3_x_v3_4_x_filecount(Camera *camera)
{
	uint8_t count;

	CHECK (ax203_read_mem (camera,
			       AX203_ABFS_START + AX203_ABFS_COUNT_OFFSET,
			       &count, 1))
	return count;
}

static int
ax203_write_v3_3_x_v3_4_x_filecount(Camera *camera, int count)
{
	uint8_t c = count;

	CHECK (ax203_write_mem (camera,
				AX203_ABFS_START + AX203_ABFS_COUNT_OFFSET,
				&c, 1))
	return GP_OK;
}

static int
ax203_read_v3_5_x_filecount(Camera *camera)
{
	/* The v3.5.x firmware "FAT" does not keep a separate file count,
	   so always return the maximum number of entries it can contain
	   (and rely on fileinfo.present to figure out how many files there
	    actually are). */
	return (AX206_ABFS_SIZE - AX206_ABFS_FILE_OFFSET (0)) /
		sizeof(struct ax203_v3_5_x_raw_fileinfo);
}

static int
ax203_write_v3_5_x_filecount(Camera *camera, int count)
{
	/* This is a no-op as the v3.5.x firmware "FAT" does not keep a
	   separate file count */
	return GP_OK;
}

static
int ax203_read_v3_3_x_v3_4_x_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	uint8_t buf[2];

	CHECK (ax203_check_file_index (camera, idx))
	CHECK (ax203_read_mem (camera,
			       AX203_ABFS_START + AX203_ABFS_FILE_OFFSET (idx),
			       buf, 2))

	fileinfo->address = le16atoh (buf) << 8;
	fileinfo->present = fileinfo->address? 1 : 0;

	switch (camera->pl->compression_version) {
	case AX203_COMPRESSION_YUV:
		fileinfo->size = camera->pl->width * camera->pl->height;
		break;
	case AX203_COMPRESSION_YUV_DELTA:
		fileinfo->size = camera->pl->width * camera->pl->height * 3/4;
		break;
	case AX203_COMPRESSION_JPEG:
		gp_log (GP_LOG_ERROR, "ax203",
			"Compression: %d not support with firmware ver: %d",
			camera->pl->compression_version,
			camera->pl->firmware_version);
		return GP_ERROR_BAD_PARAMETERS;
	}

	return GP_OK;
}

static
int ax203_write_v3_3_x_v3_4_x_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	uint8_t buf[2];
	int size_ok = 0;

	if (fileinfo->address & 0xff) {
		gp_log (GP_LOG_ERROR, "ax203", "LSB of address is not 0");
		return GP_ERROR_BAD_PARAMETERS;
	}

	switch (camera->pl->compression_version) {
	case AX203_COMPRESSION_YUV:
		size_ok = (fileinfo->size == 
			   camera->pl->width * camera->pl->height);
		break;
	case AX203_COMPRESSION_YUV_DELTA:
		size_ok = (fileinfo->size ==
			   camera->pl->width * camera->pl->height * 3 / 4);
		break;
	case AX203_COMPRESSION_JPEG:
		gp_log (GP_LOG_ERROR, "ax203",
			"Compression: %d not support with firmware ver: %d",
			camera->pl->compression_version,
			camera->pl->firmware_version);
		return GP_ERROR_BAD_PARAMETERS;
	}
	if (!size_ok) {
		gp_log (GP_LOG_ERROR, "ax203", "invalid picture size");
		return GP_ERROR_BAD_PARAMETERS;
	}

	if (!fileinfo->present)
		fileinfo->address = 0;

	htole16a(buf, fileinfo->address >> 8);
	CHECK (ax203_write_mem (camera,
			       AX203_ABFS_START + AX203_ABFS_FILE_OFFSET (idx),
			       buf, 2))

	return GP_OK;
}

static
int ax203_read_v3_5_x_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	struct ax203_v3_5_x_raw_fileinfo raw;

	CHECK (ax203_read_mem (camera,
			       AX206_ABFS_START + AX206_ABFS_FILE_OFFSET (idx),
			       &raw, sizeof(raw)))

	fileinfo->present = raw.present == 0x01;
	fileinfo->address = le32toh (raw.address);
	fileinfo->size    = le16toh (raw.size);

	return GP_OK;	
}

static
int ax203_write_v3_5_x_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	struct ax203_v3_5_x_raw_fileinfo raw;

	raw.present = fileinfo->present;
	raw.address = htole32(fileinfo->address);
	raw.size    = htole16(fileinfo->size);

	CHECK (ax203_write_mem (camera,
			       AX206_ABFS_START + AX206_ABFS_FILE_OFFSET (idx),
			       &raw, sizeof(raw)))

	return GP_OK;
}

/***************** Begin "public" functions *****************/

int
ax203_read_filecount(Camera *camera)
{
	switch (camera->pl->firmware_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		return ax203_read_v3_3_x_v3_4_x_filecount (camera);
	case AX203_FIRMWARE_3_5_x:
		return ax203_read_v3_5_x_filecount (camera);;
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;
}

static int
ax203_write_filecount(Camera *camera, int count)
{
	switch (camera->pl->firmware_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		return ax203_write_v3_3_x_v3_4_x_filecount (camera, count);
	case AX203_FIRMWARE_3_5_x:
		return ax203_write_v3_5_x_filecount (camera, count);
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;
}

static
int ax203_read_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	switch (camera->pl->firmware_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		return ax203_read_v3_3_x_v3_4_x_fileinfo (camera, idx,
							  fileinfo);
	case AX203_FIRMWARE_3_5_x:
		return ax203_read_v3_5_x_fileinfo (camera, idx, fileinfo);
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;	
}

static
int ax203_write_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	switch (camera->pl->firmware_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		return ax203_write_v3_3_x_v3_4_x_fileinfo (camera, idx,
							   fileinfo);
	case AX203_FIRMWARE_3_5_x:
		return ax203_write_v3_5_x_fileinfo (camera, idx, fileinfo);
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;	
}

int
ax203_file_present(Camera *camera, int idx)
{
	struct ax203_fileinfo fileinfo;
	
	CHECK (ax203_read_fileinfo (camera, idx, &fileinfo))

	return fileinfo.present;
}

static int
ax203_decode_image(Camera *camera, char *src, int src_size, int **dest)
{
	int ret;
	unsigned int x, y, width, height;
	unsigned char *components[3];

	switch (camera->pl->compression_version) {
#ifdef HAVE_GD
	case AX203_COMPRESSION_YUV:
		ax203_decode_yuv (src, dest, camera->pl->width,
				  camera->pl->height);
		return GP_OK;
	case AX203_COMPRESSION_YUV_DELTA:
		ax203_decode_yuv_delta (src, dest, camera->pl->width,
					camera->pl->height);
		return GP_OK;
	case AX203_COMPRESSION_JPEG:
		if (!camera->pl->jdec) {
			camera->pl->jdec = tinyjpeg_init ();
			if (!camera->pl->jdec)
				return GP_ERROR_NO_MEMORY;
		}
		ret = tinyjpeg_parse_header (camera->pl->jdec,
					     (unsigned char *)src, src_size);
		if (ret) {
			gp_log (GP_LOG_ERROR, "ax203",
				"Error parsing header: %s",
				tinyjpeg_get_errorstring (camera->pl->jdec));
			return GP_ERROR_CORRUPTED_DATA;
		}
		tinyjpeg_get_size (camera->pl->jdec, &width, &height);
		if ((int)width  != camera->pl->width ||
		    (int)height != camera->pl->height) {
			gp_log (GP_LOG_ERROR, "ax203",
				"Hdr dimensions %ux%u don't match lcd %dx%d",
				width, height,
				camera->pl->width, camera->pl->height);
			return GP_ERROR_CORRUPTED_DATA;
		}
		ret = tinyjpeg_decode (camera->pl->jdec);
		if (ret) {
			gp_log (GP_LOG_ERROR, "ax203",
				"Error decoding JPEG data: %s",
				tinyjpeg_get_errorstring (camera->pl->jdec));
			return GP_ERROR_CORRUPTED_DATA;
		}
		tinyjpeg_get_components (camera->pl->jdec, components);
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dest[y][x] = gdTrueColor (components[0][0],
							  components[0][1],
							  components[0][2]);
				components[0] += 3;
			}
		}
		return GP_OK;
#else
	case AX203_COMPRESSION_YUV:
	case AX203_COMPRESSION_YUV_DELTA:
	case AX203_COMPRESSION_JPEG:
		gp_log (GP_LOG_ERROR, "ax203",
			"Compression: %d not supported without libgd",
			camera->pl->compression_version);
		return GP_ERROR_NOT_SUPPORTED;
#endif
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;
}

/* Returns the number of bytes of dest used or a negative error code */
static int
ax203_encode_image(Camera *camera, int **src, char *dest, int dest_size)
{
	int size;

	switch (camera->pl->compression_version) {
#ifdef HAVE_GD
	case AX203_COMPRESSION_YUV:
		size = camera->pl->width * camera->pl->height;
		if (dest_size < size)
			return GP_ERROR_BAD_PARAMETERS;
		ax203_encode_yuv (src, dest, camera->pl->width,
				  camera->pl->height);
		return size;
	case AX203_COMPRESSION_YUV_DELTA:
		size = camera->pl->width * camera->pl->height * 3 / 4;
		if (dest_size < size)
			return GP_ERROR_BAD_PARAMETERS;
		ax203_encode_yuv_delta (src, dest, camera->pl->width,
					camera->pl->height);
		return size;
	case AX203_COMPRESSION_JPEG:
		return ax203_compress_jpeg (src, (uint8_t *)dest, dest_size,
					    camera->pl->width,
					    camera->pl->height);
#else
	case AX203_COMPRESSION_YUV:
	case AX203_COMPRESSION_YUV_DELTA:
	case AX203_COMPRESSION_JPEG:
		gp_log (GP_LOG_ERROR, "ax203",
			"Compression: %d not supported without libgd",
			camera->pl->compression_version);
		return GP_ERROR_NOT_SUPPORTED;
#endif
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;	
}

int
ax203_read_file(Camera *camera, int idx, int **rgb24)
{
	char *src;
	struct ax203_fileinfo fileinfo;
	int ret;

	CHECK (ax203_read_fileinfo (camera, idx, &fileinfo))

	if (!fileinfo.present) {
		gp_log (GP_LOG_ERROR, "ax203",
			"trying to read a deleted file");
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Allocate 1 extra byte as tinyjpeg's huffman decoding sometimes
	   reads a few bits more then it needs */
	src = malloc(fileinfo.size + 1);
	if (!src) {
		gp_log (GP_LOG_ERROR, "ax203", "allocating memory");
		return GP_ERROR_NO_MEMORY;
	}

	ret = ax203_read_mem (camera, fileinfo.address, src, fileinfo.size);
	if (ret < 0) { free(src); return ret; }

	ret = ax203_decode_image (camera, src, fileinfo.size + 1, rgb24);
	free(src);

	return ret;
}

int
ax203_write_file(Camera *camera, int **rgb24)
{
	int i, count, size, start, end = 0, hole_start = 0, hole_idx = 0;
	struct ax203_fileinfo fileinfo;
	const int buf_size = camera->pl->width * camera->pl->height;
	char buf[buf_size];

	size = ax203_encode_image (camera, rgb24, buf, buf_size);
	if (size < 0) return size;

	count = ax203_read_filecount (camera);
	if (count < 0) return count;

	/* Try to find a large enough "hole" in the memory */
	switch (camera->pl->firmware_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		end = AX203_PICTURE_START;
		break;
	case AX203_FIRMWARE_3_5_x:
		end = AX206_PICTURE_START;
		break;
	}
	for (i = 0; i <= count; i++) {
		/* Fake a present fileinfo at the end of picture mem */
		if (i == count) {
			fileinfo.present = 1;
			start = camera->pl->mem_size;
			/* If the last fileinfo in the "FAT" was present, we
			   set hole_start to the end of the last picture */
			if (!hole_start) {
				hole_start = end;
				hole_idx = i;
			}
		} else {
			CHECK (ax203_read_fileinfo (camera, i, &fileinfo))
			start = fileinfo.address;
			if (fileinfo.present)
				end = fileinfo.address + fileinfo.size;
		}

		/* If we have a hole start address look for present entries (so
		   a hole end address), else look for non present entries */
		if (hole_start) {
			if (fileinfo.present) {
				int hole_size = start - hole_start;
				GP_DEBUG ("found a hole at: %08x, of %d bytes "
					  "(need %d)\n", hole_start, hole_size,
					  size);
				if (hole_size < size) {
					/* Too small, start searching
					   for the next hole */
					hole_start = 0;
					continue;
				}

				/* bingo we have a large enough hole */
				fileinfo.address = hole_start;
				fileinfo.size    = size;
				fileinfo.present = 1;
				CHECK (ax203_write_fileinfo (camera, hole_idx,
							     &fileinfo))
				if (hole_idx == count) {
					/* update picture count */
					count++;
					CHECK (ax203_write_filecount (camera,
								       count))
				}
				CHECK (ax203_write_mem (camera, hole_start,
							buf, size))
				return GP_OK;
			}
		} else {
			if (!fileinfo.present) {
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
	
	gp_log (GP_LOG_ERROR, "ax203", "not enough freespace to add file");
	return GP_ERROR_NO_SPACE;
}

int
ax203_delete_file(Camera *camera, int idx)
{
	int count, new_count = 0;
	struct ax203_fileinfo fileinfo;

	count = ax203_check_file_index (camera, idx);
	if (count < 0) return count;

	CHECK (ax203_read_fileinfo (camera, idx, &fileinfo))

	if (!fileinfo.present) {
		gp_log (GP_LOG_ERROR, "ax203",
			"trying to delete an already deleted file");
		return GP_ERROR_BAD_PARAMETERS;
	}

	fileinfo.present = 0;
	CHECK (ax203_write_fileinfo (camera, idx, &fileinfo))

	/* "Recalculate" the file count */
	for (idx = 0; idx < count; idx++) {
		if (ax203_file_present (camera, idx))
			new_count = idx + 1;
	}

	CHECK (ax203_write_filecount (camera, new_count))

	return GP_OK;
}

int
ax203_delete_all(Camera *camera)
{
	char buf[AX203_ABFS_SIZE];

	switch (camera->pl->firmware_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		memset(buf, 0, AX203_ABFS_SIZE);
		CHECK (ax203_write_mem (camera,
				AX203_ABFS_START + AX203_ABFS_FILE_OFFSET (0),
				buf,
				AX203_ABFS_SIZE - AX203_ABFS_FILE_OFFSET (0)))
		break;
	case AX203_FIRMWARE_3_5_x:
		memset(buf, 0, AX206_ABFS_SIZE);
		CHECK (ax203_write_mem (camera,
				AX206_ABFS_START + AX206_ABFS_FILE_OFFSET (0),
				buf,
				AX206_ABFS_SIZE - AX206_ABFS_FILE_OFFSET (0)))
	}

	CHECK (ax203_write_filecount (camera, 0))

	return GP_OK;
}

/* bss = block start sector */
static int
ax203_commit_block_4k(Camera *camera, int bss)
{
	int block_sector_size = SPI_EEPROM_BLOCK_SIZE / SPI_EEPROM_SECTOR_SIZE;
	int i;

	for (i = 0; i < block_sector_size; i++) {
		if (!camera->pl->sector_dirty[bss + i])
			continue;

		CHECK (ax203_erase4k_sector (camera, bss + i))
		CHECK (ax203_write_sector (camera, bss + i,
					   camera->pl->mem +
					   (bss + i) *
					   SPI_EEPROM_SECTOR_SIZE))
		camera->pl->sector_dirty[bss + i] = 0;
	}
	return GP_OK;
}

static int
ax203_commit_block_64k(Camera *camera, int bss)
{
	int block_sector_size = SPI_EEPROM_BLOCK_SIZE / SPI_EEPROM_SECTOR_SIZE;
	int i;

	/* Make sure we have read the entire block before erasing it !! */
	for (i = 0; i < block_sector_size; i++)
		CHECK (ax203_check_sector_present (camera, bss + i))

	/* Erase the block */
	CHECK (ax203_erase64k_sector (camera, bss))

	/* And re-program all sectors in the block */
	for (i = 0; i < block_sector_size; i++) {
		CHECK (ax203_write_sector (camera, bss + i,
					   camera->pl->mem +
					   (bss + i) *
					   SPI_EEPROM_SECTOR_SIZE))
		camera->pl->sector_dirty[bss + i] = 0;
	}
	return GP_OK;
}

int
ax203_commit(Camera *camera)
{
	int i, j;
	int mem_sector_size = camera->pl->mem_size / SPI_EEPROM_SECTOR_SIZE;
	int block_sector_size = SPI_EEPROM_BLOCK_SIZE / SPI_EEPROM_SECTOR_SIZE;
	int dirty_sectors;

	/* We first check each 64k block for dirty sectors. If the block
	   contains dirty sectors, decide wether to use 4k sector erase
	   commands (if the eeprom supports it), or to erase and reprogram
	   the entire block */
	for (i = 0; i < mem_sector_size; i += block_sector_size) {
		dirty_sectors = 0;
		for (j = 0; j < block_sector_size; j++)
			if (camera->pl->sector_dirty[i + j])
				dirty_sectors++;

		/* If we have no dirty sectors in this block continue */
		if (!dirty_sectors)
			continue;

		/* There are 16 4k sectors per 64k block, when we need to
		   program 12 or more sectors, programming the entire block
		   becomes faster */
		if (dirty_sectors < 12 && camera->pl->has_4k_sectors)
			CHECK (ax203_commit_block_4k (camera, i))
		else
			CHECK (ax203_commit_block_64k (camera, i))
	}
	return GP_OK;
}

static int
ax203_init(Camera *camera)
{
	GP_DEBUG ("ax203_init called");

	camera->pl->mem = malloc(camera->pl->mem_size);
	if (!camera->pl->mem)
		return GP_ERROR_NO_MEMORY;

	CHECK (ax203_detect_lcd_size (camera))
	GP_DEBUG ("Appotech ax203 picframe of %dx%d detected.",
		  camera->pl->width, camera->pl->height);

	if ((camera->pl->width % 4) || (camera->pl->height % 4)) {
		gp_log (GP_LOG_ERROR, "ax203",
			"lcd width and height must be a multiple of 4");
		return GP_ERROR_IO;
	}

	CHECK ( ax203_check_fs_signature (camera))

	return GP_OK;
}

static void
ax203_exit(Camera *camera)
{
	if (camera->pl->jdec) {
		tinyjpeg_free (camera->pl->jdec);
		camera->pl->jdec = NULL;
	}
	free (camera->pl->mem);
	camera->pl->mem = NULL;
}

int
ax203_open_device(Camera *camera)
{
	char buf[64];
	uint32_t id;
	int i;

	CHECK (ax203_get_version (camera, buf))
	GP_DEBUG ("Appotech ax203 picframe firmware version: %s", buf);

	/* Not sure if this is necessary, but the windows software does it */
	CHECK (ax203_eeprom_release_from_deep_powerdown (camera))
	CHECK (ax203_eeprom_device_identification (camera, buf))
	id = le32atoh((uint8_t *)buf);
	for (i = 0; ax203_eeprom_info[i].name; i++) {
		if (ax203_eeprom_info[i].id == id)
			break;
	}
	
	if (!ax203_eeprom_info[i].name) {
		gp_log (GP_LOG_ERROR, "ax203", "unknown eeprom id: %08x", id);
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	camera->pl->mem_size       = ax203_eeprom_info[i].mem_size;
	camera->pl->has_4k_sectors = ax203_eeprom_info[i].has_4k_sectors;
	GP_DEBUG ("%s EEPROM found, capacity: %d, has 4k sectors: %d",
		  ax203_eeprom_info[i].name, camera->pl->mem_size,
		  camera->pl->has_4k_sectors);

	if (camera->pl->has_4k_sectors == -1) {
		gp_log (GP_LOG_ERROR, "ax203", "%s has an unknown sector size",
			ax203_eeprom_info[i].name);
		return GP_ERROR_NOT_SUPPORTED;
	}

	return ax203_init (camera);
}

int
ax203_open_dump(Camera *camera, const char *dump)
{
	camera->pl->mem_dump = fopen(dump, "r+");
	if (!camera->pl->mem_dump) {
		gp_log (GP_LOG_ERROR, "ax203", "opening memdump file: %s: %s",
			dump, strerror(errno));
		return GP_ERROR_IO_INIT;
	}

	if (fseek (camera->pl->mem_dump, 0, SEEK_END)) {
		gp_log (GP_LOG_ERROR, "ax203", "seeking memdump file: %s: %s",
			dump, strerror(errno));
		return GP_ERROR_IO_INIT;
	}
	camera->pl->mem_size = ftell (camera->pl->mem_dump);
	camera->pl->has_4k_sectors = 1;

	return ax203_init (camera);
}

void ax203_close(Camera *camera)
{
	ax203_exit (camera);
	if (camera->pl->mem_dump) {
		fclose (camera->pl->mem_dump);
		camera->pl->mem_dump = NULL;
	}
}

int
ax203_get_mem_size(Camera *camera)
{
	return camera->pl->mem_size;
}
