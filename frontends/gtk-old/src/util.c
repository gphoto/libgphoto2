#include "gphoto.h"
#include "util.h"
#include <stdio.h>    

void hide_dialog (GtkWidget *dialog, gpointer data) {

	gtk_widget_hide(dialog);
}

int wait_for_hide (GtkWidget *dialog) {

	int done=0;
	gtk_signal_connect(GTK_OBJECT(dialog), "delete_event",
		GTK_SIGNAL_FUNC(hide_dialog), NULL);

	gtk_widget_show(dialog);
	while (!done) {
		if(!GTK_IS_OBJECT(dialog))
			/* the window manager could destroy the window */
			return 0;
		if (GTK_WIDGET_VISIBLE(dialog))
			gtk_main_iteration();
		else
			done=1;
	}
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
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(dirsel),
		filesel_cwd);
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
	gtk_entry_set_text(GTK_ENTRY(entry), filesel_cwd);
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
