/*
  Kodak DC 240/280/3400/5000 driver.
 */

#ifndef __DC240_LIBRARY_H__
#define __DC240_LIBRARY_H__

#define DC240_ACTION_PREVIEW    (unsigned char)0x93
#define DC240_ACTION_IMAGE      (unsigned char)0x9A
#define DC240_ACTION_DELETE     (unsigned char)0x9D



int   dc240_open         (DC240Data *dd);
int   dc240_close        (DC240Data *dd);

int   dc240_set_speed    (DC240Data *dd, int speed);

int   dc240_status       (DC240Data *dd);

int   dc240_get_folders  (DC240Data *dd, CameraList *list, const char *folder);
int   dc240_get_filenames(DC240Data *dd, CameraList *list, const char *folder);

int   dc240_file_action	 (DC240Data *dd, int action, CameraFile *file,
                          const char *folder, const char *filename);

int   dc240_capture	 (DC240Data *dd, CameraFilePath *path);
int   dc240_packet_set_size (DC240Data *dd, short int size);

#endif /*__DC240_LIBRARY_H__*/
