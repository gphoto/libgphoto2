int gpfe_cache_put (int camera_number, int folder_number, int file_number, CameraFile *file);
int gpfe_cache_get (int camera_number, int folder_number, int file_number, CameraFile *file);
int gpfe_cache_exists (int camera_number, int folder_number, int file_number);
int gpfe_cache_delete (int camera_number, int folder_number, int file_number);
int gpfe_file_image_rotate (CameraFile *old_file, CameraFile *new_file, 
			    int degrees);
int gpfe_file_image_scale  (CameraFile *old_file, CameraFile *new_file, 
			    int new_width, int new_height);
int gpfe_file_image_flip_h (CameraFile *old_file, CameraFile *new_file);
int gpfe_file_image_flip_v (CameraFile *old_file, CameraFile *new_file);
int gpfe_script(char *script_line);
