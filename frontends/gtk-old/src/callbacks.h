#include <gtk/gtk.h>

int main_quit(GtkWidget *widget, gpointer data);
void debug_print (char *message);
void idle();

/* General callbacks */
/* ------------------------------------------------------*/
/* File operations */
void open_photo();
void open_directory();
void save_opened_photo();
void save_selected_photo();
void export_gallery();
void print_photo();
void close_photo();

/* Editing operations */
void flip_horizontal();
void flip_vertical();
void rotate_90();
void rotate_180();
void rotate_270();
void size_scale();
void size_half();
void size_double();

/* Selection operations */
void select_all();
void select_inverse();
void select_none();

/* Folder operations */
GtkWidget *tree_item_icon (GtkWidget *tree, char *text, char *icon_name);
GtkWidget *folder_item      (GtkWidget *tree, char *text);

void folder_set (GtkWidget *tree_item, gpointer data);
void folder_expand (GtkWidget *tree_item, gpointer data);

/* Camera operations */
void camera_select();
void camera_index_thumbnails();
void camera_index_no_thumbnails();
void camera_download_thumbnails();
void camera_download_photos();
void camera_download_both();
void camera_delete_selected();
void camera_delete_all();
void camera_configure();
void camera_show_information();
void camera_show_manual();
void camera_show_about();

/* Help operations */
void help_about();
void help_authors();
void help_license();
void help_manual();



/* Menu callbacks */
void on_open_photo_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_open_directory_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_save_open_photo_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_save_photo_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_html_gallery_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_print_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_close_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_exit_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_flip_horizontal_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_flip_vertical_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_90_degrees_activate (GtkMenuItem *menuitem, gpointer user_data);;
void on_180_degrees_activate (GtkMenuItem *menuitem, gpointer user_data);;
void on_270_degrees_activate (GtkMenuItem *menuitem, gpointer user_data);;
void on_scale_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_scale_half_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_scale_double_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_select_all_activate (GtkMenuItem *menuitem, gpointer user_data);;
void on_select_inverse_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_select_none_activate (GtkMenuItem *menuitem, gpointer user_data);;
void on_select_camera_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_index_thumbnails_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_index_no_thumbnails_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_download_thumbnails_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_download_photos_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_download_both_activate (GtkMenuItem *menuitem, gpointer user_data);;
void on_delete_photos_activate (GtkMenuItem *menuitem, gpointer user_data);;
void on_delete_all_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_configure_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_driver_information_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_driver_manual_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_driver_about_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_manual_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_authors_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_license_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_about_activate (GtkMenuItem *menuitem, gpointer user_data);
