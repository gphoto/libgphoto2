/*
 * postprocess.c
 *
 * Here are the decompression function  for the compressed photos and the 
 * postprocessing for uncompressed photos. 
 *
 * Copyright (c) 2003 Theodore Kilgore <kilgota@auburn.edu>
 * Camera library support under libgphoto2.1.1 for camera(s) 
 * with chipset from Service & Quality Technologies, Taiwan. 
 * The chip supported by this driver is presumed to be the SQ905,  
 *
 * Licensed under GNU Lesser General Public License, as part of Gphoto
 * camera support project. For a copy of the license, see the file 
 * COPYING in the main source tree of libgphoto2.
 */    

#include <config.h>


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#ifdef OS2
#include <db.h>
#endif

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include "sq905.h"

#define GP_MODULE "sq905" 

#define RED 0
#define GREEN 1
#define BLUE 2

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif


static int 
decode_panel	(unsigned char *panel_out, unsigned char *panel,               
                    		int panelwidth, int height, int color);

int
sq_decompress (SQModel model, unsigned char *output, unsigned char *data,
	    int w, int h)
{
	/*
	 * Data arranged in planar form. We decompress the raw data first, 
	 * one colorplane at a time, and put the results into a standard 
	 * Bayer pattern. The byte-reversal routine having been done already,
	 * the planes are in the order RBG. The R and B planes  each have 
	 * dimensions (w/4)*(h/2), and the G is (w/4)*h. 
	 */

	unsigned char *red, *green, *blue, *red_out, *green_out, *blue_out;
	int i, m;
	unsigned char temp;

	red = data;
	blue = data + w*h/8;
	green = data + w*h/4;

	red_out = malloc(w*h/4);
	if (!red_out)
		return -1;
	blue_out = malloc(w*h/4);
	if (!blue_out) {
		free (red_out);
		return -1;
	}
	green_out = malloc(w*h/2);
	if (!green_out) {
		free (red_out);
		free (blue_out);
		return -1;
	}
	decode_panel (red_out, red, w/2, h/2, RED);
	decode_panel (blue_out, blue, w/2, h/2, BLUE);		
	decode_panel (green_out, green, w/2, h, GREEN);


	/* Now, put things in their proper places */
	
	for ( m = 0; m < h/2 ; m++ ) {
		for ( i = 0; i < w/2; i++ ) {
			/* Reds in even rows, even columns */
			output[(2*m)*w+2*i ] = red_out[m*w/2+i ];
			/* Blues in odd rows, odd columns */
			output[(2*m+1)*w+2*i +1] = blue_out[m*w/2+i];
			/* For the greens (note m*w = (2*m)*w/2) we 
			 * get first the even rows, odd columns */
			output[(2*m)*w+ 2*i+1] = green_out[m*w +i];
			/* and then, the greens on odd rows, even columns. */
			output[(2*m+1)*w+ 2*i] = green_out[(2*m+1)*w/2 +i];
		}
	}
	


	/* De-mirroring for some models */
	switch(model) {
	case(SQ_MODEL_MAGPIX):
	case(SQ_MODEL_POCK_CAM):
        for (i = 0; i < h; i++) {                                              
	        for (m = 0 ; m < w/2; m++) {
	                temp = output[w*i +m];
	                output[w*i + m] = output[w*i + w -1 -m];
	                output[w*i + w - 1 - m] = temp;
	        }
	}
		break;
	default: ; 		/* default is "do nothing" */
	}
	free (red_out);
	free (green_out);
	free (blue_out);
	return(GP_OK);
}

static
int decode_panel (unsigned char *panel_out, unsigned char *panel,
			int panelwidth, int height, int color) {
	/* Here, "panelwidth" signifies width of panel_out 
	 * which is w/2, and height equals h */

	int diff = 0;
	int tempval = 0;
	int i, m;
	unsigned char delta_left = 0;
	unsigned char delta_right = 0;
	int input_counter = 0;

	int delta_table[] = {-144,-110,-77,-53,-35,-21,-11,-3,
				2,10,20,34,52,76,110,144};

	unsigned char *temp_line;	
	temp_line = malloc(panelwidth);

	if (!temp_line)
		return -1;

	for(i=0; i < panelwidth; i++){
	    temp_line[i] = 0x80;
	}

	if (color != GREEN) {
		for (m=0; m < height; m++) {

			for (i=0; i< panelwidth/2; i++) {

				delta_left = panel[input_counter] &0x0f;
				delta_right = (panel[input_counter]>>4)&0xff;
				input_counter ++;
				/* left pixel */
				diff = delta_table[delta_left];
				if (!i) 
					tempval = (temp_line[2*i]) + diff;
				else 
					tempval = (temp_line[2*i]
				        + panel_out[m*panelwidth+2*i-1])/2 + diff;
				tempval = MIN(tempval, 0xff);
				tempval = MAX(tempval, 0);
				panel_out[m*panelwidth+2*i] = tempval;
				temp_line[2*i] = panel_out[m*panelwidth + 2*i];
				/* right pixel */
				diff = delta_table[delta_right];
				tempval = (temp_line[2*i+1]
					+ panel_out[m*panelwidth+2*i])/2 + diff;
				tempval = MIN(tempval, 0xff);
				tempval = MAX(tempval, 0);
				panel_out[m*panelwidth+2*i+1] = tempval;
				temp_line[2*i+1] = panel_out[m*panelwidth + 2*i+1];
			}
		}
		free (temp_line);
		return 0;
	} else {	/* greens */
		for (m=0; m < height/2; m++) {
			/* First we do an even line */
			for (i=0; i< panelwidth/2; i++) {
				delta_left = panel[input_counter] &0x0f;
				delta_right = (panel[input_counter]>>4)&0xff;
				input_counter ++;
				/* left pixel */
				diff = delta_table[delta_left];
				if (!i) 
					tempval = (temp_line[0]+temp_line[1])/2 + diff;
				else 
					tempval = (temp_line[2*i+1]
				        + panel_out[2*m*panelwidth+2*i-1])/2 + diff;
				tempval = MIN(tempval, 0xff);
				tempval = MAX(tempval, 0);
				panel_out[2*m*panelwidth+2*i] = tempval;
				temp_line[2*i] = tempval;
				/* right pixel */
				diff = delta_table[delta_right];
				if (2*i == panelwidth - 2 ) 
					tempval = (temp_line[2*i+1]
						+ panel_out
							[2*m*panelwidth+2*i])/2 
							+ diff;
				else
					tempval = (temp_line[2*i+2]
						+ panel_out
							[2*m*panelwidth+2*i])/2 
							+ diff;
				tempval = MIN(tempval, 0xff);
				tempval = MAX(tempval, 0);
				panel_out[2*m*panelwidth+2*i+1] = tempval;
				temp_line[2*i+1] = tempval;
			}
			/* then an odd line */
			for (i=0; i< panelwidth/2; i++) {
				delta_left = panel[input_counter] &0x0f;
				delta_right = (panel[input_counter]>>4)&0xff;
				input_counter ++;
				/* left pixel */
				diff = delta_table[delta_left];
				if (!i) 
					tempval = (temp_line[2*i]) + diff;
				else 
					tempval = (temp_line[2*i]
				    	    + panel_out
						[(2*m+1)*panelwidth+2*i-1])/2 
						+ diff;
				tempval = MIN(tempval, 0xff);
				tempval = MAX(tempval, 0);
				panel_out[(2*m+1)*panelwidth+2*i] = tempval;
				temp_line[2*i] = tempval;
				/* right pixel */
				diff = delta_table[delta_right];
				tempval = (temp_line[2*i+1]
					+ panel_out[(2*m+1)*panelwidth+2*i])/2 
					+ diff;
				tempval = MIN(tempval, 0xff);
				tempval = MAX(tempval, 0);
				panel_out[(2*m+1)*panelwidth+2*i+1] = tempval;
				temp_line[2*i+1] = tempval;
			}
		}
		free (temp_line);
		return GP_OK;
	}	
}


