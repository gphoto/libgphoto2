/*
   file : camera_util.h
   author : jae gangemi (jgangemi@yahoo.com)

   contains function headers for camera_util.c 

*/


int camera_set(void);
void camera_config_build_rec(CameraWidget *cw, GtkWidget *box,
                             GtkWidget *window);
void camera_config_retrieve_rec(CameraWidget *cw, GtkWidget *window, 
				CameraSetting *settings, int *count);

