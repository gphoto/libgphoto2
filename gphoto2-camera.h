/* gphoto2-camera.h
 *
 * Copyright © 2000 Scott Fritzinger
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

#ifndef __GPHOTO2_CAMERA_H__
#define __GPHOTO2_CAMERA_H__

#include <gphoto2-context.h>

typedef struct _Camera Camera;

#include <gphoto2-port.h>
#include <gphoto2-port-info-list.h>

#include <gphoto2-widget.h>
#include <gphoto2-list.h>
#include <gphoto2-file.h>
#include <gphoto2-filesys.h>
#include <gphoto2-abilities-list.h>
#include <gphoto2-result.h>
#include <gphoto2-context.h>

typedef struct {
	char text [32 * 1024];
} CameraText;

typedef struct {
	char name [128];
	char folder [1024];
} CameraFilePath;

typedef enum {
	GP_CAPTURE_IMAGE,
	GP_CAPTURE_MOVIE,
	GP_CAPTURE_SOUND
} CameraCaptureType;

typedef int (*CameraExitFunc)      (Camera *camera, GPContext *context);
typedef int (*CameraGetConfigFunc) (Camera *camera, CameraWidget **widget,
				    GPContext *context);
typedef int (*CameraSetConfigFunc) (Camera *camera, CameraWidget  *widget,
				    GPContext *context);
typedef int (*CameraCaptureFunc)   (Camera *camera, CameraCaptureType type,
				    CameraFilePath *path, GPContext *context);
typedef int (*CameraCapturePreviewFunc) (Camera *camera, CameraFile *file,
					 GPContext *context);
typedef int (*CameraSummaryFunc)   (Camera *camera, CameraText *text,
				    GPContext *context);
typedef int (*CameraManualFunc)    (Camera *camera, CameraText *text,
				    GPContext *context);
typedef int (*CameraAboutFunc)     (Camera *camera, CameraText *text,
				    GPContext *context);

/**
 * CameraPrePostFunc:
 * @camera: a #Camera
 *
 * Implement this function in the camera driver if the camera needs to
 * be initialized before or reset the after each access from libgphoto2.
 * For example, you would probably set the speed to the highest one 
 * right before downloading an image, and reset it to the default speed 
 * afterwards so that other programs will not be affected by this speed
 * change.
 *
 * Return value: a gphoto2 error code
 **/
typedef int (*CameraPrePostFunc) (Camera *camera, GPContext *context);

typedef struct _CameraFunctions CameraFunctions;
struct _CameraFunctions {

	/* Those will be called before and after each operation */
	CameraPrePostFunc pre_func;
	CameraPrePostFunc post_func;

	CameraExitFunc exit;

	/* Configuration */
	CameraGetConfigFunc       get_config;
	CameraSetConfigFunc       set_config;

	/* Capturing */
	CameraCaptureFunc        capture;
	CameraCapturePreviewFunc capture_preview;

	/* Textual information */
	CameraSummaryFunc summary;
	CameraManualFunc  manual;
	CameraAboutFunc   about;

	/* Reserved space to use in the future without changing the struct size */
	void *reserved1;
	void *reserved2;
	void *reserved3;
	void *reserved4;
	void *reserved5;
	void *reserved6;
	void *reserved7;
	void *reserved8;
};

/* Those are DEPRECATED */
typedef GPPort     CameraPort;
typedef GPPortInfo CameraPortInfo;

typedef struct _CameraPrivateLibrary  CameraPrivateLibrary;
typedef struct _CameraPrivateCore     CameraPrivateCore;

struct _Camera {

	/* Those should be accessed only by the camera driver */
	GPPort           *port;
	CameraFilesystem *fs;
	CameraFunctions  *functions;

	CameraPrivateLibrary  *pl; /* Private data of camera libraries    */
	CameraPrivateCore     *pc; /* Private data of the core of gphoto2 */
};

/* Create a new camera device */
int gp_camera_new               (Camera **camera);

/* Preparing initialization */
int gp_camera_set_abilities     (Camera *camera, CameraAbilities  abilities);
int gp_camera_get_abilities	(Camera *camera, CameraAbilities *abilities);
int gp_camera_set_port_info     (Camera *camera, GPPortInfo  info);
int gp_camera_get_port_info     (Camera *camera, GPPortInfo *info);

/*
 * You normally don't use that. If you do, you prevent the camera driver
 * from selecting the optimal speed
 */
int gp_camera_set_port_speed    (Camera *camera, int speed);
int gp_camera_get_port_speed    (Camera *camera);

/* Initialization */
int gp_camera_init               (Camera *camera, GPContext *context);
int gp_camera_exit               (Camera *camera, GPContext *context);

/* Operations on cameras */
int gp_camera_ref   		 (Camera *camera);
int gp_camera_unref 		 (Camera *camera);
int gp_camera_free 		 (Camera *camera);

int gp_camera_get_config	 (Camera *camera, CameraWidget **window,
				  GPContext *context);
int gp_camera_set_config	 (Camera *camera, CameraWidget  *window,
				  GPContext *context);
int gp_camera_get_summary	 (Camera *camera, CameraText *summary,
				  GPContext *context);
int gp_camera_get_manual	 (Camera *camera, CameraText *manual,
				  GPContext *context);
int gp_camera_get_about		 (Camera *camera, CameraText *about,
				  GPContext *context);
int gp_camera_capture 		 (Camera *camera, CameraCaptureType type,
				  CameraFilePath *path, GPContext *context);
int gp_camera_capture_preview 	 (Camera *camera, CameraFile *file,
				  GPContext *context);

/* Operations on folders */
int gp_camera_folder_list_files   (Camera *camera, const char *folder, 
				   CameraList *list, GPContext *context);
int gp_camera_folder_list_folders (Camera *camera, const char *folder, 
				   CameraList *list, GPContext *context);
int gp_camera_folder_delete_all   (Camera *camera, const char *folder,
				   GPContext *context);
int gp_camera_folder_put_file     (Camera *camera, const char *folder,
				   CameraFile *file, GPContext *context);
int gp_camera_folder_make_dir     (Camera *camera, const char *folder,
				   const char *name, GPContext *context);
int gp_camera_folder_remove_dir   (Camera *camera, const char *folder,
				   const char *name, GPContext *context);

/* Operations on files */
int gp_camera_file_get_info 	(Camera *camera, const char *folder, 
				 const char *file, CameraFileInfo *info,
				 GPContext *context);
int gp_camera_file_set_info 	(Camera *camera, const char *folder, 
				 const char *file, CameraFileInfo info,
				 GPContext *context);
int gp_camera_file_get		(Camera *camera, const char *folder, 
				 const char *file, CameraFileType type,
				 CameraFile *camera_file, GPContext *context);
int gp_camera_file_delete     	(Camera *camera, const char *folder, 
				 const char *file, GPContext *context);

/* Some cameras need 'keep-alive-messages'. */
typedef int          (* CameraTimeoutFunc)      (Camera *camera,
						 GPContext *context);
typedef unsigned int (* CameraTimeoutStartFunc) (Camera *camera,
						 unsigned int timeout,
						 CameraTimeoutFunc func,
						 void *data);
typedef void         (* CameraTimeoutStopFunc)  (Camera *camera,
						 unsigned int id, void *data);
void         gp_camera_set_timeout_funcs (Camera *camera,
				          CameraTimeoutStartFunc start_func,
				          CameraTimeoutStopFunc  stop_func,
				          void *data);
int          gp_camera_start_timeout     (Camera *camera, unsigned int timeout,
					  CameraTimeoutFunc func);
void         gp_camera_stop_timeout      (Camera *camera, unsigned int id);

#endif /* __GPHOTO2_CAMERA_H__ */
