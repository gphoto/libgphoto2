/*
 * Small jpeg decoder library
 *
 * *** NOTE: This is a modified version to deal with the ax203 "JPEG" fmt
 * *** This version can not decompress regular JPEG files, see
 * *** README.ax203-compression for details
 *
 * Copyright (c) 2006, Luc Saillard <luc@saillard.org>
 *
 * ax203 modifications:
 * Copyright (c) 2010, Hans de Goede <hdegoede@redhat.com>
 *
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 * - Neither the name of the author nor the names of its contributors may be
 *  used to endorse or promote products derived from this software without
 *  specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "tinyjpeg.h"
#include "tinyjpeg-internal.h"

#define cY	0
#define cCb	1
#define cCr	2

#define BLACK_Y 0
#define BLACK_U 127
#define BLACK_V 127

#if 0
#if LOG2FILE

#define trace(fmt, args...) do { \
   FILE *f = fopen("/tmp/jpeg.log", "a"); \
   fprintf(f, fmt, ## args); \
   fflush(f); \
   fclose(f); \
} while(0)

#else

#define trace(fmt, args...) do { \
   fprintf(stderr, fmt, ## args); \
   fflush(stderr); \
} while(0)
#endif

#else
#define trace(fmt, args...) do { } while (0)
#endif

#define error(fmt, args...) do { \
   snprintf(priv->error_string, sizeof(priv->error_string), fmt, ## args); \
   return -1; \
} while(0)


static const unsigned char zigzag[64] =
{
   0,  1,  5,  6, 14, 15, 27, 28,
   2,  4,  7, 13, 16, 26, 29, 42,
   3,  8, 12, 17, 25, 30, 41, 43,
   9, 11, 18, 24, 31, 40, 44, 53,
  10, 19, 23, 32, 39, 45, 52, 54,
  20, 22, 33, 38, 46, 51, 55, 60,
  21, 34, 37, 47, 50, 56, 59, 61,
  35, 36, 48, 49, 57, 58, 62, 63
};

/*
 * 4 functions to manage the stream
 *
 *  fill_nbits: put at least nbits in the reservoir of bits.
 *              But convert any 0xff,0x00 into 0xff
 *  get_nbits: read nbits from the stream, and put it in result,
 *             bits is removed from the stream and the reservoir is filled
 *             automatically. The result is signed according to the number of
 *             bits.
 *  look_nbits: read nbits from the stream without marking as read.
 *  skip_nbits: read nbits from the stream but do not return the result.
 *
 * stream: current pointer in the jpeg data (read bytes per bytes)
 * nbits_in_reservoir: number of bits filled into the reservoir
 * reservoir: register that contains bits information. Only nbits_in_reservoir
 *            is valid.
 *                          nbits_in_reservoir
 *                        <--    17 bits    -->
 *            Ex: 0000 0000 1010 0000 1111 0000   <== reservoir
 *                        ^
 *                        bit 1
 *            To get two bits from this example
 *                 result = (reservoir >> 15) & 3
 *
 */
#define fill_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted) do { \
   while (nbits_in_reservoir<nbits_wanted) \
    { \
      unsigned char __c; \
      if (stream >= priv->stream_end) { \
	snprintf(priv->error_string, sizeof(priv->error_string), \
	  "fill_nbits error: need %u more bits\n", \
	  nbits_wanted - nbits_in_reservoir); \
	longjmp(priv->jump_state, -EIO); \
      } \
      __c = *stream++; \
      reservoir <<= 8; \
      reservoir |= __c; \
      nbits_in_reservoir+=8; \
    } \
}  while(0);

/* Signed version !!!! */
#define get_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted,result) do { \
   fill_nbits(reservoir,nbits_in_reservoir,stream,(nbits_wanted)); \
   result = ((reservoir)>>(nbits_in_reservoir-(nbits_wanted))); \
   nbits_in_reservoir -= (nbits_wanted);  \
   reservoir &= ((1U<<nbits_in_reservoir)-1); \
   if ((unsigned int)result < (1UL<<((nbits_wanted)-1))) \
       result += (0xFFFFFFFFUL<<(nbits_wanted))+1; \
}  while(0);

#define look_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted,result) do { \
   fill_nbits(reservoir,nbits_in_reservoir,stream,(nbits_wanted)); \
   result = ((reservoir)>>(nbits_in_reservoir-(nbits_wanted))); \
}  while(0);

/* To speed up the decoding, we assume that the reservoir have enough bit
 * slow version:
 * #define skip_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted) do { \
 *   fill_nbits(reservoir,nbits_in_reservoir,stream,(nbits_wanted)); \
 *   nbits_in_reservoir -= (nbits_wanted); \
 *   reservoir &= ((1U<<nbits_in_reservoir)-1); \
 * }  while(0);
 */
#define skip_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted) do { \
   nbits_in_reservoir -= (nbits_wanted); \
   reservoir &= ((1U<<nbits_in_reservoir)-1); \
}  while(0);

#define be16_to_cpu(x) (((x)[0]<<8)|(x)[1])

static void resync(struct jdec_private *priv);

/**
 * Get the next (valid) huffman code in the stream.
 *
 * To speedup the procedure, we look HUFFMAN_HASH_NBITS bits and the code is
 * lower than HUFFMAN_HASH_NBITS we have automatically the length of the code
 * and the value by using two lookup table.
 * Else if the value is not found, just search (linear) into an array for each
 * bits is the code is present.
 *
 * If the code is not present for any reason, -1 is return.
 */
static int get_next_huffman_code(struct jdec_private *priv, struct huffman_table *huffman_table)
{
  int value, hcode;
  unsigned int extra_nbits, nbits;
  uint16_t *slowtable;

  look_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, HUFFMAN_HASH_NBITS, hcode);
  value = huffman_table->lookup[hcode];
  if (value >= 0)
  {
     unsigned int code_size = huffman_table->code_size[value];
     skip_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, code_size);
     return value;
  }

  /* Decode more bits each time ... */
  for (extra_nbits=0; extra_nbits<16-HUFFMAN_HASH_NBITS; extra_nbits++)
   {
     nbits = HUFFMAN_HASH_NBITS + 1 + extra_nbits;

     look_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, nbits, hcode);
     slowtable = huffman_table->slowtable[extra_nbits];
     /* Search if the code is in this array */
     while (slowtable[0]) {
	if (slowtable[0] == hcode) {
	   skip_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, nbits);
	   return slowtable[1];
	}
	slowtable+=2;
     }
   }
  snprintf(priv->error_string, sizeof(priv->error_string),
    "unknown huffman code: %08x\n", (unsigned int)hcode);
  longjmp(priv->jump_state, -EIO);
  return 0;
}

/**
 *
 * Decode a single block that contains the DCT coefficients.
 * The table coefficients is already dezigzaged at the end of the operation.
 *
 */
static void process_Huffman_data_unit(struct jdec_private *priv, int component, int block_nr)
{
  unsigned char j;
  unsigned int huff_code;
  unsigned char size_val, count_0;

  struct component *c = &priv->component_infos[component];
  short int DCT[64];

  /* Initialize the DCT coef table */
  memset(DCT, 0, sizeof(DCT));

  /* DC coefficient decoding */
  huff_code = get_next_huffman_code(priv, c->DC_table);
  if (huff_code) {
     get_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, huff_code, DCT[0]);
     DCT[0] += c->previous_DC;
     c->previous_DC = DCT[0];
  } else {
     DCT[0] = c->previous_DC;
  }

  /* AC coefficient decoding */
  j = 1;
  while (j<64)
   {
     huff_code = get_next_huffman_code(priv, c->AC_table);

     size_val = huff_code & 0xF;
     count_0 = huff_code >> 4;

     if (size_val == 0)
      { /* RLE */
	if (count_0 == 0)
	  break;	/* EOB found, go out */
	else if (count_0 == 0xF)
	  j += 16;	/* skip 16 zeros */
      }
     else
      {
	j += count_0;	/* skip count_0 zeroes */
	if (j < 64) {
	  get_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, size_val, DCT[j]);
	  j++;
	}
      }
   }

  for (j = 0; j < 64; j++)
    c->DCT[j] = DCT[zigzag[j]];
}

/*
 * Takes two array of bits, and build the huffman table for size, and code
 *
 * lookup will return the symbol if the code is less or equal than HUFFMAN_HASH_NBITS.
 * code_size will be used to known how many bits this symbol is encoded.
 * slowtable will be used when the first lookup didn't give the result.
 */
static int build_huffman_table(struct jdec_private *priv, const unsigned char *bits, const unsigned char *vals, struct huffman_table *table)
{
  unsigned int i, j, code, code_size, val, nbits;
  unsigned char huffsize[257], *hz;
  unsigned int huffcode[257], *hc;
  int slowtable_used[16-HUFFMAN_HASH_NBITS];

  /*
   * Build a temp array
   *   huffsize[X] => numbers of bits to write vals[X]
   */
  hz = huffsize;
  for (i=1; i<=16; i++)
   {
     for (j=1; j<=bits[i]; j++)
       *hz++ = i;
   }
  *hz = 0;

  memset(table->lookup, 0xff, sizeof(table->lookup));
  for (i=0; i<(16-HUFFMAN_HASH_NBITS); i++)
    slowtable_used[i] = 0;

  /* Build a temp array
   *   huffcode[X] => code used to write vals[X]
   */
  code = 0;
  hc = huffcode;
  hz = huffsize;
  nbits = *hz;
  while (*hz)
   {
     while (*hz == nbits) {
	*hc++ = code++;
	hz++;
     }
     code <<= 1;
     nbits++;
   }

  /*
   * Build the lookup table, and the slowtable if needed.
   */
  for (i=0; huffsize[i]; i++)
   {
     val = vals[i];
     code = huffcode[i];
     code_size = huffsize[i];

     trace("val=%2.2x code=%8.8x codesize=%2.2d\n", i, code, code_size);

     table->code_size[val] = code_size;
     if (code_size <= HUFFMAN_HASH_NBITS)
      {
	/*
	 * Good: val can be put in the lookup table, so fill all value of this
	 * column with value val
	 */
	int repeat = 1UL<<(HUFFMAN_HASH_NBITS - code_size);
	code <<= HUFFMAN_HASH_NBITS - code_size;
	while ( repeat-- )
	  table->lookup[code++] = val;

      }
     else
      {
	/* Perhaps sorting the array will be an optimization */
	int slowtable_index = code_size-HUFFMAN_HASH_NBITS-1;

	if (slowtable_used[slowtable_index] == 254)
	  error("slow Huffman table overflow\n");

	table->slowtable[slowtable_index][slowtable_used[slowtable_index]]
	  = code;
	table->slowtable[slowtable_index][slowtable_used[slowtable_index] + 1]
	  = val;
	slowtable_used[slowtable_index] += 2;
      }
   }

   for (i=0; i<(16-HUFFMAN_HASH_NBITS); i++)
     table->slowtable[i][slowtable_used[i]] = 0;

   return 0;
}

/*******************************************************************************
 *
 * Colorspace conversion routine
 *
 *
 * Note:
 * YCbCr is defined per CCIR 601-1, except that Cb and Cr are
 * normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
 * The conversion equations to be implemented are therefore
 *      R = Y                + 1.40200 * Cr
 *      G = Y - 0.34414 * Cb - 0.71414 * Cr
 *      B = Y + 1.77200 * Cb
 *
 ******************************************************************************/

static unsigned char clamp(int i)
{
  if (i<0)
    return 0;
  else if (i>255)
    return 255;
  else
    return i;
}


/**
 *  YCrCb -> RGB24 (1x1)
 *  .---.
 *  | 1 |
 *  `---'
 */
static void YCrCB_to_RGB24_1x1(struct jdec_private *priv)
{
  const unsigned char *Y, *Cb, *Cr;
  unsigned char *p;
  int i,j;
  int offset_to_next_row;

#define SCALEBITS       10
#define ONE_HALF        (1UL << (SCALEBITS-1))
#define FIX(x)          ((int)((x) * (1UL<<SCALEBITS) + 0.5))

  p = priv->plane[0];
  Y = priv->Y;
  Cb = priv->Cb;
  Cr = priv->Cr;
  offset_to_next_row = priv->width*3 - 8*3;
  for (i=0; i<8; i++) {

    for (j=0; j<8; j++) {

       int y, cb, cr;
       int add_r, add_g, add_b;
       int r, g , b;

       y  = (*Y++) << SCALEBITS;
       cb = *Cb++ - 128;
       cr = *Cr++ - 128;
       add_r = FIX(1.40200) * cr + ONE_HALF;
       add_g = - FIX(0.34414) * cb - FIX(0.71414) * cr + ONE_HALF;
       add_b = FIX(1.77200) * cb + ONE_HALF;

       r = (y + add_r) >> SCALEBITS;
       *p++ = clamp(r);
       g = (y + add_g) >> SCALEBITS;
       *p++ = clamp(g);
       b = (y + add_b) >> SCALEBITS;
       *p++ = clamp(b);

    }

    p += offset_to_next_row;
  }

#undef SCALEBITS
#undef ONE_HALF
#undef FIX

}

/**
 *  YCrCb -> RGB24 (2x2)
 *  .-------.
 *  | 1 | 2 |
 *  |---+---|
 *  | 3 | 4 |
 *  `-------'
 */
static void YCrCB_to_RGB24_2x2(struct jdec_private *priv)
{
  const unsigned char *Y, *Cb, *Cr;
  unsigned char *p, *p2;
  int i,j;
  int offset_to_next_row;

#define SCALEBITS       10
#define ONE_HALF        (1UL << (SCALEBITS-1))
#define FIX(x)          ((int)((x) * (1UL<<SCALEBITS) + 0.5))

  p = priv->plane[0];
  p2 = priv->plane[0] + priv->width*3;
  Y = priv->Y;
  Cb = priv->Cb;
  Cr = priv->Cr;
  offset_to_next_row = (priv->width*3*2) - 16*3;
  for (i=0; i<8; i++) {

    for (j=0; j<8; j++) {

       int y, cb, cr;
       int add_r, add_g, add_b;
       int r, g , b;

       cb = *Cb++ - 128;
       cr = *Cr++ - 128;
       add_r = FIX(1.40200) * cr + ONE_HALF;
       add_g = - FIX(0.34414) * cb - FIX(0.71414) * cr + ONE_HALF;
       add_b = FIX(1.77200) * cb + ONE_HALF;

       y  = (*Y++) << SCALEBITS;
       r = (y + add_r) >> SCALEBITS;
       *p++ = clamp(r);
       g = (y + add_g) >> SCALEBITS;
       *p++ = clamp(g);
       b = (y + add_b) >> SCALEBITS;
       *p++ = clamp(b);

       y  = (*Y++) << SCALEBITS;
       r = (y + add_r) >> SCALEBITS;
       *p++ = clamp(r);
       g = (y + add_g) >> SCALEBITS;
       *p++ = clamp(g);
       b = (y + add_b) >> SCALEBITS;
       *p++ = clamp(b);

       y  = (Y[16-2]) << SCALEBITS;
       r = (y + add_r) >> SCALEBITS;
       *p2++ = clamp(r);
       g = (y + add_g) >> SCALEBITS;
       *p2++ = clamp(g);
       b = (y + add_b) >> SCALEBITS;
       *p2++ = clamp(b);

       y  = (Y[16-1]) << SCALEBITS;
       r = (y + add_r) >> SCALEBITS;
       *p2++ = clamp(r);
       g = (y + add_g) >> SCALEBITS;
       *p2++ = clamp(g);
       b = (y + add_b) >> SCALEBITS;
       *p2++ = clamp(b);
    }
    Y  += 16;
    p  += offset_to_next_row;
    p2 += offset_to_next_row;
  }

#undef SCALEBITS
#undef ONE_HALF
#undef FIX

}

/*
 * Decode all the 3 components for 1x1
 */
static void decode_MCU_1x1_3planes(struct jdec_private *priv, int block_nr)
{
  /* Special ax206 hack, forget remaining bits after each MCU */
  priv->stream -= (priv->nbits_in_reservoir/8);
  priv->nbits_in_reservoir = 0;
  priv->reservoir = 0;

  /* Cb */
  process_Huffman_data_unit(priv, cCb, block_nr);
  IDCT(&priv->component_infos[cCb], priv->Cb, 8);

  /* Cr */
  process_Huffman_data_unit(priv, cCr, block_nr);
  IDCT(&priv->component_infos[cCr], priv->Cr, 8);

  /* Y */
  process_Huffman_data_unit(priv, cY, block_nr);
  IDCT(&priv->component_infos[cY], priv->Y, 8);

}

/*
 * Decode a 2x2
 *  .-------.
 *  | 1 | 2 |
 *  |---+---|
 *  | 3 | 4 |
 *  `-------'
 */
static void decode_MCU_2x2_3planes(struct jdec_private *priv, int block_nr)
{
  /* Special ax206 hack, forget remaining bits after each MCU */
  priv->stream -= (priv->nbits_in_reservoir/8);
  priv->nbits_in_reservoir = 0;
  priv->reservoir = 0;

  /* Cb */
  process_Huffman_data_unit(priv, cCb, 0);
  IDCT(&priv->component_infos[cCb], priv->Cb, 8);

  /* Cr */
  process_Huffman_data_unit(priv, cCr, 0);
  IDCT(&priv->component_infos[cCr], priv->Cr, 8);

  /* Y */
  process_Huffman_data_unit(priv, cY, block_nr);
  IDCT(&priv->component_infos[cY], priv->Y, 16);
  process_Huffman_data_unit(priv, cY, -1);
  IDCT(&priv->component_infos[cY], priv->Y+8, 16);
  process_Huffman_data_unit(priv, cY, -2);
  IDCT(&priv->component_infos[cY], priv->Y+64*2, 16);
  process_Huffman_data_unit(priv, cY, -3);
  IDCT(&priv->component_infos[cY], priv->Y+64*2+8, 16);

}


static void build_quantization_table(float *qtable, const unsigned char *ref_table)
{
  /* Taken from libjpeg. Copyright Independent JPEG Group's LLM idct.
   * For float AA&N IDCT method, divisors are equal to quantization
   * coefficients scaled by scalefactor[row]*scalefactor[col], where
   *   scalefactor[0] = 1
   *   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
   * We apply a further scale factor of 8.
   * What's actually stored is 1/divisor so that the inner loop can
   * use a multiplication rather than a division.
   */
  int i, j;
  static const double aanscalefactor[8] = {
     1.0, 1.387039845, 1.306562965, 1.175875602,
     1.0, 0.785694958, 0.541196100, 0.275899379
  };
  const unsigned char *zz = zigzag;

  for (i=0; i<8; i++) {
     for (j=0; j<8; j++) {
       *qtable++ = ref_table[*zz++] * aanscalefactor[i] * aanscalefactor[j];
     }
   }

}

static int parse_DQT(struct jdec_private *priv, const unsigned char *stream)
{
  int qi;
  float *table;
  const unsigned char *dqt_block_end;

  trace("> DQT marker\n");
  dqt_block_end = stream + be16_to_cpu(stream);
  stream += 2;	/* Skip length */

  while (stream < dqt_block_end)
   {
     qi = *stream++;
#if SANITY_CHECK
     if (qi>>4)
       error("16 bits quantization table is not supported\n");
     if (qi >= COMPONENTS)
       error("No more than %d quantization tables supported (got %d)\n",
	 COMPONENTS, qi + 1);
#endif
     table = priv->Q_tables[qi];
     build_quantization_table(table, stream);
     stream += 64;
   }
  trace("< DQT marker\n");
  return 0;
}

static int parse_DHT(struct jdec_private *priv, const unsigned char *stream)
{
  unsigned int count, i;
  unsigned char huff_bits[17];
  int length, index;

  length = be16_to_cpu(stream) - 2;
  stream += 2;	/* Skip length */

  trace("> DHT marker (length=%d)\n", length);

  while (length>0) {
     index = *stream++;

     /* We need to calculate the number of bytes 'vals' will takes */
     huff_bits[0] = 0;
     count = 0;
     for (i=1; i<17; i++) {
	huff_bits[i] = *stream++;
	count += huff_bits[i];
     }
#if SANITY_CHECK
     if (count > 1024)
       error("No more than 1024 bytes is allowed to describe a huffman table\n");
     if ( (index &0xf) >= HUFFMAN_TABLES)
       error("No mode than %d Huffman tables is supported\n", HUFFMAN_TABLES);
     trace("Huffman table %s n%d\n", (index&0xf0)?"AC":"DC", index&0xf);
     trace("Length of the table: %d\n", count);
#endif

     if (index & 0xf0 ) {
       if (build_huffman_table(priv, huff_bits, stream, &priv->HTAC[index&0xf]))
	 return -1;
     } else {
       if (build_huffman_table(priv, huff_bits, stream, &priv->HTDC[index&0xf]))
	 return -1;
     }

     length -= 1;
     length -= 16;
     length -= count;
     stream += count;
  }
  trace("< DHT marker\n");
  return 0;
}

static void resync(struct jdec_private *priv)
{
  int i;

  /* Init DC coefficients */
  for (i=0; i<COMPONENTS; i++)
     priv->component_infos[i].previous_DC = 0;

  priv->reservoir = 0;
  priv->nbits_in_reservoir = 0;
}

/*******************************************************************************
 *
 * Functions exported of the library.
 *
 * Note: Some applications can access directly to internal pointer of the
 * structure. It's is not recommended, but if you have many images to
 * uncompress with the same parameters, some functions can be called to speedup
 * the decoding.
 *
 ******************************************************************************/

/**
 * Allocate a new tinyjpeg decoder object.
 *
 * Before calling any other functions, an object need to be called.
 */
struct jdec_private *tinyjpeg_init(void)
{
  struct jdec_private *priv;

  priv = (struct jdec_private *)calloc(1, sizeof(struct jdec_private));
  if (priv == NULL)
    return NULL;
  return priv;
}

/**
 * Free a tinyjpeg object.
 *
 * No others function can be called after this one.
 */
void tinyjpeg_free(struct jdec_private *priv)
{
  int i;

  for (i=0; i<COMPONENTS; i++) {
     free(priv->components[i]);
     priv->components[i] = NULL;
  }
  free(priv);
}

/**
 * Initialize the tinyjpeg object and prepare the decoding of the stream.
 *
 * Check if the jpeg can be decoded with this jpeg decoder.
 * Fill some table used for preprocessing.
 */
int tinyjpeg_parse_header(struct jdec_private *priv, const unsigned char *buf, unsigned int size)
{
  const unsigned char *stream = buf;
  struct component *c;
  int i;

  priv->width  = be16_to_cpu(stream);
  priv->height = be16_to_cpu(stream + 2);

#if SANITY_CHECK
  if (priv->width>JPEG_MAX_WIDTH || priv->height>JPEG_MAX_HEIGHT)
    error("Width and Height (%dx%d) seems suspicious\n",
          priv->width, priv->height);
  if (priv->height%8)
    error("Height need to be a multiple of 8 (current height is %d)\n",
          priv->height);
  if (priv->width%8)
    error("Width need to be a multiple of 16 (current width is %d)\n",
          priv->width);
#endif
  trace("size: %dx%d\n", priv->width, priv->height);

  switch (stream[4]) {
  case 0:
    priv->component_infos[cY].Vfactor = 1;
    priv->component_infos[cY].Hfactor = 1;
    break;
  case 3:
    priv->component_infos[cY].Vfactor = 2;
    priv->component_infos[cY].Hfactor = 2;
    break;
  default:
    error("Unknown subsampling identifier: 0x%02x\n", stream[4]);
  }
  priv->component_infos[cCb].Vfactor = 1;
  priv->component_infos[cCb].Hfactor = 1;
  priv->component_infos[cCr].Vfactor = 1;
  priv->component_infos[cCr].Hfactor = 1;

  for (i = 0; i < 3; i++) {
     c = &priv->component_infos[i];

     if (stream[5 + i] != 0 && stream[5 + i] != 1)
       error("Invalid quant table nr: %d\n", stream[5 + i]);
     if (stream[8 + i] != 0 && stream[8 + i] != 1)
       error("Invalid DC huffman table nr: %d\n", stream[8 + i]);
     if (stream[11 + i] != 0 && stream[11 + i] != 1)
       error("Invalid AC huffman table nr: %d\n", stream[11 + i]);

     c->Q_table = priv->Q_tables[stream[5 + i]];
     c->DC_table = &priv->HTDC[stream[8 + i]];
     c->AC_table = &priv->HTAC[stream[11 + i]];
     trace("Component:%d  factor:%dx%d  QT:%d AC:%d DC:%d\n",
	   i, c->Hfactor, c->Hfactor, stream[5 + i], stream[8 + i], stream[11 + i]);
  }

  /* Skip header */
  stream += 16;

  /* Skip MCU info blocks (we don't need them) */
  stream += (priv->width  / (8 * priv->component_infos[cY].Hfactor)) *
            (priv->height / (8 * priv->component_infos[cY].Vfactor)) * 8;

  /* Parse DQT table */
  if (parse_DQT(priv, stream))
    return -1;
  stream += be16_to_cpu(stream);

  /* Parse DHT table */
  if (parse_DHT(priv, stream))
    return -1;
  stream += be16_to_cpu(stream);

  priv->stream = stream;
  priv->stream_end = buf + size;
  return 0;
}

static const decode_MCU_fct decode_mcu_3comp_table[4] = {
   decode_MCU_1x1_3planes,
   decode_MCU_2x2_3planes,
};

static const convert_colorspace_fct convert_colorspace_rgb24[4] = {
   YCrCB_to_RGB24_1x1,
   YCrCB_to_RGB24_2x2,
};

/**
 * Decode and convert the jpeg image into @pixfmt@ image
 *
 * Note: components will be automatically allocated if no memory is attached.
 */
int tinyjpeg_decode(struct jdec_private *priv)
{
  unsigned int x, y, xstride_by_mcu, ystride_by_mcu;
  unsigned int bytes_per_blocklines[3], bytes_per_mcu[3];
  decode_MCU_fct decode_MCU;
  const decode_MCU_fct *decode_mcu_table;
  const convert_colorspace_fct *colorspace_array_conv;
  convert_colorspace_fct convert_to_pixfmt;

  if (setjmp(priv->jump_state))
    return -1;

  /* To keep gcc happy initialize some array */
  bytes_per_mcu[1] = 0;
  bytes_per_mcu[2] = 0;
  bytes_per_blocklines[1] = 0;
  bytes_per_blocklines[2] = 0;

  decode_mcu_table = decode_mcu_3comp_table;
  colorspace_array_conv = convert_colorspace_rgb24;
  if (priv->components[0] == NULL)
    priv->components[0] = (uint8_t *)malloc(priv->width * priv->height * 3);
  bytes_per_blocklines[0] = priv->width * 3;
  bytes_per_mcu[0] = 3*8;

  xstride_by_mcu = ystride_by_mcu = 8;
  if ((priv->component_infos[cY].Hfactor | priv->component_infos[cY].Vfactor) == 1) {
     decode_MCU = decode_mcu_table[0];
     convert_to_pixfmt = colorspace_array_conv[0];
     trace("Use decode 1x1 sampling\n");
  } else if (priv->component_infos[cY].Hfactor == 2 &&
             priv->component_infos[cY].Vfactor == 2) {
     decode_MCU = decode_mcu_table[1];
     convert_to_pixfmt = colorspace_array_conv[1];
     xstride_by_mcu = 16;
     ystride_by_mcu = 16;
     trace("Use decode 2x2 sampling\n");
  } else {
     error("Unknown sub sampling factors: %dx%d\n",
           priv->component_infos[cY].Hfactor,
           priv->component_infos[cY].Vfactor);
  }

  resync(priv);

  /* Don't forget to that block can be either 8 or 16 lines */
  bytes_per_blocklines[0] *= ystride_by_mcu;
  bytes_per_blocklines[1] *= ystride_by_mcu;
  bytes_per_blocklines[2] *= ystride_by_mcu;

  bytes_per_mcu[0] *= xstride_by_mcu/8;
  bytes_per_mcu[1] *= xstride_by_mcu/8;
  bytes_per_mcu[2] *= xstride_by_mcu/8;

  /* Just the decode the image by macroblock (size is 8x8, 8x16, or 16x16) */
  for (y=0; y < priv->height/ystride_by_mcu; y++)
   {
     priv->plane[0] = priv->components[0] + (y * bytes_per_blocklines[0]);
     priv->plane[1] = priv->components[1] + (y * bytes_per_blocklines[1]);
     priv->plane[2] = priv->components[2] + (y * bytes_per_blocklines[2]);
     for (x=0; x < priv->width/xstride_by_mcu; x++)
      {
	decode_MCU(priv, y * priv->width/xstride_by_mcu + x);
	convert_to_pixfmt(priv);
	priv->plane[0] += bytes_per_mcu[0];
	priv->plane[1] += bytes_per_mcu[1];
	priv->plane[2] += bytes_per_mcu[2];
      }
   }

  /* Additional sanity check */
  if ((priv->stream_end - priv->stream) > 1)
    error("Data (%d bytes) remaining after decoding\n",
          (int)(priv->stream_end - priv->stream));

  return 0;
}

const char *tinyjpeg_get_errorstring(struct jdec_private *priv)
{
  return priv->error_string;
}

void tinyjpeg_get_size(struct jdec_private *priv, unsigned int *width, unsigned int *height)
{
  *width = priv->width;
  *height = priv->height;
}

int tinyjpeg_get_components(struct jdec_private *priv, unsigned char **components)
{
  int i;
  for (i=0; i<COMPONENTS && priv->components[i]; i++)
    components[i] = priv->components[i];
  return 0;
}

int tinyjpeg_set_components(struct jdec_private *priv, unsigned char **components, unsigned int ncomponents)
{
  unsigned int i;
  if (ncomponents > COMPONENTS)
    ncomponents = COMPONENTS;
  for (i=0; i<ncomponents; i++)
    priv->components[i] = components[i];
  return 0;
}
