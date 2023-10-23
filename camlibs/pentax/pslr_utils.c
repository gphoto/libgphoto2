/*
    pkTriggerCord
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2011-2020 Andras Salamon <andras.salamon@melda.info>

    based on:

    pslr-shoot

    Command line remote control of Pentax DSLR cameras.
    Copyright (C) 2009 Ramiro Barreiro <ramiro_barreiro69@yahoo.es>
    With fragments of code from PK-Remote by Pontus Lidman.
    <https://sourceforge.net/projects/pkremote>

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

#ifdef RAD10
#include <windows.h>
#include <utime.h>
#include "tdbtimes.h"
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include "pslr.h"
#include "pslr_utils.h"

double timeval_diff_sec(struct timeval *t2, struct timeval *t1) {
    //DPRINT("tv2 %ld %ld t1 %ld %ld\n", t2->tv_sec, t2->tv_usec, t1->tv_sec, t1->tv_usec);
    return (t2->tv_usec - t1->tv_usec) / 1000000.0 + (t2->tv_sec - t1->tv_sec);
}

void sleep_sec(double sec) {
    int i;
    for (i=0; i<floor(sec); ++i) {
        usleep(999999); // 1000000 is not working for Windows
    }
    usleep(1000000*(sec-floor(sec)));
}

pslr_rational_t parse_aperture(char *aperture_str) {
    char C;
    float F = 0;
    pslr_rational_t aperture = {0, 0};
    if (sscanf(aperture_str, "%f%c", &F, &C) != 1) {
        F = 0;
    }
    /*It's unlikely that you want an f-number > 100, even for a pinhole.
     On the other hand, the fastest lens I know of is a f:0.8 Zeiss*/
    if (F > 100 || F < 0.8) {
        F = 0;
    }
    aperture.nom = F * 10;
    aperture.denom = 10;

    return aperture;
}

pslr_rational_t parse_shutter_speed(char *shutter_speed_str) {
    char C;
    float F = 0;
    pslr_rational_t shutter_speed = {0, 0};
    if (sscanf(shutter_speed_str, "%d/%d%c", &shutter_speed.nom, &shutter_speed.denom, &C) == 2) {
        // noop
    } else if ((sscanf(shutter_speed_str, "%d%c", &shutter_speed.nom, &C)) == 1) {
        shutter_speed.denom = 1;
    } else if ((sscanf(shutter_speed_str, "%f%c", &F, &C)) == 1) {
        F = F * 1000;
        shutter_speed.denom = 1000;
        shutter_speed.nom = F;
    } else {
        shutter_speed.nom = 0;
        shutter_speed.denom = 0;
    }
    return shutter_speed;
}
