/* ricoh.h
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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

#ifndef __RICOH_H__
#define __RICOH_H__

#include <time.h>

#include <gphoto2-camera.h>
#include <gphoto2-context.h>

typedef enum _RicohSpeed RicohSpeed;
enum _RicohSpeed {
	RICOH_SPEED_2400   = 0x00,
	RICOH_SPEED_4800   = 0x01,
	RICOH_SPEED_9600   = 0x02,
	RICOH_SPEED_19200  = 0x03,
	RICOH_SPEED_38400  = 0x04,
	RICOH_SPEED_57600  = 0x05,
	RICOH_SPEED_115200 = 0x07
};

int ricoh_set_speed (Camera *camera, GPContext *context, RicohSpeed speed);

typedef enum _RicohModel RicohModel;
enum _RicohModel {
	RICOH_MODEL_300  = 0x0300,
	RICOH_MODEL_300Z = 0x0301,
	RICOH_MODEL_4300 = 0x0400
};

int ricoh_ping      (Camera *camera, GPContext *context, RicohModel *model);
int ricoh_bye       (Camera *camera, GPContext *context);

typedef enum _RicohMode RicohMode;
enum _RicohMode {
	RICOH_MODE_PLAY   = 0x00,
	RICOH_MODE_RECORD = 0x01
};

int ricoh_get_mode  (Camera *camera, GPContext *context, RicohMode *mode);
int ricoh_set_mode  (Camera *camera, GPContext *context, RicohMode  mode);

int ricoh_get_num   (Camera *camera, GPContext *context, unsigned int *n);
int ricoh_get_size  (Camera *camera, GPContext *context, unsigned int n,
		     unsigned long *size);
int ricoh_get_date  (Camera *camera, GPContext *context, unsigned int n,
		     time_t *date);
int ricoh_del_pic   (Camera *camera, GPContext *context, unsigned int n);
int ricoh_get_pic   (Camera *camera, GPContext *context, unsigned int n,
		     unsigned char **data, unsigned int *size);
int ricoh_get_cam_date  (Camera *camera, GPContext *context, time_t *time);
int ricoh_get_cam_mem   (Camera *camera, GPContext *context, int *mem);
int ricoh_get_cam_amem  (Camera *camera, GPContext *context, int *mem);
int ricoh_get_cam_id    (Camera *camera, GPContext *context, char *cam_id);


#endif /* __RICOH_H__ */
