int camera_abilities (CameraAbilities *abilities);

int camera_init (CameraInit *init);
int camera_exit ();

int camera_open ();
int camera_close ();

int camera_folder_count ();
int camera_folder_name(int folder_number, char *folder_name);
int camera_folder_set(int folder_number);

int camera_file_count ();
int camera_file_get (int file_number, CameraFile *file);
int camera_file_get_preview (int file_number, CameraFile *preview);
int camera_file_delete (int file_number);

int camera_file_lock   (int file_number);
int camera_file_unlock (int file_number);

int camera_config_set (CameraConfig *conf);
int camera_capture (int type);
int camera_summary (char *summary);
int camera_manual (char *manual);
int camera_about (char *about);
