/* ultrapocket.h
 *
 * Copyright (C) 2003 Lee Benfield <lee@benf.org>
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
#ifndef __ULTRAPOCKET_H__

#define __ULTRAPOCKET_H__ 1

#include <gphoto2-library.h>
#include <gphoto2-result.h>

#ifndef CHECK_RESULT
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#endif

#define UP_FLAG_NEEDS_RESET 0x80

/* The windows driver downloads a bunch of info from the camera the use of
 * which I can't see.  Unless you want it, don't get it.
 */
#define COPY_WINDOWS_DRIVER 0
/* The windows software does a _lot_ of touching up of the images.
 * For now, until I figure out what that involves, just do a bit of gamma 
 * correction
 */
#define DO_GAMMA 0 

typedef enum ultrapocket_IMG_TYPE {
   TYPE_QVGA    = 0,
   TYPE_VGA     = 1,
   TYPE_QVGA_BH = 2,
   TYPE_VGA_BH  = 3
} smal_img_type;

int ultrapocket_getpicsoverview(GPPort **pport, GPContext *context,int *numpics, CameraList *list);
int ultrapocket_exit(GPPort *port, GPContext *context);
int ultrapocket_getpicture(GPPort *port, GPContext *context, unsigned char **pdata, int *size, const char *filename);
int ultrapocket_deletefile(GPPort *port, const char *filename);
int ultrapocket_deleteall(GPPort **pport);

#endif
