int get_picture_information (GPPort *,int *, int);
long get_file(GPPort*,char *name, char **data, int format, int verbose);
long get_thumbnail(GPPort*,char *name, char **data, int format, int verbose, int n);
void get_date_info (GPPort*,char *, char * ,char *);
long get_picture (GPPort*,int, char **, int, int, int);
void get_all_pictures (GPPort*,int, int, char *, int, int);
void delete_picture (GPPort*,int, int);
