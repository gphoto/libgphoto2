/* 	Header file for gPhoto2

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

int gp_camera_count ();
int gp_camera_name  (int camera_number, char *camera_name);

int gp_camera_abilities	    (int camera_number, CameraAbilities *abilities);
int gp_camera_abilities_by_name (char *camera_name, CameraAbilities *abilities);

int gp_camera_new 	  (Camera **camera, int camera_number);
int gp_camera_new_by_name (Camera **camera, char *camera_name);

int gp_camera_ref   (Camera *camera);
int gp_camera_unref (Camera *camera);

int gp_camera_free (Camera *camera);

int gp_camera_init (Camera *camera, CameraPortInfo *settings);

int gp_camera_session (Camera *camera);
	
int gp_camera_folder_list (Camera *camera, CameraList *list, char *folder);
int gp_camera_file_list   (Camera *camera, CameraList *list, char *folder);

int gp_camera_file_get         (Camera *camera, CameraFile *file, 
			   	char *folder, char *filename);
int gp_camera_file_get_preview (Camera *camera, CameraFile *file, 
				char *folder, char *filename);
				
int gp_camera_file_config_get  (Camera *camera, CameraWidget **window,
				char *folder, char *filename);
int gp_camera_file_config_set  (Camera *camera, CameraWidget *window, 
				char *folder, char *filename);
int gp_camera_folder_config_get(Camera *camera, CameraWidget **window,
				char *folder);
int gp_camera_folder_config_set(Camera *camera, CameraWidget *window,
				char *folder);
int gp_camera_config_get       (Camera *camera, CameraWidget **window);
int gp_camera_config_set       (Camera *camera, CameraWidget *window);

int gp_camera_file_put (Camera *camera, CameraFile *file, char *folder);
int gp_camera_file_delete (Camera *camera, char *folder, char *filename);

int gp_camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info);

int gp_camera_summary (Camera *camera, CameraText *summary);
int gp_camera_manual  (Camera *camera, CameraText *manual);
int gp_camera_about   (Camera *camera, CameraText *about);

int gp_camera_config (Camera *camera); //Deprecated

char *gp_camera_result_as_string (Camera *camera, int result);

