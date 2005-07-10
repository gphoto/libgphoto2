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
 *  write to the Free Software Foundation, Inc., 59 Temple Place -           *
 *  Suite 330,  Boston, MA 02111-1307, USA.                                  *
 *                                                                           *
 *****************************************************************************/


/*
    Old Maintainer : Enno Bartels <ennobartels@t-online.de> in 2002/2003
    Original author : Jason Surprise <thesurprises1@attbi.com> in 2002.
    Checksum author: Roberto Ragusa <r.ragusa@libero.it> in 2002/2003.

    Ported to libgphoto2: Marcus Meissner <marcus@jet.franken.de> in 2005.

    Camera commands:
     Global comands
                           Bytes   | 0    1cmd 2    3    4    5    6    7    8    9    10   11   12   13   14   15
    -------------------------------+----------------------------------------------------------------------------------
     init camera              8    | 0x02 0xce 0x80 0x8a 0x84 0x8d 0x83 0x03
     get camera date/time     8    | 0x02 0xc1 0x80 0x8b 0x84 0x8e 0x8d 0x03
     Get number of pics      16    | 0x02 0xc6 0x88 0x80 0x80 0x83 0x84 0x38 0x2f 0x30 0x32 0x88 0x84 0x8e 0x8b 0x03
     Delete all pics         12    | 0x02 0xb1 0x84 0x8f 0x8f 0x8f 0x8f 0x86 0x80 0x8a 0x86 0x03


     Single picture comands:
     Allways 12 bytes long
                                   | 0    1cmd 2    3    4    5    6    7    8    9    10    11
    -------------------------------+---------------------------------------------------------------
     Request for a preview         | 0x02 0xb3 0x84 0x80 ...                                 0x03
     Request for a picture         | 0x02 0xb4 0x84 0x80 ...                                 0x03
     Request date/time for preview | 0x02 0xc5 0x84 0x80 ...                                 0x03
     Request for deleting one pic  | 0x02 0xb1 0x84 0x80 ...                                 0x03

     0x02 = Startcode for a command
     0x03 = Endcode   for a command
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <gphoto2-port-log.h>
#include <gphoto2-library.h>
#include <gphoto2-result.h>

#include "crctab.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

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

     Request for a preview         | 0xb3	16bit picnum
     Request for a picture         | 0xb4	16bit picnum
     Request date/time for preview | 0xc5	16bit picnum
     Request for deleting one pic  | 0xb1	16bit picnum

    Picture number:
    ----------------
    buffer[4] = ((num&0xf00)>>8) | 0x80;
    buffer[5] = ((num&0xf0 )>>4) | 0x80;
    buffer[6] = ((num&0xf  )   ) | 0x80;

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
		gp_log (GP_LOG_ERROR, "hp215", "Using too large argument buffer %d bytes\n", bytes);
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
		gp_log (GP_LOG_ERROR, "hp215", "ACK sending failed with %d\n", ret);
	return ret;
}


static int
hp_rcv_ack (Camera *cam)
{
	int           ret;
	char byte = '\0';

	gp_log (GP_LOG_DEBUG, "hp215", "Receiving ACK ... ");
	ret = gp_port_read (cam->port, &byte, 1);
	if (ret < GP_OK)
		return ret;
	if (byte == ACK)
		return GP_OK;
	gp_log (GP_LOG_DEBUG, "hp215", "Expected ACK, but read %02x", byte);
	return GP_ERROR_IO;
}

static int
hp_send_command_and_receive_blob(
	Camera *camera, unsigned char *buf, int buflen,
	unsigned char **msg, int *msglen
) {
	int ret;
	unsigned char msgbuf[0x400];

	*msg = NULL;
	*msglen = 0;
	ret = gp_port_write (camera->port, (char*)buf, buflen);
	if (ret < GP_OK)
		return ret;
	if (hp_rcv_ack (camera))
		return GP_ERROR_IO;
	gp_log( GP_LOG_DEBUG, "hp215", "Expecting reply blob");
	ret = gp_port_read (camera->port, (char*)msgbuf, sizeof(msgbuf));
	if (ret < GP_OK)
		return ret;
	if (msgbuf[0] != STX) {
		gp_log (GP_LOG_ERROR, "hp215", "Expected STX / 02 at begin of buffer, found %02x\n", msgbuf[0]);
		return GP_ERROR_IO;
	}
	if (msgbuf[ret-1] != ETX) {
		gp_log (GP_LOG_ERROR, "hp215", "Expected ETX / 03 at end of buffer, found %02x\n", msgbuf[ret-1]);
		return GP_ERROR_IO;
	}
	*msg = malloc(ret);
	*msglen = ret;
	memcpy (*msg, msgbuf, ret);
	return hp_send_ack (camera);
}

static int
hp_get_timeDate_cam (Camera *cam, char *txtbuffer, size_t txtbuffersize)
{
	int           msglen, buflen, ret;
	unsigned char *msg;
	unsigned char *buf;
	t_date        date;

	gp_log (GP_LOG_DEBUG, "hp215", "Sending date/time command ... ");

	ret = hp_gen_cmd_blob (GET_CAMERA_CURINFO, 0, NULL, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (cam, buf, buflen, &msg, &msglen);
	free (buf);
	if (ret < GP_OK)
		return ret;

	/* Filter time and date out of the message */
	date.day   = (msg[8]-48)*10 + (msg[9]-48);
	date.month = (msg[5]-48)*10 + (msg[6]-48) - 1;
	date.year  = 2000 + (msg[11]-48)*10 + (msg[12]-48);
	date.hour  = (msg[14]-48)*10 + (msg[15]-48);
	date.min   = (msg[17]-48)*10 + (msg[18]-48);
	free (msg);

	snprintf (txtbuffer, txtbuffersize, _("Current camera time:  %04d-%02d-%02d  %02d:%02d\n"),
		date.year, date.month, date.day, date.hour, date.min
	);
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
	int           ret, buflen, msglen;
	unsigned char imgdata[0x01000];
	unsigned char *buf, *msg;
        int image_no;
	unsigned char cmd;

        image_no = gp_filesystem_number(fs, folder, filename, context);
        if(image_no < 0)
                return image_no;
	image_no++;

	memset(msg, 0, sizeof(msg));
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

	ret = hp_gen_cmd_1_16 (cmd, image_no, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen);
	free (buf);
	if (ret < GP_OK)
		return ret;
	free (msg);

	gp_file_set_mime_type (file, GP_MIME_JPEG);

	/* Read preview in 4096 byte parts (0x1000) */
	while (1)
	{
		ret = gp_port_read (camera->port, (char*)imgdata, 0x1000);
		/* Check if preview reading is complete*/
		if ((ret==1) && (imgdata[0]==0x04)) {
			gp_log (GP_LOG_DEBUG, "hp215", "Image data complete!\n");
			break;
		}

		/* Check if there was an error */
		if (ret==-1) {
			gp_log (GP_LOG_ERROR, "hp215", "Warning: Image data may be corrupted...\n");
			break;
		}
		gp_file_append (file, (char*)imgdata, ret);
	}
	return GP_OK;
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int image_no, ret, msglen, buflen;
	unsigned char  *msg, *buf;

        image_no = gp_filesystem_number(fs, folder, filename, context);
        if(image_no < 0)
                return image_no;
	ret = hp_gen_cmd_1_16 (DELETE_PHOTO, image_no+1, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen);
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
	int      ret, buflen, msglen;

	ret = hp_gen_cmd_1_16 (DELETE_PHOTO, 0xFFFF, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen);
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
	int		ret, msglen, buflen, offset = 13, image_no;
	unsigned char	*msg, *buf;

        image_no = gp_filesystem_number(fs, folder, filename, context);
        if(image_no < 0)
                return image_no;
	ret = hp_gen_cmd_1_16 (GET_PHOTO_INFO, image_no+1, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen);
	free (buf);
	if (ret < GP_OK)
		return ret;
	free (msg);
	return GP_ERROR_IO;
#if 0
	/* Copy getten date into date placeholder */
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


/*

   000013 B> 00000000: 02c6 8880 8083 8438 2f30 3288 848e 8b03 .......8/02.....

   000014 B< 00000000: 06                                      .

   000015 B< 00000000: 02c6 aae0 e050 686f 746f 416c 6275 6d00 .....PhotoAlbum.
             00000010: 3a32 3600 8080 0180 8182 8c81 8080 8080 :26.............
             00000020: 8080 8080 e480 8080 8080 8080 8087 8084 ................
             00000030: 8a03                                    ..

   000016 B> 00000000: 06                                      .

    The first cmd in the trace is sent to the camera.  It appears to
    change depending on some factors.  For example, it seems to use the
    last 4 digits of the date returned from the camera as part of the cmd
    that is sent (the 8/02 above is from 7/18/02 that was returned from
    the camera in hp_get_timeDate_cam.  However, I have found that I
    can use the above command as is regardless of the date/time and still
    receive the PhotoAlbum correctly, so I've implemented it as a static
    call.

    From observation, the 43+44+45th lower words of the returned msg contains
    the number of pictures in the camera.
 */

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int           ret, msglen, count;
	unsigned char *msg;
	unsigned char buffer[] = HP_CMD_GET_PHOTO_ALBUM;

	/* Sending photo album request command */
	gp_log (GP_LOG_DEBUG, "hp215", "Sending photo album request ... ");
	ret = hp_send_command_and_receive_blob (camera, buffer, sizeof(buffer), &msg, &msglen);
	if (ret < GP_OK)
		return ret;
	/* Calculate the number of pictures from the gotten message */
	count = 256*(msg[42]&0x7f) + 16*(msg[43]&0x7f) + (msg[44]&0x7f);

	free (msg);
        return gp_list_populate(list, "image%i.jpg", count);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
                GPContext *context)
{
	unsigned char *buf, *msg;
	int ret, buflen, msglen;

	ret = hp_gen_cmd_1_16 (TAKE_PHOTO, 1, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen);
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
	a.operations		= GP_OPERATION_CAPTURE_IMAGE;
	a.file_operations	= GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW;
	a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;
	return gp_abilities_list_append(list, a);
}


int
camera_init (Camera *camera, GPContext *context)
{
	unsigned char *msg,*buf;
	int ret, msglen, buflen;
	GPPortSettings settings;

        camera->functions->summary              = camera_summary;
        camera->functions->about                = camera_about;
        camera->functions->capture              = camera_capture;

	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, delete_file_func, camera);
	gp_filesystem_set_folder_funcs (camera->fs, NULL, delete_all_func, NULL, NULL, camera);

	gp_port_get_settings (camera->port, &settings);
	settings.usb.inep  = 0x83;
	settings.usb.outep = 0x04;
	gp_port_set_settings (camera->port, settings);
	/* This function does the initialisation sequence.
	 * CMD = 0xce, REPLY = 2 byte, 0xe0 0xe0  ... apparently means ok.
	 */
	gp_log (GP_LOG_DEBUG, "hp215", "Sending init sequence ... ");
	ret = hp_gen_cmd_blob (GET_CAMERA_READY, 0, NULL, &buf, &buflen);
	if (ret < GP_OK)
		return ret;
	ret = hp_send_command_and_receive_blob (camera, buf, buflen, &msg, &msglen);
	free (buf);
	if (ret < GP_OK)
		return ret;
	free (msg);
	return ret;
}
