/****************************************************************/
/* spca50x_sdram.c - Gphoto2 library for cameras with sunplus   */
/*             spca50x chips                                    */
/*                                                              */
/* Copyright 2002, 2003 Till Adam                               */
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
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/
#define _BSD_SOURCE

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <gphoto2/gphoto2.h>
#include "gphoto2-endian.h"

#include "spca50x.h"
#include "spca50x-sdram.h"
#include "spca50x-registers.h"
#include "spca50x-avi-header.h"

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
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define GP_MODULE "spca50x"
static int spca50x_mode_set_idle (CameraPrivateLibrary * lib);
static int spca50x_is_idle (CameraPrivateLibrary * lib);
static int spca50x_mode_set_download (CameraPrivateLibrary * lib);
static int spca50x_download_data (CameraPrivateLibrary * lib, uint32_t start,
				 unsigned int size, uint8_t * buf);
static int spca50x_get_FATs (CameraPrivateLibrary * lib, int dramtype);
static int spca50x_sdram_get_file_count_and_fat_count
                            (CameraPrivateLibrary * lib, int dramtype);
static int spca50x_get_avi_thumbnail (CameraPrivateLibrary * lib,
				     uint8_t ** buf, unsigned int *len,
				     struct SPCA50xFile *g_file);
static int spca50x_get_image_thumbnail (CameraPrivateLibrary * lib,
				       uint8_t ** buf, unsigned int *len,
				       struct SPCA50xFile *g_file);
static int spca50x_get_avi (CameraPrivateLibrary * lib, uint8_t ** buf,
			   unsigned int *len, struct SPCA50xFile *g_file);
static int spca50x_get_image (CameraPrivateLibrary * lib, uint8_t ** buf,
			     unsigned int *len, struct SPCA50xFile *g_file);
static inline uint8_t *put_dword (uint8_t * ptr, uint32_t value);


static int
spca50x_sdram_get_fat_page (CameraPrivateLibrary * lib, int index,
		            int dramtype, uint8_t *p)
{
	switch (dramtype) {
		case 4:	/* 128 Mbit */
			CHECK (spca50x_download_data
					(lib, 0x7fff80 -
					 index * 0x80,
					 SPCA50X_FAT_PAGE_SIZE, p));
			break;
		case 3:	/* 64 Mbit */
			CHECK (spca50x_download_data
					(lib, 0x3fff80 -
					 index * 0x80,
					 SPCA50X_FAT_PAGE_SIZE, p));
			break;
		default:
			break;
	}

	return GP_OK;
}

static int
spca50x_sdram_get_file_count_and_fat_count (CameraPrivateLibrary * lib,
		                            int dramtype)
{
	uint8_t theFat[256];

	lib->num_fats = 0;
	lib->num_files_on_sdram = 0;

	if (lib->bridge == BRIDGE_SPCA500){
		uint8_t lower, upper;

		CHECK (gp_port_usb_msg_write (lib->gpdev, 0x5, 0, 0, NULL, 0));
		sleep (1);
		CHECK (gp_port_usb_msg_read
				(lib->gpdev, 0, 0, 0xe15,
				 (char *) & lib->num_files_on_sdram, 1));
		LE32TOH (lib->num_files_on_sdram);

		/*  get fatscount */
		CHECK (gp_port_usb_msg_write
				(lib->gpdev, 0x05, 0x0000, 0x0008, NULL, 0));
		sleep (1);
		CHECK (gp_port_usb_msg_read
				(lib->gpdev, 0, 0, 0x0e19,
				 (char *)&lower, 1));
		CHECK (gp_port_usb_msg_read
				(lib->gpdev, 0, 0, 0x0e20,
				 (char *)&upper, 1));

		lib->num_fats = (((upper & 0xFF) << 8) | (lower & 0xFF));
	} else {
		while (1) {
			CHECK (spca50x_sdram_get_fat_page (lib, lib->num_fats,
					            dramtype, theFat));
			if (theFat[0] == 0xFF)
				break;

			if (theFat[0] == 0x08 || theFat[0] == 0x00)
				lib->num_files_on_sdram++;

			lib->num_fats++;
		}
 	}
	return (GP_OK);
}

int
spca50x_sdram_delete_file (CameraPrivateLibrary * lib, unsigned int index)
{
	struct SPCA50xFile *g_file;
	uint16_t fat_index;

	CHECK (spca50x_sdram_get_file_info (lib, index, &g_file));

	if (lib->bridge == BRIDGE_SPCA500)
		fat_index = 0x70FF - g_file->fat_start - 1;
	else			/* if (lib->bridge == BRIDGE_SPCA504) */
		fat_index = 0xD8000 - g_file->fat_start - 1;

	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0x06, fat_index, 0x0007, NULL, 0));
	sleep (1);

	/* Reread fats the next time it is accessed */
	lib->dirty_sdram = 1;

	return GP_OK;
}

int
spca50x_sdram_delete_all (CameraPrivateLibrary * lib)
{
	if (lib->fw_rev == 2) {
		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x71, 0x0000, 0x0000, NULL, 0));

	} else {
		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x02, 0x0000, 0x0005, NULL, 0));
	}
	sleep (3);

	/* Reread fats the next time it is accessed */
	lib->dirty_sdram = 1;
	return GP_OK;
}

int
spca50x_sdram_request_file (CameraPrivateLibrary * lib, uint8_t ** buf,
		     unsigned int *len, unsigned int number, int *type)
{
	struct SPCA50xFile *g_file;

	CHECK (spca50x_sdram_get_file_info (lib, number, &g_file));

	*type = g_file->mime_type;
	if (g_file->mime_type == SPCA50X_FILE_TYPE_AVI)
		return (spca50x_get_avi (lib, buf, len, g_file));
	else
		return (spca50x_get_image (lib, buf, len, g_file));
}

static int
spca50x_get_image (CameraPrivateLibrary * lib, uint8_t ** buf,
		  unsigned int *len, struct SPCA50xFile *g_file)
{
	uint8_t *p, *lp_jpg;
	uint8_t qIndex = 0, format;
	uint32_t start;
	uint8_t *mybuf;
	int size, o_size, file_size, ret;
	int omit_escape = 0;

	p = g_file->fat;

	/* get the position in memory where the image is */
	start = (p[1] & 0xff) + (p[2] & 0xff) * 0x100;
	start *= 128;

	/* decode the image size */
	if (lib->bridge == BRIDGE_SPCA500) {
		o_size = size = (p[5] & 0xff) * 0x100 + (p[6] & 0xff) * 0x10000;
		qIndex = p[7] & 0x0f;
	} else {
		o_size = size =
				(p[13] & 0xff) * 0x10000
				+ (p[12] & 0xff) * 0x100
				+ (p[11] & 0xff);
		if (lib->fw_rev == 1) {
			qIndex = p[7] & 0x0f;
		} else if (lib->fw_rev == 2) {
			omit_escape = 1;
			qIndex = p[10] & 0x0f;
		}
	}

	format = 0x21;

	/* align */
	if (size % 64 != 0)
		size = ((size / 64) + 1) * 64;

	file_size = size + SPCA50X_JPG_DEFAULT_HEADER_LENGTH + 1024 * 10;

	/* slurp in the image */
	mybuf = malloc (size);
	if (!mybuf)
		return GP_ERROR_NO_MEMORY;

	if (lib->bridge == BRIDGE_SPCA504) {
		ret = spca50x_download_data (lib, start, size, mybuf);
		if (ret < GP_OK) {
			free (mybuf);
			return ret;
		}
	} else if (lib->bridge == BRIDGE_SPCA500) {
		/* find the file index on the camera */
		int index;

		index = (g_file->fat - lib->fats) / SPCA50X_FAT_PAGE_SIZE;
		spca50x_reset (lib);
		/* trigger upload of image at that index */
		ret = gp_port_usb_msg_write (lib->gpdev, 0x06,
					      0x70FF - index, 0x01, NULL, 0);
		if (ret < GP_OK) {
			free (mybuf);
			return ret;
		}
		sleep (1);
		ret = gp_port_read (lib->gpdev, (char *)mybuf, size);
		if (ret < GP_OK) {
			free (mybuf);
			return ret;
		}
		/* the smallest ones are in a different format */
		if ((p[20] & 0xff) == 2)
			format = 0x22;
	}
	/* now build a jpeg */
	lp_jpg = malloc (file_size);
	if (!lp_jpg) {
		free (mybuf);
		return GP_ERROR_NO_MEMORY;
	}
	create_jpeg_from_data (lp_jpg, mybuf, qIndex, g_file->width,
			       g_file->height, format, o_size, &file_size,
			       0, omit_escape);

	free (mybuf);
	lp_jpg = realloc (lp_jpg, file_size);
	*buf = lp_jpg;
	*len = file_size;

	return (GP_OK);
}

static int
spca50x_get_avi (CameraPrivateLibrary * lib, uint8_t ** buf,
		unsigned int *len, struct SPCA50xFile *g_file)
{
	int i, j, length, ret;
	int frame_count = 0, frames_per_fat = 0, fn = 0;
	int size = 0;
	int file_size;
	int index_size;
	uint32_t frame_size = 0, frame_width = 0, frame_height = 0;
	uint32_t total_frame_size = 0;
	uint32_t start = 0;
	uint8_t *p, *mybuf, *avi, *start_of_file, *start_of_frame, *data;
	uint8_t qIndex;
	uint8_t *avi_index, *avi_index_ptr;
	uint8_t index_item[16];

	/* FIXME */
	if (lib->bridge == BRIDGE_SPCA500)
		return GP_ERROR_NOT_SUPPORTED;

	p = g_file->fat;

	if (lib->fw_rev == 2)
		qIndex = p[10] & 0x0f;
	else
		qIndex = p[7] & 0x0f;

	avi = mybuf = start_of_file = data = NULL;

	/* get the position in memory where the movie is */
	start = (p[1] & 0xff) + (p[2] & 0xff) * 0x100;
	start *= 128;

	/* Frame w and h and qIndex dont change, so just use the values
	 * of the first frame */
	frame_width = (p[8] & 0xFF) * 16;
	frame_height = (p[9] & 0xFF) * 16;

	/* Each movie consists of a number of frames, and can have more than
	 * one fat page. Iterate over all fat pages for this movie and count
	 * all frames in all fats. Same for size. */
	for (i = g_file->fat_start; i <= g_file->fat_end; i++) {
		frames_per_fat = (p[49] & 0xFF) * 0x100 + (p[48] & 0xFF);
		frame_count += frames_per_fat;
		size += (p[13] & 0xFF) * 0x10000 + (p[12] & 0xFF) * 0x100 +
			(p[11] & 0xFF);
		if (frames_per_fat < 60)
			break;
		p += SPCA50X_FAT_PAGE_SIZE;
	}
	/* align */
	size = (size + 63) & 0xffffffc0;

	/* mem for index */
	index_size = frame_count * 16;
	avi_index_ptr = avi_index = malloc (index_size);
	if (!avi_index)
		return GP_ERROR_NO_MEMORY;

	/* slurp in the movie */
	mybuf = malloc (size);
	if (!mybuf) {
		free (avi_index);
		return GP_ERROR_NO_MEMORY;
	}

	ret = spca50x_download_data (lib, start, size, mybuf);
	if (ret < GP_OK) {
		free (avi_index);
		free (mybuf);
		return ret;
	}

	/* Now write our data to a file with an avi header */
	file_size = size + SPCA50X_AVI_HEADER_LENGTH
		+ (SPCA50X_JPG_DEFAULT_HEADER_LENGTH * frame_count)
		+ 8 + index_size	/* for index chunk */
		+ 1024 * 10 * frame_count;

	avi = malloc (file_size);
	if (!avi) {
		free (avi_index);
		free (mybuf);
		return GP_ERROR_NO_MEMORY;
	}

	start_of_file = avi;

	/* prepare index item */
	put_dword (index_item, 0x63643030);	/* 00dc */
	put_dword (index_item + 4, 0x10);	/*  KEYFRAME */

	/* copy the header from the template */
	memcpy (avi, SPCA50xAviHeader, SPCA50X_AVI_HEADER_LENGTH);

	/* put the width and height into the riff header */
	put_dword(avi + 0x40, frame_width);
	put_dword(avi + 0x44, frame_height);

	/* and at 0xb0 and 0xb4 */
	put_dword(avi + 0xb0, frame_width);
	put_dword(avi + 0xb4, frame_height);

	avi += SPCA50X_AVI_HEADER_LENGTH;

	/* Reset to the first fat */
	p = g_file->fat;
	data = mybuf;

	/* Iterate over fats and frames in each fat and build a jpeg for
	 * each frame. Add the jpeg to the avi and write an index entry. */
	for (i = g_file->fat_start; i <= g_file->fat_end; i++) {
		frames_per_fat = ((p[49] & 0xFF) * 0x100 + (p[48] & 0xFF));

		/* frames per fat might be lying, so double check how
		   many frames were already processed */
		if (frames_per_fat > 60 || frames_per_fat == 0
		    || fn + frames_per_fat > frame_count)
			break;

		for (j = 0; j < frames_per_fat; j++) {
			frame_size = ((p[52 + j * 3] & 0xFF) * 0x10000)
				+ ((p[51 + j * 3] & 0xFF) * 0x100) +
				(p[50 + j * 3] & 0xFF);

			memcpy (avi, SPCA50xAviFrameHeader,
					SPCA50X_AVI_FRAME_HEADER_LENGTH);

			avi += SPCA50X_AVI_FRAME_HEADER_LENGTH;
			start_of_frame = avi;

			/* jpeg starts here */
			create_jpeg_from_data (avi, data, qIndex, frame_width,
					       frame_height, 0x22, frame_size,
					       &length, 1, 0);

			data += (frame_size + 7) & 0xfffffff8;
			avi += length;
			/* Make sure the next frame is aligned */
			if ((avi - start_of_frame) % 2 != 0)
				avi++;

			/* Now we know the real frame size after escaping and
			 * adding the EOI marker. Put it in the frame header. */
			frame_size = avi - start_of_frame;
			put_dword (start_of_frame - 4, frame_size);
			put_dword (index_item + 8,
				   start_of_frame - start_of_file -
				   SPCA50X_AVI_HEADER_LENGTH - 4);
			put_dword (index_item + 12, frame_size);
			memcpy (avi_index_ptr, index_item, 16);
			avi_index_ptr += 16;
			fn++;
		}
		p += SPCA50X_FAT_PAGE_SIZE;
	}
	total_frame_size = avi - (start_of_file + SPCA50X_AVI_HEADER_LENGTH - 4);
	put_dword (start_of_file + SPCA50X_AVI_HEADER_LENGTH - 8,
		   total_frame_size);

	avi = put_dword (avi, 0x31786469);	/* idx1 */
	avi = put_dword (avi, index_size);
	memcpy (avi, avi_index, index_size);
	avi += index_size;
	free (avi_index);
	put_dword (start_of_file + 0x30, frame_count);
	put_dword (start_of_file + 0x8c, frame_count);

	/* And reuse total_frame_size for the file size RIFF marker at offset
	 * 4 */
	total_frame_size = avi - (start_of_file + 4);
	put_dword (start_of_file + 4, total_frame_size);

	free (mybuf);
	start_of_file = realloc (start_of_file, avi - start_of_file);
	*buf = start_of_file;
	*len = avi - start_of_file;
	return (GP_OK);
}

int
spca50x_sdram_request_thumbnail (CameraPrivateLibrary * lib, uint8_t ** buf,
			  unsigned int *len, unsigned int number, int *type)
{
	struct SPCA50xFile *g_file;

	CHECK (spca50x_sdram_get_file_info (lib, number, &g_file));

	*type = g_file->mime_type;
	if (g_file->mime_type == SPCA50X_FILE_TYPE_AVI) {
		return (spca50x_get_avi_thumbnail (lib, buf, len, g_file));
	} else {

		/* the spca500 stores the quality in p[20], the rest of them
		 * in [p40].  We need to check for the smallest resolution of
		 * the mini, since resolution does not have thumbnails.
		 * Download the full image instead.
		 * Low:    320x240                2
		 * Middle: 640x480                0
		 * High:  1024x768 (interpolated) 1*/
		if (lib->bridge == BRIDGE_SPCA500
  		    && (g_file->fat[20] & 0xFF) == 2) {
  			return (spca50x_get_image (lib, buf, len, g_file));

		} else {
			return (spca50x_get_image_thumbnail
				(lib, buf, len, g_file));
		}
	}
}


static int
spca50x_get_avi_thumbnail (CameraPrivateLibrary * lib, uint8_t ** buf,
			  unsigned int *len, struct SPCA50xFile *g_file)
{
	uint8_t *p, *lp_jpg;
	uint8_t qIndex;
	uint32_t start;
	uint8_t *mybuf;
	int size, o_size, file_size;
	int ret;

	/* FIXME */
	if (lib->bridge == BRIDGE_SPCA500)
		return GP_ERROR_NOT_SUPPORTED;

	p = g_file->fat;

	/* get the position in memory where the image is */
	start = (p[1] & 0xff) + (p[2] & 0xff) * 0x100;
	start *= 128;

	/* decode the image size */
	o_size = size =
		(p[52] & 0xff) * 0x10000 + (p[51] & 0xff) * 0x100 +
		(p[50] & 0xff);
	qIndex = p[7] & 0x0f;

	/* align */
	if (size % 64 != 0)
		size = ((size / 64) + 1) * 64;

	file_size = size + SPCA50X_JPG_DEFAULT_HEADER_LENGTH + 1024 * 10;

	/* slurp in the image */
	mybuf = malloc (size);
	if (!mybuf)
		return GP_ERROR_NO_MEMORY;

	ret = spca50x_download_data (lib, start, size, mybuf);
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

	create_jpeg_from_data (lp_jpg, mybuf, qIndex, g_file->width,
			       g_file->height, 0x22, o_size, &file_size, 0, 0);
	free (mybuf);
	lp_jpg = realloc (lp_jpg, file_size);
	*buf = lp_jpg;
	*len = file_size;

	return (GP_OK);
}


static int
spca50x_get_image_thumbnail (CameraPrivateLibrary * lib, uint8_t ** buf,
			    unsigned int *len, struct SPCA50xFile *g_file)
{
	unsigned int size;
	uint8_t *p;
	uint32_t start;
	uint8_t *mybuf = NULL;
	uint8_t *tmp;
	unsigned int t_width, t_height;
	uint8_t *yuv_p;
	uint8_t *rgb_p;
	int headerlength, ret;

	p = g_file->fat;

	start = (p[3] & 0xff) + (p[4] & 0xff) * 0x100;
	start *= 128;

	size = g_file->width * g_file->height * 2 / 64;
	t_width = g_file->width / 8;
	t_height = g_file->height / 8;
	/* Adjust the headerlenght */
	headerlength = 13;
	if (t_width > 99) headerlength++;
	if (t_height > 99) headerlength++;

	/* align */
	if (size % 64 != 0)
		size = ((size / 64) + 1) * 64;

	mybuf = malloc (size);

	if (lib->bridge == BRIDGE_SPCA504) {
		ret = spca50x_download_data (lib, start, size, mybuf);
		if (ret < GP_OK) {
			free (mybuf);
			return ret;
		}
	} else if (lib->bridge == BRIDGE_SPCA500) {
		/* find the file index on the camera */
		int index;

		index = (g_file->fat - lib->fats) / SPCA50X_FAT_PAGE_SIZE;
		spca50x_reset (lib);
		/* trigger upload of thumbnail at that index */
		ret = gp_port_usb_msg_write (lib->gpdev, 0x06,
					      0x70FF - index, 0x09, NULL, 0);
		if (ret < GP_OK) {
			free (mybuf);
			return ret;
		}
		sleep (1);
		ret = gp_port_read (lib->gpdev, (char *)mybuf, size);
		if (ret < GP_OK) {
			free (mybuf);
			return ret;
		}
	}
	*len = t_width * t_height * 3 + headerlength;
	*buf = malloc (*len);
	if (!*buf) {
		free (mybuf);
		return (GP_ERROR_NO_MEMORY);
	}

	tmp = *buf;
	snprintf ((char*)tmp, *len, "P6 %d %d 255\n", t_width, t_height);
	tmp += headerlength;

	yuv_p = mybuf;
	rgb_p = tmp;
	while (yuv_p < mybuf + (t_width * t_height * 2)) {
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
spca50x_sdram_get_info (CameraPrivateLibrary * lib)
{
	unsigned int index;
	uint8_t dramtype = 0;
	uint8_t *p;
	uint32_t start_page, end_page;
	uint8_t file_type;

	GP_DEBUG ("* spca50x_sdram_get_info");

	if (lib->bridge == BRIDGE_SPCA504) {
		if (!spca50x_is_idle (lib))
			spca50x_mode_set_idle (lib);

		spca50x_mode_set_download (lib);

		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x00, 0x0001, SPCA50X_REG_AutoPbSize, NULL,
			0));

		CHECK (gp_port_usb_msg_read
		       (lib->gpdev, 0, 0, SPCA50X_REG_DramType,
			(char *) & dramtype, 1));
		dramtype &= 0xFF;
	}

	CHECK (spca50x_sdram_get_file_count_and_fat_count (lib, dramtype));

	if (lib->num_files_on_sdram > 0) {
		CHECK (spca50x_get_FATs (lib, dramtype));

		index = lib->files[lib->num_files_on_sdram - 1].fat_end;
		p = lib->fats + SPCA50X_FAT_PAGE_SIZE * index;
		/* p now points to the fat of the last image of the last file */

		file_type = p[0];
		start_page = ((p[2] & 0xFF) << 8) | (p[1] & 0xFF);
		end_page = start_page + (((p[6] & 0xFF) << 8) | (p[5] & 0xFF));
		if (file_type == SPCA50X_FILE_TYPE_IMAGE)
			end_page = end_page + 0xA0;

		lib->size_used = (end_page - 0x2800) * SPCA50X_FAT_PAGE_SIZE;
	} else
		lib->size_used = 0;

	lib->size_free =
		16 * 1024 * 1024 - 0x2800 * SPCA50X_FAT_PAGE_SIZE - lib->size_used;
	lib->dirty_sdram = 0;

	return GP_OK;
}

int
spca50x_sdram_get_file_info (CameraPrivateLibrary * lib, unsigned int index,
		      struct SPCA50xFile **g_file)
{
	if (lib->dirty_sdram)
		CHECK (spca50x_sdram_get_info (lib));
	*g_file = &(lib->files[index]);
	return GP_OK;
}
static int
spca50x_mode_set_idle (CameraPrivateLibrary * lib)
{
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, SPCA50X_CamMode_Idle, SPCA50X_REG_CamMode, NULL,
		0));
	return GP_OK;
}

static int
spca50x_is_idle (CameraPrivateLibrary * lib)
{
	int mode;

	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, SPCA50X_REG_CamMode, (char *) & mode, 1));

	return mode == SPCA50X_CamMode_Idle ? 1 : 0;
}

static int
spca50x_mode_set_download (CameraPrivateLibrary * lib)
{
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, SPCA50X_CamMode_Upload, SPCA50X_REG_CamMode, NULL,
		0));
	return GP_OK;
}

static int
spca50x_download_data (CameraPrivateLibrary * lib, uint32_t start,
		      unsigned int size, uint8_t * buf)
{

	uint8_t foo;
	uint8_t vlcAddressL, vlcAddressM, vlcAddressH;

	if (!spca50x_is_idle (lib))
		spca50x_mode_set_idle (lib);

	spca50x_mode_set_download (lib);

	foo = size & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo,
				      SPCA50X_REG_SdramSizeL, NULL, 0));
	foo = (size >> 8) & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo,
				      SPCA50X_REG_SdramSizeM, NULL, 0));
	foo = (size >> 16) & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo,
				      SPCA50X_REG_SdramSizeH, NULL, 0));

	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, SPCA50X_REG_VlcAddressL,
		(char *) & vlcAddressL, 1));
	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, SPCA50X_REG_VlcAddressM,
		(char *) & vlcAddressM, 1));
	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, SPCA50X_REG_VlcAddressH,
		(char *) & vlcAddressH, 1));

	foo = start & 0xFF;
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, foo, SPCA50X_REG_VlcAddressL, NULL, 0));
	foo = (start >> 8) & 0xFF;
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, foo, SPCA50X_REG_VlcAddressM, NULL, 0));
	foo = (start >> 16) & 0xFF;
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, foo, SPCA50X_REG_VlcAddressH, NULL, 0));

	/* Set mode to ram -> usb */
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, SPCA50X_DramUsb, SPCA50X_REG_PbSrc, NULL, 0));
	/* and pull the trigger */
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, SPCA50X_TrigDramFifo, SPCA50X_REG_Trigger, NULL,
		0));

	CHECK (gp_port_read (lib->gpdev, (char *)buf, size));

	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, vlcAddressL, SPCA50X_REG_VlcAddressL, NULL, 0));
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, vlcAddressM, SPCA50X_REG_VlcAddressM, NULL, 0));
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, vlcAddressH, SPCA50X_REG_VlcAddressH, NULL, 0));

	spca50x_mode_set_idle (lib);
	return GP_OK;
}

static int
spca50x_get_FATs (CameraPrivateLibrary * lib, int dramtype)
{
	uint8_t type;
	unsigned int index = 0;
	unsigned int file_index = 0;
	uint8_t *p = NULL;
	uint8_t buf[14];

	/* Reset image and movie counter */
	lib->num_images = lib->num_movies = 0;

	if (lib->fats) {
		free (lib->fats);
		lib->fats = NULL;
	}

	if (lib->files) {
		free (lib->files);
		lib->files = NULL;
	}

	lib->fats = malloc (lib->num_fats * SPCA50X_FAT_PAGE_SIZE);
	lib->files = malloc (lib->num_files_on_sdram * sizeof (struct SPCA50xFile));

	p = lib->fats;
	if (lib->bridge == BRIDGE_SPCA504) {
		while (index < lib->num_fats) {
			CHECK (spca50x_sdram_get_fat_page (lib, index,
						dramtype, p));
			if (p[0] == 0xFF)
				break;

			index++;
			p += SPCA50X_FAT_PAGE_SIZE;
		}
	} else if (lib->bridge == BRIDGE_SPCA500) {
		/* for the spca500, download the whole fat in one go. */
		spca50x_reset (lib);
		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x05, 0x00, 0x07, NULL, 0));
		sleep (1);
		CHECK (gp_port_read
		       (lib->gpdev, (char *)lib->fats,
			lib->num_fats * SPCA50X_FAT_PAGE_SIZE));
	}

	p = lib->fats;
	index = 0;

	while (index < lib->num_fats) {

		type = p[0];
		/* While the spca504a indicates start of avi as 0x08 and cont.
		 * of avi as 0x80, the spca500 uses 0x03 for both. Each frame
		 * gets its own fat table with a sequence number at p[18]. */
		if ((type == 0x80) || (type == 0x03 && (p[18] != 0x00))) {
			/* continuation of an avi */
			lib->files[file_index - 1].fat_end = index;
		} else {
			/* its an image */
			if (type == 0x00 || type == 0x01) {
				snprintf ((char*)buf, 13, "Image%03d.jpg",
					  ++lib->num_images);
				lib->files[file_index].mime_type =
					SPCA50X_FILE_TYPE_IMAGE;
			} else if ((type == 0x08) || (type == 0x03)) {
				/* its the start of an avi */
				snprintf ((char*)buf, 13, "Movie%03d.avi",
					  ++lib->num_movies);
				lib->files[file_index].mime_type =
					SPCA50X_FILE_TYPE_AVI;
			}
			lib->files[file_index].fat = p;
			lib->files[file_index].fat_start = index;
			lib->files[file_index].fat_end = index;
			lib->files[file_index].name = strdup ((char*)buf);
			if (lib->bridge == BRIDGE_SPCA504) {
				lib->files[file_index].width =
					(p[8] & 0xFF) * 16;
				lib->files[file_index].height =
					(p[9] & 0xFF) * 16;
			} else if (lib->bridge == BRIDGE_SPCA500) {
				int w, h;

				if (p[20] == 2) {
					w = 320;
					h = 240;
				} else {
					w = 640;
					h = 480;
				}
				lib->files[file_index].width = w;
				lib->files[file_index].height = h;
			}
			lib->files[file_index].thumb = NULL;
			file_index++;
		}
		p += SPCA50X_FAT_PAGE_SIZE;
		index++;
	}

	return GP_OK;
}

static inline uint8_t *
put_dword (uint8_t * ptr, uint32_t value)
{
	ptr[0] = (value & 0xff);
	ptr[1] = (value & 0xff00) >> 8;
	ptr[2] = (value & 0xff0000) >> 16;
	ptr[3] = (value & 0xff000000) >> 24;
	return ptr + 4;
}
