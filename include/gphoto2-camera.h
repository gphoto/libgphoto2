/* 	Header file for gPhoto2

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

/* Retrieve the number of available cameras */
int gp_camera_count ();

/* Retrieve the name of a particular camera */
int gp_camera_name  (int camera_number, char *camera_name);

/* Retreive abilities for a given camera */
int gp_camera_abilities	    (int camera_number, CameraAbilities *abilities);
int gp_camera_abilities_by_name (char *camera_name, CameraAbilities *abilities);

/* Create a new camera device */
int gp_camera_new		(Camera **camera);

/* Reference/Unreference a camera */
int gp_camera_ref   		(Camera *camera);
int gp_camera_unref 		(Camera *camera);

/* Free a camera */
int gp_camera_free 		(Camera *camera);

/* Initialize the camera */
int gp_camera_init 		(Camera *camera);

/* Retrieve a session identifier */
int gp_camera_session 		(Camera *camera);

/* Retrieve lists of folders or files */
int gp_camera_folder_list (Camera *camera, CameraList *list, char *folder);
int gp_camera_file_list   (Camera *camera, CameraList *list, char *folder);

/* Retrieve and set information about a file on the camera */
int gp_camera_file_info_get (Camera *camera, CameraFileInfo *info,
			     char *folder, char *filename);
int gp_camera_file_info_set (Camera *camera, CameraFileInfo *info, 
			     char *folder, char *filename);

/* Retrieve files  */
int gp_camera_file_get         (Camera *camera, CameraFile *file, 
			   	char *folder, char *filename);
int gp_camera_file_get_preview (Camera *camera, CameraFile *file, 
				char *folder, char *filename);

/* Configure the camera */
int gp_camera_config_get       (Camera *camera, CameraWidget **window);
int gp_camera_config_set       (Camera *camera, CameraWidget *window);

/* Configure a particular folder */
int gp_camera_folder_config_get(Camera *camera, CameraWidget **window,
				char *folder);
int gp_camera_folder_config_set(Camera *camera, CameraWidget *window,
				char *folder);

/* Configure a particular file */
int gp_camera_file_config_get  (Camera *camera, CameraWidget **window,
				char *folder, char *filename);
int gp_camera_file_config_set  (Camera *camera, CameraWidget *window, 
				char *folder, char *filename);

/* Upload a file to the camera */
int gp_camera_folder_put_file (Camera *camera, CameraFile *file, char *folder);

/* Delete a file from the camera */
int gp_camera_file_delete     (Camera *camera, char *folder, char *filename);
int gp_camera_folder_delete_all (Camera *camera, char *folder);

/* Capture a file to the camera */
int gp_camera_capture (Camera *camera, CameraFilePath *path, CameraCaptureSetting *setting);

/* Capture and download a preview from the camera */
int gp_camera_capture_preview (Camera *camera, CameraFile *file);

/* Retrieve information about the camera, camera usage, and camera library */
int gp_camera_summary (Camera *camera, CameraText *summary);
int gp_camera_manual  (Camera *camera, CameraText *manual);
int gp_camera_about   (Camera *camera, CameraText *about);

/* Retrieve a string description of a result */
char *gp_camera_result_as_string (Camera *camera, int result);

