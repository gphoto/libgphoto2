/** demosaic_sharpen.c
 * when demosaicing the unbayered image,
 * don't just use bilinear interpolation, but weigh
 * the to be guessed values according to the differences
 * of the known value.
 * Of course, smaller differences mean higher weights.
 *
 * Copyright Kurt Garloff <garloff@suse.de>, 2002/01/15
 * License: GNU GPL
 *
 * Note: Interpolation techniques more intelligent than
 * bilinear inerpolation have been subject to investigations.
 * Those two links came from Jérôme Fenal:
 * http://ise.stanford.edu/class/psych221/99/dixiedog/vision.htm
 * http://www-ise.stanford.edu/~tingchen/main.htm
 *
 * From the evaluation, it seems that we should strive to do something like
 * "Linear Interpolation with Laplacian 2nd order color correction terms I".
 *
 * Here; I went my own way.
 * The reason for this is twofold:
 * - Avoid all possible patent issues
 *   (If I would be American, I would probably want to patent this algo)
 * - Be conservative: By using an interpolation method (with weights bound
 *   below 1), we avoid the risk to get artefacts, like e.g. seen in the
 *   smooth hue algorithms, as our interpolated values are always in between
 *   those from the neighbours, just a little closer to the ones we think
 *   fits better.
 *
 * Our algorithm:
 * Trying to predict the known value based on the next neighbours with
 * the same colour component will yield weights. To achieve this:
 * - Measure the difference |dv| of the known colour component
 * - Choose a function f(|dv|) which prefers points with smaller |dv|
 * - Function should be monotonic and ]0;1[
 * - Sum of weights must be 1, of course
 *
 * We choose f(|dv|) = N / (ALPHA + |dv|)
 * and scale with the reciprocal total sum, so we fulfill the conditions.
 * The algorithm is integer only (unless you enable DEBUG)
 * and therefore suitable for FPU-less machines and/or the kernel.
 *
 * I've chosen ALPHA = 2 and the results look really good.
 *
 * ToDo:
 * - A similar algorithm in HSI space might be slightly better.
 * - Different weighing functions might do better
 * - Do rigorous performance analysis (quality and computation cost)
 *   in comparison to other algos as in papers cited above.
 * - There's the bilinear interpolation included for reference
 *   (debugging purposes). Use it or get rid of it for slightly better
 *   performance.
 *
 * I've implemented this algo here as a testbed. It's only tested for
 * BAYER_TILE_GBRG_INTERLACED, though it's been designed to be general
 * for all BAYERs in gphoto2. (Hence all those tables ...)
 * It should be moved to the gphoto2 infrastructure to help all
 * cameras, not just mine. It includes the demosaicing, so it should
 * be merged with the gp_bayer_decode (or the bilinear demosaicing
 * could be removed from the latter) to avoid double work.
 *
 * History:
 * 2001-01-15, 0.90, KG,
 *		working for inner points (2,2)-(width-3,height-3)
 * 2001-01-15, 1.00, KG,
 *		handle boundary points
 *
 */

#include "config.h"
#include <stdlib.h>
#include "demosaic_sharpen.h"

/* we define bayer as
 * +---> x
 * | 0 1
 * v 2 3
 * y
*/

typedef enum {
	RED = 0, GREEN = 1, BLUE = 2
} col;

/* Don't get confused reading this code; there's lots of
 * indirection through the tables to avoid branches in the code;
 * maybe I love long pipelines too much.
 * If I look at the code long enough, I get confused myself.
 * The boundary special cases unfortunately do introduce some extra
 * branching. (KG)
 */

/* relative positition */
typedef struct _off {
	signed char dx, dy;
} off;

typedef enum {
	NB_DIAG = 0, NB_TLRB, NB_LR, NB_TB, NB_TLRB2
} nb_pat;
/* locations of neighbour points with the same colour */
typedef struct _neighbours {
	unsigned char num;
	off nb_pts[4];
} neighbours;

/* possible locations */
static const neighbours n_pos[8] = {
	{	/* NB_DIAG */
		4, {
			{-1,-1},
			{ 1,-1},
			{-1, 1},
			{ 1, 1}
		}
	},{	/* NB_TLRB */
		4, {
			{ 0,-1},
			{-1, 0},
			{ 1, 0},
			{ 0, 1}
		}
	},{	/* NB_LR */
		2, {
			{-1, 0},
			{ 1, 0},
			{ 0, 0},
			{ 0, 0}
		}
	},{	/* NB_TB */
		2, {
			{ 0,-1},
			{ 0, 1},
			{ 0, 0},
			{ 0, 0}
		}
	},{	/* NB_TLRB2 */
		4, {
			{ 0,-2},
			{-2, 0},
			{ 2, 0},
			{ 0, 2},
		}
	}
};

typedef struct _t_coeff {
	unsigned char cf[4][4];
	unsigned char num;
} t_coeff;

typedef enum {
	DIAG_TO_LR = 0, DIAG_TO_TB, TLRB2_TO_DIAG, TLRB2_TO_TLRB, PATCONV_NONE
} patconv;


/* Transfer matrix pattern to pattern */
static const t_coeff pat_to_pat[4] = {
	{	/* DIAG_TO_LR */
		{
			{2, 0, 2, 0},
			{0, 2, 0, 2},
			{0, 0, 0, 0},
			{0, 0, 0, 0}
		}, 2
	},{	/* DIAG_TO_TB */
		{
			{ 2, 2, 0, 0},
			{ 0, 0, 2, 2},
			{ 0, 0, 0, 0},
			{ 0, 0, 0, 0},
		}, 2
	},{	/* TLRB2_TO_DIAG */
		{
			{1, 1, 0, 0},
			{1, 0, 1, 0},
			{0, 1, 0, 1},
			{0, 0, 1, 1}
		}, 4
	},{	/* TLRB2_TO_TLRB (trivial) */
		{
			{2, 0, 0, 0},
			{0, 2, 0, 0},
			{0, 0, 2, 0},
			{0, 0, 0, 2}
		}, 4
	}
};

static const patconv pconvmap[5][5] = {
	{ PATCONV_NONE, PATCONV_NONE, DIAG_TO_LR, DIAG_TO_TB, PATCONV_NONE },
	{ PATCONV_NONE, PATCONV_NONE, PATCONV_NONE, PATCONV_NONE, PATCONV_NONE },
	{ PATCONV_NONE, PATCONV_NONE, PATCONV_NONE, PATCONV_NONE, PATCONV_NONE },
	{ PATCONV_NONE, PATCONV_NONE, PATCONV_NONE, PATCONV_NONE, PATCONV_NONE },
	{ TLRB2_TO_DIAG, TLRB2_TO_TLRB, PATCONV_NONE, PATCONV_NONE, PATCONV_NONE }
};


/* Next mapping: col of pixel (0,1,2 = RGB &
 * index into n_pos for own, own+1, own+2 */
typedef struct _bayer_desc {
	col colour;
	nb_pat idx_pts[3];
} bayer_desc;

/* T = Bayer Tile, P = Bayer point no
 *                T  P                */
static const bayer_desc bayers[4][4] = {
	{	/* TILE_RGGB */
		{ RED,   {NB_TLRB2, NB_TLRB, NB_DIAG} },
		{ GREEN, {NB_DIAG, NB_TB, NB_LR} },
		{ GREEN, {NB_DIAG, NB_LR, NB_TB} },
		{ BLUE,  {NB_TLRB2, NB_DIAG, NB_TLRB} },
	},{	/* TILE_GRBG */
		{ GREEN, {NB_DIAG, NB_TB, NB_LR} },
		{ RED,   {NB_TLRB2, NB_TLRB, NB_DIAG} },
		{ BLUE,  {NB_TLRB2, NB_DIAG, NB_TLRB} },
		{ GREEN, {NB_DIAG, NB_LR, NB_TB} },
	},{	/* TILE_BGGR */
		{ BLUE,  {NB_TLRB2, NB_DIAG, NB_TLRB} },
		{ GREEN, {NB_DIAG, NB_LR, NB_TB} },
		{ GREEN, {NB_DIAG, NB_TB, NB_LR} },
		{ RED,   {NB_TLRB2, NB_TLRB, NB_DIAG} },
	},{	/* TILE_GBRG */
		{ GREEN, {NB_DIAG, NB_LR, NB_TB} },
		{ BLUE,  {NB_TLRB2, NB_DIAG, NB_TLRB} },
		{ RED,   {NB_TLRB2, NB_TLRB, NB_DIAG} },
		{ GREEN, {NB_DIAG, NB_TB, NB_LR} }
	}
};

/* Use integer arithmetic. Accuracy is 10^-6, which is good enough */
#define SHIFT 20
static inline int weight (const unsigned char dx, const int alpha)
{
	return (1<<SHIFT)/(alpha + dx);
}

/* alpha controls the strength of the weighting; 1 = strongest, 64 = weak */
void demosaic_sharpen (const int width, const int height,
		       const unsigned char * const src_region,
		       unsigned char * const dest_region,
		       const int alpha, const BayerTile bt)
{
	const unsigned char* src_ptr = src_region;
	unsigned char* dst_ptr = dest_region;
	const bayer_desc *bay_des = bayers [bt & 3]; /* Don't care about interlace */
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++, src_ptr += 3, dst_ptr += 3) {
			/* 3 2 */
			/* 1 0 */
			const unsigned char bayer = (1^(x&1)) + ((1^(y&1))<<1);
			const col colour = bay_des[bayer].colour;
			/* nb_pat[0] is our own pattern */
			const nb_pat * const nbpts = bay_des[bayer].idx_pts;
			/* less strong weighting for TLRB2 pattern */
			const int myalpha = (*nbpts == NB_TLRB2? (alpha << 1): alpha);
			const unsigned char colval = src_ptr[colour];
			int weights[4]; int sum_weights = 0.0;
			patconv pconv;
			/* Calc coeffs for prediction */
			int nbs; col ncol; int othcol; int i;
			int skno; int nsumw;
			int predcol = 0; /*  Only for DEBUG */
			/* DPRINTF("(%i,%i)(%p): bay %i, col %i, pat %i, val %i\n", x, y, src_ptr, bayer, colour, nbpts[0], colval);*/
			/* Copy own colour */
			dst_ptr[colour] = colval;
			/* Now calc weights */
			for (nbs = 0; nbs < 4; nbs++) {
				const off offset = n_pos[nbpts[0]].nb_pts[nbs];
				const int nx = x + offset.dx;
				const int ny = y + offset.dy;
				const signed long addr_off = 3 * (offset.dx + width * offset.dy);
				unsigned char thisval = colval; int coeff = 0;
				if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
					thisval = src_ptr[addr_off+colour];
					coeff = weight (abs ((int)colval - thisval), myalpha);
				} else if (nbpts[0] == NB_TLRB2 && x > 0 && x < width-1 && y > 0 && y < height-1) {
					coeff = weight (128, myalpha); /* assign some small weight */
				}
				/*DPRINTF(" (%i,%i)(%p): val %i, diff %i, weight %i\n", nx, ny, src_ptr+addr_off, thisval, abs ((int)colval - thisval), coeff);*/
				predcol += thisval * coeff;
				weights[nbs] = coeff;
				sum_weights += coeff;
			};
#ifdef DEBUG
			printf(" Coeffs:");
			for (nbs = 0; nbs < 4; nbs++)
				printf (" %6.4f", (double)weights[nbs]/sum_weights);
			printf (" -> pred %i\n", predcol/sum_weights);
#endif
			/* Now calculate other colours */
			ncol = (colour+1)%3;
			pconv = pconvmap[nbpts[0]][nbpts[1]];
			if (pconv == PATCONV_NONE)
				abort ();
			othcol = 0; predcol = 0; nsumw = 0; skno = 0;
			/*DPRINTF(" Col %i: pat %i pconv %i\n", ncol, nbpts[1], pconv);*/
			for (nbs = 0; nbs < n_pos[nbpts[1]].num; nbs++) {
				off offset = n_pos[nbpts[1]].nb_pts[nbs];
				const int nx = x + offset.dx;
				const int ny = y + offset.dy;
				const signed long addr_off = 3 * (offset.dx + width * offset.dy);
				int eff_weight = 0; unsigned char thisval;
				for (i = 0; i < 4; i++)
					eff_weight += pat_to_pat[pconv].cf[nbs][i] * weights[i];
				if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
					thisval = src_ptr[addr_off+ncol];
					nsumw += eff_weight;
					/*DPRINTF("  (%i,%i): val %i, eff_w %6.4f\n", nx, ny, thisval, (double)(eff_weight>>1)/sum_weights);*/
					othcol  += thisval * eff_weight;
					predcol += thisval;
				} else {
					skno++;
				}
			};
			dst_ptr[ncol] = othcol/nsumw;
			/*DPRINTF( " -> val %i (bilin: %i)\n", dst_ptr[ncol], predcol/(n_pos[nbpts[1]].num-skno));*/
			/* Third colour */
			ncol = (colour+2)%3;
			pconv = pconvmap[nbpts[0]][nbpts[2]];
			if (pconv == PATCONV_NONE)
				abort ();
			othcol = 0; predcol = 0; nsumw = 0; skno = 0;
			/*DPRINTF(" Col %i: pat %i pconv %i\n", ncol, nbpts[2], pconv);*/
			for (nbs = 0; nbs < n_pos[nbpts[2]].num; nbs++) {
				off offset = n_pos[nbpts[2]].nb_pts[nbs];
				const int nx = x + offset.dx;
				const int ny = y + offset.dy;
				const signed long addr_off = 3 * (offset.dx + width * offset.dy);
				int eff_weight = 0; unsigned char thisval;
				for (i = 0; i < 4; i++)
					eff_weight += pat_to_pat[pconv].cf[nbs][i] * weights[i];
				if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
					thisval = src_ptr[addr_off+ncol];
					nsumw += eff_weight;
					/*DPRINTF("  (%i,%i): val %i, eff_w %6.4f\n", nx, ny, thisval, (double)(eff_weight>>1)/sum_weights);*/
					othcol  += thisval * eff_weight;
					predcol += thisval;
				} else {
					skno++;
				}
			};
			dst_ptr[ncol] = othcol/nsumw;
			/*DPRINTF( " -> val %i (bilin: %i)\n", dst_ptr[ncol], predcol/(n_pos[nbpts[1]].num-skno));*/
		}
	}
}

