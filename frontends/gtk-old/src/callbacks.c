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

void debug_print (char *message) {
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
	debug_print("open photo");
}

void open_directory() {
	debug_print("open directory");
}

void save_opened_photo() {
	debug_print("save opened photo");
}

void save_selected_photo() {
	debug_print("save selected photo");
}

void export_gallery() {
	debug_print("export gallery");

}

void print_photo() {
	debug_print("print photo");

}

void close_photo() {
	debug_print("close photo");

}


/* Editing operations */
/* ----------------------------------------------------------- */

void flip_horizontal() {
	debug_print("flip horizontal");

}

void flip_vertical() {
	debug_print("flip vertical");

}

void rotate_90() {
	debug_print("rotate 90");

}

void rotate_180() {
	debug_print("rotate 180");

}

void rotate_270() {
	debug_print("rotate 270");

}

void size_scale() {
	debug_print("size scale");

}

void size_half() {
	debug_print("size half");

}

void size_double() {
	debug_print("size double");

}


/* Selection operations */
/* ----------------------------------------------------------- */

void select_all() {
	debug_print("select all");

}

void select_inverse() {
	debug_print("select inverse");

}

void select_none() {
	debug_print("select none");

}


/* Camera operations */
/* ----------------------------------------------------------- */

void camera_select_update_port(GtkWidget *entry, gpointer data) {

	GtkWidget *window, *speed;
	CameraPortType type = GP_PORT_NONE;
	char *t, *new_port, *path;
	char buf[1024];

	new_port = (char*)gtk_entry_get_text(GTK_ENTRY(entry));
	if (strlen(new_port)==0)
		return;

	sprintf(buf, "Selected port: %s\n", new_port);
	debug_print(buf);

	window = (GtkWidget*)data;
	speed  = (GtkWidget*)lookup_widget(window, "speed");
	t      = (char*)gtk_object_get_data (GTK_OBJECT(window), new_port);
	type   = (CameraPortType)atoi(t);

	sprintf(buf, "%s path", new_port);
	path = gtk_object_get_data(GTK_OBJECT(window), buf);

	if (type == GP_PORT_SERIAL)
		gtk_widget_set_sensitive(GTK_WIDGET(speed), TRUE);
}

void camera_select_update_camera(GtkWidget *entry, gpointer data) {

	/* populate the speed list */
	GtkWidget *window, *port, *speed;
	GList *port_list, *speed_list;
	CameraAbilities a;
	CameraPortInfo info;
	char *new_camera;
	char buf[1024], buf1[1024];
	int num_ports, x=0, append;

	new_camera = (char*)gtk_entry_get_text(GTK_ENTRY(entry));
	if (strlen(new_camera)==0)
		return;

	window = (GtkWidget*)data;
	port   = (GtkWidget*)lookup_widget(window, "port");
	speed  = (GtkWidget*)lookup_widget(window, "speed");

	sprintf(buf, "Selected camera: %s\n", new_camera);
	debug_print(buf);

	if (gp_camera_abilities_by_name(new_camera, &a)==GP_ERROR) {
		sprintf(buf, "Could not get abilities for %s\n", new_camera);
		debug_print(buf);
		return;
	}

	/* Do nothing if the device doesn't support any ports (ports ports) */
	if (!a.serial && !a.parallel && !a.ieee1394 && !a.socket && !a.usb)
		return;

	/* Enable the port drop-down */
	gtk_widget_set_sensitive(GTK_WIDGET(port), TRUE);

	/* Get the number of ports */
	if ((num_ports = gp_port_count())==GP_ERROR) {
		gp_interface_message("Could not retrieve number of ports");
		return;
	}

	/* Free the old port list?? */

	/* populate the port list */
	port_list = g_list_alloc();
	for (x=0; x<num_ports; x++) {
		append=0;
		if (gp_port_info(x, &info)==GP_OK) {
			if ((info.type == GP_PORT_SERIAL) && (a.serial))
				append=1;
			if ((info.type == GP_PORT_PARALLEL) && (a.parallel))
				append=1;
			if ((info.type == GP_PORT_IEEE1394) && (a.ieee1394))
				append=1;
			if ((info.type == GP_PORT_SOCKET) && (a.socket))
				append=1;
			if ((info.type == GP_PORT_USB) && (a.usb))
				append=1;
			if (append) {
				sprintf(buf, "%s (%s)", info.name, info.path);
				port_list = g_list_append(port_list, strdup(buf));
				/* Associate a path with this entry */
				sprintf(buf1, "%s path", buf);
				gtk_object_set_data(GTK_OBJECT(window), 
					buf1, (gpointer)strdup(info.path));
				/* Associate a type with this entry */
				sprintf(buf1, "%i", info.type);
				gtk_object_set_data(GTK_OBJECT(window), 
					buf, (gpointer)strdup(buf1));
			}
		}  else {
			gp_interface_message("Error retrieving the port list");
		}
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(port), port_list);

	/* Free the old speed list?? */

	/* Populate the speed list */
	speed_list = g_list_alloc();
	x=0;
	while (a.speed[x] != 0) {
		sprintf(buf, "%i", a.speed[x++]);
		speed_list = g_list_append(speed_list, strdup(buf));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(speed), speed_list);

	
}

void camera_select() {
	GtkWidget *window, *ok, *cancel, *camera, *port, *speed;
	GList *camera_list;
	CameraPortSettings port_settings;
	int num_cameras, x;
	char buf[1024];
	char *camera_name, *port_name, *port_path, *speed_name;
	
	debug_print("camera select");

	/* Get the number of cameras */
	if ((num_cameras = gp_camera_count())==GP_ERROR) {
		gp_interface_message("Could not retrieve number of cameras");
		return;
	}

	window = create_select_camera_window();
	ok     = (GtkWidget*)lookup_widget(window, "ok");
	cancel = (GtkWidget*)lookup_widget(window, "cancel");
	camera = (GtkWidget*)lookup_widget(window, "camera");
	port   = (GtkWidget*)lookup_widget(window, "port");
	speed  = (GtkWidget*)lookup_widget(window, "speed");

	gtk_widget_set_sensitive(GTK_WIDGET(port), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(speed), FALSE);

	/* populate the camera list */
	camera_list = g_list_alloc();
	for (x=0; x<num_cameras; x++) {
		if (gp_camera_name(x, buf)==GP_OK)
			camera_list = g_list_append(camera_list, strdup(buf));
		   else
			camera_list = g_list_append(camera_list, "ERROR");
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(camera), camera_list);

	/* Update the dialog if they change the camera */
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(camera)->entry), "changed",
		GTK_SIGNAL_FUNC (camera_select_update_camera), (gpointer)window);

	/* Update the dialog if they change the port */
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(port)->entry), "changed",
		GTK_SIGNAL_FUNC (camera_select_update_port), (gpointer)window);

	if (gp_get_setting("camera", buf)==GP_OK)
		gtk_entry_set_text(GTK_ENTRY(camera), buf);
camera_select_again:
	if (wait_for_hide(window, ok, cancel) == 0) {
		debug_print("clicked cancel!\n");
		return;
	}

	debug_print("clicked ok!\n");

	camera_name = (char*)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(camera)->entry));
	port_name   = (char*)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(port)->entry));
	speed_name  = (char*)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(speed)->entry));

	if (strlen(camera_name)==0) {
		gp_message("You must choose a camera model");
		goto camera_select_again;
	}

	if (strlen(port_name)==0) {
		gp_message("You must choose a port");
		goto camera_select_again;
	}

	if ((strlen(speed_name)==0)&&(GTK_WIDGET_SENSITIVE(speed))) {
		gp_message("You must choose a port speed");
		goto camera_select_again;
	}

	sprintf(buf, "%s path", port_name);
	port_path = gtk_object_get_data(GTK_OBJECT(window), buf);

	strcpy(port_settings.path, port_path);
	port_settings.speed = atoi(speed_name);
/*
	if (gp_camera_set_by_name(
	gp_setting_set("camera", (char*)gtk_
*/
	/* Set the camera */
	/* Set the port */
}

void camera_index_thumbnails() {
	debug_print("camera index thumbnails");

}

void camera_index_no_thumbnails() {
	debug_print("camera index no thumbnails");

}

void camera_download_thumbnails() {
	debug_print("camera download thumbnails");

}

void camera_download_photos() {
	debug_print("camera download photos");

}

void camera_download_both() {
	debug_print("camera download both");

}

void camera_delete_selected() {
	debug_print("camera delete selected");

}

void camera_delete_all() {
	debug_print("camera delete all");

}

void camera_configure() {
	debug_print("camera configure");

}

void camera_show_information() {
	debug_print("camera information");

}

void camera_show_manual() {
	debug_print("camera manual");

}

void camera_show_about() {
	debug_print("camera about");

}


/* Help operations */
/* ----------------------------------------------------------- */

void help_about() {
	debug_print("help about");

}

void help_authors() {
	debug_print("help authors");

}

void help_license() {
	debug_print("help license");

}

void help_manual() {
	debug_print("help manual");

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
