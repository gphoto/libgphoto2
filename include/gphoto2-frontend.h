/* gphoto2-frontend.h
 *
 * Copyright (C) 2000 Scott Fritzinger
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

#ifndef __GPHOTO2_FRONTEND_H__
#define __GPHOTO2_FRONTEND_H__

#include <gphoto2-camera.h>
#include <gphoto2-file.h>
#include <gphoto2-widget.h>

typedef enum {
	GP_CONFIRM_YES,
	GP_CONFIRM_YESTOALL,
	GP_CONFIRM_NO,
	GP_CONFIRM_NOTOALL
} CameraConfirmValue;

typedef int (* CameraStatus)             (Camera *, char *);
typedef int (* CameraMessage)            (Camera *, char *);
typedef int (* CameraConfirm)            (Camera *, char *);
typedef int (* CameraProgress)           (Camera *, CameraFile *, float);

int  gp_frontend_register (CameraStatus status, CameraProgress progress,
			   CameraMessage message, CameraConfirm confirm,
			   void *dummy);

int gp_frontend_status   (Camera *camera, char *status);
int gp_frontend_message  (Camera *camera, char *message);
int gp_frontend_confirm  (Camera *camera, char *message);
int gp_frontend_progress (Camera *camera, CameraFile *file, float percentage);

#endif /* __GPHOTO2_FRONTEND_H__ */
