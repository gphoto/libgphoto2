/****************************************************************/
/* gsmart.c - Gphoto2 library for the Mustek gSmart mini 2      */
/*                                                              */
/* Copyright © 2002 Till Adam                                 */
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
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gphoto2.h>
#include <gphoto2-endian.h>

#include "gsmart.h"
#include "gsmart-registers.h"
#include "gsmart-jpeg-header.h"
#include "gsmart-avi-header.h"

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

#define GP_MODULE "gsmartmini"
static int gsmart_mode_set_idle (CameraPrivateLibrary * lib);
static int gsmart_is_idle (CameraPrivateLibrary * lib);
static int gsmart_mode_set_download (CameraPrivateLibrary * lib);
static int gsmart_download_data (CameraPrivateLibrary * lib, u_int32_t start,
				 unsigned int size, u_int8_t * buf);
static int gsmart_get_FATs (CameraPrivateLibrary * lib);
static int gsmart_get_file_count_and_fat_count (CameraPrivateLibrary * lib);
static int yuv2rgb (int y, int u, int v, int *r, int *g, int *b);
static void create_jpeg_from_data (u_int8_t * dst, u_int8_t * src, int qIndex,
				   int w, int h, u_int8_t format,
				   int original_size, int *size,
				   int omit_huffman_table, int omit_escape);
static inline u_int8_t *put_dword (u_int8_t * ptr, u_int32_t value);
static int gsmart_get_avi_thumbnail (CameraPrivateLibrary * lib,
				     u_int8_t ** buf, unsigned int *len,
				     struct GsmartFile *g_file);
static int gsmart_get_image_thumbnail (CameraPrivateLibrary * lib,
				       u_int8_t ** buf, unsigned int *len,
				       struct GsmartFile *g_file);
static int gsmart_get_avi (CameraPrivateLibrary * lib, u_int8_t ** buf,
			   unsigned int *len, struct GsmartFile *g_file);
static int gsmart_get_image (CameraPrivateLibrary * lib, u_int8_t ** buf,
			     unsigned int *len, struct GsmartFile *g_file);

int
gsmart_get_file_count_and_fat_count (CameraPrivateLibrary * lib)
{
	u_int8_t theFat504B[256];

	lib->num_fats = 0;

	if (lib->bridge == GSMART_BRIDGE_SPCA504B) {
		while (1) {	/* assume dramtype=4 */
			CHECK (gsmart_download_data
			       (lib, 0x7fff80 - lib->num_fats * 0x80,
				FLASH_PAGE_SIZE, theFat504B));
			if (theFat504B[0] == 0xFF)
				break;

			if (theFat504B[0] == 0x08 || theFat504B[0] == 0x00)
				lib->num_files++;

			lib->num_fats++;
		}
	} else {
		u_int8_t lower, upper;

		CHECK (gp_port_usb_msg_write (lib->gpdev, 0x5, 0, 0, NULL, 0));
		sleep (1);
		CHECK (gp_port_usb_msg_read
		       (lib->gpdev, 0, 0, 0xe15,
			(u_int8_t *) & lib->num_files, 1));
		LE32TOH (lib->num_files);

		/* get fatscount */
		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x05, 0x0000, 0x0008, NULL, 0));

		/* The spca500 is a little slower */
		if (lib->bridge == GSMART_BRIDGE_SPCA504A)
			sleep (0.1);
		else if (lib->bridge == GSMART_BRIDGE_SPCA500)
			sleep (1.0);

		CHECK (gp_port_usb_msg_read
		       (lib->gpdev, 0, 0, 0x0e19, (u_int8_t *) & lower, 1));
		CHECK (gp_port_usb_msg_read
		       (lib->gpdev, 0, 0, 0x0e20, (u_int8_t *) & upper, 1));

		lib->num_fats = ((upper & 0xFF << 8) | (lower & 0xFF));
		LE32TOH (lib->num_fats);

	}

	return (GP_OK);
}

int
gsmart_delete_file (CameraPrivateLibrary * lib, unsigned int index)
{
	struct GsmartFile *g_file;
	u_int16_t fat_index;

	CHECK (gsmart_get_file_info (lib, index, &g_file));

	if (lib->bridge == GSMART_BRIDGE_SPCA500)
		fat_index = 0x70FF - g_file->fat_start - 1;
	else			/* if (lib->bridge == GSMART_BRIDGE_SPCA504A) */
		fat_index = 0xD8000 - g_file->fat_start - 1;

	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0x06, fat_index, 0x0007, NULL, 0));
	sleep (1);

	/* Reread fats the next time it is accessed */
	lib->dirty = 1;

	return GP_OK;
}

int
gsmart_delete_all (CameraPrivateLibrary * lib)
{
	if (lib->bridge == GSMART_BRIDGE_SPCA504B) {
		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x71, 0x0000, 0x0000, NULL, 0));

	} else {
		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x02, 0x0000, 0x0005, NULL, 0));
	}
	sleep (3);

	/* Reread fats the next time it is accessed */
	lib->dirty = 1;
	return GP_OK;
}

int
gsmart_request_file (CameraPrivateLibrary * lib, u_int8_t ** buf,
		     unsigned int *len, unsigned int number, int *type)
{
	struct GsmartFile *g_file;

	CHECK (gsmart_get_file_info (lib, number, &g_file));

	*type = g_file->mime_type;
	if (g_file->mime_type == GSMART_FILE_TYPE_AVI)
		return (gsmart_get_avi (lib, buf, len, g_file));
	else
		return (gsmart_get_image (lib, buf, len, g_file));
}

int
gsmart_get_image (CameraPrivateLibrary * lib, u_int8_t ** buf,
		  unsigned int *len, struct GsmartFile *g_file)
{
	u_int8_t *p, *lp_jpg;
	u_int8_t qIndex, format;
	u_int32_t start;
	u_int8_t *mybuf;
	int size, o_size, file_size;
	int omit_escape = 0;

	p = g_file->fat;

	/* get the position in memory where the image is */
	start = (p[1] & 0xff) + (p[2] & 0xff) * 0x100;
	start *= 128;

	/* decode the image size */
	if (lib->bridge == GSMART_BRIDGE_SPCA504A) {
		o_size = size =
			(p[13] & 0xff) * 0x10000 + (p[12] & 0xff) * 0x100 +
			(p[11] & 0xff);
		qIndex = p[7] & 0x07;
	} else if (lib->bridge == GSMART_BRIDGE_SPCA504B) {
		o_size = size =
			(p[13] & 0xff) * 0x10000 + (p[12] & 0xff) * 0x100 +
			(p[11] & 0xff);
		omit_escape = 1;
		qIndex = p[10] & 0x07;
	} else {		/* if (lib->bridge == GSMART_BRIDGE_SPCA500) */
		o_size = size = (p[5] & 0xff) * 0x100 + (p[6] & 0xff);
		qIndex = p[7] & 0x07;
	}
	format = 0x21;

	/* align */
	if (size % 64 != 0)
		size = ((size / 64) + 1) * 64;

	file_size = size + GSMART_JPG_DEFAULT_HEADER_LENGTH + 1024 * 10;

	/* slurp in the image */
	mybuf = malloc (size);
	if (!mybuf)
		return GP_ERROR_NO_MEMORY;

	if (lib->bridge == GSMART_BRIDGE_SPCA504A
	    || lib->bridge == GSMART_BRIDGE_SPCA504B) {
		CHECK (gsmart_download_data (lib, start, size, mybuf));
	} else if (lib->bridge == GSMART_BRIDGE_SPCA500) {
		/* find the file index on the camera */
		int index;

		index = (g_file->fat - lib->fats) / FLASH_PAGE_SIZE;
		gsmart_reset (lib);
		/* trigger upload of image at that index */
		CHECK (gp_port_usb_msg_write (lib->gpdev, 0x06,
					      0x70FF - index, 0x01, NULL, 0));
		sleep (1);
		CHECK (gp_port_read (lib->gpdev, mybuf, size));
		/* the smallest ones are in a different format */
		if ((p[20] & 0xff) == 2)
			format = 0x22;
	}
	/* now build a jpeg */
	lp_jpg = malloc (file_size);
	if (!lp_jpg)
		return GP_ERROR_NO_MEMORY;

	create_jpeg_from_data (lp_jpg, mybuf, qIndex, g_file->width,
			       g_file->height, format, o_size, &file_size,
			       0, omit_escape);

	free (mybuf);
	lp_jpg = realloc (lp_jpg, file_size);
	*buf = lp_jpg;
	*len = file_size;

	return (GP_OK);
}

int
gsmart_get_avi (CameraPrivateLibrary * lib, u_int8_t ** buf,
		unsigned int *len, struct GsmartFile *g_file)
{
	int i, j, length;
	int frame_count = 0, frames_per_fat = 0, fn = 0;
	int size = 0;
	int frame_width, frame_height;
	int file_size;
	int index_size;
	u_int32_t frame_size = 0;
	u_int32_t total_frame_size = 0;
	u_int32_t start = 0;
	u_int8_t *p, *mybuf, *avi, *start_of_file, *start_of_frame, *data;
	u_int8_t qIndex;
	u_int8_t *avi_index, *avi_index_ptr;
	u_int8_t index_item[16];

	/* FIXME */
	if (lib->bridge == GSMART_BRIDGE_SPCA500)
		return GP_ERROR_NOT_SUPPORTED;

	p = g_file->fat;
	
	if (lib->bridge == GSMART_BRIDGE_SPCA504B)
		qIndex = p[10] & 0x07;
	else
		qIndex = p[7] & 0x07;

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
		p += FLASH_PAGE_SIZE;
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

	CHECK (gsmart_download_data (lib, start, size, mybuf));

	/* Now write our data to a file with an avi header */
	file_size = size + GSMART_AVI_HEADER_LENGTH
		+ (GSMART_JPG_DEFAULT_HEADER_LENGTH * frame_count)
		+ 8 + index_size	/* for index chunk */
		+ 1024 * 10 * frame_count;

	avi = malloc (file_size);
	if (!avi)
		return GP_ERROR_NO_MEMORY;

	start_of_file = avi;

	/* prepare index item */
	put_dword (index_item, 0x63643030);	/* 00dc */
	put_dword (index_item + 4, 0x10);	// KEYFRAME

	/* copy the header from the template */
	memcpy (avi, GsmartAviHeader, GSMART_AVI_HEADER_LENGTH);
	avi += GSMART_AVI_HEADER_LENGTH;

	/* Reset to the first fat */
	p = g_file->fat;
	data = mybuf;

	/* Iterate over fats and frames in each fat and build a jpeg for
	 * each frame. Add the jpeg to the avi and write an index entry.  */
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

			memcpy (avi, GsmartAviFrameHeader,
				GSMART_AVI_FRAME_HEADER_LENGTH);

			avi += GSMART_AVI_FRAME_HEADER_LENGTH;
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
				   GSMART_AVI_HEADER_LENGTH - 4);
			put_dword (index_item + 12, frame_size);
			memcpy (avi_index_ptr, index_item, 16);
			avi_index_ptr += 16;
			fn++;
		}
		p += FLASH_PAGE_SIZE;
	}
	total_frame_size = avi - (start_of_file + GSMART_AVI_HEADER_LENGTH - 4);
	put_dword (start_of_file + GSMART_AVI_HEADER_LENGTH - 8,
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
gsmart_request_thumbnail (CameraPrivateLibrary * lib, u_int8_t ** buf,
			  unsigned int *len, unsigned int number, int *type)
{
	struct GsmartFile *g_file;

	CHECK (gsmart_get_file_info (lib, number, &g_file));

	*type = g_file->mime_type;
	if (g_file->mime_type == GSMART_FILE_TYPE_AVI) {
		return (gsmart_get_avi_thumbnail (lib, buf, len, g_file));
	} else {

		/* the spca500 stores the quality in p[20], the rest of them
		 * in [p40].  We need to check for the smallest resolution of
		 * the mini, since resolution does not have thumbnails.
		 * Download the full image instead.
		 * Low:    320x240                2
		 * Middle: 640x480                0
		 * High:  1024x768 (interpolated) 1*/
		if (lib->bridge == GSMART_BRIDGE_SPCA500
		    && (g_file->fat[20] & 0xFF) == 2) {
			return (gsmart_get_image (lib, buf, len, g_file));

		} else {
			return (gsmart_get_image_thumbnail
				(lib, buf, len, g_file));
		}
	}
}


static int
gsmart_get_avi_thumbnail (CameraPrivateLibrary * lib, u_int8_t ** buf,
			  unsigned int *len, struct GsmartFile *g_file)
{
	u_int8_t *p, *lp_jpg;
	u_int8_t qIndex;
	u_int32_t start;
	u_int8_t *mybuf;
	int size, o_size, file_size;
	int w, h;

	/* FIXME */
	if (lib->bridge == GSMART_BRIDGE_SPCA500
	    || lib->bridge == GSMART_BRIDGE_SPCA504B)
		return GP_ERROR_NOT_SUPPORTED;

	p = g_file->fat;

	/* get the position in memory where the image is */
	start = (p[1] & 0xff) + (p[2] & 0xff) * 0x100;
	start *= 128;

	/* decode the image size */
	o_size = size =
		(p[52] & 0xff) * 0x10000 + (p[51] & 0xff) * 0x100 +
		(p[50] & 0xff);
	qIndex = p[7] & 0x07;
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

	create_jpeg_from_data (lp_jpg, mybuf, qIndex, g_file->width,
			       g_file->height, 0x22, o_size, &file_size, 0, 0);
	free (mybuf);
	lp_jpg = realloc (lp_jpg, file_size);
	*buf = lp_jpg;
	*len = file_size;

	return (GP_OK);
}


static int
gsmart_get_image_thumbnail (CameraPrivateLibrary * lib, u_int8_t ** buf,
			    unsigned int *len, struct GsmartFile *g_file)
{
	unsigned int size;
	u_int8_t *p;
	u_int32_t start;
	u_int8_t *mybuf = NULL;
	u_int8_t *tmp;
	unsigned int t_width, t_height;
	u_int8_t *yuv_p;
	u_int8_t *rgb_p;
	int headerlength;

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

	if (lib->bridge == GSMART_BRIDGE_SPCA504A
	    || lib->bridge == GSMART_BRIDGE_SPCA504B) {
		CHECK (gsmart_download_data (lib, start, size, mybuf));
	} else if (lib->bridge == GSMART_BRIDGE_SPCA500) {
		/* find the file index on the camera */
		int index;

		index = (g_file->fat - lib->fats) / FLASH_PAGE_SIZE;
		gsmart_reset (lib);
		/* trigger upload of thumbnail at that index */
		CHECK (gp_port_usb_msg_write (lib->gpdev, 0x06,
					      0x70FF - index, 0x09, NULL, 0));
		sleep (1);
		CHECK (gp_port_read (lib->gpdev, mybuf, size));
	}
	*len = t_width * t_height * 3 + headerlength;
	*len += 45;
	*buf = malloc (*len);
	if (!*buf)
		return (GP_ERROR_NO_MEMORY);

	tmp = *buf;
	snprintf (tmp, *len, "P6 %d %d 255\n", t_width, t_height);
	tmp += headerlength;

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
	CHECK (gsmart_get_file_count_and_fat_count (lib));
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

	lib->size_free =
		16 * 1024 * 1024 - 0x2800 * FLASH_PAGE_SIZE - lib->size_used;
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
gsmart_get_firmware_revision (CameraPrivateLibrary * lib)
{
	u_int8_t firmware = 0;

	CHECK (gp_port_usb_msg_read (lib->gpdev, 0x20, 0x0, 0x0, &firmware, 1));
	return firmware;
}

int
gsmart_reset (CameraPrivateLibrary * lib)
{
	GP_DEBUG ("* gsmart_reset");
	if (lib->bridge == GSMART_BRIDGE_SPCA500) {
		/* This is not reset but "Change Mode to Idle (Clear Buffer)"
		 * Cant hurt, I guess. */
		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x02, 0x0000, 0x07, NULL, 0));
	} else if (lib->bridge == GSMART_BRIDGE_SPCA504A) {
		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x02, 0x0000, 0x0003, NULL, 0));
	} else if (lib->bridge == GSMART_BRIDGE_SPCA504B) {
		CHECK (gp_port_usb_msg_write(lib->gpdev, 0, 1, 
					GSMART_REG_AutoPbSize, NULL, 0));
		CHECK (gp_port_usb_msg_write(lib->gpdev, 0, 0, 
					0x0d04, NULL, 0));
		CHECK (gp_port_usb_msg_write(lib->gpdev, 0x1e, 0, 0, NULL, 0));
	}
	sleep (1);
	return GP_OK;
}

int
gsmart_capture (CameraPrivateLibrary * lib)
{
	sleep (2);
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0x06, 0x0000, 0x0003, NULL, 0));
	sleep (3);
	return GP_OK;
}

static int
gsmart_mode_set_idle (CameraPrivateLibrary * lib)
{
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, GSMART_CamMode_Idle, GSMART_REG_CamMode, NULL,
		0));
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
	       (lib->gpdev, 0, GSMART_CamMode_Upload, GSMART_REG_CamMode, NULL,
		0));
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
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo,
				      GSMART_REG_SdramSizeL, NULL, 0));
	foo = (size >> 8) & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo,
				      GSMART_REG_SdramSizeM, NULL, 0));
	foo = (size >> 16) & 0xFF;
	CHECK (gp_port_usb_msg_write (lib->gpdev, 0, foo,
				      GSMART_REG_SdramSizeH, NULL, 0));

	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, GSMART_REG_VlcAddressL,
		(u_int8_t *) & vlcAddressL, 1));
	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, GSMART_REG_VlcAddressM,
		(u_int8_t *) & vlcAddressM, 1));
	CHECK (gp_port_usb_msg_read
	       (lib->gpdev, 0, 0, GSMART_REG_VlcAddressH,
		(u_int8_t *) & vlcAddressH, 1));

	foo = start & 0xFF;
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, foo, GSMART_REG_VlcAddressL, NULL, 0));
	foo = (start >> 8) & 0xFF;
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, foo, GSMART_REG_VlcAddressM, NULL, 0));
	foo = (start >> 16) & 0xFF;
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, foo, GSMART_REG_VlcAddressH, NULL, 0));

	/* Set mode to ram -> usb */
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, GSMART_DramUsb, GSMART_REG_PbSrc, NULL, 0));
	/* and pull the trigger */
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0, GSMART_TrigDramFifo, GSMART_REG_Trigger, NULL,
		0));

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
	unsigned int index = 0;
	unsigned int file_index = 0;
	u_int8_t *p = NULL;
	u_int8_t buf[14];

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

	lib->fats = malloc (lib->num_fats * FLASH_PAGE_SIZE);
	lib->files = malloc (lib->num_files * sizeof (struct GsmartFile));

	p = lib->fats;
	if (lib->bridge == GSMART_BRIDGE_SPCA504A
	    || lib->bridge == GSMART_BRIDGE_SPCA504B) {
		if (!gsmart_is_idle (lib))
			gsmart_mode_set_idle (lib);

		gsmart_mode_set_download (lib);

		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x00, 0x0001, GSMART_REG_AutoPbSize, NULL,
			0));
		CHECK (gp_port_usb_msg_read
		       (lib->gpdev, 0, 0, GSMART_REG_DramType,
			(u_int8_t *) & dramtype, 1));

		dramtype &= 0xFF;

		while (index < lib->num_fats) {
			switch (dramtype) {
				case 4:	/* 128 Mbit */
					CHECK (gsmart_download_data
					       (lib, 0x7fff80 - index * 0x80,
						FLASH_PAGE_SIZE, p));
					break;
				case 3:	/* 64 Mbit */
					CHECK (gsmart_download_data
					       (lib, 0x3fff80 - index * 0x80,
						FLASH_PAGE_SIZE, p));
					break;
				default:
					break;
			}
			if (p[0] == 0xFF)
				break;

			index++;
			p += FLASH_PAGE_SIZE;
		}
	} else if (lib->bridge == GSMART_BRIDGE_SPCA500) {
		/* for the spca500, download the whole fat in one go. */
		gsmart_reset (lib);
		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x05, 0x00, 0x07, NULL, 0));
		sleep (1);
		CHECK (gp_port_read
		       (lib->gpdev, lib->fats,
			lib->num_fats * FLASH_PAGE_SIZE));
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
				snprintf (buf, 13, "Image%03d.jpg",
					  ++lib->num_images);
				lib->files[file_index].mime_type =
					GSMART_FILE_TYPE_IMAGE;
			} else if ((type == 0x08) || (type == 0x03)) {
				/* its the start of an avi */
				snprintf (buf, 13, "Movie%03d.avi",
					  ++lib->num_movies);
				lib->files[file_index].mime_type =
					GSMART_FILE_TYPE_AVI;
			}
			lib->files[file_index].fat = p;
			lib->files[file_index].fat_start = index;	
			lib->files[file_index].fat_end = index;
			lib->files[file_index].name = strdup (buf);
			if (lib->bridge == GSMART_BRIDGE_SPCA504A
			    || lib->bridge == GSMART_BRIDGE_SPCA504B) {
				lib->files[file_index].width =
					(p[8] & 0xFF) * 16;
				lib->files[file_index].height =
					(p[9] & 0xFF) * 16;
			} else if (lib->bridge == GSMART_BRIDGE_SPCA500) {
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
			file_index++;
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

static void
create_jpeg_from_data (u_int8_t * dst, u_int8_t * src, int qIndex, int w,
		       int h, u_int8_t format, int o_size, int *size,
		       int omit_huffman_table, int omit_escape)
{

	int i = 0;
	u_int8_t *start;
	u_int8_t value;

	start = dst;
	/* copy the header from the template */
	memcpy (dst, GsmartJPGDefaultHeaderPart1,
		GSMART_JPG_DEFAULT_HEADER_PART1_LENGTH);

	/* modify quantization table */
	memcpy (dst + 7, GsmartQTable[qIndex * 2], 64);
	memcpy (dst + 72, GsmartQTable[qIndex * 2 + 1], 64);

	dst += GSMART_JPG_DEFAULT_HEADER_PART1_LENGTH;

	/* copy Huffman table */
	if (!omit_huffman_table) {
		memcpy (dst, GsmartJPGDefaultHeaderPart2,
			GSMART_JPG_DEFAULT_HEADER_PART2_LENGTH);
		dst += GSMART_JPG_DEFAULT_HEADER_PART2_LENGTH;
	}
	memcpy (dst, GsmartJPGDefaultHeaderPart3,
		GSMART_JPG_DEFAULT_HEADER_PART3_LENGTH);

	/* modify the image width, height */
	*(dst + 8) = w & 0xFF;	//Image width low byte
	*(dst + 7) = w >> 8 & 0xFF;	//Image width high byte
	*(dst + 6) = h & 0xFF;	//Image height low byte
	*(dst + 5) = h >> 8 & 0xFF;	//Image height high byte

	/* set the format */
	*(dst + 11) = format;

	/* point to real JPG compress data start position and copy */
	dst += GSMART_JPG_DEFAULT_HEADER_PART3_LENGTH;

	for (i = 0; i < o_size; i++) {
		value = *(src + i) & 0xFF;
		*(dst) = value;
		dst++;

		if (value == 0xFF && !omit_escape) {
			*(dst) = 0x00;
			dst++;
		}
	}
	/* Add end of image marker */
	*(dst++) = 0xFF;
	*(dst++) = 0xD9;

	*size = dst - start;
}

static inline u_int8_t *
put_dword (u_int8_t * ptr, u_int32_t value)
{
	ptr[0] = (value & 0xff);
	ptr[1] = (value & 0xff00) >> 8;
	ptr[2] = (value & 0xff0000) >> 16;
	ptr[3] = (value & 0xff000000) >> 24;
	return ptr + 4;
}
