/* library.c
 *
 * Copyright (C) 2001 Lutz Mueller <lutz@users.sourceforge.net>
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
#include "config.h"
#include "pdrm65.h"
#include <string.h>
#include <gphoto2-library.h>
#include <gphoto2-result.h>

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

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "toshiba-pdrm65");

	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Toshiba:PDR-M65");
	a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port     = GP_PORT_USB;
        a.usb_vendor = 0x1132;
        a.usb_product = 0x4334;
	a.operations        =GP_OPERATION_CAPTURE_PREVIEW | GP_CAPTURE_IMAGE;
	a.file_operations   =GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW;
	a.folder_operations =GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context) 
{
	return (GP_ERROR_NOT_SUPPORTED);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	int ret = GP_OK;
	Camera *camera = data;
	ret = pdrm65_get_pict(camera->port,filename,file);
	/*
	 * Get the file from the camera. Use gp_file_set_mime_type,
	 * gp_file_set_data_and_size, etc.
	 */

	return (ret);
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, CameraFile *file,
	       void *data, GPContext *context)
{
	Camera *camera;

	/*
	 * Upload the file to the camera. Use gp_file_get_data_and_size,
	 * gp_file_get_name, etc.
	 */

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;

	/* Delete the file from the camera. */

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;

	/*
	 * Delete all files in the given folder. If your camera doesn't have
	 * such a functionality, just don't implement this function.
	 */

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context) 
{
	gp_widget_new (GP_WIDGET_WINDOW, "Camera Configuration", window);

	/* Append your sections and widgets here. */

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *context) 
{
	/*
	 * Check if the widgets' values have changed. If yes, tell the camera.
	 */

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	/*
	 * Capture a preview and return the data in the given file (again,
	 * use gp_file_set_data_and_size, gp_file_set_mime_type, etc.).
	 * libgphoto2 assumes that previews are NOT stored on the camera's 
	 * disk. If your camera does, please delete it from the camera.
	 */

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	/*
	 * Capture an image and tell libgphoto2 where to find it by filling
	 * out the path.
	 */

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	/*
	 * Fill out the summary with some information about the current 
	 * state of the camera (like pictures taken, etc.).
	 */

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	/*
	 * If you would like to tell the user some information about how 
	 * to use the camera or the driver, this is the place to do.
	 */

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Toshiba PDR-M65\n"
			       "Sean Bruno <sean.bruno@dsl-only.net>\n"
			       "Library for the Toshiba PDR-M65\n"
			       "USB interface."));

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;

	/* Get the file info here and write it into <info> */

	return (GP_ERROR_NOT_SUPPORTED);
}
static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context)
{
	Camera *camera = data;

	/* Set the file info here from <info> */

	return (GP_ERROR_NOT_SUPPORTED);
}


static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	Camera *camera = data;

	/* List your folders here */

	//return (GP_ERROR_NOT_SUPPORTED);
	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int ret = GP_OK;
	/* List your files here */
	ret = pdrm65_get_filenames(camera->port, list);
	return (ret);
}

int
camera_init (Camera *camera, GPContext *context) 
{
        int ret = GP_OK;
	/* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->get_config           = camera_config_get;
        camera->functions->set_config           = camera_config_set;
        camera->functions->capture              = camera_capture;
        camera->functions->capture_preview      = camera_capture_preview;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func,
				      folder_list_func, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, set_info_func,
				      camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      delete_file_func, camera);
	gp_filesystem_set_folder_funcs (camera->fs, put_file_func,
					delete_all_func, NULL, NULL, camera);

	/*
	 * The port is already provided with camera->port (and
	 * already open). You just have to use functions like
	 * gp_port_timeout_set, gp_port_settings_get, gp_port_settings_set.
	 */
	
	/*
	 * Once you have configured the port, you should check if a 
	 * connection to the camera can be established.
	 */
	ret = pdrm65_init(camera->port);
	
	return (ret);
}
