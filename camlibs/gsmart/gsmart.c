/****************************************************************/
/* gsmart.c - Gphoto2 library for the Mustek gSmart mini 2      */
/*                                                              */
/* Copyright (C) 2002 Till Adam                                 */
/*                                                              */
/* Author: Till Adam <till@adam-lilienthal.de>                  */
/*                                                              */
/* This library is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU Library General  */
/* Public License as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any     */
/* later version.                                               */
/*                                                              */
/* This library is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU Library General Public License for     */
/* more details.                                                */
/*                                                              */
/* You should have received a copy of the GNU Library General   */
/* Public License along with this library; if not, write to the */
/* Free Software Foundation, Inc., 59 Temple Place - Suite 330, */
/* Boston, MA 02111-1307, USA.                                  */
/****************************************************************/
#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gphoto2.h>
#include <gphoto2-endian.h>

#include "gsmart.h"
#include "gsmart-registers.h"
#include "gsmart-jpeg-header.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define GP_MODULE "gsmartmini2"
static int gsmart_mode_set_idle (CameraPrivateLibrary * lib);
static int gsmart_is_idle (CameraPrivateLibrary * lib);
static int gsmart_mode_set_download (CameraPrivateLibrary * lib);
static int gsmart_download_data (CameraPrivateLibrary * lib, u_int32_t start,
				 unsigned int size, u_int8_t * buf);
static int gsmart_get_FATs (CameraPrivateLibrary * lib);
static int yuv2rgb (int y, int u, int v, int *r, int *g, int *b);
static int gsmart_get_avi_thumbnail (CameraPrivateLibrary * lib,
				     u_int8_t ** buf, unsigned int *len,
				     struct GsmartFile *g_file);
static int gsmart_get_image_thumbnail (CameraPrivateLibrary * lib,
				       u_int8_t ** buf, unsigned int *len,
				       struct GsmartFile *g_file);

int
gsmart_get_file_count (CameraPrivateLibrary * lib)
{
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0x5, 0, 0, NULL, 0));
	sleep (1);
	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, 0xe15, (u_int8_t *) & lib->num_files, 1));

	return (GP_OK);
}

int
gsmart_delete_file (CameraPrivateLibrary * lib, unsigned int index)
{
	struct GsmartFile *g_file;
	u_int16_t fat_index;

	CHECK (gsmart_get_file_info (lib, index, &g_file));

	fat_index = 0xD8000 - g_file->fat_start - 1;

	CHECK (gp_port_usb_msg_write (lib->gpdev, 0x06, fat_index, 0x0007, NULL, 0));
	sleep (1);

	/* Reread fats the next time it is accessed */
	lib->dirty = 1;

	return GP_OK;
}

int
gsmart_delete_all (CameraPrivateLibrary * lib)
{
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0x02, 0x0000, 0x0005, NULL, 0));
	sleep (3);

	/* Reread fats the next time it is accessed */
	lib->dirty = 1;
	return GP_OK;
}

int
gsmart_request_file (CameraPrivateLibrary * lib, u_int8_t ** buf,
		     unsigned int *len, unsigned int number)
{
	struct GsmartFile *g_file;
	u_int8_t *p, *lp_jpg, *start_of_file;
	u_int8_t qIndex, quality, value;
	u_int32_t start;
	u_int8_t *mybuf;
	int size, o_size, i, file_size;

	CHECK (gsmart_get_file_info (lib, number, &g_file));
	/* FIXME implement avi downloading */
	if (g_file->mime_type == GSMART_FILE_TYPE_AVI)
		return GP_ERROR_NOT_SUPPORTED;


	p = g_file->fat;

	/* get the position in memory where the image is */
	start = (p[1] & 0xff) + (p[2] & 0xff) * 0x100;
	start *= 128;

	/* decode the image size */
	o_size = size = (p[13] & 0xff) * 0x10000 + (p[12] & 0xff) * 0x100 + (p[11] & 0xff);
	qIndex = p[7] & 0x07;
	quality = p[40] & 0xff;

	/* align */
	if (size % 64 != 0)
		size = ((size / 64) + 1) * 64;

	file_size = size + GSMART_JPG_DEFAULT_HEADER_LENGTH + 1024 * 10;

	/* slurp in the image */
	mybuf = malloc (size);
	if (!mybuf)
		return GP_ERROR_NO_MEMORY;

	CHECK (gsmart_download_data (lib, start, size, mybuf));

	/* now build a jpeg */
	lp_jpg = malloc (file_size);
	if (!lp_jpg)
		return GP_ERROR_NO_MEMORY;
	start_of_file = lp_jpg;

	/* copy the header from the template */
	memcpy (lp_jpg, GsmartJPGDefaultHeader, GSMART_JPG_DEFAULT_HEADER_LENGTH);

	/* modify quantization table */
	memcpy (lp_jpg + 7, GsmartQTable[qIndex * 2], 64);
	memcpy (lp_jpg + 72, GsmartQTable[qIndex * 2 + 1], 64);

	/* modify the image width, height */
	*(lp_jpg + 564) = (g_file->width) & 0xFF;	//Image width low byte
	*(lp_jpg + 563) = (g_file->width >> 8) & 0xFF;	//Image width high byte
	*(lp_jpg + 562) = (g_file->height) & 0xFF;	//Image height low byte
	*(lp_jpg + 561) = (g_file->height >> 8) & 0xFF;	//Image height high byte

	/* point to real JPG compress data start position and copy */
	lp_jpg += GSMART_JPG_DEFAULT_HEADER_LENGTH;

	for (i = 0; i < o_size; i++) {
		value = *(mybuf + i) & 0xFF;
		*(lp_jpg) = value;
		lp_jpg++;
		if (value == 0xFF) {
			*(lp_jpg) = 0x00;
			lp_jpg++;
		}
	}
	/* Add end of image marker */
	*(lp_jpg++) = 0xFF;
	*(lp_jpg++) = 0xD9;
	free (mybuf);
	file_size = lp_jpg - start_of_file;
	start_of_file = realloc (start_of_file, file_size);
	*buf = start_of_file;
	*len = file_size;

	return (GP_OK);
}

int
gsmart_request_thumbnail (CameraPrivateLibrary * lib, u_int8_t ** buf,
			  unsigned int *len, unsigned int number, int *type)
{
	struct GsmartFile *g_file;

	CHECK (gsmart_get_file_info (lib, number, &g_file));

	*type = g_file->mime_type;
	if (g_file->mime_type == GSMART_FILE_TYPE_AVI)
		return (gsmart_get_avi_thumbnail (lib, buf, len, g_file));
	else
		return (gsmart_get_image_thumbnail (lib, buf, len, g_file));
}


static int
gsmart_get_avi_thumbnail (CameraPrivateLibrary * lib, u_int8_t ** buf,
			  unsigned int *len, struct GsmartFile *g_file)
{
	u_int8_t *p, *lp_jpg, *start_of_file;
	u_int8_t qIndex, quality, value;
	u_int32_t start;
	u_int8_t *mybuf;
	int size, o_size, i, file_size;
	int w, h;

	p = g_file->fat;

	/* get the position in memory where the image is */
	start = (p[1] & 0xff) + (p[2] & 0xff) * 0x100;
	start *= 128;

	/* decode the image size */
	o_size = size = (p[52] & 0xff) * 0x10000 + (p[51] & 0xff) * 0x100 + (p[50] & 0xff);
	qIndex = p[7] & 0x07;
	quality = p[40] & 0xff;
	w = (int) ((p[8] & 0xFF) * 16);
	h = (int) ((p[9] & 0xFF) * 16);

	/* align */
	if (size % 64 != 0)
		size = ((size / 64) + 1) * 64;

	file_size = size + GSMART_JPG_DEFAULT_HEADER_LENGTH + 1024 * 10;

	/* slurp in the image */
	mybuf = malloc (size);
	if (!mybuf)
		return GP_ERROR_NO_MEMORY;

	CHECK (gsmart_download_data (lib, start, size, mybuf));

	/* now build a jpeg */
	lp_jpg = malloc (file_size);
	if (!lp_jpg)
		return GP_ERROR_NO_MEMORY;
	start_of_file = lp_jpg;

	/* copy the header from the template */
	memcpy (lp_jpg, GsmartJPGDefaultHeader, GSMART_JPG_DEFAULT_HEADER_LENGTH);

	/* modify quantization table */
	memcpy (lp_jpg + 7, GsmartQTable[qIndex * 2], 64);
	memcpy (lp_jpg + 72, GsmartQTable[qIndex * 2 + 1], 64);

	/* modify the image width, height */
	*(lp_jpg + 564) = w & 0xFF;	//Image width low byte
	*(lp_jpg + 563) = w >> 8 & 0xFF;	//Image width high byte
	*(lp_jpg + 562) = h & 0xFF;	//Image height low byte
	*(lp_jpg + 561) = h >> 8 & 0xFF;	//Image height high byte

	/* set the format to yuv 211 */
	*(lp_jpg + 567) = 0x22;

	/* point to real JPG compress data start position and copy */
	lp_jpg += GSMART_JPG_DEFAULT_HEADER_LENGTH;

	for (i = 0; i < o_size; i++) {
		value = *(mybuf + i) & 0xFF;
		*(lp_jpg) = value;
		lp_jpg++;
		if (value == 0xFF) {
			*(lp_jpg) = 0x00;
			lp_jpg++;
		}
	}
	/* Add end of image marker */
	*(lp_jpg++) = 0xFF;
	*(lp_jpg++) = 0xD9;
	free (mybuf);
	file_size = lp_jpg - start_of_file;
	start_of_file = realloc (start_of_file, file_size);
	*buf = start_of_file;
	*len = file_size;

	return (GP_OK);
}


static int
gsmart_get_image_thumbnail (CameraPrivateLibrary * lib, u_int8_t ** buf,
			    unsigned int *len, struct GsmartFile *g_file)
{
	unsigned int size;
	u_int8_t *p;
	u_int8_t quality;
	u_int32_t start;
	u_int8_t *mybuf = NULL;
	u_int8_t *tmp;
	unsigned int t_width, t_height;
	u_int8_t *yuv_p;
	u_int8_t *rgb_p;
	unsigned char pbm_header[16];

	p = g_file->fat;
	quality = p[40] & 0xFF;

	start = (p[3] & 0xff) + (p[4] & 0xff) * 0x100;
	start *= 128;

	size = g_file->width * g_file->height * 2 / 64;
	t_width = g_file->width / 8;
	t_height = g_file->height / 8;
	snprintf (pbm_header, sizeof (pbm_header), "P6 %d %d 255\n", t_width, t_height);

	/* align */
	if (size % 64 != 0)
		size = ((size / 64) + 1) * 64;

	mybuf = malloc (size);
	CHECK (gsmart_download_data (lib, start, size, mybuf));
	*len = t_width * t_height * 3 + sizeof (pbm_header);
	*buf = malloc (*len);
	if (!*buf)
		return (GP_ERROR_NO_MEMORY);

	tmp = *buf;
	snprintf (tmp, sizeof (pbm_header), pbm_header);
	tmp += sizeof (pbm_header) - 1;

	yuv_p = mybuf;
	rgb_p = tmp;
	while (yuv_p < mybuf + size) {
		unsigned int u, v, y, y2;
		unsigned int r, g, b;

		y = yuv_p[0];
		y2 = yuv_p[1];
		u = yuv_p[2];
		v = yuv_p[3];

		CHECK (yuv2rgb (y, u, v, &r, &g, &b));
		*rgb_p++ = r;
		*rgb_p++ = g;
		*rgb_p++ = b;

		CHECK (yuv2rgb (y2, u, v, &r, &g, &b));
		*rgb_p++ = r;
		*rgb_p++ = g;
		*rgb_p++ = b;

		yuv_p += 4;
	}

	free (mybuf);
	return (GP_OK);
}


int
gsmart_get_info (CameraPrivateLibrary * lib)
{
	unsigned int index;
	u_int8_t *p;
	u_int32_t start_page, end_page;
	u_int8_t file_type;

	GP_DEBUG ("* gsmart_get_info");
	CHECK (gsmart_get_file_count (lib));
	if (lib->num_files > 0) {
		CHECK (gsmart_get_FATs (lib));

		index = lib->files[lib->num_files - 1].fat_end;
		p = lib->fats + FLASH_PAGE_SIZE * index;
		/* p now points to the fat of the last image of the last file */

		file_type = p[0];
		start_page = ((p[2] & 0xFF) << 8) | (p[1] & 0xFF);
		end_page = start_page + (((p[6] & 0xFF) << 8) | (p[5] & 0xFF));
		if (file_type == GSMART_FILE_TYPE_IMAGE)
			end_page = end_page + 0xA0;

		lib->size_used = (end_page - 0x2800) * FLASH_PAGE_SIZE;
	} else
		lib->size_used = 0;

	lib->size_free = 16 * 1024 * 1024 - 0x2800 * FLASH_PAGE_SIZE - lib->size_used;
	lib->dirty = 0;

	return GP_OK;
}

int
gsmart_get_file_info (CameraPrivateLibrary * lib, unsigned int index,
		      struct GsmartFile **g_file)
{
	if (lib->dirty)
		CHECK (gsmart_get_info (lib));
	*g_file = &(lib->files[index]);
	return GP_OK;
}

int
gsmart_reset (CameraPrivateLibrary * lib)
{
	GP_DEBUG ("* gsmart_reset");
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0x02, 0x0000, 0x0003, NULL, 0));
	sleep (1);
	return GP_OK;
}

int
gsmart_capture (CameraPrivateLibrary * lib)
{
	sleep (2);
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0x06, 0x0000, 0x0003, NULL, 0));
	sleep (3);
	return GP_OK;
}

static int
gsmart_mode_set_idle (CameraPrivateLibrary * lib)
{
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, GSMART_CamMode_Idle, GSMART_REG_CamMode, NULL, 0));
	return GP_OK;
}

static int
gsmart_is_idle (CameraPrivateLibrary * lib)
{
	int mode;

	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, GSMART_REG_CamMode, (u_int8_t *) & mode, 1));

	return mode == GSMART_CamMode_Idle ? 1 : 0;
}

static int
gsmart_mode_set_download (CameraPrivateLibrary * lib)
{
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, GSMART_CamMode_Upload, GSMART_REG_CamMode, NULL, 0));
	return GP_OK;
}

static int
gsmart_download_data (CameraPrivateLibrary * lib, u_int32_t start,
		      unsigned int size, u_int8_t * buf)
{

	u_int8_t foo;
	u_int8_t vlcAddressL, vlcAddressM, vlcAddressH;

	if (!gsmart_is_idle (lib))
		gsmart_mode_set_idle (lib);

	gsmart_mode_set_download (lib);

	foo = size & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo, GSMART_REG_SdramSizeL, NULL, 0));
	foo = (size >> 8) & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo, GSMART_REG_SdramSizeM, NULL, 0));
	foo = (size >> 16) & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo, GSMART_REG_SdramSizeH, NULL, 0));

	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, GSMART_REG_VlcAddressL, (u_int8_t *) & vlcAddressL, 1));
	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, GSMART_REG_VlcAddressM, (u_int8_t *) & vlcAddressM, 1));
	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, GSMART_REG_VlcAddressH, (u_int8_t *) & vlcAddressH, 1));

	foo = start & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo, GSMART_REG_VlcAddressL, NULL, 0));
	foo = (start >> 8) & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo, GSMART_REG_VlcAddressM, NULL, 0));
	foo = (start >> 16) & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo, GSMART_REG_VlcAddressH, NULL, 0));

	/* Set mode to ram -> usb */
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, GSMART_DramUsb, GSMART_REG_PbSrc, NULL, 0));
	/* and pull the trigger */
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, GSMART_TrigDramFifo, GSMART_REG_Trigger, NULL, 0));

	CHECK (gp_port_read (lib->gpdev, buf, size));

	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, vlcAddressL, GSMART_REG_VlcAddressL, NULL, 0));
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, vlcAddressM, GSMART_REG_VlcAddressM, NULL, 0));
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, vlcAddressH, GSMART_REG_VlcAddressH, NULL, 0));

	gsmart_mode_set_idle (lib);
	return GP_OK;
}

static int
gsmart_get_FATs (CameraPrivateLibrary * lib)
{
	u_int8_t dramtype, type;
	u_int8_t lower, upper;
	u_int32_t fatscount;
	unsigned int index = 0;
	unsigned int file_index = 0;
	u_int8_t *p = NULL;
	u_int8_t buf[14];

	/* Reset image and movie counter */
	lib->num_images = lib->num_movies = 0;

	CHECK (gp_port_usb_msg_write (lib->gpdev, 0x05, 0x0000, 0x0008, NULL, 0));
	sleep (0.1);

	CHECK (gp_port_usb_msg_read (lib->gpdev, 0, 0, 0x0e19, (u_int8_t *) & lower, 1));
	CHECK (gp_port_usb_msg_read (lib->gpdev, 0, 0, 0x0e20, (u_int8_t *) & upper, 1));

	fatscount = ((upper & 0xFF << 8) | (lower & 0xFF));

	if (lib->fats)
		free (lib->fats);
	lib->fats = malloc (fatscount * FLASH_PAGE_SIZE);

	if (lib->files)
		free (lib->files);
	lib->files = malloc (lib->num_files * sizeof (struct GsmartFile));

	if (!gsmart_is_idle (lib))
		gsmart_mode_set_idle (lib);

	gsmart_mode_set_download (lib);

	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0x00, 0x0001, GSMART_REG_AutoPbSize, NULL, 0));
	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, GSMART_REG_DramType, (u_int8_t *) & dramtype, 1));

	dramtype &= 0xFF;
	p = lib->fats;

	while (index < fatscount) {
		switch (dramtype) {
			case 4:	/* 128 Mbit */
				CHECK (gsmart_download_data
				       (lib, 0x7fff80 - index * 0x80, FLASH_PAGE_SIZE, p));
				break;
			default:	/* FALLTHROUGH */
			case 3:	/* 64 Mbit */
				CHECK (gsmart_download_data
				       (lib, 0x3fff80 - index * 0x80, FLASH_PAGE_SIZE, p));
				break;
		}
		if (p[0] == 0xFF)
			break;
		type = p[0];
		if (type == 0x00 || type == 0x08) {
			if (type == 0x00) {	/* its an image */
				snprintf (buf, 13, "Image%03d.jpg", ++lib->num_images);
				lib->files[file_index].mime_type = GSMART_FILE_TYPE_IMAGE;
				lib->files[file_index].fat_end = index;
			} else if (type == 0x08) {	/* its the start of an avi */
				snprintf (buf, 13, "Movie%03d.avi", ++lib->num_movies);
				lib->files[file_index].mime_type = GSMART_FILE_TYPE_AVI;
			}
			lib->files[file_index].fat = p;
			lib->files[file_index].fat_start = index;
			lib->files[file_index].name = strdup (buf);
			lib->files[file_index].width = (p[8] & 0xFF) * 16;
			lib->files[file_index].height = (p[9] & 0xFF) * 16;
			lib->files[file_index].name = strdup (buf);
			file_index++;
		} else if (type == 0x80) {	/* continuation of an avi */
			lib->files[file_index - 1].fat_end = index;
		}
		p += FLASH_PAGE_SIZE;
		index++;
	}

	return GP_OK;
}

static int
yuv2rgb (int y, int u, int v, int *_r, int *_g, int *_b)
{
	double r, g, b;

	r = (char) y + 128 + 1.402 * (char) v;
	g = (char) y + 128 - 0.34414 * (char) u - 0.71414 * (char) v;
	b = (char) y + 128 + 1.772 * (char) u;

	if (r > 255)
		r = 255;
	if (g > 255)
		g = 255;
	if (b > 255)
		b = 255;
	if (r < 0)
		r = 0;
	if (g < 0)
		g = 0;
	if (b < 0)
		b = 0;

	*_r = r;
	*_g = g;
	*_b = b;

	return GP_OK;
}
