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
#include <gamma.h>
#include "digigr8.h"

#define GP_MODULE "digigr8"

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

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
	unsigned char translator[16] =
		    {8, 7, 9, 6, 10, 11, 12, 13, 14, 15, 5, 4, 3, 2, 1, 0};

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
				temp1 = (temp1 <<1) & 0xFF;
				bit_counter ++ ;
				cycles ++ ;
				if (cycles > 8) {
					GP_DEBUG ("Too many cycles?\n");
					return GP_ERROR;
				}
				lookup = temp2 & 0xff;
			}
			temp2 = 0;
			for (i=0; i < 17; i++ ) {
				if (i == 16) {
					GP_DEBUG(
					"Illegal lookup value during decomp\n");
					return GP_ERROR;
				}
				if (lookup == lookup_table[i] ) {
					nibble_to_keep[parity] = translator[i];
					break;
				}
			}
			cycles = 0;
			parity ++ ;
		}
		output[bytes_done] = (nibble_to_keep[0] << 4)
						| nibble_to_keep[1];
		bytes_done++;
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
	int i, m;
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
		return GP_ERROR_NO_MEMORY;
	}
	for(i=0; i < width; i++)
	    templine_red[i] = 0x80;

	templine_green = malloc(width);
	if (!templine_green) {
		free (templine_red);
		return GP_ERROR_NO_MEMORY;
	}
	for(i=0; i < width; i++)
		templine_green[i] = 0x80;

	templine_blue = malloc(width);
	if (!templine_blue) {
		free (templine_red);
		free (templine_green);
		return GP_ERROR_NO_MEMORY;
	}
	for(i=0; i < width; i++)
		templine_blue[i] = 0x80;

	GP_DEBUG ("Running second_decompress.\n");
	for (m = 0; m < height / 2; m++) {
		/* First we do an even-numbered line */
		for (i = 0; i < width / 2; i++) {
			delta_right = in[input_counter] & 0x0f;
			delta_left = (in[input_counter] >> 4) & 0xff;
			input_counter++;
			/* left pixel (red) */
			diff = delta_table[delta_left];
			if (!i)
				tempval = templine_red[0] + diff;
			else
				tempval = (templine_red[i]
				    + uncomp[2 *m * width + 2 * i - 2]) / 2
								    + diff;
			tempval = MIN(tempval, 0xff);
			tempval = MAX(tempval, 0);
			uncomp[2 * m * width + 2 * i] = tempval;
			templine_red[i] = tempval;
			/* right pixel (green) */
			diff = delta_table[delta_right];
			if (!i)
				tempval = templine_green[1] + diff;
			else if (2 * i == width - 2 )
				tempval = (templine_green[i]
				    + uncomp[2 * m * width + 2 * i -1]) / 2
								    + diff;
			else
				tempval = (templine_green[i + 1]
				    + uncomp[2 * m * width + 2 * i - 1]) / 2
								    + diff;
			tempval = MIN(tempval, 0xff);
			tempval = MAX(tempval, 0);
			uncomp[2 * m * width + 2 * i + 1] = tempval;
			templine_green[i] = tempval;
		}
		/* then an odd-numbered line */
		for (i = 0; i < width / 2; i++) {
			delta_right = in[input_counter] &0x0f;
			delta_left = (in[input_counter] >> 4) & 0xff;
			input_counter++;
			/* left pixel (green) */
			diff = delta_table[delta_left];
			if (!i)
				tempval = templine_green[0] + diff;
			else
				tempval = (templine_green[i]
				    + uncomp[(2 * m + 1) * width
						+ 2 * i - 2]) / 2 + diff;
			tempval = MIN(tempval, 0xff);
			tempval = MAX(tempval, 0);
			uncomp[(2 * m + 1) * width + 2 * i] = tempval;
			templine_green[i] = tempval;
			/* right pixel (blue) */
			diff = delta_table[delta_right];
			if (!i)
				tempval = templine_blue[0] + diff;
			else
				tempval = (templine_blue[i]
				    + uncomp[(2 * m + 1) * width
						+ 2 * i - 1]) / 2 + diff;
			tempval = MIN(tempval, 0xff);
			tempval = MAX(tempval, 0);
			uncomp[(2 * m + 1) * width + 2 * i + 1] = tempval;
			templine_blue[i] = tempval;
		}
	}
	free(templine_green);
	free(templine_red);
	free(templine_blue);
	return GP_OK;
}

int
digi_decompress (unsigned char *out_data, unsigned char *data,
							int w, int h)
{
	int size;
	unsigned char *temp_data;
	size = w * h / 2;
	temp_data = malloc(size);
	if (!temp_data)
		return GP_ERROR_NO_MEMORY;
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


#define MINMAX(a,min,max) { (min)=MIN(min,a); (max)=MAX(max,a); }

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef CLAMP
#define CLAMP(x)        ((x)<0?0:((x)>255)?255:(x))
#endif

int
digi_postprocess(int width, int height,
					unsigned char* rgb)
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
			MINMAX( RED(rgb , x , y, width), red_min, red_max);
			MINMAX( GREEN(rgb, x, y, width), green_min, green_max);
			MINMAX( BLUE(rgb, x, y, width), blue_min, blue_max);
		}
	}

	/* Normalize brightness ... */

	max = MAX( MAX( red_max, green_max ), blue_max);
	min = MIN( MIN( red_min, green_min ), blue_min);
	amplify = 255.0/(max-min);

	for (y = 0; y < height; y++){
		for(x = 0; x < width; x++ ){
			RED(rgb, x, y, width)= MIN(amplify *
				  (double)(RED(rgb, x, y, width) - min), 255);
			GREEN(rgb, x, y, width)= MIN(amplify *
				  (double)(GREEN(rgb, x, y, width) - min), 255);
			BLUE(rgb, x, y, width)= MIN(amplify *
				  (double)(BLUE(rgb, x, y, width) -min), 255);
		}
	}
	return GP_OK;
}

/*	===== White Balance / Color Enhance / Gamma adjust (experimental) =====

	Get histogram for each color plane
	Expand to reach 0.5% of white dots in image

	Get new histogram for each color plane
	Expand to reach 0.5% of black dots in image

	Get new histogram
	Calculate and apply gamma correction

	if not a dark image:
	For each dot, increases color separation

	===================================================================== */

static int
histogram (unsigned char *data, unsigned int size, int *htable_r,
					    int *htable_g, int *htable_b)
{
	int x;
	/* Initializations */
	for (x = 0; x < 0x100; x++) {
		htable_r[x] = 0;
		htable_g[x] = 0;
		htable_b[x] = 0;
	}
	/* Building the histograms */
	for (x = 0; x < (size * 3); x += 3)
	{
		htable_r[data[x + 0]]++;	/* red   histogram */
		htable_g[data[x + 1]]++;	/* green histogram */
		htable_b[data[x + 2]]++;	/* blue  histogram */
	}
	return GP_OK;
}

int
white_balance (unsigned char *data, unsigned int size, float saturation)
{
	int x, r, g, b, max, d;
	double r_factor, g_factor, b_factor, max_factor;
	int htable_r[0x100], htable_g[0x100], htable_b[0x100];
	unsigned char gtable[0x100];
	double new_gamma, gamma=1.0;

	/* ------------------- GAMMA CORRECTION ------------------- */

	histogram(data, size, htable_r, htable_g, htable_b);
	x = 1;
	for (r = 64; r < 192; r++)
	{
		x += htable_r[r];
		x += htable_g[r];
		x += htable_b[r];
	}
	new_gamma = sqrt((double) (x * 1.5) / (double) (size * 3));
	GP_DEBUG("Provisional gamma correction = %1.2f\n", new_gamma);
	/* Recalculate saturation factor for later use. */
	saturation = saturation * new_gamma * new_gamma;
	GP_DEBUG("saturation = %1.2f\n", saturation);
	gamma = new_gamma;
	if (new_gamma < .70) gamma = 0.70;
	if (new_gamma > 1.2) gamma = 1.2;
	GP_DEBUG("Gamma correction = %1.2f\n", gamma);
	gp_gamma_fill_table(gtable, gamma);
	gp_gamma_correct_single(gtable,data,size);
	if (saturation < .5 ) /* If so, exit now. */
		return GP_OK;

	/* ---------------- BRIGHT DOTS ------------------- */
	max = size / 200;
	histogram(data, size, htable_r, htable_g, htable_b);

	for (r = 0xfe, x = 0; (r > 32) && (x < max); r--)
		x += htable_r[r];
	for (g = 0xfe, x = 0; (g > 32) && (x < max); g--)
		x += htable_g[g];
	for (b = 0xfe, x = 0; (b > 32) && (x < max); b--)
		x += htable_b[b];
	r_factor = (double) 0xfd / r;
	g_factor = (double) 0xfd / g;
	b_factor = (double) 0xfd / b;

	max_factor = r_factor;
	if (g_factor > max_factor) max_factor = g_factor;
	if (b_factor > max_factor) max_factor = b_factor;
	if (max_factor >= 4.0) {
	/* We need a little bit of control, here. If max_factor > 4 the photo
	 * was very dark, after all.
	 */
		if (2.0 * b_factor < max_factor)
			b_factor = max_factor / 2.;
		if (2.0 * r_factor < max_factor)
			r_factor = max_factor / 2.;
		if (2.0 * g_factor < max_factor)
			g_factor = max_factor / 2.;
		r_factor = (r_factor / max_factor) * 4.0;
		g_factor = (g_factor / max_factor) * 4.0;
		b_factor = (b_factor / max_factor) * 4.0;
	}

	if (max_factor > 1.5)
		saturation = 0;
	GP_DEBUG("White balance (bright): r=%1d, g=%1d, b=%1d, \
			r_factor=%1.3f, g_factor=%1.3f, b_factor=%1.3f\n",
					r, g, b, r_factor, g_factor, b_factor);
	if (max_factor <= 1.4) {
		for (x = 0; x < (size * 3); x += 3)
		{
			d = (data[x + 0] << 8) * r_factor + 8;
			d >>= 8;
			if (d > 0xff)
				d = 0xff;
			data[x + 0] = d;
			d = (data[x + 1] << 8) * g_factor + 8;
			d >>= 8;
			if (d > 0xff) { d = 0xff; }
			data[x + 1] = d;
			d = (data[x + 2] << 8) * b_factor + 8;
			d >>= 8;
			if (d > 0xff)
				d = 0xff;
			data[x + 2] = d;
		}
	}
	/* ---------------- DARK DOTS ------------------- */
	max = size / 200;  /*  1/200 = 0.5%  */
	histogram(data, size, htable_r, htable_g, htable_b);

	for (r = 0, x = 0; r < 96 && x < max; r++)
		x += htable_r[r];
	for (g = 0, x = 0; g < 96 && x < max; g++)
		x += htable_g[g];
	for (b = 0, x = 0; b < 96 && x < max; b++)
		x += htable_b[b];

	r_factor = (double) 0xfe / (0xff - r);
	g_factor = (double) 0xfe / (0xff - g);
	b_factor = (double) 0xfe / (0xff - b);

	GP_DEBUG(
	"White balance (dark): r=%1d, g=%1d, b=%1d, \
			r_factor=%1.3f, g_factor=%1.3f, b_factor=%1.3f\n",
				r, g, b, r_factor, g_factor, b_factor);

	for (x = 0; x < (size * 3); x += 3)
	{
		d = (int) 0xff08 - (((0xff - data[x + 0]) << 8) * r_factor);
		d >>= 8;
		if (d < 0)
			 d = 0;
		data[x + 0] = d;
		d = (int) 0xff08 - (((0xff - data[x + 1]) << 8) * g_factor);
		d >>= 8;
		if (d < 0)
			d = 0;
		data[x + 1] = d;
		d = (int) 0xff08 - (((0xff - data[x + 2]) << 8) * b_factor);
		d >>= 8;
		if (d < 0)
			d = 0;
		data[x+2] = d;
	}

	/* ------------------ COLOR ENHANCE ------------------ */

	if(saturation > 0.0) {
		for (x = 0; x < (size * 3); x += 3)
		{
			r = data[x+0]; g = data[x+1]; b = data[x+2];
			d = (int) (r + g + b) / 3.;
			if ( r > d )
				r = r + (int) ((r - d) * (0xff - r)
						/(0x100 - d) * saturation);
			else
				r = r + (int) ((r - d) * (0xff - d)
						/ (0x100 - r) * saturation);
			if (g > d)
				g = g + (int) ((g - d) * (0xff - g)
						/ (0x100 - d) * saturation);
			else
				g = g + (int) ((g - d) * (0xff - d)
						/ (0x100 - g) * saturation);
			if (b > d)
				b = b + (int) ((b - d) * (0xff - b)
						/(0x100 - d) * saturation);
			else
				b = b + (int) ((b - d) * (0xff - d)
						/(0x100 - b) * saturation);
			data[x+0] = CLAMP(r);
			data[x+1] = CLAMP(g);
			data[x+2] = CLAMP(b);
		}
	}
	return GP_OK;
}
