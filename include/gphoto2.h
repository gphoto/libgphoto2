/*      Header file for gPhoto 0.5-Dev

        Author: Scott Fritzinger <scottf@unr.edu>

        This library is covered by the LGPL.
*/

#ifndef GPHOTO2_H
#define GPHOTO2_H 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef OS2
#  include <db.h>
#  include <sys/param.h>
#  define CAMLIBS     getenv("CAMLIBS")
#  define RTLD_LAZY   0x001
#  define VERSION     "2"
#  define usleep(t)   _sleep2(((t)+500)/ 1000)
#endif

#ifdef WIN32
#define CAMLIBS "."
#endif

#include <gphoto2-port.h>
#include <gphoto2-camera.h>
#include <gphoto2-core.h>
#include <gphoto2-debug.h>
#include <gphoto2-file.h>
#include <gphoto2-filesys.h>
#include <gphoto2-frontend.h>
#include <gphoto2-library.h>
#include <gphoto2-list.h>
#include <gphoto2-setting.h>
#include <gphoto2-widget.h>


#ifdef __cplusplus
}
#endif

#endif
