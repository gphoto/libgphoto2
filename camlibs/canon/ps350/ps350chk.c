/* Canon PowerShot 350 Checksum Calculation

   Copyright (C) 1999 J.A. Bezemer

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* This is really simple. It only took quite some time to figure out
   its simplicity ;-)

   The `electronic' structure is this:

   In
   --->X<----------------------------------------------------------------.
       |                                                                 |
       +--------------------+---------------------------.                |
       |                    |                           |                |
       |                    v                           v                |
       `->T-->T-->T-->T-->T-X>T-->T-->T-->T-->T-->T-->T-X>T-->T-->T-->T--'
          15  14  13  12  11  10  9   8   7   6   5   4   3   2   1   0

   The three X's are XOR gates; T's are 1-bit memory elements and the rest
   are wires with arrows indicating the direction of the logic signals.
   Bits are fed into the input (`In') LSBit first. When all bytes are
   fed in, the T's hold the checksum (bit numbers 15 through 0 as
   indicated). Only the 296 `real data' bytes are fed in (thus excluding
   the 0xC0 header) and initialization is 0x0000.

   In other words: this is just a standard CRC-CCITT algorithm. But I
   only saw that when I drew the schematic. There are quite a few
   programs available that calculate this CRC, but I didn't understand
   most of them, so I wrote my own version for some experiments. (And most
   implementations that I've seen didn't have a clear copyright...) 

   I implemented the 16-stage shift register in a `long' variable
   that is shifted and XOR'd when necessary.

   Note that this is only a sample implementation that is probably very
   slow. However, it should be fast enough to use it with 300-byte
   packets at 115200 baud.

   This program uses hard-coded data for easy experimentation. Two sample
   packets are provided. Of course, this must be made into a proper
   function to be usable.
 */

#include <stdio.h>

void
main (void)
{
  long datalen = 296;		/* number of data bytes. Always 296. */

  char data[] =
  {0x05, 0x03, 0x04, 0x00, 0x01,
/* this is a EOT packet, count=5. The checksum is "87 3F" (LSByte-first!) */

/* a few 0's to fill up the packet (far too much 0's actually, but that's
   no problem) */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


/* from a serial snoop: jut some arbitrary packet from a JPG download. */
/* rename this to `data' and the original `data' to `data1' to try it. */
  char data1[] =
  {
  /* 0x3f1e *//* 0xC0, */ 0x05,
  /* 0x3f20 */ 0x10, 0x24, 0x81, 0x4d, 0xf6, 0xa7, 0x52, 0xbd,
  /* 0x3f28 */ 0x83, 0xec, 0xe9, 0x88, 0xbc, 0xcc, 0x0f, 0x90,
  /* 0x3f30 */ 0xf4, 0x99, 0x97, 0xb0, 0xf6, 0xae, 0xa3, 0x9b,
  /* 0x3f38 */ 0x99, 0x98, 0xb6, 0xf7, 0xfb, 0x55, 0x8b, 0xbe,
  /* 0x3f40 */ 0x23, 0x68, 0xf3, 0xc7, 0xad, 0x72, 0xe6, 0xf2,
  /* 0x3f48 */ 0x1b, 0x88, 0xdf, 0x9c, 0x1e, 0xb4, 0xa3, 0x1d,
  /* 0x3f50 */ 0x59, 0xac, 0xe5, 0xa9, 0xdd, 0xe8, 0x97, 0x65,
  /* 0x3f58 */ 0x63, 0xd9, 0xe6, 0x7c, 0xb8, 0xad, 0x2b, 0x8b,
  /* 0x3f60 */ 0xce, 0x3f, 0xa5, 0x73, 0x3d, 0xcd, 0x8c, 0xc9,
  /* 0x3f68 */ 0x65, 0xbb, 0xfb, 0x3f, 0x99, 0x1d, 0xc4, 0xa0,
  /* 0x3f70 */ 0x15, 0x3c, 0x29, 0xf4, 0xc7, 0xbf, 0xbd, 0x32,
  /* 0x3f78 */ 0xfa, 0xd6, 0xf6, 0xde, 0x29, 0x0f, 0xf6, 0x9d,
  /* 0x3f80 */ 0xe4, 0xbf, 0x7b, 0xfd, 0x58, 0x27, 0xa2, 0xb3,
  /* 0x3f88 */ 0x7f, 0x7b, 0xfd, 0x9f, 0xd6, 0xb7, 0x80, 0x4a,
  /* 0x3f90 */ 0x71, 0xb6, 0xa8, 0xdd, 0xd1, 0xe0, 0x31, 0xe8,
  /* 0x3f98 */ 0x0c, 0xc3, 0xed, 0xb7, 0x33, 0xc2, 0x0b, 0x71,
  /* 0x3fa0 */ 0xbb, 0xe6, 0xff, 0x00, 0x67, 0x83, 0xd6, 0x9d,
  /* 0x3fa8 */ 0xb0, 0xde, 0x68, 0xcf, 0x25, 0xd5, 0xbd, 0xf5,
  /* 0x3fb0 */ 0x85, 0xc4, 0xaa, 0x53, 0x6c, 0x9b, 0xb3, 0x1f,
  /* 0x3fb8 */ 0xd7, 0x35, 0xc9, 0x3d, 0x64, 0xf5, 0x34, 0x53,
  /* 0x3fc0 */ 0x56, 0x5a, 0x1c, 0xb4, 0x30, 0x6a, 0x37, 0x1c,
  /* 0x3fc8 */ 0x0d, 0x52, 0xfa, 0x3c, 0x7f, 0x7f, 0x23, 0xbb,
  /* 0x3fd0 */ 0x8f, 0xef, 0x7f, 0xb2, 0x7f, 0xef, 0xaa, 0x82,
  /* 0x3fd8 */ 0xe1, 0x6f, 0xfc, 0xdb, 0x88, 0x5f, 0x52, 0xbb,
  /* 0x3fe0 */ 0xf9, 0x37, 0x7c, 0xcc, 0x4e, 0x1b, 0x0e, 0x57,
  /* 0x3fe8 */ 0xfb, 0xde, 0xd5, 0xe8, 0x9c, 0x7e, 0xd2, 0x1d,
  /* 0x3ff0 */ 0x8e, 0x46, 0x6b, 0xc9, 0x3e, 0x62, 0x64, 0xce,
  /* 0x3ff8 */ 0x17, 0xb9, 0xed, 0x5c, 0xaa, 0xdc, 0xb8, 0x47,
  /* 0x4000 */ 0x22, 0xba, 0x60, 0x8c, 0x31, 0x0e, 0xcc, 0xf4,
  /* 0x4008 */ 0x5d, 0x2e, 0xf0, 0x06, 0x45, 0xdd, 0xc7, 0x7a,
  /* 0x4010 */ 0xda, 0xbb, 0xbb, 0xc4, 0x13, 0x30, 0x2c, 0xc5,
  /* 0x4018 */ 0x57, 0x27, 0x00, 0xf1, 0x9e, 0x95, 0xc1, 0x34,
  /* 0x4020 */ 0x74, 0xa6, 0x32, 0xea, 0xfa, 0xca, 0x4d, 0x21,
  /* 0x4028 */ 0x55, 0x65, 0x5f, 0x33, 0x04, 0x00, 0x47, 0x5e,
  /* 0x4030 */ 0x63, 0xcf, 0x24, 0x7b, 0x51, 0xa8, 0x6b, 0x16,
  /* 0x4038 */ 0xf7, 0x31, 0x4b, 0x1d, 0xad, 0xc2, 0x33, 0x37,
  /* 0x4040 */ 0x98, 0xa7, 0x2c, 0x07, 0x58, 0x9c, 0x77, 0x6e,
  /* 0x4048 */ 0xc7, 0xc1};
/* note: last 3 bytes are chksum and trailer */

  char bits[] =
  {1, 2, 4, 8, 16, 32, 64, 128};
  long chksum = 0;
  long i, j;
  int inputbit, lastbit, xorbit;

/* calculate checksum */
  for (i = 0; i < datalen; i++)
    for (j = 0; j <= 7; j++)
      {
	lastbit = chksum & 0x0001;	/* remember if last bit was 1 */
	chksum >>= 1;		/* shift 1 place */
	inputbit = data[i] & bits[j] ? 1 : 0;	/* the In bit */
	xorbit = inputbit ^ lastbit;	/* output of upper XOR gate */
	if (xorbit)		/* XOR if needed */
	  chksum ^= 0x8408;
      }

/* display checksum */
  printf ("Checksum: 0x%lX\n", chksum);
}
