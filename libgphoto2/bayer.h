/* bayer.h
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __BAYER_H__
#define __BAYER_H__

typedef enum {
	BAYER_TILE_RGGB = 0,
	BAYER_TILE_GRBG = 1,
	BAYER_TILE_BGGR = 2,
	BAYER_TILE_GBRG = 3
} BayerTile;

int gp_bayer_decode (unsigned char *input, int w, int h, unsigned char *output,
		     BayerTile tile);

#endif /* __BAYER_H__ */
