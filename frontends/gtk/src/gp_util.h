/* 
  
   file : gp_util.h
   author : jae gangemi (jgangemi@yahoo.com)

   header file for gp_util.c

*/

#define SUSPEND_TIME 100000

int gp_gtk_debug;
int gp_gtk_camera_init;
Camera *gp_gtk_camera;
GtkWidget *gp_gtk_progress_window;

void gp_debug_print(char *message);
int gp_wait_for_hide(GtkWidget *dialog, GtkWidget *ok, GtkWidget *cancel);
void gp_idle(void);

int gp_interface_message_long(Camera *camera, char *message);

/* 
   these are defined in gphoto2.h - but we'll list them here anyway :)

int gp_interface_progress (Camera *camera, CameraFile *file, float percentage);
int gp_interface_confirm (Camera *camera, char *message);
int gp_interface_status (Camera *camera, char *status);
int gp_interface_message (Camera *camera, char *message);
*/

