/* 
   file : camera_util.c 
   author : jae gangemi (jgangemi@yahoo.com)

   contains all functions to create and manipulate camera settings through
   the gtk interface. 

*/

#include <stdio.h>
#include <stdlib.h>
#include <gphoto2.h>
#include <gtk/gtk.h>

#include "globals.h"
#include "camera_util.h"


int camera_set() {

  gp_gtk_camera_init = 0;

  return 1;

} /* end camera_set */

void camera_config_retrieve_rec(CameraWidget *cw, GtkWidget *window, 
				CameraSetting *settings, int *count) {

  
}

void camera_config_build_rec(CameraWidget *cw, GtkWidget *box, 
			     GtkWidget *window) {

}
