/* library.c:
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <gphoto2-library.h>
#include <gphoto2-port.h>
#include <gphoto2-port-log.h>

#include "fuji.h"

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

#define GP_MODULE "fuji"

#define CR(result) {int r = (result); if (r < 0) return (r);}

static struct {
	const char *model;
} models[] = {
	{"Apple QuickTake 200"},
	{"Fuji DS-7"},
	{"Fuji DX-5"},
	{"Fuji DX-7"},
	{"Fuji DX-10"},
	{"Fuji MX-500"},
	{"Fuji MX-600"},
	{"Fuji MX-700"},
	{"Fuji MX-1200"},
	{"Fuji MX-1700"},
	{"Fuji MX-2700"},
	{"Fuji MX-2900"},
	{"Leica Digilux Zoom"},
	{"Samsung Kenox SSC-350N"},
	{"Toshiba PDR-M1"},
	{NULL}
};

struct _CameraPrivateLibrary {
	unsigned long speed;
	unsigned char cmds[0xff];
};

struct {
        FujiCmd command;
        const char *name;
} Commands[] = {
        {FUJI_CMD_PIC_GET,              "get picture"},
        {FUJI_CMD_PIC_GET_THUMB,        "get thumbnail"},
        {FUJI_CMD_SPEED,                "change speed"},
        {FUJI_CMD_VERSION,              "get version"},
        {FUJI_CMD_PIC_NAME,             "get name of picture"},
        {FUJI_CMD_PIC_COUNT,            "get number of pictures"},
        {FUJI_CMD_PIC_SIZE,             "get size of picture"},
        {FUJI_CMD_PIC_DEL,              "delete picture"},
        {FUJI_CMD_TAKE,                 "capture picture"},
	{FUJI_CMD_FLASH_GET,		"get flash mode"},
	{FUJI_CMD_FLASH_SET,		"set flash mode"},
	{FUJI_CMD_FLASH_CHARGE,         "charge flash"},
        {FUJI_CMD_CMDS_VALID,           "list valid commands"},
        {FUJI_CMD_PREVIEW,              "capture preview"},
	{FUJI_CMD_DATE_GET,		"get date"},
        {0, NULL}};

static const char *
cmd_get_name (FujiCmd command)
{
	unsigned int i;

	for (i = 0; Commands[i].name; i++)
		if (Commands[i].command == command)
			break;

	return (Commands[i].name);
}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "Fuji");

	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;
	int i;

	memset (&a, 0, sizeof (a));
	for (i = 0; models[i].model; i++) {
		strcpy (a.model, models[i].model);
		a.port = GP_PORT_SERIAL;
		a.speed[0] = 9600;
		a.speed[1] = 19200;
		a.speed[2] = 38400;
		a.speed[3] = 56700;
		a.speed[4] = 115200;
		a.speed[5] = 0;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		a.file_operations = GP_FILE_OPERATION_PREVIEW |
				    GP_FILE_OPERATION_DELETE;
		a.operations = GP_OPERATION_CONFIG;
		CR (gp_abilities_list_append (list, a));
	}

	return (GP_OK);
}
		
static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _(""
		"Matthew G. Martin\n"
		"Based on fujiplay by Thierry Bousch "
		"<bousch@topo.math.u-psud.fr>\n"));

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int n, i;
	const char *name;

	/*
	 * Count the pictures on the camera. If we don't have any, we
	 * are done.
	 */
	CR (fuji_pic_count (camera, &n, context));
	if (!n)
		return (GP_OK);

	/*
	 * Try to get the name of the first file. If we can't, just come
	 * up with some dummy names.
	 */
	if (fuji_pic_name (camera, 1, &name, context) < 0) {
		CR (gp_list_populate (list, "DSCF%04i.JPG", n));
		return (GP_OK);
	}
	CR (gp_list_append (list, name, NULL));

	/* Get the names of the remaining files. */
	for (i = 2; i <= n; i++) {
		CR (fuji_pic_name (camera, i, &name, context));
		CR (gp_list_append (list, name, NULL));
	}

	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, CameraFileType type,
	       CameraFile *file, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;
	unsigned char *d;
	unsigned int size;

	/* We need file numbers starting with 1 */
	CR (n = gp_filesystem_number (camera->fs, folder, filename, context));
	n++;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		CR (fuji_pic_get (camera, n, &d, &size, context));
		break;
	case GP_FILE_TYPE_PREVIEW:
		CR (fuji_pic_get_thumb (camera, n, &d, &size, context));
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
	CR (gp_file_set_data_and_size (file, d, size));
	CR (gp_file_set_mime_type (file, GP_MIME_JPEG));

	return (GP_OK);
}

static int
del_file_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;

	/* We need file numbers starting with 1 */
	CR (n = gp_filesystem_number (camera->fs, folder, filename, context));
	n++;

	CR (fuji_pic_del (camera, n, context));

	return (GP_OK);
}

struct {
	FujiSpeed speed;
	unsigned int bit_rate;
} Speeds[] = {
	{FUJI_SPEED_115200, 115200},
	{FUJI_SPEED_57600, 57600},
	{FUJI_SPEED_38400, 38400},
	{FUJI_SPEED_19200, 19200},
	{FUJI_SPEED_9600, 9600},
	{FUJI_SPEED_9600, 0}
};

static int
camera_exit (Camera *camera, GPContext *context)
{
	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

static int
pre_func (Camera *camera, GPContext *context)
{
	int r;
	unsigned int i;
	GPPortSettings settings;

	GP_DEBUG ("Initializing connection...");

	CR (gp_port_get_settings (camera->port, &settings));
	CR (fuji_ping (camera, context));
	if (!camera->pl->speed) {

		/* Set to the highest possible speed. */
		for (i = 0; Speeds[i].bit_rate; i++) {
			r = fuji_set_speed (camera, Speeds[i].speed, NULL);
			if (r >= 0)
				break;
		}

		/*
		 * Change the port's speed and check if the camera is
		 * still there.
		 */
		settings.serial.speed = Speeds[i].bit_rate;
		CR (gp_port_set_settings (camera->port, settings));
		CR (fuji_ping (camera, context));

	} else {

		/* User specified a speed. Check if the speed is possible */
		for (i = 0; Speeds[i].bit_rate; i++)
			if (Speeds[i].bit_rate == camera->pl->speed)
				break;
		if (!Speeds[i].bit_rate) {
			gp_context_error (context, _("Bit rate %i is not "
					"supported."), camera->pl->speed);
			return (GP_ERROR_NOT_SUPPORTED);
		}

		/* Change the speed if necessary. */
		if (camera->pl->speed != Speeds[i].bit_rate) {
			CR (fuji_set_speed (camera, Speeds[i].speed, context));

			/*
			 * Change the port's speed and check if the camera is 
			 * still there.
			 */
			settings.serial.speed = Speeds[i].bit_rate;
			CR (gp_port_set_settings (camera->port, settings));
			CR (fuji_ping (camera, context));
		}
	}

	return (GP_OK);
}

static int
post_func (Camera *camera, GPContext *context)
{
	GP_DEBUG ("Terminating connection...");

	/* Reset the camera and put it back to 9600 bps. */
	CR (fuji_reset (camera, context));
	CR (fuji_set_speed (camera, FUJI_SPEED_9600, context));

	return (GP_OK);
}

static int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *widget;
	struct tm tm;
	time_t t;
	FujiDate date;

	CR (gp_widget_new (GP_WIDGET_WINDOW, _("Configuration for "
					"your FUJI camera"), window));

	/* Date & Time */
	CR (fuji_date_get (camera, &date, context));
	CR (gp_widget_new (GP_WIDGET_DATE, _("Date & Time"), &widget));
	CR (gp_widget_append (*window, widget));
	memset (&tm, 0, sizeof (struct tm));
	tm.tm_year = date.year;
	tm.tm_mon  = date.month;
	tm.tm_mday = date.day;
	tm.tm_hour = date.hour;
	tm.tm_min  = date.min;
	tm.tm_sec  = date.sec;
	t = mktime (&tm);
	CR (gp_widget_set_value (widget, &t));

	return (GP_OK);
}

static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *widget;
	FujiDate date;
	time_t t;
	struct tm *tm;

	/* Date & Time */
	CR (gp_widget_get_child_by_label (window, _("Date & Time"), &widget));
	if (gp_widget_changed (widget)) {
		CR (gp_widget_get_value (widget, &t));
		tm = localtime (&t);
		date.year  = tm->tm_year;
		date.month = tm->tm_mon;
		date.day   = tm->tm_mday;
		date.hour  = tm->tm_hour;
		date.min   = tm->tm_min;
		date.sec   = tm->tm_sec;
		CR (fuji_date_set (camera, date, context));
	}

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;
	unsigned int size;

	info->file.fields = GP_FILE_INFO_NONE;
	info->preview.fields = GP_FILE_INFO_NONE;
	info->audio.fields = GP_FILE_INFO_NONE;

	/* Name */
	info->file.fields |= GP_FILE_INFO_NAME;
	strncpy (info->file.name, filename, sizeof (info->file.name));

	/* We need file numbers starting with 1 */
	CR (n = gp_filesystem_number (camera->fs, folder, filename, context));
	n++;

	/* Size */
	if (fuji_pic_size (camera, n, &size, context) >= 0) {
		info->file.fields |= GP_FILE_INFO_SIZE;
		info->file.size = size;
	}

	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	unsigned int i;

	/* Setup all function pointers */
	camera->functions->pre_func   = pre_func;
	camera->functions->post_func  = post_func;
	camera->functions->about      = camera_about;
	camera->functions->exit       = camera_exit;
	camera->functions->get_config = camera_get_config;
	camera->functions->set_config = camera_set_config;

	/* We need to store some data */
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));

	/* Set up the port, but remember the current speed. */
	CR (gp_port_set_timeout (camera->port, 1000));
	CR (gp_port_get_settings (camera->port, &settings));
	camera->pl->speed = settings.serial.speed;
	settings.serial.speed    = 9600;
	settings.serial.bits     = 8;
	settings.serial.parity   = GP_PORT_SERIAL_PARITY_EVEN;
	settings.serial.stopbits = 1;
	CR (gp_port_set_settings (camera->port, settings));

	/* Set up the filesystem. */
	CR (gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL,
					  camera));
	CR (gp_filesystem_set_file_funcs (camera->fs, get_file_func,
					  del_file_func, camera));
	CR (gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL,
					  camera));

	/* Initialize the connection */
	CR (pre_func (camera, context));

	/*
	 * What commands does this camera support? The question is not
	 * easy to answer, as "One issue the DS7 has is that the 
	 * supported command list command is not supported" 
	 * (Matt Martin <mgmartin@screaminet.com>).
	 */
	if (fuji_get_cmds (camera, camera->pl->cmds, context) >= 0) {
		GP_DEBUG ("Your camera supports the following command(s):");
		for (i = 0; i < 0xff; i++)
			if (camera->pl->cmds[i])
				GP_DEBUG (" - 0x%02x: '%s'", i,
					  cmd_get_name (i));
	}

	return (GP_OK);
}
