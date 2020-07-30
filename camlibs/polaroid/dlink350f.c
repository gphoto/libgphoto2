/* dlink350f.c
 *
 * orientation, byte order, and brightness correction for the D-Link 350F
 *
 * Copyright 2003 Mark Slemko <slemkom@users.sourceforge.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>

#define GP_MODULE "dlink350f"

#include "dlink350f.h"

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

/*
 * function added for the D-Link DSC 350F - written by Mark Slemko - mslemko@netscape.net
 * This function correctly adjusts the color and orientation of the image
 */
int dlink_dsc350f_postprocessing_and_flip_both (int width, int height, unsigned char* rgb) {
	unsigned char *start, *end, c;

	int whichcolor = 0;
	int lowred=255, lowgreen=255, lowblue=255;
	int hired=0, higreen=0, hiblue=0;

	GP_DEBUG("flipping byte order");

	/* flip image left/right and top/bottom (actually reverse byte order) */
	start = rgb;
	end = start + ((width * height) * 3);

	while (start < end) {
		c = *start;

		/* validation - debugging info - collect the color range info
		 * for first half of image.
		 */
		switch (whichcolor % 3) {
			case 0: /* blue */
				MINMAX((int)c,lowblue,hiblue);
				break;
			case 1: /* green */
				MINMAX((int)c,lowgreen,higreen);
				break;
			default: /* red */
				MINMAX((int)c,lowred,hired);
				break;
		}

		/* adjust color magnitude, since it appears that the 350f only had 7 bits of color info */
		*start++ = *--end << 1;
		*end = c << 1;

		whichcolor++;
	}

	/* could do more color processing here
	GP_DEBUG("adjusting color");

	// adjust image colours
	start = rgb;
	end = start + ((width * height) * 3);

	while (start < end) {
		c = *start++;
	}
	*/

	/* show the color range of image in debug mode. */
	GP_DEBUG("\nred low = %d high = %d\ngreen low = %d high = %d\nblue low = %d high = %d\n", lowred,hired, lowgreen,higreen, lowblue,hiblue);
	return GP_OK;
}
