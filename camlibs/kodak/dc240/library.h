#define DC240_ACTION_PREVIEW    (unsigned char)0x93
#define DC240_ACTION_IMAGE      (unsigned char)0x9A
#define DC240_ACTION_DELETE     (unsigned char)0x9D


char *dc240_packet_new   (int command_byte);
int   dc240_packet_write (DC240Data *dd, char *packet, int size,
                          int read_response);
int   dc240_packet_read  (DC240Data *dd, char *packet, int size);

int   dc240_packet_set_size (DC240Data *dd, short int size);

int   dc240_open         (DC240Data *dd);
int   dc240_close        (DC240Data *dd);

int   dc240_set_speed    (DC240Data *dd, int speed);

int   dc240_status       (DC240Data *dd);

int   dc240_get_folders  (DC240Data *dd, CameraList *list, char *folder);
int   dc240_get_filenames(DC240Data *dd, CameraList *list, char *folder);

int   dc240_file_action	 (DC240Data *dd, int action, CameraFile *file,
                          char *folder, char *filename);

int   dc240_capture	 (DC240Data *dd, CameraFile *file);
