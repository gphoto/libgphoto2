/* jl2005a.c
 *
 * Copyright (C) 2006 Theodore Kilgore <kilgota@auburn.edu>
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
#include <math.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include "gphoto2-endian.h"

#include "jl2005a.h"

#define GP_MODULE "jl2005a" 

int 
jl2005a_init (Camera *camera, GPPort *port, CameraPrivateLibrary *priv) 
{
	GP_DEBUG("Running jl2005a_init\n");

	jl2005a_shortquery(port, 0x0d); 	/* Supposed to get 0x08 */
	jl2005a_shortquery(port, 0x1c);		/* Supposed to get 0x01 */
	jl2005a_shortquery(port, 0x20);		/* Supposed to get 0x04 */
	gp_port_write (port, "\xab\x00", 2); 
	gp_port_write (port, "\xa1\x02", 2); 
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, "\xa2\x02", 2); 
	jl2005a_shortquery(port, 0x1d);		/* 1 if camera is full, else 0 */
	gp_port_write (port, "\xab\x00", 2); 
	gp_port_write (port, "\xa1\x00", 2); 
	priv->nb_entries = jl2005a_shortquery(port, 0x0a)&0xff;
	/* Number of pix returned here */
	GP_DEBUG("%d entries in the camera\n", priv->nb_entries);
	return jl2005a_shortquery(port, 0x1d);	/* Should get 0, same as GP_OK */
}

int
jl2005a_get_pic_data_size (GPPort *port, int n)
{
	unsigned int size = 0;
	char command[2];
	char response=0;
	command[0] = 0xa1;
	command[1] = (char)(n&0xff);
	GP_DEBUG("Getting photo data size\n");
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, command, 2);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, "\xa2\x0b", 2);
	jl2005a_shortquery(port, 0x1d);
	size = response&0xff;
	GP_DEBUG("size = 0x%x\n", size);
	response = (jl2005a_read_info_byte(port, 1) )&0xff;
	size = ((response&0xff) << 8)| size;
	GP_DEBUG("size = 0x%x\n", size);
	response = (jl2005a_read_info_byte(port, 2 ))&0xff;
	size = ((response&0xff)<<16)|size;
	if (size == 0x3100)
		size += 0x80;
	GP_DEBUG("size = 0x%x\n", size);
	return (size); 
}

int
jl2005a_get_pic_width (GPPort *port)
{
	int width;
	char response;
	response = (jl2005a_read_info_byte(port, 3))&0xff;
	width = (response&0xff);
	response = (jl2005a_read_info_byte(port, 4) )&0xff;
	width = ((response&0xff) << 8)|width;
	return (width); 
}

int
jl2005a_get_pic_height (GPPort *port)
{
	int height;
	char response;
	response = (jl2005a_read_info_byte(port, 5) )&0xff;
	height = (response&0xff);
	response = (jl2005a_read_info_byte(port, 6) )&0xff;
	height = ((response&0xff)<< 8)|height;	
	return (height);
}

int 
set_usb_in_endpoint	(Camera *camera, int inep) 
{
	GPPortSettings settings;
	gp_port_get_settings ( camera ->port, &settings);
	settings.usb.inep = inep;
	GP_DEBUG("inep reset to %02X\n", inep);
	return gp_port_set_settings ( camera ->port, settings);
}	

int 
jl2005a_read_picture_data (Camera *camera, GPPort *port, 
					unsigned char *data, unsigned int size) 
{
	unsigned char *to_read;
	int maxdl = 0xfa00;
	to_read=data;
	jl2005a_read_info_byte(port, 7);
	/* Always 0x80. Purpose unknown */
	jl2005a_read_info_byte(port, 0x0a);
	/* Previous byte is 0x11 if what is to be downloaded is the first
	 * frame in a clip, is 0x01 if it is any clip frame after the initial
	 * one, and is zero if what is to be downloaded is a standalone photo.
	 * If clips will in the future be processed as AVI files, then there is
	 * not any information to know how many frames are present, prior to 
	 * downloading them. There is only a starting point and an indicator
	 * for each frame. 
	 */
        gp_port_write (port, "\xab\x00", 2);
        gp_port_write (port, "\xa1\x04", 2);
        gp_port_write (port, "\xab\x00", 2);
        gp_port_write (port, "\xa2\x08", 2);
        gp_port_write (port, "\xab\x00", 2);
        gp_port_write (port, "\xa1\x05", 2);
        gp_port_write (port, "\xab\x00", 2);
        gp_port_write (port, "\xa2\x08", 2);

	/* Switch the inep over to 0x81. */ 
	set_usb_in_endpoint	(camera, 0x81); 
	while (size > maxdl) {
		gp_port_read(port, (char *)to_read, maxdl);
		to_read += maxdl;
		size -= maxdl;
	}
	gp_port_read(port, (char *)to_read, size);
	/* Switch the inep back to 0x84. */ 
	set_usb_in_endpoint	(camera, 0x84);
	return GP_OK;
}

int
jl2005a_reset (Camera *camera, GPPort *port)
{
	int i; 
	gp_port_write (port,"\xab\x00" , 2);
	gp_port_write (port, "\xa1\x00", 2);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, "\xa2\x02", 2);
	for (i=0; i < 4; i++) 
		jl2005a_shortquery(port, 0x1d);
	/* Supposed to get something like 0x01, 0x01, 0x01, 0x00 */
    	return GP_OK;
}

int jl2005a_read_info_byte(GPPort *port, int n) 
{
	char response;
	char command[2];
	command[0] = 0xa1;
	command[1] = (char)(n&0xff);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, command , 2);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, "\xa2\x0c", 2);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, "\xa3\xa1", 2);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_read (port,&response, 1);
	return response&0xff;

}

int jl2005a_shortquery(GPPort *port, int n)
{
	char response;
	char command[2];
	command[0] = 0xa2;
	command[1] = (char)(n&0xff);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, command, 2);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, "\xa3\xa1", 2);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_write (port, "\xab\x00", 2);
	gp_port_read (port, &response, 1);
	return response&0xff;
}

int jl2005a_decompress (unsigned char *inp, unsigned char *outp, int width,
   int height)
{
	int i,j;
	for (i=0; i < height/2; i+=2) {
		memcpy(outp+2*i*width,inp+i*width, 2*width);
	}
	memcpy(outp+(height-2)*width,outp+(height-4)*width, 2*width);
	for (i=0; i < height/4-1; i++) {
		for (j=0; j < width; j++) {
			outp[(4*i+2)*width+j]=(inp[(2*i)*width+j]+
						inp[(2*i+2)*width+j])/2;
				outp[(4*i+3)*width+j]=(outp[(4*i+1)*width+j]+
						outp[(4*i+5)*width+j])/2;
		}
	}
	if (width == 176) 
		memmove(outp+6*width, outp, (height-6)*width);

	return 0;
}

