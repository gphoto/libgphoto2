int camera_id 			(CameraText *id);
int camera_abilities 		(CameraAbilitiesList *list);
int camera_init 		(Camera *camera);
int camera_exit 		(Camera *camera);

int camera_get_config		(Camera *camera, CameraWidget **window);
int camera_set_config		(Camera *camera, CameraWidget  *window);
int camera_capture 		(Camera *camera, int capture_type, 
				 CameraFilePath *path);
int camera_capture_preview     	(Camera *camera, CameraFile *file);
int camera_summary 		(Camera *camera, CameraText *summary);
int camera_manual 		(Camera *camera, CameraText *manual);
int camera_about 		(Camera *camera, CameraText *about);

int camera_folder_list_folders	(Camera *camera, const char *folder, 
				 CameraList *list);
int camera_folder_list_files   	(Camera *camera, const char *folder, 
				 CameraList *list);
int camera_folder_put_file     	(Camera *camera, const char *folder, 
				 CameraFile *file);
int camera_folder_delete_all   	(Camera *camera, const char *folder);
int camera_folder_get_config	(Camera *camera, const char *folder, 
				 CameraWidget **window);
int camera_folder_set_config	(Camera *camera, const char *folder, 
				 CameraWidget  *window);

int camera_file_get_info        (Camera *camera, const char *folder, 
				 const char *filename, CameraFileInfo *info);
int camera_file_set_info	(Camera *camera, const char *folder, 
				 const char *filename, CameraFileInfo *info);
int camera_file_get       	(Camera *camera, const char *folder, 
				 const char *filename, CameraFile *file);
int camera_file_get_preview	(Camera *camera, const char *folder, 
				 const char *filename, CameraFile *file);
int camera_file_delete   	(Camera *camera, const char *folder, 
				 const char *filename);
int camera_file_get_config	(Camera *camera, const char *folder, 
				 const char *filename, CameraWidget **window);
int camera_file_set_config	(Camera *camera, const char *folder, 
				 const char *filename, CameraWidget  *window);

char *camera_result_as_string 	(Camera *camera, int result);
