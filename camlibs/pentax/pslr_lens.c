/*
    pkTriggerCord
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    Pentax lens database comes from ExifTool ( http://www.sno.phy.queensu.ca/~phil/exiftool/ )

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

#include "pslr_lens.h"

#include <stdio.h>

static const struct {
    uint32_t id1;
    uint32_t id2;
    const char *name;
} lens_id[] = {
#include "exiftool_pentax_lens.txt"
};

const char *get_lens_name( uint32_t id1, uint32_t id2) {
    int lens_num = sizeof(lens_id)/sizeof(lens_id[0]);
    int i;
    for ( i=0; i<lens_num; ++i ) {
        if ( lens_id[i].id1 == id1 &&
                lens_id[i].id2 == id2 ) {
            return lens_id[i].name;
        }
    }
    return "";
}
