/* qm150.c
 *
 * Copyright 2003 Marcus Meissner <marcus@jet.franken.de>
 *                Aurelien Croc (AP2C) <programming@ap2c.com>
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
 *
 * Modified by Aurélien Croc (AP²C) <programming@ap2c.com>
 * In particular : fix some bugs, and implementation of advanced 
 * functions : delete some images, delete all images, capture new image,
 * update an image to the camera, get thumbnails, get information, get
 * summary, get configuration, get manual, get about, get EXIF informations
 *
 */

/* NOTES :
 * Becareful, image information number starts at 1 and finish at the 
 * max image number. But when you want to work on the images, you must
 * read image informations, and check the real image number in the image
 * information header and use this number with all functions !!
 */

#define _BSD_SOURCE

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>
#include <exif.h>
#include <unistd.h>

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

/* Functions codes */
#define CAPTUREIMAGE_CMD2	0x30
#define SETSPEED		0x42
#define ERASEIMAGE_CMD1		0x45
#define IMAGE_CMD2		0x46
#define GETIMAGE_CMD1		0x47
#define GETIMAGEINFO		0x49
#define CAPTUREIMAGE_CMD1	0x52
#define GETCAMINFO		0x53
#define GETTHUMBNAIL_CMD1	0x54
#define UPLOADDATA		0x55
#define PING			0x58

/* Return codes */
#define ESC			0x1b
#define ACK			0x06
#define NACK			0x15
#define NEXTFRAME		0x01
#define EOT			0x04

/* Data section pointers */
#define REC_MODE		0x01
#define IMAGE_PROTECTED		0x01

#define GP_MODULE		"Konica"
#define FILENAME		"image%04d.jpg"
#define IMAGE_WIDTH		1360
#define IMAGE_HEIGHT		1024
#define PREVIEW_WIDTH		160
#define PREVIEW_HEIGHT		120
#define FILENAME_LEN		1024+128

#define ACK_LEN			1
#define STATE_LEN		1
#define CSUM_LEN		1
#define INFO_BUFFER		256
#define DATA_BUFFER		512

/* Image information pointers */
#define PREVIEW_SIZE_PTR	0x4
#define IMAGE_SIZE_PTR		0x8
#define IMAGE_NUMBER		0xE
#define IMAGE_PROTECTION_FLAG	0x11

/* Camera information pointers */
#define CAPACITY_PTR		0x3
#define POWER_STATE_PTR		0x7
#define AUTO_OFF_PTR		0x8
#define CAMERA_MODE_PTR		0xA
#define LCD_STATE_PTR		0xB
#define ICON_STATE_PTR		0xC
#define FLASH_STATE_PTR		0xD
#define TIMER_PTR		0xE
#define RESOLUTION_PTR		0xF
#define WHITE_BALANCE_PTR	0x10
#define EXPOSURE_TIME_PTR	0x11
#define TAKEN_IMAGE_PTR		0x12
#define FREE_IMAGE_PTR		0x14
#define SHARPNESS_PTR		0x16
#define COLOR_PTR		0x17
#define RED_EYE_STATE_PTR	0x18
#define FOCUS_PTR		0x19
#define MACRO_PTR		0x1A
#define ZOOM_PTR		0x1B
#define CAPTURE_TYPE_PTR	0x1E
#define REC_DATE_DISP_PTR	0x1F
#define PLAY_DATE_DISP_PTR	0x20
#define DATE_FORMAT_PTR		0x21
#define TIMESTAMP_PTR		0x22

/** Local functions **********************************************************/

/* 
 * Check the integrity of datas
 */
static unsigned char
k_calculate_checksum (unsigned char *buf, unsigned long int len)
{
	int i;
	unsigned char result=0;
	for (i=0; i < len; i++)
		result += buf[i];
	return result;
}

/* 
 * Check the connection and the camera
 */
static int
k_ping (GPPort *port) {
	char	cmd[2], buf[1];
	int	ret;
		
	cmd[0] = ESC;
	cmd[1] = PING;
	ret = gp_port_write (port, cmd, 2); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (port, buf, 1);
	if (ret<GP_OK) return ret;
	if (buf[0] != ACK) return GP_ERROR;
	return GP_OK;
}

/* 
 * Read image informations 
 */
static int
k_info_img (unsigned int image_no, void *data, CameraFileInfo* info, 
		int *data_number)
{
	unsigned char	cmd[6], buf[INFO_BUFFER];
	Camera *camera = data;
	int ret;

	/* Read file information */
	cmd[0] = ESC;
	cmd[1] = GETIMAGEINFO;
	cmd[2] = 0x30 + ((image_no/1000)%10);
	cmd[3] = 0x30 + ((image_no/100 )%10);
	cmd[4] = 0x30 + ((image_no/10  )%10);
	cmd[5] = 0x30 + ( image_no      %10);
	ret = gp_port_write (camera->port, (char*)cmd, sizeof(cmd)); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, (char*)buf, INFO_BUFFER);
	if (ret<GP_OK) return ret;

	/* Search the image data number into the memory */
	if (data_number != NULL)
		*data_number = (buf[IMAGE_NUMBER] << 8)
			| (buf[IMAGE_NUMBER+1]);

	/* Get the file info here and write it into <info> */
	/* There is no audio support with this camera */
	info->audio.fields = GP_FILE_INFO_NONE;
	/* Preview informations */
	info->preview.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_SIZE 
		| GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;
	strcpy (info->preview.type, GP_MIME_JPEG);
	info->preview.size = ((buf[PREVIEW_SIZE_PTR] << 24) | 
		(buf[PREVIEW_SIZE_PTR+1] << 16) | (buf[PREVIEW_SIZE_PTR+2] << 8)
		| buf[PREVIEW_SIZE_PTR+3]);
	info->preview.width = PREVIEW_WIDTH;
	info->preview.height = PREVIEW_HEIGHT;

	/* Image information */
	info->file.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_SIZE
		| GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT
		| GP_FILE_INFO_PERMISSIONS;
	strcpy (info->file.type, GP_MIME_JPEG);
	info->file.size = ((buf[IMAGE_SIZE_PTR] << 24) |
		(buf[IMAGE_SIZE_PTR+1] << 16) | (buf[IMAGE_SIZE_PTR+2] << 8)
		| buf[IMAGE_SIZE_PTR+3]);
	info->file.width = IMAGE_WIDTH;
	info->file.height = IMAGE_HEIGHT;
	if (buf[IMAGE_PROTECTION_FLAG] == IMAGE_PROTECTED)
		info->file.permissions = GP_FILE_PERM_READ;
	else
		info->file.permissions = GP_FILE_PERM_ALL;
	return (GP_OK);
}

/*
 * Get data from the camera
 * type = GP_FILE_TYPE_NORMAL, GP_FILE_TYPE_PREVIEW or GP_FILE_TYPE_EXIF
 */
static int
k_getdata (int image_no, int type, unsigned int len, void *data, 
		unsigned char *d, GPContext *context)
{
	Camera *camera = data;
	unsigned char ack,state, cmd[7], buf[DATA_BUFFER], *buffer=d;
	unsigned int id=0, bytes_read=0, i;
	int ret;

	cmd[0] = ESC;
	cmd[1] = GETIMAGE_CMD1;
	if (type != GP_FILE_TYPE_NORMAL)
		cmd[1] = GETTHUMBNAIL_CMD1;
	cmd[2] = IMAGE_CMD2;
	cmd[3] = 0x30 + ((image_no/1000)%10);
	cmd[4] = 0x30 + ((image_no/100 )%10);
	cmd[5] = 0x30 + ((image_no/10  )%10);
	cmd[6] = 0x30 + ( image_no      %10);

	ret = gp_port_write (camera->port, (char*)cmd, sizeof(cmd));
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, (char*)&ack, ACK_LEN);
	if (ret < GP_OK) return ret;
	if (ack == NACK) {
		gp_context_error(context, _("This preview doesn't exist."));
		return (GP_ERROR);
	}

	/* Download the required image */
	if (type == GP_FILE_TYPE_NORMAL)
		id = gp_context_progress_start (context, len, 
			_("Downloading image..."));
	for (i=0; i <= (len+DATA_BUFFER-1)/DATA_BUFFER ; i++) {
		unsigned char csum;
		int xret;

		xret = gp_port_read (camera->port, (char*)buf, DATA_BUFFER);
		if (xret < GP_OK) {
			if (type == GP_FILE_TYPE_NORMAL)
				gp_context_progress_stop (context, id);
			return xret; 
		}
		ret = gp_port_read (camera->port, (char*)&csum, CSUM_LEN);
		if (ret < GP_OK) {
			if (type == GP_FILE_TYPE_NORMAL)
				gp_context_progress_stop (context, id);
			return ret;
		}
		if ((k_calculate_checksum(buf, DATA_BUFFER)) != csum) {
			if (type == GP_FILE_TYPE_NORMAL)
				gp_context_progress_stop (context, id);
			/* acknowledge the packet */
			ack = NACK;
			ret = gp_port_write (camera->port, (char*)&ack, ACK_LEN);
			if (ret < GP_OK)
				return ret;
			gp_context_error(context, _("Data has been corrupted."));
			return (GP_ERROR_CORRUPTED_DATA);
		}
		if ((len - bytes_read) > DATA_BUFFER) {
			memcpy((char *)buffer, buf, xret);
			buffer += DATA_BUFFER;
		} else {
			memcpy((char *)buffer, buf, (len - bytes_read));
			buffer += len - bytes_read;
		}

		/* acknowledge the packet */
		ack = ACK; 
		ret = gp_port_write (camera->port, (char*)&ack, ACK_LEN);
		if (ret < GP_OK) {
			if (type == GP_FILE_TYPE_NORMAL)
				gp_context_progress_stop (context, id);
			return ret;
		}
		ret = gp_port_read (camera->port, (char*)&state, STATE_LEN);
		if (ret < GP_OK) {
			if (type == GP_FILE_TYPE_NORMAL)
				gp_context_progress_stop (context, id);
			return ret;
		}
		if (state == EOT)
			break;
		bytes_read += DATA_BUFFER;
		if (type == GP_FILE_TYPE_NORMAL)
			gp_context_progress_update (context, id, bytes_read);
	}
	/* acknowledge the packet */
	ack = ACK; 
	ret = gp_port_write (camera->port, (char*)&ack, ACK_LEN);
	if (ret < GP_OK) {
		if (type == GP_FILE_TYPE_NORMAL)
			gp_context_progress_stop (context, id);
		return ret;
	}
	if (type == GP_FILE_TYPE_NORMAL)
		gp_context_progress_stop (context, id);
	return (GP_OK);
}


/** Get and delete files *****************************************************/

/*
 * Get images, thumbnails or EXIF datas
 */
static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	unsigned char *d,*thumbnail;
	int image_number, image_no, len, ret;
	long long_len;
	CameraFileInfo file_info;
	exifparser exifdat;
	GP_DEBUG ("*** ENTER: get_file_func ***");

        image_no = gp_filesystem_number(fs, folder, filename, context);
	if (image_no < 0) return image_no;

	/* Search the image informations */
	image_no++;
	ret = k_info_img (image_no, data, (CameraFileInfo *)&file_info, 
		&image_number);
	image_no = image_number;
	if (ret < GP_OK)
		return ret;

	switch (type) {
		case GP_FILE_TYPE_NORMAL:
			len = file_info.file.size;
			if (!(d = (unsigned char *)malloc(len)))
				return (GP_ERROR_NO_MEMORY);
			ret = k_getdata(image_no, GP_FILE_TYPE_NORMAL,len,
				data, d, context);
			if (ret < GP_OK) {
				free(d);
				return ret; 
			}
			break;
		case GP_FILE_TYPE_PREVIEW:
			len = file_info.preview.size;
			long_len = (long)len;
			if (!(d = (unsigned char *)malloc(len)))
				return (GP_ERROR_NO_MEMORY);
			ret = k_getdata(image_no, GP_FILE_TYPE_PREVIEW, len, 
				data, d, context);
			if (ret < GP_OK) {
				free(d);
				return ret;
			}
			exifdat.header = d;
			exifdat.data = d+12;
			thumbnail = gpi_exif_get_thumbnail_and_size(&exifdat,
				&long_len);
			free(d);
			d = thumbnail;
			break;
		case GP_FILE_TYPE_EXIF:
			len = file_info.preview.size;
			if (!(d = (unsigned char *)malloc(len)))
				return (GP_ERROR_NO_MEMORY);
			ret = k_getdata(image_no, GP_FILE_TYPE_EXIF, len, 
				data, d, context);
			if (ret < GP_OK) {
				free(d);
				return ret;
			}
			break;
		default:
			gp_context_error(context, 
				_("Image type %d is not supported by this camera !"), type);
			return (GP_ERROR_NOT_SUPPORTED);
	}
	gp_file_set_mime_type (file, GP_MIME_JPEG);
	ret = gp_file_append(file, (char*)d, len);
	free(d);
	return (ret);
}

/*
 * Delete one image
 * The image mustn't be protected
 */
static int
delete_file_func (CameraFilesystem *fs, const char *folder, 
		const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	CameraFileInfo file_info;
	unsigned char cmd[7], ack;
	int image_no;
	int ret;

	GP_DEBUG ("*** ENTER: delete_file_func ***");

        image_no = gp_filesystem_number(fs, folder, filename, context);
	if (image_no < 0) return image_no;

	image_no++;
	ret = k_info_img (image_no, data, (CameraFileInfo *)&file_info, 
		&image_no);
	if (ret < GP_OK)
		return ret;

	/* Now, check if the image isn't protected */
	if (file_info.file.permissions == GP_FILE_PERM_READ) {
		gp_context_error(context, _("Image %s is delete protected."),
			filename);
		return (GP_ERROR);
	}

	/* Erase the image */
	cmd[0] = ESC;
	cmd[1] = ERASEIMAGE_CMD1;
	cmd[2] = IMAGE_CMD2;
	cmd[3] = 0x30 + ((image_no/1000)%10);
	cmd[4] = 0x30 + ((image_no/100 )%10);
	cmd[5] = 0x30 + ((image_no/10  )%10);
	cmd[6] = 0x30 + ( image_no      %10);
	ret = gp_port_write (camera->port, (char*)cmd, sizeof(cmd));
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, (char*)&ack, ACK_LEN);
	if (ret<GP_OK) return ret;
	if (ack != ACK) {
		gp_context_error(context, _("Can't delete image %s."),filename);
		return (GP_ERROR);
	}	
	return (GP_OK);
}

/*
 * Delete all images
 */
static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		GPContext *context)
{
	unsigned char	cmd[7], ack;
	int	ret;
	Camera *camera = data;

	GP_DEBUG ("*** ENTER: delete_all_func ***");

	cmd[0] = ESC;
	cmd[1] = ERASEIMAGE_CMD1;
	cmd[2] = IMAGE_CMD2;
	cmd[3] = 0x30;
	cmd[4] = 0x30;
	cmd[5] = 0x30;
	cmd[6] = 0x30;
	ret = gp_port_write (camera->port, (char*)cmd, sizeof(cmd));
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, (char*)&ack, ACK_LEN);
	if (ret<GP_OK) return ret;
	if (ack != ACK) {
		gp_context_error(context, _("Can't delete all images."));
		return (GP_ERROR);
	}	
	return (GP_OK);
}


/** Additional functions to handle images ************************************/

/*
 * Upload an image to the camera
 */
static int
put_file_func (CameraFilesystem *fs, const char *folder, const char *name,
		CameraFileType type, CameraFile *file, 
		void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned char	cmd[2], buf[DATA_BUFFER],ack,sum,state;
	const char *d;
	unsigned long int len, len_sent=0;
	unsigned int id;
	int ret,i;
	
	GP_DEBUG ("*** ENTER: put_file_func ***");

	/* Send function */
	cmd[0] = ESC;
	cmd[1] = UPLOADDATA;
	ret = gp_port_write (camera->port, (char*)cmd, sizeof(cmd)); 
	if (ret<GP_OK) 
		return ret;
	gp_file_get_data_and_size(file, &d, &len);
	id = gp_context_progress_start (context, len, _("Uploading image..."));
	for (i=0; i < ((len+DATA_BUFFER-1) / DATA_BUFFER); i++) {
		ret = gp_port_read (camera->port, (char*)&ack, ACK_LEN);
		if (ret<GP_OK) {
			gp_context_progress_stop (context, id);
			return ret;
		}
		if (ack != ACK) {
			gp_context_progress_stop (context, id);
			gp_context_error(context, 
				_("Can't upload this image to the camera. "
				"An error has occurred."));
			return (GP_ERROR);
		}
		state = NEXTFRAME;
		ret = gp_port_write (camera->port, (char*)&state, STATE_LEN); 
		if (ret<GP_OK) {
			gp_context_progress_stop (context, id);
			return ret;
		}
		if ((len - len_sent) <= DATA_BUFFER) {
			/* Send the last datas */
			ret = gp_port_write (camera->port, (char*)&d[i*DATA_BUFFER],
				(len - len_sent));
			if (ret<GP_OK) {
				gp_context_progress_stop (context, id);
				return ret;
			}
			/* and complete with zeros */
			memset(buf,0,DATA_BUFFER);
			ret = gp_port_write (camera->port, (char*)buf, (DATA_BUFFER - 
				(len - len_sent)));
			if (ret<GP_OK) {
				gp_context_progress_stop (context, id);
				return ret;
			}
			/* Calculate the checksum */
			sum = k_calculate_checksum(
				(unsigned char *)&d[i*DATA_BUFFER],
				(len-len_sent));
			len_sent += (len - len_sent);
		}
		else {
			/* Just send the datas */
			ret = gp_port_write (camera->port, &d[i*DATA_BUFFER],
				DATA_BUFFER);
			if (ret<GP_OK) {
				gp_context_progress_stop (context, id);
				return ret;
			}
			len_sent += DATA_BUFFER;
			sum = k_calculate_checksum(
				(unsigned char *)&d[i*DATA_BUFFER],
				DATA_BUFFER);
		}
		ret = gp_port_write (camera->port, (char*)&sum, CSUM_LEN); 
		if (ret<GP_OK) {
			gp_context_progress_stop (context, id);
			return ret;
		}
		gp_context_progress_update (context, id, len_sent);
	}
	state = EOT;
	ret = gp_port_write (camera->port, (char*)&state, STATE_LEN); 
	if (ret<GP_OK) {
		gp_context_progress_stop (context, id);
		return ret;
	}
	ret = gp_port_read (camera->port, (char*)&ack, ACK_LEN);
	if (ret<GP_OK) {
		gp_context_progress_stop (context, id);
		return ret;
	}
	if (ack != ACK) {
		gp_context_progress_stop (context, id);
		gp_context_error(context, _("Can't upload this image to the "
			"camera. An error has occurred."));
		return (GP_ERROR);
	}
	gp_context_progress_stop (context, id);
	return (GP_OK);
}

/*
 * Capture an image
 */
static int
camera_capture (Camera* camera, CameraCaptureType type, CameraFilePath* path,
		GPContext *context)
{
	unsigned char cmd[3], buf[256], ack;
	int ret, nbr_images,images_taken,i;

	GP_DEBUG ("*** ENTER: camera_capture ***");

	/* Just check if there is space available yet */
	cmd[0] = ESC;
	cmd[1] = GETCAMINFO;
	ret = gp_port_write (camera->port, (char*)cmd, 2); 
	if (ret<GP_OK)
		return ret;
	ret = gp_port_read (camera->port, (char*)buf, INFO_BUFFER);
	nbr_images = (buf[FREE_IMAGE_PTR] << 8) | (buf[FREE_IMAGE_PTR+1]);
	images_taken = (buf[TAKEN_IMAGE_PTR] << 8) | (buf[TAKEN_IMAGE_PTR+1]);

	/* Capture the image */
	cmd[0] = ESC;
	cmd[1] = CAPTUREIMAGE_CMD1;
	cmd[2] = CAPTUREIMAGE_CMD2;
	ret = gp_port_write (camera->port, (char*)cmd, sizeof(cmd));
	if (ret<GP_OK)
		return ret;
	ret = gp_port_read (camera->port, (char*)&ack, ACK_LEN);
	if (ret<GP_OK)
		return ret;
	if (ack == NACK) {
		if (buf[CAMERA_MODE_PTR] != REC_MODE)
			gp_context_error(context, _("You must be in record "
				"mode to capture images."));
		else if (!nbr_images)
			gp_context_error(context, _("No space available "
				"to capture new images. You must delete some "
				"images."));
		else
			gp_context_error(context, _("Can't capture new images. "
				"Unknown error"));
		return (GP_ERROR);
	}

	/* Wait image writting in camera's memory */
	for (i=0; i<=15; i++) {
		sleep(1);
		if ((ret = k_ping(camera->port)) == GP_OK)
			break;
	}
	if (ret < GP_OK) {
		gp_context_error(context, _("No answer from the camera."));
		return (GP_ERROR);
	}

	/* Now register new image */
	images_taken++;
	sprintf (path->name, FILENAME, (unsigned int) images_taken);
	return (GP_OK);
}

/*
 * Get informations about an image
 */
static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	int image_no;

	GP_DEBUG ("*** ENTER: get_info_func ***");

        image_no = gp_filesystem_number(fs, folder, filename, context);
	if (image_no < 0)
		return image_no;
	image_no++;

	return (k_info_img(image_no, data, info, NULL));
}


/** Driver base functions ****************************************************/

/*
 * Create the image list
 */
static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned char	cmd[2], buf[INFO_BUFFER];
	int	num, ret;

	GP_DEBUG ("*** ENTER: file_list_func ***");

	cmd[0] = ESC;
	cmd[1] = GETCAMINFO;
	ret = gp_port_write (camera->port, (char*)cmd, sizeof(cmd));
	if (ret<GP_OK)
		return ret;
	ret = gp_port_read (camera->port, (char*)buf, INFO_BUFFER);
	if (ret<GP_OK)
		return ret;
	num = (buf[TAKEN_IMAGE_PTR] << 8) | (buf[TAKEN_IMAGE_PTR+1]);
	gp_list_populate (list, FILENAME, num);
	return GP_OK;
}


/** Camera settings **********************************************************/
/*
 * List all informations about the camera
 */
static int
camera_get_config (Camera* camera, CameraWidget** window, GPContext *context)
{
	unsigned char cmd[2], buf[INFO_BUFFER];
	int ret;
        CameraWidget *widget;
        CameraWidget *section;
	time_t timestamp=0;
	float value_float;

	GP_DEBUG ("*** ENTER: camera_get_config ***");

	/* get informations about camera */
	cmd[0] = ESC;
	cmd[1] = GETCAMINFO;
	ret = gp_port_write (camera->port, (char*)cmd, sizeof(cmd));
	if (ret<GP_OK)
		return ret;
	ret = gp_port_read (camera->port, (char*)buf, INFO_BUFFER);
	if (ret<GP_OK)
		return ret;

	/* Informations manipulation */
	timestamp = (buf[TIMESTAMP_PTR] << 24) + (buf[TIMESTAMP_PTR+1] << 16)
		+ (buf[TIMESTAMP_PTR+2] << 8) + buf[TIMESTAMP_PTR+3];
	/*
	 * This timestamp start the 1 January 1980 at 00:00 
	 * but UNIX timestamp start the 1 January 1970 at 00:00
	 * so we calculate the UNIX timestamp with the camera's one
	 */
	timestamp += (8*365 + 2*366)*24*3600-3600;

	/* Window creation */
	gp_widget_new (GP_WIDGET_WINDOW, _("Konica Configuration"), window);

        /************************/
        /* Persistent Settings  */
        /************************/
        gp_widget_new (GP_WIDGET_SECTION, _("Persistent Settings"), &section);
        gp_widget_append (*window, section);

        /* Date */
        gp_widget_new (GP_WIDGET_DATE, _("Date and Time"), &widget);
        gp_widget_append (section, widget);
        gp_widget_set_value (widget, &timestamp);

        /* Auto Off Time */
        gp_widget_new (GP_WIDGET_RANGE, _("Auto Off Time"), &widget);
        gp_widget_append (section, widget);
        gp_widget_set_range (widget, 1, 255, 1);
        value_float = ((buf[AUTO_OFF_PTR] << 8) + buf[AUTO_OFF_PTR+1]) / 60;
        gp_widget_set_value (widget, &value_float);


        /* Resolution */
        gp_widget_new (GP_WIDGET_RADIO, _("Resolution"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Low"));
        gp_widget_add_choice (widget, _("Medium"));
        gp_widget_add_choice (widget, _("High"));
        switch (buf[RESOLUTION_PTR]) {
	        case 1:
        	        gp_widget_set_value (widget, _("High"));
                	break;
	        case 2:
        	        gp_widget_set_value (widget, _("Low"));
	               	break;
		case 0:
        	        gp_widget_set_value (widget, _("Medium"));
                	break;
        }

        /* LCD */
        gp_widget_new (GP_WIDGET_RADIO, _("LCD"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("On"));
        gp_widget_add_choice (widget, _("Off"));
        switch (buf[LCD_STATE_PTR]) {
	        case 0:
        	        gp_widget_set_value (widget, _("On"));
                	break;
	        case 1:
        	        gp_widget_set_value (widget, _("Off"));
                	break;
        }

        /* Icons */
        gp_widget_new (GP_WIDGET_RADIO, _("Icons"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("On"));
        gp_widget_add_choice (widget, _("Off"));
        switch (buf[ICON_STATE_PTR]) {
	        case 0:
        	        gp_widget_set_value (widget, _("On"));
                	break;
	        case 1:
        	        gp_widget_set_value (widget, _("Off"));
                	break;
        }

        /****************/
        /* Localization */
        /****************/
        gp_widget_new (GP_WIDGET_SECTION, _("Localization"), &section);
        gp_widget_append (*window, section);

        /* Date format */
        gp_widget_new (GP_WIDGET_MENU, _("Date Format"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Month/Day/Year"));
        gp_widget_add_choice (widget, _("Day/Month/Year"));
        gp_widget_add_choice (widget, _("Year/Month/Day"));
	switch (buf[DATE_FORMAT_PTR]) {
		case 0:
			gp_widget_set_value (widget, _("Month/Day/Year"));
			break;
		case 1:
			gp_widget_set_value (widget, _("Day/Month/Year"));
			break;
		case 2:
			gp_widget_set_value (widget, _("Year/Month/Day"));
			break;
	}

	/********************************/
        /* Session-persistent Settings  */
        /********************************/
        gp_widget_new (GP_WIDGET_SECTION, _("Session-persistent Settings"),
                       &section);
        gp_widget_append (*window, section);

        /* Flash */
        gp_widget_new (GP_WIDGET_RADIO, _("Flash"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Off"));
        gp_widget_add_choice (widget, _("On"));
        gp_widget_add_choice (widget, _("On, red-eye reduction"));
        gp_widget_add_choice (widget, _("Auto"));
        gp_widget_add_choice (widget, _("Auto, red-eye reduction"));
        switch (buf[FLASH_STATE_PTR]) {
	        case 2:
        	        gp_widget_set_value (widget, _("Off"));
                	break;
	        case 1:
			if (buf[RED_EYE_STATE_PTR] == 1)
	        	        gp_widget_set_value (widget, 
					_("On, red-eye reduction"));
			else
				gp_widget_set_value (widget, _("On"));
                	break;
	        case 0:
			if (buf[RED_EYE_STATE_PTR] == 1)
	        	        gp_widget_set_value (widget, 
					_("Auto, red-eye reduction"));
			else
				gp_widget_set_value (widget, _("Auto"));
                	break;
        }

        /* Exposure */
        gp_widget_new (GP_WIDGET_RANGE, _("Exposure"), &widget);
        gp_widget_append (section, widget);
        gp_widget_set_range (widget, -2, 2, 0.1);
	switch(buf[EXPOSURE_TIME_PTR]) {
		case 0:
			value_float = 0;
			break;
		case 1:
			value_float = 0.3;
			break;
		case 2:
			value_float = 0.5;
			break;
		case 3:
			value_float = 0.8;
			break;
		case 4:
			value_float = 1.0;
			break;
		case 5:
			value_float = 1.3;
			break;
		case 6:
			value_float = 1.5;
			break;
		case 7:
			value_float = 1.8;
			break;
		case 8:
			value_float = 2.0;
			break;
		case 0xF8:
			value_float = -2.0;
			break;
		case 0xF9:
			value_float = -1.8;
			break;
		case 0xFA:
			value_float = -1.5;
			break;
		case 0xFB:
			value_float = -1.3;
			break;
		case 0xFC:
			value_float = -1.0;
			break;
		case 0xFD:
			value_float = -0.8;
			break;
		case 0xFE:
			value_float = -0.5;
			break;
		case 0xFF:
			value_float = -0.3;
			break;
	}
        gp_widget_set_value (widget, &value_float);

       /* Focus */
        gp_widget_new (GP_WIDGET_RADIO, _("Focus"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("2.0 m"));
	gp_widget_add_choice (widget, _("0.5 m"));
	gp_widget_add_choice (widget, _("0.1 m"));
        gp_widget_add_choice (widget, _("Auto"));
        switch (buf[FOCUS_PTR]) {
		case 0:
			gp_widget_set_value (widget, _("Auto"));
			break;
		case 1:
	                gp_widget_set_value (widget, _("2.0 m"));
	                break;
	        case 2:
        	        gp_widget_set_value (widget, _("0.5 m"));
                	break;
	        case 3:
        	        gp_widget_set_value (widget, _("0.1 m"));
                	break;
        }

       /* white balance */
        gp_widget_new (GP_WIDGET_RADIO, _("White balance"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Office"));
	gp_widget_add_choice (widget, _("Daylight"));
	gp_widget_add_choice (widget, _("Auto"));
        switch (buf[WHITE_BALANCE_PTR]) {
	        case 0:
        	        gp_widget_set_value (widget, _("Auto"));
                	break;
	        case 1:
        	        gp_widget_set_value (widget, _("Daylight"));
                	break;
	        case 2:
        	        gp_widget_set_value (widget, _("Office"));
                	break;
	}

       /* Sharpness */
        gp_widget_new (GP_WIDGET_RADIO, _("Sharpness"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Sharp"));
	gp_widget_add_choice (widget, _("Soft"));
	gp_widget_add_choice (widget, _("Auto"));
        switch (buf[SHARPNESS_PTR]) {
	        case 0:
        	        gp_widget_set_value (widget, _("Auto"));
                	break;
	        case 1:
        	        gp_widget_set_value (widget, _("Sharp"));
                	break;
	        case 2:
        	        gp_widget_set_value (widget, _("Soft"));
                	break;
	}

       /* Color */
        gp_widget_new (GP_WIDGET_RADIO, _("Color"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Light"));
	gp_widget_add_choice (widget, _("Deep"));
	gp_widget_add_choice (widget, _("Black and White"));
	gp_widget_add_choice (widget, _("Sepia"));
	gp_widget_add_choice (widget, _("Auto"));
        switch (buf[COLOR_PTR]) {
	        case 0:
        	        gp_widget_set_value (widget, _("Auto"));
                	break;
	        case 1:
        	        gp_widget_set_value (widget, _("Light"));
                	break;
	        case 2:
        	        gp_widget_set_value (widget, _("Deep"));
                	break;
	        case 3:
        	        gp_widget_set_value (widget, _("Black and White"));
                	break;
	        case 4:
        	        gp_widget_set_value (widget, _("Sepia"));
                	break;
	}

       /* Macro */
        gp_widget_new (GP_WIDGET_RADIO, _("Macro"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("On"));
	gp_widget_add_choice (widget, _("Off"));
        switch (buf[MACRO_PTR]) {
	        case 0:
        	        gp_widget_set_value (widget, _("Off"));
                	break;
	        case 1:
        	        gp_widget_set_value (widget, _("On"));
                	break;
	}

       /* Zoom */
        gp_widget_new (GP_WIDGET_RADIO, _("Zoom"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("On"));
	gp_widget_add_choice (widget, _("Off"));
        switch (buf[ZOOM_PTR]) {
	        case 0:
        	        gp_widget_set_value (widget, _("Off"));
                	break;
	        case 1:
        	        gp_widget_set_value (widget, _("On"));
                	break;
	}

       /* Capture */
        gp_widget_new (GP_WIDGET_RADIO, _("Capture"), &widget);
        gp_widget_append (section, widget);
	gp_widget_add_choice (widget, _("Single"));
	gp_widget_add_choice (widget, _("Sequence 9"));
        switch (buf[CAPTURE_TYPE_PTR]) {
	        case 0:
        	        gp_widget_set_value (widget, _("Single"));
                	break;
	        case 1:
        	        gp_widget_set_value (widget, _("Sequence 9"));
                	break;
	}

       /* Date display */
        gp_widget_new (GP_WIDGET_RADIO, _("Date display"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Anywhere"));
	gp_widget_add_choice (widget, _("Play mode"));
	gp_widget_add_choice (widget, _("Record mode"));
	gp_widget_add_choice (widget, _("Everywhere"));
        switch (buf[REC_DATE_DISP_PTR]) {
	        case 0:
			if (buf[PLAY_DATE_DISP_PTR] == 0)
				gp_widget_set_value (widget, _("Play mode"));
			else
        	        	gp_widget_set_value (widget, _("Anywhere"));
                	break;
	        case 1:
			if (buf[PLAY_DATE_DISP_PTR] == 0)
				gp_widget_set_value (widget, _("Everywhere"));
			else
        	        	gp_widget_set_value (widget, _("Record mode"));
                	break;
	}

        /************************/
        /* Volatile Settings    */
        /************************/
        gp_widget_new (GP_WIDGET_SECTION, _("Volatile Settings"), &section);
        gp_widget_append (*window, section);

        /* Self Timer */
        gp_widget_new (GP_WIDGET_RADIO, _("Self Timer"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Self Timer (next picture only)"));
        gp_widget_add_choice (widget, _("Normal"));
        switch (buf[TIMER_PTR]) {
	        case 1:
        	        gp_widget_set_value (widget, _("Self Timer ("
                	                     "next picture only)"));
	                break;
	        case 0:
        	        gp_widget_set_value (widget, _("Normal"));
                	break;
	        }
	return (GP_OK);
}

static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	/* Can't change configuration */
	return(GP_ERROR);
}

static int
camera_summary (Camera *camera, CameraText *text, GPContext *context)
{
	unsigned char cmd[2], buf[INFO_BUFFER];
	char date_disp[20],date[50];
	char power[20],mode[20];
	int ret,capacity=0,autopoweroff=0,image_taken=0,image_remained=0;
	time_t timestamp=0;
	struct tm tmp;

	GP_DEBUG ("*** ENTER: camera_summary ***");

	/* get informations about camera */
	cmd[0] = ESC;
	cmd[1] = GETCAMINFO;
	ret = gp_port_write (camera->port, (char*)cmd, sizeof(cmd));
	if (ret<GP_OK)
		return ret;
	ret = gp_port_read (camera->port, (char*)buf, INFO_BUFFER);
	if (ret<GP_OK)
		return ret;

	capacity = buf[CAPACITY_PTR]*0x100 + buf[CAPACITY_PTR+1];
	snprintf(power,sizeof(power),_("Battery"));
	if (buf[POWER_STATE_PTR] == 1)
		snprintf(power,sizeof(power),_("AC"));
	autopoweroff = buf[AUTO_OFF_PTR]*0x100 + buf[AUTO_OFF_PTR+1];
	autopoweroff /= 60;
	snprintf(mode,sizeof(mode),_("Play"));
	if (buf[CAMERA_MODE_PTR] == 1)
		snprintf(mode,sizeof(mode),_("Record"));
	image_taken = buf[TAKEN_IMAGE_PTR] * 0x100 + buf[TAKEN_IMAGE_PTR+1];
	image_remained = buf[FREE_IMAGE_PTR] * 0x100 + buf[FREE_IMAGE_PTR+1];

	timestamp = (buf[TIMESTAMP_PTR] << 24) + (buf[TIMESTAMP_PTR+1] << 16)
		+ (buf[TIMESTAMP_PTR+2] << 8) + buf[TIMESTAMP_PTR+3];
	timestamp = (unsigned int)timestamp + (8*365 + 2*366)*24*3600-3600;
	tmp = *localtime(&timestamp);
	switch (buf[DATE_FORMAT_PTR]) {
		case 1:
			snprintf(date_disp,sizeof(date_disp),_("DD/MM/YYYY"));
			strftime(date,sizeof(date),"%d/%m/%Y %H:%M",&tmp);
			break;
		case 2:
			strftime(date,sizeof(date),"%Y/%m/%d %H:%M",&tmp);
			snprintf(date_disp,sizeof(date_disp),_("YYYY/MM/DD"));
			break;
		default:
			strftime(date,sizeof(date),"%m/%d/%Y %H:%M",&tmp);
			snprintf(date_disp,sizeof(date_disp),_("MM/DD/YYYY"));
			break;
	}
	snprintf(text->text, sizeof(text->text), 
			_("Model: %s\n"
			"Capacity: %i Mb\n"
			"Power: %s\n"
			"Auto Off Time: %i min\n"
			"Mode: %s\n"
			"Images: %i/%i\n"
			"Date display: %s\n"
			"Date and Time: %s\n"),
			"Konica Q-M150",capacity,
			power,autopoweroff,mode,
			image_taken,image_remained,
			date_disp,date);
	return (GP_OK);
}

/** Camera's driver informations *********************************************/

/*
 * Say somethings about driver's authors
 */
static int
camera_about (Camera* camera, CameraText* about, GPContext *context)
{
	snprintf(about->text,sizeof(about->text),_("Konica Q-M150 Library\n"
			"Marcus Meissner <marcus@jet.franken.de>\n"
			"Aurelien Croc (AP2C) <programming@ap2c.com>\n"
			"http://www.ap2c.com\n"
			"Support for the french Konica Q-M150."));
	return(GP_OK);
}

/*
 * just say some things about this driver
 */
static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	snprintf(manual->text,sizeof(manual->text),
	_("About Konica Q-M150:\n"
	"This camera does not allow any changes\n"
	"from the outside. So in the configuration, you can\n"
	"only see what it is configured on the camera\n"
	"but you can not change anything.\n\n"
	"If you have some issues with this driver, please e-mail its authors.\n"
	));
	return (GP_OK);
}

/*
 * Camera's identification
 */
int
camera_id (CameraText *id) 
{
	strcpy(id->text, "konica qm150");
	return (GP_OK);
}

/*
 * List all camera's abilities
 */
int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Konica:Q-M150");
	a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port = GP_PORT_SERIAL;
	a.speed[0] = 115200;
	a.speed[1] = 0;
	a.operations = GP_OPERATION_CAPTURE_IMAGE | 
		GP_OPERATION_CAPTURE_PREVIEW | GP_OPERATION_CONFIG;
	a.file_operations = GP_FILE_OPERATION_DELETE | 
		GP_FILE_OPERATION_EXIF | GP_FILE_OPERATION_PREVIEW;
	a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL | 
		GP_FOLDER_OPERATION_PUT_FILE;
	gp_abilities_list_append(list, a);
	return (GP_OK);
}

/** Camera's initialization **************************************************/
/*
 * Initialization of the camera
 */

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.put_file_func = put_file_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func,
};
int
camera_init (Camera *camera, GPContext *context) 
{
	GPPortSettings settings;
	int speeds[] = { 115200, 9600, 19200, 38400, 57600, 115200 };
	int ret, i;
	char cmd[3], buf[1];

	/* First, set up all the function pointers. */
	camera->functions->capture              = camera_capture;
	camera->functions->about		= camera_about;
	camera->functions->get_config		= camera_get_config;
	camera->functions->set_config		= camera_set_config;
	camera->functions->summary		= camera_summary;
	camera->functions->manual		= camera_manual;


	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	/*
	 * The port is already provided with camera->port (and
	 * already open). You just have to use functions like
	 * gp_port_timeout_set, gp_port_settings_get, gp_port_settings_set.
	 */
	gp_port_get_settings (camera->port, &settings);
	settings.serial.speed	= 115200;
	settings.serial.bits	= 8;
	settings.serial.stopbits= 1;
	settings.serial.parity	= 0;
	gp_port_set_settings (camera->port, settings);
	/*
	 * Once you have configured the port, you should check if a 
	 * connection to the camera can be established.
	 */
	for (i=0;i<sizeof(speeds)/sizeof(speeds[0]);i++) {
		gp_port_get_settings (camera->port, &settings);
		settings.serial.speed	= speeds[i];
		gp_port_set_settings (camera->port, settings);
		if (GP_OK<=k_ping (camera->port))
			break;
	}
	if (i == sizeof(speeds)/sizeof(speeds[0]))
		return GP_ERROR;
	cmd[0] = ESC;
	cmd[1] = SETSPEED;
	cmd[2] = 0x30 + 4; /* speed nr 4:(9600, 19200, 38400, 57600, 115200) */
	ret = gp_port_write (camera->port, cmd, 3); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 1);
	if (ret<GP_OK) return ret;
	if (buf[0] != ACK) return GP_ERROR;
	gp_port_get_settings (camera->port, &settings);
	settings.serial.speed	= 115200;
	gp_port_set_settings (camera->port, settings);
	return GP_OK;
}
