/* jd350e.c
 *
 * White balancing and brightness correction for the Jenoptik
 * JD350 entrance camera
 *
 * Copyright 2001 Michael Trawny <trawny99@users.sourceforge.net>
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
#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>

#include "jd350e.h"
#include "jd350e_red.h"
/*#include "jd350e_blue.h"*/

#define GP_MODULE "jd350e"

#define THRESHOLD 0xf8

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


int jd350e_postprocessing(int width, int height, unsigned char* rgb){
	int
		x,y,
		red_min=255, red_max=0,
		blue_min=255, blue_max=0,
		green_min=255, green_max=0;
	double
		min, max, amplify;

	/* reverse image row by row... */

	for( y=0; y<height; y++){
		for( x=0; x<width/2; x++ ){
			SWAP( RED(rgb,x,y,width), RED(rgb,width-x-1,y,width));
			SWAP( GREEN(rgb,x,y,width), GREEN(rgb,width-x-1,y,width));
			SWAP( BLUE(rgb,x,y,width), BLUE(rgb,width-x-1,y,width));
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

	/* white balancing ...                               */
	/* here is still some work to do: either blue or red */
	/* pixles need to be emphasized.                     */
	/* but how can the driver decide?                    */

#if 0
	if( (green_max+blue_max)/2 > red_max ){
#endif
		/* outdoor daylight : red color correction curve*/
		GP_DEBUG( "daylight mode");
		for( y=0; y<height; y++){
			for( x=0; x<width; x++ ){
				RED(rgb,x,y,width) = jd350e_red_curve[ RED(rgb,x,y,width) ];
			/* RED(rgb,x,y,width) = MIN(2*(unsigned)RED(rgb,x,y,width),255); */
			}
		}
		red_min = jd350e_red_curve[ red_min ];
		red_max = jd350e_red_curve[ red_max ];
		/* red_min = MIN(2*(unsigned)red_min,255); */
		/* red_max = MIN(2*(unsigned)red_max,255); */
#if 0
	}
	else if( (green_max+red_max)/2 > blue_max ){
		/* indoor electric light */
		GP_DEBUG( "electric light mode");
		for( y=0; y<height; y++){
			for( x=0; x<width; x++ ){
				BLUE(rgb,x,y,width) = MIN(2*(unsigned)BLUE(rgb,x,y,width),255);
			}
		}
		blue_min = MIN(2*(unsigned)blue_min,255);
		blue_max = MIN(2*(unsigned)blue_max,255);
	}
#endif

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

int jd350e_postprocessing_and_flip(int width, int height, unsigned char* rgb){
	char *tmpline;
	int y, ret;

	ret = jd350e_postprocessing (width,height,rgb);
	if (ret < GP_OK)
		return ret;
	tmpline = malloc(width*3);
	if (!tmpline)
		return GP_ERROR_NO_MEMORY;
	for( y=0; y<height/2; y++){
		memcpy (tmpline, rgb+y*width*3, width*3);
		memcpy (rgb+y*width*3, rgb+(height-y-1)*width*3, width*3);
		memcpy (rgb+(height-y-1)*width*3, tmpline, width*3);
	}
	free(tmpline);
	return GP_OK;
}

int trust350fs_postprocessing(int width, int height, unsigned char* rgb) {
	int		i,x,y,min=255,max=0;
	double		amplify;
	unsigned char	*buf;
	const int	brightness_adjust = 16;

	/* flip horizontal */
#define RED(p,x,y,w) *((p)+3*((y)*(w)+(x))  )
#define GREEN(p,x,y,w) *((p)+3*((y)*(w)+(x))+1)
#define BLUE(p,x,y,w) *((p)+3*((y)*(w)+(x))+2)

#define SWAP(a,b) {unsigned char t=(a); (a)=(b); (b)=t;}

	for( y=0; y<height; y++){
		for( x=0; x<width/2; x++ ){
			SWAP( RED(rgb,x,y,width), RED(rgb,width-x-1,y,width));
			SWAP( GREEN(rgb,x,y,width), GREEN(rgb,width-x-1,y,width));
			SWAP( BLUE(rgb,x,y,width), BLUE(rgb,width-x-1,y,width));
		}
	}

	/* flip vertical */
	buf = malloc(width*3);
	if (!buf) return GP_ERROR_NO_MEMORY;
	for (i=0;i<height/2;i++) {
		memcpy(buf,rgb+i*width*3,width*3);
		memcpy(rgb+i*width*3,rgb+(height-i-1)*width*3,width*3);
		memcpy(rgb+(height-i-1)*width*3,buf,width*3);
	}
	free(buf);

	/* Normalize & adjust brightness ... */
#define MINMAX(a,min,max) { (min)=MIN(min,a); (max)=MAX(max,a); }

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

	for(i=0; i<(width*height*3); i++)
			MINMAX( rgb[i], min, max  );

	amplify = 255.0/(max-min);

	for(i=0; i<(width*height*3); i++)
	{
		int val = amplify * (rgb[i] - min);

		if(val < brightness_adjust)
		   rgb[i] = val * 2;
		else if (val > (255 - brightness_adjust))
		   rgb[i] = 255;
		else
		   rgb[i] = val + brightness_adjust;
	}

	return GP_OK;
}
