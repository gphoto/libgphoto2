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
  along with puppy; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef _TF_BYTES_H
#define _TF_BYTES_H 1

#include <asm/types.h>

unsigned short get_u16(void *addr);
unsigned short get_u16_raw(void *addr);
unsigned int get_u32(void *addr);
unsigned int get_u32_raw(void *addr);
__u64 get_u64(void *addr);

void put_u16(void *addr, unsigned short val);
void put_u32(void *addr, unsigned int val);
void put_u64(void *addr, __u64 val);

#endif /* _TF_BYTES_H */
