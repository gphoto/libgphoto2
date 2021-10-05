/*
 * STV0680 Vision Camera Chipset Driver
 * Copyright 2000 Adam Harrison <adam@antispin.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef CAMLIBS_STV0680_BAYER_H
#define CAMLIBS_STV0680_BAYER_H

void bayer_unshuffle_preview(unsigned int w, unsigned int h, unsigned int scale,
			     unsigned char *raw, unsigned char *output);
void light_enhance(unsigned int vw, unsigned int vh, unsigned int coarse, unsigned int fine,
		   unsigned char avg_pix_val, unsigned char *output);

#endif /* !defined(CAMLIBS_STV0680_BAYER_H) */

