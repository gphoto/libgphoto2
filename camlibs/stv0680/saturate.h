#ifndef _SATURATE_H
#define _SATURATE_H
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
 */

#define RED_PIX          0
#define GREEN_PIX        1
#define BLUE_PIX         2

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif


typedef enum
{
  ALL_HUES,
  RED_HUES,
  YELLOW_HUES,
  GREEN_HUES,
  CYAN_HUES,
  BLUE_HUES,
  MAGENTA_HUES
} HueRange;

typedef struct _HueSaturationDialog HueSaturationDialog;

struct _HueSaturationDialog
{
  int     hue_data;
  int     lightness_data;
  int     saturation_data;

  double  hue[7];
  double  lightness[7];
  double  saturation[7];

  HueRange  hue_partition;

  int	hue_transfer[6][256];
  int	lightness_transfer[6][256];
  int	saturation_transfer[6][256];
};

extern void stv680_hue_saturation( int width, int height, unsigned char *srcPR, unsigned char *destPR);
#endif
