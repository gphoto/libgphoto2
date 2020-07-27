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

/* MJD conversions as per Annex C of ETSI EN 300 468 */

#include "mjd.h"
#include "tf_bytes.h"

/* Convert Topfield MJD date and time structure to time_t */
time_t tfdt_to_time(struct tf_datetime * dt)
{
    int mjd = get_u16(&dt->mjd);
    int y, m, d, k;
    struct tm tm;
    time_t result;

    y = (int) ((mjd - 15078.2) / 365.25);
    m = (int) ((mjd - 14956.1 - ((int) (y * 365.25))) / 30.6001);
    d = mjd - 14956 - ((int) (y * 365.25)) - ((int) (m * 30.6001));
    if((m == 14) || (m == 15))
    {
        k = 1;
    }
    else
    {
        k = 0;
    }
    y += k;
    m = m - 1 - k * 12;

    tm.tm_sec = dt->second;
    tm.tm_min = dt->minute;
    tm.tm_hour = dt->hour;
    tm.tm_mday = d;
    tm.tm_mon = m - 1;
    tm.tm_year = y;
    tm.tm_wday = 0;
    tm.tm_yday = 0;
    tm.tm_isdst = -1;

    result = mktime(&tm);
    return result;
}

/* Convert itime_t to Topfield MJD date and time structure */
void time_to_tfdt(time_t t, struct tf_datetime *dt)
{
    int y, m, d, k, mjd;
    struct tm *tm = localtime(&t);

    y = tm->tm_year;
    m = tm->tm_mon + 1;
    d = tm->tm_mday;

    if((m == 1) || (m == 2))
    {
        k = 1;
    }
    else
    {
        k = 0;
    }

    mjd = 14956 + d +
        ((int) ((y - k) * 365.25)) + ((int) ((m + 1 + k * 12) * 30.6001));
    put_u16(&dt->mjd, mjd);
    dt->hour = tm->tm_hour;
    dt->minute = tm->tm_min;
    dt->second = tm->tm_sec;
}
