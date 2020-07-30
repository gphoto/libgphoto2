/* library.h
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sf.net>
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

#ifndef __LIBRARY_H__
#define __LIBRARY_H__

#include <gphoto2/gphoto2-camera.h>

#define TIMEOUT	   2000

#define CHECK_STOP(camera,result)				\
{								\
	int res = (result);					\
								\
	if (res < 0) {						\
		GP_DEBUG ("Operation failed in %s (%i)!", __FUNCTION__, res);	\
		camera_stop (camera, context);			\
		return (res);					\
	}							\
}

#define CHECK_STOP_FREE(camera,result)				\
{								\
	int res = (result);					\
								\
	if (res < 0) {						\
		GP_DEBUG ("Operation failed in %s (%i)!", __FUNCTION__, res);	\
		camera_stop (camera, context);			\
		free (camera->pl);				\
		camera->pl = NULL;				\
		return (res);					\
	}							\
}

#define CHECK_FREE(camera,result)				\
{								\
	int res = (result);					\
								\
	if (res < 0) {						\
		GP_DEBUG ("Operation failed in %s (%i)!", __FUNCTION__, res);	\
		free (camera->pl);				\
		camera->pl = NULL;				\
		return (res);					\
	}							\
}

enum _SierraAction {
	SIERRA_ACTION_DELETE_LAST_PIC = 0x00,
	SIERRA_ACTION_DELETE_ALL = 0x01,
	SIERRA_ACTION_CAPTURE    = 0x02,
	SIERRA_ACTION_END        = 0x04,
	SIERRA_ACTION_PREVIEW    = 0x05,
	SIERRA_ACTION_TESTING    = 0x06,
	SIERRA_ACTION_DELETE     = 0x07,
	SIERRA_ACTION_LCD_MODE   = 0x08,
	SIERRA_ACTION_PROT_STATE = 0x09,
	SIERRA_ACTION_UPLOAD     = 0x0b,
	SIERRA_ACTION_LCD_TEST   = 0x0c,
};
typedef enum _SierraAction SierraAction;

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
enum _SierraSpeed {
	SIERRA_SPEED_9600   = 1,
	SIERRA_SPEED_19200  = 2,
	SIERRA_SPEED_38400  = 3,
	SIERRA_SPEED_57600  = 4,
	SIERRA_SPEED_115200 = 5
};
typedef enum _SierraSpeed SierraSpeed;
int sierra_set_speed		  (Camera *camera, SierraSpeed speed,
				   GPContext *context);
int sierra_end_session		  (Camera *camera, GPContext *context);
int sierra_init			  (Camera *camera, GPContext *context);
int sierra_get_memory_left        (Camera *camera, int *memory,
				   GPContext *context);
int sierra_check_battery_capacity (Camera *camera, GPContext *context);
int sierra_sub_action		  (Camera *camera, SierraAction action,
				  int sub_action, GPContext *context);
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
int sierra_get_size		  (Camera *camera, int reg, unsigned int n,
				   int *value, GPContext *context);
int camera_start		  (Camera *camera, GPContext *context);
int camera_stop			  (Camera *camera, GPContext *context);

/* Filesystem functions */
int sierra_list_files         (Camera *camera, const char *folder, CameraList *list, GPContext *context);
int sierra_list_folders       (Camera *camera, const char *folder, CameraList *list, GPContext *context);
int sierra_get_picture_folder (Camera *camera, char **folder);
int sierra_upload_file        (Camera *camera, CameraFile *file,
			       GPContext *context);

/* Camera desc functions in sierra-desc.c */
int camera_set_config_cam_desc	  (Camera *camera, CameraWidget *window,
				   GPContext *context);
int camera_get_config_cam_desc	  (Camera *camera, CameraWidget **window,
				   GPContext *context);
#endif /* __LIBRARY_H__ */
