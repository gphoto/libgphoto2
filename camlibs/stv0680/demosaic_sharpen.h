/** Decl of demosaic_sharpen */

#ifndef CAMLIBS_STV0680_DEMOSAIC_SHARPEN_H
#define CAMLIBS_STV0680_DEMOSAIC_SHARPEN_H


#include "bayer-types.h"


void demosaic_sharpen (const int width, const int height,
		       const unsigned char * const src_region,
		       unsigned char * const dest_region,
		       const int alpha, const BayerTile bt);

#endif /* !defined(CAMLIBS_STV0680_DEMOSAIC_SHARPEN_H) */
