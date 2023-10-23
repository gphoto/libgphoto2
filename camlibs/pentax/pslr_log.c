/*
    pkTriggerCord
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2011-2020 Andras Salamon <andras.salamon@melda.info>

    which is based on:

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

#include <stdio.h>
#include <stdarg.h>

#include "pslr_log.h"

pslr_verbosity_t verbosity_level = PSLR_ERROR;

void pslr_set_verbosity(pslr_verbosity_t verbosity) {
    verbosity_level = verbosity;
}

pslr_verbosity_t pslr_get_verbosity() {
    return verbosity_level;
}

bool pslr_verbosity_enabled(pslr_verbosity_t level) {
    return level >= verbosity_level;
}

void pslr_write_log(pslr_verbosity_t level, const char* message, ...) {
    // immediatly returns for disabled log levels
    if (!pslr_verbosity_enabled(level)) {
        return;
    }

    // Write to stderr
    va_list argp;
    va_start(argp, message);
    vfprintf( stderr, message, argp );
    va_end(argp);
}