/*
 * Bases on STV0680 Vision Camera Chipset Driver
 * Copyright (C) 2000 Adam Harrison <adam@antispin.org> 
 * 
 * bayer_unshuffle() adapted to Jenoptik JD350e raw images
 * format by Michael Trawny <trawny99@yahoo.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA   
 */

#include "redcurve.h" // mtr: correction curve for red values
#include "bayer.h"

void bayer_unshuffle(int w, int h, unsigned char *raw, unsigned char *output)
{
	int x, y, i,j, p;
	int colour, bayer;

	for(i=0,j=w*h-1; i<j; i++,j--){ // mtr: rotate raw 180 degrees
		unsigned char t = raw[i];
		raw[i] = raw[j];
		raw[j] = t;
	}

	for(y = 0; y < h; ++y) {
		for(x = 0; x < w; ++x) {
			//p = x & 1 ? raw[(y*w) + (x >> 1)] : raw[(y*w) + (x>>1) + (w >> 1)];
			p = raw[(y*w) + x]; // mtr

			bayer = (y&1?2:0) + (x&1?1:0);

			switch(bayer) {
			case 0:
			case 3: colour = 1; break;
			case 1: colour = 0; break;
			case 2: colour = 2; break;
			}

			i = (y * w + x) * 3;

			output[i] = 0;
			output[i+1] = 0;
			output[i+2] = 0;
			output[i+colour] = p;
                        output[i] = red_curve[output[i]]; // mtr: apply color correction curve 
		}
	}
}

#define ISEVEN(a) (((a)&1)!=1)
#define GETADD(x, y) ((3*(x))+(356*3*(y)))
#define RED 0
#define GREEN 1
#define BLUE 2

#define AD(x, y, w) ((y)*(w)*3+3*(x))

void bayer_demosaic(int w, int h, unsigned char *image)
{
	int x, y, bayer;

	for(y = 1; y < (h - 2); y++) {
		for(x = 1; x < (w - 2); x++) {
			// work out pixel type
			bayer = (x&1?0:1) + (y&1?0:2);

	switch(bayer) {
	case 0: // green. blue lr, red tb
	image[AD(x,y,w) + BLUE] = image[AD(x-1,y,w)+BLUE];
	image[AD(x,y,w) + RED] = image[AD(x,y-1,w)+RED];
	break;
	case 1: // blue. green lrtb, red diagonals
	image[AD(x,y,w)+GREEN] =image[AD(x-1,y,w)+GREEN];
	image[AD(x,y,w)+RED] =image[AD(x-1,y-1,w)+RED];
	break;
	case 2: // red. green lrtb, blue diagonals
	image[AD(x,y,w)+GREEN] =image[AD(x-1,y,w)+GREEN];
	image[AD(x,y,w)+BLUE] =image[AD(x-1,y-1,w)+BLUE];
	break;
	case 3: // green. red lr, blue tb
	image[AD(x,y,w)+RED] =image[AD(x-1,y,w)+RED];
	image[AD(x,y,w)+BLUE] =image[AD(x,y-1,w)+BLUE];
	break;
	}
		}
	}
}

