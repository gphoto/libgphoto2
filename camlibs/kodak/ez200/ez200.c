/* ez200.c
 *
 * Copyright (C) 2004 Bucas Jean-François <jfbucas@tuxfamily.org>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <gphoto2.h>
#include <gphoto2-port.h>

#include "ez200.h"

#define GP_MODULE "ez200" 

#define WRITE gp_port_usb_msg_write
#define READ  gp_port_usb_msg_read


int ez200_init (GPPort *port, Model *model, Info *info) 
{
	char c = 0;

	GP_DEBUG("Running ez200_init\n");

	/*  photo mode  */
	WRITE(port, ACTIVE, 0, 1, NULL, 0);  /*  index = 1; */
	ez200_wait_status_ok(port);
	
	/*  get number of pictures */
	READ(port, PICTURE, 0, 0, &c, 1);
	memcpy (info, &c, 1);
	GP_DEBUG("number of pics : %i\n", c); 
	
	GP_DEBUG("Leaving ez200_init\n");

        return GP_OK;
}

int ez200_exit (GPPort *port) {
	
	/*  quit photo mode  */
	WRITE(port, ACTIVE, 0, 0, NULL, 0);  /* index = 0; */
	ez200_wait_status_ok(port);

	return GP_OK;
}
	
int ez200_wait_status_ok(GPPort *port) {
	char c = 0;
	
	do READ(port, STATUS, 0, 0, &c, 1);
	while (c != 0); /* Wait */
	
	return GP_OK;
}

int ez200_get_num_pics (Info *info) {
	GP_DEBUG("Running ez200_get_num_pics\n");
	return info[0];
}
	
int ez200_get_picture_size (GPPort *port, int n) {

	unsigned char c[4];
        unsigned int size = 0;
	memset (c,0,sizeof(c));

	GP_DEBUG("Running ez200_get_picture_size\n");
	
    	READ(port, PICTURE, n, 1, c, 3); 
	size = (int)c[0] + (int)c[1]*0x100 + (int)c[2]*0x10000;
	
	GP_DEBUG(" size of picture %i is 0x%x = %i byte(s)\n", n, size, size);
	GP_DEBUG("Leaving ez200_get_picture_size\n");
	
	if ( (size >= 0xfffff ) ) { return GP_ERROR; } 
	return size; 
}

int ez200_read_data (GPPort *port, char *data, int size) { 
	int MAX_BULK = 0x1000;

	/* Read Data by blocks */
	while(size > 0) {
		int len = (size>MAX_BULK)?MAX_BULK:size;
	        gp_port_read  (port, data, len); 
		data += len;
		size -= len;
	}
        return 1;
}

int ez200_read_picture_data (GPPort *port, char *data, int size, int n) {
	char c[4];
	memset(c,0,sizeof(c));

	/* ask picture n transfert */
    	READ(port, PICTURE, n, 1, c, 3);
        ez200_read_data (port, data, size);
	
        return GP_OK;
}


/* after read_picture_data, we can retrieve the header by : */
int ez200_read_picture_header(GPPort *port, char *data) {
	/*  lecture de l'entete :: read header of picture  */
    	READ(port, PICTURE_HEAD, 3, 3, data, HEADER_SIZE);	
	
	return GP_OK;
}


