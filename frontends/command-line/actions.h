typedef int image_action(char *folder, char *filename);
typedef int folder_action(char *subfolder, image_action action, int reverse);

int print_folder(char *subfolder, image_action action, int reverse);
int print_files(char *subfolder, image_action iaction, int reverse);
int save_picture_action(char *folder, char *filename);
int save_thumbnail_action(char *folder, char *filename);
int save_raw_action(char *folder, char *filename);
int delete_picture_action(char *folder, char *filename);
