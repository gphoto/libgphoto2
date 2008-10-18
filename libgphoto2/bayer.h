/** \file
 *
 * \author Copyright 2001 Lutz Müller <lutz@users.sf.net>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __BAYER_H__
#define __BAYER_H__

/**
 * \brief how the bayer CCD array is layed out
 * 
 * This enumeration defines how the CCD bayer array is laid out.
 */
typedef enum {
	BAYER_TILE_RGGB = 0,			/**< \brief raster is RG,GN */
	BAYER_TILE_GRBG = 1,			/**< \brief raster is GR,BG */
	BAYER_TILE_BGGR = 2,			/**< \brief raster is BG,GR */
	BAYER_TILE_GBRG = 3,			/**< \brief raster is RG,GB */
	BAYER_TILE_RGGB_INTERLACED = 4,		/**< \brief scanline order: R1,G1,R2,G2,...,G1,B1,G2,B2,... */
	BAYER_TILE_GRBG_INTERLACED = 5,		/**< \brief scanline order: G1,R1,R2,G2,...,B1,G1,B2,G2,... */
	BAYER_TILE_BGGR_INTERLACED = 6,		/**< \brief scanline order: B1,G1,R2,G2,...,G1,R1,G2,R2,... */
	BAYER_TILE_GBRG_INTERLACED = 7,		/**< \brief scanline order: G1,B1,G2,B2,...,R1,G1,R2,G2,... */
} BayerTile;

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

#endif /* __BAYER_H__ */
