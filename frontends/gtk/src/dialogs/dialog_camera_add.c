#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#ifdef WITHOUT_GNOME
#  include <gdk/gdkkeysyms.h>
#  include <gtk/gtk.h>
#  include "../support.h"
#  ifdef ENABLE_NLS
#    include <libintl.h>
#    undef _
#    define _(String) dgettext (PACKAGE, String)
#    ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#    else
#      define N_(String) (String)
#    endif
#  else
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  include <gnome.h>
#  include <glade/glade.h>
#endif
#include <gphoto2.h>
#include "dialogs.h"

#ifdef WITHOUT_GNOME

/******************************************************************************/
/* Non-gnome stuff                                                            */
/******************************************************************************/

/* Functions */

gboolean on_combo_entry_model_focus_out_event (
	GtkWidget *widget,
        GdkEventFocus *event,
        gpointer user_data)
{
	gchar *model;
	CameraAbilities abilities;
	GList *abilities_list;
	guint i;

	/* Fill speed box */
	model = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (
                        lookup_widget (widget, "combo_model"))->entry));
	if (strcmp ("", model) == 0) return (FALSE);
        if (gp_camera_abilities_by_name (model, &abilities) == GP_ERROR) {
		gp_frontend_message (NULL, "Could not get camera abilities!");
		gtk_widget_destroy (
			lookup_widget (widget, "window_camera_add"));
	}
	abilities_list = g_list_alloc ();
	i = 0;
	while (abilities.speed[i] != 0) {
		g_list_append (
			abilities_list, 
			g_strdup_printf ("%i", abilities.speed[i]));
		i++;
	}
	gtk_combo_set_popdown_strings (
                GTK_COMBO (lookup_widget (widget, "combo_speed")),
		abilities_list);
	return (FALSE);
}

void on_button_ok_clicked (GtkWidget *widget, gpointer *user_data)
{
	//FIXME: Actually save the settings
	//Check if all gtk_combos are filled out. Get the values.
	//Write them to a file via external function.
	g_warning ("Not yet implemented!");
	gtk_widget_destroy (widget->parent->parent->parent);
}

void on_button_cancel_clicked (GtkWidget *widget, gpointer *user_data)
{
	gtk_widget_destroy (widget->parent->parent->parent);
}

GtkWidget* create_window_camera_add (void)
{
	GtkWidget *window_camera_add;
	GtkWidget *vbox_camera_add;
	GtkWidget *label_model;
	GtkWidget *combo_model;
	GtkWidget *combo_entry_model;
	GtkWidget *label_port;
	GtkWidget *combo_port;
	GtkWidget *combo_entry_port;
	GtkWidget *label_speed;
	GtkWidget *combo_speed;
	GtkWidget *combo_entry_speed;
	GtkWidget *hbuttonbox;
	GtkWidget *button_ok;
	GtkWidget *button_cancel;
	
	window_camera_add = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data (GTK_OBJECT (window_camera_add), "window_camera_add", window_camera_add);
	gtk_window_set_title (GTK_WINDOW (window_camera_add), _("Camera Selection"));
	
	vbox_camera_add = gtk_vbox_new (FALSE, 15);
	gtk_widget_ref (vbox_camera_add);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "vbox_camera_add", vbox_camera_add,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox_camera_add);
	gtk_container_add (GTK_CONTAINER (window_camera_add), vbox_camera_add);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_camera_add), 10);
	
	label_model = gtk_label_new (_("Please select your camera model."));
	gtk_widget_ref (label_model);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "label_model", label_model,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_model);
	gtk_box_pack_start (GTK_BOX (vbox_camera_add), label_model, TRUE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label_model), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label_model), TRUE);
	
	combo_model = gtk_combo_new ();
	gtk_widget_ref (combo_model);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "combo_model", combo_model,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (combo_model);
	gtk_box_pack_start (GTK_BOX (vbox_camera_add), combo_model, FALSE, FALSE, 0);
	
	combo_entry_model = GTK_COMBO (combo_model)->entry;
	gtk_widget_ref (combo_entry_model);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "combo_entry_model", combo_entry_model,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (combo_entry_model);
	
	label_port = gtk_label_new (_("Please select the port."));
	gtk_widget_ref (label_port);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "label_port", label_port,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_port);
	gtk_box_pack_start (GTK_BOX (vbox_camera_add), label_port, TRUE, FALSE, 0);
	gtk_label_set_line_wrap (GTK_LABEL (label_port), TRUE);
	
	combo_port = gtk_combo_new ();
	gtk_widget_ref (combo_port);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "combo_port", combo_port,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (combo_port);
	gtk_box_pack_start (GTK_BOX (vbox_camera_add), combo_port, FALSE, FALSE, 0);
	
	combo_entry_port = GTK_COMBO (combo_port)->entry;
	gtk_widget_ref (combo_entry_port);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "combo_entry_port", combo_entry_port,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (combo_entry_port);
	
	label_speed = gtk_label_new (_("Please select the speed."));
	gtk_widget_ref (label_speed);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "label_speed", label_speed,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_speed);
	gtk_box_pack_start (GTK_BOX (vbox_camera_add), label_speed, TRUE, FALSE, 0);
	gtk_label_set_line_wrap (GTK_LABEL (label_speed), TRUE);
	
	combo_speed = gtk_combo_new ();
	gtk_widget_ref (combo_speed);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "combo_speed", combo_speed,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (combo_speed);
	gtk_box_pack_start (GTK_BOX (vbox_camera_add), combo_speed, FALSE, FALSE, 0);
	
	combo_entry_speed = GTK_COMBO (combo_speed)->entry;
	gtk_widget_ref (combo_entry_speed);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "combo_entry_speed", combo_entry_speed,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (combo_entry_speed);
	
	hbuttonbox = gtk_hbutton_box_new ();
	gtk_widget_ref (hbuttonbox);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "hbuttonbox", hbuttonbox,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (hbuttonbox);
	gtk_box_pack_start (GTK_BOX (vbox_camera_add), hbuttonbox, TRUE, TRUE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_SPREAD);
	
	button_ok = gtk_button_new_with_label (_("OK"));
	gtk_widget_ref (button_ok);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "button_ok", button_ok,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (button_ok);
	gtk_container_add (GTK_CONTAINER (hbuttonbox), button_ok);
	GTK_WIDGET_SET_FLAGS (button_ok, GTK_CAN_DEFAULT);
	
	button_cancel = gtk_button_new_with_label (_("Cancel"));
	gtk_widget_ref (button_cancel);
	gtk_object_set_data_full (GTK_OBJECT (window_camera_add), "button_cancel", button_cancel,
	                          (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (button_cancel);
	gtk_container_add (GTK_CONTAINER (hbuttonbox), button_cancel);
	GTK_WIDGET_SET_FLAGS (button_cancel, GTK_CAN_DEFAULT);

	/* Connect signals */
	gtk_signal_connect (GTK_OBJECT (combo_entry_model), "focus_out_event",
        	GTK_SIGNAL_FUNC (on_combo_entry_model_focus_out_event),
                NULL);
	gtk_signal_connect (GTK_OBJECT (button_ok), "clicked",
                GTK_SIGNAL_FUNC (on_button_ok_clicked),
                NULL);
	gtk_signal_connect (GTK_OBJECT (button_cancel), "clicked",
		GTK_SIGNAL_FUNC (on_button_cancel_clicked), 
		NULL);

	return window_camera_add;
}

void dialog_camera_add (void)
{
	GtkWidget *window_camera_add;
	gint number_of_models, number_of_ports;
	guint i;
	gchar buffer[1024];
	GList *model_list, *port_list;
	CameraPortInfo info;

	window_camera_add = create_window_camera_add ();
	gtk_widget_show (window_camera_add);
	
	/* Fill model combo box */
	if ((number_of_models = gp_camera_count ()) == GP_ERROR) {
		gp_frontend_message (NULL, _("Could not count cameras!"));
		gtk_widget_destroy (window_camera_add);
	}
	model_list = g_list_alloc ();
        for (i = 0; i < number_of_models; i++) {
        	if (gp_camera_name (i, buffer) == GP_ERROR) {
			gp_frontend_message (
				NULL, 
				_("Could not get camera name!"));
			gtk_widget_destroy (window_camera_add);
		}
		g_list_append (model_list, g_strdup (buffer));
	}
	gtk_combo_set_popdown_strings (
		GTK_COMBO (lookup_widget (window_camera_add, "combo_model")), 
		model_list);

	/* Fill port box */
	if ((number_of_ports = gp_port_count ()) == GP_ERROR) {
		gp_frontend_message (NULL, _("Could not count ports!"));
		gtk_widget_destroy (window_camera_add);
	}
	port_list = g_list_alloc ();
	for (i = 0; i < number_of_ports; i++) {
		if (gp_port_info (i, &info) == GP_ERROR) {
			gp_frontend_message (
				NULL, 
				_("Could not get port info!"));
			gtk_widget_destroy (window_camera_add);
		}
		g_list_append (port_list, g_strdup (info.name));
	}
	gtk_combo_set_popdown_strings (
		GTK_COMBO (lookup_widget (window_camera_add, "combo_port")),
		port_list);
}

#else

/******************************************************************************/
/* Gnome stuff                                                                */
/******************************************************************************/

/* Variables */

static GladeXML *xml;
static gint model_number;
static gint speed_number;
static gint port_number;

/* Functions */

gboolean on_druid_camera_add_cancel (GtkWidget *widget, GnomeDruid *druid) 
{
	/* The user clicked the cancel button. */
	gtk_widget_destroy (glade_xml_get_widget (xml, "window_camera_add"));
        gtk_object_unref (GTK_OBJECT (xml));
	return (FALSE);
}

gboolean on_druidpagefinish_finish (GtkWidget *page, GnomeDruid *druid)
{
	/* The user clicked the finish button */
	//FIXME: Actually save the settings!
	//Get values of model_number, speed_number, and port_number, and
	//look up the values in the gtk_clists.
	//Then, save them to disk via external function.
	g_warning ("Not yet implemented!");
	gtk_widget_destroy (glade_xml_get_widget (xml, "window_camera_add"));
        gtk_object_unref (GTK_OBJECT (xml));
	return (FALSE);
}

gboolean on_druidpagefinish_prepare (GtkWidget *page, GnomeDruid *druid)
{
	GtkWindow *window;

	window = GTK_WINDOW (glade_xml_get_widget (xml, "window_camera_add"));
	/* Check if the user has selected a camera model. */
	if (model_number == -1) {
		gnome_error_dialog_parented (
			_("Please select a camera model first!"),
			window);
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (
			glade_xml_get_widget (
				xml, 
				"druidpagestandard_model")));
		return (FALSE);
	}
	/* Check if the user has selected a port. */
	if (port_number == -1) {
                gnome_error_dialog_parented (
                        _("Please select a port first!"),
                        window);
                gnome_druid_set_page (druid, GNOME_DRUID_PAGE (
                        glade_xml_get_widget (
                                xml,
                                "druidpagestandard_port")));
		return (FALSE);
	}
	return (FALSE);
		
}

gboolean on_druidpagestandard_model_prepare (GtkWidget *page, GnomeDruid *druid)
{
	gint number_of_models;
	GtkWindow *window;
	guint i;
	gchar buffer[1024];
	gchar *message;
	gchar *clist_item[1];
	
	window = GTK_WINDOW (glade_xml_get_widget (xml, "window_camera_add"));
	if ((number_of_models = gp_camera_count ()) == GP_ERROR) {
		gnome_error_dialog_parented (
			_("Could not get number of cameras!"),
			window);
		return (FALSE);
	}
	if (number_of_models == 0) {
		gnome_error_dialog_parented (
			_("No cameras are supported!"),
			window);
		return (FALSE);
	}
	gtk_clist_clear (GTK_CLIST (glade_xml_get_widget (xml, "clist_model")));
	for (i = 0; i < number_of_models; i++) {
		if (gp_camera_name (i, buffer) == GP_ERROR) {
			message = g_strdup_printf (
				_("Could not get name of camera %i!"),
				i);
			gnome_dialog_run_and_close (GNOME_DIALOG (
				gnome_error_dialog_parented (
					message,
					window)));
			g_free (message);
			return (FALSE);
		}
		clist_item[0] = g_strdup_printf ("%s", buffer);
		gtk_clist_append (
			GTK_CLIST (glade_xml_get_widget (xml, "clist_model")),
			clist_item);
		
	}
	if (model_number != -1) gtk_clist_select_row (
		GTK_CLIST (glade_xml_get_widget (xml, "clist_model")),
		model_number,
		0);
	return (FALSE);
}

void on_clist_model_select_row (
	GtkWidget *widget,
	gint row,
	gint column,
	GdkEventButton *event,
	gpointer *data)
{
	model_number = row;
}

gboolean on_druidpagestandard_port_prepare (GtkWidget *page, GnomeDruid *druid)
{
	gint number_of_ports;
	guint i;
	gchar *message;
	CameraPortInfo info;
	GtkWindow *window;
	gchar *clist_item[2];

	window = GTK_WINDOW (glade_xml_get_widget (xml, "window_camera_add"));
	if ((number_of_ports = gp_port_count ()) == GP_ERROR) {
		gnome_error_dialog_parented (
			_("Could not get number of ports!"), 
			window); 
		return (FALSE);
	}
	if (number_of_ports == 0) {
		gnome_error_dialog_parented (
			_("No ports have been found!"),
			window);
		return (FALSE);
	}
	gtk_clist_clear (GTK_CLIST (glade_xml_get_widget (xml, "clist_port")));
	for (i = 0; i < number_of_ports; i++) {
		if (gp_port_info (i, &info) == GP_ERROR) {
			message = g_strdup_printf (
				_("Could not get information about port %i!"),
				i);
			gnome_dialog_run_and_close (GNOME_DIALOG (
				gnome_error_dialog_parented (
					message, 
					window)));
			g_free (message);
			return (FALSE);
		}
		clist_item[0] = info.name;
		clist_item[1] = info.path;
		gtk_clist_append (
			GTK_CLIST (glade_xml_get_widget (xml, "clist_port")), 
			clist_item);
	}
        if (port_number != -1) gtk_clist_select_row (
                GTK_CLIST (glade_xml_get_widget (xml, "clist_port")),
                port_number,
                0);
	return (FALSE);
}

void on_clist_port_select_row (
        GtkWidget *widget,
        gint row,
        gint column,
        GdkEventButton *event,
        gpointer *data)
{
        port_number = row;
}

gboolean on_druidpagestandard_speed_prepare (GtkWidget *page, GnomeDruid *druid)
{
	guint i;
	GtkWindow *window;
	gchar *clist_item[1];
	CameraAbilities abilities;
	gchar *message;

	window = GTK_WINDOW (glade_xml_get_widget (xml, "window_camera_add"));
	if (model_number == -1) {
		/* The user did not select a model yet. */
		return (FALSE);
	}
	if (gp_camera_abilities (model_number, &abilities) == GP_ERROR) {
		message = g_strdup_printf (
			_("Could not get abilities of camera %i!"),
			model_number);
		gnome_dialog_run_and_close (GNOME_DIALOG (
			gnome_error_dialog_parented (
				message,
				window)));
			g_free (message);
		return (FALSE);
	}
	i = 0;
	gtk_clist_clear (GTK_CLIST (glade_xml_get_widget (xml, "clist_speed")));
	while (abilities.speed[i] != 0) {
		clist_item[0] = g_strdup_printf ("%i", abilities.speed[i]);
		gtk_clist_append (
			GTK_CLIST (glade_xml_get_widget (xml, "clist_speed")),
			clist_item);
		i++;
	}
        if (speed_number != -1) gtk_clist_select_row (
                GTK_CLIST (glade_xml_get_widget (xml, "clist_speed")),
                speed_number,
                0);
	return (FALSE);
}

void on_clist_speed_select_row (
        GtkWidget *widget,
        gint row,
        gint column,
        GdkEventButton *event,
        gpointer *data)
{
	/* Check if user deselected a speed */
	if (row == speed_number) speed_number = -1;
        else speed_number = row;
}

void dialog_camera_add (void)
{
	xml = NULL;
	model_number = -1;
	port_number = -1;
	speed_number = -1;
	xml = glade_xml_new (
		GPHOTO_GLADEDIR "camera_add.glade", 
		"window_camera_add");
	if (!xml) {
		gnome_error_dialog (
			"Could not find " GPHOTO_GLADEDIR "camera_add.glade!\n"
			"Please check if gphoto has been installed correctly!");
	} else {
		glade_xml_signal_autoconnect (xml);
	}
}

#endif
