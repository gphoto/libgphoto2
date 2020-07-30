/* dc240.c
 *
 * Copyright (C) 2001,2002 Hubert Figuiere <hfiguiere@teaser.fr>
 * Copyright (C) 2000,2001,2002 Scott Fritzinger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
  Kodak DC 240/280/3400/5000 driver.
  Maintainer:
       Hubert Figuiere <hfiguiere@teaser.fr>
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "dc240.h"
#include "library.h"

int
camera_id (CameraText *id)
{
	strcpy(id->text, "kodak-dc240");

	return (GP_OK);
}

static const struct camera_to_usb {
	char *name;
	unsigned short idVendor;
	unsigned short idProduct;
} camera_to_usb[] = {
	{ "Kodak:DC240", 0x040A, 0x0120 },
	{ "Kodak:DC280", 0x040A, 0x0130 },
	{ "Kodak:DC3400", 0x040A, 0x0132 },
	{ "Kodak:DC5000", 0x040A, 0x0131 },
        { NULL, 0, 0 }
};

/*
  Abilities are based upon what we can do with a DC240.
  Later cameras have a superset of the DC240 feature and are not
  currently supported.
 */
int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;
        int i;

        for (i = 0; camera_to_usb[i].name; i++)
        {
	    memset (&a, 0, sizeof (a));
            strcpy(a.model, camera_to_usb[i].name);
	    a.status = GP_DRIVER_STATUS_PRODUCTION;
            a.port     = GP_PORT_SERIAL | GP_PORT_USB;
            a.speed[0] = 9600;
            a.speed[1] = 19200;
            a.speed[2] = 38400;
            a.speed[3] = 57600;
            a.speed[4] = 115200;
            a.speed[5] = 0;
            a.usb_vendor  = camera_to_usb[i].idVendor;
            a.usb_product = camera_to_usb[i].idProduct;
            a.operations        = 	GP_OPERATION_CAPTURE_IMAGE;
            a.file_operations   = 	GP_FILE_OPERATION_DELETE |
                                        GP_FILE_OPERATION_PREVIEW;
            a.folder_operations = 	GP_FOLDER_OPERATION_NONE;

            gp_abilities_list_append(list, a);
        }
	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	dc240_close (camera, context);

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	Camera *camera = data;

	return dc240_get_directory_list(camera, list, folder, 0x10, context);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;

	return dc240_get_directory_list(camera, list, folder, 0x00, context);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
	case GP_FILE_TYPE_RAW:
		return dc240_file_action (camera, DC240_ACTION_IMAGE, file,
					   folder, filename, context);
	case GP_FILE_TYPE_PREVIEW:
		return dc240_file_action (camera, DC240_ACTION_PREVIEW, file,
					   folder, (char*) filename, context);
	default:
		return GP_ERROR_NOT_SUPPORTED;
	}
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;

	return (dc240_file_action (camera, DC240_ACTION_DELETE, NULL, folder,
    				   filename, context));
}

static int
camera_capture (Camera *camera, CameraCaptureType type,
		CameraFilePath *path, GPContext *context)
{
	int result;

	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Capture the image */
	result = dc240_capture (camera, path, context);
	if (result < 0)
		return (result);

	/* Tell the filesystem about it */
	result = gp_filesystem_append (camera->fs, path->folder, path->name, context);
	if (result < 0)
		return (result);

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
    char buf [32 * 1024];
    char temp [1024];
    int retval;
    DC240StatusTable table;

    retval = dc240_get_status (camera, &table, context);
    if (retval == GP_OK) {
	sprintf (buf, _("Model: Kodak %s\n"), dc240_convert_type_to_camera(table.cameraType));
	sprintf (temp, _("Firmware version: %d.%02d\n"), table.fwVersInt, table.fwVersDec);
	strcat (buf, temp);
	sprintf (temp, _("Battery status: %s, AC Adapter: %s\n"),
		 dc240_get_battery_status_str(table.battStatus),
		 dc240_get_ac_status_str(table.acAdapter));
	strcat (buf, temp);
	sprintf (temp, _("Number of pictures: %d\n"), table.numPict);
	strcat (buf, temp);
	sprintf (temp, _("Space remaining: High: %d, Medium: %d, Low: %d\n"),
		 table.remPictHigh, table.remPictMed, table.remPictLow);
	strcat (buf, temp);

	sprintf (temp, _("Memory card status (%d): %s\n"), table.memCardStatus,
		 dc240_get_memcard_status_str(table.memCardStatus));
	strcat (buf, temp);

	sprintf (temp, _("Total pictures captured: %d, Flashes fired: %d\n"),
		 table.totalPictTaken, table.totalStrobeFired);
	strcat (buf, temp);


	strcpy(summary->text, buf);
    }
    return retval;
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text,
		_("Kodak DC240 Camera Library\n"
		"Scott Fritzinger <scottf@gphoto.net> and Hubert Figuiere <hfiguiere@teaser.fr>\n"
		"Camera Library for the Kodak DC240, DC280, DC3400 and DC5000 cameras.\n"
		"Rewritten and updated for gPhoto2."));

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func
};

int
camera_init (Camera *camera, GPContext *context)
{
	int ret, selected_speed = 0;
	GPPortSettings settings;

	/* First, set up all the function pointers */
	camera->functions->exit             = camera_exit;
	camera->functions->capture          = camera_capture;
	camera->functions->summary          = camera_summary;
	camera->functions->about            = camera_about;

	/* Set up the CameraFilesystem */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	ret = gp_port_get_settings (camera->port, &settings);
	if (ret < 0)
		return (ret);
	switch (camera->port->type) {
	case GP_PORT_SERIAL:

		/* Remember the selected speed */
		selected_speed = settings.serial.speed;

		settings.serial.speed    = 9600;
		settings.serial.bits     = 8;
		settings.serial.parity   = 0;
		settings.serial.stopbits = 1;
		break;
	case GP_PORT_USB:
		settings.usb.inep       = 0x82;
		settings.usb.outep      = 0x01;
		settings.usb.config     = 1;
		settings.usb.interface  = 0;
		settings.usb.altsetting = 0;
		break;
	default:
		return (GP_ERROR_UNKNOWN_PORT);
	}

	ret = gp_port_set_settings (camera->port, settings);
	if (ret < 0)
		return (ret);

	ret = gp_port_set_timeout (camera->port, TIMEOUT);
	if (ret < 0)
		return (ret);

	if (camera->port->type == GP_PORT_SERIAL) {
		char buf[8];
		/* Reset the camera to 9600 */
		gp_port_send_break(camera->port, 1);

		/* Used to have a 1500 msec pause here to give
		 * the camera time to reset - but, since
		 * the serial port sometimes returns a garbage
		 * character or two after the break, we do
		 * a couple of TIMEOUT (750 msec) pauses here
		 * force the delay as well as flush the port
		 */
		gp_port_read(camera->port, buf, 8);
		gp_port_read(camera->port, buf, 8);

		ret = dc240_set_speed (camera, selected_speed);
		if (ret < 0)
			return (ret);
	}

	/* Open the CF card */
	ret = dc240_open (camera);
	if (ret < 0)
		return (ret);

	ret = dc240_packet_set_size (camera, HPBS+2);
	if (ret < 0)
		return (ret);

	return (GP_OK);
}
