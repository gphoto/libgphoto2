/* mars.c
 *
 * Copyright (C) 2004 Theodore Kilgore <kilgota@auburn.edu>
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
#include <math.h>

#include <gphoto2.h>
#include <gphoto2-port.h>

#include "mars.h"

#define GP_MODULE "mars" 

#define INIT 		0xb5
#define GET_DATA 	0x0f
#define RESET		0xba

int 
mars_init (Camera *camera, GPPort *port, Model *model, Info *info) 
{
	unsigned char c[16];
	unsigned char status = 0;
	memset(info,0, sizeof(info)); 
	memset(c,0,sizeof(c));
	GP_DEBUG("Running mars_init\n");

	/* Init routine done twice, usually. First time is a dry run. But if 
	 * camera reports 0x02 it is "jammed" and we must clear it.
	 */ 

    	M_READ(port, c, 16); 	
	if ( (c[0] == 0x02 ) ) {
		gp_port_write(port, "\x19", 1);
		gp_port_read(port, c, 16);
	}

	while ( (status != 0xb5)){
	status = mars_routine (info, port, INIT, 0);    
	GP_DEBUG("status = 0x%x\n", status);
	}

	/* Not a typo. This _will_ download the config data ;) */
	mars_read_picture_data (camera, info, port, info, 0x2000, 0); 

	/* Removing extraneous line or lines of config data. See "protocol.txt/" */
	 
	switch (info[0]) {

	case 0xff:
		memmove(info, info + 16, 0x1ff0); /* Saving config */
		break;
	default: 
		memcpy(info, info + 144, 0x1f70); /* Saving config */
	}

	GP_DEBUG("Leaving mars_init\n");
        return GP_OK;
}

int 
mars_get_num_pics      (Info *info) 
{
	unsigned int i = 0;

	for (i = 0; i < 0x3fe; i++) if ( !(0xff - info[8*i]) )  
	{
	GP_DEBUG ( "i is %i\n", i);
	return i;
	}
	return 0;
}

int
mars_get_comp_ratio (Info *info, int n)
{
    	switch (info[8*n]) {
	case 0x28:
	case 0x26:
	case 0xa8:
	case 0xa6: return 3;
	case  0x8:
	case 0x88:
	case  0x6:
	case 0x86: return 1;
	default:
		GP_DEBUG ("Your camera has unknown resolution settings.\n");
		return (GP_ERROR_NOT_SUPPORTED);
	}
}

int
mars_get_picture_width (Info *info, int n)
{
    	switch (info[8*n]) {  
	case 0x6:
	case 0x26:
	case 0x86:
	case 0xa6: return 320;
	case 0x8:
	case 0x88:
	case 0x28:
	case 0xa8: return 640;
	default:
		GP_DEBUG ("Your pictures have unknown width.\n");
		return (GP_ERROR_NOT_SUPPORTED);
	}
}

int 
set_usb_in_endpoint	     (Camera *camera, int inep) 
{
	GPPortSettings settings;
	
	gp_port_get_settings ( camera ->port, &settings);
	settings.usb.inep = inep;
	return gp_port_set_settings ( camera ->port, settings);
}	

int 
mars_read_data         (Camera *camera, GPPort *port, char *data, int size) 
{ 
	int MAX_BULK = 0x2000;

	while(size > 0) {
		int len = (size>MAX_BULK)?MAX_BULK:size;
	        gp_port_read  (port, data, len); 
    		data += len;
		size -= len;
	}

        return 1;
}

int 
mars_read_picture_data (Camera *camera, Info *info, GPPort *port, 
					char *data, int size, int n) 
{
	unsigned char c[16];
	unsigned char complete[2] = {0x19, 0x54};
	unsigned char status = 0;	
	memset(c,0,sizeof(c));
	/*Initialization routine for download. */
	while( (status != 0xa8 )) {
		status = mars_routine (info, port, GET_DATA, n);
	}
	/*Data transfer begins*/
	set_usb_in_endpoint (camera, 0x82); 
	mars_read_data (camera, port, data, size);
	set_usb_in_endpoint (camera, 0x83); 	
	gp_port_write(port, complete, 2);		
	
    	return GP_OK;
} 

int
mars_reset (GPPort *port)
{
	unsigned char bogus[6];
	memset(bogus, 0, sizeof(bogus)); 
	mars_routine (bogus, port, RESET, 0); 
    	return GP_OK;
}

int 
mars_decompress (char *data,char *p_data, int b, int w, int h) 
{
	/* FIXME! This is supposed to decompress the photo! */

	int i;
	memmove(data, data+140, b);
	for ( i=0; i< b; i++ ) {
	
	memcpy(p_data+3*i,data+i, 1);
	}
	return GP_OK;
}

int 
M_READ (GPPort *port, char *data, int size) 
{
	gp_port_write(port, "\x21", 1);
    	gp_port_read(port, data, 16); 	
	return GP_OK;
}

int 
M_COMMAND (GPPort *port, char *command, int size, char *response) 
{
	gp_port_write(port, command, size);	
    	M_READ(port, response, 16); 	
	return GP_OK;
}

int 
mars_routine (Info *info, GPPort *port, char param, int n) 
{
	unsigned char c[16];
	unsigned char start[2] = {0x19, 0x51};
	unsigned char do_something[2]; 
	/* See protocol.txt for my theories about what these mean. */
	unsigned char address1[2] = {0x19, info[8*n+1]};
	unsigned char address2[2] = {0x19, info[8*n+2]};
	unsigned char address3[2] = {0x19, info[8*n+3]};
	unsigned char address4[2] = {0x19, info[8*n+4]};
	unsigned char address5[2] = {0x19, info[8*n+5]};
	unsigned char address6[2] = {0x19, info[8*n+6]};

	do_something[0]= 0x19; 
	do_something[1]=param;

	memset(c,0,sizeof(c));

	/*Routine used in initialization, photo download, and reset. */

    	M_READ(port, c, 16); 	
	if ( (c[0] == 0x02 ) ) {		/* Clears camera if jammed. */
		gp_port_write(port, "\x19", 1);
		gp_port_read(port, c, 16);
    		M_READ(port, c, 16); 	
	}

	M_COMMAND(port, start, 2, c);
	M_COMMAND(port, do_something, 2, c);
	M_COMMAND(port, address1, 2, c);

	c[0] = 0;
	gp_port_write(port, address2, 2);	

	/* Moving the memory cursor to the given address? */
	while (( c[0] != 0xa) ) {	
    	M_READ(port, c, 16); 	
	}

	M_COMMAND(port, address3, 2, c);
	M_COMMAND(port, address4, 2, c);
	M_COMMAND(port, address5, 2, c);
	M_COMMAND(port, address6, 2, c);
	
	gp_port_write(port, "\x19", 1);
	gp_port_read(port, c , 16);	

	return(c[0]);
}


int 
mars_get_gamma(Info *info, int n) 
{
return info[8*n + 7];
}

