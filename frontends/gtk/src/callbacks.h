#include <gtk/gtk.h>

gboolean exit_callback(GtkMenuItem *menuitem, gpointer user_data);
gboolean close_callback(GtkMenuItem *menuitem, gpointer user_data);

void display_window(GtkMenuItem *menuitem, gint window);

void select_camera_callback(GtkMenuItem *menuitem, gpointer user_data);
