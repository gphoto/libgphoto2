int write_file(u_char *buf, int len, FILE *outfp);
int make_jpeg_comment(u_char *buf, u_char *jpeg_comment);
int get_picture_information (int *, int);
long get_file(char *name, char **data, int format, int verbose);
long get_thumbnail(char *name, char **data, int format, int verbose, int n);
void get_date_info (char *, char * ,char *);
long get_picture (int, char **, int, int, int);
void get_all_pictures (int, int, char *, int, int);
void delete_picture (int, int);
