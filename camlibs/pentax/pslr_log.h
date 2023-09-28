/*
    pkTriggerCord
    Copyright (C) 2011-2020 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    Support for K200D added by Jens Dreyer <jens.dreyer@udo.edu> 04/2011
    Support for K-r added by Vincenc Podobnik <vincenc.podobnik@gmail.com> 06/2011
    Support for K-30 added by Camilo Polymeris <cpolymeris@gmail.com> 09/2012
    Support for K-01 added by Ethan Queen <ethanqueen@gmail.com> 01/2013
    Support for K-3 added by Tao Wang <twang2218@gmail.com> 01/2016

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

#ifndef CAMLIBS_PENTAX_PSLR_LOG_H
#define CAMLIBS_PENTAX_PSLR_LOG_H

#ifdef ANDROID
#include <android/log.h>
#define DPRINT(...) __android_log_print(ANDROID_LOG_DEBUG, "PkTriggerCord", __VA_ARGS__)
#else
#ifdef LIBGPHOTO
#include <gphoto2/gphoto2-log.h>
#define DPRINT(x...) gp_log (GP_LOG_DEBUG, "pentax", x)
#else
#define DPRINT(x...) pslr_write_log(PSLR_DEBUG, x)
#endif
#endif

#define PSLR_DEBUG_ENABLED pslr_verbosity_enabled(PSLR_DEBUG)

#include <stdbool.h>

typedef enum {
    PSLR_DEBUG,
    PSLR_WARNING,
    PSLR_ERROR,
    PSLR_SILENT,
} pslr_verbosity_t;

extern pslr_verbosity_t verbosity_level;

void pslr_set_verbosity(pslr_verbosity_t verbosity);
pslr_verbosity_t pslr_get_verbosity();

bool pslr_verbosity_enabled(pslr_verbosity_t level);

//TODO : offer a write_log_callback equivalent so a caller can fully configure what happends when pslr_write_log is called ?
void pslr_write_log(pslr_verbosity_t level, const char* message, ...);

#endif
