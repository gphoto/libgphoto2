/*      Header file for gPhoto 0.5-Dev

        Author: Scott Fritzinger <scottf@unr.edu>

        This library is covered by the LGPL.
*/

#ifndef GPHOTO2_H
#define GPHOTO2_H

#ifdef OS2
#include <gphotoos2.h>
#include <gpio.h>
#else
#include <gpio/gpio.h>
#endif

#include <gphoto2-datatypes.h>
#include <gphoto2-core.h>
#include <gphoto2-library.h>
#include <gphoto2-interface.h>

#endif
