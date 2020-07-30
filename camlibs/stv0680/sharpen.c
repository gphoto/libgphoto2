/*
 * GIMP CVS: "|Id: sharpen.c,v 1.23.2.1 2001/01/12 13:15:10 neo Exp |"
 *
 *   Sharpen filters for The GIMP -- an image manipulation program
 *
 *   Copyright 1997-1998 Michael Sweet (mike@easysw.com)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA  02110-1301  USA
 *
 * Contents:
 *
 *   sharpen()                 - Sharpen an image using a median filter.
 *   rgb_filter()              - Sharpen RGB pixels.
 *
 * Revision History:
 *
 *   See GIMP ChangeLog
 */

#include <stdlib.h>
#include <string.h>

#include "sharpen.h"

#define new(ctype, ccount)   calloc(ccount, sizeof(ctype))

static void
compute_luts (int sharpen_percent, int *pos_lut, int *neg_lut )
{
  int i;	/* Looping var */
  int fact;	/* 1 - sharpness */

  fact = 100 - sharpen_percent;
  if (fact < 1)
    fact = 1;

  for (i = 0; i < 256; i ++)
    {
      pos_lut[i] = (800 * i) / fact;
      neg_lut[i] = (4 + pos_lut[i] - (i << 3)) >> 3;
    };
}

/** 'rgb_filter()' - Sharpen RGB pixels.  **/

static void rgb_filter (int    width,	/* I - Width of line in pixels */
		 int    *pos_lut,
		 int    *neg_lut,
 	         unsigned char *src,	/* I - Source line */
	         unsigned char *dst,	/* O - Destination line */
		long int *neg0,	/* I - Top negative coefficient line */
		long int *neg1,	/* I - Middle negative coefficient line */
		long int *neg2)	/* I - Bottom negative coefficient line */
{
  long int pixel;		/* New pixel value */

  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
  width -= 2;

  while (width > 0)
    {
      pixel = (pos_lut[*src++] - neg0[-3] - neg0[0] - neg0[3] -
	       neg1[-3] - neg1[3] -
	       neg2[-3] - neg2[0] - neg2[3]);
      pixel = (pixel + 4) >> 3;
      if (pixel < 0)
	*dst++ = 0;
      else if (pixel < 255)
	*dst++ = pixel;
      else
	*dst++ = 255;

      pixel = (pos_lut[*src++] - neg0[-2] - neg0[1] - neg0[4] -
	       neg1[-2] - neg1[4] -
	       neg2[-2] - neg2[1] - neg2[4]);
      pixel = (pixel + 4) >> 3;
      if (pixel < 0)
	*dst++ = 0;
      else if (pixel < 255)
	*dst++ = pixel;
      else
	*dst++ = 255;

      pixel = (pos_lut[*src++] - neg0[-1] - neg0[2] - neg0[5] -
	       neg1[-1] - neg1[5] -
	       neg2[-1] - neg2[2] - neg2[5]);
      pixel = (pixel + 4) >> 3;
      if (pixel < 0)
	*dst++ = 0;
      else if (pixel < 255)
	*dst++ = pixel;
      else
	*dst++ = 255;

      neg0 += 3;
      neg1 += 3;
      neg2 += 3;
      width --;
    };

  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
}

/** 'sharpen()' - Sharpen an image using a convolution filter. **/
void  sharpen(int width, int height,
	unsigned char *src_region, unsigned char *dest_region,
	int sharpen_percent
) {
    unsigned char   *src_rows[4],	/* Source pixel rows */
		    *src_ptr,		/* Current source pixel */
		    *dst_row;		/* Destination pixel row */
    long int	*neg_rows[4],	/* Negative coefficient rows */
		*neg_ptr;	/* Current negative coefficient */
    int		i,		/* Looping vars */
		y,		/* Current location in image */
		row,		/* Current row in src_rows */
		count,		/* Current number of filled src_rows */
		pitch;		/* Byte width of the image */
    int		img_bpp=3;		/* Bytes-per-pixel in image */
    int		neg_lut[256];		/* Negative coefficient LUT */
    int		pos_lut[256];		/* Positive coefficient LUT */

    compute_luts (sharpen_percent,pos_lut,neg_lut);
    pitch = width * img_bpp;

    for (row = 0; row < 4; row ++)
    {
    	    src_rows[row] = new (unsigned char, pitch);
	    neg_rows[row] = new (long int, pitch);
    }

    dst_row = new (unsigned char, pitch);

    /** Pre-load the first row for the filter...  **/
    memcpy(src_rows[0], src_region, pitch);

    for (i = pitch, src_ptr = src_rows[0], neg_ptr = neg_rows[0];
	 i > 0;   i --, src_ptr ++, neg_ptr ++)
	    *neg_ptr = neg_lut[*src_ptr];

    row   = 1;
    count = 1;

    /** Sharpen...  **/

    for (y = 0; y < height; y ++)
    {
	/** Load the next pixel row... **/
	if ((y + 1) < height)
	{
	    /** Check to see if our src_rows[] array is overflowing yet... **/
	    if (count >= 3)
		count --;

	    /** Grab the next row... **/
	    memcpy(src_rows[row], (src_region + pitch*(y+1)), pitch);

	    for (i = pitch, src_ptr = src_rows[row], neg_ptr = neg_rows[row];
		 i > 0;  i --, src_ptr ++, neg_ptr ++)
		    *neg_ptr = neg_lut[*src_ptr];
	    count ++;
	    row = (row + 1) & 3;
	}
	else
	{
	  /** No more pixels at the bottom...  Drop the oldest samples... **/
	  count --;
	}

	/** Now sharpen pixels and save the results... **/

	if (count == 3)
	{
	     rgb_filter(width, pos_lut, neg_lut,
		    src_rows[(row + 2) & 3], dst_row,
		    neg_rows[(row + 1) & 3] + img_bpp,
		    neg_rows[(row + 2) & 3] + img_bpp,
		    neg_rows[(row + 3) & 3] + img_bpp);

	    /** Set the row...  **/
	    memcpy((dest_region + pitch*y), dst_row, pitch);
	}
	else if (count == 2)
	{
	    if (y == 0)	/* first row */
		memcpy((dest_region), src_rows[0], pitch);
	    else                  /* last row  */
		memcpy((dest_region + pitch*(y)), src_rows[(height-1)&3], pitch);
	}
    } /* for */

    /** OK, we're done.  Free all memory used...  **/
    for (row = 0; row < 4; row ++)
    {
	free (src_rows[row]);
	free (neg_rows[row]);
    }
    free (dst_row);
}
