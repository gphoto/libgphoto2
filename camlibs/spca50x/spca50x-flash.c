/****************************************************************/
/* spca50x_flash.c - Gphoto2 library for cameras with a sunplus */
/*                   spca50x chip and flash memory              */
/*                                                              */
/*                                                              */
/* Authors: Till Adam <till@adam-lilienthal.de>                 */
/*                                                              */
/* based on work done by:                                       */
/*          Mark A. Zimmerman <mark@foresthaven.com>            */
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gphoto2.h>
#include <gphoto2-port.h>
#include <gphoto2-port-log.h>
#include <gphoto2-endian.h>

#define GP_MODULE "spca50x"
#include "spca50x.h"
#include "spca50x-flash.h"


static int
spca50x_flash_wait_for_ready(CameraPrivateLibrary *pl)
{
	/* FIXME tweak this. What is reasonable? 30 s seems a bit long */
	int timeout = 30;
	uint8_t ready = 0;
	while (timeout--) {
		sleep(1);
		if (pl->fw_rev == 1) {
			CHECK (gp_port_usb_msg_read (pl->gpdev, 0x0b, 0x0000, 0x0004, 
				(char*)&ready, 0x01));
		} else {
			CHECK (gp_port_usb_msg_read (pl->gpdev, 0x21, 0x0000, 0x0000, 
				(char*)&ready, 0x01));
		}
		if (ready) 
			return GP_OK;
	}
	return GP_ERROR;
}

int
spca50x_flash_get_TOC(CameraPrivateLibrary *pl, int *filecount)
{
	uint16_t n_toc_entries;
	int toc_size = 0;
		
	if (pl->fw_rev == 1) {
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x0b, 0x0000, 0x0000, 
					(char*)&n_toc_entries, 0x02));
		/* Each file gets two toc entries, one for the image, one for the
		 * thumbnail */
		LE16TOH (n_toc_entries);
		*filecount = n_toc_entries/2;
	} else {
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x54, 0x0000, 0x0000, 
					(char*)&n_toc_entries, 0x02));
		LE16TOH (n_toc_entries);
		*filecount = n_toc_entries;
	}
	/* If empty, return now */
	if (n_toc_entries == 0)
		return GP_OK;

	/* Request the TOC */

	if (pl->fw_rev == 1) {
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x0a, n_toc_entries, 0x000c, 
					NULL, 0x00));
	} else {
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x54, n_toc_entries, 0x0001, 
					NULL, 0x00));
	}
	/* slurp it in. Supposedly there's 32 bytes in each entry. */
	toc_size = n_toc_entries * 32;
	/* align */
	if (toc_size % 512 != 0)
		toc_size = ((toc_size / 512) + 1) * 512;

	if (pl->flash_toc) 
		free (pl->flash_toc);
	pl->flash_toc = malloc(toc_size);
	if (!pl->flash_toc)
		return GP_ERROR_NO_MEMORY;
	CHECK (spca50x_flash_wait_for_ready(pl));
	CHECK (gp_port_read (pl->gpdev, pl->flash_toc, toc_size));

	return GP_OK;
}


int
spca50x_flash_get_filecount (CameraPrivateLibrary *pl, int *filecount)
{
	uint16_t response = 0;
	
	if (pl->fw_rev == 1) {
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x0b, 0x0000, 0x0000, 
					(char*)&response, 0x02));
		/* Each file gets two toc entries, one for the image, one for the
		 * thumbnail */
		LE16TOH (response);
		*filecount = response/2;
	} else {
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x54, 0x0000, 0x0000, 
					(char*)&response, 0x02));
		LE16TOH (response);
		*filecount = response;
	}
	return GP_OK;
}

int
spca50x_flash_delete_all (CameraPrivateLibrary *pl, GPContext *context)
{
	if (pl->fw_rev == 1) {
		CHECK (gp_port_usb_msg_write (pl->gpdev, 0x1, 0x0, 0x1, NULL, 0x0));
	} else {
		CHECK (gp_port_usb_msg_write (pl->gpdev, 0x52, 0x0, 0x0, NULL, 0x0));
	}
	return GP_OK;
}


int
spca50x_flash_get_file_name (CameraPrivateLibrary *pl, int index, char *name)
{
	uint8_t *p;

	if (pl->fw_rev == 1) {
		p = pl->flash_toc + index*2*32;
	} else {
		p = pl->flash_toc + index*32;
	}
	memcpy (name, p, 8);
	name[8] = '.';
	memcpy (name+9, p+8, 3);
	name[12] = '\0';
	return GP_OK;
}

int
spca50x_flash_get_file_dimensions (CameraPrivateLibrary *pl, int index, int *w, int *h)
{
	uint8_t *p;

	if (pl->fw_rev == 1) {
		p = pl->flash_toc + index*2*32;
	} else {
		p = pl->flash_toc + index*32;
	}

	*w = (p[0x0c] & 0xff) + (p[0x0d] & 0xff) * 0x100;
	*h = (p[0x0e] & 0xff) + (p[0x0f] & 0xff) * 0x100;
	return GP_OK;
}

int
spca50x_flash_get_file_size (CameraPrivateLibrary *pl, int index, int *size)
{	
	uint8_t *p;

	if (pl->fw_rev == 1) {
		p = pl->flash_toc + index*2*32;
	} else {
		p = pl->flash_toc + index*32;
	}

	*size =   (p[0x1c] & 0xff) 
		+ (p[0x1d] & 0xff) * 0x100
		+ (p[0x1e] & 0xff) * 0x10000
		+ (p[0x1f] & 0xff) * 0x1000000;
	
	return GP_OK;
}

int
spca50x_flash_get_file (CameraPrivateLibrary *lib, GPContext *context, 
		uint8_t ** data, unsigned int *len, int index, int thumbnail)
{
	uint32_t file_size = 0, aligned_size = 0;
	uint8_t *p, *buf;
	uint8_t *tmp, *rgb_p, *yuv_p;

	if (lib->fw_rev != 1 && thumbnail)
		return GP_ERROR_NOT_SUPPORTED;
	
	if (thumbnail) {
		p = lib->flash_toc + (index*2+1) * 32;
	} else {
		if (lib->fw_rev == 1) {
			p = lib->flash_toc + index*2*32;
		} else {
			p = lib->flash_toc + index*32;
		}
	}

	aligned_size = file_size = 
		  (p[0x1c] & 0xff) 
		+ (p[0x1d] & 0xff) * 0x100 
		+ (p[0x1e] & 0xff) * 0x10000;

	/* Trigger download of image data */
	if (thumbnail) {
		CHECK (gp_port_usb_msg_write (lib->gpdev, 
					0x0c, index+1, 0x0006, NULL, 0x00));
	} else {
		if (lib->fw_rev == 1) {
			CHECK (gp_port_usb_msg_write (lib->gpdev, 
					0x0a, index+1, 0x000d, NULL, 0x00));
		} else {
			CHECK (gp_port_usb_msg_write (lib->gpdev, 
					0x54, index+1, 0x0002, NULL, 0x00));
		}
	}
	/* align */
	if (file_size % 8192 != 0)
		aligned_size = ((file_size / 8192) + 1) * 8192;

	buf = malloc (aligned_size); 
	if (!buf)
		return GP_ERROR_NO_MEMORY;

	CHECK (spca50x_flash_wait_for_ready(lib));
	CHECK (gp_port_read (lib->gpdev, buf, aligned_size));
	/* For images, we are done now, thumbnails need to be converted from
	 * yuv to rgb and a pbm header added. */
	if (thumbnail) {
		int alloc_size, true_size, w, h, hdrlen;
		uint8_t *p2 = lib->flash_toc + index*2*32;

		/* thumbnails are generated from dc coefficients and are
		 * therefor w/8 x h/8
		 * Since the dimensions of the thumbnail in the TOC are
		 * always [0,0], Use the TOC entry for the full image.
		 */
		w = ((p2[0x0c] & 0xff) + (p2[0x0d] & 0xff) * 0x100) / 8; 
		h = ((p2[0x0e] & 0xff) + (p2[0x0f] & 0xff) * 0x100) / 8;

		/* Allow for a long header; get the true length later.
		 */
		hdrlen = 15;
		alloc_size = w * h * 3 + hdrlen;
		tmp = malloc (alloc_size);
		if (!tmp)
			return GP_ERROR_NO_MEMORY;

		/* Write the header and get its length. Then, verify that
		 * we allocated enough memory.
		 * This should never fail; it would be nice to have an error
		 * code like GP_ERROR_CAMLIB_INTERNAL for cases like this.
		 */
		hdrlen = snprintf(tmp, alloc_size, "P6 %d %d 255\n", w, h);	
		true_size = w * h * 3 + hdrlen;
		if ( true_size > alloc_size )
			return GP_ERROR;

		yuv_p = buf;
		rgb_p = tmp + hdrlen;
		while (yuv_p < buf + file_size) {
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
		free (buf);
		*data = tmp;
		*len = true_size;
	} else {
		*data = buf;
		*len = file_size;
	}
	return GP_OK;
}

/*
 * Deinit camera
 */
int
spca50x_flash_close (CameraPrivateLibrary *pl, GPContext *context)
{
	if (pl->fw_rev == 1) {
		CHECK (gp_port_usb_msg_write (pl->gpdev, 0x00, 0x01, 0x2306, 
					NULL, 0x00));
		CHECK (gp_port_usb_msg_write (pl->gpdev, 0x00, 0x00, 0x0d04, 
					NULL, 0x00));
	} else {
		/* Anything we need to do? */

	}
	return GP_OK;
}

/*
 * Initialize the camera and write the jpeg registers. Read them back in to
 * check whether the values are set correctly (wtf?)
 */
int
spca50x_flash_init (CameraPrivateLibrary *pl, GPContext *context)
{
	struct JPREG {
		int reg;
		int val;
	};
	struct JPREG jpReg[] = {
		{ 0x2800, 0x0005 },       { 0x2840, 0x0005 },
		{ 0x2801, 0x0003 },       { 0x2841, 0x0005 },
		{ 0x2802, 0x0003 },       { 0x2842, 0x0007 },
		{ 0x2803, 0x0005 },       { 0x2843, 0x000e },
		{ 0x2804, 0x0007 },       { 0x2844, 0x001e },
		{ 0x2805, 0x000c },       { 0x2845, 0x001e },
		{ 0x2806, 0x000f },       { 0x2846, 0x001e },
		{ 0x2807, 0x0012 },       { 0x2847, 0x001e },
		{ 0x2808, 0x0004 },       { 0x2848, 0x0005 },
		{ 0x2809, 0x0004 },       { 0x2849, 0x0006 },
		{ 0x280a, 0x0004 },       { 0x284a, 0x0008 },
		{ 0x280b, 0x0006 },       { 0x284b, 0x0014 },
		{ 0x280c, 0x0008 },       { 0x284c, 0x001e },
		{ 0x280d, 0x0011 },       { 0x284d, 0x001e },
		{ 0x280e, 0x0012 },       { 0x284e, 0x001e },
		{ 0x280f, 0x0011 },       { 0x284f, 0x001e },
		{ 0x2810, 0x0004 },       { 0x2850, 0x0007 },
		{ 0x2811, 0x0004 },       { 0x2851, 0x0008 },
		{ 0x2812, 0x0005 },       { 0x2852, 0x0011 },
		{ 0x2813, 0x0007 },       { 0x2853, 0x001e },
		{ 0x2814, 0x000c },       { 0x2854, 0x001e },
		{ 0x2815, 0x0011 },       { 0x2855, 0x001e },
		{ 0x2816, 0x0015 },       { 0x2856, 0x001e },
		{ 0x2817, 0x0011 },       { 0x2857, 0x001e },
		{ 0x2818, 0x0004 },       { 0x2858, 0x000e },
		{ 0x2819, 0x0005 },       { 0x2859, 0x0014 },
		{ 0x281a, 0x0007 },       { 0x285a, 0x001e },
		{ 0x281b, 0x0009 },       { 0x285b, 0x001e },
		{ 0x281c, 0x000f },       { 0x285c, 0x001e },
		{ 0x281d, 0x001a },       { 0x285d, 0x001e },
		{ 0x281e, 0x0018 },       { 0x285e, 0x001e },
		{ 0x281f, 0x0013 },       { 0x285f, 0x001e },
		{ 0x2820, 0x0005 },       { 0x2860, 0x001e },
		{ 0x2821, 0x0007 },       { 0x2861, 0x001e },
		{ 0x2822, 0x000b },       { 0x2862, 0x001e },
		{ 0x2823, 0x0011 },       { 0x2863, 0x001e },
		{ 0x2824, 0x0014 },       { 0x2864, 0x001e },
		{ 0x2825, 0x0021 },       { 0x2865, 0x001e },
		{ 0x2826, 0x001f },       { 0x2866, 0x001e },
		{ 0x2827, 0x0017 },       { 0x2867, 0x001e },
		{ 0x2828, 0x0007 },       { 0x2868, 0x001e },
		{ 0x2829, 0x000b },       { 0x2869, 0x001e },
		{ 0x282a, 0x0011 },       { 0x286a, 0x001e },
		{ 0x282b, 0x0013 },       { 0x286b, 0x001e },
		{ 0x282c, 0x0018 },       { 0x286c, 0x001e },
		{ 0x282d, 0x001f },       { 0x286d, 0x001e },
		{ 0x282e, 0x0022 },       { 0x286e, 0x001e },
		{ 0x282f, 0x001c },       { 0x286f, 0x001e },
		{ 0x2830, 0x000f },       { 0x2870, 0x001e },
		{ 0x2831, 0x0013 },       { 0x2871, 0x001e },
		{ 0x2832, 0x0017 },       { 0x2872, 0x001e },
		{ 0x2833, 0x001a },       { 0x2873, 0x001e },
		{ 0x2834, 0x001f },       { 0x2874, 0x001e },
		{ 0x2835, 0x0024 },       { 0x2875, 0x001e },
		{ 0x2836, 0x0024 },       { 0x2876, 0x001e },
		{ 0x2837, 0x001e },       { 0x2877, 0x001e },
		{ 0x2838, 0x0016 },       { 0x2878, 0x001e },
		{ 0x2839, 0x001c },       { 0x2879, 0x001e },
		{ 0x283a, 0x001d },       { 0x287a, 0x001e },
		{ 0x283b, 0x001d },       { 0x287b, 0x001e },
		{ 0x283c, 0x0022 },       { 0x287c, 0x001e },
		{ 0x283d, 0x001e },       { 0x287d, 0x001e },
		{ 0x283e, 0x001f },       { 0x287e, 0x001e },
		{ 0x283f, 0x001e },       { 0x287f, 0x001e }
	};

	int len = sizeof (jpReg) / sizeof (jpReg[0]);
	uint8_t bytes[4];
	int i;

	if (pl->fw_rev == 1) {
		CHECK (gp_port_usb_msg_write (pl->gpdev, 0x00, 0x0000, 0x2000, 
					NULL, 0x00));
		CHECK (gp_port_usb_msg_write (pl->gpdev, 0x00, 0x0013, 0x2301, 
					NULL, 0x00));
		CHECK (gp_port_usb_msg_write (pl->gpdev, 0x00, 0x0001, 0x2883, 
					NULL, 0x00));

		for (i=0 ; i<len ; i++) {
			CHECK (gp_port_usb_msg_write (pl->gpdev, 0x00, 
						jpReg[i].val, jpReg[i].reg, 
						NULL, 0x00));
			CHECK (gp_port_usb_msg_read (pl->gpdev, 0x00, 0x0000, 
						jpReg[i].reg, bytes, 0x01));	   
		}
		CHECK (gp_port_usb_msg_write (pl->gpdev, 0x00, 0x0001, 0x2501, 
					NULL, 0x00));
		CHECK (gp_port_usb_msg_write (pl->gpdev, 0x00, 0x0000, 0x2306, 
					NULL, 0x00));
		CHECK (gp_port_usb_msg_write (pl->gpdev, 0x08, 0x0000, 0x0006, 
					NULL, 0x00));

		/* Read the same thing thrice, for whatever reason. 
		 * From working on getting iso traffic working with this chip, I've found
		 * out, that 0x01, 0x0000, 0x0001 polls the chip for completion of a
		 * command. Unfortunately it is of yet unclear, what the exact procedure
		 * is. Until we know, let's keep this here. */
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x01, 0x0000, 0x0001, 
					bytes, 0x01));
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x01, 0x0000, 0x0001, 
					bytes, 0x01));
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x01, 0x0000, 0x0001, 
					bytes, 0x01));

		/* Set to idle? */
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x01, 0x0000, 0x000f, 
					NULL, 0x00));

	} else {

		uint8_t bytes[7];
		time_t t;
		struct tm *ftm;
		int i;

		/* firmware detection */
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x20, 0x0000, 0x0000, 
					bytes, 0x01));
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x20, 0x0000, 0x0000, 
					bytes, 0x05));
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x21, 0x0000, 0x0000,
					bytes, 0x01));

		/* 
		 * The cam is supposed to sync up with the computer time here
		 * somehow, or at least that's what we think. 
		 */

		time(&t);
		ftm = localtime(&t);

		bytes[0] = ftm->tm_sec;
		bytes[1] = ftm->tm_min;
		bytes[2] = ftm->tm_hour;
		bytes[3] = 0;               /* what is that? either 0x0 or 0x6 */
		bytes[4] = ftm->tm_mday;
		bytes[5] = ftm->tm_mon+1;
		bytes[6] = ftm->tm_year-100; /* stupido. Year is 1900 + tm_year. 
						We need two digits only. */


		GP_DEBUG("Timestamp: %4d-%02d-%02d %2d:%02d:%02d",
				ftm->tm_year+1900,ftm->tm_mon+1,ftm->tm_mday,
				ftm->tm_hour,ftm->tm_min,ftm->tm_sec);

		for (i=0;i<7;i++)
			CHECK (gp_port_usb_msg_write (pl->gpdev, 0x29, 0x0000, 
						i, bytes+i, 0x01));

	}
	return GP_OK;
}
