/*

  Copyright (C) 2004 Peter Urbanec <toppy at urbanec.net>

  This file is part of puppy.

  puppy is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  puppy is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with puppy; if not, write to the
  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA  02110-1301  USA

*/

#include <stdio.h>
#include "tf_bytes.h"

/* The Topfield packet handling is a bit unusual. All data is stored in
 * memory in big endian order, however, just prior to transmission all
 * data is byte swapped.
 *
 * We try to handle this in here in a some kind of a portable manner.
 *
 * We provide functions to read and write the memory version of packets
 * under the name get_X() and put_X().
 *
 * The USB I/O layer then takes care of CRC generation and byte swapping.
 */

unsigned short get_u16(void *addr)
{
    unsigned char *b = addr;

    return ((b[0] << 8) | (b[1] << 0));
}

/* Retrieve a 16-bit integer from the raw buffer (prior to byteswapping) */
unsigned short get_u16_raw(void *addr)
{
    unsigned char *b = addr;

    return ((b[1] << 8) | (b[0] << 0));
}

void put_u16(void *addr, unsigned short val)
{
    unsigned char *b = addr;

    b[0] = (val >> 8) & 0xFF;
    b[1] = (val & 0xFF);
}

unsigned int get_u32(void *addr)
{
    unsigned char *b = addr;

    return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | (b[3]);
}

/* Retrieve a 32-bit integer from the raw buffer (prior to byteswapping) */
unsigned int get_u32_raw(void *addr)
{
    unsigned char *b = addr;

    return (b[1] << 24) | (b[0] << 16) | (b[3] << 8) | (b[2]);
}

void put_u32(void *addr, unsigned int val)
{
    unsigned char *b = addr;

    b[0] = (val >> 24) & 0xFF;
    b[1] = (val >> 16) & 0xFF;
    b[2] = (val >> 8) & 0xFF;
    b[3] = (val & 0xFF);
}

uint64_t get_u64(void *addr)
{
    unsigned char *b = addr;
    uint64_t r = b[0];

    r = (r << 8) | b[1];
    r = (r << 8) | b[2];
    r = (r << 8) | b[3];
    r = (r << 8) | b[4];
    r = (r << 8) | b[5];
    r = (r << 8) | b[6];
    r = (r << 8) | b[7];
    return r;
}

void put_u64(void *addr, uint64_t val)
{
    unsigned char *b = addr;

    b[0] = (val >> 56) & 0xFF;
    b[1] = (val >> 48) & 0xFF;
    b[2] = (val >> 40) & 0xFF;
    b[3] = (val >> 32) & 0xFF;
    b[4] = (val >> 24) & 0xFF;
    b[5] = (val >> 16) & 0xFF;
    b[6] = (val >> 8) & 0xFF;
    b[7] = (val & 0xFF);
}
