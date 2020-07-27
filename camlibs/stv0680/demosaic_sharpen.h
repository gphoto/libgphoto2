/** Decl of demosaic_sharpen */
#ifndef __STV680_DEMOSAIC_SHARPEN_H
#define __STV680_DEMOSAIC_SHARPEN_H

#ifndef __BAYER_H__
typedef enum _BayerTile{
	BAYER_TILE_RGGB = 0,
	BAYER_TILE_GRBG = 1,
	BAYER_TILE_BGGR = 2,
	BAYER_TILE_GBRG = 3,
	BAYER_TILE_RGGB_INTERLACED = 4,		/* scanline order: R1,G1,R2,G2,...,G1,B1,G2,B2,... */
	BAYER_TILE_GRBG_INTERLACED = 5,
	BAYER_TILE_BGGR_INTERLACED = 6,
	BAYER_TILE_GBRG_INTERLACED = 7
} BayerTile;
#endif

void demosaic_sharpen (const int width, const int height,
		       const unsigned char * const src_region,
		       unsigned char * const dest_region,
		       const int alpha, const BayerTile bt);
#endif
