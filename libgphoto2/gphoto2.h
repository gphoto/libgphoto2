/* gphoto2.h: Convenience header for gphoto2
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GPHOTO2_H__
#define __GPHOTO2_H__

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
#include <gphoto2-port-log.h>
#include <gphoto2-port-result.h>

#include <gphoto2-result.h>
#include <gphoto2-camera.h>
#include <gphoto2-file.h>
#include <gphoto2-filesys.h>
#include <gphoto2-library.h>
#include <gphoto2-list.h>
#include <gphoto2-setting.h>
#include <gphoto2-widget.h>

#ifdef __cplusplus
}
#endif

#endif /* __GPHOTO2_H__ */
