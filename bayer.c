/* bayer.c
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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
#include "bayer.h"

#include <gphoto2-result.h>

static int tile_colors[8][4] = {
	{0, 1, 1, 2},
	{1, 0, 2, 1},
	{2, 1, 1, 0},
	{1, 2, 0, 1},
	{0, 1, 1, 2},
	{1, 0, 2, 1},
	{2, 1, 1, 0},
	{1, 2, 0, 1}};

#define RED 0
#define GREEN 1
#define BLUE 2

int
gp_bayer_expand (unsigned char *input, int w, int h, unsigned char *output,
		 BayerTile tile)
{
	int x, y, i;
	int colour, bayer;
	char *ptr = input;

	switch (tile) {

		case BAYER_TILE_RGGB:
		case BAYER_TILE_GRBG:
		case BAYER_TILE_BGGR:
		case BAYER_TILE_GBRG: 

			for (y = 0; y < h; ++y)
				for (x = 0; x < w; ++x, ++ptr) {
					bayer = (x&1?0:1) + (y&1?0:2);

					colour = tile_colors[tile][bayer];

					i = (y * w + x) * 3;

					output[i+RED]    = 0;
					output[i+GREEN]  = 0;
					output[i+BLUE]   = 0;
					output[i+colour] = *ptr;
				}
			break;

		case BAYER_TILE_RGGB_INTERLACED:
		case BAYER_TILE_GRBG_INTERLACED:
		case BAYER_TILE_BGGR_INTERLACED:
		case BAYER_TILE_GBRG_INTERLACED:
 

			for (y = 0; y < h; ++y, ptr+=w)
				for (x = 0; x < w; ++x) {
					bayer = (x&1?0:1) + (y&1?0:2);
	
					colour = tile_colors[tile][bayer];
	
					i = (y * w + x) * 3;

					output[i+RED]    = 0;
					output[i+GREEN]  = 0;
					output[i+BLUE]   = 0;
					output[i+colour] = (x&1)? ptr[x>>1]:ptr[(w>>1)+(x>>1)];
				}
			break;
	}

	return (GP_OK);
}

#define AD(x, y, w) ((y)*(w)*3+3*(x))

int
gp_bayer_interpolate (unsigned char *image, int w, int h, BayerTile tile)
{
	int x, y, bayer;
	int p0, p1, p2, p3;
	int div, value;

	switch (tile) {
	default:
	case BAYER_TILE_RGGB:
	case BAYER_TILE_RGGB_INTERLACED:
		p0 = 0; p1 = 1; p2 = 2; p3 = 3;
		break;
	case BAYER_TILE_GRBG:
	case BAYER_TILE_GRBG_INTERLACED:
		p0 = 1; p1 = 0; p2 = 3; p3 = 2;
		break;
	case BAYER_TILE_BGGR:
	case BAYER_TILE_BGGR_INTERLACED:
		p0 = 3; p1 = 2; p2 = 1; p3 = 0;
		break;
	case BAYER_TILE_GBRG:
	case BAYER_TILE_GBRG_INTERLACED:	
		p0 = 2; p1 = 3; p2 = 0; p3 = 1;
		break;
	}

	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			bayer = (x&1?0:1) + (y&1?0:2);
			if ( bayer == p0 ) {
				
				/* red. green lrtb, blue diagonals */
				div = value = 0;
				if (y) {
					value += image[AD(x,y-1,w)+GREEN];
					div++;
				}
				if (y < (h - 1)) {
					value += image[AD(x,y+1,w)+GREEN];
					div++;
				}
				if (x) {
					value += image[AD(x-1,y,w)+GREEN];
					div++;
				}
				if (x < (w - 1)) {
					value += image[AD(x+1,y,w)+GREEN];
					div++;
				}
				image[AD(x,y,w)+GREEN] = value / div;

				div = value = 0;
				if ((y < (h - 1)) && (x < (w - 1))) {
					value += image[AD(x+1,y+1,w)+BLUE];
					div++;
				}
				if ((y) && (x)) {
					value += image[AD(x-1,y-1,w)+BLUE];
					div++;
				}
				if ((y < (h - 1)) && (x)) {
					value += image[AD(x-1,y+1,w)+BLUE];
					div++;
				}
				if ((y) && (x < (w - 1))) {
					value += image[AD(x+1,y-1,w)+BLUE];
					div++;
				}
				image[AD(x,y,w)+BLUE] = value / div;

			} else if (bayer == p1) {
				
				/* green. red lr, blue tb */
				div = value = 0;
				if (x < (w - 1)) {
					value += image[AD(x+1,y,w)+RED];
					div++;
				}
				if (x) {
					value += image[AD(x-1,y,w)+RED];
					div++;
				}
				image[AD(x,y,w)+RED] = value / div;

				div = value = 0;
				if (y < (h - 1)) {
					value += image[AD(x,y+1,w)+BLUE];
					div++;
				}
				if (y) {
					value += image[AD(x,y-1,w)+BLUE];
					div++;
				}
				image[AD(x,y,w)+BLUE] = value / div;

			} else if ( bayer == p2 ) {
				
				/* green. blue lr, red tb */
				div = value = 0;
				
				if (x < (w - 1)) {
					value += image[AD(x+1,y,w)+BLUE];
					div++;
				}
				if (x) {
					value += image[AD(x-1,y,w)+BLUE];
					div++;
				}
				image[AD(x,y,w)+BLUE] = value / div;

				div = value = 0;
				if (y < (h - 1)) {
					value += image[AD(x,y+1,w)+RED];
					div++;
				}
				if (y) {
					value += image[AD(x,y-1,w)+RED];
					div++;
				}
				image[AD(x,y,w)+RED] = value / div;

			} else {
				
				/* blue. green lrtb, red diagonals */
				div = value = 0;

				if (y) {
					value += image[AD(x,y-1,w)+GREEN];
					div++;
				}
				if (y < (h - 1)) {
					value += image[AD(x,y+1,w)+GREEN];
					div++;
				}
				if (x) {
					value += image[AD(x-1,y,w)+GREEN];
					div++;
				}
				if (x < (w - 1)) {
					value += image[AD(x+1,y,w)+GREEN];
					div++;
				}
				image[AD(x,y,w)+GREEN] = value / div;

				div = value = 0;
				if ((y < (h - 1)) && (x < (w - 1))) {
					value += image[AD(x+1,y+1,w)+RED];
					div++;
				}
				if ((y) && (x)) {
					value += image[AD(x-1,y-1,w)+RED];
					div++;
				}
				if ((y < (h - 1)) && (x)) {
					value += image[AD(x-1,y+1,w)+RED];
					div++;
				}
				if ((y) && (x < (w - 1))) {
					value += image[AD(x+1,y-1,w)+RED];
					div++;
				}
				image[AD(x,y,w)+RED] = value / div;
			}
		}

	return (GP_OK);
}

int
gp_bayer_decode (unsigned char *input, int w, int h, unsigned char *output,
		 BayerTile tile)
{
	gp_bayer_expand (input, w, h, output, tile);
	gp_bayer_interpolate (output, w, h, tile);

	return (GP_OK);
}


