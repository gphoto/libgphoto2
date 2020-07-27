/* library.c -- copied from the template
 *
 * Copyright 2003 David Hogue <david@jawa.gotdns.org>
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

#include "config.h"
#include "pdrm11.h"

#include <_stdint.h>
#include <string.h>

#include <gphoto2/gphoto2.h>


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
#  define _(String) (String)
#  define N_(String) (String)
#endif


#define GP_MODULE "Toshiba"

int
camera_id (CameraText *id)
{
	strcpy(id->text, "toshiba-pdrm11");

	return (GP_OK);
}


int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Toshiba:PDR-M11");
	a.status = GP_DRIVER_STATUS_TESTING;
	a.port     = GP_PORT_USB;
	a.speed[0] = 0;
	a.usb_vendor = 0x1132;
	a.usb_product = 0x4337;
	a.operations        = 	GP_OPERATION_NONE;
	a.file_operations   = 	GP_FILE_OPERATION_DELETE |
				GP_FILE_OPERATION_PREVIEW;
	a.folder_operations = 	GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}


static int
camera_exit (Camera *camera, GPContext *context)
{
	return (GP_OK);
}


static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	int picNum;
	Camera *camera = data;

	switch(type){
	case GP_FILE_TYPE_PREVIEW:
	case GP_FILE_TYPE_NORMAL:
		picNum = gp_filesystem_number(fs, folder, filename, context) + 1;
		return pdrm11_get_file (fs, filename, type, file, camera->port, picNum);
	default:
		return GP_ERROR_NOT_SUPPORTED;
	}
}


static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	int picNum;
	Camera *camera = data;

	picNum = gp_filesystem_number(fs, folder, filename, context) + 1;
	return pdrm11_delete_file(camera->port, picNum);
}


static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Toshiba\n"
			       "David Hogue <david@jawa.gotdns.org>\n"
			       "Toshiba pdr-m11 driver.\n"));

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;

	return pdrm11_get_filenames(camera->port, list);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func
};

int
camera_init (Camera *camera, GPContext *context)
{
        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->about                = camera_about;
	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
	return pdrm11_init(camera->port);
}
