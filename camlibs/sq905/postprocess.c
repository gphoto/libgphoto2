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

#include <gphoto2.h>
#include <gphoto2-port.h>
#include "sq905.h"

#define GP_MODULE "sq905" 



int
sq_decompress (unsigned char *output, unsigned char *data,
	    int w, int h, int n)
{
	/*
	 * Data arranged in planar form. The format seems 
	 * to be some kind of differential encoding of four-bit data.
	 * The top row of each colorplane seems to represent unmodified 
	 * data, used to initialize the subsequent encoding, which proceeds 
	 * down columns. Here, we try to "decompress" the raw data first and
	 * then to do Bayer interpolation afterwards. The byte-reversal 
	 * routine having been done already, the planes are in the order 
	 * RBG. The R and B planes are of size b/4, and the G of size b/2. 
	 */

	unsigned char *red, *green, *blue;
	unsigned char *mark_redblue; /* Even stores red; odd stores blue */ 
	unsigned char *mark_green;
	unsigned char mark = 0, datum = 0; 
	int i, j, m;


		/* First we spread out the data to size w*h. */
		for ( i=1; i <= w*h/2 ; i++ ) data[2*(w*h/2 -i)] 
						    = data[w*h/2 - i];
		/* Then split the bytes ("backwards" because we 
		 * reversed the data) into the first digits of 2 bytes.*/
		for ( i=0; i < w*h/2 ; i++ ) {
			data[2*i + 1] = 16*(data[2*i]/16);
			data[2*i]     = 16*(data[2*i]%16);
		}

	/* Now, having done this, we have, in order, a red plane of dimension 
	 * w/2 * h/2, followed by a blue plane the same size, and then a green
	 * plane of dimension w/2 * h. We need to separate them to work on, 
	 * then at the end put them together. 
	 */  

	red = malloc(w*h/4);
	if (!red) return GP_ERROR_NO_MEMORY;
	memcpy (red,data, w*h/4);

	blue = malloc(w*h/4);
	if (!blue) return GP_ERROR_NO_MEMORY;
	memcpy (blue,data+w*h/4, w*h/4);


	green = malloc(w*h/2);
	if (!green) return GP_ERROR_NO_MEMORY;
	memcpy (green,data + w*h/2, w*h/2);

	memset(data, 0x0, w*h);

	mark_redblue  = malloc(w);
	if (!(mark_redblue)) return GP_ERROR_NO_MEMORY;
	memset (mark_redblue,0x0,w);

	mark_green= malloc(w);
	if (!(mark_green)) return GP_ERROR_NO_MEMORY;
	memset (mark_green,0x0,w);

	/* Unscrambling the respective colorplanes. Then putting them 
	 * back together. 
	 */

	for (m = 0; m < h/2; m++) {

		for (j = 0; j < 2; j++) {

			for (i = 0; i < w/2 ; i++) {
				/*		
				 * First the greens on the even lines at 
				 * indices 2*i+1, when j=0. Then the greens
				 * on the odd lines at indices 2*i, when j=1. 
				 */ 
			
				datum = green[(2*m+j)*w/2 + i];
				
				if (!m && !j) {

					mark_green[2*i] =
					data[2*i+1] =  
					MIN(MAX(datum, 0x20), 0xe0);    


				} else {
					mark= mark_green[2*i + 1-j];
			
					if (datum >= 0x80) {
						mark_green[2*i +j] =
						data[(2*m+j)*w + 2*i +1 - j] =
						MIN(2*(mark + datum - 0x80) 
						- (mark*datum)/128., 0xe0);
					} else {
						mark_green[2*i +j] =
						data[(2*m+j)*w + 2*i +1 - j] =
		    				MAX((mark*datum)/128, 0x20);
					}
				}

				mark_green[2*i +j] =
				data[(2*m+j)*w + 2*i +1 - j] =
		    		MIN( 256 *
				pow((float)mark_green[2*i + j]/256., .95),
				0xe0);

				/*
        			 * Here begin the reds and blues. 
				 * Reds in even slots on even-numbered lines.
				 * Blues in odd slots on odd-numbered lines. 
				 */
				
				if (j)	datum =  blue[m*w/2 + i ] ;
				else	datum =  red[m*w/2 + i ] ;

				if(!m) {
					mark_redblue[2*i+j]= 
					data[j*w +2*i+j]=  
					MIN(MAX(datum, 0x20), 0xe0);
				} else {
					mark = mark_redblue[2*i + j];
					if (datum >= 0x80) {
						mark_redblue[2*i +j] =
						data[(2*m+j)*w + 2*i + j] =
						MIN(2*(mark + datum - 0x80) 
						- (mark*datum)
						/128., 0xe0);
					    
					} else {
					
						mark_redblue[2*i +j] =
						data[(2*m+j)*w + 2*i + j] =
						MAX((mark*datum)/128,0x20);
					}

				}

				mark_redblue[2*i + j] =
				data[(2*m+j)*w + 2*i + j] =
		    		MIN( 256 *
				pow((float)mark_redblue[2*i + j]/256., .95),
				0xe0);



			}					

			/* Averaging of data inputs */

			for (i = 1; i < w/2-1; i++ ) {

				mark_redblue[2*i + j] =
				data[(2*m+j)*w + 2*i + j] =
				(data[(2*m+j)*w + 2*i + j] +
				data[(2*m+j)*w + 2*i -2 + j])/2;
			
				if (j) {
					mark_green[2*i + j] =
					data[(2*m+j)*w + 2*i +1 - j] =
					(data[(2*m+j)*w + 2*i +1 - j] +
					data[(2*m+j)*w + 2*i +3 - j])/2;
				} else if(m) {
					mark_green[2*i + j] =
					data[(2*m+j)*w + 2*i +1 - j] =
					(data[(2*m+j)*w + 2*i +1 - j] +
					data[(2*m+j)*w + 2*i -1 - j])/2;
				}
			}
			mark_green[j] =
			data[(2*m+j)*w +1-j] =
			(data[(2*m+j)*w +1-j] +
			data[(2*m+j)*w +3-j])/2;

			mark_redblue[w -2 + j] =
			data[(2*m+j)*w + w- 2 + j] =
			(data[(2*m+j)*w + w-2 + j] +
			data[(2*m+j)*w + w- 2  -2 + j])/2;

			mark_redblue[ j] =
			data[(2*m+j)*w + j] =
			(data[(2*m+j)*w + j] +
			data[(2*m+j)*w + 2 + j])/2;



		}
	}
	free (green);
	free (red);
	free (blue);

	/* Some horizontal Bayer interpolation. */

	for (m = 0; m < h/2; m++) {
		for (j = 0; j < 2; j++) {
			for (i = 0; i < w/2; i++) {

				/* the known greens */
				output[3*((2*m+j)*w + 2*i +1 - j) +1] =	
				data[(2*m+j)*w + 2*i +1 - j];
				/* known reds and known blues */
				output[3*((2*m+j)*w + 2*i + j) + 2*j] =
				data[(2*m+j)*w + 2*i + j];

			}
			/*
			 * the interpolated greens (at the even pixels on 
			 * even lines and odd pixels on odd lines)
			 */
			if (!j) {
			output[3*(2*m*w) +1] =	
			data[2*m*w + 1 ];
			output[3*(2*m*w + w - 1) +1] =	
			data[2*m*w + w - 2 ];
			}
			
			for (i= 1; i < w/2 - 1; i++)		
				output[3*((2*m+j)*w + 2*i - j) + 1] =
				(output[3*((2*m+j)*w + 2*i -1 - j) + 1] +
				output[3*((2*m+j)*w + 2*i +1 - j) + 1])/2;					
			/*		
			 * the interpolated reds on even (red-green) lines and
			 * the interpolated blues on odd (green-blue) lines
			 */
			output[3*((2*m+j)*w ) +2*j] =	
			data[(2*m+j)*w + j];
			output[3*((2*m+j)*w + w - 1) +2*j] =	
			data[(2*m+j)*w + w - 1 - 1 + j];

			for (i= 1; i < w/2 -1; i++)		
	    			output[3*((2*m+j)*w + 2*i +1 -j ) +2*j] =	
				(output[3*((2*m+j)*w + 2*i - j) + 2*j] +
				output[3*((2*m+j)*w + 2*i +2 - j) + 2*j])/2;					
		}
	}
	/*
	 * finally the missing blues, on even-numbered lines
	 * and reds on odd-numbered lines.
	 * we just interpolate diagonally for both
	 */
	for (m = 0; m < h/2; m++) {

		if ((m) && (h/2 - 1 - m))
		for (i= 0; i < w; i++)	{	
		
			output[3*((2*m)*w + i) +2] =	
			(output[3*((2*m-1)*w + i-1) + 2] +
			output[3*((2*m+1)*w +i+1 ) + 2])/2;
		
			output[3*((2*m+1)*w + i) +0] =	
			(output[3*(2*m*w + i-1) + 0] +
			output[3*((2*m+2)*w +i+1) + 0])/2;
		}
	}		
	
	for (i= 0; i < w; i++) {	
		output[3*i +2] = output[3*(w + i) + 2];
		output[3*(w+i)] = output[3*i];
	}
	for (i= 0; i < w; i++)		
		output[3*((h-1)*w + i) +0] = output[3*((h -2)*w +i) + 0];					


	/* Diagonal smoothing */

	for (m = 1; m < h - 1; m++) {
		
		for (i= 0; i < w; i++)	{	
		
				output[3*(m*w + i) +0] =	
				(output[3*((m-1)*w + i-1) + 0] +
				2*output[3*(m*w + i) +0] +
				output[3*((m+1)*w +i+1 ) + 0])/4;

				output[3*(m*w + i) +1] =	
				(output[3*((m-1)*w + i-1) + 1] +
				2*output[3*(m*w + i) +1] +
				output[3*((m+1)*w +i+1 ) + 1])/4;

				output[3*(m*w + i) +2] =	
				(output[3*((m-1)*w + i-1) + 2] +
				2*output[3*(m*w + i) + 2] +
				output[3*((m+1)*w +i+1 ) + 2])/4;
		

			}
	}		
	



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
sq_postprocess(CameraPrivateLibrary *priv, int width, int height, 
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

