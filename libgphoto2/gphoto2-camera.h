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
#include <gphoto2-port-info-list.h>

#include <gphoto2-widget.h>
#include <gphoto2-list.h>
#include <gphoto2-file.h>
#include <gphoto2-filesys.h>
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

/**
 * CameraExitFunc:
 * @camera: a #Camera
 *
 * Implement this function in camera drivers if you need to clean up on
 * exit. You need this function for example if you use camera->pl and 
 * allocated memory for that on #camera_init. Then, you would free the
 * memory here.
 *
 * Return value: a gphoto2 error code
 **/
typedef int (*CameraExitFunc)      (Camera *camera);

/**
 * CameraGetConfigFunc:
 * @camera: a #Camera
 * @widget:
 *
 * Implement this function if your camera supports some configuration
 * operations (like adjusting exposure).
 *
 * Return value: a gphoto2 error code
 **/
typedef int (*CameraGetConfigFunc) (Camera *camera, CameraWidget **widget);

/**
 * CameraSetConfigFunc:
 * @camera: a #Camera
 * @widget: a #CameraWidget
 *
 * If you implement the #CameraGetConfigFunc, you should also implement this
 * function in order to make it possible for frontends to actually adjust
 * configuration options. Parse the @widget tree and check for each item
 * if the value has been changed (using #gp_widget_changed). If this is the
 * case, adjust this configuration option on the camera.
 *
 * Return value: a gphoto2 error code
 **/
typedef int (*CameraSetConfigFunc) (Camera *camera, CameraWidget  *widget);

/**
 * CameraCaptureFunc: 
 * @camera: a #Camera
 * @type: a #CameraCaptureType
 * @path: a #CameraFilePath
 *
 * Implement this function if your camera supports remote capturing of images.
 * When capturing an image, this image needs to be saved on the camera. The
 * location of the image on the camera should then be written into 
 * supplied @path so that the frontend will know where to get the 
 * newly captured image from. Don't forget to tell the #CameraFilesystem 
 * (camera->fs) that a new picture is on the camera using
 * #gp_filesystem_append.
 *
 * Return value: a gphoto2 error code
 **/
typedef int (*CameraCaptureFunc)        (Camera *camera, CameraCaptureType type,
				         CameraFilePath *path);

/**
 * CameraCapturePreviewFunc:
 * @camera: a #Camera
 * @file: a #CameraFile
 *
 * Implement this function if your camera supports capturing
 * previews of low resolution. Those previews must not be stored on the camera.
 * They need to be written to the supplied @file (using for example
 * #gp_file_set_data_and_size).
 *
 * Return value: a gphoto2 error code
 **/
typedef int (*CameraCapturePreviewFunc) (Camera *camera, CameraFile *file);

/**
 * CameraSummaryFunc:
 * @camera: a #Camera
 * @text: a #CameraText
 *
 * Implement this function if your camera provides static information and
 * non-configurable data like serial number or number of pictures taken. All
 * configurable data should be displayed through #CameraGetConfigFunc 
 * instead.
 *
 * Return value: a gphoto2 error code
 **/
typedef int (*CameraSummaryFunc) (Camera *camera, CameraText *text);

/**
 * CameraManualFunc:
 * @camera: a #Camera
 * @text: a #CameraText
 *
 * Implement this function if you need to give the user instructions on
 * how to use his camera. Here, you can for example inform the user about known
 * limitations of the protocol.
 *
 * Return value: a gphoto2 error code
 **/
typedef int (*CameraManualFunc)  (Camera *camera, CameraText *text);

/**
 * CameraAboutFunc:
 * @camera: a #Camera
 * @text: a #CameraText
 *
 * Implement this function if you want to give frontends the possibility to 
 * inform users who wrote this driver or who contributed to it.
 *
 * Return value: a gphoto2 error code
 **/
typedef int (*CameraAboutFunc)   (Camera *camera, CameraText *text);

/**
 * CameraFunctions:
 *
 * Those functions are optionally. Depending on the features of the protocol,
 * you would implement some functions and leave others out. Tell gphoto2
 * what functions you provide on #camera_init.
 **/
typedef struct {
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
int gp_camera_set_port_info     (Camera *camera, GPPortInfo  info);
int gp_camera_get_port_info     (Camera *camera, GPPortInfo *info);

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
 ************************************************************************/

int gp_camera_folder_list_files   (Camera *camera, const char *folder, 
				   CameraList *list);
int gp_camera_folder_list_folders (Camera *camera, const char *folder, 
				   CameraList *list);
int gp_camera_folder_delete_all   (Camera *camera, const char *folder);
int gp_camera_folder_put_file     (Camera *camera, const char *folder, 
				   CameraFile *file);
								
/************************************************************************
 * Part III:                                                            *
 *             Operations on FILES                                      *
 *                                                                      *
 *   - get_info   : Get specific information about a file               *
 *   - set_info   : Set specific parameters of a file                   *
 *   - get_file   : Get a file                                          *
 *   - delete     : Delete a file                                       *
 ************************************************************************/

int gp_camera_file_get_info 	(Camera *camera, const char *folder, 
				 const char *file, CameraFileInfo *info);
int gp_camera_file_set_info 	(Camera *camera, const char *folder, 
				 const char *file, CameraFileInfo *info);
int gp_camera_file_get		(Camera *camera, const char *folder, 
				 const char *file, CameraFileType type,
				 CameraFile *camera_file);
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
