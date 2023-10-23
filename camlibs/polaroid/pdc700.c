/* pdc700.c
 *
 * Copyright 2001 Lutz Mueller
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

#define _DEFAULT_SOURCE

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>

#include "libgphoto2/i18n.h"


#define GP_MODULE "pdc700"

#define PDC700_INIT	0x01
#define PDC700_INFO	0x02
#define PDC700_CONFIG   0x03
#define PDC700_BAUD	0x04
#define PDC700_PICINFO	0x05
#define PDC700_THUMB	0x06
#define PDC700_PIC	0x07
#define PDC700_DEL	0x09
#define PDC700_CAPTURE  0x0a

#define RETRIES 5

enum _PDCStatus {
	PDC_STATUS_FAIL = 0x00,
	PDC_STATUS_DONE = 0x01,
	PDC_STATUS_LAST = 0x02
};
typedef enum _PDCStatus PDCStatus;

enum _PDCConf {
	PDC_CONF_FLASH    = 0x00,
	PDC_CONF_TIMER    = 0x01,
	PDC_CONF_CAPTION  = 0x02,
	PDC_CONF_LCD      = 0x03,
	PDC_CONF_QUALITY  = 0x04,
	PDC_CONF_TIME     = 0x05,
	PDC_CONF_POWEROFF = 0x06,
	PDC_CONF_SIZE     = 0x07
	/* I think we have them all... 8 and 9 return failure */
};
typedef enum _PDCConf PDCConf;

enum _PDCBaud {
	PDC_BAUD_9600   = 0x00,
	PDC_BAUD_19200  = 0x01,
	PDC_BAUD_38400  = 0x02,
	PDC_BAUD_57600  = 0x03,
	PDC_BAUD_115200 = 0x04
};
typedef enum _PDCBaud PDCBaud;

enum _PDCBool {
	PDC_BOOL_OFF = 0,
	PDC_BOOL_ON  = 1
};
typedef enum _PDCBool PDCBool;

typedef struct _PDCDate PDCDate;
struct _PDCDate {

	/*
	 * v2.45: Years since 1980
	 * V3.10: Years since 2000
	 */
	unsigned char year, month, day;
	unsigned char hour, minute, second;
};

enum _PDCMode {
	PDC_MODE_PLAY   = 0,
	PDC_MODE_RECORD = 1,
	PDC_MODE_MENU   = 2
};
typedef enum _PDCMode PDCMode;

enum _PDCQuality {
	PDC_QUALITY_NORMAL    = 0,
	PDC_QUALITY_FINE      = 1,
	PDC_QUALITY_SUPERFINE = 2
};
typedef enum _PDCQuality PDCQuality;

enum _PDCSize {
	PDC_SIZE_VGA = 0,
	PDC_SIZE_XGA = 1,
};
typedef enum _PDCSize PDCSize;

enum _PDCFlash {
	PDC_FLASH_AUTO = 0,
	PDC_FLASH_ON   = 1,
	PDC_FLASH_OFF  = 2
};
typedef enum _PDCFlash PDCFlash;

typedef struct _PDCInfo PDCInfo;
struct _PDCInfo {
	unsigned int num_taken, num_free;
	unsigned char auto_poweroff;
	char version[6];
	unsigned char memory;
	PDCDate date;
	PDCMode mode;
	PDCQuality quality;
	PDCSize size;
	PDCFlash   flash;
	PDCBaud    speed;
	PDCBool caption, timer, lcd, ac_power;
};

typedef struct _PDCPicInfo PDCPicInfo;
struct _PDCPicInfo {
	char version[6];
	unsigned int pic_size, thumb_size;
	unsigned char flash;
};

#define IMAGE_QUALITY _("Image Quality")
#define IMAGE_SIZE    _("Image Size")
#define FLASH_SETTING _("Flash Setting")
#define LCD_STATE     _("LCD")
#define SELF_TIMER    _("Self Timer")
#define AUTO_POWEROFF _("Auto Power Off (minutes)")
#define SHOW_CAPTIONS _("Information")

static const char *quality[] = {N_("normal"), N_("fine"), N_("superfine"),
				NULL};
static const char *flash[]   = {N_("auto"), N_("on"), N_("off"), NULL};
static const char *bool[]    = {N_("off"), N_("on"), NULL};
static const char *mode[]    = {N_("play"), N_("record"), N_("menu"), NULL};
static const char *power[]   = {N_("battery"), N_("a/c adaptor"), NULL};
/* no real need to translate those ... */
static const char *speed[]   = {"9600", "19200", "38400", "57600", "115200", NULL};
static const char *size[]    = {"VGA (640x480)", "XGA (1024x768", NULL};

#define CR(result) {int __r=(result);if(__r<0) return (__r);}
#define CRF(result,d)      {int r=(result);if(r<0) {free(d);return(r);}}

#define PDC_EPOCH(info) ((!strcmp ((info)->version, "v2.45")) ? 1980 : 2000)

/*
 * Every command sent to the camera begins with 0x40 followed by two bytes
 * indicating the number of following bytes. Then follows the byte that
 * indicates the command (see above defines), perhaps some parameters. The
 * last byte is the checksum, the sum of all bytes between length bytes
 * (3rd one) and the last one (checksum).
 */
static int
calc_checksum (unsigned char *cmd, unsigned int len)
{
	unsigned int i;
	unsigned char checksum;

	for (checksum = 0, i = 0; i < len; i++)
		checksum += cmd[i];

	return (checksum);
}

static int
pdc700_send (Camera *camera, unsigned char *cmd, unsigned int cmd_len)
{
	/* Finish the command and send it */
	cmd[0] = 0x40;
	cmd[1] = (cmd_len - 3) >> 8;
	cmd[2] = (cmd_len - 3) & 0xff;
	cmd[cmd_len - 1] = calc_checksum (cmd + 3, cmd_len - 1 - 3);
	CR (gp_port_write (camera->port, (char *)cmd, cmd_len));

	return (GP_OK);
}

static int
pdc700_read (Camera *camera, unsigned char *cmd,
	     unsigned char *b, unsigned int *b_len,
	     PDCStatus *status, unsigned char *sequence_number,
	     GPContext *context)
{
	unsigned char header[3], checksum;
	unsigned int i;

	/*
	 * Read the header (0x40 plus 2 bytes indicating how many bytes
	 * will follow)
	 */
	CR (gp_port_read (camera->port, (char *)header, 3));
	if (header[0] != 0x40) {
		gp_context_error (context, _("Received unexpected "
				     "header (%i)"), header[0]);
		return (GP_ERROR_CORRUPTED_DATA);
	}
	*b_len = (header[2] << 8) | header [1];
	if (*b_len > 2048) {
		GP_DEBUG ("length %d too large", *b_len);
		return GP_ERROR_CORRUPTED_DATA;
	}

	/* Read the remaining bytes */
	CR (gp_port_read (camera->port, (char *)b, *b_len));

	/*
	 * The first byte indicates if this the response for our command.
	 */
	if (b[0] != (0x80 | cmd[3])) {
		gp_context_error (context, _("Received unexpected response"));
		return (GP_ERROR_CORRUPTED_DATA);
	}

	/* Will other packets follow? Has the transaction been successful? */
	*status = b[1];

	/*
	 * If everything went ok and if we are downloading a picture or
	 * thumbnail, we got a sequence number (number of next packet).
	 */
	if ((*status != PDC_STATUS_FAIL) && ((cmd[3] == PDC700_THUMB) ||
					     (cmd[3] == PDC700_PIC)))
		*sequence_number = b[2];
	else
		sequence_number = NULL;

	/* Check the checksum */
	for (checksum = i = 0; i < *b_len - 1; i++)
		checksum += b[i];
	if (checksum != b[*b_len - 1]) {
		gp_context_error (context, _("Checksum error"));
		return (GP_ERROR_CORRUPTED_DATA);
	}

	/* Preserve only the actual data */
	*b_len -= (sequence_number ? 4 : 3);
	memmove (b, b + (sequence_number ? 3 : 2), *b_len);

	return (GP_OK);
}

static int
pdc700_transmit (Camera *camera, unsigned char *cmd, unsigned int cmd_len,
		 unsigned char *buf, unsigned int *buf_len, GPContext *context)
{
	unsigned char b[2048], n;
	unsigned int b_len, r;
	unsigned int target = *buf_len, id;
	int result;
	PDCStatus status;

	status = PDC_STATUS_DONE;
	for (r = 0; r < RETRIES; r++) {
		if (status == PDC_STATUS_FAIL)
			GP_DEBUG ("Retrying (%i)...", r);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return GP_ERROR_CANCEL;
		CR (pdc700_send (camera, cmd, cmd_len));
		CR (pdc700_read (camera, cmd, b, &b_len, &status, &n, context));
		if (status != PDC_STATUS_FAIL)
			break;
	}
	if (status == PDC_STATUS_FAIL) {
		gp_context_error (context, _("The camera did not accept the "
				     "command."));
		return (GP_ERROR);
	}

	/* Copy over the data */
	*buf_len = b_len;
	memcpy (buf, b, b_len);

	/*
	 * Other packets will follow only in case of PDC700_THUMB or
	 * PDC700_PIC.
	 */
	if ((cmd[3] == PDC700_THUMB) || (cmd[3] == PDC700_PIC)) {

		/* Get those other packets */
		r = 0;
		id = gp_context_progress_start (context, target,
						_("Downloading..."));
		while ((status != PDC_STATUS_LAST) && (r < RETRIES)) {
			GP_DEBUG ("Fetching sequence %i...", n);
			cmd[4] = status;
			cmd[5] = n;
			CR (pdc700_send (camera, cmd, 7));

			/* Read data. */
			result = pdc700_read (camera, cmd, b, &b_len,
					      &status, &n, context);

			/* Did libgphoto2(_port) report an error? */
			if (result < 0) {
				GP_DEBUG ("Read failed ('%s'). Trying again.",
					  gp_result_as_string (result));
				r++;
				continue;
			}

			/* Did the camera report an error? */
			if (status == PDC_STATUS_FAIL) {
				GP_DEBUG ("Read failed: camera reported "
					  "failure. Trying again.");
				r++;
				continue;
			}

			/* Read succeeded. Reset error counter */
			r = 0;

			/*
			 * Sanity check: We should never read more bytes than
			 * targeted
			 */
			if (*buf_len + b_len > target) {
				gp_context_error (context, _("The camera "
					"sent more bytes than expected (%i)"),
					target);
				return (GP_ERROR_CORRUPTED_DATA);
			}

			/* Copy over the data */
			memcpy (buf + *buf_len, b, b_len);
			*buf_len += b_len;

			/*
			 * Update the progress bar and check for
			 * cancellation.
			 */
			gp_context_progress_update (context, id, *buf_len);
			if (gp_context_cancel (context) ==
						GP_CONTEXT_FEEDBACK_CANCEL) {
				cmd[4] = PDC_STATUS_LAST;
				cmd[5] = n;
				CR (pdc700_send (camera, cmd, 7));
				return (GP_ERROR_CANCEL);
			}
		}

		/* Check if anything went wrong */
		if (status != PDC_STATUS_LAST)
			return (GP_ERROR_CORRUPTED_DATA);

		/* Acknowledge last packet */
		cmd[4] = PDC_STATUS_LAST;
		cmd[5] = n;
		CR (pdc700_send (camera, cmd, 7));

		gp_context_progress_stop (context, id);
	}

	return (GP_OK);
}

static int
pdc700_baud (Camera *camera, int baud, GPContext *context)
{
	unsigned char cmd[6];
	unsigned char buf[2048];
	unsigned int buf_len = 0;

	cmd[3] = PDC700_BAUD;
	switch (baud) {
	case 115200:
		cmd[4] = PDC_BAUD_115200;
		break;
	case 57600:
		cmd[4] = PDC_BAUD_57600;
		break;
	case 38400:
		cmd[4] = PDC_BAUD_38400;
		break;
	case 19200:
		cmd[4] = PDC_BAUD_19200;
		break;
	case 9600:
		cmd[4] = PDC_BAUD_9600;
		break;
	default:
		return (GP_ERROR_IO_SERIAL_SPEED);
	}
	CR (pdc700_transmit (camera, cmd, 6, buf, &buf_len, context));

	return (GP_OK);
}

static int
pdc700_init (Camera *camera, GPContext *context)
{
	unsigned int buf_len = 0;
	unsigned char cmd[5];
	unsigned char buf[2048];

	cmd[3] = PDC700_INIT;
	CR (pdc700_transmit (camera, cmd, 5, buf, &buf_len, context));

	return (GP_OK);
}

static int
pdc700_picinfo (Camera *camera, unsigned int n, PDCPicInfo *info,
		GPContext *context)
{
	unsigned int buf_len = 0;
	unsigned char cmd[7];
	unsigned char buf[2048];

	GP_DEBUG ("Getting info about picture %i...", n);
	cmd[3] = PDC700_PICINFO;
	cmd[4] = n;
	cmd[5] = n >> 8;
	CR (pdc700_transmit (camera, cmd, 7, buf, &buf_len, context));

	/* We don't know about the meaning of buf[0-1] */

	/* Check if this information is about the right picture */
	if (n != (unsigned int)(buf[2] | (buf[3] << 8))) {
		gp_context_error (context, _("Requested information about "
			"picture %i (= 0x%x), but got information about "
			"picture %i back"), n, cmd[4] | (cmd[5] << 8),
			buf[2] | (buf[3] << 8));
		return (GP_ERROR_CORRUPTED_DATA);
	}

	/* Picture size */
	info->pic_size = buf[4] | (buf[5] << 8) |
			(buf[6] << 16) | (buf[7] << 24);
	GP_DEBUG ("Size of picture: %i", info->pic_size);

	/* Flash used? */
	info->flash = buf[8];
	GP_DEBUG ("This picture has been taken with%s flash.",
		  buf[8] ? "" : "out");

	/* The meaning of buf[9-17] is unknown */

	/* Thumbnail size */
	info->thumb_size = buf[18] | (buf[19] <<  8) | (buf[20] << 16) |
			  (buf[21] << 24);
	GP_DEBUG ("Size of thumbnail: %i", info->thumb_size);

	/* The meaning of buf[22] is unknown */

	/* Version info */
	strncpy (info->version, (char *)&buf[23], 6);

	/*
	 * Now follows some picture data we have yet to reverse
	 * engineer (buf[24-63]).
	 */

	return (GP_OK);
}

static int
pdc700_config (Camera *camera, PDCConf conf, unsigned char value, GPContext *context)
{
	unsigned char cmd[12];
	unsigned char buf[512];
	unsigned int buf_len = 0;

	cmd[3] = PDC700_CONFIG;
	cmd[4] = conf;
	cmd[5] = value;

	CR (pdc700_transmit (camera, cmd, 12, buf, &buf_len, context));

	return GP_OK;
}


static int
pdc700_info (Camera *camera, PDCInfo *info, GPContext *context)
{
	unsigned int buf_len = 0;
	unsigned char buf[2048];
	unsigned char cmd[5];

	cmd[3] = PDC700_INFO;
	CR (pdc700_transmit (camera, cmd, 5, buf, &buf_len, context));

	/*
	 * buf[0-1,3]: We don't know. The following has been seen:
	 * 01 12 .. 01
	 * 01 20 .. 02
	 * 01 20 .. 02
	 */

 	info->memory = buf[2];

	/* Power source state (make sure it's valid) */
	info->ac_power = buf[4];
	if (info->ac_power != 0 && info->ac_power != 1) {
		GP_DEBUG ("Unknown power source: %i", info->ac_power);
		info->ac_power = PDC_BOOL_OFF;
	}

 	info->auto_poweroff = buf[5];

	/* Mode (make sure we know it) */
	info->mode = buf[6];
	if (info->mode < 0 || info->mode > 2) {
		GP_DEBUG ("Unknown mode setting: %i", info->mode);
		/* record mode is the power-on default -gi */
		info->mode = PDC_MODE_RECORD;
	}

	/* Flash (make sure we know it) */
	info->flash = buf[7];
	if (info->flash < 0 || info->flash > 2) {
		GP_DEBUG ("Unknown flash setting: %i", info->flash);
		info->flash = PDC_FLASH_AUTO;
	}

	/* Protocol version */
	strncpy (info->version, (char *)&buf[8], 6);

	/* buf[14-15]: We don't know. Seems to be always 00 00 */

	/* Pictures */
	info->num_taken = buf[16] | (buf[17] << 8);
	info->num_free = buf[18] | (buf[19] << 8);

	/* Date */
	info->date.year   = buf[20];
	info->date.month  = buf[21];
	info->date.day    = buf[22];
	info->date.hour   = buf[23];
	info->date.minute = buf[24];
	info->date.second = buf[25];

	/* Speed (kind of bogus as we already know about it) */
	info->speed = buf[26];
	if (info->speed < 0 || info->speed > 4) {
		GP_DEBUG ("Unknown speed: %i", info->speed);
		info->speed = PDC_BAUD_9600;
	}

	/* Caption/Information state (make sure it's valid) */
	info->caption = buf[27];
	if (info->caption != 0 && info->caption != 1) {
		GP_DEBUG ("Unknown caption state: %i", info->caption);
		info->caption = PDC_BOOL_OFF;
	}

	/*
	 * buf[28-31]: We don't know. Below are some samples:
	 *
	 * f8 b2 64 03
	 *
	 * c6 03 86 28
	 *
	 * 3a 7f 65 83
	 *
	 * 23 25 66 83
	 */

	/* Timer state (make sure it's valid) */
	info->timer = buf[32];
	if (info->timer != 0 && info->timer != 1) {
		GP_DEBUG ("Unknown timer state %i", info->timer);
		info->timer = PDC_BOOL_OFF;
	}

	/* LCD state (make sure it's valid) */
	info->lcd = buf[33];
	if (info->lcd != 0 && info->lcd != 1) {
		GP_DEBUG ("Unknown LCD state %i", info->lcd);
		info->lcd = PDC_BOOL_OFF;
	}

	/* Quality (make sure we know it) */
	info->quality = buf[34];
	if (info->quality < 0 || info->quality > 2) {
		GP_DEBUG ("Unknown quality: %i", info->quality);
		info->quality = PDC_QUALITY_NORMAL;
	}

	/* Here follow lots of 0s */

	info->size = 0;

	return (GP_OK);
}

static int
pdc700_set_date (Camera *camera, time_t time, GPContext *context)
{
	unsigned char cmd[15];
	unsigned char buf[512];
	unsigned int buf_len = 0;
	struct tm *tm;
	PDCInfo info;

	CR (pdc700_info (camera, &info, context));

	tm = localtime(&time);

	cmd[3]  = PDC700_CONFIG;
	cmd[4]  = PDC_CONF_TIME;

	cmd[5]  = (tm->tm_year + 1900) - PDC_EPOCH(&info);
	cmd[6]  = tm->tm_mon + 1;
	cmd[7]  = tm->tm_mday;
	cmd[8]  = tm->tm_hour;
	cmd[9]  = tm->tm_min;
	cmd[10] = tm->tm_sec;

	CR (pdc700_transmit (camera, cmd, 12, buf, &buf_len, context));
	return GP_OK;
}

static int
pdc700_capture (Camera *camera, GPContext *context)
{
        unsigned char cmd[5], buf[1024];
        unsigned int buf_len = 0;
	int r = 0;
	int try;
	PDCInfo info;

        cmd[3] = PDC700_CAPTURE;
	cmd[4] = 0;

        CR (pdc700_transmit (camera, cmd, 5, buf, &buf_len, context));

	/*
	 * This is rather hackish.  The camera needs a little time to recover
	 * after taking a picture.  I tried a general-purpose retry thing in
	 * pdc_transmit, but that got in the way during initialization.  So
	 * I'm leaving this ugly-but-works junk here for now.
	 */
	for (try = 0; try < 10; try++)
	  if ((r = pdc700_info(camera, &info, context)) == GP_OK)
	    break;

        return r;
};

static int
pdc700_pic (Camera *camera, unsigned int n,
	    unsigned char **data, unsigned int *size, unsigned char thumb,
	    GPContext *context)
{
	unsigned char cmd[8];
	int r;
	PDCPicInfo info;

	/* Picture size? Allocate the memory */
	CR (pdc700_picinfo (camera, n, &info, context));
	*size = thumb ? info.thumb_size : info.pic_size;
	*data = malloc (sizeof (char) * *size);
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	/* Get picture data */
	GP_DEBUG ("Getting picture %i...", n);
	cmd[3] = (thumb) ? PDC700_THUMB : PDC700_PIC;
	cmd[4] = 0; /* No idea what that byte is for */
	cmd[5] = n;
	cmd[6] = n >> 8;
	r = pdc700_transmit (camera, cmd, 8, *data, size, context);
	if (r < 0) {
		free (*data);
		return (r);
	}

	return (GP_OK);
}

static int
pdc700_delete (Camera *camera, unsigned int n, GPContext *context)
{
	unsigned char cmd[6], buf[1024];
	unsigned int buf_len = 0;

	cmd[3] = PDC700_DEL;
	cmd[4] = n;
	CR (pdc700_transmit (camera, cmd, 6, buf, &buf_len, context));

	/*
	 * We get three bytes back but don't know the meaning of those.
	 * Perhaps some error codes with regard to read-only images?
	 */

	return (GP_OK);
}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "Polaroid DC700");

	return (GP_OK);
}

static struct {
	const char *model;
	unsigned short usb_vendor;
	unsigned short usb_product;

} models[] = {
	{"Polaroid:DC700", 0x784, 0x2888},
	{NULL, 0, 0}
};

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].model; i++) {
		memset(&a, 0, sizeof(a));
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port     = GP_PORT_SERIAL | GP_PORT_USB;
		a.usb_vendor = models[i].usb_vendor;
		a.usb_product= models[i].usb_product;
		a.speed[0] = 9600;
		a.speed[1] = 19200;
		a.speed[2] = 38400;
		a.speed[3] = 57600;
		a.speed[4] = 115200;
		a.operations        = GP_OPERATION_CAPTURE_IMAGE |
                                      GP_OPERATION_CONFIG;
		a.file_operations   = GP_FILE_OPERATION_DELETE |
		                      GP_FILE_OPERATION_PREVIEW;
		a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

		CR (gp_abilities_list_append (list, a));
	}

	return (GP_OK);
}

static void
pdc700_expand (unsigned char *src, unsigned char *dst)
{
	int Y, Y2, U, V;
	int x, y;

	for (y = 0; y < 60; y++)
		for (x = 0; x < 80; x += 2) {
			Y  = (char) src[0]; Y  += 128;
			U  = (char) src[1]; U  -= 0;
			Y2 = (char) src[2]; Y2 += 128;
			V  = (char) src[3]; V  -= 0;

			if ((Y  > -16) && (Y  < 16)) Y  = 0;
			if ((Y2 > -16) && (Y2 < 16)) Y2 = 0;
			if ((U  > -16) && (U  < 16)) U  = 0;
			if ((V  > -16) && (V  < 16)) V  = 0;

			dst[0] = Y + (1.402000 * V);
			dst[1] = Y - (0.344136 * U) - (0.714136 * V);
			dst[2] = Y + (1.772000 * U);
			dst += 3;

			dst[0] = Y2 + (1.402000 * V);
			dst[1] = Y2 - (0.344136 * U) - (0.714136 * V);
			dst[2] = Y2 + (1.772000 * U);
			dst += 3;

			src += 4;
		}
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	int count;
	char buf[1024];

	CR (pdc700_capture (camera, context));

	/*
	 * We don't get any info back. However, we need to tell the
	 * CameraFilesystem that there is one additional picture.
	 */
	CR (count = gp_filesystem_count (camera->fs, "/", context));
	snprintf (buf, sizeof (buf), "PDC700%04i.jpg", count + 1);
	CR (gp_filesystem_append (camera->fs, "/", buf, context));

	/* Now tell the frontend where to look for the image */
	strncpy (path->folder, "/", sizeof (path->folder));
	strncpy (path->name, buf, sizeof (path->name));

	return (GP_OK);
}

static int
del_file_func (CameraFilesystem *fs, const char *folder, const char *file,
	       void *data, GPContext *context)
{
	Camera *camera = data;
	int n;

	/* We need picture numbers starting with 1 */
	CR (n = gp_filesystem_number (fs, folder, file, context));
	n++;

	CR (pdc700_delete (camera, n, context));

	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
	int n;
	unsigned int size;
	unsigned char *data = NULL;

#if 0
	if (type == GP_FILE_TYPE_RAW)
		return (GP_ERROR_NOT_SUPPORTED);
#endif

	/* Get the number of the picture from the filesystem */
	CR (n = gp_filesystem_number (camera->fs, folder, filename, context));

	/* Get the file */
	CR (pdc700_pic (camera, n + 1, &data, &size,
			(type == GP_FILE_TYPE_NORMAL) ? 0 : 1, context));
	switch (type) {
	case GP_FILE_TYPE_NORMAL:

		/* Files are always JPEG */
		CRF (gp_file_set_data_and_size (file, (char *)data, size), data);
		CR (gp_file_set_mime_type (file, GP_MIME_JPEG));
		break;

	case GP_FILE_TYPE_PREVIEW:

		/*
		 * Depending on the protocol version, we have different
		 * encodings.
		 */
		if ((data[0]        == 0xff) && (data[1]        == 0xd8) &&
		    (data[size - 2] == 0xff) && (data[size - 1] == 0xd9)) {

			/* Image is JPEG */
			CRF (gp_file_set_data_and_size (file, (char *)data, size),
			     data);
			CR (gp_file_set_mime_type (file, GP_MIME_JPEG));

		} else if (size == 9600) {
			const char header[] = "P6\n80 60\n255\n";
			unsigned char *ppm;
			unsigned int ppm_size;

			/*
			 * Image is a YUF 2:1:1 format: 4 bytes for each 2
			 * pixels. Y0 U0 Y1 V0 Y2 U2 Y3 V2 Y4 U4 Y5 V4 ....
			 * So the first four bytes make up the first two
			 * pixels. You use Y0, U0, V0 to get pixel0 then Y1,
			 * U0, V0 to get pixel 2. Then onto the next 4-byte
			 * sequence.
			 */
			ppm_size = size * 3 / 2;
			ppm = malloc (sizeof (char) * ppm_size);
			if (!ppm) {
				free (data);
				return (GP_ERROR_NO_MEMORY);
			}
			pdc700_expand (data, ppm);
			free (data);
			CRF (gp_file_append (file, header, strlen (header)),
			     ppm);
			CRF (gp_file_append (file, (char *)ppm, ppm_size), ppm);
			free (ppm);
			CR (gp_file_set_mime_type (file, GP_MIME_PPM));

		} else {
			free (data);
			gp_context_error (context, _("%i bytes of an "
				"unknown image format have been received. "
				"Please write to %s and ask for "
				"assistance."), size, MAIL_GPHOTO_DEVEL);
			return (GP_ERROR);
		}
		break;

	case GP_FILE_TYPE_RAW:
#if 1
		CRF (gp_file_set_data_and_size (file, (char *)data, size), data);
		CR (gp_file_set_mime_type (file, GP_MIME_RAW));
		break;
#endif
	default:
		free (data);
		return (GP_ERROR_NOT_SUPPORTED);
	}

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Download program for Polaroid DC700 camera. "
		"Originally written by Ryan Lantzer "
		"<rlantzer@umr.edu> for gphoto-4.x. Adapted for gphoto2 by "
		"Lutz Mueller <lutz@users.sf.net>."));

	return (GP_OK);
}

/*
 * We encapsulate the process of adding an entire radio control.
 */
static void
add_radio (CameraWidget *section, const char *blurb, const char **opt,
	   int selected)
{
	CameraWidget *child;
	int i;

	gp_widget_new (GP_WIDGET_RADIO, blurb, &child);

	for (i = 0; opt[i]; i++)
		gp_widget_add_choice (child, opt[i]);

	gp_widget_set_value (child, (void *) opt[selected]);
	gp_widget_append (section, child);
}


static int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *child;
	CameraWidget *section;
	float range;
	PDCInfo info;
	time_t time;
	struct tm tm;
	int xtime;

	CR (pdc700_info (camera, &info, context));

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);

	gp_widget_new (GP_WIDGET_SECTION, _("Camera"), &section);
	gp_widget_append (*window, section);

	add_radio (section, LCD_STATE,     bool,    info.lcd);
	add_radio (section, SELF_TIMER,    bool,    info.timer);
	add_radio (section, SHOW_CAPTIONS, bool,    info.caption);

	/* Auto poweroff */
	gp_widget_new (GP_WIDGET_RANGE, AUTO_POWEROFF, &child);
	gp_widget_set_range (child, 1., 99., 1.);
	range = (float) info.auto_poweroff;
	gp_widget_set_value (child, &range);
	gp_widget_append (section, child);
	gp_widget_set_info (child, _("How long will it take until the "
				     "camera powers off?"));

	gp_widget_new (GP_WIDGET_SECTION, _("Image"), &section);
	gp_widget_append (*window, section);
	add_radio (section, IMAGE_QUALITY, quality, info.quality);
	add_radio (section, IMAGE_SIZE, size, info.size);
	add_radio (section, FLASH_SETTING, flash,   info.flash);

	gp_widget_new (GP_WIDGET_SECTION, _("Date and Time"), &section);
	gp_widget_append (*window, section);

	/* Date and time */
	tm.tm_year = info.date.year +
		((!strcmp (info.version, "v2.45")) ? 1980 : 2000) - 1900;
	tm.tm_mon = info.date.month - 1;
	tm.tm_mday = info.date.day;
	tm.tm_hour = info.date.hour;
	tm.tm_min = info.date.minute;
	tm.tm_sec = info.date.second;
	time = mktime (&tm);
	GP_DEBUG ("time: %X", (unsigned int) time);
	gp_widget_new (GP_WIDGET_DATE, _("Date and Time"), &child);
	gp_widget_append (section, child);
	xtime = time;
	gp_widget_set_value (child, &xtime);
	return GP_OK;
}

static int
which_radio_button (CameraWidget *window, const char *label,
		    const char * const *opt)
{
  	CameraWidget *child;
	int i;
	const char *value;

	if (gp_widget_get_child_by_label (window, label, &child) != GP_OK)
		return -1;

	if (!gp_widget_changed (child))
		return -1;

	gp_widget_set_changed (child, FALSE);
	gp_widget_get_value (child, &value);

	for (i = 0; opt[i]; i++)
		if (!strcmp (value, opt[i]))
			return i;

	return -1;
}

static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
  	CameraWidget *child;
	int i = 0, r;
	float range;

	if ((i = which_radio_button (window, IMAGE_QUALITY, quality)) >= 0)
		CR (pdc700_config (camera, PDC_CONF_QUALITY,
				   (unsigned char) i, context));

	if ((i = which_radio_button (window, IMAGE_SIZE, size)) >= 0)
		CR (pdc700_config (camera, PDC_CONF_SIZE,
				   (unsigned char) i, context));

	if ((i = which_radio_button (window, FLASH_SETTING, flash)) >= 0)
		CR (pdc700_config (camera, PDC_CONF_FLASH,
				   (unsigned char) i, context));

	if ((i = which_radio_button (window, LCD_STATE, bool)) >= 0)
		CR (pdc700_config (camera, PDC_CONF_LCD,
				   (unsigned char) i, context));

	if ((i = which_radio_button (window, SELF_TIMER, bool)) >= 0)
		CR (pdc700_config (camera, PDC_CONF_TIMER,
				   (unsigned char) i, context));

	if ((i = which_radio_button (window, SHOW_CAPTIONS, bool)) >= 0)
		CR (pdc700_config (camera, PDC_CONF_CAPTION,
				   (unsigned char) i, context));

	/* Auto poweroff */
	r = gp_widget_get_child_by_label (window, AUTO_POWEROFF, &child);
	if ((r == GP_OK) && gp_widget_changed (child)) {
		gp_widget_set_changed (child, FALSE);
		gp_widget_get_value (child, &range);
		CR (pdc700_config (camera, PDC_CONF_POWEROFF,
				   (unsigned char) range, context));
	}

	/* Date and time */
	r = gp_widget_get_child_by_label (window, _("Date and Time"), &child);
	if ((r == GP_OK) && gp_widget_changed (child)) {
		gp_widget_set_changed (child, FALSE);
		gp_widget_get_value (child, &i);
		if (i != -1)
		  pdc700_set_date(camera, (time_t) i, context);
		else
		  GP_DEBUG ("date widget returned -1, not setting date/time");
		/* GP_DEBUG ("Implement setting of date & time!"); */
	}


	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *about, GPContext *context)
{
	PDCInfo info;

	CR (pdc700_info (camera, &info, context));

	sprintf (about->text, _(
		"Date: %i/%02i/%02i %02i:%02i:%02i\n"
		"Pictures taken: %i\n"
		"Free pictures: %i\n"
		"Software version: %s\n"
		"Baudrate: %s\n"
		"Memory: %i megabytes\n"
		"Camera mode: %s\n"
		"Image quality: %s\n"
		"Flash setting: %s\n"
		"Information: %s\n"
		"Timer: %s\n"
		"LCD: %s\n"
		"Auto power off: %i minutes\n"
		"Power source: %s"),
		info.date.year + ((!strcmp (info.version, "v2.45")) ? 1980 :
								      2000),
		info.date.month, info.date.day,
		info.date.hour, info.date.minute, info.date.second,
		info.num_taken, info.num_free, info.version,
		_(speed[info.speed]),
		info.memory,
		_(mode[info.mode]),
		_(quality[info.quality]),
		_(flash[info.flash]),
		_(bool[info.caption]),
		_(bool[info.timer]),
		_(bool[info.lcd]),
		info.auto_poweroff,
		_(power[info.ac_power]));

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	PDCInfo info;

	/* Fill the list */
	CR (pdc700_info (camera, &info, context));
	gp_list_populate (list, "PDC700%04i.jpg", info.num_taken);

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	int n;
	Camera *camera = data;
	PDCPicInfo pic_info;

	/* Get the picture number from the CameraFilesystem */
	CR (n = gp_filesystem_number (fs, folder, file, context));

	CR (pdc700_picinfo (camera, n + 1, &pic_info, context));
	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	strcpy (info->file.type, GP_MIME_JPEG);
	strcpy (info->preview.type, GP_MIME_JPEG);
	info->file.size = pic_info.pic_size;
	info->preview.size = pic_info.thumb_size;

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.del_file_func = del_file_func
};

int
camera_init (Camera *camera, GPContext *context)
{
	int result = GP_OK, i;
	GPPortSettings settings;
	int speeds[] = {115200, 9600, 57600, 19200, 38400};

        /* First, set up all the function pointers */
	camera->functions->capture = camera_capture;
	camera->functions->summary = camera_summary;
        camera->functions->about   = camera_about;
	camera->functions->get_config = camera_get_config;
	camera->functions->set_config = camera_set_config;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	/* Check if the camera is really there */
	CR (gp_port_get_settings (camera->port, &settings));
	CR (gp_port_set_timeout (camera->port, 1000));

	switch (camera->port->type) {
	case GP_PORT_SERIAL:

		/* Figure out current speed */
		for (i = 0; i < 5; i++) {
			settings.serial.speed = speeds[i];
			CR (gp_port_set_settings (camera->port, settings));
			result = pdc700_init (camera, context);
			if (result == GP_OK)
				break;
		}
		if (i == 5)
			return (result);

		/* Set the speed to the highest one */
		if (speeds[i] < 115200) {
			CR (pdc700_baud (camera, 115200, context));
			settings.serial.speed = 115200;
			CR (gp_port_set_settings (camera->port, settings));
		}
		break;
	case GP_PORT_USB:
		/* Use the defaults the core parsed */
		CR (gp_port_set_settings (camera->port, settings));
		CR (pdc700_init (camera, context));
		break;
	default:
		gp_context_error (context, _("The requested port type (%i) "
				     "is not supported by this driver."),
				     camera->port->type);
		return (GP_ERROR_NOT_SUPPORTED);
	}

	return (GP_OK);
}
