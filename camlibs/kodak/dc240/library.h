/*
  Kodak DC 240/280/3400/5000 driver.
 */

#ifndef __DC240_LIBRARY_H__
#define __DC240_LIBRARY_H__

#define DC240_ACTION_PREVIEW    (unsigned char)0x93
#define DC240_ACTION_IMAGE      (unsigned char)0x9A
#define DC240_ACTION_DELETE     (unsigned char)0x9D



int   dc240_open         (Camera *camera);
int   dc240_close        (Camera *camera);

int   dc240_set_speed    (Camera *camera, int speed);

int   dc240_status       (Camera *camera);

int   dc240_get_folders  (Camera *camera, CameraList *list, const char *folder);
int   dc240_get_filenames(Camera *camera, CameraList *list, const char *folder);

int   dc240_file_action	 (Camera *camera, int action, CameraFile *file,
                          const char *folder, const char *filename);

int   dc240_capture	 (Camera *camera, CameraFilePath *path);
int   dc240_packet_set_size (Camera *camera, short int size);

#endif /*__DC240_LIBRARY_H__*/
