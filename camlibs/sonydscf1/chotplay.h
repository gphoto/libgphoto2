int make_jpeg_comment P__((u_char *, u_char *));
int get_picture_information P__((int *, int));
long get_file P__((char *, FILE *, int, int));
long get_thumbnail P__((char *, FILE *, int, int, int));
void get_date_info P__((char *, char * ,char *));
void get_picture P__((int, char *, int, int, int));
void get_all_pictures P__((int, int, char *, int, int));
void delete_picture P__((int, int));
