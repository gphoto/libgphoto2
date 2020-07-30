/* smal.c
 *
 * Copyright (C) 2003 Lee Benfield <lee@benf.org>
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
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <bayer.h>
#include <gamma.h>

#include "smal.h"
#include "ultrapocket.h"

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
#  define _(String) (String)
#  define N_(String) (String)
#endif

int
camera_id (CameraText *id)
{
	strcpy(id->text, "smal");
	return (GP_OK);
}


static const struct smal_cameras {
	char * name;
	unsigned short idVendor;
	unsigned short idProduct;
} smal_cameras [] = {
	{ "Fuji:Axia Slimshot",      USB_VENDOR_ID_SMAL,     USB_DEVICE_ID_ULTRAPOCKET },
	{ "Fuji:Axia Eyeplate",      USB_VENDOR_ID_SMAL,     USB_DEVICE_ID_ULTRAPOCKET },
	{ "Logitech:Pocket Digital", USB_VENDOR_ID_LOGITECH, USB_DEVICE_ID_POCKETDIGITAL },
	{ "SMaL:Ultra-Pocket",       USB_VENDOR_ID_SMAL,     USB_DEVICE_ID_ULTRAPOCKET },
        { "Radioshack:Flatfoto",     USB_VENDOR_ID_SMAL,     USB_DEVICE_ID_FLATFOTO },
	{ "Creative:CardCam",        USB_VENDOR_ID_CREATIVE, USB_DEVICE_ID_CARDCAM },
	{ NULL, 0, 0 }
};

int
camera_abilities (CameraAbilitiesList *list)
{
   CameraAbilities a;
   int i;

   memset(&a, 0, sizeof(a));
   a.status = GP_DRIVER_STATUS_EXPERIMENTAL; /* highly! */
   a.port     		= GP_PORT_USB;
   a.speed[0] 		= 0;
   a.operations		= GP_OPERATION_NONE;
   a.file_operations	= GP_FILE_OPERATION_DELETE;
   a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;
   for (i = 0; smal_cameras[i].name; i++) {
      strcpy(a.model, smal_cameras[i].name);
      a.usb_vendor	= smal_cameras[i].idVendor;
      a.usb_product     = smal_cameras[i].idProduct;
      gp_abilities_list_append(list, a);
   }
   return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
    if (camera->pl) {
	free(camera->pl);
	camera->pl = NULL;
    }
    return ultrapocket_exit(camera->port, context);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
   Camera *camera = user_data;
   int size,image_no,result;
   unsigned char *data;

   image_no = gp_filesystem_number(fs, folder, filename, context);
   if (image_no < GP_OK)
     return image_no;

   switch (type) {
    case GP_FILE_TYPE_NORMAL:
      result = ultrapocket_getpicture(camera,context,&data,&size,filename);
      gp_file_set_mime_type (file, GP_MIME_PPM);
      break;
    case GP_FILE_TYPE_RAW:
      result = ultrapocket_getrawpicture(camera, context,
		      &data, &size, filename);
      gp_file_set_mime_type (file, GP_MIME_PPM);
      break;
    case GP_FILE_TYPE_PREVIEW:
    default:
      return (GP_ERROR_NOT_SUPPORTED);
   }
   if (result < 0)
     return result;

   CHECK_RESULT(gp_file_set_data_and_size (file, (char *)data, size));

   return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
   Camera *camera = data;
   int image_no;

   image_no = gp_filesystem_number(fs, folder, filename, context);
   if (image_no < GP_OK)
     return image_no;

   CHECK_RESULT(ultrapocket_deletefile(camera, filename));

   return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char* folder, void *data,
 	         GPContext *context)
{
   Camera *camera = data;
   if (strcmp (folder, "/"))
     return (GP_ERROR_DIRECTORY_NOT_FOUND);
   return ultrapocket_deleteall(camera);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
   strcpy (about->text, _("Smal Ultrapocket\n"
			  "Lee Benfield <lee@benf.org>\n"
			  "Driver for accessing the Smal Ultrapocket camera, and OEM versions (slimshot)"));

   return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
   Camera *camera = data;
   int count, result;

   result =  ultrapocket_getpicsoverview(camera, context, &count, list);
   if (result != GP_OK)
     return result;

   return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func,
};

int
camera_init (Camera *camera, GPContext *context)
{
    CameraAbilities cab;
    up_badge_type badge;

    camera->functions->exit                 = camera_exit;
    camera->functions->about                = camera_about;
    gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
    badge = BADGE_UNKNOWN;
    gp_camera_get_abilities(camera, &cab);
    switch (cab.usb_vendor) {
     case USB_VENDOR_ID_SMAL:
     case USB_VENDOR_ID_CREATIVE:
	switch (cab.usb_product) {
	 case USB_DEVICE_ID_ULTRAPOCKET:
	    /* could be an axia eyeplate or a slimshot
	     * figure it out later, when we get the image
	     * catalogue.
	     */
	    badge = BADGE_GENERIC;
	    break;
	 case USB_DEVICE_ID_FLATFOTO:
	    badge = BADGE_FLATFOTO;
	    break;
	 case USB_DEVICE_ID_CARDCAM:
	    badge = BADGE_CARDCAM;
	    break;
	 default:
	    break;
	}
	break;
     case USB_VENDOR_ID_LOGITECH:
	switch (cab.usb_product) {
	 case USB_DEVICE_ID_POCKETDIGITAL:
	    badge = BADGE_LOGITECH_PD;
	    break;
	 default:
	    break;
	}
	break;
     default:
	break;
    }

    if (badge == BADGE_UNKNOWN) {
	/* can't happen.  Otherwise, how'd we get to camera_init, neh? */
	return GP_ERROR;
    }

    camera->pl = malloc (sizeof (CameraPrivateLibrary));
    camera->pl->up_type = badge;
    /* don't need to do any exciting init stuff until we get pic numbers */
    return GP_OK;
}
