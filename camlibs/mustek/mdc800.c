/* mdc800.c
 *
 * Copyright 1999/2000 by Henning Zabel <henning@uni-paderborn.de>
 * Copyright 2001 Marcus Meissner <marcus@jet.franken.de>
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
#include <string.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>

#include "print.h"
#include "core.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

int
camera_id (CameraText *id)
{
	strcpy(id->text, "Mustek MDC 800");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;

	memset(&a,0,sizeof(a));
	strcpy(a.model,	"Mustek:MDC 800");
	a.status	= GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port		= GP_PORT_SERIAL | GP_PORT_USB;
	a.speed[0]	= 19200;
	a.speed[1]	= 57600;
	a.speed[2]	= 115200;
	a.speed[3]	= 0;

	a.usb_vendor	= 0x055f;
	a.usb_product	= 0xa800;
	a.operations        = 	GP_OPERATION_CAPTURE_PREVIEW |
				GP_CAPTURE_IMAGE;
	a.file_operations   = 	GP_FILE_OPERATION_DELETE |
				GP_FILE_OPERATION_PREVIEW;
	a.folder_operations = 	GP_FOLDER_OPERATION_NONE;
/* Deliberately commented out. this driver does not work due to USB interrupts
	gp_abilities_list_append(list, a);
 */
	return (GP_OK);
}

#if 0
static int
camera_exit (Camera *camera, GPContext *context)
{
	return (GP_OK);
}
#endif

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int	size, nr, result;

	nr = gp_filesystem_number(fs, folder, filename, context);
	if(nr < 0)
		return nr;

	switch (type) {
#if 0
	case GP_FILE_TYPE_RAW:
		result =  jd11_get_image_full (camera, nr, &data, (int*) &size, 1);
		break;
#endif
	case GP_FILE_TYPE_NORMAL:
		result = mdc800_getImage(camera,nr,&data,&size);
		break;
	case GP_FILE_TYPE_PREVIEW:
		result = mdc800_getThumbnail(camera,nr,&data,&size);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
	if (result < 0)
		return result;

	gp_file_set_mime_type (file, GP_MIME_JPEG);
	gp_file_set_data_and_size(file, data, size);
	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int ret,nr ;

	nr = gp_filesystem_number(fs, folder, filename, context);
	if (nr < 0)
	    return nr;

	ret = mdc800_setTarget (camera,1);
	if (ret!=GP_OK)
	{
		printAPIError ("(mdc800_delete_image) can't set Target\n");
		return ret;
	}
	ret = mdc800_io_sendCommand(camera->port, COMMAND_DELETE_IMAGE,nr/100,(nr%100)/10,nr%10,0,0);
	if (ret != GP_OK)
	{
		printAPIError ("(mdc800_delete_image ) deleting Image %i fails !.\n",nr);
		return ret;
	}
	return GP_OK;
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
	GPContext *context)
{
	/*Camera *camera = data;*/

	/*
	 * Delete all files in the given folder. If your camera doesn't have
	 * such a functionality, just don't implement this function.
	 */

	return (GP_OK);
}

static int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *ctx)
{
	gp_widget_new (GP_WIDGET_WINDOW, "Camera Configuration", window);

	/* Append your sections and widgets here. */

	return (GP_OK);
}

static int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *ctx)
{
	/*
	 * Check if the widgets' values have changed. If yes, tell the camera.
	 */

	return (GP_OK);
}

#if 0
static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	/*
	 * Capture a preview and return the data in the given file (again,
	 * use gp_file_set_data_and_size, gp_file_set_mime_type, etc.).
	 * libgphoto2 assumes that previews are NOT stored on the camera's
	 * disk. If your camera does, please delete it from the camera.
	 */

	return (GP_OK);
}
#endif

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
	GPContext *context)
{
	/*
	 * Capture an image and tell libgphoto2 where to find it by filling
	 * out the path.
	 */

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *ctx)
{
	char mdc800_summary_output[500];
	char line[50];
	/*
	 * Fill out the summary with some information about the current
	 * state of the camera (like pictures taken, etc.).
	 */
	strcpy (mdc800_summary_output,_("Summary for Mustek MDC800:\n"));
	if (GP_OK!=mdc800_getSystemStatus (camera))
	{
		strcat(mdc800_summary_output,_("no status reported."));
		strcpy(summary->text, mdc800_summary_output);
		return GP_OK;
	}

	if (mdc800_isCFCardPresent (camera))
		strcpy (line,_("Compact Flash Card detected\n"));
	else
		strcpy (line,_("No Compact Flash Card detected\n"));
	strcat(mdc800_summary_output,line);

	if (mdc800_getMode (camera) == 0)
		strcpy (line, _("Current Mode: Camera Mode\n"));
	else
		strcpy (line, _("Current Mode: Playback Mode\n"));
	strcat (mdc800_summary_output,line);

	strcpy (line,mdc800_getFlashLightString (mdc800_getFlashLightStatus (camera)));
	strcat (line,"\n");
	strcat (mdc800_summary_output,line);


	if (mdc800_isBatteryOk (camera))
		strcpy (line, _("Batteries are ok."));
	else
		strcpy (line, _("Batteries are low."));
	strcat (mdc800_summary_output,line);
	strcpy(summary->text,mdc800_summary_output);
	return GP_OK;
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *ctx)
{
	/*
	 * If you would like to tell the user some information about how
	 * to use the camera or the driver, this is the place to do.
	 */
	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *ctx)
{
	strcpy (about->text, _("Mustek MDC-800 gPhoto2 Library\n"
			       "Henning Zabel <henning@uni-paderborn.de>\n"
			       "Ported to gphoto2 by Marcus Meissner <marcus@jet.franken.de>\n"
			       "Supports Serial and USB Protocols."));
	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/* Get the file info here and write it into <info> */

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/* List your folders here */

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int count, result;

	result =  mdc800_number_of_pictures (camera, &count);
	if (result != GP_OK)
		return result;
	gp_list_populate(list, "image%02i.jpg", count);
	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func
};

int
camera_init (Camera *camera, GPContext *context)
{
        /* First, set up all the function pointers */
        /* camera->functions->exit                 = camera_exit; */
        camera->functions->get_config		= camera_config_get;
        camera->functions->set_config		= camera_config_set;
        camera->functions->capture              = camera_capture;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	/* Configure port */
	gp_port_set_timeout(camera->port,MDC800_DEFAULT_COMMAND_RETRY_DELAY);
	if (camera->port->type == GP_PORT_SERIAL) {
	    GPPortSettings settings;
	    gp_port_get_settings(camera->port, &settings);
	    settings.serial.speed   = 57600;
	    settings.serial.bits    = 8;
	    settings.serial.parity  = 0;
	    settings.serial.stopbits= 1;
	    gp_port_set_settings(camera->port, settings);
	}
	return mdc800_openCamera(camera);
}
