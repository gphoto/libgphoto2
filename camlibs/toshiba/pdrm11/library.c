/* library.c -- copied from the template
 *
 * Copyright © 2003 David Hogue <david@jawa.gotdns.org>
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
#include "pdrm11.h"

#include <_stdint.h>
#include <string.h>

#include <gphoto2.h>


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
	int ret;
	Camera *camera = data;
	
	switch(type){
	case GP_FILE_TYPE_PREVIEW:
	case GP_FILE_TYPE_NORMAL:
		picNum = gp_filesystem_number(fs, folder, filename, context) + 1;
		ret =  pdrm11_get_file (fs, filename, type, file, camera->port, picNum);
		return(ret);
	default:
		return GP_ERROR_NOT_SUPPORTED;
	}
}


static int
put_file_func (CameraFilesystem *fs, const char *folder, CameraFile *file,
	       void *data, GPContext *context)
{
	GP_DEBUG("put_file_func");
	return (GP_OK);
}


static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	int picNum;
	int ret;
	Camera *camera = data;


	picNum = gp_filesystem_number(fs, folder, filename, context) + 1;
	ret = pdrm11_delete_file(camera->port, picNum);
	return (GP_OK);
}


static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	/*Camera *camera = data;*/
	GP_DEBUG("deleta_all_func");

	/*
	 * Delete all files in the given folder. If your camera doesn't have
	 * such a functionality, just don't implement this function.
	 */

	return (GP_OK);
}


static int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context) 
{
	GP_DEBUG("camera_config_get");
	gp_widget_new (GP_WIDGET_WINDOW, "Camera Configuration", window);


	/* Append your sections and widgets here. */

	return (GP_OK);
}


static int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *context) 
{
	GP_DEBUG("camera_config_set");
	/*
	 * Check if the widgets' values have changed. If yes, tell the camera.
	 */

	return (GP_OK);
}


static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	GP_DEBUG("camera_capture_preview");
	return (GP_OK);
}


static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	GP_DEBUG("camera_capture");
	return (GP_OK);
}


static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	GP_DEBUG("camera_summary");
	/*
	 * Fill out the summary with some information about the current 
	 * state of the camera (like pictures taken, etc.).
	 */

	return (GP_OK);
}


static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	GP_DEBUG("camera_manual");
	/*
	 * If you would like to tell the user some information about how 
	 * to use the camera or the driver, this is the place to do.
	 */

	return (GP_OK);
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
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	/* Camera *camera = data; */
	time_t now;

	GP_DEBUG("get_info_func");

	now = time(NULL);
	GP_DEBUG("now: 0x%x", now);
	info->file.mtime = now;

	info->file.fields = GP_FILE_INFO_MTIME;
	

	return (GP_OK);
}


static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context)
{
	/*Camera *camera = data;*/
	GP_DEBUG("set_info_func");

	/* Set the file info here from <info> */
	

	return (GP_OK);
}



static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	/*Camera *camera = data;*/
	GP_DEBUG("folder_list_func");

	/* List your folders here */

	return (GP_OK);
}



static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int ret;

	ret = pdrm11_get_filenames(camera->port, list);
	return (GP_OK);
}



int
camera_init (Camera *camera, GPContext *context) 
{
	int ret;

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



	ret = pdrm11_init(camera->port);
	return(ret);
}
