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

#include <gphoto2-library.h>
#include <gphoto2-port.h>

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
		a.file_operations = GP_FILE_OPERATION_PREVIEW;
		a.operations = GP_OPERATION_NONE;
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

	/* Count the pictures on the camera */
	CR (n = fuji_pic_count (camera, context));

	/* Get the name of each file */
	for (i = 1; i <= n; i++) {
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

	/* We need file numbers starting with 1 */
	CR (n = gp_filesystem_number (camera->fs, folder, filename, context));
	n++;

	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;

	/* Setup all function pointers */
	camera->functions->about = camera_about;

	/* Set up the port */
	CR (gp_port_set_timeout (camera->port, 1000));
	CR (gp_port_get_settings (camera->port, &settings));
	settings.serial.speed    = 9600;
	settings.serial.bits     = 8;
	settings.serial.parity   = 0;
	settings.serial.stopbits = 1;
	CR (gp_port_set_settings (camera->port, settings));

	/* Set up the filesystem. */
	CR (gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL,
					  camera));
	CR (gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL,
					  camera));

	/* Is the camera there? */
	CR (fuji_ping (camera, context));

	return (GP_OK);
}
