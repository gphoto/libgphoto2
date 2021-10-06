/** \file bayer-types.h
 * \brief bayer type definitions common to camlibs and libgphoto2
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

#ifndef LIBGPHOTO2_BAYER_TYPES_H
#define LIBGPHOTO2_BAYER_TYPES_H

/**
 * \brief how the bayer CCD array is laid out
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

#endif /* !defined(LIBGPHOTO2_BAYER_TYPES_H) */
