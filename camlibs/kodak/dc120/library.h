char *dc120_packet_new   (int command_byte);
int   dc120_packet_write (DC120Data *dd, char *packet, int size, int read_response);
int   dc120_packet_read  (DC120Data *dd, char *packet, int size);
int   dc120_set_speed    (DC120Data *dd, int speed);
int   dc120_get_status   (DC120Data *dd);
int   dc120_get_albums	 (DC120Data *dd, int from_card, CameraList *list);
int   dc120_get_file	 (DC120Data *dd, int get_preview, int from_card, int album_number, 
			  int file_number, CameraFile *file);
