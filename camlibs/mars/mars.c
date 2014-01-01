/* mars.c
 *
 * Copyright (C) 2004 Theodore Kilgore <kilgota@auburn.edu>
 *
 * white_balance() Copyright (C) 2008 Theodore Kilgore and Amauri Magagna.
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

#define _BSD_SOURCE

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <gamma.h>

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
	char c[16];
	unsigned char status = 0;
	memset(info,0, sizeof(*info)); 
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
	mars_read_picture_data (camera, info, port, (char *)info, 0x2000, 0); 

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
    			val = +8;
    			len = 4;
		}else if ((i & 0xF0) == 0x90) {
    			/* code 1001 */
    			val = -8;
    			len = 4;
		}else if ((i & 0xF0) == 0xF0) {
    			/* code 1111 */
    			val = -20;
    			len = 4;
		}else if ((i & 0xF8) == 0xE0) {
    			/* code 11100 */
    			val = +20;
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
	unsigned char lp=0,tp=0,tlp=0,trp=0;
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
        				if (col < width-2)
        					trp = outp[-2*width+2];
    				}
    				if (row < 2) {
        				/* top row: relative to left pixel */
        				val += lp;
    				}else if (col < 2) {
        				/* left column: relative to top pixel */
        				/* initial estimate */
        				val += (tp + trp)/2; 
    				}else if (col > width - 3) {
        				/* left column: relative to top pixel */
        				val += (tp + lp + tlp +1)/3;
					/* main area: average of left and top pixel */
    				}else {
        				/* initial estimate for predictor */
					tlp>>=1;
					trp>>=1;
					val += (lp + tp + tlp + trp +1)/3;
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
	char c[16];
	char start[2] = {0x19, 0x51};
	char do_something[2]; 
	char address1[2];
	char address2[2];
	char address3[2];
	char address4[2];
	char address5[2];
	char address6[2];

	do_something[0]= 0x19; 
	do_something[1]=param;

	/* See protocol.txt for my theories about what these mean. */
	address1[0] = 0x19;
	address1[1] = info[8*n+1];
	address2[0] = 0x19;
	address2[1] = info[8*n+2];
	address3[0] = 0x19;
	address3[1] = info[8*n+3];
	address4[0] = 0x19;
	address4[1] = info[8*n+4];
	address5[0] = 0x19;
	address5[1] = info[8*n+5];
	address6[0] = 0x19;
	address6[1] = info[8*n+6];


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


/*
 *	========= White Balance / Color Enhance / Gamma adjust ===============
 *
 *	Get histogram for each color plane
 *	Expand to reach 0.5% of white dots in image
 *
 *	Get new histogram for each color plane
 *	Expand to reach 0.5% of black dots in image
 *
 *	Get new histogram
 *	Calculate and apply gamma correction
 *
 *	if not a dark image:
 *	For each dot, increases color separation
 *
 *	======================================================================
 */

int
histogram (unsigned char *data, unsigned int size, int *htable_r, int *htable_g, int *htable_b)
{
	int x;
	/* Initializations */
	for (x = 0; x < 0x100; x++) { 
		htable_r[x] = 0; 
		htable_g[x] = 0; 
		htable_b[x] = 0; 
	}
	/* Building the histograms */
	for (x = 0; x < (size * 3); x += 3)
	{
		htable_r[data[x+0]]++;	/* red histogram */
		htable_g[data[x+1]]++;	/* green histogram */
		htable_b[data[x+2]]++;	/* blue histogram */
	}
	return 0;
}

int
mars_white_balance (unsigned char *data, unsigned int size, float saturation,
						float image_gamma)
{
	int x, r, g, b, max, d;
	double r_factor, g_factor, b_factor, max_factor;
	int htable_r[0x100], htable_g[0x100], htable_b[0x100];
	unsigned char gtable[0x100];
	double new_gamma, gamma=1.0;

	/* ------------------- GAMMA CORRECTION ------------------- */

	histogram(data, size, htable_r, htable_g, htable_b);
	x = 1;
	for (r = 48; r < 208; r++)
	{
		x += htable_r[r]; 
		x += htable_g[r];
		x += htable_r[r]; 
	}
	new_gamma = sqrt((double) (x * 1.5) / (double) (size * 3));
	x=0;
	GP_DEBUG("Provisional gamma correction = %1.2f\n", new_gamma);
	/* Recalculate saturation factor for later use. */
	saturation=saturation*new_gamma*new_gamma;
	GP_DEBUG("saturation = %1.2f\n", saturation);
	if(new_gamma >= 1.0)
		gamma = new_gamma;
	else
		gamma = image_gamma;

	GP_DEBUG("Gamma correction = %1.2f\n", gamma);
	gp_gamma_fill_table(gtable, gamma);

	/* ---------------- BRIGHT DOTS ------------------- */
	max = size / 200; 
	histogram(data, size, htable_r, htable_g, htable_b);

	for (r=0xfe, x=0; (r > 32) && (x < max); r--)  
		x += htable_r[r]; 
	for (g=0xfe, x=0; (g > 32) && (x < max); g--) 
		x += htable_g[g];
	for (b=0xfe, x=0; (b > 32) && (x < max); b--) 
		x += htable_b[b];
	r_factor = (double) 0xfd / r;
	g_factor = (double) 0xfd / g;
	b_factor = (double) 0xfd / b;

	max_factor = r_factor;
	if (g_factor > max_factor) max_factor = g_factor;
	if (b_factor > max_factor) max_factor = b_factor;

	if (max_factor >= 2.5) {
		r_factor = (r_factor / max_factor) * 2.5;
		g_factor = (g_factor / max_factor) * 2.5;
		b_factor = (b_factor / max_factor) * 2.5;
	}
	GP_DEBUG("White balance (bright): r=%1d, g=%1d, b=%1d, fr=%1.3f, fg=%1.3f, fb=%1.3f\n", r, g, b, r_factor, g_factor, b_factor);
	if (max_factor <= 2.5) {
		for (x = 0; x < (size * 3); x += 3)
		{
			d = (data[x+0]<<8) * r_factor;
			d >>=8;
			if (d > 0xff) { d = 0xff; }
			data[x+0] = d;
			d = (data[x+1]<<8) * g_factor;
			d >>=8;
			if (d > 0xff) { d = 0xff; }
			data[x+1] = d;
			d = (data[x+2]<<8) * b_factor;
			d >>=8;
			if (d > 0xff) { d = 0xff; }
			data[x+2] = d;
		}
	}
	/* ---------------- DARK DOTS ------------------- */
	max = size / 200;  /*  1/200 = 0.5%  */
	histogram(data, size, htable_r, htable_g, htable_b);

	for (r=0, x=0; (r < 96) && (x < max); r++)  
		x += htable_r[r]; 
	for (g=0, x=0; (g < 96) && (x < max); g++) 
		x += htable_g[g];
	for (b=0, x=0; (b < 96) && (x < max); b++) 
		x += htable_b[b];

	r_factor = (double) 0xfe / (0xff-r);
	g_factor = (double) 0xfe / (0xff-g);
	b_factor = (double) 0xfe / (0xff-b);

	max_factor = r_factor;
	if (g_factor > max_factor) max_factor = g_factor;
	if (b_factor > max_factor) max_factor = b_factor;

	if (max_factor >= 1.15) {
		r_factor = (r_factor / max_factor) * 1.15;
		g_factor = (g_factor / max_factor) * 1.15;
		b_factor = (b_factor / max_factor) * 1.15;
	}
	GP_DEBUG(
	"White balance (dark): r=%1d, g=%1d, b=%1d, fr=%1.3f, fg=%1.3f, fb=%1.3f\n", 
				r, g, b, r_factor, g_factor, b_factor);

	for (x = 0; x < (size * 3); x += 3)
	{
		d = (int) 0xff08-(((0xff-data[x+0])<<8) * r_factor);
		d >>= 8;
		if (d < 0) { d = 0; }
		data[x+0] = d;
		d = (int) 0xff08-(((0xff-data[x+1])<<8) * g_factor);
		d >>= 8;
		if (d < 0) { d = 0; }
		data[x+1] = d;
		d = (int) 0xff08-(((0xff-data[x+2])<<8) * b_factor);
		d >>= 8;
		if (d < 0) { d = 0; }
		data[x+2] = d;
	}

	/* ------------------ COLOR ENHANCE ------------------ */

	if(saturation > 0.0) {
		for (x = 0; x < (size * 3); x += 3)
		{
			r = data[x+0]; g = data[x+1]; b = data[x+2];
			d = (int) (r + g + b) /3.;
			if ( r > d )
				r = r + (int) ((r - d) * (0xff-r)/(0x100-d) * saturation);
			else 
				r = r + (int) ((r - d) * (0xff-d)/(0x100-r) * saturation);
			if (g > d)
				g = g + (int) ((g - d) * (0xff-g)/(0x100-d) * saturation);
			else 
				g = g + (int) ((g - d) * (0xff-d)/(0x100-g) * saturation);
			if (b > d)
				b = b + (int) ((b - d) * (0xff-b)/(0x100-d) * saturation);
			else 
				b = b + (int) ((b - d) * (0xff-d)/(0x100-b) * saturation);
			data[x+0] = CLAMP(r);
			data[x+1] = CLAMP(g);
			data[x+2] = CLAMP(b);
		}
	}
	return 0;
}
