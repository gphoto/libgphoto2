#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"


gboolean exit_callback(GtkMenuItem *menuitem, gpointer user_data) {

  gtk_main_quit();
  return TRUE;

} /*end exit_callback */

gboolean close_callback(GtkMenuItem *menuitem, gpointer user_data) {

  gtk_widget_destroy(user_data);
  return TRUE;
  
} /* end close_callback */

void display_window(GtkMenuItem *menuitem, gint window) {
  /* is this the best way to do this?? */

  GtkWidget *window_to_display;

  switch (window) {
  case XPORT_WIN:
    window_to_display = create_xport_win();
    gtk_widget_show(window_to_display);
    break;
  default:
    break;
  }

} /* end display_window */

void select_camera_callback(GtkMenuItem *menuitem, gpointer user_data) {


} /* end select_camera_callback */

