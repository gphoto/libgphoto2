/****************************************************************/
/* spca504_flash.c - Gphoto2 library for cameras with a sunplus */
/*                   spca504 chip and flash memory              */
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
#include "spca504_flash.h"

#include <gphoto2.h>
#include <gphoto2-port.h>
#include <gphoto2-port-log.h>

#define GP_MODULE "spca504_flash"

static int yuv2rgb (int y, int u, int v, int *_r, int *_g, int *_b);

static int
spca504_flash_wait_for_ready(GPPort *port)
{
	/* FIXME tweak this. What is reasonable? 30 s seems a bit long */
	int timeout = 30;
	u_int8_t ready = 0;
	while (timeout--) {
		sleep(1);
		CHECK (gp_port_usb_msg_read (port, 0x0b, 0x0000, 0x0004, 
				(char*)&ready, 0x01));
		if (ready) 
			return GP_OK;
	}
	return GP_ERROR;
}

int
spca504_flash_get_TOC(CameraPrivateLibrary *pl, int *filecount)
{
	u_int16_t n_toc_entries;
	int toc_size = 0;
	
	if (!pl->dirty)
		return GP_OK;
	CHECK (gp_port_usb_msg_read (pl->gpdev, 0x0b, 0x0000, 0x0000, 
				(char*)&n_toc_entries, 0x02));
	/* Each file gets two toc entries, one for the image, one for the
	 * thumbnail */
	*filecount = n_toc_entries/2;

	/* If empty, return now */
	if (n_toc_entries == 0)
		return GP_OK;

	/* Request the TOC */
	CHECK (gp_port_usb_msg_read (pl->gpdev, 0x0a, n_toc_entries, 0x000c, 
				NULL, 0x00));

	/* slurp it in. Supposedly there's 32 bytes in each entry. */
	toc_size = n_toc_entries * 32;
	/* align */
	if (toc_size % 512 != 0)
		toc_size = ((toc_size / 512) + 1) * 512;

	if (pl->toc) 
		free (pl->toc);
	pl->toc = malloc(toc_size);
	if (!pl->toc)
		return GP_ERROR_NO_MEMORY;
	CHECK (spca504_flash_wait_for_ready(pl->gpdev));
	CHECK (gp_port_read (pl->gpdev, pl->toc, toc_size));

	pl->dirty = 0;
	return GP_OK;
}


int
spca504_flash_get_filecount (GPPort *port, int *filecount)
{
	u_int16_t response = 0;
	CHECK (gp_port_usb_msg_read (port, 0x0b, 0x0000, 0x0000, 
				(char*)&response, 0x02));
	/* Each file gets two toc entries, one for the image, one for the
	 * thumbnail */
	*filecount = response/2;
	return GP_OK;
}
	
int
spca504_flash_get_file_name (CameraPrivateLibrary *lib, int index, char *name)
{
	u_int8_t *p;

	p = lib->toc + index*2*32;
	memcpy (name, p, 8);
	name[8] = '.';
	memcpy (name+9, p+8, 3);
	name[12] = '\0';
	return GP_OK;
}

int
spca504_flash_get_file_dimensions (CameraPrivateLibrary *lib, int index, int *w, int *h)
{
	u_int8_t *p;

	p = lib->toc + index*2*32;
	*w = (p[0x0c] & 0xff) + (p[0x0d] & 0xff) * 0x100;
	*h = (p[0x0e] & 0xff) + (p[0x0f] & 0xff) * 0x100;
	return GP_OK;
}

int
spca504_flash_get_file_size (CameraPrivateLibrary *lib, int index, int *size)
{	
	u_int8_t *p;

	p = lib->toc + index*2*32;
	*size =   (p[0x1c] & 0xff) 
		+ (p[0x1d] & 0xff) * 0x100
		+ (p[0x1e] & 0xff) * 0x10000
		+ (p[0x1f] & 0xff) * 0x1000000;
	
	return GP_OK;
}

int
spca504_flash_get_file (CameraPrivateLibrary *lib, GPContext *context, 
		u_int8_t ** data, unsigned int *len, int index, int thumbnail)
{
	u_int32_t file_size = 0, aligned_size = 0;
	u_int32_t addr = 0;
	u_int8_t *p, *buf;
	u_int8_t *tmp, *rgb_p, *yuv_p;
	unsigned char pbm_header[15];

	if (thumbnail) {
		p = lib->toc + (index*2+1) * 32;
	} else {
		p = lib->toc + index*2*32;
	}

	aligned_size = file_size = 
		  (p[0x1c] & 0xff) 
		+ (p[0x1d] & 0xff) * 0x100 
		+ (p[0x1e] & 0xff) * 0x10000;
	
	addr =    (p[0x18] & 0xff) 
		+ (p[0x19] & 0xff) * 0x100 
		+ (p[0x1a] & 0xff) * 0x10000
		+ (p[0x1b] & 0xff) * 0x1000000;

	/* Trigger download of image data */
	if (thumbnail) {
		CHECK (gp_port_usb_msg_write (lib->gpdev, 
					0x0c, index+1, 0x0006, NULL, 0x00));
	} else {
		CHECK (gp_port_usb_msg_write (lib->gpdev, 
					0x0a, index+1, 0x000d, NULL, 0x00));
	}
	/* align */
	if (file_size % 16384 != 0)
		aligned_size = ((file_size / 16384) + 1) * 16384;

	buf = malloc (aligned_size); 
	if (!buf)
		return GP_ERROR_NO_MEMORY;

	CHECK (spca504_flash_wait_for_ready(lib->gpdev));
	CHECK (gp_port_read (lib->gpdev, buf, aligned_size));
	/* For images, we are done now, thumbnails need to be converted from
	 * yuv to rgb and a pbm header added. */
	if (thumbnail) {
		int size,w,h;
		u_int8_t *p2 = lib->toc + index*2*32;

		/* thumbnails are generated from dc coefficients and are
		 * therefor w/8 x h/8
		 * Since the dimensions of the thumbnail in the TOC are
		 * always [0,0], Use the TOC entry for the full image.
		 */
		w = ((p2[0x0c] & 0xff) + (p2[0x0d] & 0xff) * 0x100) / 8; 
		h = ((p2[0x0e] & 0xff) + (p2[0x0f] & 0xff) * 0x100) / 8;
		
		snprintf (pbm_header, sizeof (pbm_header), "P6 %d %d 255\n", 
				w, h );
		size = w * h * 3 + sizeof(pbm_header);
		tmp = malloc (size);
		if (!tmp)
			return GP_ERROR_NO_MEMORY;

		snprintf ( tmp, sizeof(pbm_header), pbm_header);	
		yuv_p = buf;
		rgb_p = tmp + sizeof (pbm_header);
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
		*len = size;
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
spca504_flash_close (GPPort *port, GPContext *context)
{
	CHECK (gp_port_usb_msg_write (port, 0x00, 0x01, 0x2306, NULL, 0x00));
	CHECK (gp_port_usb_msg_write (port, 0x00, 0x00, 0x0d04, NULL, 0x00));
	return GP_OK;
}

/*
 * Initialize the camera and write the jpeg registers. Read them back in to
 * check whether the values are set correctly (wtf?)
 */
int
spca504_flash_init (GPPort *port, GPContext *context)
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
	u_int8_t bytes[4];
	int i;
	
   CHECK (gp_port_usb_msg_write (port, 0x00, 0x0000, 0x2000, NULL, 0x00));
   CHECK (gp_port_usb_msg_write (port, 0x00, 0x0013, 0x2301, NULL, 0x00));
   CHECK (gp_port_usb_msg_write (port, 0x00, 0x0001, 0x2883, NULL, 0x00));

   for (i=0 ; i<len ; i++) {
	   CHECK (gp_port_usb_msg_write (port, 0x00, jpReg[i].val, 
				   jpReg[i].reg, NULL, 0x00));
	   CHECK (gp_port_usb_msg_read (port, 0x00, 0x0000, 
				   jpReg[i].reg, bytes, 0x01));	   
   }
   CHECK (gp_port_usb_msg_write (port, 0x00, 0x0001, 0x2501, NULL, 0x00));
   CHECK (gp_port_usb_msg_write (port, 0x00, 0x0000, 0x2306, NULL, 0x00));
   CHECK (gp_port_usb_msg_write (port, 0x08, 0x0000, 0x0006, NULL, 0x00));

   /* Read the same thing thrice, for whatever reason. 
    * From working on getting iso traffic working with this chip, I've found
    * out, that 0x01, 0x0000, 0x0001 polls the chip for completion of a
    * command. Unfortunately it is of yet unclear, what the exact procedure
    * is. Until we know, let's keep this here. */
   CHECK (gp_port_usb_msg_read (port, 0x01, 0x0000, 0x0001, bytes, 0x01));
   CHECK (gp_port_usb_msg_read (port, 0x01, 0x0000, 0x0001, bytes, 0x01));
   CHECK (gp_port_usb_msg_read (port, 0x01, 0x0000, 0x0001, bytes, 0x01));

   /* Set to idle? */
   CHECK (gp_port_usb_msg_read (port, 0x01, 0x0000, 0x000f, NULL, 0x00));
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
