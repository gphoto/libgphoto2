/*      Header file for gPhoto 0.5-Dev

        Author: Scott Fritzinger <scottf@unr.edu>

        This library is covered by the LGPL.
*/

#ifndef GPHOTO2_H
#define GPHOTO2_H

#ifdef OS2
#include <gphotoos2.h>
#endif

#ifdef WIN32
#define CAMLIBS "."
#endif

#include <gpio.h>
#include <gphoto2-datatypes.h>
#include <gphoto2-camera.h>
#include <gphoto2-core.h>
/* #include <gphoto2-dynamic.h> Not Yet   -Scott */
#include <gphoto2-file.h>
#include <gphoto2-filesys.h>
#include <gphoto2-frontend.h>
#include <gphoto2-library.h>
#include <gphoto2-lists.h>
#include <gphoto2-port.h>
#include <gphoto2-settings.h>
#include <gphoto2-widget.h>

#endif
