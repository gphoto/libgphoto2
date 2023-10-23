/****************************************************************/
/* gsmart300.c - Gphoto2 library for the Mustek gSmart 300      */
/*                                                              */
/* Copyright (C) 2002 Jérôme Lodewyck                           */
/*                                                              */
/* Author: Jérôme Lodewyck <jerome.lodewyck@ens.fr>             */
/*                                                              */
/* based on code by: Till Adam <till@adam-lilienthal.de>        */
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
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/

#define _DEFAULT_SOURCE

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gphoto2/gphoto2.h>
#include "libgphoto2/gphoto2-endian.h"

#include "gsmart300.h"
#include "gsmart300-jpeg-header.h"

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

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#define sleep(x)
#endif

#define GP_MODULE "gsmart300"
static int gsmart300_download_data (CameraPrivateLibrary * lib,
		int data_type, uint16_t index, unsigned int size,
		uint8_t * buf);
static int gsmart300_get_FATs (CameraPrivateLibrary * lib);
static int yuv2rgb (int y, int u, int v, unsigned int *r, unsigned int *g, unsigned int *b);
static int gsmart300_get_image_thumbnail (CameraPrivateLibrary * lib,
				       CameraFile *file,
				       struct GsmartFile *g_file);

#define __GS300_FAT		0
#define __GS300_THUMB		1
#define __GS300_PIC		2
#define __GS300_INIT		3

static int
gsmart300_get_file_count (CameraPrivateLibrary * lib)
{
	uint8_t buf[0x100];

	gsmart300_download_data (lib, __GS300_INIT,  0, 0x100, buf);
	lib->num_files = (buf[21] >> 4) * 10 + (buf[21] & 0xf)
			+ 100 * ((buf[22] >> 4) * 10 + (buf[22] & 0xf));
	return (GP_OK);
}

int
gsmart300_delete_file (CameraPrivateLibrary * lib, unsigned int index)
{
	struct GsmartFile *g_file;
	uint16_t fat_index;

	CHECK (gsmart300_get_file_info (lib, index, &g_file));

	fat_index = 0x1FFF - index;

	CHECK (gp_port_usb_msg_write (lib->gpdev, 0x03, fat_index, 0x0003,
				      NULL, 0));
	sleep (1);

	/* Reread fats the next time it is accessed */
	lib->dirty = 1;

	return GP_OK;
}

/* GSmart 300 specific */
int
gsmart300_delete_all (CameraPrivateLibrary * lib)
{
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0x02, 0x0000, 0x0004,
				      NULL, 0));
	sleep (3);

	/* Reread fats the next time it is accessed */
	lib->dirty = 1;

	return GP_OK;
}

int
gsmart300_request_file (CameraPrivateLibrary * lib, CameraFile *file,
		     unsigned int number)
{
	struct GsmartFile *g_file;
	uint8_t *p, *lp_jpg, *start_of_file;
	uint8_t qIndex, value;
	uint8_t *mybuf;
	int i, ret;
	/* NOTE : these variables are slightly renamed */
	int flash_size, data_size, file_size;

	CHECK (gsmart300_get_file_info (lib, number, &g_file));

	p = g_file->fat;

	/* decode the image size */
	flash_size = (p[6] * 0x100 + p[5])*0x200;
	data_size = (p[13] & 0xff) * 0x10000
		  + (p[12] & 0xff) * 0x100
		  + (p[11] & 0xff);
	qIndex = p[7] & 0x07;

	if (qIndex >= 5) {
		GP_DEBUG("qIndex %d is larger or equal than 5\n", qIndex);
		return GP_ERROR_CORRUPTED_DATA;
	}

	file_size = data_size + GSMART_JPG_DEFAULT_HEADER_LENGTH + 1024 * 10;

	if (flash_size < data_size) {
		GP_DEBUG("flash_size %d is smaller than data_size %d\n", flash_size, data_size);
		return GP_ERROR_CORRUPTED_DATA;
	}

	/* slurp in the image */
	mybuf = malloc (flash_size);
	if (!mybuf)
		return GP_ERROR_NO_MEMORY;

	ret = gsmart300_download_data (lib, __GS300_PIC, g_file->index,
				        flash_size, mybuf);
	if (ret < GP_OK) {
		free (mybuf);
		return ret;
	}

	/* now build a jpeg */
	lp_jpg = malloc (file_size);
	if (!lp_jpg) {
		free (mybuf);
		return GP_ERROR_NO_MEMORY;
	}
	start_of_file = lp_jpg;

	/* copy the header from the template */
	memcpy (lp_jpg, Gsmart_300_JPGDefaultHeader,
			GSMART_JPG_DEFAULT_HEADER_LENGTH);

	/* modify quantization table */
	memcpy (lp_jpg + 7, Gsmart_300_QTable[qIndex * 2], 64);
	memcpy (lp_jpg + 72, Gsmart_300_QTable[qIndex * 2 + 1], 64);

	/* modify the image width, height */

	/* NOTE : the picture width and height written on flash is either
	 * 640x480 or 320x240, but the real size id 640x480 for ALL pictures */

	*(lp_jpg + 564) = (640) & 0xFF;      /* Image width low byte */
	*(lp_jpg + 563) = (640 >> 8) & 0xFF; /* Image width high byte */
	*(lp_jpg + 562) = (480) & 0xFF;      /* Image height low byte */
	*(lp_jpg + 561) = (480 >> 8) & 0xFF; /* Image height high byte */

	/* point to real JPG compress data start position and copy */
	lp_jpg += GSMART_JPG_DEFAULT_HEADER_LENGTH;

	for (i = 0; i < data_size; i++) {
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
	gp_file_append (file, (char*)start_of_file, lp_jpg - start_of_file);
	free (start_of_file);
	return (GP_OK);
}

int
gsmart300_request_thumbnail (CameraPrivateLibrary * lib, CameraFile *file,
			  unsigned int number, int *type)
{
	struct GsmartFile *g_file;

	CHECK (gsmart300_get_file_info (lib, number, &g_file));

	*type = g_file->mime_type;
	return (gsmart300_get_image_thumbnail (lib, file, g_file));
}



static int
gsmart300_get_image_thumbnail (CameraPrivateLibrary * lib, CameraFile *file,
			    struct GsmartFile *g_file)
{
	unsigned int size;
	uint8_t *mybuf = NULL;
	uint8_t *tmp, *buf;
	unsigned int t_width, t_height;
	uint8_t *yuv_p, *rgb_p;
	uint32_t len;
	char pbm_header[14];
	int ret;


	/* No thumbnail for 320x240 pictures */
	if (g_file->width < 640)
		return (GP_ERROR_NOT_SUPPORTED);

	t_width = 80;
	t_height = 60;
	snprintf (pbm_header, sizeof (pbm_header), "P6 %d %d 255\n",
		  t_width, t_height);

	/* size needed for download */
	size = 9728;

	mybuf = malloc (size);
	if (!mybuf)
		return (GP_ERROR_NO_MEMORY);
	ret = gsmart300_download_data (lib, __GS300_THUMB, g_file->index,
				        size, mybuf);
	if (ret < GP_OK) {
		free (mybuf);
		return ret;
	}
	/* effective size of file */
	size = 9600;

	len = t_width * t_height * 3;
	buf = malloc (len);
	if (!buf) {
		free (mybuf);
		return (GP_ERROR_NO_MEMORY);
	}

	gp_file_append (file, pbm_header, strlen(pbm_header));

	tmp = buf;
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
	gp_file_append (file, (char*)buf, len);
	free (buf);
	return (GP_OK);
}

int
gsmart300_get_info (CameraPrivateLibrary * lib)
{
	GP_DEBUG ("* gsmart300_get_info");
	CHECK (gsmart300_get_file_count (lib));
	if (lib->num_files > 0) {
		CHECK (gsmart300_get_FATs (lib));
	}

	lib->dirty = 0;

	return GP_OK;
}

int
gsmart300_get_file_info (CameraPrivateLibrary * lib, unsigned int index,
		      struct GsmartFile **g_file)
{
	if (lib->dirty)
		CHECK (gsmart300_get_info (lib));
	*g_file = &(lib->files[index]);
	return GP_OK;
}

int
gsmart300_reset (CameraPrivateLibrary * lib)
{
	GP_DEBUG ("* gsmart300_reset");
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0x02, 0x0000, 0x0003,
				      NULL, 0));
	sleep (1);
	return GP_OK;
}


/* Download procedure for GSmart300 */
/* buf must be of length n*0x200 */
/* Pictured are pointed by there index which runs over 0 to npic -1*/
static int gsmart300_download_data (CameraPrivateLibrary * lib, int data_type,
				 uint16_t index, unsigned int size, uint8_t * buf)
{
	uint16_t fat_index = 0x1fff - index;
	unsigned int i;

	if (data_type == __GS300_FAT)
		CHECK (gp_port_usb_msg_write (lib->gpdev, 0x03,
					      fat_index, 0x0000, NULL, 0));
	if (data_type == __GS300_THUMB)
		CHECK (gp_port_usb_msg_write (lib->gpdev, 0x0a,
					      fat_index, 0x0003, NULL, 0));
	if (data_type == __GS300_PIC)
		CHECK (gp_port_usb_msg_write (lib->gpdev, 0x03,
					      fat_index, 0x0008, NULL, 0));
	if (data_type == __GS300_INIT  ) {
		CHECK (gp_port_usb_msg_write (lib->gpdev, 0x02, 0x00,
					      0x0007, NULL, 0));
		CHECK (gp_port_usb_msg_write (lib->gpdev, 0x0a, 0x00,
					      0x0001, NULL, 0));
	}

	for (i = 0; i < size >> 8; i++)
		CHECK (gp_port_read (lib->gpdev, (char*)(buf + i*0x100), 0x100));

	return GP_OK;
}

static int
gsmart300_get_FATs (CameraPrivateLibrary * lib)
{
	uint8_t type;
	int index = 0;
	unsigned int file_index = 0;
	uint8_t *p = NULL;
	char buf[14];

	CHECK (gsmart300_get_file_count(lib));

	if (lib->fats)
		free (lib->fats);
	lib->fats = calloc (lib->num_files,FLASH_PAGE_SIZE_300);

	if (lib->files)
		free (lib->files);
	lib->files = calloc (lib->num_files,sizeof (struct GsmartFile));

	p = lib->fats;

	while (index < lib->num_files) {
		CHECK (gsmart300_download_data (lib, __GS300_FAT, index,
					        FLASH_PAGE_SIZE_300, p));
		if (p[0] == 0xFF)
			break;
		type = p[0];
		if (type == 0x00) {
			snprintf (buf, 13, "Image%03d.jpg", index + 1);
			lib->files[file_index].mime_type =
				GSMART_FILE_TYPE_IMAGE;
			lib->files[file_index].index = index;
			lib->files[file_index].fat = p;
			lib->files[file_index].width = (p[8] & 0xFF) * 16;
			lib->files[file_index].height = (p[9] & 0xFF) * 16;
			lib->files[file_index].name = strdup (buf);
			file_index++;
		}

		p += FLASH_PAGE_SIZE_300;
		index++;
	}

	return GP_OK;
}

static int
yuv2rgb (int y, int u, int v, unsigned int *_r, unsigned int *_g, unsigned int *_b)
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
