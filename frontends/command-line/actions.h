typedef int image_action  (const char *folder, const char *filename);
typedef int folder_action (const char *subfolder, image_action action,
			   int reverse);

int print_folder          (const char *subfolder, image_action action,
			   int reverse);

int print_picture_action  (const char *folder, const char *filename);
int save_picture_action   (const char *folder, const char *filename);
int save_thumbnail_action (const char *folder, const char *filename);
int save_raw_action       (const char *folder, const char *filename);
int save_audio_action     (const char *folder, const char *filename);
int delete_picture_action (const char *folder, const char *filename);
