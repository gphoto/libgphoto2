#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gphoto2.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "globals.h"
#include "util.h"

void debug (char *message) {
	if (gp_gtk_debug)
		printf("%s\n", message);
}

int main_quit(GtkWidget *widget, gpointer data) {

	gp_exit();
	gtk_main_quit();

	return (FALSE);
}

/* File operations */
/* ----------------------------------------------------------- */

void open_photo() {
	debug("open photo");
}

void open_directory() {
	debug("open directory");
}

void save_opened_photo() {
	debug("save opened photo");
}

void save_selected_photo() {
	debug("save selected photo");
}

void export_gallery() {
	debug("export gallery");

}

void print_photo() {
	debug("print photo");

}

void close_photo() {
	debug("close photo");

}


/* Editing operations */
/* ----------------------------------------------------------- */

void flip_horizontal() {
	debug("flip horizontal");

}

void flip_vertical() {
	debug("flip vertical");

}

void rotate_90() {
	debug("rotate 90");

}

void rotate_180() {
	debug("rotate 180");

}

void rotate_270() {
	debug("rotate 270");

}

void size_scale() {
	debug("size scale");

}

void size_half() {
	debug("size half");

}

void size_double() {
	debug("size double");

}


/* Selection operations */
/* ----------------------------------------------------------- */

void select_all() {
	debug("select all");

}

void select_inverse() {
	debug("select inverse");

}

void select_none() {
	debug("select none");

}


/* Camera operations */
/* ----------------------------------------------------------- */

void camera_select() {
	GtkWidget *window, *ok, *cancel, *camera, *port;
	GList *camera_list, *port_list;
	CameraPortInfo info;
	int num_cameras, num_ports, x;
	char buf[1024];
	
	debug("camera select");

	/* Get the number of cameras */
	if ((num_cameras = gp_camera_count())==GP_ERROR) {
		gp_interface_message("Could not retrieve number of cameras");
		return;
	}

	/* Get the number of ports */
	if ((num_ports = gp_port_count())==GP_ERROR) {
		gp_interface_message("Could not retrieve number of ports");
		return;
	}

	/* Create the dialog (thank you GLADE!) */
	window = create_select_camera_window();
	ok     = (GtkWidget*)lookup_widget(window,"ok");
	cancel = (GtkWidget*)lookup_widget(window,"cancel");
	camera = (GtkWidget*)lookup_widget(window, "camera");
	port   = (GtkWidget*)lookup_widget(window, "port");

	/* populate the camera list */
	camera_list = g_list_alloc();
	for (x=0; x<num_cameras; x++) {
		if (gp_camera_name(x, buf)==GP_OK)
			camera_list = g_list_append(camera_list, strdup(buf));
		   else
			camera_list = g_list_append(camera_list, "ERROR");
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(camera), camera_list);

	/* populate the port list */
	port_list = g_list_alloc();
	for (x=0; x<num_ports; x++) {
		if (gp_port_info(x, &info)==GP_OK) {
			sprintf(buf, "%s (%s)", info.name, info.path);
			port_list = g_list_append(port_list, strdup(buf));
		}  else {
			port_list = g_list_append(port_list, "ERROR");
		}
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(port), port_list);

	/* populate the speed list from abilities */

	if (wait_for_hide(window, ok, cancel) == 0) {
		debug("clicked cancel!\n");
		return;
	}
	
	debug("clicked ok!\n");
	/* Set the camera */
	/* Set the port */
}

void camera_index_thumbnails() {
	debug("camera index thumbnails");

}

void camera_index_no_thumbnails() {
	debug("camera index no thumbnails");

}

void camera_download_thumbnails() {
	debug("camera download thumbnails");

}

void camera_download_photos() {
	debug("camera download photos");

}

void camera_download_both() {
	debug("camera download both");

}

void camera_delete_selected() {
	debug("camera delete selected");

}

void camera_delete_all() {
	debug("camera delete all");

}

void camera_configure() {
	debug("camera configure");

}

void camera_show_information() {
	debug("camera information");

}

void camera_show_manual() {
	debug("camera manual");

}

void camera_show_about() {
	debug("camera about");

}


/* Help operations */
/* ----------------------------------------------------------- */

void help_about() {
	debug("help about");

}

void help_authors() {
	debug("help authors");

}

void help_license() {
	debug("help license");

}

void help_manual() {
	debug("help manual");

}

/* Menu callbacks. just calls above mentioned functions */
void on_open_photo_activate (GtkMenuItem *menuitem, gpointer user_data) {
	open_photo();
}

void on_open_directory_activate (GtkMenuItem *menuitem, gpointer user_data) {
	open_directory();
}


void on_save_open_photo_activate (GtkMenuItem *menuitem, gpointer user_data) {
	save_opened_photo();
}


void on_save_photo_activate (GtkMenuItem *menuitem, gpointer user_data) {
	save_selected_photo();
}


void on_html_gallery_activate (GtkMenuItem *menuitem, gpointer user_data) {
	export_gallery();
}


void on_print_activate (GtkMenuItem *menuitem, gpointer user_data) {
	print_photo();
}


void on_close_activate (GtkMenuItem *menuitem, gpointer user_data) {
	close_photo();
}


void on_exit_activate (GtkMenuItem *menuitem, gpointer user_data) {
	main_quit(NULL, NULL);
}


void on_flip_horizontal_activate (GtkMenuItem *menuitem, gpointer user_data) {
	flip_horizontal();
}


void on_flip_vertical_activate (GtkMenuItem *menuitem, gpointer user_data) {
	flip_vertical();
}


void on_90_degrees_activate (GtkMenuItem *menuitem, gpointer user_data) {
	rotate_90();
}


void on_180_degrees_activate (GtkMenuItem *menuitem, gpointer user_data) {
	rotate_180();
}


void on_270_degrees_activate (GtkMenuItem *menuitem, gpointer user_data) {
	rotate_270();
}


void on_scale_activate (GtkMenuItem *menuitem, gpointer user_data) {
	size_scale();
}


void on_scale_half_activate (GtkMenuItem *menuitem, gpointer user_data) {
	size_half();
}


void on_scale_double_activate (GtkMenuItem *menuitem, gpointer user_data) {
	size_double();
}


void on_select_all_activate (GtkMenuItem *menuitem, gpointer user_data) {
	select_all();
}


void on_select_inverse_activate (GtkMenuItem *menuitem, gpointer user_data) {
	select_inverse();
}


void on_select_none_activate (GtkMenuItem *menuitem, gpointer user_data) {
	select_none();
}


void on_select_camera_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_select();
}


void on_index_thumbnails_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_index_thumbnails();
}


void on_index_no_thumbnails_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_index_no_thumbnails();
}


void on_download_thumbnails_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_download_thumbnails();
}


void on_download_photos_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_download_photos();
}


void on_download_both_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_download_both();
}


void on_delete_photos_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_delete_selected();
}


void on_delete_all_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_delete_all();
}


void on_configure_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_configure();
}


void on_driver_information_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_show_information();
}


void on_driver_manual_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_show_manual();
}


void on_driver_about_activate (GtkMenuItem *menuitem, gpointer user_data) {
	camera_show_about();
}


void on_manual_activate (GtkMenuItem *menuitem, gpointer user_data) {
	help_manual();
}


void on_authors_activate (GtkMenuItem *menuitem, gpointer user_data) {
	help_authors();
}


void on_license_activate (GtkMenuItem *menuitem, gpointer user_data) {
	help_license();
}


void on_about_activate (GtkMenuItem *menuitem, gpointer user_data) {
	help_about();
}
