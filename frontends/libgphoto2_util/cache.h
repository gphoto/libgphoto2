int cache_put (int camera_number, int folder_number, int file_number, CameraFile *file);
int cache_get (int camera_number, int folder_number, int file_number, CameraFile *file);
int cache_exists (int camera_number, int folder_number, int file_number);
int cache_delete (int camera_number, int folder_number, int file_number);

