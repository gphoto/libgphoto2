#include <dirent.h>
#include <stdio.h>    
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <gphoto2.h>
#include "util.h"

void ok_click (GtkWidget *dialog) {

        gtk_object_set_data(GTK_OBJECT(dialog), "button", "OK");
        gtk_widget_hide(dialog);
}
 
int wait_for_hide (GtkWidget *dialog, 
		   GtkWidget *ok_button,
                   GtkWidget *cancel_button) {
                   
	int cont=1;
	
        gtk_object_set_data(GTK_OBJECT(dialog), "button", "CANCEL");
        gtk_signal_connect_object(
                        GTK_OBJECT(ok_button), "clicked",
                        GTK_SIGNAL_FUNC(ok_click),
                        GTK_OBJECT(dialog));
	if (cancel_button)
	        gtk_signal_connect_object(GTK_OBJECT(cancel_button),
	                        "clicked",
	                        GTK_SIGNAL_FUNC(gtk_widget_hide),
	                        GTK_OBJECT(dialog));
        gtk_widget_show(dialog);
        while (cont) {
		/* because the window manager could destroy the window */
		if(!GTK_IS_OBJECT(dialog))
			return 0;
		if (GTK_WIDGET_VISIBLE(dialog))
			gtk_main_iteration();
		else
			cont = 0;
	}
        if (strcmp("CANCEL",
           (char*)gtk_object_get_data(GTK_OBJECT(dialog), "button"))==0)
                return 0;
        return 1;
}

void gtk_directory_selection_update(GtkWidget *entry, GtkWidget *dirsel) {

	DIR *dir=NULL;

	char *dirname;

	dir = opendir(gtk_entry_get_text(GTK_ENTRY(entry)));

	if (dir == NULL)
		/* not a directory */
		return;

	closedir(dir);

	dirname = (char *)malloc(sizeof(char)*strlen(
		gtk_entry_get_text(GTK_ENTRY(entry)))+2);
	strcpy(dirname, gtk_entry_get_text(GTK_ENTRY(entry)));
	if (dirname[strlen(dirname)-2] != '/')
		strcat(dirname, "/");

	gtk_file_selection_set_filename(
		GTK_FILE_SELECTION(dirsel),dirname);

	free(dirname);
}

GtkWidget *gtk_directory_selection_new(char *title) {

	GtkWidget *dirsel, *entry;

	GList *child;

	dirsel = gtk_file_selection_new(title);
	gtk_window_set_position (GTK_WINDOW (dirsel), GTK_WIN_POS_CENTER);
/*
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(dirsel),
		filesel_cwd);
*/
        gtk_widget_hide(GTK_FILE_SELECTION(dirsel)->selection_entry);
        gtk_widget_hide(GTK_FILE_SELECTION(dirsel)->selection_text);

	/* Hide the file selection */
        /* get the main vbox children */
        child = gtk_container_children(
                GTK_CONTAINER(GTK_FILE_SELECTION(dirsel)->main_vbox));
        /* get the dir/file list box children */
        child = gtk_container_children(
                GTK_CONTAINER(child->next->next->data));
        gtk_widget_hide(GTK_WIDGET(child->next->data));

	entry = gtk_entry_new();
	gtk_widget_show(entry);
//	gtk_entry_set_text(GTK_ENTRY(entry), filesel_cwd);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
		GTK_SIGNAL_FUNC(gtk_directory_selection_update),
		dirsel);
	gtk_box_pack_start_defaults(GTK_BOX(
		GTK_FILE_SELECTION(dirsel)->main_vbox), entry);
	gtk_box_reorder_child(GTK_BOX(
		GTK_FILE_SELECTION(dirsel)->main_vbox), entry, 2);
	gtk_widget_grab_focus(entry);
	return(dirsel);
}
