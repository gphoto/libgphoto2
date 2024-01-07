/* quicktake1x0.c
 *
 * Copyright 2023, Colin Leroy-Mira <colin@colino.net>
 * Quicktake 100/150 protocol documentation available at
 * https://www.colino.net/wordpress/en/archives/2023/10/29/the-apple-quicktake-100-serial-communication-protocol/
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

#include "libgphoto2/i18n.h"
#include "quicktake1x0.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gphoto2/gphoto2-library.h>

#define GP_MODULE "Quicktake 1x0"

#define BUFFER_SIZE 1024
static unsigned char buffer[BUFFER_SIZE];

#define CHECK_RESULT(result) {int r = result; if (r < 0) return (r);}

/* Get an ack from the camera
 * Acks are single-byte, 0x00 in case of success, 0x02 in case of error
 */
static int
get_ack(GPPort *port)
{
	if (gp_port_read(port, (char *)buffer, 1) < 1)
		return GP_ERROR_IO;

	return buffer[0] == 0x00 ? GP_OK : GP_ERROR_IO;
}


/* Send an ack to the camera
 * Our acks are single byte, 0x06 that mean "I'm ready to get data"
 */
static int
send_ack(GPPort *port)
{
	char cmd[] = {0x06};

	return gp_port_write (port, cmd, 1) == 1 ? GP_OK : GP_ERROR_IO;
}

/* Send a command to the camera
 * cmd: the command as an hexadecimal char array
 * len: the length of the array
 * s_ack: whether the camera expects us to send an ack after we receive
 * 				its own ack
 */
static int
send_command(GPPort *port, const char *cmd, int len, int s_ack)
{
	if (gp_port_write (port, cmd, len) < GP_OK)
		return GP_ERROR_IO;

	if (get_ack(port) != GP_OK)
		return GP_ERROR_IO;

	if (s_ack)
		return send_ack(port);

	return GP_OK;
}

/* Ping the camera
 * A simple command that does nothing, and that the camera replies to
 * with an ack. Useful to make sure the camera did not go to sleep.
 */
static int
qt1x0_send_ping(GPPort *port)
{
	char cmd[] = {0x16,0x00,0x00,0x00,0x00,0x00,0x00};

	return send_command(port, cmd, sizeof cmd, 0);
}

/* Initialize the camera */
static int
qt1x0_initialize (Camera *camera)
{
	/* "Hello" command, sent at 9600bps, 8n1. At bytes 6-7, the desired serial 
	 * speed is indicated. the last byte is a checksum:
	 * (sum of the preceding bytes)&0xFF
	 */
	char hello[] = {0x5A,0xA5,0x55,0x05,0x00,0x00,0xE1,0x00,0x00,0x80,0x02,0x00,0xBC};

	/* Command that triggers the serial speed upgrade */
	char upgrade_speed[] = {0x16,0x2A,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x03,0x03,0x30,0x04,0x00};
	GPPortSettings settings;

	/* Clearing DTR wakes the camera up. */
	CHECK_RESULT(gp_port_set_pin (camera->port, GP_PIN_DTR, GP_LEVEL_LOW));
	usleep(100 * 1000);
	CHECK_RESULT(gp_port_set_pin (camera->port, GP_PIN_DTR, GP_LEVEL_HIGH));

	/* It then tells us 7 bytes that help identify it */
	if (gp_port_read (camera->port, (char *)buffer, 7) < 7)
		return GP_ERROR_MODEL_NOT_FOUND;

	if ((unsigned char)buffer[0] != 0xA5)
		return GP_ERROR_MODEL_NOT_FOUND;

	if ((unsigned char)buffer[3] == 0xC8)
		camera->pl->model = QUICKTAKE_MODEL_150;
	else
		camera->pl->model = QUICKTAKE_MODEL_100;

	/* We then send our first command to inform it of the speed we desire */
	if (gp_port_write (camera->port, hello, sizeof(hello)) < GP_OK)
		return GP_ERROR_MODEL_NOT_FOUND;

	/* The camera replies, with data we can ignore */
	if (gp_port_read (camera->port, (char *)buffer, 10) < 10)
		return GP_ERROR_MODEL_NOT_FOUND;

	/* At this point, the camera expects us to set even parity. */
	CHECK_RESULT (gp_port_get_settings (camera->port, &settings));
	settings.serial.parity = GP_PORT_SERIAL_PARITY_EVEN;
	CHECK_RESULT (gp_port_set_settings (camera->port, settings));

	/* We let it do the same. */
	usleep(1 * 1000 * 1000);

	/* Once parity is set on both sides, we ask to upgrade speed. */
	if (gp_port_write (camera->port, upgrade_speed, sizeof(upgrade_speed)) < GP_OK)
		return GP_ERROR_MODEL_NOT_FOUND;

	/* The camera acks that, */
	if (get_ack(camera->port) != GP_OK)
		return GP_ERROR_MODEL_NOT_FOUND;

	/* We ack the ack, */
	if (send_ack(camera->port) != GP_OK)
		return GP_ERROR_MODEL_NOT_FOUND;

	/* Wait a little bit of time, */
	usleep(100 * 1000);

	/* Upgrade our serial line, */
	settings.serial.speed = 57600;
	CHECK_RESULT (gp_port_set_settings (camera->port, settings));

	/* And flush a full kilobytes of 0xAA that the camera sends us,
	 * probably to verify that we're good to go.
	 */
	gp_port_read (camera->port, (char *)buffer, 1024); /* Ignored */

	/* We ack that, */
	if (send_ack(camera->port) != GP_OK)
		return GP_ERROR_MODEL_NOT_FOUND;

	/* It acks our ack, and we're ready! */
	return get_ack(camera->port);
}

/* Get information from the camera. It tells us how many pics were taken,
 * how much free space there is, its current battery level, quality mode,
 * flash mode, and date and time.
 */
static int
camera_get_info (Camera *camera)
{
	/* Command to get information from the camera.
	 * byte 3 is the requested information:
	 * - 0x00 for a thumbnail
	 * - 0x10 for a full picture
	 * - 0x21 for metadata
	 * - 0x30 for camera information
	 * byte 6 is the picture number. (0 for camera information)
	 * bytes 7 to 9 is the expected response size:
	 * - 0x000040 for metadata
	 * - 0x000080 for camera information
	 * - 0x000960 for the thumbnail
	 * - depends on the picture for the full picture. We get the full
	 *   picture size in the metadata response so we can reuse that.
	 */
	char cmd[] = {0x16,0x28,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x80,0x00};
	/* Offsets of interesting data */
	#define BATTERY_IDX    0x02
	#define NUM_PICS_IDX   0x04
	#define LEFT_PICS_IDX  0x06
	#define MONTH_IDX      0x10
	#define DAY_IDX        0x11
	#define YEAR_IDX       0x12
	#define HOUR_IDX       0x13
	#define MIN_IDX        0x14
	#define SEC_IDX        0x15
	#define FLASH_IDX      0x16
	#define QUALITY_IDX    0x1B
	#define NAME_IDX       0x2F

	/* Ping it */
	if (qt1x0_send_ping(camera->port) != GP_OK)
		return GP_ERROR_IO;

	/* Send our command */
	if (send_command(camera->port, cmd, sizeof cmd, 1) != GP_OK)
		return GP_ERROR_IO;

	/* Read the 128 bytes response */
	if (gp_port_read(camera->port, (char *)buffer, 128) != 128)
		return GP_ERROR_IO;

	/* Copy the relevant data. */
	strcpy(camera->pl->name, (char *)buffer + NAME_IDX);
	while (camera->pl->name[strlen(camera->pl->name) - 1] == ' ')
		camera->pl->name[strlen(camera->pl->name) - 1] = '\0';

	camera->pl->num_pics      = buffer[NUM_PICS_IDX];
	camera->pl->free_pics     = buffer[LEFT_PICS_IDX];
	camera->pl->battery_level = buffer[BATTERY_IDX];
	camera->pl->quality_mode  = buffer[QUALITY_IDX];
	camera->pl->flash_mode    = buffer[FLASH_IDX];
	camera->pl->year          = buffer[YEAR_IDX] + 2000;
	camera->pl->month         = buffer[MONTH_IDX];
	camera->pl->day           = buffer[DAY_IDX];
	camera->pl->hour          = buffer[HOUR_IDX];
	camera->pl->minute        = buffer[MIN_IDX];

	camera->pl->info_fetched  = 1;
	return GP_OK;
}

/* Change the name of the camera */
static int
camera_set_name(Camera *camera, const char *name)
{
	char set_name_cmd[] = {0x16,0x2a,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x22,0x00,0x02,0x20,
												 0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
												 0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
	#define SET_NAME_IDX    0x0D
	int len;

	len = strlen(name);
	if (len > 31)
		len = 31;

	if (qt1x0_send_ping(camera->port) != GP_OK)
		return GP_ERROR_IO;

	memcpy(set_name_cmd + SET_NAME_IDX, name, len);
	return send_command(camera->port, set_name_cmd, sizeof set_name_cmd, 0);
}

/* Change the date of the camera */
static int
camera_set_time(Camera *camera, int year, int month, int day, int hour, int minute)
{
	char set_time_cmd[] = {0x16,0x2A,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x01,0x06,0x00,0x00,0x00,0x00,0x00,0x00};
	#define SET_MONTH_IDX   0x0D
	#define SET_DAY_IDX     0x0E
	#define SET_YEAR_IDX    0x0F
	#define SET_HOUR_IDX    0x10
	#define SET_MIN_IDX     0x11
	#define SET_SEC_IDX     0x12

	if (qt1x0_send_ping(camera->port) != GP_OK)
		return GP_ERROR_IO;

	set_time_cmd[SET_DAY_IDX]   = day;
	set_time_cmd[SET_MONTH_IDX] = month;
	set_time_cmd[SET_YEAR_IDX]  = year % 100;
	set_time_cmd[SET_HOUR_IDX]  = hour;
	set_time_cmd[SET_MIN_IDX]   = minute;
	set_time_cmd[SET_SEC_IDX]   = 0;
	return send_command(camera->port, set_time_cmd, sizeof set_time_cmd, 0);
}

/* Change the flash mode of the camera */
static int
camera_set_flash_mode(Camera *camera, Quicktake1x0FlashMode flash_mode)
{
	char set_flash_cmd[] = {0x16,0x2A,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x07,0x01,0x00};
	#define SET_FLASH_IDX   0x0D

	set_flash_cmd[SET_FLASH_IDX] = flash_mode;
	return send_command(camera->port, set_flash_cmd, sizeof set_flash_cmd, 0);
}

/* Change the quality_mode of the camera */
static int
camera_set_quality_mode(Camera *camera, QuickTake1x0Quality quality_mode)
{
	char set_quality_cmd[] = {0x16,0x2A,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x06,0x02,0x10,0x00};
	#define SET_QUALITY_IDX 0x0D

	set_quality_cmd[SET_QUALITY_IDX] = quality_mode == QUALITY_HIGH ? 0x10 : 0x20;
	return send_command(camera->port, set_quality_cmd, sizeof set_quality_cmd, 0);
}

static time_t
camera_get_time(Camera *camera)
{
	struct tm tm_time;

	/* Build a UNIX representation of the picture creation time */
	tm_time.tm_year   = camera->pl->year - 1900;
	tm_time.tm_mon    = camera->pl->month - 1;
	tm_time.tm_mday   = camera->pl->day;
	tm_time.tm_hour   = camera->pl->hour;
	tm_time.tm_min    = camera->pl->minute;
	tm_time.tm_sec    = 0;
	tm_time.tm_isdst  = -1;

	return mktime(&tm_time);
}

static int
camera_trigger_capture (Camera *camera, GPContext *context)
{
	char cmd[] = {0x16,0x1B,0x00,0x00,0x00,0x00,0x00};

	/* Ping, */
	if (qt1x0_send_ping(camera->port) != GP_OK)
		return GP_ERROR_IO;

	/* Command, */
	if (send_command(camera->port, cmd, sizeof cmd, 0) != GP_OK)
		return GP_ERROR_IO;

	/* And update our information to keep track of the
	 * taken/free pics counters
	 */
	return camera_get_info(camera);
}

/* Command the camera to capture an image. */
static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	if (camera_trigger_capture(camera, context) != GP_OK)
		return GP_ERROR_IO;

	strcpy (path->folder, "/");
	sprintf (path->name, "Image_%02i.ppm", camera->pl->num_pics);

	CHECK_RESULT (gp_filesystem_append (camera->fs, "/", path->name, context));

	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;

	memset (&a, 0, sizeof(a));
	strcpy (a.model, "Apple QuickTake 1x0");
	a.status		= GP_DRIVER_STATUS_PRODUCTION;
	a.port     		= GP_PORT_SERIAL;
	a.speed[0]		= 57600;
	a.speed[1]		= 0;
	a.operations		= GP_OPERATION_CAPTURE_IMAGE|GP_OPERATION_CONFIG;
	a.file_operations	= GP_FILE_OPERATION_RAW|GP_FILE_OPERATION_PREVIEW;
	a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;
	CHECK_RESULT (gp_abilities_list_append (list, a));

	return GP_OK;
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	/* Translators: please write 'M"uller' and 'Mei"sner' (that
		 is, with u-umlaut and eszett resp.) if your charset
		 allows it.  If not, use "Mueller" and "Meissner".  */
	strcpy (about->text, _("The Apple QuickTake 1x0 driver has been written "
				"by Colin Leroy-Mira <colin@colino.net>.\n"
				"It handles the QuickTake 100, 100 plus and 150 models.\n"
				"\n"
				"It can fetch information, thumbnails, raw and ppm data, and "
			  "command the camera to take pictures.")
		);

	return GP_OK;
}

static const char *
model_to_str(int model)
{
	switch(model) {
	case QUICKTAKE_MODEL_100:
		return "QuickTake 100";
	case QUICKTAKE_MODEL_150:
		return "QuickTake 150";
	default:
		return "unknown";
	}
}

static const char *
quality_to_str(int mode)
{
	switch(mode) {
	case QUALITY_STANDARD:
		return _("Standard");
	case QUALITY_HIGH:
		return _("High");
	default:
		return _("Unknown");
	}
}

static const char *
flash_to_str(int mode)
{
	switch(mode) {
	case FLASH_AUTO:
		return _("Automatic");
	case FLASH_OFF:
		return _("Disabled");
	case FLASH_ON:
		return _("Forced");
	default:
		return _("Unknown");
	}
}

/* Show the information we gathered */
static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	if (!camera->pl->info_fetched &&
			camera_get_info(camera) != GP_OK)
		return GP_ERROR_IO;

	sprintf(summary->text,
					"Camera model:         %s\n"
					"Camera name:          %s\n"
					"Pictures taken:       %d\n"
					"Available space:      %d\n"
					"\n"
					"Battery level:        %d%%\n"
					"Current quality mode: %s\n"
					"Current flash mode:   %s\n"
					"Camera date and time: %04d/%02d/%02d %02d:%02d\n",

					model_to_str(camera->pl->model),
					camera->pl->name,
					camera->pl->num_pics,
					camera->pl->free_pics,
					camera->pl->battery_level,
					quality_to_str(camera->pl->quality_mode),
					flash_to_str(camera->pl->flash_mode),
					camera->pl->year,
					camera->pl->month,
					camera->pl->day,
					camera->pl->hour,
					camera->pl->minute);

	return GP_OK;
}

/* Get the list of files on the camera, that is, images 1 to N */
static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;

	if (!camera->pl->info_fetched &&
			camera_get_info(camera) != GP_OK)
		return GP_ERROR_IO;

	return gp_list_populate (list, "Image_%02i.ppm", camera->pl->num_pics);
}

#define BLOCK_SIZE 		 512
#define WH_OFFSET      0x220
#define DATA_OFFSET    0x2E0

/* Camera "metadata" command. Elicits a 64-byte response containing
 * metadata about the picture N: width, height, size, creation time,
 * quality mode, flash mode.
 */
static int
get_metadata (GPPort *port, int n, unsigned char *data)
{
	/* Command to get information from the camera (see camera_get_info
   * for value details).
	 */
	char cmd[] = {0x16,0x28,0x00,0x21,0x00,0x00,0x01,0x00,0x00,0x40,0x00};
	#define PNUM_IDX        0x06
	#define PSIZE_IDX       0x07

	#define IMG_NUM_IDX     0x03
	#define IMG_SIZE_IDX    0x05
	#define IMG_WIDTH_IDX   0x08
	#define IMG_HEIGHT_IDX  0x0A
	#define IMG_MONTH_IDX   0x0D
	#define IMG_DAY_IDX     0x0E
	#define IMG_YEAR_IDX    0x0F
	#define IMG_HOUR_IDX    0x10
	#define IMG_MINUTE_IDX  0x11
	#define IMG_SECOND_IDX  0x12
	#define IMG_FLASH_IDX   0x13
	#define IMG_QUALITY_IDX 0x18

	cmd[PNUM_IDX] = n;

	if (qt1x0_send_ping(port) != GP_OK)
		return GP_ERROR_IO;

	if (send_command(port, cmd, sizeof cmd, 1) != GP_OK)
		return GP_ERROR_IO;

	if (gp_port_read(port, (char *)data, 64) != 64)
		return GP_ERROR_IO;

	return (GP_OK);
}

static int
width_from_metadata(const unsigned char *buffer)
{
	return (buffer[IMG_WIDTH_IDX + 1]) + (buffer[IMG_WIDTH_IDX] << 8);
}

static int
height_from_metadata(const unsigned char *buffer)
{
	return (buffer[IMG_HEIGHT_IDX + 1]) + (buffer[IMG_HEIGHT_IDX] << 8);
}

static int
size_from_metadata(const unsigned char *buffer)
{
	return (buffer[IMG_SIZE_IDX] << 16)
					+ (buffer[IMG_SIZE_IDX + 1] << 8)
					+ (buffer[IMG_SIZE_IDX + 2]);
}

static time_t
mtime_from_metadata(const unsigned char *buffer)
{
	struct tm tm_time;

	/* Build a UNIX representation of the picture creation time */
	tm_time.tm_year   = buffer[IMG_YEAR_IDX] + 100;
	tm_time.tm_mon    = buffer[IMG_MONTH_IDX] - 1;
	tm_time.tm_mday   = buffer[IMG_DAY_IDX];
	tm_time.tm_hour   = buffer[IMG_HOUR_IDX];
	tm_time.tm_min    = buffer[IMG_MINUTE_IDX];
	tm_time.tm_sec    = buffer[IMG_SECOND_IDX];
	tm_time.tm_isdst  = 0;

	return mktime(&tm_time);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
							 CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;

	CHECK_RESULT (n = gp_filesystem_number (fs, folder, file, context));
	/* The camera counts from 1. */
	n++;

	if (get_metadata(camera->port, n, buffer) != GP_OK)
		return GP_ERROR_IO;

	/* Build the file information */
	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_WIDTH |
											GP_FILE_INFO_HEIGHT | GP_FILE_INFO_TYPE |
											GP_FILE_INFO_MTIME;
	info->file.width  = width_from_metadata(buffer);
	info->file.height = height_from_metadata(buffer);
	info->file.mtime  = mtime_from_metadata(buffer);
	info->file.size   = qtk_ppm_size(info->file.width, info->file.height);
	strcpy (info->file.type, GP_MIME_PPM);

	/* Build the preview information */
	info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_WIDTH |
												 GP_FILE_INFO_HEIGHT | GP_FILE_INFO_TYPE;
	info->preview.width  = QT1X0_THUMB_WIDTH;
	info->preview.height = QT1X0_THUMB_HEIGHT;
	info->preview.size   = qtk_ppm_size(QT1X0_THUMB_WIDTH, QT1X0_THUMB_HEIGHT);
	strcpy (info->preview.type, GP_MIME_PPM);

	return (GP_OK);
}

/* Read (long) data from the camera. Wraps around gp_port_read, as the
 * camera expects us to tell it to continue every 512 bytes.
 */
static int
receive_data(GPPort *port, unsigned char *data, int size)
{
	int i;

	for (i = 0; i < (size / BLOCK_SIZE); i++) {
		/* Read, */
		if (gp_port_read(port, (char *)data, BLOCK_SIZE) != BLOCK_SIZE)
			return GP_ERROR_IO;

		data += BLOCK_SIZE;

		/* ack, */
		send_ack(port);
	}

	/* and read the last partial block */
	size %= BLOCK_SIZE;
	if (gp_port_read(port, (char *)data, size) != size)
		return GP_ERROR_IO;

	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
							 CameraFileType type, CameraFile *file, void *user_data,
							 GPContext *context)
{
	/* Commands to get information from the camera (see camera_get_info
   * for value details).
	 */
	char full_photo_cmd[] = {0x16,0x28,0x00,0x10,0x00,0x00,0x01,0x00,0x70,0x80,0x00};
	char thumbnail_cmd[]  = {0x16,0x28,0x00,0x00,0x00,0x00,0x01,0x00,0x09,0x60,0x00};

	Camera *camera = user_data;
	int n, raw_size, file_size, width = 0, height = 0;
	unsigned char *data;
	char *cmd;
	const char *mime_type;
	time_t mtime;

	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, filename, context));
	/* The camera counts from 1 */
	n++;

	/* Get metadata */
	if (get_metadata(camera->port, n, buffer) != GP_OK)
		return GP_ERROR_IO;

	mtime = mtime_from_metadata(buffer);

	/* Set parameters according to the user's request */
	if (type == GP_FILE_TYPE_PREVIEW) {
		cmd = thumbnail_cmd;

		mime_type = GP_MIME_PPM;
		raw_size  = QT1X0_THUMB_SIZE;
		file_size = qtk_ppm_size(QT1X0_THUMB_WIDTH, QT1X0_THUMB_HEIGHT);

	} else if (type == GP_FILE_TYPE_RAW) {
		cmd = full_photo_cmd;

		mime_type = camera->pl->model == QUICKTAKE_MODEL_100 ? GP_MIME_QTKT : GP_MIME_QTKN;
		raw_size  = size_from_metadata(buffer);
		file_size = raw_size + DATA_OFFSET;

	} else if (type == GP_FILE_TYPE_NORMAL) {
		cmd = full_photo_cmd;

		mime_type = GP_MIME_PPM;
		width     = width_from_metadata(buffer);
		height    = height_from_metadata(buffer);
		raw_size  = size_from_metadata(buffer);
		file_size = qtk_ppm_size(width, height);

	} else
		return GP_ERROR_NOT_SUPPORTED;

	/* Set the command's parameters */
	cmd[PNUM_IDX]      = n;
	cmd[PSIZE_IDX]     = (raw_size >> 16) & 0xFF;
	cmd[PSIZE_IDX + 1] = (raw_size >> 8)  & 0xFF;
	cmd[PSIZE_IDX + 2] = (raw_size >> 0)  & 0xFF;

	data = malloc(file_size);
	if (data == NULL)
		return GP_ERROR_NO_MEMORY;

	memset(data, 0, file_size);

	/* Send the command to the camera */
	if (send_command(camera->port, cmd, sizeof full_photo_cmd, 1) != GP_OK)
		return GP_ERROR_IO;

	if (type == GP_FILE_TYPE_PREVIEW) {
		unsigned char *ppm_data;

		/* Get the data */
		CHECK_RESULT(receive_data(camera->port, data, raw_size));

		/* And decode the thumbnail to PPM */
		CHECK_RESULT(qtk_thumbnail_decode(data, &ppm_data, camera->pl->model));

		free(data);
		data = ppm_data;

	} else if (type == GP_FILE_TYPE_RAW) {
		/* Build the .qtk file header */
		qtk_raw_header(data, camera->pl->model == QUICKTAKE_MODEL_100 ? "qktk":"qktn");
	 	memcpy(data + 0x12, buffer + 4, 60);
		data[WH_OFFSET]     = buffer[IMG_HEIGHT_IDX];
		data[WH_OFFSET + 1] = buffer[IMG_HEIGHT_IDX + 1];
		data[WH_OFFSET + 2] = buffer[IMG_WIDTH_IDX];
		data[WH_OFFSET + 3] = buffer[IMG_WIDTH_IDX + 1];

		/* And get data after the header */
		CHECK_RESULT(receive_data(camera->port, data + DATA_OFFSET, raw_size));

	} else if (type == GP_FILE_TYPE_NORMAL) {
		unsigned char *ppm_data;

		/* Get the data */
		CHECK_RESULT(receive_data(camera->port, data, raw_size));

		/* And decode it to PPM */
		if (camera->pl->model == QUICKTAKE_MODEL_100) {
			CHECK_RESULT(qtkt_decode(data, width, height, &ppm_data));
		} else {
			CHECK_RESULT(qtkn_decode(data, width, height, &ppm_data));
		}
		free(data);
		data = ppm_data;

	} else
		return GP_ERROR_NOT_SUPPORTED;

	/* Finalize the file */
	CHECK_RESULT (gp_file_set_mtime (file, mtime));
	CHECK_RESULT (gp_file_set_mime_type (file, mime_type));
	CHECK_RESULT (gp_file_set_data_and_size (file, (char*)data, file_size));

	return GP_OK;
}

/* Command to delete all files from the camera */
static int
delete_all_func (CameraFilesystem *fs, const char* folder, void *data,
								 GPContext *context)
{
	char cmd[] = {0x16,0x29,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

	Camera *camera = data;
	if (strcmp (folder, "/"))
		return (GP_ERROR_DIRECTORY_NOT_FOUND);

	if (qt1x0_send_ping(camera->port) != GP_OK) {
		return GP_ERROR_IO;
	}

	return send_command(camera->port, cmd, sizeof cmd, 1);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.delete_all_func = delete_all_func,
};

static int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *child;
	time_t t;

	if (!camera->pl->info_fetched &&
			camera_get_info(camera) != GP_OK)
		return GP_ERROR_IO;

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);

	gp_widget_new (GP_WIDGET_TEXT, _("Camera name"), &child);
	gp_widget_set_name (child, "camera_name");
	gp_widget_set_value (child, camera->pl->name);
	gp_widget_append (*window, child);

	gp_widget_new (GP_WIDGET_RADIO, _("Image quality"), &child);
	gp_widget_set_name (child, "quality_mode");
	gp_widget_add_choice (child, quality_to_str(QUALITY_STANDARD));
	gp_widget_add_choice (child, quality_to_str(QUALITY_HIGH));
	gp_widget_set_value (child, quality_to_str(camera->pl->quality_mode));
	gp_widget_append (*window, child);

	gp_widget_new (GP_WIDGET_RADIO, _("Flash"), &child);
	gp_widget_set_name (child, "flash_mode");
	gp_widget_add_choice (child, flash_to_str(FLASH_ON));
	gp_widget_add_choice (child, flash_to_str(FLASH_OFF));
	gp_widget_add_choice (child, flash_to_str(FLASH_AUTO));
	gp_widget_set_value (child, flash_to_str(camera->pl->flash_mode));
	gp_widget_append (*window, child);

	gp_widget_new (GP_WIDGET_DATE, _("Camera date"), &child);
	gp_widget_set_name (child, "camera_date");
	t = camera_get_time(camera);
	gp_widget_set_value (child, &t);
	gp_widget_append (*window, child);


	return GP_OK;
}

static int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *widget;
	const char *val;

	CHECK_RESULT(gp_widget_get_child_by_name (window, "camera_name", &widget));
	if (gp_widget_changed(widget)) {
		CHECK_RESULT(gp_widget_get_value(widget, &val));
		CHECK_RESULT(camera_set_name(camera, val));
	}

	CHECK_RESULT(gp_widget_get_child_by_name (window, "quality_mode", &widget));
	if (gp_widget_changed(widget)) {
		CHECK_RESULT(gp_widget_get_value(widget, &val));
		if (!strcmp(val, quality_to_str(QUALITY_HIGH))) {
			CHECK_RESULT(camera_set_quality_mode(camera, QUALITY_HIGH));
		} else {
			CHECK_RESULT(camera_set_quality_mode(camera, QUALITY_STANDARD));
		}
	}

	CHECK_RESULT(gp_widget_get_child_by_name (window, "flash_mode", &widget));
	if (gp_widget_changed(widget)) {
		CHECK_RESULT(gp_widget_get_value(widget, &val));
		if (!strcmp(val, flash_to_str(FLASH_ON))) {
			CHECK_RESULT(camera_set_flash_mode(camera, FLASH_ON));
		} else if (!strcmp(val, flash_to_str(FLASH_OFF))) {
			CHECK_RESULT(camera_set_flash_mode(camera, FLASH_OFF));
		} else {
			CHECK_RESULT(camera_set_flash_mode(camera, FLASH_AUTO));
		}
	}

	CHECK_RESULT(gp_widget_get_child_by_name (window, "camera_date", &widget));
	if (gp_widget_changed(widget)) {
		int v;
		time_t time;
		struct tm *tm_time;
		CHECK_RESULT(gp_widget_get_value(widget, &v));
		time = (time_t)v;
		tm_time = localtime(&time);
		CHECK_RESULT(camera_set_time(camera,
										tm_time->tm_year + 1900, tm_time->tm_mon + 1,
										tm_time->tm_mday, tm_time->tm_hour, tm_time->tm_min));
	}

	return camera_get_info(camera);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "Apple QuickTake 1x0");
	return GP_OK;
}

int camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;

	GP_DEBUG("Init QuickTake 1x0...");
	/* First, set up all the function pointers */
	camera->functions->about             = camera_about;
	camera->functions->summary           = camera_summary;
	camera->functions->capture           = camera_capture;
	camera->functions->trigger_capture   = camera_trigger_capture;
	camera->functions->exit              = camera_exit;
	camera->functions->get_config        = camera_config_get;
	camera->functions->set_config        = camera_config_set;

	camera->pl = calloc (1, sizeof (CameraPrivateLibrary));

	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	camera->pl->model = QUICKTAKE_MODEL_UNKNOWN;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	/* Initial serial settings */
	CHECK_RESULT (gp_port_get_settings (camera->port, &settings));
	settings.serial.speed   = 9600;
	settings.serial.bits    = 8;
	settings.serial.parity  = GP_PORT_SERIAL_PARITY_OFF;
	settings.serial.stopbits= 1;
	CHECK_RESULT (gp_port_set_settings (camera->port, settings));
	CHECK_RESULT (gp_port_set_timeout (camera->port, 20000));
	/* Open the port and check if the camera is there */
	CHECK_RESULT (qt1x0_initialize (camera));
	return (GP_OK);
}
