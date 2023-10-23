/* Appotech ax203 picframe access library
 *
 *   Copyright (c) 2010-2012 Hans de Goede <hdegoede@redhat.com>
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
#ifdef HAVE_LIBGD
#include <gd.h>
#endif

#include <gphoto2/gphoto2-result.h>
#include "ax203.h"
#ifdef HAVE_LIBJPEG
#include "jpeg_memsrcdest.h"
#endif

static const struct eeprom_info {
	const char *name;
	uint32_t id;
	int mem_size;
	int has_4k_sectors;
	int pp_64k;
} ax203_eeprom_info[] = {
	{ "AMIC A25L040", 0x37133037,  524288, 1, 0 },
	{ "AMIC A25L080", 0x37143037, 1048576, 1, 0 },
	{ "AMIC A25L40P", 0x1320377f,  524288, 0, 0 },
	{ "AMIC A25L80P", 0x1420377f, 1048576, 0, 0 },
	{ "AMIC A25L16P", 0x1520377f, 2097152, 0, 0 },

	/* Note the ATmel AT26DF041 id:0x0000441f is fsck-ed up. It doesn't
	   support ERASE_64K, (only 4K) and SPI_EEPROM_PP is 0x11 rather then
	   0x02 (0x02 only programs a single byte). */
	/* I cannot find a datasheet for the ATmel AT26DF081 id:0x0000451f */
	{ "ATmel AT26DF161",  0x0000461f, 2097152, 1, 0 },
	{ "ATmel AT26DF081A", 0x0001451f, 1048576, 1, 0 },
	{ "ATmel AT26DF161A", 0x0001461f, 2097152, 1, 0 },
	{ "ATmel AT25DF081",  0x0002451f, 1048576, 1, 0 },
	{ "ATmel AT25DF161",  0x0002461f, 2097152, 1, 0 },

	{ "EON EN25B16", 0x1c15201c, 2097152, 0, 0 },
	{ "EON EN25B32", 0x1c16201c, 4194304, 0, 0 },
	{ "EON EN25F80", 0x1c14311c, 1048576, 1, 0 },
	{ "EON EN25F16", 0x1c15311c, 2097152, 1, 0 },

	{ "ESI ES25P80", 0x0014204a, 1048576, 0, 0 },
	{ "ESI ES25P16", 0x0015204a, 2097152, 0, 0 },

	{ "ESMT F25L008 (top)",    0x8c14208c, 1048576, 1, 0 },
	{ "ESMT F25L008 (bottom)", 0x8c14218c, 1048576, 1, 0 },

	{ "GigaDevice GD25Q40", 0xc81340c8,  524288, 1, 0 },
	{ "GigaDevice GD25Q80", 0xc81440c8, 1048576, 1, 0 },
	{ "GigaDevice GD25Q16", 0xc81540c8, 2097152, 1, 0 },

	{ "MXIC MX25L4005A", 0xc21320c2,  524288, 1, 0 },
	{ "MXIC MX25L8005A", 0xc21420c2, 1048576, 1, 0 },
	{ "MXIC MX25L1605A", 0xc21520c2, 2097152, 1, 0 },

	{ "Nantronics N25S80", 0xd51430d5, 1048576, 1, 0 },

	{ "PMC Pm25LV010", 0x007e9d7f, 524288, 0, 0 },

	{ "Spansion S25FL004A", 0x00120201,  524288, 0, 0 },
	{ "Spansion S25FL008A", 0x00130201, 1048576, 0, 0 },
	{ "Spansion S25FL016A", 0x00140201, 2097152, 0, 0 },

	{ "SST25VF080", 0xbf8e25bf, 1048576, 0, 1 },
	{ "SST25VF016", 0xbf4125bf, 2097152, 0, 1 },

	{ "ST M25P08", 0x7f142020, 1048576, 0, 0 },
	{ "ST M25P16", 0x7f152020, 2097152, 0, 0 },
	{ "ST M25P32", 0x7f162020, 4194304, 0, 0 },
	{ "ST M25P64", 0x7f172020, 8388608, 0, 0 },

	{ "Winbond W25P80", 0x001420ef, 1048576, 0, 0 },
	{ "Winbond W25P16", 0x001520ef, 2097152, 0, 0 },

	{ "Winbond W25X40", 0x001330ef,  524288, 1, 0 },
	{ "Winbond W25X80", 0x001430ef, 1048576, 1, 0 },
	{ "Winbond W25X16", 0x001530ef, 2097152, 1, 0 },
	{ "Winbond W25X32", 0x001630ef, 4194304, 1, 0 },
	{ "Winbond W25X64", 0x001730ef, 8388608, 1, 0 },

	{ "Winbond W25Q80", 0x001440ef, 1048576, 1, 0 },
	{ "Winbond W25Q16", 0x001540ef, 2097152, 1, 0 },
	{ "Winbond W25Q32", 0x001640ef, 4194304, 1, 0 },

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
	char *data, int data_size, char extra_arg)
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

	cmd_buffer[15] = extra_arg;

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
ax203_get_checksum(Camera *camera, int address, int size)
{
	int ret;
	char cmd_buffer[16];
	uint8_t buf[2];

	memset (cmd_buffer, 0, sizeof (cmd_buffer));

	cmd_buffer[0] = AX203_FROM_DEV;
	cmd_buffer[5] = AX203_GET_CHECKSUM;
	cmd_buffer[6] = AX203_GET_CHECKSUM;
	cmd_buffer[7] = (size >> 8) & 0xff;
	cmd_buffer[8] = size & 0xff;
	cmd_buffer[11] = (address >> 16) & 0xff;
	cmd_buffer[12] = (address >> 8) & 0xff;
	cmd_buffer[13] = address & 0xff;

	ret = ax203_send_cmd (camera, 0, cmd_buffer, sizeof(cmd_buffer),
			      (char *)buf, 2);
	if (ret < 0) return ret;

	/*return be16atoh(buf);*/
	return (buf[0] << 8) | buf[1];
}

static int
ax3003_get_frame_id(Camera *camera)
{
	int ret;
	char cmd_buffer[16];
	uint8_t id;

	memset (cmd_buffer, 0, sizeof (cmd_buffer));

	cmd_buffer[0] = AX3003_FRAME_CMD;
	cmd_buffer[1] = AX3003_GET_FRAME_ID;

	ret = ax203_send_cmd (camera, 0, cmd_buffer, sizeof(cmd_buffer),
			      (char *)&id, 1);
	if (ret < 0) return ret;

	return id;
}

static int
ax3003_get_abfs_start(Camera *camera)
{
	int ret;
	char cmd_buffer[16];
	uint8_t buf[2];

	memset (cmd_buffer, 0, sizeof (cmd_buffer));

	cmd_buffer[0] = AX3003_FRAME_CMD;
	cmd_buffer[1] = AX3003_GET_ABFS_START;

	ret = ax203_send_cmd (camera, 0, cmd_buffer, sizeof(cmd_buffer),
			      (char *)buf, 2);
	if (ret < 0) return ret;

	return ((buf[0] << 8) | buf[1]) * 0x100;
	/*return be16atoh(buf) * 0x100;*/
}

int
ax203_set_time_and_date(Camera *camera, struct tm *t)
{
	char cmd_buffer[16];

	memset (cmd_buffer, 0, sizeof (cmd_buffer));

	cmd_buffer[0] = AX203_SET_TIME;

	cmd_buffer[5] = t->tm_year % 100;

	switch (camera->pl->frame_version) {
	case AX3003_FIRMWARE_3_5_x:
		/* Note this is what the windows software does, with the
		   one AX3003 frame I have this does not do anything */
		cmd_buffer[0] = AX3003_FRAME_CMD;
		cmd_buffer[1] = AX3003_SET_TIME;
		/* fall through */
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		cmd_buffer[6] = t->tm_mon + 1;
		cmd_buffer[7] = t->tm_wday;
		break;
	case AX206_FIRMWARE_3_5_x:
		cmd_buffer[6] = 19 + t->tm_year / 100;
		cmd_buffer[7] = t->tm_mon + 1;
		break;
	}
	cmd_buffer[8]  = t->tm_mday;
	cmd_buffer[9]  = t->tm_hour;
	cmd_buffer[10] = t->tm_min;
	cmd_buffer[11] = t->tm_sec;

	return ax203_send_cmd (camera, 0, cmd_buffer, sizeof(cmd_buffer),
			       NULL, 0);
}

static int
ax203_eeprom_device_identification(Camera *camera, char *buf)
{
	char cmd = SPI_EEPROM_RDID;

	return ax203_send_eeprom_cmd (camera, 0, &cmd, 1, buf, 64, 0);
}

static int
ax203_eeprom_release_from_deep_powerdown(Camera *camera)
{
	char cmd = SPI_EEPROM_RDP;

	return ax203_send_eeprom_cmd (camera, 1, &cmd, 1, NULL, 0, 0);
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
				      buf_size, 0);
}

static int
ax203_eeprom_program_page(Camera *camera, int address, char *buf, int buf_size,
	char extra_arg)
{
	char cmd[4];

	cmd[0] = SPI_EEPROM_PP;
	cmd[1] = (address >> 16) & 0xff;
	cmd[2] = (address >> 8) & 0xff;
	cmd[3] = (address) & 0xff;

	return ax203_send_eeprom_cmd (camera, 1, cmd, sizeof(cmd), buf,
				      buf_size, extra_arg);
}

static int
ax203_eeprom_write_enable(Camera *camera)
{
	char cmd = SPI_EEPROM_WREN;

	return ax203_send_eeprom_cmd (camera, 1, &cmd, 1, NULL, 0, 0);
}

static int
ax203_eeprom_clear_block_protection(Camera *camera)
{
	char cmd[2];

	cmd[0] = SPI_EEPROM_WRSR;
	cmd[1] = 0;

	return ax203_send_eeprom_cmd (camera, 1, cmd, sizeof(cmd), NULL, 0, 0);
}

static int
ax203_eeprom_erase_4k_sector(Camera *camera, int address)
{
	char cmd[4];

	cmd[0] = SPI_EEPROM_ERASE_4K;
	cmd[1] = (address >> 16) & 0xff;
	cmd[2] = (address >> 8) & 0xff;
	cmd[3] = (address) & 0xff;

	return ax203_send_eeprom_cmd (camera, 1, cmd, sizeof(cmd), NULL, 0, 0);
}

static int
ax203_eeprom_erase_64k_sector(Camera *camera, int address)
{
	char cmd[4];

	cmd[0] = SPI_EEPROM_ERASE_64K;
	cmd[1] = (address >> 16) & 0xff;
	cmd[2] = (address >> 8) & 0xff;
	cmd[3] = (address) & 0xff;

	return ax203_send_eeprom_cmd (camera, 1, cmd, sizeof(cmd), NULL, 0, 0);
}

static int
ax203_eeprom_wait_ready(Camera *camera)
{
	char cmd = SPI_EEPROM_RDSR; /* Read status */
	char buf[64];
	int count = 0;

	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
	case AX206_FIRMWARE_3_5_x:
		/* Do as windows does, read the status word 64 times,
		   then check */
		count = 64;
		break;
	case AX3003_FIRMWARE_3_5_x:
		/* On the ax3003 continuously reading the status word
		   does not work. */
		count = 1;
		break;
	}

	while (1) {
		CHECK (ax203_send_eeprom_cmd (camera, 0, &cmd, 1,
					      buf, count, 0))
		/* We only need to check the last read */
		if (!(buf[count - 1] & 0x01)) /* Check write in progress bit */
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
							  buf + i, 256, 0))
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

/* This function reads the parameter block from the eeprom and:
   1) checks some hopefully constant bytes to make sure it is not reading
      garbage
   2) Gets the lcd size from the parameter block (ax203_get_lcd_size seems
      to result in all frames being detected as being 128x128 pixels).
   3) Determines the compression type for v3.4.x frames
   4) Determines the start of the ABFS
*/
static int ax203_read_parameter_block(Camera *camera)
{
	uint8_t buf[32], expect[32];
	int i, param_offset = 0, resolution_offset = 0;
	int compression_offset = -1, abfs_start_offset = 0, expect_size = 0;
	const unsigned int valid_resolutions[][2] = {
		{  96,  64 },
		{ 120, 160 },
		{ 128, 128 },
		{ 132, 132 },
		{ 128, 160 },
		{ 160, 120 },
		{ 160, 128 },
		{ 240, 320 },
		{ 320, 240 },
		{ }
	};
	const uint8_t expect_33x[] = {
		0x13, 0x15, 0, 0, 0x02, 0x01, 0x02, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	const uint8_t expect_34x[] = {
		0x13, 0x15, 0, 0, 0, 0, 0, 0x01,
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	const uint8_t expect_35x[] = {
		0x00, 0x00, 0, 0, 0, 0, 0, 0xd8 };

	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
		param_offset = 0x50;
		resolution_offset  = 2;
		abfs_start_offset  = 16;
		memcpy (expect, expect_33x, sizeof(expect_33x));
		expect_size = sizeof(expect_33x);
		/* Byte 4 of the parameter block probably is the compression
		   type like with v3.4.x, but this needs confirmation. */
		camera->pl->compression_version = AX203_COMPRESSION_YUV;
		break;
	case AX203_FIRMWARE_3_4_x:
		param_offset = 0x50;
		resolution_offset  = 2;
		compression_offset = 6;
		abfs_start_offset  = 16;
		memcpy (expect, expect_34x, sizeof(expect_34x));
		expect_size = sizeof(expect_34x);
		break;
	case AX206_FIRMWARE_3_5_x:
		param_offset = 0x20;
		abfs_start_offset  = 2;
		resolution_offset  = 3;
		memcpy (expect, expect_35x, sizeof(expect_35x));
		expect_size = sizeof(expect_35x);
		/* ax206 + 3.5.x firmware has a fixed compression type */
		camera->pl->compression_version = AX206_COMPRESSION_JPEG;
		break;
	case AX3003_FIRMWARE_3_5_x:
		i = ax3003_get_frame_id (camera);
		if (i < 0) return i;
		switch (i) {
		case 0:
		case 1:
			camera->pl->width  = 320;
			camera->pl->height = 240;
			break;
		default:
			gp_log (GP_LOG_ERROR, "ax203",
				"unknown ax3003 frame id: %d", i);
			return GP_ERROR_MODEL_NOT_FOUND;
		}

		i = ax3003_get_abfs_start (camera);
		if (i < 0) return i;
		camera->pl->fs_start = i;

		camera->pl->compression_version = AX3003_COMPRESSION_JPEG;

		goto verify_parameters;
	}

	CHECK (ax203_read_mem (camera, param_offset, buf, sizeof(buf)))

	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
		/* 1 byte width / height */
		camera->pl->width  = buf[resolution_offset    ];
		camera->pl->height = buf[resolution_offset + 1];
		expect[resolution_offset    ] = camera->pl->width;
		expect[resolution_offset + 1] = camera->pl->height;
		break;
	case AX203_FIRMWARE_3_4_x:
	case AX206_FIRMWARE_3_5_x:
		/* 2 byte little endian width / height */
		camera->pl->width  = le16atoh (buf + resolution_offset);
		camera->pl->height = le16atoh (buf + resolution_offset + 2);
		htole16a(expect + resolution_offset    , camera->pl->width);
		htole16a(expect + resolution_offset + 2, camera->pl->height);
		break;
	case AX3003_FIRMWARE_3_5_x:
		/* Never reached, here to silence a compiler warning */
		break;
	}

	if (compression_offset != -1) {
		i = buf[compression_offset];
		switch (i) {
		case 2:
			camera->pl->compression_version =
				AX203_COMPRESSION_YUV;
			break;
		case 3:
			camera->pl->compression_version =
				AX203_COMPRESSION_YUV_DELTA;
			break;
		default:
			gp_log (GP_LOG_ERROR, "ax203",
				"unknown compression version: %d", i);
			return GP_ERROR_MODEL_NOT_FOUND;
		}
		expect[compression_offset] = i;
	}

	i = buf[abfs_start_offset];
	camera->pl->fs_start = i * 0x10000;
	expect[abfs_start_offset] = i;

	if (memcmp (buf, expect, expect_size)) {
		gp_log (GP_LOG_ERROR, "ax203",
			"unexpected contents of parameter block");
		return GP_ERROR_MODEL_NOT_FOUND;
	}

verify_parameters:
	for (i = 0; valid_resolutions[i][0]; i++)
		if (valid_resolutions[i][0] == camera->pl->width &&
		    valid_resolutions[i][1] == camera->pl->height)
			break;

	if (!valid_resolutions[i][0]) {
		gp_log (GP_LOG_ERROR, "ax203", "unknown resolution: %dx%d",
			camera->pl->width, camera->pl->height);
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	CHECK (ax203_read_mem (camera, camera->pl->fs_start, buf, 4))
	if (memcmp (buf, AX203_ABFS_MAGIC, 4)) {
		gp_log (GP_LOG_ERROR, "ax203", "ABFS magic not found at: %x",
			camera->pl->fs_start);
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	GP_DEBUG ("lcd size %dx%d, compression ver: %d, fs-start: %x",
		  camera->pl->width, camera->pl->height,
		  camera->pl->compression_version, camera->pl->fs_start);

	return GP_OK;
}

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
			"file index beyond end of ABFS");
		return GP_ERROR_BAD_PARAMETERS;
	}

	return GP_OK;
}

int
ax203_filesize(Camera *camera)
{
	switch (camera->pl->compression_version) {
	case AX203_COMPRESSION_YUV:
		return camera->pl->width * camera->pl->height;
	case AX203_COMPRESSION_YUV_DELTA:
		return camera->pl->width * camera->pl->height * 3 / 4;
	case AX206_COMPRESSION_JPEG:
	case AX3003_COMPRESSION_JPEG:
		/* Variable size */
		return 0;
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;
}

static int
ax203_max_filecount(Camera *camera)
{
	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		return (AX203_ABFS_SIZE - AX203_ABFS_FILE_OFFSET (0)) / 2;
	case AX206_FIRMWARE_3_5_x:
		return (AX203_ABFS_SIZE - AX206_ABFS_FILE_OFFSET (0)) /
			sizeof(struct ax206_v3_5_x_raw_fileinfo);
	case AX3003_FIRMWARE_3_5_x:
		return (AX203_ABFS_SIZE - AX3003_ABFS_FILE_OFFSET (0)) /
			sizeof(struct ax3003_v3_5_x_raw_fileinfo);
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;
}

static int
ax203_read_v3_3_x_v3_4_x_filecount(Camera *camera)
{
	/* The v3.3.x and v3.4.x firmware ABFS keeps a single byte file count
	   at (camera->pl->fs_start + AX203_ABFS_COUNT_OFFSET) However the
	   frame does not seem to care about this.
	   So always return the maximum number of entries the ABFS can contain
	   (and rely on fileinfo.present to figure out how many files there
	    actually are). */
	return ax203_max_filecount (camera);
}

static int
ax203_write_v3_3_x_v3_4_x_filecount(Camera *camera, int count)
{
	uint8_t c = count;

	/* Despite the frame not caring, we do write the filecount just
	   like windows does. */
	CHECK (ax203_write_mem (camera,
				camera->pl->fs_start + AX203_ABFS_COUNT_OFFSET,
				&c, 1))
	return GP_OK;
}

static int
ax203_read_v3_5_x_filecount(Camera *camera)
{
	/* The v3.5.x firmware ABFS does not contain a separate file count,
	   so always return the maximum number of entries it can contain
	   (and rely on fileinfo.present to figure out how many files there
	    actually are). */
	return ax203_max_filecount (camera);
}

static int
ax203_write_v3_5_x_filecount(Camera *camera, int count)
{
	/* This is a no-op as the v3.5.x firmware ABFS does not contain a
	   separate file count */
	return GP_OK;
}

static
int ax203_read_v3_3_x_v3_4_x_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	uint8_t buf[2];

	CHECK (ax203_read_mem (camera,
			       camera->pl->fs_start +
					AX203_ABFS_FILE_OFFSET (idx),
			       buf, 2))

	/*fileinfo->address = le16atoh (buf) << 8;*/
	fileinfo->address = (buf[0] | (buf[1] << 8)) << 8;
	fileinfo->size    = ax203_filesize (camera);
	fileinfo->present = fileinfo->address? 1 : 0;

	return GP_OK;
}

static
int ax203_write_v3_3_x_v3_4_x_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	uint8_t buf[2];

	if (fileinfo->address & 0xff) {
		gp_log (GP_LOG_ERROR, "ax203", "LSB of address is not 0");
		return GP_ERROR_BAD_PARAMETERS;
	}

	if (!fileinfo->present)
		fileinfo->address = 0;

	/*htole16a(buf, fileinfo->address >> 8);*/
	buf[0] = (fileinfo->address >> 8) & 0xff;
	buf[1] = (fileinfo->address >>16) & 0xff;
	CHECK (ax203_write_mem (camera,
				camera->pl->fs_start +
					AX203_ABFS_FILE_OFFSET (idx),
				buf, 2))

	return GP_OK;
}

static
int ax206_read_v3_5_x_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	struct ax206_v3_5_x_raw_fileinfo raw;

	CHECK (ax203_read_mem (camera,
			       camera->pl->fs_start +
					AX206_ABFS_FILE_OFFSET (idx),
			       &raw, sizeof(raw)))

	fileinfo->present = raw.present == 0x01;
	fileinfo->address = le32toh (raw.address);
	fileinfo->size    = le16toh (raw.size);

	return GP_OK;
}

static
int ax206_write_v3_5_x_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	struct ax206_v3_5_x_raw_fileinfo raw;

	raw.present = fileinfo->present;
	raw.address = htole32(fileinfo->address);
	raw.size    = htole16(fileinfo->size);

	CHECK (ax203_write_mem (camera,
				camera->pl->fs_start +
					AX206_ABFS_FILE_OFFSET (idx),
				&raw, sizeof(raw)))

	return GP_OK;
}

static
int ax3003_read_v3_5_x_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	struct ax3003_v3_5_x_raw_fileinfo raw;

	CHECK (ax203_read_mem (camera,
			       camera->pl->fs_start +
					AX3003_ABFS_FILE_OFFSET (idx),
			       &raw, sizeof(raw)))

	if (raw.address == 0xffff || raw.size == 0xffff) {
		memset(fileinfo, 0, sizeof(*fileinfo));
		return GP_OK;
	}

	fileinfo->present = raw.address && raw.size;
	fileinfo->address = be16toh (raw.address) * 0x100;
	fileinfo->size    = be16toh (raw.size) * 0x100;

	return GP_OK;
}

static
int ax3003_write_v3_5_x_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	struct ax3003_v3_5_x_raw_fileinfo raw;

	if (fileinfo->address & 0xff) {
		gp_log (GP_LOG_ERROR, "ax203", "LSB of address is not 0");
		return GP_ERROR_BAD_PARAMETERS;
	}

	if (fileinfo->size & 0xff) {
		gp_log (GP_LOG_ERROR, "ax203", "LSB of size is not 0");
		return GP_ERROR_BAD_PARAMETERS;
	}

	if (fileinfo->present) {
		raw.address = htobe16(fileinfo->address / 0x100);
		raw.size    = htobe16(fileinfo->size / 0x100);
	} else {
		raw.address = 0;
		raw.size    = 0;
	}

	CHECK (ax203_write_mem (camera,
				camera->pl->fs_start +
					AX3003_ABFS_FILE_OFFSET (idx),
				&raw, sizeof(raw)))

	return GP_OK;
}

/***************** Begin "public" functions *****************/

int
ax203_read_filecount(Camera *camera)
{
	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		return ax203_read_v3_3_x_v3_4_x_filecount (camera);
	case AX206_FIRMWARE_3_5_x:
	case AX3003_FIRMWARE_3_5_x:
		return ax203_read_v3_5_x_filecount (camera);
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;
}

static int
ax203_update_filecount(Camera *camera)
{
	int i, max, count = 0;

	/* "Calculate" the real file count */
	max = ax203_max_filecount (camera);
	for (i = 0; i < max; i++) {
		if (ax203_file_present (camera, i))
			count = i + 1;
	}

	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		return ax203_write_v3_3_x_v3_4_x_filecount (camera, count);
	case AX206_FIRMWARE_3_5_x:
	case AX3003_FIRMWARE_3_5_x:
		return ax203_write_v3_5_x_filecount (camera, count);
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;
}

static
int ax203_read_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	CHECK (ax203_check_file_index (camera, idx))

	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		return ax203_read_v3_3_x_v3_4_x_fileinfo (camera, idx,
							  fileinfo);
	case AX206_FIRMWARE_3_5_x:
		return ax206_read_v3_5_x_fileinfo (camera, idx, fileinfo);
	case AX3003_FIRMWARE_3_5_x:
		return ax3003_read_v3_5_x_fileinfo (camera, idx, fileinfo);
	}
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;
}

static
int ax203_write_fileinfo(Camera *camera, int idx,
	struct ax203_fileinfo *fileinfo)
{
	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		return ax203_write_v3_3_x_v3_4_x_fileinfo (camera, idx,
							   fileinfo);
	case AX206_FIRMWARE_3_5_x:
		return ax206_write_v3_5_x_fileinfo (camera, idx, fileinfo);
	case AX3003_FIRMWARE_3_5_x:
		return ax3003_write_v3_5_x_fileinfo (camera, idx, fileinfo);
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
#ifdef HAVE_LIBGD
	int ret;
	unsigned int x, y, width, height, row_skip = 0;
	unsigned char *components[3];
#ifdef HAVE_LIBJPEG
	struct jpeg_decompress_struct dinfo;
	struct jpeg_error_mgr jderr;
	JSAMPLE row[camera->pl->width * 3];
	JSAMPROW row_pointer[1] = { row };
#endif

	switch (camera->pl->compression_version) {
	case AX203_COMPRESSION_YUV:
		ax203_decode_yuv (src, dest, camera->pl->width,
				  camera->pl->height);
		return GP_OK;
	case AX203_COMPRESSION_YUV_DELTA:
		ax203_decode_yuv_delta (src, dest, camera->pl->width,
					camera->pl->height);
		return GP_OK;
	case AX206_COMPRESSION_JPEG:
		/* Note this uses a modified tinyjpeg which was modified to
		   handle the AX206' custom JPEG format (the AX3003 uses
		   normal JPEG compression). */
		if (!camera->pl->jdec) {
			camera->pl->jdec = tinyjpeg_init ();
			if (!camera->pl->jdec)
				return GP_ERROR_NO_MEMORY;
		}

		/* Hack for width / heights which are not a multiple of 16 */
		if (camera->pl->width % 16 || camera->pl->height % 16) {
			width  = (camera->pl->width  + 15) & ~15;
			height = (camera->pl->height + 15) & ~15;
			htobe16a(src,     width);
			htobe16a(src + 2, height);
			row_skip = (width - camera->pl->width) * 3;
		}
		ret = tinyjpeg_parse_header (camera->pl->jdec,
					     (unsigned char *)src, src_size);
		if (ret) {
			gp_log (GP_LOG_ERROR, "ax203",
				"Error parsing header: %s",
				tinyjpeg_get_errorstring (camera->pl->jdec));
			return GP_ERROR_CORRUPTED_DATA;
		}
		if (!row_skip) {
			tinyjpeg_get_size (camera->pl->jdec, &width, &height);
			if (width  != camera->pl->width ||
			    height != camera->pl->height) {
				gp_log (GP_LOG_ERROR, "ax203",
					"Hdr dimensions %ux%u don't match lcd %dx%d",
					width, height,
					camera->pl->width, camera->pl->height);
				return GP_ERROR_CORRUPTED_DATA;
			}
		}
		ret = tinyjpeg_decode (camera->pl->jdec);
		if (ret) {
			gp_log (GP_LOG_ERROR, "ax203",
				"Error decoding JPEG data: %s",
				tinyjpeg_get_errorstring (camera->pl->jdec));
			return GP_ERROR_CORRUPTED_DATA;
		}
		tinyjpeg_get_components (camera->pl->jdec, components);
		for (y = 0; y < camera->pl->height; y++) {
			for (x = 0; x < camera->pl->width; x++) {
				dest[y][x] = gdTrueColor (components[0][0],
							  components[0][1],
							  components[0][2]);
				components[0] += 3;
			}
			components[0] += row_skip;
		}
		return GP_OK;
	case AX3003_COMPRESSION_JPEG:
#ifdef HAVE_LIBJPEG
		dinfo.err = jpeg_std_error (&jderr);
		jpeg_create_decompress (&dinfo);
		jpeg_mem_src (&dinfo, (unsigned char *)src, src_size);
		jpeg_read_header (&dinfo, TRUE);
		jpeg_start_decompress (&dinfo);
		if (dinfo.output_width  != camera->pl->width ||
		    dinfo.output_height != camera->pl->height ||
		    dinfo.output_components != 3 ||
		    dinfo.out_color_space != JCS_RGB) {
			gp_log (GP_LOG_ERROR, "ax203",
				"Wrong JPEG header parameters: %dx%d, "
				"%d components, colorspace: %d",
				dinfo.output_width, dinfo.output_height,
				dinfo.output_components,
				dinfo.out_color_space);
			return GP_ERROR_CORRUPTED_DATA;
		}

		for (y = 0; y < dinfo.output_height; y++) {
			jpeg_read_scanlines (&dinfo, row_pointer, 1);
			for (x = 0; x < dinfo.output_width; x++) {
				dest[y][x] = gdTrueColor (row[x * 3 + 0],
							  row[x * 3 + 1],
							  row[x * 3 + 2]);
			}
		}
		jpeg_finish_decompress (&dinfo);
		jpeg_destroy_decompress (&dinfo);
		return GP_OK;
#else
		gp_log(GP_LOG_ERROR,"ax203", "jpeg decompression not supported - no libjpeg during build");
		return GP_ERROR_NOT_SUPPORTED;
#endif
		break;
	}
#endif
	gp_log(GP_LOG_ERROR,"ax203", "GD decompression not supported - no libGD present during build");
	/* Never reached */
	return GP_ERROR_NOT_SUPPORTED;
}

/* Returns the number of bytes of dest used or a negative error code */
static int
ax203_encode_image(Camera *camera, int **src, char *dest, unsigned int dest_size)
{
#ifdef HAVE_LIBGD
	int size = ax203_filesize (camera);
#ifdef HAVE_LIBJPEG
	unsigned int x, y;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jcerr;
	JOCTET *jpeg_dest = NULL;
	unsigned long jpeg_size = 0;
	JSAMPLE row[camera->pl->width * 3];
	JSAMPROW row_pointer[1] = { row };
#endif

	if (size < GP_OK)
		return size;

	if (dest_size < (unsigned int)size)
		return GP_ERROR_FIXED_LIMIT_EXCEEDED;

	switch (camera->pl->compression_version) {
	case AX203_COMPRESSION_YUV:
		ax203_encode_yuv (src, dest, camera->pl->width,
				  camera->pl->height);
		return size;
	case AX203_COMPRESSION_YUV_DELTA:
		ax203_encode_yuv_delta (src, dest, camera->pl->width,
					camera->pl->height);
		return size;
	case AX206_COMPRESSION_JPEG:
#if defined(HAVE_LIBGD) && defined(HAVE_LIBJPEG)
		return ax206_compress_jpeg (camera, src,
					    (uint8_t *)dest, dest_size,
					    camera->pl->width,
					    camera->pl->height);
#else
		return GP_ERROR_NOT_SUPPORTED;
#endif
	case AX3003_COMPRESSION_JPEG:
#ifdef HAVE_LIBJPEG
		cinfo.err = jpeg_std_error (&jcerr);
		jpeg_create_compress (&cinfo);
		jpeg_mem_dest (&cinfo, &jpeg_dest, &jpeg_size);
		cinfo.image_width = camera->pl->width;
		cinfo.image_height = camera->pl->height;
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_RGB;
		jpeg_set_defaults (&cinfo);
		jpeg_start_compress (&cinfo, TRUE);
		for (y = 0; y < (unsigned int)cinfo.image_height; y++) {
			for (x = 0; x < (unsigned int)cinfo.image_width; x++) {
				int p = src[y][x];
				row[x * 3 + 0] = gdTrueColorGetRed(p);
				row[x * 3 + 1] = gdTrueColorGetGreen(p);
				row[x * 3 + 2] = gdTrueColorGetBlue(p);
			}
			jpeg_write_scanlines (&cinfo, row_pointer, 1);
		}
		jpeg_finish_compress (&cinfo);
		jpeg_destroy_compress (&cinfo);

		if (jpeg_size > dest_size) {
			free (jpeg_dest);
			gp_log (GP_LOG_ERROR, "ax203",
				"JPEG is bigger then buffer");
			return GP_ERROR_FIXED_LIMIT_EXCEEDED;
		}
		memcpy (dest, jpeg_dest, jpeg_size);
		free (jpeg_dest);
		/* Round size up to a multiple of 256 because of ax3003
		   abfs size granularity. */
                return (jpeg_size + 0xff) & ~0xff;
#else
		return GP_ERROR_NOT_SUPPORTED;
#endif
	}
	/* Never reached */
#endif
	gp_log(GP_LOG_ERROR,"ax203", "GD decompression not supported - no libGD present during build");
	return GP_ERROR_NOT_SUPPORTED;
}

static int
ax203_defrag_memory(Camera *camera)
{
	char **raw_pictures;
       	struct ax203_fileinfo *fileinfo;
	int i, count, ret = GP_OK;

	count = ax203_read_filecount (camera);
	if (count < 0) return count;

	raw_pictures = calloc (count, sizeof (char *));
	fileinfo     = calloc (count, sizeof (struct ax203_fileinfo));
	if (!raw_pictures || !fileinfo) {
		free (raw_pictures); free (fileinfo);
		gp_log (GP_LOG_ERROR, "ax203", "allocating memory");
		return GP_ERROR_NO_MEMORY;
	}

	/* First read all pictures in raw format */
	for (i = 0; i < count; i++) {
		ret = ax203_read_fileinfo (camera, i, &fileinfo[i]);
		if (ret < 0) goto cleanup;

		if (!fileinfo[i].present)
			continue;

		ret = ax203_read_raw_file(camera, i, &raw_pictures[i]);
		if (ret < 0) goto cleanup;
	}

	/* Delete all pictures from the frame */
	ret = ax203_delete_all (camera);
	if (ret < 0) goto cleanup;

	/* An last write them back (in one continuous block) */
	for (i = 0; i < count; i++) {
		if (!fileinfo[i].present)
			continue;

		ret = ax203_write_raw_file (camera, i, raw_pictures[i],
					    fileinfo[i].size);
		if (ret < 0) {
			gp_log (GP_LOG_ERROR, "ax203",
				"AAI error writing back images during "
				"defragmentation some images will be lost!");
			break;
		}
	}
cleanup:
	for (i = 0; i < count; i++)
		free (raw_pictures[i]);
	free (raw_pictures);
	free (fileinfo);

	return ret;
}

int
ax203_read_raw_file(Camera *camera, int idx, char **raw)
{
	struct ax203_fileinfo fileinfo;
	int ret;

	*raw = NULL;

	CHECK (ax203_read_fileinfo (camera, idx, &fileinfo))

	if (!fileinfo.present) {
		gp_log (GP_LOG_ERROR, "ax203",
			"trying to read a deleted file");
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Allocate 1 extra byte as tinyjpeg's huffman decoding sometimes
	   reads a few bits more then it needs */
	*raw = malloc (fileinfo.size + 1);
	if (!*raw) {
		gp_log (GP_LOG_ERROR, "ax203", "allocating memory");
		return GP_ERROR_NO_MEMORY;
	}

	ret = ax203_read_mem (camera, fileinfo.address, *raw, fileinfo.size);
	if (ret < 0) {
		free (*raw);
		*raw = NULL;
		return ret;
	}

	return fileinfo.size;
}

int
ax203_read_file(Camera *camera, int idx, int **rgb24)
{
	int ret;
	char *src;

	ret = ax203_read_raw_file (camera, idx, &src);
	if (ret < 0) return ret;

	ret = ax203_decode_image (camera, src, ret + 1, rgb24);
	free(src);

	return ret;
}

/* ax203_write_file() helper functions */

/* The picture table in ABFS does not necessarily lists the pictures
   in the order they are stored in memory, but when looking for a hole
   in memory to store a new picture we need a list of used memory blocks
   in memory order. So we build one here. */
static int
ax203_fileinfo_cmp(const void *p1, const void *p2)
{
	const struct ax203_fileinfo *f1 = p1;
	const struct ax203_fileinfo *f2 = p2;
	return f1->address - f2->address;
}

static int
ax203_build_used_mem_table(Camera *camera, struct ax203_fileinfo *table)
{
	int i, count, found = 0;
       	struct ax203_fileinfo fileinfo;

	count = ax203_read_filecount (camera);
	if (count < 0) return count;

	/* The beginning of the memory is used by the CD image and stuff */
	fileinfo.address = 0;
	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		fileinfo.size = camera->pl->fs_start + AX203_PICTURE_OFFSET;
		break;
	case AX206_FIRMWARE_3_5_x:
		fileinfo.size = camera->pl->fs_start + AX206_PICTURE_OFFSET;
		break;
	case AX3003_FIRMWARE_3_5_x:
		fileinfo.size = camera->pl->fs_start + AX3003_PICTURE_OFFSET;
		break;
	}
	fileinfo.present = 1;
	table[found++] = fileinfo;

	for (i = 0; i < count; i++) {
		CHECK (ax203_read_fileinfo (camera, i, &fileinfo))
		if (!fileinfo.present)
			continue;
		table[found++] = fileinfo;
	}
	qsort (table, found, sizeof(struct ax203_fileinfo),
	       ax203_fileinfo_cmp);

	/* Add a last memory used region which starts at the end of memory
	   the size does not matter as this is the last entry. */
	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
	case AX206_FIRMWARE_3_5_x:
		fileinfo.address = camera->pl->mem_size;
		break;
	case AX3003_FIRMWARE_3_5_x:
		fileinfo.address = camera->pl->mem_size - AX3003_BL_SIZE;
		break;
	}

	fileinfo.size    = 0;
	fileinfo.present = 1;
	table[found++] = fileinfo;

	return found;
}

/* Find a free slot in the ABFS table to store the fileinfo */
static int
ax203_find_free_abfs_slot(Camera *camera)
{
	int i, max;
	struct ax203_fileinfo fileinfo;

	max = ax203_max_filecount (camera);
	for (i = 0; i < max; i++) {
		CHECK (ax203_read_fileinfo (camera, i, &fileinfo))
		if (!fileinfo.present)
			return i;
	}

	gp_log (GP_LOG_ERROR, "ax203", "no free slot in ABFS ??");
	return GP_ERROR_NO_SPACE;
}

int
ax203_write_raw_file(Camera *camera, int idx, char *buf, int size)
{
	struct ax203_fileinfo fileinfo;
	struct ax203_fileinfo used_mem[AX203_ABFS_SIZE / 2];
	int i, hole_size, used_mem_count, prev_end, free;

retry:
	used_mem_count = ax203_build_used_mem_table (camera, used_mem);
	if (used_mem_count < 0) return used_mem_count;

	/* Try to find a large enough "hole" in the memory */
	for (i = 1, free = 0; i < used_mem_count; i++, free += hole_size) {
		prev_end = used_mem[i - 1].address + used_mem[i - 1].size;
		hole_size = used_mem[i].address - prev_end;
		if (hole_size)
			GP_DEBUG ("found a hole at: %08x, of %d bytes "
				  "(need %d)\n", prev_end, hole_size, size);
		if (hole_size >= size) {
			/* bingo we have a large enough hole */
			fileinfo.address = prev_end;
			fileinfo.size    = size;
			fileinfo.present = 1;
			CHECK (ax203_write_fileinfo (camera, idx,
						     &fileinfo))
			CHECK (ax203_update_filecount (camera))
			CHECK (ax203_write_mem (camera, fileinfo.address,
						buf, size))
			return GP_OK;
		}
	}

	if (free >= size) {
		gp_log (GP_LOG_DEBUG, "ax203",
			"not enough continuous freespace to add file, "
			"defragmenting memory");
		CHECK (ax203_defrag_memory (camera))
		goto retry;
	}

	gp_log (GP_LOG_ERROR, "ax203", "not enough freespace to add file");
	return GP_ERROR_NO_SPACE;
}

int
ax203_write_file(Camera *camera, int **rgb24)
{
	const int buf_size = camera->pl->width * camera->pl->height;
	char buf[buf_size];
	int size, abfs_slot;

	size = ax203_encode_image (camera, rgb24, buf, buf_size);
	if (size < 0) return size;

	abfs_slot = ax203_find_free_abfs_slot (camera);
	if (abfs_slot < 0) return abfs_slot;

	CHECK (ax203_write_raw_file (camera, abfs_slot, buf, size))

	return GP_OK;
}

int
ax203_delete_file(Camera *camera, int idx)
{
	struct ax203_fileinfo fileinfo;

	CHECK (ax203_read_fileinfo (camera, idx, &fileinfo))

	if (!fileinfo.present) {
		gp_log (GP_LOG_ERROR, "ax203",
			"trying to delete an already deleted file");
		return GP_ERROR_BAD_PARAMETERS;
	}

	fileinfo.present = 0;
	CHECK (ax203_write_fileinfo (camera, idx, &fileinfo))
	CHECK (ax203_update_filecount (camera))

	return GP_OK;
}

int
ax203_delete_all(Camera *camera)
{
	char buf[AX203_ABFS_SIZE];
	int size, file0_offset = 0;

	switch (camera->pl->frame_version) {
	case AX203_FIRMWARE_3_3_x:
	case AX203_FIRMWARE_3_4_x:
		file0_offset = AX203_ABFS_FILE_OFFSET (0);
		break;
	case AX206_FIRMWARE_3_5_x:
		file0_offset = AX206_ABFS_FILE_OFFSET (0);
		break;
	case AX3003_FIRMWARE_3_5_x:
		file0_offset = AX3003_ABFS_FILE_OFFSET (0);
		break;
	}

	size = AX203_ABFS_SIZE - file0_offset;
	memset(buf, 0, size);
	CHECK (ax203_write_mem (camera, camera->pl->fs_start + file0_offset,
				buf, size))
	CHECK (ax203_update_filecount (camera))

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

/* The ax3003 and the ax206 with AAI capable eeproms can program 64k at once,
   this is probably done by special handling inside the firmware. */
static int
ax203_commit_block_64k_at_once(Camera *camera, int bss)
{
	int block_sector_size = SPI_EEPROM_BLOCK_SIZE / SPI_EEPROM_SECTOR_SIZE;
	int i, address = bss * SPI_EEPROM_SECTOR_SIZE;
	int checksum = 0;
	char extra_arg = 0;

	/* The ax206 (and we assume the same applies for ax203, untested!):
	   1) Needs the last byte of the scsi cmd to be 2 to enable 64k pp
	   2) Has an extra checksum function, which we might as well use. */
	if (camera->pl->frame_version != AX3003_FIRMWARE_3_5_x) {
		extra_arg = 2;
		checksum = 1;
	}

	/* Make sure we have read the entire block before erasing it !! */
	for (i = 0; i < block_sector_size; i++)
		CHECK (ax203_check_sector_present (camera, bss + i))

	if (!camera->pl->block_protection_removed) {
		CHECK (ax203_eeprom_write_enable (camera))
		CHECK (ax203_eeprom_clear_block_protection (camera))
		CHECK (ax203_eeprom_wait_ready (camera))
		camera->pl->block_protection_removed = 1;
	}

	/* Erase the block */
	CHECK (ax203_erase64k_sector (camera, bss))

	/* program the block in one large 64k page write */
	CHECK (ax203_eeprom_write_enable (camera))
	CHECK (ax203_eeprom_program_page (camera, address,
					  camera->pl->mem + address,
					  SPI_EEPROM_BLOCK_SIZE, extra_arg))
	CHECK (ax203_eeprom_wait_ready (camera))

	/* and ask the device to verify the write with a checksum */
	if (checksum) {
		checksum = 0;
		for (i = address; i < (address + SPI_EEPROM_BLOCK_SIZE); i++)
			checksum += ((uint8_t *)camera->pl->mem)[i];
		checksum &= 0xffff;

		i = ax203_get_checksum(camera, address, SPI_EEPROM_BLOCK_SIZE);
		if (i < 0)
			return i;
		if (i != checksum) {
			gp_log (GP_LOG_ERROR, "ax203",
				"checksum mismatch after programming "
				"expected %04x, got %04x\n", checksum, i);
			return GP_ERROR_IO;
		}
	}

	for (i = 0; i < block_sector_size; i++)
		camera->pl->sector_dirty[bss + i] = 0;

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
	   contains dirty sectors, decide whether to use 4k sector erase
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

		if (camera->pl->pp_64k)
			CHECK (ax203_commit_block_64k_at_once (camera, i))
		/* There are 16 4k sectors per 64k block, when we need to
		   program 12 or more sectors, programming the entire block
		   becomes faster */
		else if (dirty_sectors < 12 && camera->pl->has_4k_sectors)
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

	CHECK (ax203_read_parameter_block (camera))

	if ((camera->pl->width % 4) || (camera->pl->height % 4)) {
		gp_log (GP_LOG_ERROR, "ax203",
			"lcd width and height must be a multiple of 4");
		return GP_ERROR_IO;
	}

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
	/*id = le32atoh((uint8_t *)buf);*/
	id =	 (uint8_t)buf[0]	|
		((uint8_t)buf[1] << 8)	|
		((uint8_t)buf[2] << 16)	|
		((uint8_t)buf[3] << 24);
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
	camera->pl->pp_64k         = ax203_eeprom_info[i].pp_64k;
	if (camera->pl->frame_version == AX3003_FIRMWARE_3_5_x)
		camera->pl->pp_64k = 1;
	GP_DEBUG (
		"%s EEPROM found, capacity: %d, has 4k sectors: %d, pp_64k %d",
		ax203_eeprom_info[i].name, camera->pl->mem_size,
		camera->pl->has_4k_sectors, camera->pl->pp_64k);

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

int
ax203_get_free_mem_size(Camera *camera)
{
	struct ax203_fileinfo used_mem[AX203_ABFS_SIZE / 2];
	int i, used_mem_count, prev_end, free = 0;

	used_mem_count = ax203_build_used_mem_table (camera, used_mem);
	if (used_mem_count < 0) return used_mem_count;

	/* Add the size of all holes in memory together */
	for (i = 1; i < used_mem_count; i++) {
		prev_end = used_mem[i - 1].address + used_mem[i - 1].size;
		free += used_mem[i].address - prev_end;
	}

	return free;
}
