/****************************************************************/
/* benq.c - Gphoto2 library for Benq DC1300 cameras and friends */
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "benq.h"

#include <gphoto2.h>
#include <gphoto2-port.h>
#include <gphoto2-port-log.h>

#define GP_MODULE "benq"

static int
benq_wait_for_ready(GPPort *port)
{
	int timeout = 30;
	uint8_t ready = 0;
	while (timeout--) {
		sleep(1);
		CHECK (gp_port_usb_msg_read (port, 0x21, 0x0000, 0x0000, 
				(char*)&ready, 0x01));
		if (ready) 
			return GP_OK;
	}
	return GP_ERROR;
}

int
benq_get_TOC(CameraPrivateLibrary *pl, int *filecount)
{
	uint16_t n_toc_entries;
	int toc_size = 0;
	
	CHECK (gp_port_usb_msg_read (pl->gpdev, 0x54, 0x0000, 0x0000, 
				(char*)&n_toc_entries, 0x02));
	*filecount = n_toc_entries;

	if (n_toc_entries == 0)
		return GP_OK;

	/* Request the TOC */
	CHECK (gp_port_usb_msg_read (pl->gpdev, 0x54, n_toc_entries, 0x0001, 
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
	CHECK (benq_wait_for_ready(pl->gpdev));
	CHECK (gp_port_read (pl->gpdev, pl->toc, toc_size));

	return GP_OK;
}

int
benq_get_filecount (GPPort *port, int *filecount)
{
	uint16_t response = 0;
	CHECK (gp_port_usb_msg_read (port, 0x54, 0x0000, 0x0000, 
				(char*)&response, 0x02));
	*filecount = response;
	return GP_OK;
}

int
benq_delete_all (GPPort *port)
{
	CHECK (gp_port_usb_msg_write (port, 0x52, 0x0, 0x0, NULL, 0x0));
	return GP_OK;
}

int
benq_get_file_name (CameraPrivateLibrary *lib, int index, char *name)
{
	uint8_t *p;

	p = lib->toc + index*32;
	memcpy (name, p, 8);
	name[8] = '.';
	memcpy (name+9, p+8, 3);
	name[12] = '\0';
	return GP_OK;
}

int
benq_get_file_size (CameraPrivateLibrary *lib, int index, int *size)
{	
	uint8_t *p;

	p = lib->toc + index*32;
	*size =   (p[0x1c] & 0xff) 
		+ (p[0x1d] & 0xff) * 0x100
		+ (p[0x1e] & 0xff) * 0x10000
		+ (p[0x1f] & 0xff) * 0x1000000;
	
	return GP_OK;
}

int
benq_get_file (CameraPrivateLibrary *lib, GPContext *context, 
		uint8_t ** data, unsigned int *len, int index, int thumbnail)
{
	uint32_t file_size = 0, aligned_size = 0;
	uint32_t addr = 0;
	uint8_t *p, *buf;

	p = lib->toc + index*32;

	aligned_size = file_size = 
		  (p[0x1c] & 0xff) 
		+ (p[0x1d] & 0xff) * 0x100 
		+ (p[0x1e] & 0xff) * 0x10000;
	
	addr =    (p[0x18] & 0xff) 
		+ (p[0x19] & 0xff) * 0x100 
		+ (p[0x1a] & 0xff) * 0x10000
		+ (p[0x1b] & 0xff) * 0x1000000;

	/* Trigger download of image data */
	CHECK (gp_port_usb_msg_write (lib->gpdev, 
					0x54, index+1, 0x0002, NULL, 0x00));
	/* align */
	if (file_size % 16384 != 0)
		aligned_size = ((file_size / 16384) + 1) * 16384;

	buf = malloc (aligned_size); 
	if (!buf)
		return GP_ERROR_NO_MEMORY;

	CHECK (benq_wait_for_ready(lib->gpdev));
	CHECK (gp_port_read (lib->gpdev, buf, aligned_size));
	
	*data = buf;
	*len = file_size;
	return GP_OK;
}

int
benq_init (GPPort *port, GPContext *context)
{
	uint8_t bytes[7];
	time_t t;
	struct tm *ftm;
	int i;

	/* firmware detection */
	CHECK (gp_port_usb_msg_read (port, 0x20, 0x0000, 0x0000, bytes, 0x01));
	CHECK (gp_port_usb_msg_read (port, 0x20, 0x0000, 0x0000, bytes, 0x05));

	CHECK (gp_port_usb_msg_read (port, 0x21, 0x0000, 0x0000, bytes, 0x01));

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
		CHECK (gp_port_usb_msg_write (port, 0x29, 0x0000, i, 
					bytes+i, 0x01));

	return GP_OK;
}
