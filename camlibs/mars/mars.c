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

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include "gphoto2-endian.h"

#include "mars.h"

#define GP_MODULE "mars" 

#define INIT 		0xb5
#define GET_DATA 	0x0f

static int 
m_read (GPPort *port, char *data, int size) 
{
	gp_port_write(port, "\x21", 1);
    	gp_port_read(port, data, 16); 	
	return GP_OK;
}

static int 
m_command (GPPort *port, char *command, int size, char *response) 
{
	gp_port_write(port, command, size);
    	m_read(port, response, 16); 	
	return GP_OK;
}

static int mars_routine (Info *info, GPPort *port, char param, int n);

int 
mars_init (Camera *camera, GPPort *port, Info *info) 
{
	unsigned char c[16];
	unsigned char status = 0;
	memset(info,0, sizeof(info)); 
	memset(c,0,sizeof(c));
	GP_DEBUG("Running mars_init\n");

	/* Init routine done twice, usually. First time is a dry run. But if 
	 * camera reports 0x02 it is "jammed" and we must clear it.
	 */ 

    	m_read(port, c, 16); 	
	if ( (c[0] == 0x02 ) ) {
		gp_port_write(port, "\x19", 1);
		gp_port_read(port, c, 16);
	}
	else {
		status = mars_routine (info, port, INIT, 0);    
		GP_DEBUG("status = 0x%x\n", status);
	}

	/* Not a typo. This _will_ download the config data ;) */
	mars_read_picture_data (camera, info, port, info, 0x2000, 0); 

	/* Removing extraneous line(s) of data. See "protocol.txt" */
	 
	if ((info[0] == 0xff)&& (info[1] == 0)&&(info[2]==0xff))
		memmove(info, info + 16, 0x1ff0); /* Saving config */
	else
		memcpy(info, info + 144, 0x1f70); /* Saving config */

	GP_DEBUG("Leaving mars_init\n");
        return GP_OK;
}

int 
mars_get_num_pics      (Info *info) 
{
	unsigned int i = 0;

	for (i = 0; i < 0x3fe; i++) 
		if ( !(0xff - info[8*i]) )  {
			GP_DEBUG ( "i is %i\n", i);
			memcpy(info+0x1ff0, "i", 1);
			return i;
		}
	memcpy(info+0x1ff0, "0", 1);
	return 0;
}


int
mars_get_pic_data_size (Info *info, int n)
{
	return (info[8*n+6]*0x100 + info[8*n+5])*0x100 + info[8*n+4];
}

static int 
set_usb_in_endpoint	(Camera *camera, int inep) 
{
	GPPortSettings settings;
	gp_port_get_settings ( camera ->port, &settings);
	settings.usb.inep = inep;
	GP_DEBUG("inep reset to %02X\n", inep);
	return gp_port_set_settings ( camera ->port, settings);
}	


static int 
mars_read_data         (GPPort *port, char *data, int size) 
{ 
	int MAX_BULK = 0x2000;
	int len = 0;
	while(size > 0) {
		len = (size>MAX_BULK)?MAX_BULK:size;
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
	
	memset(c,0,sizeof(c));
	/*Initialization routine for download. */
	mars_routine (info, port, GET_DATA, n);
	/*Data transfer begins*/
	set_usb_in_endpoint (camera, 0x82); 
	mars_read_data (port, data, size); 
	set_usb_in_endpoint (camera, 0x83); 
    	return GP_OK;
} 

int
mars_reset (GPPort *port)
{
	gp_port_write(port, "\x19\x54", 2);
    	return GP_OK;
}

static void precalc_table(code_table_t *table)
{
	int i;
	int is_abs, val, len;

	for (i = 0; i < 256; i++) {
		is_abs = 0;
		val = 0;
		len = 0;
		if ((i & 0x80) == 0) {
    			/* code 0 */
    			val = 0;
    			len = 1;
		}else if ((i & 0xE0) == 0xC0) {
    			/* code 110 */
    			val = -3;
    			len = 3;
		}else if ((i & 0xE0) == 0xA0) {
    			/* code 101 */
    			val = +3;
    			len = 3;
		}else if ((i & 0xF0) == 0x80) {
    			/* code 1000 */
    			val = +7;
    			len = 4;
		}else if ((i & 0xF0) == 0x90) {
    			/* code 1001 */
    			val = -7;
    			len = 4;
		}else if ((i & 0xF0) == 0xF0) {
    			/* code 1111 */
    			val = -15;
    			len = 4;
		}else if ((i & 0xF8) == 0xE0) {
    			/* code 11100 */
    			val = +15;
    			len = 5;
		}else if ((i & 0xF8) == 0xE8) {
    			/* code 11101xxxxx */
    			is_abs = 1;
    			val = 0;	/* value is calculated later */
    			len = 5;
		}
		table[i].is_abs = is_abs;
		table[i].val = val;
		table[i].len = len;
	}
}

#define CLAMP(x)	((x)<0?0:((x)>255)?255:(x))
#define GET_CODE 	addr = inp + (bitpos >> 3); \
	    code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)))


int mars_decompress (unsigned char *inp, unsigned char *outp, int width,
   int height)
{
	int row, col;
	unsigned char code;
	int val;
	code_table_t table[256];
	unsigned char *addr;
	int bitpos;
	unsigned char lp,tp,tlp,trp;
	/* First calculate the Huffman table */
	precalc_table(table);

	bitpos = 0;

	/* main decoding loop */
	for (row = 0; row < height; row++) {
		col = 0;

	/* first two pixels in first two rows are stored as raw 8-bit */
	if (row < 2) {
    		GET_CODE;
    		bitpos += 8;
    		*outp++ = code;

    		GET_CODE;
    		bitpos += 8;
    		*outp++ = code;

    		col += 2;
	}

	while (col < width) {
    		/* get bitcode */
    		GET_CODE;
    		/* update bit position */
    		bitpos += table[code].len;

    		/* calculate pixel value */
    		if (table[code].is_abs) {
    			/* get 5 more bits and use them as absolute value */
    			GET_CODE;
    			val = (code & 0xF8);
    			bitpos += 5;

    			}else {
    				/* value is relative to top or left pixel */
    				val = table[code].val;
    				lp =  outp[-2];
    				if (row > 1) {
        				tlp = outp[-2*width-2];
        				tp  = outp[-2*width];
        				trp = outp[-2*width+2];
    				}
    				if (row < 2) {
        				/* top row: relative to left pixel */
        				val += lp;
    				}else if (col < 2) {
        				/* left column: relative to top pixel */
        				/* initial estimate */
        				val += (2*tp + 2*trp +1)/4; 
    				}else if (col > width - 3) {
        				/* left column: relative to top pixel */
        				val += (2*tp + 2*tlp +1)/4;
					/* main area: average of left and top pixel */
    				}else {
        				/* initial estimate for predictor */
					val += (2*lp + tp + trp + 1)/4;
    				}
    			}
    			/* store pixel */
    			*outp++ = CLAMP(val);
    			col++;
		}
	}
	return GP_OK;
}

static int 
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
    	m_read(port, c, 16); 	
	m_command(port, start, 2, c);
	m_command(port, do_something, 2, c);
	m_command(port, address1, 2, c);

	c[0] = 0;
	gp_port_write(port, address2, 2);	
	/* Moving the memory cursor to the given address? */
	while (( c[0] != 0xa) ) {	
    	m_read(port, c, 16); 	
	}
	
	m_command(port, address3, 2, c);
	m_command(port, address4, 2, c);
	m_command(port, address5, 2, c);
	m_command(port, address6, 2, c);
	gp_port_write(port, "\x19", 1);
	gp_port_read(port, c , 16);
	/* Next thing is to switch the inep. Some cameras need a pause here */
	usleep (MARS_SLEEP); 

	return(c[0]);
}
