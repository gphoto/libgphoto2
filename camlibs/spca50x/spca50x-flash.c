/****************************************************************/
/* spca50x_flash.c - Gphoto2 library for cameras with a sunplus */
/*                   spca50x chip and flash memory              */
/*                                                              */
/*                                                              */
/* Authors: Till Adam <till@adam-lilienthal.de>                 */
/*          Ian MacArthur <ian@imm.uklinux.net>                 */
/*          John Maushammer <gphoto2@maushammer.com>            */
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
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/
#define _BSD_SOURCE

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-log.h>
#include "gphoto2-endian.h"

#define GP_MODULE "spca50x"
#include "spca50x.h"
#include "spca50x-flash.h"

/* forward declarations... local to this file */
static int
spca500_flash_84D_get_file_info (CameraPrivateLibrary * lib, int index,
							int *w, int *h, int *t, int *sz);

/* dsc-350_get_file func. */
static int
spca500_flash_84D_get_file (CameraPrivateLibrary * pl,
		uint8_t ** data, unsigned int *len, int index, int thumbnail);

/*****************************************************************************/
static void
free_files (CameraPrivateLibrary *pl)
{
	int i;
	if (pl->files) {
		for (i = 0; i < pl->num_files_on_flash; i++){
			if(pl->files[i].thumb) free (pl->files[i].thumb);
		}
		free(pl->files);
	}
}

static int
spca50x_flash_wait_for_ready(CameraPrivateLibrary *pl)
{
	/* FIXME tweak this. What is reasonable? 30 s seems a bit long */
	int timeout = 30;
	uint8_t ready = 0;
	while (timeout--) {
		sleep(1);
		if (pl->bridge == BRIDGE_SPCA500) {
			CHECK (gp_port_usb_msg_read (pl->gpdev,
							0x00, 0x0000, 0x0101,
							(char*)&ready, 0x01));
		} else {
			if (pl->fw_rev == 1) {
				CHECK (gp_port_usb_msg_read (pl->gpdev,
							0x0b, 0x0000, 0x0004,
							(char*)&ready, 0x01));
			} else {
				CHECK (gp_port_usb_msg_read (pl->gpdev,
							0x21, 0x0000, 0x0000,
							(char*)&ready, 0x01));
			}
		}
		if (ready)
			return GP_OK;
	}
	return GP_ERROR;
}

static int
spca500_flash_84D_wait_while_busy(CameraPrivateLibrary *pl)
{
	int timeout = 30;
	uint8_t ready = 0;
	while (timeout--) {
		sleep(1);
	CHECK (gp_port_usb_msg_read (pl->gpdev,
					0x00, 0x0000, 0x0100,
					(char*)&ready, 0x01));

		if (ready == 0)
			return GP_OK;
	}
	return GP_ERROR;
}

int
spca50x_flash_get_TOC(CameraPrivateLibrary *pl, int *filecount)
{
	uint16_t n_toc_entries;
	int toc_size = 0;
	if (pl->dirty_flash == 0){
		/* TOC has been read already, and stored in the pl,
		   so let's not read it again unless "dirty" gets set,
		   e.g. by a reset, delete or capture action... */
		*filecount = pl->num_files_on_flash;
		return GP_OK;
	}
	pl->num_files_on_flash = 0;

	if (pl->bridge == BRIDGE_SPCA500) { /* for dsc350 type cams */
				/* command mode */
		CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x00, 0x0080, 0x0100,
					NULL, 0x00));
		/* trigger TOC upload */
		CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x05, 0x0000, 0x000d,
					NULL, 0x00));
		toc_size = 0x100; /* always 256 for the dsc-350 cams */
	} else {

		if (pl->fw_rev == 1) {
			CHECK (gp_port_usb_msg_read (pl->gpdev,
						0x0b, 0x0000, 0x0000,
						(char*)&n_toc_entries, 0x02));
			/* Each file gets two toc entries, one for the image,
			   one for the thumbnail */
			LE16TOH (n_toc_entries);
			*filecount = n_toc_entries/2;
		} else {
			CHECK (gp_port_usb_msg_read (pl->gpdev,
						0x54, 0x0000, 0x0000,
						(char*)&n_toc_entries, 0x02));
			LE16TOH (n_toc_entries);
			*filecount = n_toc_entries;
		}
		/* If empty, return now */
		if (n_toc_entries == 0)
			return GP_OK;

		/* Request the TOC */
		if (pl->fw_rev == 1) {
			CHECK (gp_port_usb_msg_read (pl->gpdev, 0x0a,
						n_toc_entries, 0x000c,
						NULL, 0x00));
		} else {
			CHECK (gp_port_usb_msg_read (pl->gpdev, 0x54,
						n_toc_entries, 0x0001,
						NULL, 0x00));
		}
		/* slurp it in. Supposedly there's 32 bytes in each entry. */
		toc_size = n_toc_entries * 32;

		/* align */
		if (toc_size % 512 != 0)
			toc_size = ((toc_size / 512) + 1) * 512;
	}
	if (pl->flash_toc)
		free (pl->flash_toc);
	pl->flash_toc = malloc(toc_size);
	if (!pl->flash_toc)
		return GP_ERROR_NO_MEMORY;

	CHECK (spca50x_flash_wait_for_ready(pl));

	if (pl->bridge == BRIDGE_SPCA500) { /* For dsc350 type cams */
		/* read the TOC from the cam */
		CHECK (gp_port_read (pl->gpdev, (char *)pl->flash_toc, toc_size));
		/* reset to idle */
		CHECK (gp_port_usb_msg_write(pl->gpdev, 0x00, 0x0000, 0x0100, NULL, 0x0));
		*filecount = (int)pl->flash_toc[10];
		/* Now, create the files info buffer */
		free_files(pl);
		/* NOTE: using calloc to ensure new block is "empty" */
		pl->files = calloc (1, *filecount * sizeof (struct SPCA50xFile));
		if (!pl->files)
			return GP_ERROR_NO_MEMORY;
	} else { /* all other cams with flash... */
		/* read the TOC from the cam */
		CHECK (gp_port_read (pl->gpdev, (char *)pl->flash_toc, toc_size));
	}
	/* record that TOC has been updated - clear the "dirty" flag */
	pl->num_files_on_flash = *filecount;
	pl->dirty_flash = 0;

	return GP_OK;
}


int
spca50x_flash_get_filecount (CameraPrivateLibrary *pl, int *filecount)
{
	uint16_t response = 0;

	if (pl->bridge == BRIDGE_SPCA500) { /* dsc350 cams */
		return spca50x_flash_get_TOC (pl, filecount);
	} else {
		if (pl->fw_rev == 1) {
			CHECK (gp_port_usb_msg_read (pl->gpdev,
						0x0b, 0x0000, 0x0000,
						(char*)&response, 0x02));
			/* Each file gets two toc entries, one for the
			   image, one for the thumbnail */
			LE16TOH (response);
			*filecount = response/2;
		} else {
			CHECK (gp_port_usb_msg_read (pl->gpdev,
						0x54, 0x0000, 0x0000,
						(char*)&response, 0x02));
			LE16TOH (response);
			*filecount = response;
		}
	}
	return GP_OK;
}

int
spca500_flash_delete_file (CameraPrivateLibrary *pl, int index)
{
	if (pl->bridge == BRIDGE_SPCA500) {
		/* command mode */
		CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x00, 0x0080, 0x0100,
					NULL, 0x00));

		/* trigger image delete */
		CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x07, (index + 1), 0x000a,
					NULL, 0x00));

		/* reset to idle */
		CHECK (gp_port_usb_msg_write(pl->gpdev, 0x00, 0x0000, 0x0100, NULL, 0x0));

		/* invalidate TOC/info cache */
		pl->dirty_flash = 1;
		return GP_OK;
	} else {
		/* not supported on the 504 style cams */
		return GP_ERROR_NOT_SUPPORTED;
	}
} /* spca500_flash_delete_file */

int
spca50x_flash_delete_all (CameraPrivateLibrary *pl, GPContext *context)
{
	if (pl->bridge == BRIDGE_SPCA500) { /* dsc350 cams */
		/* command mode */
		CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x00, 0x0080, 0x0100,
					NULL, 0x00));

		/* delete all command */
		CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x07, 0xffff, 0x000a,
					NULL, 0x00));

		/* wait until the camera is not busy any more */
		CHECK (spca500_flash_84D_wait_while_busy(pl));
	} else {
		if (pl->fw_rev == 1) {
			CHECK (gp_port_usb_msg_write (pl->gpdev, 0x1, 0x0, 0x1, NULL, 0x0));
		} else {
			CHECK (gp_port_usb_msg_write (pl->gpdev, 0x52, 0x0, 0x0, NULL, 0x0));
		}
	}

	/* invalidate TOC/info cache */
	pl->dirty_flash = 1;
	return GP_OK;
}

int
spca500_flash_capture (CameraPrivateLibrary *pl)
{
	if (pl->bridge == BRIDGE_SPCA500) {
		/* command mode */
		CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x00, 0x0080, 0x0100,
					NULL, 0x00));

		/* trigger image capture */
		CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x03, 0x0000, 0x0004,
					NULL, 0x00));

		/* wait until the camera is not busy any more */
		CHECK (spca500_flash_84D_wait_while_busy(pl));

		/* invalidate TOC/info cache */
		pl->dirty_flash = 1;
		return GP_OK;
	} else if (pl->bridge == BRIDGE_SPCA504B_PD) {
		/* trigger image capture */
		CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x51, 0x0000, 0x0000,
					NULL, 0));

		/* wait until the camera is not busy any more */
		/* spca50x_flash_wait_for_ready doesn't work here */
		sleep(3);
		
		/* invalidate TOC/info cache */
		pl->dirty_flash = 1;
		return GP_OK;
	} else {
		/* not supported on the 504 style cams, but procedure used by
		 * SPCA504_PD will probably work */
		return GP_ERROR_NOT_SUPPORTED;
	}
}

int
spca50x_flash_get_file_name (CameraPrivateLibrary *pl, int index, char *name)
{
	uint8_t *p;
	if (pl->bridge == BRIDGE_SPCA500) {
		char p[14];
		int w, h, type, size;

		memset (p, 0, sizeof(p));
		/* dsc350 - get the file info, so we can set the file type
	           correctly */
		spca500_flash_84D_get_file_info (pl, index, &w, &h, &type, &size);
		if (type < 3){ /*  for single images */
			snprintf (p, sizeof(p), "Img%03d.jpg", (index + 1));
		}
		else if (type < 6){ /*  for multi-frame images */
			snprintf (p, sizeof(p), "Img%03d-4.jpg", (index + 1));
		}
		else if (type < 8){ /*  for avi */
			snprintf (p, sizeof(p), "Mov%03d.avi", (index + 1));
		}
		else{
			snprintf (p, sizeof(p), "Unknown");
		}
		memcpy (name, p, sizeof(p) );
	} else {
		if (pl->fw_rev == 1) {
			p = pl->flash_toc + index*2*32;
		} else {
			p = pl->flash_toc + index*32;
		}
		memcpy (name, p, 8);
		name[8] = '.';
		memcpy (name+9, p+8, 3);
		name[12] = '\0';
	}
	return GP_OK;
}

/* extract ascii-encoded number from file name */
static int
spca50x_flash_get_number_from_file_name (CameraPrivateLibrary *pl, int index, int *file_number)
{
	char name[14];

	CHECK (spca50x_flash_get_file_name (pl, index, name));
	if(sscanf(&(name[4]), "%d", file_number) != 1)    /* skip "DSC_" */
		return GP_ERROR;
	return GP_OK;
}

static int
spca500_flash_84D_get_file_info (CameraPrivateLibrary * pl, int index,
							int *w, int *h, int *t, int *sz)
{
	char hdr[260];
	char waste[260];
	int i;
	unsigned long size;

	/* First, check if the info. is already buffered in the cam private lib
	 * We DO NOT want to be fetching the entire thumbnail just to get the file size...*/
	 if ((pl->dirty_flash == 0) && (pl->files[index].type != 0)){
		 /* This file must have been checked before */
		*w = pl->files[index].width ;
		*h = pl->files[index].height ;
		*t = pl->files[index].type ;
		*sz = pl->files[index].size ;
		return GP_OK;
	 } else if (pl->dirty_flash != 0){ /* should never happen, but just in case... */
		CHECK(spca50x_flash_get_TOC (pl, &i));
		if ( index >= i){
			/* asking for a picture that doesn't exist */
			return GP_ERROR;
		}
	 }

	/* ...else we must query the cam the hard way... */
	/* command mode */
	CHECK (gp_port_usb_msg_write (pl->gpdev,
				0x00, 0x0080, 0x0100,
				NULL, 0x00));
	/* trigger Thumbnail upload */
	CHECK (gp_port_usb_msg_write (pl->gpdev,
				0x07, (index + 1), 0x0000,
				NULL, 0x00));
	/* NOTE: must use (index + 1) here as the cam indexes images from 1,
	 * whereas libgphoto2 indexes from 0.... */

	/* wait for ready */
	CHECK (spca50x_flash_wait_for_ready(pl));
	/* read the header from the cam */
	CHECK (gp_port_read (pl->gpdev, hdr, 256)); /*  always 256 for the DSC-350+ */

	/* Read the rest of the header... and discard it */
	CHECK (gp_port_read (pl->gpdev, waste, 256));

	/* Now we have to read in the thumbnail data anyway -
	   so we might as well store it for later */
	{
		int j;
		uint8_t * buf;
		if (pl->files[index].thumb){
			free (pl->files[index].thumb); /* discard any previous thumbnail data */
			pl->files[index].thumb = NULL;
		}
		buf = malloc (38 * 256); /* create a new buffer to hold the thumbnail data */
		if (buf){
			j = 0;
			for (i = 0; i < 38; i++) { /* Now read in the image data */
				/* read a buffer from the cam */
				CHECK (gp_port_read (pl->gpdev, (char *)&buf[j], 256));
				j += 256;
			}
			pl->files[index].thumb = buf;
		} else { /* couldn't get a buffer - read in the data anyway and discard it */
			for (i = 0; i < 38; i++) {/* Thumbnails ALWAYS download 38 blocks...*/
				CHECK (gp_port_read (pl->gpdev, waste, 256));
			}
			pl->files[index].thumb = NULL;
		}
	} /* end of thumbnail storing loop */

	/* reset to idle */
	CHECK (gp_port_usb_msg_write(pl->gpdev, 0x00, 0x0000, 0x0100,
			NULL, 0x0));

	/* OK, now we have a hdr we can work with... */
	/* try and get the size */
	size =  ((unsigned long)hdr[15] & 0xff)
	     + (((unsigned long)hdr[16] & 0xff) << 8)
		 + (((unsigned long)hdr[17] & 0xff) << 16);
	*sz = size;

	/* Try and determine the file type */
	/* Should enumerate the possible types properly, somewhere... */
	i = (int)hdr[2]; /* should be the file type */
	*t = i;
	switch (i){
	case 0: /*  this will be 320 x 240 single-frame */
		*w = 320;
		*h = 240;
		break;
	case 1: /*  this will be 640 x 480 single-frame */
		*w = 640;
		*h = 480;
		break;
	case 2: /*  this will be 1024 x 768 single-frame */
/* These are actually 640x480 pics - the win driver just expands them
 * in the host before it stores them... What should we do here? Continue with that
 * pretence, or tell the user the truth? */
		/* *w = 1024; */
		/* *h = 768; */
		*w = 640;
		*h = 480;
		break;
	case 3: /*  this will be 320 x 240 multi-frame */
		*w = 320;
		*h = 240;
		break;
	case 4: /*  this will be 640 x 480 multi-frame */
		*w = 640;
		*h = 480;
		break;
	case 5: /*  this will be 1024 x 768 multi-frame */
/* See notes above! */
		/* *w = 1024; */
		/* *h = 768; */
		*w = 640;
		*h = 480;
		break;
	case 6: /*  ??? TAKE A GUESS!!! 160 x 120 avi ??? MUST verify this sometime.... */
		*w = 160;
		*h = 120;
		break;
	case 7: /*  this is 320 x 240 avi */
		*w = 320;
		*h = 240;
		break;
	default:
		*t = 99; /*  or something equally invalid... */
		*w = 0;
		*h = 0;
		*sz = 0; /*  if we don't know what it is, it can't have a size! */
		break;
	}

	/* now add this new set of data to the info_cache */
	if (pl->dirty_flash == 0){
		/* Only update the files cache if it exists, i.e. if dirty is zero */
		/* We should never get here if dirty is set, however ! */
		pl->files[index].type = *t;
		pl->files[index].width = *w;
		pl->files[index].height = *h;
		pl->files[index].size = *sz;
	}

	return GP_OK;

} /* end of spca500_flash_84D_get_file_info */

static int
spca50x_process_thumbnail (CameraPrivateLibrary *lib,	/*  context */
		uint8_t ** data, unsigned int *len,			/*  return these items */
		uint8_t * buf, uint32_t file_size, int index)	/*  input these items */
{
	uint32_t alloc_size, true_size, w, h, hdrlen;
	uint8_t *tmp, *rgb_p, *yuv_p;
	uint8_t *p2 = lib->flash_toc + index*2*32;

	if (lib->bridge == BRIDGE_SPCA500) { /* for dsc 350 cams */
		w = 80; /* Always seems to be 80 x 60 for the DSC-350 cams */
		h = 60;
	} else {
		/* thumbnails are generated from dc coefficients and are
		 * therefore w/8 x h/8
		 * Since the dimensions of the thumbnail in the TOC are
		 * always [0,0], Use the TOC entry for the full image.
 */
		w = ((p2[0x0c] & 0xff) + (p2[0x0d] & 0xff) * 0x100) / 8;
		h = ((p2[0x0e] & 0xff) + (p2[0x0f] & 0xff) * 0x100) / 8;
	}

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
	hdrlen = snprintf((char*)tmp, alloc_size, "P6 %d %d 255\n", w, h);
	true_size = w * h * 3 + hdrlen;
	if ( true_size > alloc_size ) {
		free (tmp);
		return GP_ERROR;
	}

	yuv_p = buf;
	rgb_p = tmp + hdrlen;
	while (yuv_p < buf + file_size) {
		uint32_t u, v, y, y2;
		uint32_t r, g, b;

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

	return GP_OK;
} /* spca50x_process_thumbnail */

static int
spca50x_flash_process_image (CameraPrivateLibrary *pl,	/*  context */
		uint8_t ** data, unsigned int *len,				/*  return these items */
		uint8_t * buf, uint32_t buf_size, int index)	/*  input these items */
{
	uint8_t *lp_jpg;
	uint8_t qIndex = 0, format;
	int file_size;

	int w = pl->files[index].width;
	int h = pl->files[index].height;

	/* qindex == 2 seems to be right for all the dsc350 images - so far!!! */
	qIndex = 2 ; /*  FIXME - what to do about Qtable stuff? */

	/* decode the image size */
	if (w > 320){
		format = 0x21; /*  for 640 size images */
	} else {
		format = 0x22; /*  for 320 x 240 size images */
	}

	file_size = buf_size + SPCA50X_JPG_DEFAULT_HEADER_LENGTH + 1024 * 10;

	/* now build a jpeg */
	lp_jpg = malloc (file_size);
	if (!lp_jpg)
		return GP_ERROR_NO_MEMORY;

	create_jpeg_from_data (lp_jpg, buf, qIndex, w,
			       h, format, buf_size, &file_size,
			       0, 0);
	free (buf);
	lp_jpg = realloc (lp_jpg, file_size);
	*data = lp_jpg;
	*len = file_size;

	return (GP_OK);
}

static int
spca500_flash_84D_get_file (CameraPrivateLibrary * pl,
		uint8_t ** data, unsigned int *len, int index, int thumbnail)
{
	char tbuf[260];		/*  for the file data blocks */
	int i, j;		/*  general loop vars */
	int blks;		/*  number of 256 byte blocks to fetch */
	int sz;			/*  number of bytes in image */
	uint8_t *buf;		/*  buffer for the read data */
	int type;		/*  type of image reported by header */
	int w, h;		/*  width, height of file being read */
	int true_len;		/*  length determined by actually counting the loaded data! */

	/* Check the info. first, so we KNOW the file type before we start to download it...! */
	/* NOTE: The check should be really easy, as the info ought to be in the files info cache */
	spca500_flash_84D_get_file_info (pl, index, &w, &h, &type, &sz);
	if (type >= 3) { /* for now, only support single frame still images */
		return GP_ERROR_NOT_SUPPORTED;
		/* what to do for multi-frame images? (type 3, 4, 5) or avi's? (6, 7) */
	}

	/* OK, we've decided what type of file it is, now lets upload it... */
	if ((thumbnail != 0) && (pl->files[index].thumb != NULL)){
		/* We can cheat here - we already have the thumbnail data in a cache, so don't load it again */
		blks = 38;
		buf = pl->files[index].thumb;
		pl->files[index].thumb = NULL; /* we will release buf, so this pointer must be "emptied" */
	}
	else {
		/* We need to download this data from the cam... */
		/* set command mode */
		CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x00, 0x0080, 0x0100,
					NULL, 0x00));

		/* trigger image upload */
		if (thumbnail){
			CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x07, (index + 1), 0x0000,  /*  code 0 gets the thumbnail */
					NULL, 0x00));
		} else {
			/* code for different image types...? */
			CHECK (gp_port_usb_msg_write (pl->gpdev,
					0x07, (index + 1), 0x0001, /*  code 1 gets the main image */
					NULL, 0x00));
		}

		/* wait for ready */
		CHECK (spca50x_flash_wait_for_ready(pl));

		/* read the header from the cam */
		CHECK (gp_port_read (pl->gpdev, tbuf, 256)); /*  always 256 for the DSC-350+ */

		/* Read the rest of the header... and discard it */
		CHECK (gp_port_read (pl->gpdev, tbuf, 256));

		if (thumbnail){
			blks = 38 ; /* always 38 blocks in a thumbnail with this setup */
			sz = 0; /* unknown, it's NOT recorded in the header or TOC as such... */
			/* Note the width/height of the thumbnail is fixed at 80 x 60 I believe. */
		}
		else { /* full image... */
			/* find nearest EVEN number of blocks bigger than the size */
			blks = (sz / 256) + 1;
			if ((blks & 1) != 0){ /* block count is odd, not allowed */
				blks += 1;
			}
		}

		/* create a buffer to hold all the read in data */
		buf = malloc (blks * 256);
		if (!buf)
			return GP_ERROR_NO_MEMORY;

		j = 0; /* k = 0; */
		/* Now read in the image data */
		for (i = 0; i < blks; i++) {
			/* read a buffer from the cam */
			CHECK (gp_port_read (pl->gpdev, (char *)&buf[j], 256));
			j += 256;
		}

		/* OK, that's all the data read in, set cam to idle */
		CHECK (gp_port_usb_msg_write(pl->gpdev, 0x00, 0x0000, 0x0100, NULL, 0x0));
	}

	/* Count how many bytes file really is! We use this to discard the padding zeros */
	sz = (blks * 256) - 1;
	/* look for the last non-zero byte in the file...
	 * I hope the file never *really* ends with a zero!*/
	while (buf[sz] == 0) {
		sz -= 1;
	}
	true_len = sz + 1;

	if (thumbnail) { /* post processing of thumbnail data */
		CHECK(spca50x_process_thumbnail (pl, data, len, buf, true_len, index));
	} else { /* post processing of single frame images */
		CHECK(spca50x_flash_process_image (pl, data, len, buf, true_len, index));
	}
	return GP_OK;
} /* end of spca500_flash_84D_get_file */


int
spca50x_flash_get_file_dimensions (CameraPrivateLibrary *pl, int index, int *w, int *h)
{
	uint8_t *p;

	if (pl->bridge == BRIDGE_SPCA500) { /* for dsc 350 cams */
		int type, size;
		return spca500_flash_84D_get_file_info (pl, index, w, h, &type, &size);
	}

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

	if (pl->bridge == BRIDGE_SPCA500) { /* for dsc350 cams */
		int type, w, h;
		return spca500_flash_84D_get_file_info (pl, index, &w, &h, &type, size);
	}

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
	int file_number;
	int align_to, ret;

	if (lib->bridge == BRIDGE_SPCA500) { /* for dsc 350 cams */
		return spca500_flash_84D_get_file (lib, data, len, index, thumbnail);
	}

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

	/* Trigger upload of image data */
	if (thumbnail) {
		CHECK (gp_port_usb_msg_write (lib->gpdev,
					0x0c, index+1, 0x0006, NULL, 0x00));
	} else {
		if (lib->fw_rev == 1) {
			CHECK (gp_port_usb_msg_write (lib->gpdev,
					0x0a, index+1, 0x000d, NULL, 0x00));
		} else {
			if(lib->bridge != BRIDGE_SPCA504B_PD) {
				CHECK (gp_port_usb_msg_write (lib->gpdev,
					0x54, index+1, 0x0002, NULL, 0x00));
			} else {
				CHECK (spca50x_flash_get_number_from_file_name(
					lib, index, &file_number));
				CHECK (gp_port_usb_msg_write (lib->gpdev,
					0x54, file_number, 0x0002, NULL, 0x00));
			}
		}
	}

	if ((lib->fw_rev == 1) || (lib->bridge == BRIDGE_SPCA504B_PD)) {
		align_to = 0x4000;
	} else {
		align_to = 0x2000;
	}
	/* align */
	if (file_size % align_to != 0)
		aligned_size = ((file_size / align_to) + 1) * align_to;

	buf = malloc (aligned_size);
	if (!buf)
		return GP_ERROR_NO_MEMORY;

	ret = spca50x_flash_wait_for_ready(lib);
	if (ret < GP_OK) {
		free (buf);
		return ret;
	}
	ret = gp_port_read (lib->gpdev, (char *)buf, aligned_size);
	if (ret < GP_OK) {
		free (buf);
		return ret;
	}
	/* For images, we are done now, thumbnails need to be converted from
	 * yuv to rgb and a pbm header added. */
	if (thumbnail) {
		ret = spca50x_process_thumbnail (lib, data, len, buf, file_size, index);
		if (ret < GP_OK) {
			free (buf);
			return ret;
		}
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

	if (!pl->dirty_flash && pl->bridge == BRIDGE_SPCA500) {
		/* check if we need to free the file info buffers */
		free_files(pl);
	}
	pl->dirty_flash = 1;
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
	uint8_t bytes[7];
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
						jpReg[i].reg, (char *)bytes, 0x01));
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
					(char *)bytes, 0x01));
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x01, 0x0000, 0x0001,
					(char *)bytes, 0x01));
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x01, 0x0000, 0x0001,
					(char *)bytes, 0x01));

		/* Set to idle? */
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x01, 0x0000, 0x000f,
					NULL, 0x00));

	} else {
		time_t t;
		struct tm *ftm;

		/* firmware detection */
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x20, 0x0000, 0x0000,
					(char *)bytes, 0x01));
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x20, 0x0000, 0x0000,
					(char *)bytes, 0x05));
		CHECK (gp_port_usb_msg_read (pl->gpdev, 0x21, 0x0000, 0x0000,
					(char *)bytes, 0x01));

		/*
		 * The cam is supposed to sync up with the computer time here
		 * somehow, or at least that's what we think. */

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
						i, (char *)bytes+i, 0x01));

	}

	pl->dirty_flash = 1;
	return GP_OK;
}
