int for_each_subfolder      (const char *folder, folder_action faction,
			     image_action action, int reverse);
int for_each_image          (const char *folder, image_action iaction,
			     int reverse);
int for_each_image_in_range (const char *folder, char *range,
			     image_action action, int reverse);
