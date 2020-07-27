/* The GIMP -- an image manipulation program
 * Copyright 1995 Spencer Kimball and Peter Mattis
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Simplified and modified by Kevin Sisson <kjsisson@bellsouth.net> for use
 * in the pencam program, December, 2001
 *
 * Integrated into gphoto2 by Marcus Meissner <marcus@jet.franken.de>, Dec 2001
 */

#include <string.h>
#include "saturate.h"

/* FIXME: should be variable, and user configurable. Which is problematic in
 * gphoto2 :/ */
#define HUE_DATA        40
#define LIGHTNESS_DATA  40
#define SATURATION_DATA 40

static const int default_colors[6][3] =
{
  { 255,   0,   0 },
  { 255, 255,   0 },
  {   0, 255,   0 },
  {   0, 255, 255 },
  {   0,   0, 255 },
  { 255,   0, 255 }
};


/*********************************
 *   color conversion routines   *
 *********************************/

static void
gimp_rgb_to_hls (int *red, int *green, int *blue)
{
  int    r, g, b;
  double h, l, s;
  int    min, max;
  int    delta;

  r = *red;
  g = *green;
  b = *blue;

  if (r > g)
    {
      max = MAX (r, b);
      min = MIN (g, b);
    }
  else
    {
      max = MAX (g, b);
      min = MIN (r, b);
    }

  l = (max + min) / 2.0;

  if (max == min)
    {
      s = 0.0;
      h = 0.0;
    }
  else
    {
      delta = (max - min);

      if (l < 128)
	s = 255 * (double) delta / (double) (max + min);
      else
	s = 255 * (double) delta / (double) (511 - max - min);

      if (r == max)
	h = (g - b) / (double) delta;
      else if (g == max)
	h = 2 + (b - r) / (double) delta;
      else
	h = 4 + (r - g) / (double) delta;

      h = h * 42.5;

      if (h < 0)
	h += 255;
      else if (h > 255)
	h -= 255;
    }

  *red   = h;
  *green = l;
  *blue  = s;
}

static int gimp_hls_value (double n1, double n2, double hue)
{
  double value;

  if (hue > 255)
    hue -= 255;
  else if (hue < 0)
    hue += 255;
  if (hue < 42.5)
    value = n1 + (n2 - n1) * (hue / 42.5);
  else if (hue < 127.5)
    value = n2;
  else if (hue < 170)
    value = n1 + (n2 - n1) * ((170 - hue) / 42.5);
  else
    value = n1;

  return (int) (value * 255);
}

static void gimp_hls_to_rgb (int *hue, int *lightness, int *saturation)
{
  double h, l, s;
  double m1, m2;

  h = *hue;
  l = *lightness;
  s = *saturation;

  if (s == 0)
    {
      /*  achromatic case  */
      *hue        = l;
      *lightness  = l;
      *saturation = l;
    }
  else
    {
      if (l < 128)
	m2 = (l * (255 + s)) / 65025.0;
      else
	m2 = (l + s - (l * s) / 255.0) / 255.0;

      m1 = (l / 127.5) - m2;

      /*  chromatic case  */
      *hue        = gimp_hls_value (m1, m2, h + 85);
      *lightness  = gimp_hls_value (m1, m2, h);
      *saturation = gimp_hls_value (m1, m2, h - 85);
    }
}


/*  hue saturation machinery  */

static void hue_saturation_initialize (HueSaturationDialog *hsd)
{
  int i;

  for (i = 0; i < 7; i++)
    {
      hsd->hue[i] = 0.0;
      hsd->lightness[i] = 0.0;
      hsd->saturation[i] = (double) (hsd->saturation_data)/2;
    }
}

static void hue_saturation_calculate_transfers (HueSaturationDialog *hsd)
{
  int value;
  int hue;
  int i;

  /*  Calculate transfers  */
  for (hue = 0; hue < 6; hue++)
    for (i = 0; i < 256; i++)
      {
	value = (hsd->hue[0] + hsd->hue[hue + 1]) * 255.0 / 360.0;
	if ((i + value) < 0)
	  hsd->hue_transfer[hue][i] = 255 + (i + value);
	else if ((i + value) > 255)
	  hsd->hue_transfer[hue][i] = i + value - 255;
	else
	  hsd->hue_transfer[hue][i] = i + value;

	/*  Lightness  */
	value = (hsd->lightness[0] + hsd->lightness[hue + 1]) * 127.0 / 100.0;
	value = CLAMP (value, -255, 255);
	if (value < 0)
	  hsd->lightness_transfer[hue][i] = (unsigned char) ((i * (255 + value)) / 255);
	else
	  hsd->lightness_transfer[hue][i] = (unsigned char) (i + ((255 - i) * value) / 255);

	/*  Saturation  */
	value = (hsd->saturation[0] + hsd->saturation[hue + 1]) * 255.0 / 100.0;
	value = CLAMP (value, -255, 255);

	hsd->saturation_transfer[hue][i] = CLAMP ((i * (255 + value)) / 255, 0, 255);
      }
}

static void hue_saturation_update (HueSaturationDialog *hsd)
{
  int i;
  int rgb[3];

  hue_saturation_calculate_transfers (hsd);

  for (i = 0; i < 6; i++)
    {
      rgb[RED_PIX]   = default_colors[i][RED_PIX];
      rgb[GREEN_PIX] = default_colors[i][GREEN_PIX];
      rgb[BLUE_PIX]  = default_colors[i][BLUE_PIX];

      gimp_rgb_to_hls (rgb, rgb + 1, rgb + 2);

      rgb[RED_PIX]   = hsd->hue_transfer[i][rgb[RED_PIX]];
      rgb[GREEN_PIX] = hsd->lightness_transfer[i][rgb[GREEN_PIX]];
      rgb[BLUE_PIX]  = hsd->saturation_transfer[i][rgb[BLUE_PIX]];

      gimp_hls_to_rgb (rgb, rgb + 1, rgb + 2);

    }
}

/********************************  */
void stv680_hue_saturation(
    int width, int height, unsigned char *srcPR, unsigned char *destPR
) {
    unsigned char *src, *s;
    unsigned char *dest, *d;
    int w, h;
    int r, g, b;
    int hue;

    /*  the hue-saturation tool dialog  */
    HueSaturationDialog hsd;

    memset(&hsd, 0, sizeof(hsd));

    hsd.saturation_data = SATURATION_DATA;
    hsd.lightness_data = LIGHTNESS_DATA;
    hsd.hue_data = HUE_DATA;

    hue_saturation_initialize(&hsd);
    hue_saturation_update(&hsd);

    /*  Set the transfer arrays  (for speed)  */
    h = height;
    src = srcPR;
    dest = destPR;

  while (h--)
    {
      w = width;
      s = src;
      d = dest;
      while (w--)
	{
	  r = s[RED_PIX];
	  g = s[GREEN_PIX];
	  b = s[BLUE_PIX];

	  gimp_rgb_to_hls (&r, &g, &b);

	  if (r < 43)
	    hue = 0;
	  else if (r < 85)
	    hue = 1;
	  else if (r < 128)
	    hue = 2;
	  else if (r < 171)
	    hue = 3;
	  else if (r < 213)
	    hue = 4;
	  else
	    hue = 5;

	  r = hsd.hue_transfer[hue][r];
	  g = hsd.lightness_transfer[hue][g];
	  b = hsd.saturation_transfer[hue][b];

	  gimp_hls_to_rgb (&r, &g, &b);

	  d[RED_PIX] = r;
	  d[GREEN_PIX] = g;
	  d[BLUE_PIX] = b;

	  s += 3;   /* bytes/pixel = 3  */
	  d += 3;
	}
      src += 3*width;
      dest += 3*width;
    }
}
