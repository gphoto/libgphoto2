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

#include <gphoto2-widget.h>
#include <gphoto2-list.h>
#include <gphoto2-file.h>
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
	GP_FILE_INFO_NONE            = 0,
	GP_FILE_INFO_TYPE            = 1 << 0,
	GP_FILE_INFO_NAME            = 1 << 1,
	GP_FILE_INFO_SIZE            = 1 << 2,
	GP_FILE_INFO_WIDTH           = 1 << 3,
	GP_FILE_INFO_HEIGHT          = 1 << 4,
	GP_FILE_INFO_PERMISSIONS     = 1 << 5,
	GP_FILE_INFO_ALL             = 0xFF
} CameraFileInfoFields;

typedef enum {
	GP_FILE_PERM_NONE       = 0,
	GP_FILE_PERM_READ       = 1 << 0,
	GP_FILE_PERM_DELETE     = 1 << 1,
	GP_FILE_PERM_ALL        = 0xFF
} CameraFilePermissions;

typedef struct {
	CameraFileInfoFields fields;
	char type [64];
	CameraFilePermissions permissions; /* Don't use with preview */
	char name [64];
	int size;
	int width, height;
} CameraFileInfoStruct;

typedef struct {
	CameraFileInfoStruct    preview;
	CameraFileInfoStruct    file;
} CameraFileInfo;

typedef int (*c_id)                     (CameraText *);
typedef int (*c_abilities)              (CameraAbilitiesList *);
typedef int (*c_init)                   (Camera*);
typedef int (*c_exit)                   (Camera*);
typedef int (*c_get_config)             (Camera*, CameraWidget**);
typedef int (*c_set_config)             (Camera*, CameraWidget*);
typedef int (*c_capture)                (Camera*, int, CameraFilePath*);
typedef int (*c_capture_preview)        (Camera*, CameraFile*);
typedef int (*c_summary)                (Camera*, CameraText*);
typedef int (*c_manual)                 (Camera*, CameraText*);
typedef int (*c_about)                  (Camera*, CameraText*);

typedef int (*c_folder_list_folders)    (Camera*, const char*, CameraList*);
typedef int (*c_folder_list_files)      (Camera*, const char*, CameraList*);
typedef int (*c_folder_get_config)      (Camera*, const char*, CameraWidget**);
typedef int (*c_folder_set_config)      (Camera*, const char*, CameraWidget*);
typedef int (*c_folder_put_file)        (Camera*, const char*, CameraFile*);
typedef int (*c_folder_delete_all)      (Camera*, const char*);

typedef int (*c_file_get_info)          (Camera*, const char*,
					 const char*, CameraFileInfo*);
typedef int (*c_file_set_info)          (Camera*, const char*,
					 const char*, CameraFileInfo*);
typedef int (*c_file_get_config)        (Camera*, const char*,
		                         const char*, CameraWidget**);
typedef int (*c_file_set_config)        (Camera*, const char*,
		                         const char*, CameraWidget*);
typedef int (*c_file_get)               (Camera*, const char*,
		                         const char*, CameraFileType type,
					 CameraFile*);
typedef int (*c_file_delete)            (Camera*, const char*, const char*);

typedef char *(*c_result_as_string)     (Camera*, int);


typedef struct {
	c_id                    id;
	c_abilities             abilities;
	c_init                  init;
	c_exit                  exit;
	c_get_config            get_config;
	c_set_config            set_config;
	c_capture               capture;
	c_capture_preview       capture_preview;
	c_summary               summary;
	c_manual                manual;
	c_about                 about;

	c_folder_list_folders   folder_list_folders;
	c_folder_list_files     folder_list_files;
	c_folder_get_config     folder_get_config;
	c_folder_set_config     folder_set_config;
	c_folder_put_file       folder_put_file;
	c_folder_delete_all     folder_delete_all;

	c_file_get_info         file_get_info;
	c_file_set_info         file_set_info;
	c_file_get_config       file_get_config;
	c_file_set_config       file_set_config;
	c_file_get              file_get;
	c_file_delete           file_delete;

	c_result_as_string      result_as_string;
} CameraFunctions;

typedef gp_port_info CameraPortInfo;

struct _Camera {
	char            model[128];

	CameraPortInfo  *port;
	
	int             ref_count;
	
	void            *library_handle;
	
	CameraAbilities *abilities;
	CameraFunctions *functions;

	void            *camlib_data;
	void            *frontend_data;

	int             session;
};

/* Create a new camera device */
int gp_camera_new               (Camera **camera);

/************************************************************************
 * Part I:                                                              *
 *             Operations on CAMERAS                                    *
 *                                                                      *
 *   - ref                 : Ref the camera                             *
 *   - unref               : Unref the camera                           *
 *   - free                : Free                                       *
 *   - init                : Initialize the camera                      *
 *   - get_session         : Get the session identifier                 *
 *   - get_config          : Get configuration options                  *
 *   - set_config          : Set those configuration options            *
 *   - get_summary         : Get information about the camera           *
 *   - get_manual          : Get information about the driver           *
 *   - get_about           : Get information about authors of the driver*
 *   - capture             : Capture an image                           *
 *   - capture_preview     : Capture a preview                          *
 *                                                                      *
 *   - get_result_as_string: Translate a result into a string           *
 ************************************************************************/

int gp_camera_ref   		 (Camera *camera);
int gp_camera_unref 		 (Camera *camera);
int gp_camera_free 		 (Camera *camera);
int gp_camera_init 		 (Camera *camera);
int gp_camera_get_session 	 (Camera *camera);
int gp_camera_get_config	 (Camera *camera, CameraWidget **window);
int gp_camera_set_config	 (Camera *camera, CameraWidget  *window);
int gp_camera_get_summary	 (Camera *camera, CameraText *summary);
int gp_camera_get_manual	 (Camera *camera, CameraText *manual);
int gp_camera_get_about		 (Camera *camera, CameraText *about);
int gp_camera_capture 		 (Camera *camera, int capture_type,
				  CameraFilePath *path);
int gp_camera_capture_preview 	 (Camera *camera, CameraFile *file);

char *gp_camera_get_result_as_string (Camera *camera, int result);

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

#endif /* __GPHOTO2_CAMERA_H__ */
