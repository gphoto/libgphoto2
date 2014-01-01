/* ycctoppm.c
 *
 * Copyright (c) 2004 Michael Haardt
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

#define _BSD_SOURCE

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>

#include "ycctoppm.h"

#define THUMBNAIL_WIDTH 52
#define THUMBNAIL_HEIGHT 36

#define RATE_H 2

int QVycctoppm(const unsigned char *ycc, long int yccSize, int width, int height, int ratew, unsigned char **ppm, long int *ppmSize)
{
  char header[64];
  size_t headerSize;
  unsigned char *dst;
  int x, y;
  long cr, cb;
  long L;
  long r,g,b;
  const unsigned char *Y;
  const unsigned char *Cr;
  const unsigned char *Cb;

  snprintf(header,sizeof(header),"P6\n%d %d\n255\n",width,height);
  headerSize=strlen(header);

  *ppm=malloc(*ppmSize=(headerSize+3*width*height));

  dst=*ppm;
  memcpy(dst,header,headerSize);  dst+=headerSize;

  Y = ycc;
  Cb = Y + (height * width);
  Cr = Cb + (height / RATE_H) * (width / ratew);

  for (y=0; y<height; ++y)
  {
    for (x=0; x<width; ++x)
    {
      L = Y[y * width + x] *  100000; 
      cb = Cb[(y/RATE_H) * width/ratew + x/ratew];
      if (cb > 127) cb = cb - 256;
      cr = Cr[(y/RATE_H) * width/ratew + x/ratew];
      if (cr > 127) cr = cr - 256;

      r = L + 140200 * cr;
      g = L - 34414 * cb - 71414 * cr;
      b = L + 177200 * cb;

      r = r / 100000;
      g = g / 100000;
      b = b / 100000;

      if (r<0) r=0; else if (r>255) r=255;
      if (g<0) g=0; else if (g>255) g=255;
      if (b<0) b=0; else if (b>255) b=255;

      *dst++ = r;
      *dst++ = g;
      *dst++ = b;
    }
  }

  return GP_OK;
}
