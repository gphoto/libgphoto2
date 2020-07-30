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

#ifndef _TF_BYTES_H
#define _TF_BYTES_H 1

#include <_stdint.h>

unsigned short get_u16(void *addr);
unsigned short get_u16_raw(void *addr);
unsigned int get_u32(void *addr);
unsigned int get_u32_raw(void *addr);
uint64_t get_u64(void *addr);

void put_u16(void *addr, unsigned short val);
void put_u32(void *addr, unsigned int val);
void put_u64(void *addr, uint64_t val);

#endif /* _TF_BYTES_H */
