/* gphoto2-camera.h
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

#ifndef __GPHOTO2_CAMERA_H__
#define __GPHOTO2_CAMERA_H__

typedef struct _Camera Camera;

#include <gphoto2-port.h>
#include <gphoto2-port-core.h>

#include <gphoto2-widget.h>
#include <gphoto2-list.h>
#include <gphoto2-file.h>
#include <gphoto2-filesys.h>
#include <gphoto2-abilities.h>
#include <gphoto2-abilities-list.h>
#include <gphoto2-result.h>

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

typedef int (*CameraExitFunc)      (Camera *camera);

/* Configuration */
typedef int (*CameraGetConfigFunc) (Camera *camera, CameraWidget **widget);
typedef int (*CameraSetConfigFunc) (Camera *camera, CameraWidget  *widget);
typedef int (*CameraFolderGetConfigFunc) (Camera *camera, const char *folder,
					  CameraWidget **widget);
typedef int (*CameraFolderSetConfigFunc) (Camera *camera, const char *folder,
					  CameraWidget *widget);
typedef int (*CameraFileGetConfigFunc) (Camera *camera, const char *folder,
				        const char *file,
					CameraWidget **widget);
typedef int (*CameraFileSetConfigFunc) (Camera *camera, const char *folder,
				        const char *file, CameraWidget *widget);

/* Capturing */
typedef int (*CameraCaptureFunc)        (Camera *camera, CameraCaptureType type,
				         CameraFilePath *path);
typedef int (*CameraCapturePreviewFunc) (Camera *camera, CameraFile *file);

/* Textual information */
typedef int (*CameraSummaryFunc) (Camera *camera, CameraText *text);
typedef int (*CameraManualFunc)  (Camera *camera, CameraText *text);
typedef int (*CameraAboutFunc)   (Camera *camera, CameraText *text);

/* Error reporting */
typedef const char *(*CameraResultFunc) (Camera *camera, int result);

typedef struct {
	CameraExitFunc exit;

	/* Configuration */
	CameraGetConfigFunc       get_config;
	CameraSetConfigFunc       set_config;
	CameraFolderGetConfigFunc folder_get_config;
	CameraFolderSetConfigFunc folder_set_config;
	CameraFileGetConfigFunc   file_get_config;
	CameraFileSetConfigFunc   file_set_config;

	/* Capturing */
	CameraCaptureFunc        capture;
	CameraCapturePreviewFunc capture_preview;

	/* Textual information */
	CameraSummaryFunc summary;
	CameraManualFunc  manual;
	CameraAboutFunc   about;

} CameraFunctions;

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
int gp_camera_set_abilities     (Camera *camera, CameraAbilities  abilities);
int gp_camera_get_abilities	(Camera *camera, CameraAbilities *abilities);
int gp_camera_set_port_name     (Camera *camera, const char  *port_name);
int gp_camera_get_port_name     (Camera *camera, const char **port_name);
int gp_camera_set_port_path     (Camera *camera, const char  *port_path);
int gp_camera_get_port_path     (Camera *camera, const char **port_path);

/*
 * You normally don't use that. If you do, you prevent the camera driver
 * from selecting the optimal speed
 */
int gp_camera_set_port_speed    (Camera *camera, int speed);
int gp_camera_get_port_speed    (Camera *camera);

/************************************************************************
 * Part I:                                                              *
 *             Operations on CAMERAS                                    *
 *                                                                      *
 *   - ref                 : Ref the camera                             *
 *   - unref               : Unref the camera                           *
 *   - free                : Free                                       *
 *   - init                : Initialize the camera                      *
 *   - get_config          : Get configuration options                  *
 *   - set_config          : Set those configuration options            *
 *   - get_summary         : Get information about the camera           *
 *   - get_manual          : Get information about the driver           *
 *   - get_about           : Get information about authors of the driver*
 *   - capture             : Capture an image                           *
 *   - capture_preview     : Capture a preview                          *
 ************************************************************************/

int gp_camera_ref   		 (Camera *camera);
int gp_camera_unref 		 (Camera *camera);
int gp_camera_free 		 (Camera *camera);
int gp_camera_init 		 (Camera *camera);
int gp_camera_get_config	 (Camera *camera, CameraWidget **window);
int gp_camera_set_config	 (Camera *camera, CameraWidget  *window);
int gp_camera_get_summary	 (Camera *camera, CameraText *summary);
int gp_camera_get_manual	 (Camera *camera, CameraText *manual);
int gp_camera_get_about		 (Camera *camera, CameraText *about);
int gp_camera_capture 		 (Camera *camera, CameraCaptureType type,
				  CameraFilePath *path);
int gp_camera_capture_preview 	 (Camera *camera, CameraFile *file);

/************************************************************************
 * Part II:                                                             *
 *             Operations on FOLDERS                                    *
 *                                                                      *
 *   - list_files  : Get a list of files in this folder                 *
 *   - list_folders: Get a list of folders in this folder               *
 *   - delete_all  : Delete all files in this folder                    *
 *   - put_file    : Upload a file into this folder                     *
 *   - get_config  : Get configuration options of a folder              *
 *   - set_config  : Set those configuration options                    *
 ************************************************************************/

int gp_camera_folder_list_files   (Camera *camera, const char *folder, 
				   CameraList *list);
int gp_camera_folder_list_folders (Camera *camera, const char *folder, 
				   CameraList *list);
int gp_camera_folder_delete_all   (Camera *camera, const char *folder);
int gp_camera_folder_put_file     (Camera *camera, const char *folder, 
				   CameraFile *file);
int gp_camera_folder_get_config   (Camera *camera, const char *folder, 
				   CameraWidget **window);
int gp_camera_folder_set_config   (Camera *camera, const char *folder, 
				   CameraWidget  *window);
								
/************************************************************************
 * Part III:                                                            *
 *             Operations on FILES                                      *
 *                                                                      *
 *   - get_info   : Get specific information about a file               *
 *   - set_info   : Set specific parameters of a file                   *
 *   - get_file   : Get a file                                          *
 *   - get_config : Get additional configuration options of a file      *
 *   - set_config : Set those additional configuration options          *
 *   - delete     : Delete a file                                       *
 ************************************************************************/

int gp_camera_file_get_info 	(Camera *camera, const char *folder, 
				 const char *file, CameraFileInfo *info);
int gp_camera_file_set_info 	(Camera *camera, const char *folder, 
				 const char *file, CameraFileInfo *info);
int gp_camera_file_get		(Camera *camera, const char *folder, 
				 const char *file, CameraFileType type,
				 CameraFile *camera_file);
int gp_camera_file_get_config  	(Camera *camera, const char *folder, 
				 const char *file, CameraWidget **window);
int gp_camera_file_set_config  	(Camera *camera, const char *folder, 
				 const char *file, CameraWidget  *window);
int gp_camera_file_delete     	(Camera *camera, const char *folder, 
				 const char *file);

/* Informing frontends */
typedef void (* CameraMessageFunc)  (Camera *, const char *msg, void *data);
typedef void (* CameraStatusFunc)   (Camera *, const char *status, void *data);
typedef void (* CameraProgressFunc) (Camera *, float percentage, void *data);
int gp_camera_set_status_func   (Camera *camera, CameraStatusFunc func,
				 void *data);
int gp_camera_set_progress_func (Camera *camera, CameraProgressFunc func,
				 void *data);
int gp_camera_set_message_func  (Camera *camera, CameraMessageFunc func,
				 void *data);

int gp_camera_message           (Camera *camera, const char *format, ...);
int gp_camera_status            (Camera *camera, const char *format, ...);
int gp_camera_progress          (Camera *camera, float percentage);

/* Error reporting */
int         gp_camera_set_error (Camera *camera, const char *format, ...);
const char *gp_camera_get_error (Camera *camera);

#endif /* __GPHOTO2_CAMERA_H__ */
