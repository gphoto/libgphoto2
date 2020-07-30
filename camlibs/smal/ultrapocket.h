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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#ifndef __ULTRAPOCKET_H__

#define __ULTRAPOCKET_H__ 1

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>

#ifndef CHECK_RESULT
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#endif

#define UP_FLAG_NEEDS_RESET 0x80

#define DO_GAMMA 1
#define GAMMA_NUMBER 0.5

typedef enum ultrapocket_IMG_TYPE {
   TYPE_QVGA    = 0,
   TYPE_VGA     = 1,
   TYPE_QVGA_BH = 2,
   TYPE_VGA_BH  = 3
} smal_img_type;

int ultrapocket_getpicsoverview(Camera *camera, GPContext *context,int *numpics, CameraList *list);
int ultrapocket_exit(GPPort *port, GPContext *context);
int ultrapocket_getrawpicture(Camera *camera, GPContext *context, unsigned char **pdata, int *size, const char *filename);
int ultrapocket_getpicture(Camera *camera, GPContext *context, unsigned char **pdata, int *size, const char *filename);
int ultrapocket_deletefile(Camera *camera, const char *filename);
int ultrapocket_deleteall(Camera *camera);

#endif
