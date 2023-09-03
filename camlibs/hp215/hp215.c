/*****************************************************************************
 *  Copyright (C) 2002  Jason Surprise <thesurprises1@attbi.com>             *
 *                2003  Enno Bartels   <ennobartels@t-online.de>             *
 *		  2005  Marcus Meissner <marcus@jet.franken.de>              *
 *****************************************************************************
 *                                                                           *
 *  This program is free software; you can redistribute it and/or            *
 *  modify it under the terms of the GNU Library General Public              *
 *  License as published by the Free Software Foundation; either             *
 *  version 2 of the License, or (at your option) any later version.         *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
 *  Library General Public License for more details.                         *
 *                                                                           *
 *  You should have received a copy of the GNU Library General Public        *
 *  License along with this program; see the file COPYING.LIB.  If not,      *
 *  write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301  USA
 *                                                                           *
 *****************************************************************************/


/*
    Old Maintainer : Enno Bartels <ennobartels@t-online.de> in 2002/2003
    Original author : Jason Surprise <thesurprises1@attbi.com> in 2002.
    Checksum author: Roberto Ragusa <r.ragusa@libero.it> in 2002/2003.

    Ported to libgphoto2: Marcus Meissner <marcus@jet.franken.de> in 2005.

    Camera commands:
     Global commands
                           Bytes   | 0    1cmd 2    3    4    5    6    7    8    9    10   11   12   13   14   15
    -------------------------------+----------------------------------------------------------------------------------
     init camera              8    | 0x02 0xce 0x80 0x8a 0x84 0x8d 0x83 0x03
     get camera date/time     8    | 0x02 0xc1 0x80 0x8b 0x84 0x8e 0x8d 0x03
     Get number of pics      16    | 0x02 0xc6 0x88 0x80 0x80 0x83 0x84 0x38 0x2f 0x30 0x32 0x88 0x84 0x8e 0x8b 0x03
     Delete all pics         12    | 0x02 0xb1 0x84 0x8f 0x8f 0x8f 0x8f 0x86 0x80 0x8a 0x86 0x03


     Single picture commands:
     Always 12 bytes long
                                   | 0    1cmd 2    3    4    5    6    7    8    9    10    11
    -------------------------------+---------------------------------------------------------------
     Request for a preview         | 0x02 0xb3 0x84 0x80 ...                                 0x03
     Request for a picture         | 0x02 0xb4 0x84 0x80 ...                                 0x03
     Request date/time for preview | 0x02 0xc5 0x84 0x80 ...                                 0x03
     Request for deleting one pic  | 0x02 0xb1 0x84 0x80 ...                                 0x03

     0x02 = Startcode for a command
     0x03 = Endcode   for a command
 */

#define _DEFAULT_SOURCE

#include "config.h"

#include <stdio.h>
#include <string.h>

#include <sys/types.h>

#include <fcntl.h>
#include <stdlib.h>

#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>

#include "libgphoto2/i18n.h"

#include "crctab.h"
#include "hp215.h"


enum hp215_cmd {
	SET_TIME			= 0x86,
	SET_TIME_AS_STR			= 0x87,
	SET_TIME_MODE			= 0x88,
	SET_TIME_PERIOD			= 0x89,
	SET_XFER_PROTOCOL		= 0x8a,
	SET_AUTO_SHUTDOWN_TIME		= 0x8b,
	SET_PHOTO_NO_DISPLAY_MODE	= 0x8c,
	DO_CALIBRATION			= 0x8d,
	GET_CALIBRATION_PARAM		= 0x8e,
	SET_SHOOT_AUTO			= 0x90,
	SET_FLASH			= 0x91,
	SET_EXPOSURE			= 0x92,
	SET_FOCUS			= 0x93,
	SET_SHUTTER			= 0x94,
	SET_RESOLUTION			= 0x95,
	SET_COMPRESSION			= 0x96,
	SET_TIMER			= 0x97,
	SET_EXT_FLASH			= 0x98,
	SET_PIC_FORMAT			= 0x99,
	RECORD_FUNC			= 0xa2,
	SET_SHOOTING_DEBUG_MODE		= 0xa3,
	GET_INTERNAL_SHOOT_AUTO_FUNC	= 0xa4,
	SET_INTERNAL_SHOOT_PARAM	= 0xa6,
	SET_SHOOT_PARAM			= 0xa7,
	SET_REVIEW_MODE			= 0xa8,
	SET_DISPLAY_PICTURE_NO		= 0xa9,
	SET_SHOOT_MODE_BY_INDEX		= 0xaa,
	GET_SHOOT_MODE_TABLE		= 0xab,
	SET_ACTIVE_SHOOTING_MODE	= 0xac,
	TAKE_PHOTO 			= 0xb0,
	DELETE_PHOTO			= 0xb1,
	PROTECT_PHOTO			= 0xb2,
	DOWNLOAD_THUMBNAIL		= 0xb3,
	DOWNLOAD_PHOTO			= 0xb4,
	TAKE_PREVIEW			= 0xb5,
	SELF_TEST			= 0xb6,
	DISPLAY_PATTERN			= 0xb7,
	UPLOAD_PHOTO			= 0xb8,
	UPLOAD_FIRMWARE			= 0xba,
	UPDATE_FIRMWARE			= 0xbb,
	DOWNLOAD_FILE_BY_NAME		= 0xbd,
	__UPDATE_FIRMWARE		= 0xbc,
	DOWNLOAD_PHOTO_FROM_ALBUM	= 0xbe,
	GET_CAMERA_CAPS			= 0xc0,
	GET_CAMERA_CURINFO		= 0xc1,
	GET_SHOOT_MODE_2		= 0xc3,
	GET_PHOTO_INFO			= 0xc5,
	GET_ALBUM_INFO			= 0xc6,
	GET_CAMERA_READY		= 0xce,
	GET_INTERNAL_SHOOT_PARAM	= 0xcf,
	CREATE_ALBUM			= 0xd0,
	RENAME_ALBUM			= 0xd2,
	SET_ACTIVE_ALBUM		= 0xd4,
};

/*
    Every commands looks like:
    02 AA BB .... CC CC CC CC 03

    02 - STX
    AA - cmd byte
    BB - 0x80 | argumentlength
    ... - argumentlength bytes
    CC CC CC CC   16 bit CRC split into 4 bit pieces, | 0x80
    03 - ETX

    A generic CRC is done over the buffer, starting with the CMD byte (offset 1).

    buffer[7]  = ((crc>>12) & 0xf) | 0x80;
    buffer[8]  = ((crc>>8 ) & 0xf) | 0x80;
    buffer[9]  = ((crc>>4 ) & 0xf) | 0x80;
    buffer[10] = ((crc    ) & 0xf) | 0x80;

    buffer[11] = 0x03;

*/

static int
hp_gen_cmd_blob (enum hp215_cmd cmd, int bytes, unsigned char *argdata, unsigned char **buf, int *buflen)
{
	int i, crc = 0;

	*buflen = 1+1+1+bytes+4+1; /* STX, CMD, ARGLEN, arguments, 4xCRC, ETX */
	*buf    = malloc(*buflen);
	if (!*buf)
		return GP_ERROR_NO_MEMORY;

	/* store STX */
	(*buf)[0] = STX;

	/* store CMD */
	(*buf)[1] = cmd;
	if (bytes >= 0x7d) {
		gp_log (GP_LOG_ERROR, "hp215", "Using too large argument buffer %d bytes", bytes);
		free (*buf);
		*buf = NULL;
		return GP_ERROR_BAD_PARAMETERS;
	}
	/* store arglen */
	(*buf)[2] = 0x80 | bytes;

	if (bytes) {
		/* store arguments */
		memcpy ((*buf)+3, argdata, bytes);
	}

	/* generate CRC over cmd, len, and arguments */
	for (i=1;i<bytes + 3;i++)
		crc = updcrc((*buf)[i], crc);

	/* store CRC */
	(*buf)[bytes+3] = ((crc >> 12) & 0xf) | 0x80;
	(*buf)[bytes+4] = ((crc >>  8) & 0xf) | 0x80;
	(*buf)[bytes+5] = ((crc >>  4) & 0xf) | 0x80;
	(*buf)[bytes+6] = ((crc >>  0) & 0xf) | 0x80;
	/* store ETX */
	(*buf)[bytes+7] = ETX;
	return GP_OK;
}

static int
hp_gen_cmd_1_16 (enum hp215_cmd cmd, unsigned short val, unsigned char **buf, int *buflen)
{
	unsigned char argbuf[4];

	argbuf[0] = ((val&0xf000)>>12) | 0x80;
	argbuf[1] = ((val&0x0f00)>> 8) | 0x80;
	argbuf[2] = ((val&0x00f0)>> 4) | 0x80;
	argbuf[3] = ((val&0x000f)    ) | 0x80;
	return hp_gen_cmd_blob (cmd, 4, argbuf, buf, buflen);
}


static int
hp_send_ack (Camera *cam)
{
	char byte = ACK;
	int ret;

	gp_log (GP_LOG_DEBUG, "hp215", "Sending ACK ... ");
	ret = gp_port_write (cam->port, &byte, 1);
	if (ret < GP_OK)
		gp_log (GP_LOG_ERROR, "hp215", "ACK sending failed with %d", ret);
	return ret;
}


static int
hp_rcv_ack (Camera *cam, char *ackval)
{
	int           ret;
	char byte = '\0';

	gp_log (GP_LOG_DEBUG, "hp215", "Receiving ACK ... ");
	ret = gp_port_read (cam->port, &byte, 1);
	if (ret < GP_OK)
		return ret;
	if (ackval)
		*ackval = byte;
	if (byte == ACK)
		return GP_OK;
	gp_log (GP_LOG_DEBUG, "hp215", "Expected ACK, but read %02x", byte);
	return GP_ERROR_IO;
}

static int
decode_u32(unsigned char **msg, int *msglen, unsigned int *val) {
	unsigned int x = 0,i;
	*val = 0;
	for (i=0;i<8;i++) {
		if (!*msglen)
			return GP_ERROR;
		x <<= 4;
		x  |= ((**msg) & 0x0f);
		(*msg)++;
		(*msglen)--;
	}
	*val = x;
	return GP_OK;
}

static int
decode_u16(unsigned char **msg, int *msglen, unsigned short *val) {
	unsigned int x = 0,i;
	*val = 0;
	for (i=0;i<4;i++) {
		if (!*msglen)
			return GP_ERROR;
		x <<= 4;
		x  |= ((**msg) & 0x0f);
		(*msg)++;
		(*msglen)--;
	}
	*val = x;
	return GP_OK;
}

static int
hp_send_command_and_receive_blob(
	Camera *camera, unsigned char *buf, int buflen,
	unsigned char **msg, int *msglen, unsigned int *retcode
) {
	int tries, ackret, ret, replydatalen;
	unsigned char msgbuf[0x400];
	char ackval;

	tries = 0;
	*msg = NULL;
	*msglen = 0;
	for (tries = 0; tries < 3; tries++ ) {
		ret = gp_port_write (camera->port, (char*)buf, buflen);
		if (ret < GP_OK)
			return ret;
		ret = hp_rcv_ack (camera, &ackval);
		if (ret < GP_OK) {
			if (ackval == NAK)
				continue;
			return GP_ERROR_IO;
		}
		break;
	}
	gp_log( GP_LOG_DEBUG, "hp215", "Expecting reply blob");
	ret = gp_port_read (camera->port, (char*)msgbuf, sizeof(msgbuf));
	if (ret < GP_OK)
		return ret;
	ackret = hp_send_ack (camera);
	if (ackret < GP_OK)
		return ackret;
	if (msgbuf[0] != STX) {
		gp_log (GP_LOG_ERROR, "hp215", "Expected STX / 02 at begin of buffer, found %02x", msgbuf[0]);
		return GP_ERROR_IO;
	}
	if (msgbuf[ret-1] != ETX) {
		gp_log (GP_LOG_ERROR, "hp215", "Expected ETX / 03 at end of buffer, found %02x", msgbuf[ret-1]);
		return GP_ERROR_IO;
	}
	replydatalen = msgbuf[2] & 0x7f;
	if (replydatalen != ret - 8) {
		gp_log (GP_LOG_ERROR, "hp215", "Reply datablob length says %d, but just %d bytes available?", replydatalen, ret-8);
		return GP_ERROR_IO;
	}
	if (replydatalen < 2) {
		gp_log (GP_LOG_ERROR, "hp215", "Reply datablob length is smaller than retcode (%d)", replydatalen);
		return GP_ERROR_IO;
	}
	*retcode = (msgbuf[3] << 8) | msgbuf[4];

	if (msgbuf[2] == 0xff) {
		unsigned char *xmsg = msgbuf+5;
		int xmsglen = 8;
		unsigned int newlen;
		char eot;
		/* ok, overlong reply */

		ret = decode_u32(&xmsg, &xmsglen, &newlen);
		if (ret < GP_OK)
			return ret;
		replydatalen = newlen;
		*msglen = replydatalen;
		*msg = malloc (replydatalen);
		ret = gp_port_read (camera->port, (char*)*msg, replydatalen);
		if (ret < GP_OK) {
			free (*msg);
			*msg = NULL;
			return ret;
		}
		ret = gp_port_read (camera->port, &eot, 1);
		if (ret < GP_OK) {
			free (*msg);
			*msg = NULL;
			return ret;
		}
		if (ret != 1) {
			free (*msg);
			*msg = NULL;
			return GP_ERROR_IO;
		}
		if (eot != EOT) {
			free (*msg);
			*msg = NULL;
			gp_log (GP_LOG_ERROR, "hp215", "read %02x instead of expected 04", eot);
			return GP_ERROR_IO;
		}
		ackret = hp_send_ack (camera);
		if (ackret < GP_OK) {
			free (*msg);
			*msg = NULL;
			return ackret;
		}
	} else {
		/* short reply - normal mode, do not copy E0 E0  */
		*msg = malloc (replydatalen-2);
		*msglen = replydatalen-2;
		memcpy (*msg, msgbuf+5, replydatalen-2);
	}
	gp_log (GP_LOG_DEBUG, "hp215", "Read Blob: retcode is %04x", *retcode);
	GP_LOG_DATA ((char*)*msg, *msglen, "Read Blob: argument block is:");
	return GP_OK;
}

static int
hp_get_timeDate_cam (Camera *cam, char *txtbuffer, size_t txtbuffersize)
{
	int		msglen, buflen, ret;
	unsigned char	*xmsg, *msg, *buf;
	unsigned int	retcode;
	t_date		date;
	unsigned short	val16;
	unsigned int	percent, val32, freespace, unixtime, freeimages, usedimages;
	char		datebuf[15];

	gp_log (GP_LOG_DEBUG, "hp215", "Sending date/time command ... ");

	ret = hp_gen_cmd_blob (GET_CAMERA_CURINFO, 0, NULL, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (cam, buf, buflen, &msg, &msglen, &retcode);
	free (buf);
	if (ret < GP_OK)
		return ret;
	xmsg = msg;
	if (msglen < 0x59) {
		free (msg);
		gp_log (GP_LOG_ERROR, "hp215", "too short reply block, %d bytes", msglen);
		return GP_ERROR_IO;
	}

	memcpy (datebuf, msg, 15);
	gp_log (GP_LOG_DEBUG, "hp215", "0f: %02x", msg[0x0f] & 0x7f);
	gp_log (GP_LOG_DEBUG, "hp215", "10: %02x", msg[0x10] & 0x7f);

	xmsg += 0x12;
	msglen -= 0x12;
	decode_u16(&xmsg, &msglen, &val16);
	gp_log (GP_LOG_DEBUG, "hp215", "12: %04x", val16);
	gp_log (GP_LOG_DEBUG, "hp215", "16: %02x", msg[0x16] & 0x7f);
	xmsg++;msglen--;
	decode_u16(&xmsg, &msglen, &val16);
	gp_log (GP_LOG_DEBUG, "hp215", "17: %04x", val16);
	decode_u16(&xmsg, &msglen, &val16);
	gp_log (GP_LOG_DEBUG, "hp215", "1b: %04x", val16);
	percent = msg[0x1f] & 0x7f;
	xmsg++;msglen--;
	decode_u32(&xmsg, &msglen, &val32);
	gp_log (GP_LOG_DEBUG, "hp215", "20: %08x", val32);
	decode_u32(&xmsg, &msglen, &val32);
	gp_log (GP_LOG_DEBUG, "hp215", "28: %08x", val32);
	decode_u32(&xmsg, &msglen, &val32);
	gp_log (GP_LOG_DEBUG, "hp215", "30: %08x", val32);
	gp_log (GP_LOG_DEBUG, "hp215", "38: %02x", msg[0x38] & 0x7f);
	xmsg++;msglen--;
	decode_u32(&xmsg, &msglen, &unixtime);
	decode_u32(&xmsg, &msglen, &freeimages);
	decode_u32(&xmsg, &msglen, &usedimages);
	decode_u32(&xmsg, &msglen, &val32);
	gp_log (GP_LOG_DEBUG, "hp215", "51: %08x", val32);
	decode_u32(&xmsg, &msglen, &freespace);

	/* Filter time and date out of the message */
	date.day   = (msg[3]-48)*10 + (msg[4]-48);
	date.month = (msg[0]-48)*10 + (msg[1]-48);
	date.year  = 2000 + (msg[6]-48)*10 + (msg[7]-48);
	date.hour  = (msg[9]-48)*10 + (msg[10]-48);
	date.min   = (msg[12]-48)*10 + (msg[13]-48);
	free (msg);


	snprintf (txtbuffer, txtbuffersize, _("Current camera time:  %04d-%02d-%02d  %02d:%02d\nFree card memory: %d\nImages on card: %d\nFree space (Images): %d\nBattery level: %d %%."),
		date.year, date.month, date.day, date.hour, date.min,
		freespace, usedimages, freeimages, percent
	);
	/* FIXME: communication hangs here ... perhaps need to reset USB pipe */
	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	char	buffer[200];
	int	ret;

	ret = hp_get_timeDate_cam (camera, buffer, sizeof(buffer));
	if (ret < GP_OK)
		return ret;
	strcpy (summary->text, buffer);
	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("hp215\n"
				"Marcus Meissner <marcus@jet.franken.de>\n"
				"Driver to access the HP Photosmart 215 camera.\n"
				"Merged from the standalone hp215 program.\n"
				"This driver allows download of images and previews, and deletion of images.\n"
	));
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int           ret, buflen, msglen, image_no;
	unsigned char cmd, *buf, *msg;
	unsigned int  retcode;

	image_no = gp_filesystem_number(fs, folder, filename, context);
	if(image_no < 0)
		return image_no;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		cmd = DOWNLOAD_PHOTO;
		break;
	case GP_FILE_TYPE_PREVIEW:
		cmd = DOWNLOAD_THUMBNAIL;
		break;
	default:
		return GP_ERROR_BAD_PARAMETERS;
	}

	ret = hp_gen_cmd_1_16 (cmd, image_no+1, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen, &retcode);
	free (buf);
	if (ret < GP_OK)
		return ret;
	gp_file_set_mime_type (file, GP_MIME_JPEG);
	gp_file_set_data_and_size (file, (char*)msg, msglen);
	/* do not free (msg), we handed it to the lower layer right now */
	return GP_OK;
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int image_no, ret, msglen, buflen;
	unsigned char  *msg, *buf;
	unsigned int retcode;

	image_no = gp_filesystem_number(fs, folder, filename, context);
	if(image_no < 0)
		return image_no;
	ret = hp_gen_cmd_1_16 (DELETE_PHOTO, image_no+1, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen, &retcode);
	free (buf);
	if (ret < GP_OK)
		return ret;
	free (msg);
	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;
	unsigned char *buf;
	unsigned char *msg;
	unsigned int retcode;
	int      ret, buflen, msglen;

	ret = hp_gen_cmd_1_16 (DELETE_PHOTO, 0xFFFF, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen, &retcode);
	free (buf);
	if (ret < GP_OK)
		return ret;
	free (msg);
	return (GP_OK);
}


static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera		*camera = data;
	int		ret, msglen, buflen, /* offset = 13, */ image_no;
	unsigned int	val, retcode;
	unsigned char	*xmsg, *msg, *buf;

	gp_log (GP_LOG_DEBUG, "hp215", "folder %s, filename %s", folder, filename);

	image_no = gp_filesystem_number(fs, folder, filename, context);
	if(image_no < 0)
		return image_no;
	ret = hp_gen_cmd_1_16 (GET_PHOTO_INFO, image_no+1, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen, &retcode);
	free (buf);
	if (ret < GP_OK)
		return ret;

	if (msglen < 2) {
		free (msg);
		return GP_ERROR_IO;
	}

/*
0000  80 80 80 85 86 85-84 84 30 31 2f 30 32 2f  ..........01/02/
0010  30 30 20 31 34 3a 34 38-00 80 81 80 80 80 80 81  00 14:48........
0020  88 89 84 81 80 80 80 80-80 81 00 00 00 00 00 00  ................
0030  00 00 00 00 00 00      -                         ......
 */
	xmsg = msg;
	ret = decode_u32(&xmsg, &msglen, &val);
	if (ret < GP_OK) {
		free (msg);
		return ret;
	}
	memset (info, 0, sizeof(*info));
	info->file.fields = GP_FILE_INFO_SIZE;
	info->file.size = val;

	xmsg	+= 0x0f;
	msglen	-= 0x0f;
	gp_log (GP_LOG_DEBUG, "hp215", "byte0 %02x", xmsg[0]);
	gp_log (GP_LOG_DEBUG, "hp215", "byte1 %02x", xmsg[1]);
	xmsg	+= 2;
	msglen	-= 2;

	ret = decode_u32(&xmsg, &msglen, &val);
	if (ret < GP_OK) {
		free (msg);
		return ret;
	}
	info->preview.fields = GP_FILE_INFO_SIZE;
	info->preview.size = val;

	gp_log (GP_LOG_DEBUG, "hp215", "byte2 %02x", xmsg[0]);
	gp_log (GP_LOG_DEBUG, "hp215", "byte3 %02x", xmsg[1]);

	free (msg);
	return GP_OK;
#if 0
	strcpy ((camera->pic[cnt+1])->date_time, &msg[0x0d]);

	/* Calculate day,month, year, hour and minute of the photo */
	(camera->pic[cnt+1])->date.day   = (msg[3+offset]-48)*10 + (msg[4+offset]-48);
	(camera->pic[cnt+1])->date.month = (msg[0+offset]-48)*10 + (msg[1+offset]-48);
	(camera->pic[cnt+1])->date.year  = 2000 + (msg[6+offset]-48)*10 + (msg[7+offset]-48);
	(camera->pic[cnt+1])->date.hour  = (msg[ 9+offset]-48)*10 + (msg[10+offset]-48);
	(camera->pic[cnt+1])->date.min   = (msg[12+offset]-48)*10 + (msg[13+offset]-48);

	/* Get the file info here and write it into <info> */

	return (GP_OK);
#endif
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera		*camera = data;
	int		ret, msglen, buflen;
	unsigned int	retcode, count;
	unsigned char	*xmsg, *msg, *buf;

	/* Note: The original windows driver sends 0x348, and 4 bytes junk.
	 * we just send the 0x348 and it works. */
	ret = hp_gen_cmd_1_16 (GET_ALBUM_INFO, 0x348, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	gp_log (GP_LOG_DEBUG, "hp215", "Sending photo album request ... ");
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen, &retcode);
	free (buf);
	if (ret < GP_OK)
		return ret;
	xmsg	= msg + 0x20; /* skip 0x20 byte string */
	msglen	= msglen - 0x20;
	ret = decode_u32(&xmsg, &msglen, &count);
	free (msg);
	if (ret < GP_OK)
		return ret;
	return gp_list_populate(list, "image%i.jpg", count);
}

#if 0 /* This function is not used by anybody */
static int
get_shoot_mode_table (Camera *camera) {
	unsigned char *buf, *msg, *xmsg;
	int ret, buflen, msglen;
	unsigned int i, cnt, val1, val2, val3, retcode;

	ret = hp_gen_cmd_blob (GET_SHOOT_MODE_TABLE, 0, NULL, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen, &retcode);
	gp_port_set_timeout (camera->port, 10000);
	free (buf);
	if (ret < GP_OK)
		return ret;
	if (retcode != HP215_OK) {
		free (msg);
		return GP_ERROR_BAD_PARAMETERS;
	}
	xmsg = msg;
	ret = decode_u32(&xmsg, &msglen, &val1);
	if (ret < GP_OK) return ret;
	ret = decode_u32(&xmsg, &msglen, &val2);
	if (ret < GP_OK) return ret;
	ret = decode_u32(&xmsg, &msglen, &val3);
	if (ret < GP_OK) return ret;

	gp_log (GP_LOG_DEBUG, "hp215", "shootmode table 0x%08x, 0x%08x, 0x%08x", val1, val2, val3);

	cnt = val1+val2;
	for (i=0;i<cnt;i++) {
		unsigned short val;
		/* 0x35 byte every loop */
		/* 0x20 byte string */
		gp_log (GP_LOG_DEBUG , "hp215", "%d: %s", i, xmsg);
		xmsg += 0x20;
		msglen -= 0x20;
		/* 0x15 left */
		/* u4 u4 u8 u7 u4 u4 u4 u4 u4 u4 */
		gp_log (GP_LOG_DEBUG, "hp215", "%01x %01x %02x %02x %01x %01x %01x %01x %01x %01x",
			xmsg[0] & 0x0f,
			xmsg[1] & 0x0f,
			((xmsg[2] & 0xf) << 4) | (xmsg[3] & 0xf),
			xmsg[4] & 0x7f,
			xmsg[5] & 0x0f,
			xmsg[6] & 0x0f,
			xmsg[7] & 0x0f,
			xmsg[8] & 0x0f,
			xmsg[9] & 0x0f,
			xmsg[10] & 0x0f
		);
		/* u7 u7 u8 u7 u7 u16 */
		gp_log (GP_LOG_DEBUG, "hp215", "%02x %02x %02x %02x %02x",
			xmsg[11] & 0x7f,
			xmsg[12] & 0x7f,
			((xmsg[13] & 0xf) << 4) | (xmsg[14] & 0xf),
			xmsg[15] & 0x7f,
			xmsg[16] & 0x7f
		);
		xmsg += 0x11;
		msglen -= 0x11;
		decode_u16 (&xmsg, &msglen, &val);
		gp_log (GP_LOG_DEBUG, "hp215", "%04x", val);
	}

	free (msg);
	return GP_OK;
}
#endif

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	unsigned char *buf, *msg;
	int ret, buflen, msglen;
	unsigned int retcode;

	/* there is one mode where we send a 16bit 0x1 */
	ret = hp_gen_cmd_blob (TAKE_PREVIEW, 0, NULL, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	gp_port_set_timeout (camera->port, 10000);
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen, &retcode);
	free (buf);
	if (ret < GP_OK)
		return ret;
	if (retcode != HP215_OK) {
		free (msg);
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_file_set_mime_type (file, GP_MIME_JPEG);
	gp_file_set_data_and_size (file, (char*)msg, msglen);
	/* do not free (msg), we handed it to the lower layer right now */
	return ret;
}


static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	unsigned char *buf, *msg;
	int ret, buflen, msglen;
	unsigned int retcode;

/*	get_shoot_mode_table (camera); */

	/*ret = hp_gen_cmd_1_16 (TAKE_PHOTO, 1, &buf, &buflen);*/ /* download after capture */
	gp_port_set_timeout (camera->port, 60000);
	ret = hp_gen_cmd_blob (TAKE_PHOTO, 0, NULL, &buf, &buflen); /* store on camera */
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen, &retcode);
	gp_port_set_timeout (camera->port, 10000);
	free (buf);
	if (ret < GP_OK)
		return ret;
	free (msg);
	/*
	 * tell libgphoto2 where to find it by filling out the path.
	 */
	return GP_OK;
}


int
camera_id (CameraText *id) {
	strcpy(id->text, "hp215");
	return (GP_OK);
}


int
camera_abilities (CameraAbilitiesList *list) {
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "HP:PhotoSmart 215");
	a.status		= GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port			= GP_PORT_USB;
	a.usb_vendor		= 0x3f0;	/* HP */
	a.usb_product		= 0x6202;	/* Photosmart 215 */
	a.operations		= GP_OPERATION_CAPTURE_PREVIEW | GP_OPERATION_CAPTURE_IMAGE;
	a.file_operations	= GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW;
	a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;
	return gp_abilities_list_append(list, a);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func
};

int
camera_init (Camera *camera, GPContext *context)
{
	unsigned char *msg,*buf;
	int ret, msglen, buflen;
	unsigned int retcode;
	GPPortSettings settings;

	camera->functions->summary              = camera_summary;
	camera->functions->about                = camera_about;
	camera->functions->capture              = camera_capture;
	camera->functions->capture_preview      = camera_capture_preview;

	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	gp_port_get_settings (camera->port, &settings);
	settings.usb.inep  = 0x83;
	settings.usb.outep = 0x04;
	gp_port_set_settings (camera->port, settings);
	/* This function does the initialization sequence.
	 * CMD = 0xce, REPLY = 2 byte, 0xe0 0xe0  ... apparently means ok.
	 */
	gp_log (GP_LOG_DEBUG, "hp215", "Sending init sequence ... ");
	ret = hp_gen_cmd_blob (GET_CAMERA_READY, 0, NULL, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen, &retcode);
	free (buf);
	if (ret < GP_OK)
		return ret;
	free (msg);
	if (retcode != HP215_OK)
		return GP_ERROR_IO;
	return ret;
}
