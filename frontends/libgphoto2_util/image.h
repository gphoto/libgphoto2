int gp_file_image_rotate (CameraFile *old_file, CameraFile *new_file, 
			  int degrees);
int gp_file_image_scale  (CameraFile *old_file, CameraFile *new_file, 
			  int new_width, int new_height);
int gp_file_image_flip_h (CameraFile *old_file, CameraFile *new_file);
int gp_file_image_flip_v (CameraFile *old_file, CameraFile *new_file);
