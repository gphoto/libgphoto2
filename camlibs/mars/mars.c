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
#include <gphoto2-endian.h>

#include "mars.h"

#define GP_MODULE "mars" 

#define INIT 		0xb5
#define GET_DATA 	0x0f
#define RESET		0xba

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

	for (i = 0; i < 0x3fe; i++) if ( !(0xff - info[8*i]) )  
	{
	GP_DEBUG ( "i is %i\n", i);
	memcpy(info+0x1ff0, "i", 1);
	return i;
	}
	memcpy(info+0x1ff0, "0", 1);
	return 0;
}

int
mars_chk_compression (Info *info, int n)
{
	return	((info[8*n] >>4) & 2);
}

int
mars_get_picture_width (Info *info, int n)
{
    	switch (info[8*n] & 0x0f) {  
	case 0x00: return 176;
	case 0x02: return 352;
	case 0x06: return 320;
	case 0x08: return 640;
	default:
		GP_DEBUG ("Your pictures have unknown width.\n");
		return (GP_ERROR_NOT_SUPPORTED);
	}
}

int
mars_get_pic_data_size (Info *info, int n)
{
	return (info[8*n+6]*0x100 + info[8*n+5])*0x100 + info[8*n+4];
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
	
	memset(c,0,sizeof(c));
	/*Initialization routine for download. */
	mars_routine (info, port, GET_DATA, n);
	/*Data transfer begins*/
	set_usb_in_endpoint (camera, 0x82); 
	mars_read_data (camera, port, data, size);
	set_usb_in_endpoint (camera, 0x83); 	
    	return GP_OK;
} 

int
mars_reset (GPPort *port)
{
	gp_port_write(port, "\x19\x54", 2);	
    	return GP_OK;
}


void precalc_table(code_table_t *table)
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
		}
		else if ((i & 0xE0) == 0xC0) {
			/* code 110 */
			val = -3;
			len = 3;
		}
		else if ((i & 0xE0) == 0xA0) {
			/* code 101 */
			val = +3;
			len = 3;
		}
		else if ((i & 0xF0) == 0x80) {
			/* code 1000 */
			val = +8;
			len = 4;
		}
		else if ((i & 0xF0) == 0x90) {
			/* code 1001 */
			val = -8;
			len = 4;
		}
		else if ((i & 0xF0) == 0xF0) {
			/* code 1111 */
		//	val = -20;
			val = -20;
			len = 4;
		}
		else if ((i & 0xF8) == 0xE0) {
			/* code 11100 */
		//	val = +20;
			val = +20;
			len = 5;
		}
		else if ((i & 0xF8) == 0xE8) {
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


int mars_decompress (unsigned char *inp, 
				unsigned char *outp, int width, int height)
{
	int row, col;
	unsigned char code;
	int val;
	code_table_t table[256];
	unsigned char *addr;
	int bitpos;
	unsigned char A,B,C,D;
	/* First calculate the Huffman table */
	precalc_table(table);
	

	bitpos = 0;

	/* main decoding loop */
	for (row = 0; row < height; row++) {
		col = 0;

		/* first two pixels in first two rows are stored as raw 8-bit */
		if (row < 2) {
			addr = inp + (bitpos >> 3);
			code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));
			bitpos += 8;
			*outp++ = code;

			addr = inp + (bitpos >> 3);
			code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));
			bitpos += 8;
			*outp++ = code;

			col += 2;
		}

		while (col < width) {
			/* get bitcode */
			addr = inp + (bitpos >> 3);
			code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));

			/* update bit position */
			bitpos += table[code].len;

			/* calculate pixel value */
			if (table[code].is_abs) {
				/* get 5 more bits and use them as absolute value */
				addr = inp + (bitpos >> 3);
				code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));
				val = (code & 0xF8);
				bitpos += 5;
			}
			else {
				/* value is relative to top or left pixel */
				val = table[code].val;
				if (row < 2) {
					/* top row: relative to left pixel */
					val += outp[-2];
				}
				else if (col < 2) {
					/* left column: relative to top pixel */
					val += (outp[-2*width] + 
						    outp[-2*width + 2])/2;
				
				}
				else if (col > width - 3) {
					/* left column: relative to top pixel */
					val += (outp[-2*width] +
					    outp[-2*width - 2])/2;
				}
				
				else {
					/* main area: average of left pixel and top pixel */
					A=outp[-2];
					C=outp[-2*width-2];
					B=outp[-2*width ];
					D=outp[-2*width+2];
					if (D-A >= 0)
						val += MIN((D-A)/7, 32)+ (A+B)/2;
				
					else 
						val += -MIN((A-D)/7, 32)+ (A+B)/2;    
				}
			}

			/* store pixel */
			*outp++ = CLAMP(val);
			col++;
		}
	}

	return 0;
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
	if ( (c[0] == 0x02 ) ) {	/* Clears camera if jammed */
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




/* Brightness correction routine adapted from 
 * camlibs/polaroid/jd350e.c, copyright © 2001 Michael Trawny 
 * <trawny99@users.sourceforge.net>
 */


#define RED(p,x,y,w) *((p)+3*((y)*(w)+(x))  )
#define GREEN(p,x,y,w) *((p)+3*((y)*(w)+(x))+1)
#define BLUE(p,x,y,w) *((p)+3*((y)*(w)+(x))+2)

#define SWAP(a,b) {unsigned char t=(a); (a)=(b); (b)=t;}

#define MINMAX(a,min,max) { (min)=MIN(min,a); (max)=MAX(max,a); }

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif


int 
mars_postprocess(CameraPrivateLibrary *priv, int width, int height, 
			int is_compressed, unsigned char* rgb, int n)
{
	int
		x,y,
		red_min=255, red_max=0, 
		blue_min=255, blue_max=0, 
		green_min=255, green_max=0;
	double
		min, max, amplify = 1.0, THE_MAX = 255.0, THE_MIN = 0.0;

	if (is_compressed) {
		THE_MAX = 239.0;	
		THE_MIN = 16.0;
	}	

	/* determine min and max per color... */

	for( y=0; y<height; y++){
		for( x=0; x<width; x++ ){
			MINMAX( RED(rgb,x,y,width), red_min,   red_max  );
			MINMAX( GREEN(rgb,x,y,width), green_min, green_max);
			MINMAX( BLUE(rgb,x,y,width), blue_min,  blue_max );
		}
	}




	/* Normalize brightness ... */

	max = MAX( MAX( red_max, green_max ), blue_max);
	min = MIN( MIN( red_min, green_min ), blue_min);
	amplify = (255.0)/(max-min);

	for( y=0; y<height; y++){
		for( x=0; x<width; x++ ){
			RED(rgb,x,y,width) = 
			    MIN(amplify*(double)(RED(rgb,x,y,width)-min),THE_MAX);
			GREEN(rgb,x,y,width) = 
			    MIN(amplify*(double)(GREEN(rgb,x,y,width)-min),THE_MAX);
			BLUE(rgb,x,y,width) = 
			    MIN(amplify*(double)(BLUE(rgb,x,y,width)-min),THE_MAX);
		}
	}

	return GP_OK;
}

