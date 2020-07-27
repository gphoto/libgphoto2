/*
 * Jenopt JD11 Camera Driver
 * Copyright 1999-2001 Marcus Meissner <marcus@jet.franken.de>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef _JD11_DECOMP_H
#define _JD11_DECOMP_H
extern void picture_decomp_v1(unsigned char *compressed,unsigned char *uncompressed,int width,int height);
extern void picture_decomp_v2(unsigned char *compressed,unsigned char *uncompressed,int width,int height);
#endif
