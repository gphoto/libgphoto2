/* aox.c
 *
 * Copyright (C) 2003 Theodore Kilgore <kilgota@auburn.edu>
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
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#include "aox.h"

#define GP_MODULE "aox"

#define WRITE gp_port_usb_msg_write
#define READ  gp_port_usb_msg_read


int aox_init (GPPort *port, Model *model, Info *info)
{
	char c[16];
	unsigned char hi[2];
	unsigned char lo[2];
	memset(c,0,sizeof(c));
	memset (hi,0,2);
	memset (lo,0,2);

	GP_DEBUG("Running aox_init\n");

	READ(port, 1, 0, 0, c,0x10);
	WRITE(port, 8, 1, 0, c, 0x10);
        READ(port, 0xff, 0x07, 0xfffc, c, 4); /* Returns version number, apparently */
        READ(port, 0x06, 0x0, 0x0, c, 2);
        READ(port, 0x04, 0x1, 0x1, (char *)lo, 2);
	GP_DEBUG("%02x %02x number of lo-res pics\n", lo[0],lo[1]);
	/* We need to keep this information. We presume that
	 * more than 255 pictures is impossible with this camera.
	 */
	memcpy (info, &lo[0], 1);
        READ(port, 0x04, 0x2, 0x1, c, 2);
        READ(port, 0x04, 0x3, 0x1, c, 2);
        READ(port, 0x04, 0x4, 0x1, c, 2);
        READ(port, 0x04, 0x5, 0x1, (char *)hi, 2);
	GP_DEBUG("%02i %02i number of hi-res pics\n", hi[0], hi[1]);
	/* This information, too. */
	memcpy (info +1, &hi[0], 1);
        READ(port, 0x04, 0x6, 0x1, c, 2);	/* "Completion" flag.*/
	GP_DEBUG("info[0] = 0x%x\n", info[0]);
	GP_DEBUG("info[1] = 0x%x\n", info[1]);
	GP_DEBUG("Leaving aox_init\n");

        return GP_OK;
}


int aox_get_num_lo_pics      (Info *info)
{
	GP_DEBUG("Running aox_get_num_lo_pics\n");
	return info[0];
}

int aox_get_num_hi_pics      (Info *info)
{
	GP_DEBUG("Running aox_get_num_hi_pics\n");
	return info[1];
}

int aox_get_picture_size  (GPPort *port, int lo, int hi, int n, int k)
{

	unsigned char c[4];
        unsigned int size;
	memset (c,0,4);

	GP_DEBUG("Running aox_get_picture_size for aox_pic%03i\n", k+1);

	if ( ( (lo) && ( n ==k ) && (k ==0)) ) {
	    	READ(port, 0x04, 0x1, 0x1, (char *)c, 2);
	}
	if ( ( (hi) && ( n < k ) && (n == 0))   ) {
	        READ(port, 0x04, 0x5, 0x1, (char *)c, 2);
	}
	READ(port, 0x05, n+1, 0x1, (char *)c, 4);
	size = (int)c[0] + (int)c[1]*0x100 + (int)c[2]*0x10000;
	GP_DEBUG(" size of picture %i is 0x%x\n", k, size);
	if ( (size >= 0xfffff ) ) {return GP_ERROR;}
	GP_DEBUG("Leaving aox_get_picture_size\n");

	return size;
}

static int aox_read_data         (GPPort *port, char *data, int size)
{
	int MAX_BULK = 0x1000;

	while(size > 0) {
		int len = (size>MAX_BULK)?MAX_BULK:size;
	        gp_port_read  (port, data, len); /* 0x84 = EP IN NEEDED HERE.*/
    		data += len;
		size -= len;
	}
        return 1;
}

int aox_read_picture_data (GPPort *port, char *data, int size, int n) {
	char c[4];
	memset(c,0,4);

    	READ(port, 0x06, n+1, 0x1, c, 4);
        aox_read_data (port, data , size);

        return GP_OK;
}
