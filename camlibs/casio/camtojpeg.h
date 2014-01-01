/* camtojpeg.h
 *
 * Copyright (c) 2004 Michael Haardt
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

#ifndef __CASIO_QV_CAMTOJPEG_H__
#define __CASIO_QV_CAMTOJPEG_H__

int QVcamtojpeg(const unsigned char *cam, long int camSize, unsigned char **jpeg, long int *jpegSize);
int QVfinecamtojpeg(const unsigned char *cam, long int camSize, unsigned char **jpeg, long int *jpegSize);

#endif
