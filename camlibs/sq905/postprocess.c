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
sq_decompress (unsigned char *data, int b, int w, int h)
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
	unsigned char mark, datum = 0; 


	int i, j, m;
		/* First we spread out the data to size w*h. */
		for ( i=1; i <= b ; i++ ) data[2*(b-i)] = data[b-i];
		/* Then split the bytes ("backwards" because we 
		 * reversed the data) into the first digits of 2 bytes.*/
		for ( i=0; i < b ; i++ ) {
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

	memset(data, 0, w*h);

	mark_redblue  = malloc(w);
	if (!(mark_redblue)) return GP_ERROR_NO_MEMORY;
	memset (mark_redblue,0x80,w);

	mark_green= malloc(w);
	if (!(mark_green)) return GP_ERROR_NO_MEMORY;
	memset (mark_green,0x80,w);
	


	/* Unscrambling the respective colorplanes. This still does not work
	 * very well. Since we still do not really know what is going on, 
	 * we make this short and sweet. 
	 */

	for (i = 0; i < w/2 ; i++) {

		for (m = 0; m < h/2; m++) {
			for (j = 0; j < 2; j++) {
				/* First the greens on the even lines at 
				 * indices 2*i+1, when j=0. Then the greens
				 * on the odd lines at indices 2*i, when j=1. 
				 */ 
	
		    
				mark   = mark_green[2*i +1 -j];
				datum  = green[(2*m+j)*w/2+ i]; 

			

				data[(2*m +j)*w + 2*i +1 - j] 
				= mark + (int)(datum - 0x80);		
		
				if ( 
				(mark + datum < 0x80) 
				|| (mark + datum > 0x170) 

				)


					mark_green[2*i+1 - j] = 
					mark_green[2*i +j] = /*sets up next line*/
					data[(2*m +j)*w + 2*i +1 - j] =
					datum;

        			/* Here begin the reds and blues. 
				 * Reds in even slots on even-numbered lines.
				 * Blues in odd slots on odd-numbered lines. 
				 */
		
			
				mark = mark_redblue[2*i + j];
				if (j) {			
				datum =  blue[m*w/2 + i ] ;
				} else { 
				datum =  red[m*w/2 + i ] ;
				}
				data[(2*m+j)*w + 2*i +j] 

				= mark + datum - 0x80;
		
				if ( !(mark + datum - 80)


				) 
				

					data[(2*m+j)*w + 2*i +j] = 
					mark_redblue[2*i+j] = datum;
				

			/* Finished the reds and the blues */
			}


		}

	}
	free (red);
	free (blue);
	free (green);
	return(GP_OK);
}

/* White balancing and brightness correction routine adapted from 
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
		min, max, amplify, temp;

	/* determine min and max per color... */

/*	for( y=0; y<height; y++){
		for( x=0; x<width; x++ ){
			MINMAX( RED(rgb,x,y,width), red_min,   red_max  );
			MINMAX( GREEN(rgb,x,y,width), green_min, green_max);
			MINMAX( BLUE(rgb,x,y,width), blue_min,  blue_max );
		}
	}
*/
	/* white balancing ...                               */

	if( (priv->catalog[16*n+9] >= priv->catalog[16*n+10])  ){ 
/*	if( ((green_max+blue_max)/2 > red_max)  ){  */

		/* outdoor daylight : red color correction curve*/
		GP_DEBUG( "daylight mode");
		for( y=0; y<height; y++){
			for( x=0; x<width; x++ ){

				temp = (float)RED(rgb,x,y,width);
				RED(rgb,x,y,width) 
				= (256)*(3*(temp/256.)/2. 
				- pow(temp/256., 2)/2.);


				temp = (float)GREEN(rgb,x,y,width);
				GREEN(rgb,x,y,width) 
				= (256)*(3*(temp/256.)/2. 
				- pow(temp/256., 2)/2.);


/*			 RED(rgb,x,y,width) = 
				MIN(2*(unsigned)RED(rgb,x,y,width),255); */
			}
		}
/*
		 red_min = MIN(2*(unsigned)red_min,255); 
		 red_max = MIN(2*(unsigned)red_max,255); 
*/
	}

/*
	else { 
*/
		/* indoor electric light */
/*		GP_DEBUG( "electric light mode");
		for( y=0; y<height; y++){
			for( x=0; x<width; x++ ){

				temp = (float)BLUE(rgb,x,y,width);
				BLUE(rgb,x,y,width) 
				= (256)*(3*(temp/256.)/2. 
				- pow(temp/256., 1.8)/2.);



				temp = (float)GREEN(rgb,x,y,width);
				GREEN(rgb,x,y,width) 
				= (256)*(3*(temp/256.)/2. 
				- pow(temp/256., 2)/2.);



			}
		}
/*
		blue_min = MIN(2*(unsigned)blue_min,255);
		blue_max = MIN(2*(unsigned)blue_max,255);

	}


*/

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

