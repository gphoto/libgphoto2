/** \file bayer.c
 * \brief Bayer array conversion routines.
 *
 * \author Copyright 2001 Lutz Mueller <lutz@users.sf.net>
 * \author Copyright 2007 Theodore Kilgore <kilgota@auburn.edu>
 *
 * \par
 * gp_bayer_accrue() from Theodore Kilgore <kilgota@auburn.edu>
 * contains suggestions by B. R. Harris (e-mail address disappeared) and
 * Werner Eugster <eugster@giub.unibe.ch>
 *
 * \par License
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \par
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "bayer.h"

#include <gphoto2/gphoto2-result.h>

static const int tile_colours[8][4] = {
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

static int
gp_bayer_accrue (unsigned char *image, int w, int h, int x0, int y0,
		int x1, int y1, int x2, int y2, int x3, int y3, int colour);

/**
 * \brief Expand a bayer raster style image to a RGB raster.
 *
 * \param input the bayer CCD array as linear input
 * \param w width of the above array
 * \param h height of the above array
 * \param output RGB output array (linear, 3 bytes of R,G,B for every pixel)
 * \param tile how the 2x2 bayer array is layed out
 *
 * A regular CCD uses a raster of 2 green, 1 blue and 1 red components to
 * cover a 2x2 pixel area. The camera or the driver then interpolates a
 * 2x2 RGB pixel set out of this data.
 *
 * This function expands the bayer array to 3 times larger bitmap with
 * RGB values copied as-is. Pixels were no sensor was there are 0.
 * The data is supposed to be processed further by for instance gp_bayer_interpolate().
 * 
 * \return a gphoto error code
 */
int
gp_bayer_expand (unsigned char *input, int w, int h, unsigned char *output,
		 BayerTile tile)
{
	int x, y, i;
	int colour, bayer;
	unsigned char *ptr = input;

	switch (tile) {

		case BAYER_TILE_RGGB:
		case BAYER_TILE_GRBG:
		case BAYER_TILE_BGGR:
		case BAYER_TILE_GBRG:

			for (y = 0; y < h; ++y)
				for (x = 0; x < w; ++x, ++ptr) {
					bayer = (x&1?0:1) + (y&1?0:2);

					colour = tile_colours[tile][bayer];

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

					colour = tile_colours[tile][bayer];
	
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

/**
 * \brief Interpolate a expanded bayer array into an RGB image.
 *
 * \param image the linear RGB array as both input and output
 * \param w width of the above array
 * \param h height of the above array
 * \param tile how the 2x2 bayer array is layed out
 *
 * This function interpolates a bayer array which has been pre-expanded
 * by gp_bayer_expand() to an RGB image. It uses various interpolation
 * methods, also see gp_bayer_accrue().
 *
 * \return a gphoto error code
 */
int
gp_bayer_interpolate (unsigned char *image, int w, int h, BayerTile tile)
{
	int x, y, bayer;
	int p0, p1, p2;
	int value, div ;

	switch (tile) {
	default:
	case BAYER_TILE_RGGB:
	case BAYER_TILE_RGGB_INTERLACED:
		p0 = 0; p1 = 1; p2 = 2;
		break;
	case BAYER_TILE_GRBG:
	case BAYER_TILE_GRBG_INTERLACED:
		p0 = 1; p1 = 0; p2 = 3;
		break;
	case BAYER_TILE_BGGR:
	case BAYER_TILE_BGGR_INTERLACED:
		p0 = 3; p1 = 2; p2 = 1;
		break;
	case BAYER_TILE_GBRG:
	case BAYER_TILE_GBRG_INTERLACED:
		p0 = 2; p1 = 3; p2 = 0;
		break;
	}

	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			bayer = (x&1?0:1) + (y&1?0:2);

			if ( bayer == p0 ) {

				/* red. green lrtb, blue diagonals */
				image[AD(x,y,w)+GREEN] =
					gp_bayer_accrue(image, w, h, x-1, y, x+1, y, x, y-1, x, y+1, GREEN) ;

				image[AD(x,y,w)+BLUE] =
					gp_bayer_accrue(image, w, h, x+1, y+1, x-1, y-1, x-1, y+1, x+1, y-1, BLUE) ;

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
				image[AD(x,y,w)+GREEN] =
					gp_bayer_accrue (image, w, h, x-1, y, x+1, y, x, y-1, x, y+1, GREEN) ;

				image[AD(x,y,w)+RED] =
					gp_bayer_accrue (image, w, h, x+1, y+1, x-1, y-1, x-1, y+1, x+1, y-1, RED) ;
			}
		}

	return (GP_OK);
}
/**
 * \brief interpolate one pixel from a bayer 2x2 raster
 * 
 * For red and blue data, compare the four surrounding values.  If three
 * values are all one side of the mean value, the fourth value is ignored.
 * This will sharpen boundaries. Treatment of green data looks for vertical and
 * horizontal edges. Any such which are discovered get special treatment.
 * Otherwise, the same comparison test is applied which is applied for red
 * and blue. Standard algorithm is applied without change at edges of the image.
 */
static int
gp_bayer_accrue (unsigned char *image, int w, int h, int x0, int y0,
		int x1, int y1, int x2, int y2, int x3, int y3, int colour)
{	int x [4] ;
	int y [4] ;
	int value [4] ;
	int above [4] ;
	int counter   ;
	int sum_of_values;
	int average ;
	int i ;
	x[0] = x0 ; x[1] = x1 ; x[2] = x2 ; x[3] = x3 ;
	y[0] = y0 ; y[1] = y1 ; y[2] = y2 ; y[3] = y3 ;
	
	/* special treatment for green */
	counter = sum_of_values = 0 ;
	if(colour == GREEN)
	{
	  	/* We need to make sure that horizontal or vertical lines
		 * become horizontal and vertical lines even in this
		 * interpolation procedure. Therefore, we determine whether
		 * we might have such a line structure.
		 */
	  	
		for (i = 0 ; i < 4 ; i++)
	  	{	if ((x[i] >= 0) && (x[i] < w) && (y[i] >= 0) && (y[i] < h))
			{
				value [i] = image[AD(x[i],y[i],w) + colour] ;
				counter++;
			}
			else
			{
				value [i] = -1 ;
			}
	  	}
		if(counter == 4)
		{	
			/* It is assumed that x0,y0 and x1,y1 are on a
			 * horizontal line and
			 * x2,y2 and x3,y3 are on a vertical line
			 */
			int hdiff ;
			int vdiff ;
			hdiff = value [1] - value [0] ;
			hdiff *= hdiff ;	/* Make value positive by squaring */
			vdiff = value [3] - value [2] ;
			vdiff *= vdiff ;	/* Make value positive by squaring */
			if(hdiff > 2*vdiff)
			{
				/* We might have a vertical structure here */
				return (value [3] + value [2])/2 ;
			}
			if(vdiff > 2*hdiff)
			{
				/* we might have a horizontal structure here */
				return (value [1] + value [0])/2 ;
			}
			/* else we proceed as with blue and red */
		}
		/* if we do not have four points then we proceed as we do for
		 * blue and red */
	}
	
	/* for blue and red */
	counter = sum_of_values = 0 ;
	for (i = 0 ; i < 4 ; i++)
	{	if ((x[i] >= 0) && (x[i] < w) && (y[i] >= 0) && (y[i] < h))
		{	value [i] = image[AD(x[i],y[i],w) + colour] ;
			sum_of_values += value [i] ;
			counter++ ;
		}
	}
	average = sum_of_values / counter ;
	if (counter < 4) return average ;
	/* Less than four surrounding - just take average */
	counter = 0 ;
	for (i = 0 ; i < 4 ; i++)
	{	above[i] = value[i] > average ;
		if (above[i]) counter++ ;
	}
	/* Note: counter == 0 indicates all values the same */
	if ((counter == 2) || (counter == 0)) return average ;
	sum_of_values = 0 ;
	for (i = 0 ; i < 4 ; i++)
	{	if ((counter == 3) == above[i])
		{	sum_of_values += value[i] ; }
	}
	return sum_of_values / 3 ;
}

/**
 * \brief Convert a bayer raster style image to a RGB raster.
 *
 * \param input the bayer CCD array as linear input
 * \param w width of the above array
 * \param h height of the above array
 * \param output RGB output array (linear, 3 bytes of R,G,B for every pixel)
 * \param tile how the 2x2 bayer array is layed out
 *
 * A regular CCD uses a raster of 2 green, 1 blue and 1 red components to
 * cover a 2x2 pixel area. The camera or the driver then interpolates a
 * 2x2 RGB pixel set out of this data.
 *
 * This function expands and interpolates the bayer array to 3 times larger
 * bitmap with RGB values interpolated.
 * 
 * \return a gphoto error code
 */
int
gp_bayer_decode (unsigned char *input, int w, int h, unsigned char *output,
		 BayerTile tile)
{
	gp_bayer_expand (input, w, h, output, tile);
	gp_bayer_interpolate (output, w, h, tile);

	return (GP_OK);
}
