/*
 * STV0674 Camera Chipset Driver
 * Copyright 2002 Vincent Sanders <vince@kyllikki.org>
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-library.h>

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

#include "stv0674.h"
#include "library.h"

#define GP_MODULE "stv0674"

static const struct camera_to_usb {
	  char *name;
	  unsigned short idVendor;
	  unsigned short idProduct;
} camera_to_usb[] = {
	/* http://www.digitaldreamco.com/shop/xtra.htm, SVGA */
      { "DigitalDream:l'espion xtra",   0x05DA, 0x1020 },
      { "Che-ez!:Splash",   		0x0553, 0x1002 }

};

int camera_id (CameraText *id)
{
	strcpy(id->text, "STV0674");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list)
{
    CameraAbilities a;
    unsigned int i;

    for(i = 0;
	i < (sizeof(camera_to_usb) / sizeof(struct camera_to_usb));
	i++)
    {

	memset(&a, 0, sizeof(a));
	strcpy(a.model, camera_to_usb[i].name);

	a.port			= GP_PORT_USB;
	a.status		= GP_DRIVER_STATUS_EXPERIMENTAL;
	a.operations		= GP_OPERATION_CAPTURE_IMAGE |
				  GP_OPERATION_CAPTURE_PREVIEW;
	a.file_operations	= GP_FILE_OPERATION_PREVIEW;
	a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;

	a.usb_vendor  = camera_to_usb[i].idVendor;
	a.usb_product = camera_to_usb[i].idProduct;

	gp_abilities_list_append(list, a);
    }

    return (GP_OK);
}


static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context)
{
    Camera *camera = data;
    int count, result;


    result = stv0674_file_count(camera->port, &count);
    if (result < GP_OK)
    {
	GP_DEBUG("file count returnd %d\n",result);
	return result;
    }

    GP_DEBUG("count is %x\n",count);

    gp_list_populate(list, "image%03i.jpg", count);

    return (GP_OK);
}



static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
	int image_no, result;

	image_no = gp_filesystem_number(camera->fs, folder, filename, context);
	if(image_no < 0)
		return image_no;

	gp_file_set_mime_type (file, GP_MIME_JPEG);
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		result = stv0674_get_image (camera->port, image_no, file);
		break;
	case GP_FILE_TYPE_RAW:
		result = stv0674_get_image_raw (camera->port, image_no, file);
		break;
	case GP_FILE_TYPE_PREVIEW:
		result = stv0674_get_image_preview (camera->port, image_no, file);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
	return result;
}

static int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
{
	int result;
	int count,oldcount;

	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);
	result = stv0674_file_count(camera->port,&oldcount);

	result = stv0674_capture(camera->port);
	if (result < 0)
		return result;

	/* Just added a new picture... */
	result = stv0674_file_count(camera->port,&count);
	if (count == oldcount)
	    return GP_ERROR; /* unclear what went wrong  ... hmm */
	strcpy(path->folder,"/");
	sprintf(path->name,"image%03i.jpg",count);

	/* Tell the filesystem about it */
	result = gp_filesystem_append (camera->fs, path->folder, path->name,
				       context);
	if (result < 0)
		return (result);

	return (GP_OK);
}

static int camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	char *data;
	int size, result;

	result = stv0674_capture_preview (camera->port, &data, &size);
	if (result < 0)
		return result;
	gp_file_set_mime_type (file, GP_MIME_JPEG);
	return gp_file_set_data_and_size (file, data, size);
}

static int camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	stv0674_summary(camera->port,summary->text);
	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text,
		_("STV0674\n"
		"Vincent Sanders <vince@kyllikki.org>\n"
		"Driver for cameras using the STV0674 processor ASIC.\n"
		"Protocol reverse engineered using SnoopyPro\n"));

	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char* folder, void *data,
		 GPContext *context)
{
        Camera *camera = data;
	if (strcmp (folder, "/"))
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	return stv0674_delete_all(camera->port);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.delete_all_func = delete_all_func
};

int camera_init (Camera *camera, GPContext *context)
{
    GPPortSettings settings;
    int ret;

    /* First, set up all the function pointers */
    camera->functions->summary              = camera_summary;
    camera->functions->about                = camera_about;
    camera->functions->capture_preview	= camera_capture_preview;
    camera->functions->capture		= camera_capture;

    gp_port_get_settings(camera->port, &settings);
    switch(camera->port->type) {
    case GP_PORT_USB:
	/* Modify the default settings the core parsed */
	settings.usb.altsetting=1;/* we need to use interface 0 setting 1 */
	settings.usb.inep=2;
	settings.usb.intep=3;
	settings.usb.outep=5;

	/* Use the defaults the core parsed */
	break;
    default:
	return (GP_ERROR_UNKNOWN_PORT);
	break;
    }

    ret = gp_port_set_settings (camera->port, settings);
    if (ret != GP_OK) {
	gp_context_error (context, _("Could not apply USB settings"));
	return ret;
    }
    /* Set up the filesystem */
    gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
    /* test camera */
    return stv0674_ping(camera->port);
}
