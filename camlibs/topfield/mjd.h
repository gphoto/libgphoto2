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

#ifndef _MJD_H
#define _MJD_H 1

#include <time.h>
#include <sys/types.h>

/* Encapsulation of MJD date and h:m:s timestamp */
#pragma pack(1)
struct tf_datetime
{
    unsigned short mjd;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
};
#pragma pack()

time_t tfdt_to_time(struct tf_datetime *dt);
void time_to_tfdt(time_t t, struct tf_datetime *dt);

#endif /* _MJD_H */
