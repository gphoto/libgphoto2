#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gphoto2.h>

#include "callbacks.h"
#include "gtkiconlist.h"
#include "interface.h"
#include "support.h"
#include "globals.h"
#include "util.h"

#include "../pixmaps/no_thumbnail.xpm"

void debug_print (char *message) {
	if (gp_gtk_debug)
		printf("%s\n", message);
}

void idle() {
	/* Let's GTK do some processing */

        while (gtk_events_pending())
                gtk_main_iteration();
	usleep(300000);
        while (gtk_events_pending())
                gtk_main_iteration();
}

int camera_set() {
	/* Takes the settings 'camera', 'port', and 'speed' and
	   sets it to the current camera */

	GtkWidget *message, *message_label, *camera_label;
	CameraPortSettings ps;
	char camera[1024], port[1024], speed[1024];

	/* create a transient window */
	message = create_message_window_transient();
	message_label = (GtkWidget*) lookup_widget(message, "message");
	gtk_label_set_text(GTK_LABEL(message_label), "Initializing camera...");

	if (gp_setting_get("camera", camera)==GP_ERROR) {
		gp_message("You must choose a camera");
		camera_select();
	}

	gp_setting_get("port", port);
	gp_setting_get("speed", speed);

	strcpy(ps.path, port);
	if (strlen(speed)>0)
		ps.speed = atoi(speed);
	   else
		ps.speed = 0; /* use the default speed */

	gtk_widget_show_all(message);
	idle();

	if (gp_camera_set_by_name(camera, &ps)==GP_ERROR) {
		gtk_widget_destroy(message);
		gp_gtk_camera_init = 0;
		return (GP_ERROR);
	}

	camera_label = (GtkWidget*) lookup_widget(gp_gtk_main_window, "camera_label");
	gtk_label_set_text(GTK_LABEL(camera_label), camera);
	gtk_widget_destroy(message);

	gp_gtk_camera_init = 1;
	return (GP_OK);
}

int main_quit(GtkWidget *widget, gpointer data) {

	char buf[1024];
	int x, y;

	/* Save the window size */
	gdk_window_get_size(gp_gtk_main_window->window, &x, &y);
	sprintf(buf, "%i", x);
	gp_setting_set("width", buf);
	sprintf(buf, "%i", y);
	gp_setting_set("height", buf);

	if (gp_gtk_camera_init)
		gp_exit();
	gtk_main_quit();

	return (FALSE);
}

/* File operations */
/* ----------------------------------------------------------- */

void append_photo(CameraFile *file) {
	debug_print("append photo");

}

void open_photo() {
	
	GtkWidget *filesel;
	char buf[1024];

	debug_print("open photo");

	filesel = gtk_file_selection_new("Open a photo");
	gp_setting_get("cwd", buf);
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), buf);

	if (wait_for_hide(filesel, GTK_FILE_SELECTION(filesel)->ok_button,
	    GTK_FILE_SELECTION(filesel)->cancel_button)==0)
		return;
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

/* Folder Callbacks */
/* ----------------------------------------------------------- */

void folder_set (GtkWidget *tree_item, gpointer data) {

	char buf[1024];
	char *path = (char*)gtk_object_get_data(GTK_OBJECT(tree_item), "path");

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

	sprintf(buf, "camera folder path = %s", path);
	debug_print(buf);

	if (gp_folder_set(path)==GP_ERROR) {
		sprintf(buf, "Could not open folder:\n%s", path);
		gp_message(buf);
		return;
	}
}

GtkWidget *folder_item (GtkWidget *tree, char *text) {
	/* Create an item in the "tree" with the label of text and a folder icon */

	return (tree_item_icon(tree, text, "folder.xpm"));
}

GtkWidget *tree_item_icon (GtkWidget *tree, char *text, char *icon_name) {
	/* Create an item in the "tree" with the label of text and an icon */

	GtkWidget *item, *hbox, *pixmap, *label, *subtree, *subitem;

	item = gtk_tree_item_new();
	gtk_widget_show(item);
	gtk_tree_append(GTK_TREE(tree), item);
	gtk_signal_connect(GTK_OBJECT(item), "select", 
		GTK_SIGNAL_FUNC(folder_set),NULL);
	gtk_signal_connect_after(GTK_OBJECT(item), "expand", 
		GTK_SIGNAL_FUNC(folder_expand),NULL);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(item), hbox);

	pixmap = create_pixmap(gp_gtk_main_window, icon_name);
	gtk_widget_show(pixmap);
	gtk_box_pack_start(GTK_BOX(hbox), pixmap, FALSE, FALSE, 0);

	label = gtk_label_new(text);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	subtree = gtk_tree_new();
	gtk_widget_show(subtree);
	gtk_tree_item_set_subtree (GTK_TREE_ITEM(item), subtree);

	subitem = gtk_tree_item_new();
	gtk_widget_show(subitem);
	gtk_tree_append(GTK_TREE(subtree), subitem);
	gtk_object_set_data(GTK_OBJECT(subitem), "blech", (gpointer)"foo");
	
	return (item);
}

void folder_expand (GtkWidget *tree_item, gpointer data) {

	GtkWidget *tree, *item;
	CameraFolderInfo list[256], t;
	char *path = (char*)gtk_object_get_data(GTK_OBJECT(tree_item), "path");
	char buf[1024];
	int x=0, y=0, z=0, count=0;

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

	tree = GTK_TREE_ITEM(tree_item)->subtree;

	/* See if we've expanded this before! */
	if (gtk_object_get_data(GTK_OBJECT(tree_item), "expanded")) return;

	if ((count = gp_folder_list(path, list))==GP_ERROR) {
		sprintf(buf, "Could not open folder\n%s", path);
		gp_message(buf);
		return;
	}

	if (count == 0) {
		if (strcmp(path, "/")!=0)
			gtk_tree_item_remove_subtree(GTK_TREE_ITEM(tree_item));
		return;
	}

	/* sort the folder list (old bubble) */
	for (x=0; x<count-1; x++) {
		for (y=x+1; y<count; y++) {
			z = strcmp(list[x].name, list[y].name);
			if (z > 0) {
				memcpy(&t, &list[x], sizeof(t));
				memcpy(&list[x], &list[y], sizeof(list[x]));
				memcpy(&list[y], &t, sizeof(list[y]));
			}
		}
	}

	/* Append the new folders */
	for (x=0; x<count; x++) {
		if (strcmp(path, "/")==0)
			sprintf(buf, "/%s", list[x].name);
		   else
			sprintf(buf, "%s/%s", path, list[x].name);
		item = folder_item(tree, list[x].name);
		gtk_object_set_data(GTK_OBJECT(item), "path", strdup(buf));
	}

	gtk_object_set_data(GTK_OBJECT(tree_item), "expanded", "yes");
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

	sprintf(buf, "Selected port: %s", new_port);
	debug_print(buf);

	window = (GtkWidget*)data;
	speed  = (GtkWidget*)lookup_widget(window, "speed");
	t      = (char*)gtk_object_get_data (GTK_OBJECT(window), new_port);
	if (!t) {
		debug_print("port type not being set");
		return;
	}

	type   = (CameraPortType)atoi(t);
	sprintf(buf, "%s path", new_port);
	path = gtk_object_get_data(GTK_OBJECT(window), buf);

	/* Disabled by default */
	gtk_widget_set_sensitive(GTK_WIDGET(speed), FALSE);
	if (type == GP_PORT_SERIAL)
		gtk_widget_set_sensitive(GTK_WIDGET(speed), TRUE);
	   else
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(speed)->entry), "");

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

	sprintf(buf, "Selected camera: %s", new_camera);
	debug_print(buf);

	if (gp_camera_abilities_by_name(new_camera, &a)==GP_ERROR) {
		sprintf(buf, "Could not get abilities for %s", new_camera);
		debug_print(buf);
		return;
	}

	/* Get the number of ports */
	if ((num_ports = gp_port_count())==GP_ERROR) {
		gp_message("Could not retrieve number of ports");
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
			if ((info.type == GP_PORT_NETWORK) && (a.network))
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
			gp_message("Error retrieving the port list");
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

	gtk_widget_set_sensitive(GTK_WIDGET(port), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(speed), FALSE);
	if (a.serial || a.parallel || a.ieee1394 || a.network || a.usb)
		gtk_widget_set_sensitive(GTK_WIDGET(port), TRUE);
}

void camera_select() {
	GtkWidget *window, *ok, *cancel, *camera, *port, *speed;
	GList *camera_list;
	int num_cameras, x;
	char buf[1024];
	char *camera_name, *port_name, *port_path, *speed_name;
	
	debug_print("camera select");

	/* Get the number of cameras */
	if ((num_cameras = gp_camera_count())==GP_ERROR) {
		gp_message("Could not retrieve number of cameras");
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

	/* Retrieve the saved values */
	if (gp_setting_get("camera", buf)==GP_OK)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(camera)->entry), buf);
	if (gp_setting_get("port name", buf)==GP_OK)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(port)->entry), buf);
	if (gp_setting_get("speed", buf)==GP_OK)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(speed)->entry), buf);

camera_select_again:
	if (wait_for_hide(window, ok, cancel) == 0)
		return;

	idle(); /* let GTK catch up */

	camera_name = (char*)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(camera)->entry));
	port_name   = (char*)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(port)->entry));
	speed_name  = (char*)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(speed)->entry));

	if (strlen(camera_name)==0) {
		gp_message("You must choose a camera model");
		goto camera_select_again;
	}

	if ((strlen(port_name)==0)&&(GTK_WIDGET_SENSITIVE(port))) {
		gp_message("You must choose a port");
		goto camera_select_again;
	}

	if ((strlen(speed_name)==0)&&(GTK_WIDGET_SENSITIVE(speed))) {
		gp_message("You must choose a port speed");
		goto camera_select_again;
	}

	gp_setting_set("camera", camera_name);

	if (GTK_WIDGET_SENSITIVE(port)) {
		sprintf(buf, "%s path", port_name);
		port_path = (char*)gtk_object_get_data(GTK_OBJECT(window), buf);
		gp_setting_set("port name", port_name);
		gp_setting_set("port", port_path);
	}  else {
		gp_setting_set("port name", "");
		gp_setting_set("port", "");
	}

	if (GTK_WIDGET_SENSITIVE(speed))
		gp_setting_set("speed", speed_name);
	   else
		gp_setting_set("speed", "");


	if (camera_set()==GP_ERROR)
		goto camera_select_again;

	/* Clean up */
	gtk_widget_destroy(window);
}

void camera_index_common(int thumbnails) {

	CameraFile *f;
	GtkWidget *icon_list;
	GtkIconListItem *item;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	char buf[1024];
	int x, count=0;

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

	icon_list = (GtkWidget*) lookup_widget(gp_gtk_main_window, "icons");
	GTK_ICON_LIST(icon_list)->is_editable = FALSE;

	if ((count = gp_file_count())==GP_ERROR) {
		gp_message("Could not retrieve the number of pictures");
		return;
	}

	gtk_icon_list_clear (GTK_ICON_LIST(icon_list));
	for (x=0; x<count; x++) {
		sprintf(buf,"#%04i", x);
		item = gtk_icon_list_add_from_data(GTK_ICON_LIST(icon_list),
			no_thumbnail_xpm,buf,NULL);
		if (thumbnails) {
			f = gp_file_new();
			if (gp_file_get_preview(x, f) == GP_OK) {
				gdk_image_new_from_data(f->data,f->size,1,&pixmap,&bitmap);
				gtk_pixmap_set(GTK_PIXMAP(item->pixmap), pixmap, bitmap);
			}
			gp_file_free(f);
		}
		idle();
		gp_progress(100.0*(float)x/(float)count);
	}
}

void camera_index_thumbnails() {

	debug_print("camera index thumbnails");

	camera_index_common(1);
}

void camera_index_no_thumbnails() {

	debug_print("camera index no thumbnails");

	camera_index_common(0);
}

void camera_download_thumbnails() {
	debug_print("camera download thumbnails");

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

}

void camera_download_photos() {
	debug_print("camera download photos");

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

}

void camera_download_both() {
	debug_print("camera download both");

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

}

void camera_delete_selected() {
	debug_print("camera delete selected");

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

}

void camera_delete_all() {
	debug_print("camera delete all");

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

}

void camera_configure() {

	debug_print("camera configure");

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}
}

void camera_show_information() {

	char buf[1024*32];

	debug_print("camera information");

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

	if (gp_summary(buf)==GP_ERROR) {
		gp_message("Could not retrieve camera information");
		return;
	}

	gp_message(buf);
}

void camera_show_manual() {

	char buf[1024*32];

	debug_print("camera manual");

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

	if (gp_manual(buf)==GP_ERROR) {
		gp_message("Could not retrieve the camera manual");
		return;
	}

	gp_message(buf);

}

void camera_show_about() {

	char buf[1024*32];

	debug_print("camera about");

	if (!gp_gtk_camera_init)
		if (camera_set()==GP_ERROR) {return;}

	if (gp_about(buf)==GP_ERROR) {
		gp_message("Could not retrieve library information");
		return;
	}

	gp_message(buf);
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
