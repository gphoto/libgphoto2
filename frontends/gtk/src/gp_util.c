/* 
   file : gp_util.c
   author : jae gangemi (jgangemi@yahoo.com)
   
   contains functions that are required by the frontend standard

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gphoto2.h>
#include <gtk/gtk.h>

#include "globals.h"
#include "gp_util.h"
#include "support.h"
#include "interface.h"

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

  GtkWidget *window, *label, *ok;

  if (strlen(message) > 1024) {
    gp_interface_message_long(camera, message);
    return (GP_OK);
  } 
  
  if (GTK_WIDGET_VISIBLE(gp_gtk_progress_window)) {
    label = (GtkWidget*) lookup_widget(gp_gtk_progress_window, "message");
    gtk_label_set_text(GTK_LABEL(label), message);
    gp_idle();
    return (GP_OK);
  }

  window = create_message_win(); // not yet implemented
  label = (GtkWidget*)lookup_widget(window, "message");
  ok = (GtkWidget*)lookup_widget(window, "close");
  
  gtk_label_set_text(GTK_LABEL(label), message);
  
  gp_wait_for_hide(window, ok, NULL);
  gtk_widget_destroy(window);
  
  return (GP_OK);
  
} /* end gp_interface_message */

int gp_interface_message_long(Camera *camera, char *message) {

  GtkWidget *window, *ok, *text;

  window = create_message_win_long(); // not yet implemented
  text = (GtkWidget*)lookup_widget(window, "message");
  ok = (GtkWidget*)lookup_widget(window, "close");
  gtk_label_set_text(GTK_LABEL(text), message);

  gp_wait_for_hide(window, ok, NULL);

  gtk_widget_destroy(window);
  return (GP_OK);

} /* end gp_interface_message_long */

int gp_wait_for_hide(GtkWidget *dialog, GtkWidget *ok, GtkWidget *cancel) {

  return 1;

} 

void gp_debug_print(char *message) {
  /* print out our debug messages */

  if (gp_gtk_debug)
    g_print("DEBUG : %s\n", message);
  
} /* end gp_debug_print */ 

void gp_idle(void) {
  /* let gtk do some processing */
  
  while (gtk_events_pending())
    gtk_main_iteration();
  usleep(SUSPEND_TIME);
  while (gtk_events_pending())
    gtk_main_iteration();
  
} /* end gp_idle */
