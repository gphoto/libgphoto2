#include <gtk/gtk.h>

int wait_for_hide (GtkWidget *dialog, GtkWidget *ok_button, GtkWidget *cancel_button);

GtkWidget *gtk_directory_selection_new(char *title);
