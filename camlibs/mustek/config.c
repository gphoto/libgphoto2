/*
 * Copyright (C) 1999/2000 by 
 *     Henning Zabel <henning@uni-paderborn.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
/*
 * gphoto driver for the Mustek MDC800 Digital Camera. The driver 
 * supports rs232 and USB.
 */

/* FIXME: Needs to be converted to new config stuff first */


#include "config.h"
#include "mdc800.h"

#include "../src/gphoto.h"
#include "../src/util.h"
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <glib.h>
#include <gdk_imlib.h>
#include <gdk/gdk.h>

#include <errno.h>

#include <fcntl.h>

#include "core.h"
#include "print.h"
#include "io.h"

/****************************************************************************
	Dialog Data
*****************************************************************************/

struct ConfigValues {
	GtkWidget *cam_id;
	
	GtkWidget *image_quality;
	
	GtkWidget *lcd;
	GtkObject *lcd_adj;
	
	GtkWidget *light;
	GtkWidget *flash;
	GtkWidget *lcd_on;
	GtkWidget *source;
	GtkWidget *exp_mode;
	
	GtkWidget *baud;
	
	GtkWidget *save_button, *cancel_button;
	GtkWidget *dialog;
	
} mdc800_dialog;


char* mdc800_image_quality_string [3]  = {"Economic(506x384)","Standard(1012x768)","High(1012x768)"};
char* mdc800_storage_source_string [2] = {"Compact Flash Card","Internal Memory"};
char* mdc800_exposure_mode_string [2]  = {"MTRX","CNTR"};
char* mdc800_wb_string [4]             = {"Auto","Indoor","Indoor with Flashlight","Outdoor"};
char* mdc800_flash_light_string [5]    = {"Auto","Auto(Redeye Red.)","On","On(Redeye Red.)","Off"};
char* mdc800_baud_rate_string [3]      = {"19200","57600","115200"};

/*****************************************************************************
	Creating/Disposing GUI
******************************************************************************/

/*
 * Return a combo Box filled up with content, assuming that
 * content has count entries.
 */
GtkWidget* mdc800_createComboBox (char** content,int count)
{
	int 	 i=0;
	GList* list=0;
	
	GtkWidget* retval=gtk_combo_new ();
	for (i=0; i<count; i++)
		list=g_list_append (list, content [i] );
		
	gtk_widget_show (retval);
	gtk_combo_set_popdown_strings (GTK_COMBO (retval), list);
	gtk_entry_set_editable (GTK_ENTRY (GTK_COMBO (retval)->entry),0);
	return retval;
}


/* 
 * Sets up the item index in StringList list of
 * ComboBox box
 */
void mdc800_ComboBox_SetEntry (GtkWidget *box, char** list, int index)
{
	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (box)->entry), list[index]);
}


/*
 * Gets the index of the Item in StringList list
 * of ComboBox box
 */
int mdc800_ComboBox_GetEntry (GtkWidget *box, char** list,int length)
{
	int i=0;
	char* entry=gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (box)->entry));
	while (i<length)
	{
		if (strcmp (entry,list[i]) == 0)
			return i;
		i++;
	}
	return -1;
}


void mdc800_createDialog (void )
{
	long value;

	GtkWidget *table, *label, *spacer, *toggle;
	GtkObject *adj;

	mdc800_dialog.dialog = gtk_dialog_new();
	gtk_window_set_title (GTK_WINDOW(mdc800_dialog.dialog), "Configure Camera");
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(mdc800_dialog.dialog)->vbox),10);
	table = gtk_table_new(16,5,FALSE);
	gtk_widget_show(table);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(mdc800_dialog.dialog)->vbox), table);

	spacer = gtk_vseparator_new();
	gtk_widget_show(spacer);
	gtk_table_attach_defaults(GTK_TABLE(table),spacer,2,3,2,22);	
      
	/* camera id ---------------------- */
	label = gtk_label_new("Camera ID:");
	gtk_widget_show(label);
	mdc800_dialog.cam_id = gtk_label_new ("Mustek MDC800");
	gtk_widget_show (mdc800_dialog.cam_id);
	gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,0,1);	
	gtk_table_attach_defaults(GTK_TABLE(table),mdc800_dialog.cam_id,1,2,0,1);

	spacer = gtk_hseparator_new();
	gtk_widget_show(spacer);
	gtk_table_attach_defaults(GTK_TABLE(table),spacer,0,5,1,2);

	
	/* image quality ---------------------- */
	label = gtk_label_new("Image Quality:");
	gtk_widget_show(label);
	gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,2,3);
	mdc800_dialog.image_quality=mdc800_createComboBox (mdc800_image_quality_string,3);
	gtk_table_attach_defaults(GTK_TABLE(table),mdc800_dialog.image_quality,1,2,2,3);


	/* exposure seting ---------------------- */
	label = gtk_label_new("Exposure:");
	gtk_widget_show(label);
	value = 0;
	adj = gtk_adjustment_new(value, -2, 2, 1, 0, 0);
	mdc800_dialog.lcd_adj=adj;
	mdc800_dialog.lcd = gtk_hscale_new(GTK_ADJUSTMENT(adj));
	gtk_range_set_update_policy(GTK_RANGE(mdc800_dialog.lcd),GTK_UPDATE_CONTINUOUS);
	gtk_scale_set_draw_value(GTK_SCALE(mdc800_dialog.lcd), TRUE);
	gtk_scale_set_digits(GTK_SCALE(mdc800_dialog.lcd), 0);
	gtk_widget_show(mdc800_dialog.lcd);
	gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,6,7);	
	gtk_table_attach_defaults(GTK_TABLE(table),mdc800_dialog.lcd,1,2,6,7);	


	/* flash mode ---------------------- */
	label = gtk_label_new("Flash Mode:");
	gtk_widget_show(label);
	mdc800_dialog.flash=mdc800_createComboBox (mdc800_flash_light_string,5);
	gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,3,4);	
	gtk_table_attach_defaults(GTK_TABLE(table),mdc800_dialog.flash,1,2,3,4);

	
	/* light source ---------------------- */
	label = gtk_label_new("White Balance:");
	gtk_widget_show(label);
	mdc800_dialog.light=mdc800_createComboBox (mdc800_wb_string,4);
	gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,4,5);
	gtk_table_attach_defaults(GTK_TABLE(table),mdc800_dialog.light,1,2,4,5);	

	
	/* Set exposure Mode ---------------------- */
	label = gtk_label_new("Exposure Mode:");
	gtk_widget_show(label);
	mdc800_dialog.exp_mode=mdc800_createComboBox (mdc800_exposure_mode_string,2);
	gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,5,6);
	gtk_table_attach_defaults(GTK_TABLE(table),mdc800_dialog.exp_mode,1,2,5,6);
	
	
	/* Storage Source ---------------------- */
	label = gtk_label_new("Storage-Source:");
	gtk_widget_show(label);
	mdc800_dialog.source=mdc800_createComboBox (mdc800_storage_source_string,2);
	gtk_table_attach_defaults(GTK_TABLE(table),label,3,4,2,3);
	gtk_table_attach_defaults(GTK_TABLE(table),mdc800_dialog.source,4,5,2,3);
	
	spacer = gtk_hseparator_new();
	gtk_widget_show(spacer);
	gtk_table_attach_defaults(GTK_TABLE(table),spacer,3,5,3,4);



	/* Different Options */
	label = gtk_label_new("Special Options:");
	gtk_widget_show(label);
	gtk_table_attach_defaults(GTK_TABLE(table),label,3,4,4,5);	


	/* LCD ON/OFF ---------------------- */
	mdc800_dialog.lcd_on = gtk_check_button_new_with_label("Turn on LCD");
	gtk_widget_show(mdc800_dialog.lcd_on);
	gtk_table_attach_defaults(GTK_TABLE(table),mdc800_dialog.lcd_on,4,5,4,5);
	
	
	spacer = gtk_hseparator_new();
	gtk_widget_show(spacer);
	gtk_table_attach_defaults(GTK_TABLE(table),spacer,3,5,7,8);


	/* Baud Rate Selection (only for rs232)*/
	if (mdc800_io_using_usb)
	{
		label=gtk_label_new ("USB detected.");
		gtk_widget_show(label);
		gtk_table_attach_defaults(GTK_TABLE(table),label,3,4,8,9);
	}
	else
	{
		label = gtk_label_new("Baudrate:");
		gtk_widget_show(label);
		mdc800_dialog.baud=mdc800_createComboBox (mdc800_baud_rate_string,3);
		gtk_table_attach_defaults(GTK_TABLE(table),label,3,4,8,9);
		gtk_table_attach_defaults(GTK_TABLE(table),mdc800_dialog.baud,4,5,8,9);
	}
	

	/* done with the widgets */

   toggle = gtk_toggle_button_new();
	gtk_widget_show(toggle);
	gtk_widget_hide(toggle);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mdc800_dialog.dialog)->action_area),
                           toggle, TRUE, TRUE, 0);

	mdc800_dialog.save_button = gtk_button_new_with_label("Save");
	gtk_widget_show(mdc800_dialog.save_button);
	GTK_WIDGET_SET_FLAGS (mdc800_dialog.save_button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mdc800_dialog.dialog)->action_area),
			   mdc800_dialog.save_button, FALSE, FALSE, 0);
	mdc800_dialog.cancel_button = gtk_button_new_with_label("Cancel");
	gtk_widget_show(mdc800_dialog.cancel_button);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mdc800_dialog.dialog)->action_area),
			   mdc800_dialog.cancel_button, FALSE, FALSE, 0);
	gtk_widget_grab_default (mdc800_dialog.save_button);

	gtk_object_set_data(GTK_OBJECT(mdc800_dialog.dialog), "button", "CANCEL");

}

void mdc800_disposeDialog (void)
{
	gtk_widget_destroy(mdc800_dialog.dialog);
}


/*****************************************************************************
	Set Dialog Field / Set Konfiguration from Dialog
******************************************************************************/

int mdc800_setupDialog (void)
{
	int value, tmp;
	
	if (!mdc800_initialize ())
	{
		return -1;
	}
	
	update_status("Receiving Configuration.");	
	update_progress (0);
	
	/* Image Quality */
	value=mdc800_getImageQuality ();
	if (value<0)
		return -1;

	mdc800_ComboBox_SetEntry (mdc800_dialog.image_quality, mdc800_image_quality_string, value);
	update_progress (12);


	if (!mdc800_getWBandExposure (&tmp, &value))
		return -1;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(mdc800_dialog.lcd_adj), tmp);
	
	switch (value) {
		case 1: value=0; break;
		case 2: value=1; break;
		case 4: value=2; break;
		case 8: value=3; break;
	}
	mdc800_ComboBox_SetEntry (mdc800_dialog.light,mdc800_wb_string,value);
	update_progress (25);


	/* flash mode ---------------------- */
	value = mdc800_getFlashLightStatus ();
	mdc800_ComboBox_SetEntry (mdc800_dialog.flash,mdc800_flash_light_string,value);
	update_progress (37);


	/* LCD ON/OFF ---------------------- */
	if (mdc800_isLCDEnabled ())
		gtk_widget_activate(mdc800_dialog.lcd_on);
	update_progress (50);


	/* Storage Source ---------------------- */
	value = mdc800_getStorageSource ();
	if (value < 0)
		return -1;
	mdc800_ComboBox_SetEntry (mdc800_dialog.source, mdc800_storage_source_string, value);
	update_progress (87);


	/* Set exposure Mode ---------------------- */
	value = mdc800_getExposureMode ();
	if (value < 0)
		return -1;
	mdc800_ComboBox_SetEntry (mdc800_dialog.exp_mode, mdc800_exposure_mode_string, value);
	update_progress (95);
	
	/* Baudrate */
	if (!mdc800_io_using_usb) {
		int speed;
	        mdc800_getSpeed(camera,&speed);
		mdc800_ComboBox_SetEntry (mdc800_dialog.baud, mdc800_baud_rate_string, speed);
	}
		
	update_progress (100);
	update_status("Done.");
	
	update_progress (0);
	return 0;
}


int mdc800_setupConfig (void)
{
	int value;
	
	update_status("Writing Configuration.");	
	update_progress (0);


	/* Set image quality... */
	value = mdc800_ComboBox_GetEntry (mdc800_dialog.image_quality, mdc800_image_quality_string,3);
	if (value < 0)
		return -1;
	if (!mdc800_setImageQuality (value))
		return -1;
	update_progress (12);
	
	
	/* set exposure setting */
	value= GTK_ADJUSTMENT(mdc800_dialog.lcd_adj)->value;
	if (!mdc800_setExposure (value))
		return -1;
	update_progress(25);


	/* Set flash mode... */
	value=mdc800_ComboBox_GetEntry (mdc800_dialog.flash, mdc800_flash_light_string,5);
	if (value<0)
		return -1;
	if (!mdc800_setFlashLight (value))
		return -1;
	update_progress(37);


	/* Set White Balance... */
	value = mdc800_ComboBox_GetEntry (mdc800_dialog.light, mdc800_wb_string,4);
	switch (value)
	{
		case 0: value=1; break;
		case 1: value=2; break;
		case 2: value=4; break;
		case 3: value=8; break;
		default:
			return -1;
	}
	if (!mdc800_setWB (value))
		return -1;
	update_progress(50);


	/* Set LCD ON/OFF... */
	if (GTK_WIDGET_STATE(mdc800_dialog.lcd_on) == GTK_STATE_ACTIVE)
	  	mdc800_enableLCD (1);
	else 
	  	mdc800_enableLCD (0);
	update_progress(62);


	/* Set Storage Source... */
	value = mdc800_ComboBox_GetEntry (mdc800_dialog.source, mdc800_storage_source_string,2);
	if (value < 0)
		return -1;
	update_progress(75);		


	if (!mdc800_setStorageSource (value))
		return -1;
	update_progress(87);


	mdc800_setTarget (1);


	/* Set Exposure Mode... */
	value =mdc800_ComboBox_GetEntry (mdc800_dialog.exp_mode, mdc800_exposure_mode_string,2);
	if (value < 0)
		return -1;
	if (!mdc800_setExposureMode (value))
		return -1;
	update_progress (95);
			
		
	/* Change BaudRate */
	if (!mdc800_io_using_usb)
	{
		value = mdc800_ComboBox_GetEntry (mdc800_dialog.baud, mdc800_baud_rate_string,3);
		if (value < 0)
			return -1;
		mdc800_changespeed (value);
	}
	
	update_progress(100);

	update_status("Done.");
	update_progress(0);
	return 0;
}


/*****************************************************************************
	API for gphoto
******************************************************************************/


int mdc800_configure ()
{
	if (mdc800_initialize ())
	{
		mdc800_createDialog ();
		if (mdc800_setupDialog () == 0)
		{
			gtk_widget_show(mdc800_dialog.dialog);
			if (wait_for_hide(mdc800_dialog.dialog, mdc800_dialog.save_button, mdc800_dialog.cancel_button) != 0)
			{
				if (mdc800_setupConfig () != 0)
				{
					printAPIError ("(mdc800_configure) Error sending Configuration to Camera\n");
					mdc800_close ();
				}
			}
		}
		else
		{
			printAPIError ("(mdc800_configure) Error receiving Configuration from Camera\n");
			mdc800_close ();
		}
		mdc800_disposeDialog ();
	}
	return 1;
}
