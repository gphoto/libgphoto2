#include <gtk/gtk.h>

int wait_for_hide (GtkWidget *dialog, GtkWidget *ok_button, GtkWidget *cancel_button);
int exec_command (char *command, char *args);
int file_exists (char *filename);
GtkWidget *gtk_directory_selection_new(char *title);
int gdk_image_new_from_data (char *image_data, int image_size, int scale,
	GdkPixmap **pixmap, GdkBitmap **bitmap);
