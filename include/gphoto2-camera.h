/* 	Header file for gPhoto2

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

/* Retrieve the number of available cameras */
int gp_camera_count ();

/* Retrieve the name of a particular camera */
int gp_camera_name  (int camera_number, char *camera_name);

/* Retreive abilities for a given camera */
int gp_camera_abilities     (int camera_number, CameraAbilities *abilities);
int gp_camera_abilities_by_name (char *camera_name, CameraAbilities *abilities);

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
 *   - get_preview: Get the preview of a file                           *
 *   - get_config : Get additional configuration options of a file      *
 *   - set_config : Set those additional configuration options          *
 *   - delete     : Delete a file                                       *
 ************************************************************************/

int gp_camera_file_get_info 	(Camera *camera, const char *folder, 
				 const char *file, CameraFileInfo *info);
int gp_camera_file_set_info 	(Camera *camera, const char *folder, 
				 const char *file, CameraFileInfo *info);
int gp_camera_file_get_file 	(Camera *camera, const char *folder, 
				 const char *file, CameraFile *camera_file);
int gp_camera_file_get_preview 	(Camera *camera, const char *folder, 
				 const char *file, CameraFile *camera_file);
int gp_camera_file_get_config  	(Camera *camera, const char *folder, 
				 const char *file, CameraWidget **window);
int gp_camera_file_set_config  	(Camera *camera, const char *folder, 
				 const char *file, CameraWidget  *window);
int gp_camera_file_delete     	(Camera *camera, const char *folder, 
				 const char *file);


