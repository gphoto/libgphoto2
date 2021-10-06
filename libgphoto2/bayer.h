/** \file bayer.h
 *
 * \author Copyright 2001 Lutz Mueller <lutz@users.sf.net>
 *
 * \note
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \note
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \note
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBGPHOTO2_BAYER_H
#define LIBGPHOTO2_BAYER_H


#include "bayer-types.h"


int gp_bayer_expand (unsigned char *input, int w, int h, unsigned char *output,
                     BayerTile tile);
int gp_bayer_decode (unsigned char *input, int w, int h, unsigned char *output,
		     BayerTile tile);
int gp_bayer_interpolate (unsigned char *image, int w, int h, BayerTile tile);
/*
 * The following two functions use an alternative procedure called Adaptive
 * Homogeneity-directed demosaicing instead of the standard bilinear
 * interpolation with basic edge-detection method used in the previous two
 * functions. To use or test this method of Bayer interpolation, just use
 * gp_ahd_decode() in the same way and in the same place as gp_bayer_decode()
 * is used.
 */

int gp_ahd_decode (unsigned char *input, int w, int h, unsigned char *output,
		     BayerTile tile);
int gp_ahd_interpolate (unsigned char *image, int w, int h, BayerTile tile);

#endif /* !defined(LIBGPHOTO2_BAYER_H) */
