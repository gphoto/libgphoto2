/*
    pkTriggerCord
    Copyright (C) 2011-2020 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    PK-Remote for Windows
    Copyright (C) 2010 Tomasz Kos

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License
    and GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CAMLIBS_PENTAX_PKTRIGGERCORD_COMMON_H
#define CAMLIBS_PENTAX_PKTRIGGERCORD_COMMON_H

#ifdef RAD10
#include <utime.h>
#else
#include <sys/time.h>
#endif
#include <time.h>
#include <math.h>

#include "pslr_model.h"

double timeval_diff_sec(struct timeval *t2, struct timeval *t1);
void sleep_sec(double sec);
pslr_rational_t parse_shutter_speed(char *shutter_speed_str);
pslr_rational_t parse_aperture(char *aperture_str);

#endif
