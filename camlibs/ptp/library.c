/* library.c
 *
 * Copyright (C) 2001 Mariusz Woloszyn <emsi@ipartners.pl>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <gphoto2-library.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
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

#include "ptp.h"

#define CR(result) {int r=(result);if(r<0) return (r);}
#define PCR(result) {int r=(result);if(r<0) return (PTP_ERROR);}

static struct {
	const char *model;
	unsigned short usb_vendor;
	unsigned short usb_product;
} models[] = {
	{"Kodak DC-240 (PTP)",  0x40a, 0x121}, /* Special firmware */
	{"Kodak DC-4800", 0x40a, 0x160},
	{"Kodak DX-3500", 0x40b, 0x500},
	{"Kodak DX-3600", 0, 0},
	{"Kodak DX-3900", 0, 0},
	{"Kodak MC3", 0, 0},
	{"Sony DSC-P5", 0, 0},
	{"Sony DSC-F707", 0, 0},
	{"HP PhotoSmart 318", 0, 0},
	{NULL, 0, 0}
};

struct _CameraPrivateLibrary {
	PTPParams params;
};

static PTPResult
ptp_read (unsigned char *bytes, unsigned int size, void *data)
{
	Camera *camera = data;

	PCR (gp_port_read (camera->port, bytes, size));

	return (PTP_OK);
}

static PTPResult
ptp_write (unsigned char *bytes, unsigned int size, void *data)
{
	Camera *camera = data;

	PCR (gp_port_write (camera->port, bytes, size));

	return (PTP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].model; i++) {
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port   = GP_PORT_USB;
		a.speed[0] = 0;
		a.usb_vendor = models[i].usb_vendor;
		a.usb_product= models[i].usb_product;
		a.operations        = GP_OPERATION_NONE;
		a.file_operations   = GP_FILE_OPERATION_NONE;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		CR (gp_abilities_list_append (list, a));
	}

	return (GP_OK);
}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "PTP");

	return (GP_OK);
}

static int
camera_exit (Camera *camera)
{
	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *text)
{
	strncpy (text->text,
		 _("Written by Mariusz Woloszyn <emsi@ipartners.pl>. "
		   "Enjoy!"), sizeof (text->text));
	return (GP_OK);
}

int
camera_init (Camera *camera)
{
	unsigned char c;
	PTPResult result;

	camera->functions->about = camera_about;
	camera->functions->exit = camera_exit;

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	camera->pl->params.io_write = ptp_write;
	camera->pl->params.io_read = ptp_read;
	camera->pl->params.io_data = camera;

	result = ptp_do_something (camera->pl->params, '?', &c);
	if (result != PTP_OK) {
		gp_camera_set_error (camera, _("Unfortunatelly, an error "
			"happened..."));
		return (GP_ERROR);
	}

	return (GP_OK);
}
