#define DC120_ACTION_IMAGE      0
#define DC120_ACTION_PREVIEW    1
#define DC120_ACTION_DELETE     2   

char *dc120_packet_new   (int command_byte);
int   dc120_packet_write (Camera *camera, char *packet, int size, int read_response);
int   dc120_packet_read  (Camera *camera, char *packet, int size);
int   dc120_set_speed    (Camera *camera, int speed);
int   dc120_get_status   (Camera *camera);
int   dc120_get_albums	 (Camera *camera, int from_card, CameraList *list);
int   dc120_get_filenames(Camera *camera, int from_card, int album_number, CameraList *list);
int   dc120_file_action	 (Camera *camera, int action, int from_card, int album_number, 
			  int file_number, CameraFile *file);
int   dc120_capture	 (Camera *camera, CameraFile *file);
