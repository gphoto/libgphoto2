/* library.h
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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

#ifndef __LIBRARY_H__
#define __LIBRARY_H__

#include <gphoto2-camera.h>

typedef enum {
	SIERRA_LOCKED_NO  = 0x00,
	SIERRA_LOCKED_YES = 0x01
} SierraLocked;

typedef struct _SierraPicInfo SierraPicInfo;
struct _SierraPicInfo {
	unsigned int size_file;
	unsigned int size_preview;
	unsigned int size_audio;
	unsigned int resolution;
	SierraLocked locked;
	unsigned int date;
	unsigned int animation_type;
};
int sierra_get_pic_info (Camera *camera, unsigned int n,
			 SierraPicInfo *pic_info, GPContext *context);
int sierra_set_locked (Camera *camera, unsigned int n, SierraLocked locked,
		       GPContext *context);

/* Communications functions */
int sierra_set_speed		  (Camera *camera, int speed,
				   GPContext *context);
int sierra_end_session		  (Camera *camera, GPContext *context);
int sierra_init			  (Camera *camera, GPContext *context);
int sierra_get_memory_left        (Camera *camera, int *memory,
				   GPContext *context);
int sierra_check_battery_capacity (Camera *camera, GPContext *context);
int sierra_set_int_register 	  (Camera *camera, int reg, int value,
				   GPContext *context);
int sierra_get_int_register 	  (Camera *camera, int reg, int *value,
				   GPContext *context);
int sierra_set_string_register	  (Camera *camera, int reg, const char *s,
				   long int length, GPContext *context);
int sierra_get_string_register	  (Camera *camera, int reg, int file_number,
				   CameraFile *file,
				   unsigned char *b, unsigned int *b_len,
				   GPContext *context);
int sierra_delete		  (Camera *camera, int picture_number,
				   GPContext *context);
int sierra_delete_all             (Camera *camera, GPContext *context);
int sierra_capture		  (Camera *camera, CameraCaptureType type, 
				   CameraFilePath *filepath,
				   GPContext *context);
int sierra_capture_preview 	  (Camera *camera, CameraFile *file,
				   GPContext *context);
int sierra_change_folder          (Camera *camera, const char *folder,
				   GPContext *context);

/* Filesystem functions */
int sierra_list_files         (Camera *camera, const char *folder, CameraList *list, GPContext *context);
int sierra_list_folders       (Camera *camera, const char *folder, CameraList *list, GPContext *context);
int sierra_get_picture_folder (Camera *camera, char **folder);
int sierra_upload_file        (Camera *camera, CameraFile *file,
			       GPContext *context);

#endif /* __LIBRARY_H__ */
