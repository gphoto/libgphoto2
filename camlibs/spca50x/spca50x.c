/****************************************************************/
/* spca50x.c - Gphoto2 library for cameras with sunplus spca50x */
/*             chips                                            */
/*                                                              */
/* Copyright © 2002, 2003 Till Adam                             */
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

#include "spca50x.h"
#include "spca50x-registers.h"
#include "spca50x-jpeg-header.h"

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

int
spca50x_get_firmware_revision (CameraPrivateLibrary *lib)
{

	CHECK (gp_port_usb_msg_read (lib->gpdev, 0x20, 0x0, 0x0,
				&(lib->fw_rev), 1));
	return GP_OK;
}

int
spca50x_detect_storage_type (CameraPrivateLibrary *lib)
{
	int i;
	uint8_t buf[3];

	for (i=0;i<3;i++)
	{
		CHECK (gp_port_usb_msg_read (lib->gpdev, 0x28, 0x0000,
					i, &buf[i], 0x01));
	}

	if (buf[0]) lib->storage_media_mask |= SPCA50X_SDRAM;
	if (buf[1]) lib->storage_media_mask |= SPCA50X_FLASH;
	if (buf[2]) lib->storage_media_mask |= SPCA50X_CARD;
	GP_DEBUG("SPCA50x: has_sdram: 0x%x has_flash 0x%x has_card: 0x%x\n"
			, buf[0], buf[1], buf[2]);

	return GP_OK;
}

int
spca50x_reset (CameraPrivateLibrary * lib)
{
	GP_DEBUG ("* spca50x_reset");
	if (lib->bridge == BRIDGE_SPCA500) {
		if (lib->storage_media_mask & SPCA50X_SDRAM) {
			/* This is not reset but "Change Mode to Idle
			 * (Clear Buffer)" Cant hurt, I guess. */
			CHECK (gp_port_usb_msg_write
				(lib->gpdev, 0x02, 0x0000, 0x07, NULL, 0));
		}
	} else if (lib->fw_rev == 1) {
		CHECK (gp_port_usb_msg_write
		       (lib->gpdev, 0x02, 0x0000, 0x0003, NULL, 0));
	} else if (lib->fw_rev == 2) {
		CHECK (gp_port_usb_msg_write(lib->gpdev, 0, 1,
					SPCA50X_REG_AutoPbSize, NULL, 0));
		CHECK (gp_port_usb_msg_write(lib->gpdev, 0, 0,
					0x0d04, NULL, 0));
		CHECK (gp_port_usb_msg_write(lib->gpdev, 0x1e, 0, 0, NULL, 0));
	}
	usleep(200000);
	return GP_OK;
}


int
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

int
spca50x_capture (CameraPrivateLibrary * lib)
{
	sleep (2);
	CHECK (gp_port_usb_msg_write
	       (lib->gpdev, 0x06, 0x0000, 0x0003, NULL, 0));
	sleep (3);
	return GP_OK;
}

void
create_jpeg_from_data (uint8_t * dst, uint8_t * src, int qIndex, int w,
		       int h, uint8_t format, int o_size, int *size,
		       int omit_huffman_table, int omit_escape)
{
	int i = 0;
	uint8_t *start;
	uint8_t value;

	start = dst;
	/* copy the header from the template */
	memcpy (dst, SPCA50xJPGDefaultHeaderPart1,
			SPCA50X_JPG_DEFAULT_HEADER_PART1_LENGTH);

	/* modify quantization table */
	memcpy (dst + 7, SPCA50xQTable[qIndex * 2], 64);
	memcpy (dst + 72, SPCA50xQTable[qIndex * 2 + 1], 64);

	dst += SPCA50X_JPG_DEFAULT_HEADER_PART1_LENGTH;

	/* copy Huffman table */
	if (!omit_huffman_table) {
	    memcpy (dst, SPCA50xJPGDefaultHeaderPart2,
			    SPCA50X_JPG_DEFAULT_HEADER_PART2_LENGTH);
	    dst += SPCA50X_JPG_DEFAULT_HEADER_PART2_LENGTH;
	}
	memcpy (dst, SPCA50xJPGDefaultHeaderPart3,
			SPCA50X_JPG_DEFAULT_HEADER_PART3_LENGTH);

	/* modify the image width, height */
	*(dst + 8) = w & 0xFF;		/* Image width low byte */
	*(dst + 7) = w >> 8 & 0xFF;	/* Image width high byte */
	*(dst + 6) = h & 0xFF;		/* Image height low byte */
	*(dst + 5) = h >> 8 & 0xFF;	/* Image height high byte */

	/* set the format */
	*(dst + 11) = format;

	/* point to real JPG compress data start position and copy */
	dst += SPCA50X_JPG_DEFAULT_HEADER_PART3_LENGTH;

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
