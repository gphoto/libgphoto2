/* 
   file : gp_util.c
   author : jae gangemi (jgangemi@yahoo.com)
   
   contains functions that are required by the frontend standard

*/

#include <stdio.h>
#include <stdlib.h>
#include <gphoto2.h>
#include <gtk/gtk.h>

#include "globals.h"
#include "gp_util.h"

int gp_interface_progress(Camera *camera, CameraFile *file, float percentage) {
  
  return 1;

} 

int gp_interface_confirm(Camera *camera, char *message) {

  return 1;

}

int gp_interface_status(Camera *camera, char *status) {

  return 1;

}

int gp_interface_message(Camera *camera, char *message) {

  return 1;

} 

int gp_wait_for_hide(GtkWidget *dialog, GtkWidget *ok, GtkWidget *cancel) {

  return 1;

} 

void gp_debug_print(char *message) {
  /* print out our debug messages */

  if (gp_gtk_debug)
    g_print("DEBUG : %s\n", message);
  
} /* end gp_debug_print */ 
