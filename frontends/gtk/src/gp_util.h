/* 
  
   file : gp_util.h
   author : jae gangemi (jgangemi@yahoo.com)

   header file for gp_util.c

*/

int gp_gtk_debug;
int gp_gtk_camera_init;
Camera *gp_gtk_camera;

void gp_debug_print(char *message);
int gp_wait_for_hide(GtkWidget *dialog, GtkWidget *ok, GtkWidget *cancel);


/* 
   these are defined in gphoto2.h - but we'll list them here anyway :)

int gp_interface_progress (Camera *camera, CameraFile *file, float percentage);
int gp_interface_confirm (Camera *camera, char *message);
int gp_interface_status (Camera *camera, char *status);
int gp_interface_message (Camera *camera, char *message);
*/

