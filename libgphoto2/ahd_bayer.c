/** \file ahd_bayer.c
 *  
 * \brief Adaptive Homogeneity-Directed Bayer array conversion routine.
 *
 * \author Copyright March 12, 2008 Theodore Kilgore <kilgota@auburn.edu>
 *
 * \par
 * gp_ahd_interpolate() from Eero Salminen <esalmine@gmail.com>
 * and Theodore Kilgore. The work of Eero Salminen is for partial completion 
 * of a Diploma in Information and Computer Science, 
 * Helsinki University of Technology, Finland.
 *
 * \par
 * The algorithm is based upon the paper
 *
 * \par
 * Adaptive Homogeneity-Directed Democsaicing Algoritm, 
 * Keigo Hirakawa and Thomas W. Parks, presented in the 
 * IEEE Transactions on Image Processing, vol. 14, no. 3, March 2005. 
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
 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "config.h"
#include "bayer.h"
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>

#define MAX(x,y) ((x < y) ? (y) : (x))
#define MIN(x,y) ((x > y) ? (y) : (x))
#define CLAMP(x) MAX(MIN(x,0xff),0)
#define RED	0
#define GREEN 	1
#define BLUE 	2

static
int dRGB(int i1, int i2, unsigned char *RGB);
static
int do_rb_ctr_row(unsigned char *image_h, unsigned char *image_v, int w, 
					int h, int y, int *pos_code);
static
int do_green_ctr_row(unsigned char *image, unsigned char *image_h, 
		    unsigned char *image_v, int w, int h, int y, int *pos_code);
static
int get_diffs_row2(unsigned char * hom_buffer_h, unsigned char *hom_buffer_v, 
		    unsigned char * buffer_h, unsigned char *buffer_v, int w);

#define AD(x, y, w) ((y)*(w)*3+3*(x))
/**
 * \brief This function computes distance^2 between two sets of pixel data. 
 * \param i1 location of a pixel
 * \param i2 location of another pixel
 * \param RGB some RGB data. 
 */
static
int dRGB(int i1, int i2, unsigned char *RGB) {
	int dR,dG,dB;
	dR=RGB[i1+RED]-RGB[i2+RED];
	dG=RGB[i1+GREEN]-RGB[i2+GREEN];
	dB=RGB[i1+BLUE]-RGB[i2+BLUE];
	return dR*dR+dG*dG+dB*dB;
}
/**
 * \brief Missing reds and/or blues are reconstructed on a single row
 * \param image_h three-row window, horizontal interpolation of row 1 is done
 * \param image_v three-row window, vertical interpolation of row 1 is done
 * \param w width of image
 * \param h height of image. 
 * \param y row number from image which is under construction
 * \param pos_code position code related to Bayer tiling in use
 */
static
int do_rb_ctr_row(unsigned char *image_h, unsigned char *image_v, int w, 
					int h, int y, int *pos_code) 
{
	int x, bayer;
	int value,value2,div,color;
	/*
	 * pos_code[0] = red. green lrtb, blue diagonals 
	 * pos_code[1] = green. red lr, blue tb 
	 * pos_code[2] = green. blue lr, red tb 
	 * pos_code[3] = blue. green lrtb, red diagonals 
	 *
	 * The Red channel reconstruction is R=G+L(Rs-Gs), in which
	 *	G = interpolated & known Green
	 *	Rs = known Red
	 *	Gs = values of G at the positions of Rs
	 *	L()= should be a 2D lowpass filter, now we'll check 
	 *	them from a 3x3 square
	 *	L-functions' convolution matrix is 
	 *	[1/4 1/2 1/4;1/2 1 1/2; 1/4 1/2 1/4]
	 * 
	 * The Blue channel reconstruction uses exactly the same methods.
	 */
	for (x = 0; x < w; x++) 
	{
		bayer = (x&1?0:1) + (y&1?0:2);
		for (color=0; color < 3; color+=2) {
			if ((color==RED && bayer == pos_code[3]) 
					|| (color==BLUE 
						    && bayer == pos_code[0])) {
				value=value2=div=0;
				if (x > 0 && y > 0) {
					value += image_h[AD(x-1,0,w)+color]
						-image_h[AD(x-1,0,w)+GREEN];
					value2+= image_v[AD(x-1,0,w)+color]
						-image_v[AD(x-1,0,w)+GREEN];
					div++;
				}
				if (x > 0 && y < h-1) {
					value += image_h[AD(x-1,2,w)+color]
						-image_h[AD(x-1,2,w)+GREEN];
					value2+= image_v[AD(x-1,2,w)+color]
						-image_v[AD(x-1,2,w)+GREEN];
					div++;
				}
				if (x < w-1 && y > 0) {
					value += image_h[AD(x+1,0,w)+color]
						-image_h[AD(x+1,0,w)+GREEN];
					value2+= image_v[AD(x+1,0,w)+color]
						-image_v[AD(x+1,0,w)+GREEN];
					div++;
				}
				if (x < w-1 && y < h-1) {
					value += image_h[AD(x+1,2,w)+color]
					        -image_h[AD(x+1,2,w)+GREEN];
					value2+= image_v[AD(x+1,2,w)+color]
						-image_v[AD(x+1,2,w)+GREEN];
								div++;
				}
				image_h[AD(x,1,w)+color]=
						CLAMP(
						image_h[AD(x,1,w)+GREEN]
						+value/div);
				image_v[AD(x,1,w)+color]=
						CLAMP(image_v[AD(x,1,w)+GREEN]
						+value2/div);
			} else if ((color==RED && bayer == pos_code[2]) 
					|| (color==BLUE 
						    && bayer == pos_code[1])) {
				value=value2=div=0;
				if (y > 0) {
					value += image_h[AD(x,0,w)+color]
						-image_h[AD(x,0,w)+GREEN];
					value2+= image_v[AD(x,0,w)+color]
						-image_v[AD(x,0,w)+GREEN];
						div++;
				}
				if (y < h-1) {
					value += image_h[AD(x,2,w)+color]
						-image_h[AD(x,2,w)+GREEN];
					value2+= image_v[AD(x,2,w)+color]
						-image_v[AD(x,2,w)+GREEN];
					div++;
				}
				image_h[AD(x,1,w)+color]=
						CLAMP(
						image_h[AD(x,1,w)+GREEN]
						+value/div);
				image_v[AD(x,1,w)+color]=
						CLAMP(
						image_v[AD(x,1,w)+GREEN]
						+value2/div);
			} else if ((color==RED && bayer == pos_code[1]) 
					|| (color==BLUE 
						    && bayer == pos_code[2])) {
					value=value2=div=0;
				if (x > 0) {
					value += image_h[AD(x-1,1,w)+color]
						-image_h[AD(x-1,1,w)+GREEN];
					value2+= image_v[AD(x-1,1,w)+color]
						-image_v[AD(x-1,1,w)+GREEN];
					div++;
				}
				if (x < w-1) {
					value += image_h[AD(x+1,1,w)+color]
						-image_h[AD(x+1,1,w)+GREEN];
					value2+= image_v[AD(x+1,1,w)+color]
						-image_v[AD(x+1,1,w)+GREEN];
					div++;
				}
				image_h[AD(x,1,w)+color]=
						CLAMP(
						image_h[AD(x,1,w)+GREEN]
						+value/div);
				image_v[AD(x,1,w)+color]=
						CLAMP(
						image_v[AD(x,1,w)+GREEN]
						+value2/div);
			}
		}
	}
	return GP_OK;
}


/**
 * \brief Missing greens are reconstructed on a single row
 * \param image the image which is being reconstructed
 * \param image_h three-row window, horizontal interpolation of row 1 is done
 * \param image_v three-row window, vertical interpolation of row 1 is done
 * \param w width of image
 * \param h height of image. 
 * \param y row number from image which is under construction
 * \param pos_code position code related to Bayer tiling in use
 */

static
int do_green_ctr_row(unsigned char *image, unsigned char *image_h, 
		    unsigned char *image_v, int w, int h, int y, int *pos_code)
{
	int x, bayer;
	int value,div;
	/*
	 * The horizontal green estimation on a red-green row is 
	 * G(x) = (2*R(x)+2*G(x+1)+2*G(x-1)-R(x-2)-R(x+2))/4
	 * The estimation on a green-blue row works in the same
	 * way.
	 */
	for (x = 0; x < w; x++) {
		bayer = (x&1?0:1) + (y&1?0:2);
		/* pos_code[0] = red. green lrtb, blue diagonals */
		/* pos_code[3] = blue. green lrtb, red diagonals */
		if ( bayer == pos_code[0] || bayer == pos_code[3]) {
			div=value=0;
			if (bayer==pos_code[0])
				value += 2*image[AD(x,y,w)+RED];
			else
				value += 2*image[AD(x,y,w)+BLUE];
			div+=2;
			if (x < (w-1)) {
				value += 2*image[AD(x+1,y,w)+GREEN];
				div+=2;	
			}
			if (x < (w-2)) {
				if (bayer==pos_code[0])
					value -= image[AD(x+2,y,w)+RED];
				else
					value -= image[AD(x+2,y,w)+BLUE];
				div--;
			}
			if (x > 0) {
				value += 2*image[AD(x-1,y,w)+GREEN];
				div+=2;
			}
			if (x > 1) {
				if (bayer==pos_code[0])
					value -= image[AD(x-2,y,w)+RED];
				else
					value -= image[AD(x-2,y,w)+BLUE];
				div--;
			}
			image_h[AD(x,1,w)+GREEN] = CLAMP(value / div);
			/* The method for vertical estimation is just like 
			 * what is done for horizontal estimation, with only  
			 * the obvious difference that it is done vertically. 
			 */
			div=value=0;
			if (bayer==pos_code[0])
				value += 2*image[AD(x,y,w)+RED];
			else
				value += 2*image[AD(x,y,w)+BLUE];
			div+=2;
			if (y < (h-1)) {
				value += 2*image[AD(x,y+1,w)+GREEN];
				div+=2;	
			}
			if (y < (h-2)) {
				if (bayer==pos_code[0])
					value -= image[AD(x,y+2,w)+RED];
				else
					value -= image[AD(x,y+2,w)+BLUE];
				div--;
			}
			if (y > 0) {
				value += 2*image[AD(x,y-1,w)+GREEN];
				div+=2;
			}
			if (y > 1) {
				if (bayer==pos_code[0])
					value -= image[AD(x,y-2,w)+RED];
				else
					value -= image[AD(x,y-2,w)+BLUE];
				div--;
			}
			image_v[AD(x,1,w)+GREEN] = CLAMP(value / div);
			
		}
	}
	return GP_OK;
}

/**
 * \brief Differences are assigned scores across row 2 of buffer_v, buffer_h
 * \param hom_buffer_h tabulation of scores for buffer_h
 * \param hom_buffer_v tabulation of scores for buffer_v
 * \param buffer_h three-row window, scores assigned for pixels in row 2
 * \param buffer_v three-row window, scores assigned for pixels in row 2
 * \param w pixel width of image and buffers
 */

static
int get_diffs_row2(unsigned char * hom_buffer_h, unsigned char *hom_buffer_v, 
		    unsigned char * buffer_h, unsigned char *buffer_v, int w)
{
	int i,j;
	int RGBeps;
	unsigned char Usize_h, Usize_v;

	for (j = 1; j < w-1; j++) {
		i=3*j+9*w;
		Usize_h=0;
		Usize_v=0;

		/* 
		 * Data collected here for adaptive estimates. First we take 
		 * at the given pixel vertical diffs if working in window_v;
		 * left and right diffs if working in window_h. We then choose
		 * of these two diffs as a permissible epsilon-radius within 
		 * which to work. Checking within this radius, we will 
		 * compute scores for the various possibilities. The score 
		 * added in each step is either 1, if the directional change 
		 * is within the prescribed epsilon, or 0 if it is not. 
		 */
		 
		RGBeps=MIN(
			MAX(dRGB(i,i-3,buffer_h),dRGB(i,i+3,buffer_h)),
			MAX(dRGB(i,i-3*w,buffer_v),dRGB(i,i+3*w,buffer_v))
			);
		/*
		 * The scores for the homogeneity mapping. These will be used 
		 * in the choice algorithm to choose the best value.
		 */

		if (dRGB(i,i-3,buffer_h) <= RGBeps)
			Usize_h++;
		if (dRGB(i,i-3,buffer_v) <= RGBeps)
			Usize_v++;
		if (dRGB(i,i+3,buffer_h) <= RGBeps)
			Usize_h++;
		if (dRGB(i,i+3,buffer_v) <= RGBeps)
			Usize_v++;
		if (dRGB(i,i-3*w,buffer_h)<= RGBeps)
			Usize_h++;
		if (dRGB(i,i-3*w,buffer_v) <= RGBeps)
			Usize_v++;
		if (dRGB(i,i+3*w,buffer_h) <= RGBeps)
			Usize_h++;
		if (dRGB(i,i+3*w,buffer_v) <= RGBeps)
			Usize_v++;
		hom_buffer_h[j+2*w]=Usize_h;
		hom_buffer_v[j+2*w]=Usize_v;
	}
	return GP_OK;
}

/**
 * \brief Interpolate a expanded bayer array into an RGB image.
 *
 * \param image the linear RGB array as both input and output
 * \param w width of the above array
 * \param h height of the above array
 * \param tile how the 2x2 bayer array is layed out
 *
 * This function interpolates a bayer array which has been pre-expanded
 * by gp_bayer_expand() to an RGB image. It applies the method of adaptive 
 * homogeneity-directed demosaicing. 
 *
 * \return a gphoto error code
 *
 * \par
 * In outline, the interpolation algorithm used here does the 
 * following:
 *
 * \par
 * In principle, the first thing which is done is to split off from the 
 * image two copies. In one of these, interpolation will be done in the 
 * vertical direction only, and in the other copy only in the 
 * horizontal direction. "Cross-color" data is used throughout, on the 
 * principle that it can be used as a corrector for brightness even if it is 
 * derived from the "wrong" color. Finally, at each pixel there is a choice 
 * criterion to decide whether to use the result of the vertical 
 * interpolation, the horizontal interpolation, or an average of the two. 
 *
 * \par
 * Memory use and speed are optimized by using two sliding windows, one  
 * for the vertical interpolation and the other for the horizontal 
 * interpolation instead of using two copies of the entire input image. The 
 * nterpolation and the choice algorithm are then implemented entirely within
 * these windows, too. When this has been done, a completed row is written back
 * to the image. Then the windows are moved, and the process repeats. 
 */

int gp_ahd_interpolate (unsigned char *image, int w, int h, BayerTile tile) 
{
	int i, j, k, x, y;
	int p[4];
	int color;
	unsigned char *window_h, *window_v, *cur_window_h, *cur_window_v;
	unsigned char *homo_h, *homo_v;
	unsigned char *homo_ch, *homo_cv;

	window_h = calloc (w * 18, 1);
	window_v = calloc (w * 18, 1);
	homo_h = calloc (w*3, 1);
	homo_v = calloc (w*3, 1);
	homo_ch = calloc (w, 1);
	homo_cv = calloc (w, 1);
	if (!window_h || !window_v || !homo_h || !homo_v || !homo_ch || !homo_cv) {
		free (window_h);
		free (window_v);
		free (homo_h);
		free (homo_v);
		free (homo_ch);
		free (homo_cv);
		GP_LOG_E ("Out of memory");
		return GP_ERROR_NO_MEMORY;
	}
	switch (tile) {
	default:
	case BAYER_TILE_RGGB:
	case BAYER_TILE_RGGB_INTERLACED:
		p[0] = 0; p[1] = 1; p[2] = 2; p[3] = 3;
		break;
	case BAYER_TILE_GRBG:
	case BAYER_TILE_GRBG_INTERLACED:
		p[0] = 1; p[1] = 0; p[2] = 3; p[3] = 2;
		break;
	case BAYER_TILE_BGGR:
	case BAYER_TILE_BGGR_INTERLACED:
		p[0] = 3; p[1] = 2; p[2] = 1; p[3] = 0;
		break;
	case BAYER_TILE_GBRG:
	case BAYER_TILE_GBRG_INTERLACED:
		p[0] = 2; p[1] = 3; p[2] = 0; p[3] = 1;
		break;
	}

	/* 
	 * Once the algorithm is initialized and running, one cycle of the 
	 * algorithm can be described thus:
	 * 
	 * Step 1
	 * Write from row y+3 of the image to row 5 in window_v and in 
	 * window_h. 
	 *
	 * Step 2
	 * Interpolate missing green data on row 5 in each window. Data from
	 * the image only is needed for this, not data from the windows. 
	 *
	 * Step 3
	 * Now interpolate the missing red or blue data on row 4 in both 
	 * windows. We need to do this inside the windows; what is required 
	 * is the real or interpolated green data from rows 3 and 5, and the 
	 * real data on rows 3 and 5 about the color being interpolated on 
	 * row 4, so all of this information is available in the two windows. 
	 * Note that for this operation we are interpolating the center row 
	 * of cur_window_v and cur_window_h. 
	 * 
	 * Step 4
	 * Now we have five completed rows in each window, 0 through 4 (rows
	 * 0 - 3 having been done in previous cycles). Completed rows 0 - 4 
	 * are what is required in order to run the choice algorithm at 
	 * each pixel location across row 2, to decide whether to choose the 
	 * data for that pixel from window_v or from window_h. We run the 
	 * choice algorithm, sending the data from row 2 over to row y of the 
	 * image, pixel by pixel. 
	 *
	 * Step 5
	 * Move the windows down (or the data in them up) by one row.
	 * Increment y, the row counter for the image. Go to Step 1.
	 * 
	 * Initialization of the algorithm clearly requires some special 
	 * steps, which are described below as they occur. 
	 */
	cur_window_h = window_h+9*w; 
	cur_window_v = window_v+9*w; 
	/*
	 * Getting started. Copy row 0 from image to line 4 of windows
	 * and row 1 from image to line 5 of windows. 
	 */
	memcpy (window_h+12*w, image, 6*w);
	memcpy (window_v+12*w, image, 6*w);
	/*
	 * Now do the green interpolation in row 4 of the windows, the 
	 * "center" row of cur_window_v and  _h, with the help of image row 0
	 * and image row 1.
	 */
	do_green_ctr_row(image, cur_window_h, cur_window_v, w, h, 0, p);
	/* this does the green interpolation in row 5 of the windows */
	do_green_ctr_row(image, cur_window_h+3*w, cur_window_v+3*w, w, h, 1, p);
	/*
	 * we are now ready to do the rb interpolation on row 4 of the 
	 * windows, which relates to row 0 of the image. 
	 */ 
	do_rb_ctr_row(cur_window_h, cur_window_v, w, h, 0, p);
	/*
	 * Row row 4, which will be mapped to image row 0, is finished in both
	 * windows. Row 5 has had only the green interpolation. 
	 */
	memmove(window_h, window_h+3*w,15*w);
	memmove(window_v, window_v+3*w,15*w);
	memcpy (window_h+15*w, image+6*w, 3*w);
	memcpy (window_v+15*w, image+6*w, 3*w);
	/*
	 * now we have shifted backwards and we have row 0 of the image in 
	 * row 3 of the windows. Row 4 of the window contains row 1 of image
	 * and needs the rb interpolation. We have copied row 2 of the image 
	 * into row 5 of the windows and need to do green interpolation. 
	 */
	do_green_ctr_row(image, cur_window_h+3*w, cur_window_v+3*w, w, h, 2, p);
	do_rb_ctr_row(cur_window_h, cur_window_v, w, h, 1, p);
	memmove (window_h, window_h+3*w, 15*w);
	memmove(window_v, window_v+3*w,15*w); 
	/*
	 * We have shifted one more time. Row 2 of the two windows is 
	 * the original row 0 of the image, now fully interpolated. Rows 3 
	 * and 4 of the windows contain the original rows 1 and 2 of the 
	 * image, also fully interpolated. They will be used while applying 
	 * the choice algorithm on row 2, in order to write it back to row
	 * 0 of the image. The algorithm is now fully initialized. We enter 
	 * the loop which will complete the algorithm for the whole image.
	 */
	 
	for (y = 0; y < h; y++) {
		if(y<h-3) {
			memcpy (window_v+15*w,image+3*y*w+9*w, 3*w);
			memcpy (window_h+15*w,image+3*y*w+9*w, 3*w);
		} else {
			memset(window_v+15*w, 0, 3*w);
			memset(window_h+15*w, 0, 3*w);
		}
		if (y<h-3) 
			do_green_ctr_row(image, cur_window_h+3*w, 
					cur_window_v+3*w, w, h, y+3, p);
		if (y<h-2) 
			do_rb_ctr_row(cur_window_h, cur_window_v, w, h, y+2, p);
		/*
		 * The next function writes row 2 of diffs, which is the set of 
		 * diff scores for row y+1 of the image, which is row 3 of our 
		 * windows. When starting with row 0 of the image, this is all
		 * we need. As we continue, the results of this calculation 
		 * will also be rotated; in general we need the diffs for rows
		 * y-1, y, and y+1 in order to carry out the choice algorithm
		 * for writing row y.
		 */
		get_diffs_row2(homo_h, homo_v, window_h, window_v, w);
		memset(homo_ch, 0, w);
		memset(homo_cv, 0, w);

		/* The choice algorithm now will use the sum of the nine diff 
		 * scores computed at the pixel location and at its eight 
		 * nearest neighbors. The direction with highest score will 
		 * be used; if the scores are equal an average is used. 
		 */
		for (x=0; x < w; x++) {
			for (i=-1; i < 2;i++) {
				for (k=0; k < 3;k++) {
					j=i+x+w*k; 
					homo_ch[x]+=homo_h[j];
					homo_cv[x]+=homo_v[j];
				}
			}
			for (color=0; color < 3; color++) {
				if (homo_ch[x] > homo_cv[x])
					image[3*y*w+3*x+color]
					= window_h[3*x+6*w+color];
				else if (homo_ch[x] < homo_cv[x])
					image[3*y*w+3*x+color]
					= window_v[3*x+6*w+color];
				else
					image[3*y*w+3*x+color]
					= (window_v[3*x+6*w+color]+
						window_h[3*x+6*w+color])/2;
			}
		}
		/* Move the windows; loop back if not finished. */
		memmove(window_v, window_v+3*w, 15*w);
		memmove(window_h, window_h+3*w, 15*w);
		memmove (homo_h,homo_h+w,2*w);
		memmove (homo_v,homo_v+w,2*w);
	}
	free(window_v);
	free(window_h);
	free(homo_h);
	free(homo_v);
	free(homo_ch);
	free(homo_cv);
	return GP_OK;
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
 * bitmap with RGB values interpolated. It does the same job as  
 * gp_bayer_decode() but it calls gp_ahd_interpolate() instead of calling
 * gp_bayer_interpolate(). Use this instead of gp_bayer_decode() if you 
 * want to use or to test AHD interpolation in a camera library. 
 * \return a gphoto error code
 */

int
gp_ahd_decode (unsigned char *input, int w, int h, unsigned char *output,
		 BayerTile tile)
{
	gp_bayer_expand (input, w, h, output, tile);
	gp_ahd_interpolate (output, w, h, tile);
	return GP_OK;
}
