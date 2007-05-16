/*
 * postprocess.c
 *
 * Here are the decompression function  for the compressed photos and the 
 * postprocessing for uncompressed photos. 
 *
 * Copyright (c) 2005 and 2007 Theodore Kilgore <kilgota@auburn.edu>
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
#include "digigr8.h"

#define GP_MODULE "digigr8" 

static int
digi_first_decompress (unsigned char *output, unsigned char *input,
					    unsigned int outputsize)
{
	unsigned char parity = 0;
	unsigned char nibble_to_keep[2];
	unsigned char temp1 = 0, temp2 = 0;
	unsigned char input_byte;
	unsigned char lookup = 0;
	unsigned int i = 0;
	unsigned int bytes_used = 0;
	unsigned int bytes_done = 0;
	unsigned int bit_counter = 8;
	unsigned int cycles = 0;
	int table[9] = { -1, 0, 2, 6, 0x0e, 0x0e, 0x0e, 0x0e, 0xfb};
	unsigned char lookup_table[16]
		     ={0, 2, 6, 0x0e, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4,
		           0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
	unsigned char translator[16] = {8,7,9,6,10,11,12,13,14,15,5,4,3,2,1,0};

	GP_DEBUG ("Running first_decompress.\n");
	nibble_to_keep[0] = 0;
	nibble_to_keep[1] = 0;

        while (bytes_done < outputsize) {
        	while (parity < 2 ) {
			while ( lookup > table[cycles]) {
				if (bit_counter == 8) {
        				input_byte = input[bytes_used];
        				bytes_used ++;
					temp1 = input_byte;
        				bit_counter = 0;
        			}
				input_byte = temp1;
				temp2 = (temp2 << 1) & 0xFF;
				input_byte = input_byte >> 7;
				temp2 = temp2 | input_byte;
				temp1 = (temp1 <<1)&0xFF;
				bit_counter ++ ;
    				cycles ++ ;
        			if (cycles > 9) {
					GP_DEBUG ("Too many cycles?\n");
					return -1; 
    				}
        			lookup = temp2 & 0xff;        
			}
			temp2 = 0;
			for (i=0; i < 17; i++ ) {
				if (lookup == lookup_table[i] ) {
					nibble_to_keep[parity] = translator[i];
					break;	
				}
				if (i == 16) {
					GP_DEBUG ("Illegal lookup value during decomp\n");
					return -1;
				}	
			}		
        		cycles = 0;
        		parity ++ ;
        	} 
            	output[bytes_done] = (nibble_to_keep[0]<<4)|nibble_to_keep[1];
            	bytes_done ++ ;
        	parity = 0; 
        }
	GP_DEBUG ("bytes_used = 0x%x = %i\n", bytes_used, bytes_used);        
        return GP_OK;
}

static int
digi_second_decompress (unsigned char *uncomp, unsigned char *in, 
						    int width, int height)
{
	int diff = 0;
	int tempval = 0;
	int i, m, parity;
	unsigned char delta_left = 0;
	unsigned char delta_right = 0;
	int input_counter = 0;
	int delta_table[] = {-144, -110, -77, -53, -35, -21, -11, -3,
				2, 10, 20, 34, 52, 76, 110, 144};
	unsigned char *templine_red;
	unsigned char *templine_green;
	unsigned char *templine_blue;
	templine_red = malloc(width);
	if (!templine_red) {
		free(templine_red);
		return -1;
	}	
	for(i=0; i < width; i++){
	    templine_red[i] = 0x80;
	}
	templine_green = malloc(width);	
	if (!templine_green) {
		free(templine_green);
		return -1;
	}	
	for(i=0; i < width; i++){
	    templine_green[i] = 0x80;
	}
	templine_blue = malloc(width);	
	if (!templine_blue) {
		free(templine_blue);
		return -1;
	}	
	for(i=0; i < width; i++){
	    templine_blue[i] = 0x80;
	}
	GP_DEBUG ("Running second_decompress.\n");
	for (m=0; m < height/2; m++) {
		/* First we do an even-numbered line */
		for (i=0; i< width/2; i++) {
			parity = i&1;
	    		delta_right = in[input_counter] &0x0f;
			delta_left = (in[input_counter]>>4)&0xff;
			input_counter ++;
			/* left pixel (red) */
			diff = delta_table[delta_left];
			if (!i) 
				tempval = templine_red[0] + diff;
			else 
				tempval = (templine_red[i]
				        + uncomp[2*m*width+2*i-2])/2 + diff;
			tempval = MIN(tempval, 0xff);
			tempval = MAX(tempval, 0);
			uncomp[2*m*width+2*i] = tempval;
			templine_red[i] = tempval;
			/* right pixel (green) */
			diff = delta_table[delta_right];
			if (!i) 
				tempval = templine_green[1] + diff;
			else if (2*i == width - 2 ) 
				tempval = (templine_green[i]
						+ uncomp[2*m*width+2*i-1])/2 
							+ diff;
			else
				tempval = (templine_green[i+1]
						+ uncomp[2*m*width+2*i-1])/2 
							+ diff;
			tempval = MIN(tempval, 0xff);
			tempval = MAX(tempval, 0);
			uncomp[2*m*width+2*i+1] = tempval;
			templine_green[i] = tempval;
		}
		/* then an odd-numbered line */
		for (i=0; i< width/2; i++) {
			delta_right = in[input_counter] &0x0f;
			delta_left = (in[input_counter]>>4)&0xff;
			input_counter ++;
			/* left pixel (green) */
			diff = delta_table[delta_left];
			if (!i) 
				tempval = templine_green[0] + diff;
			else 
				tempval = (templine_green[i]
				    	    + uncomp[(2*m+1)*width+2*i-2])/2 
						+ diff;
			tempval = MIN(tempval, 0xff);
			tempval = MAX(tempval, 0);
			uncomp[(2*m+1)*width+2*i] = tempval;
			templine_green[i] = tempval;
			/* right pixel (blue) */
			diff = delta_table[delta_right];
			if (!i) 
				tempval = templine_blue[0] + diff;
			else 
				tempval = (templine_blue[i]
					    + uncomp[(2*m+1)*width+2*i-1])/2 
						+ diff;
			tempval = MIN(tempval, 0xff);
			tempval = MAX(tempval, 0);
			uncomp[(2*m+1)*width+2*i+1] = tempval;
			templine_blue[i] = tempval;
		}
	}
	return 0;
}

int
digi_decompress (unsigned char *out_data, unsigned char *data,
	    int w, int h)
{
	int size, row, i;
	unsigned char temp;
	unsigned char *temp_data;
	size = w*h/2;
	temp_data = malloc(size);
	if (!temp_data) 
		return(GP_ERROR_NO_MEMORY);	
	digi_first_decompress (temp_data, data, size);
	GP_DEBUG("Stage one done\n");
	digi_second_decompress (out_data, temp_data, w, h);
	GP_DEBUG("Stage two done\n");
	free(temp_data);
	return(GP_OK);
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
digi_postprocess(CameraPrivateLibrary *priv, int width, int height, 
					unsigned char* rgb, int n)
{
	int
		x,y,
		red_min=255, red_max=0, 
		blue_min=255, blue_max=0, 
		green_min=255, green_max=0;
	double
		min, max, amplify; 

	/* determine min and max per color... */

	for( y=0; y<height; y++){
		for( x=0; x<width; x++ ){
			MINMAX( RED(rgb,x,y,width), red_min,   red_max  );
			MINMAX( GREEN(rgb,x,y,width), green_min, green_max);
			MINMAX( BLUE(rgb,x,y,width), blue_min,  blue_max );
		}
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
	amplify = 255.0/(max-min);

	for( y=0; y<height; y++){
		for( x=0; x<width; x++ ){
			RED(rgb,x,y,width)= MIN(amplify*(double)(RED(rgb,x,y,width)-min),255);
			GREEN(rgb,x,y,width)= MIN(amplify*(double)(GREEN(rgb,x,y,width)-min),255);
			BLUE(rgb,x,y,width)= MIN(amplify*(double)(BLUE(rgb,x,y,width)-min),255);
		}
	}

	return GP_OK;
}

