/*
 * img_enhance.h
 *
 * Header file for image processor for raw files from JL2005B/C/D
 * cameras.
 *
 * Copyright (c) 2010 Theodore Kilgore <kilgota@auburn.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __IMG_ENHANCE_H__
#define __IMG_ENHANCE_H__


int
histogram(unsigned char *data, unsigned int size, int *htable_r,
						int *htable_g, int *htable_b);
int
white_balance(unsigned char *data, unsigned int size, float saturation);

#endif
