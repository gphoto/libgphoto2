#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <gphoto2.h>

#include "globals.h"

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#include "gp_util.h"
#include "camera_util.h"
#include "tree_list_util.h"



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

/*********** camera callback section **********/

void select_camera_cb(GtkMenuItem *menuitem, gpointer user_data) {

} /* end select_camera_cb */

void config_camera_cb(GtkMenuItem *menuitem, gpointer user_data) {

  CameraWidget *cw;
  CameraSetting settings[128];
  GtkWidget *window, *ok, *cancel;
  int set_count = 0;

  gp_debug_print("camera configure");

  if (!gp_gtk_camera_init)
    if (camera_set() == GP_ERROR) {
      return;
    }
  
  cw = gp_widget_new(GP_WIDGET_WINDOW, "Camera Configuration");
  
  if (gp_camera_config_get(gp_gtk_camera, cw) == GP_ERROR) {
    /* error message */
    gp_widget_free(cw);
    return;
  }
  
  if (gp_gtk_debug) 
    gp_widget_dump(cw);
  
  /* create our config window */

  window = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(window), gp_widget_label(cw));
  gtk_window_set_modal(GTK_WINDOW(window), TRUE);
  gtk_window_set_default_size(GTK_WINDOW(window), 300, 400);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  
  ok = gtk_button_new_with_label("ok");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), ok, FALSE,
		     TRUE, 0);
  gtk_widget_show(ok);
  
  cancel = gtk_button_new_with_label("cancel");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), cancel, FALSE,
		     TRUE, 0);
  gtk_widget_show(cancel);
  
  camera_config_build_rec(cw, GTK_DIALOG(window)->vbox, window);
  
  if (gp_wait_for_hide(window, ok, cancel) == 0) {
    gp_widget_free(cw);
    return;
  }
  
  camera_config_retrieve_rec(cw, window, settings, &set_count);
  
  if (gp_camera_config_set(gp_gtk_camera, settings, set_count) == GP_ERROR)
    /* error message */
    g_print("error\n");
  
  /* clean up and say byes */
  gp_widget_free(cw);
  gtk_widget_destroy(window);

} /* end config_camera_cb */

/*********** end camera callback section **********/

/************* start some other cb section ****************/


void populate_dir(GtkWidget *tree_item, gpointer data) {
  /* change this name */
  
  char *path = (char *)gtk_object_get_data(GTK_OBJECT(tree_item),"path");

  /* can't do this or the data gets corrupted after a bit, perhaps
     gtk handles this on it's own? */
  /* free(path); */
  
  create_sub_tree((char *)path,"y",tree_item);
  
} /* end populate_dir */
