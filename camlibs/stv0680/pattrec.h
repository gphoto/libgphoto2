/* 
 * pattrec - demosaic by Y/Cr/Cb with pattern recognition
 * Copyright (C) 2000 Eric Brombaugh <emeb@goodnet.com>
 *
 * Redistributed under the GPL with STV0680 driver by kind permission.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA   
 */

/*
 *  pattrec.h - demosaic by Y/Cr/Cb with pattern recognition
 *  09-02-00 E. Brombaugh
 *
 *  14-12-2000 - trivial modification allowing caller to specify
 *               image size. (Adam Harrison <adam@antispin.org>)
 */

#ifndef _pattrec_
#define _pattrec_

int pattrec(int w, int h, unsigned char *gfx);

#endif
