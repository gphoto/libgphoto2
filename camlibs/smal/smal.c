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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2-library.h>
#include <gphoto2-result.h>
#include <bayer.h>
#include <gamma.h>

#include "ultrapocket.h"
#include "smal.h"

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
	strcpy(id->text, "Smal:Ultrapocket");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list) 
{
   CameraAbilities a;
   
   memset(&a, 0, sizeof(a));
   strcpy(a.model, "Smal:Ultrapocket");
   a.status = GP_DRIVER_STATUS_EXPERIMENTAL; /* highly! */
   a.port     		= GP_PORT_USB;
   a.speed[0] 		= 0;
   a.usb_vendor		= USB_VENDOR_ID_SMAL;
   a.usb_product        = USB_DEVICE_ID_ULTRAPOCKET;
   a.operations		= GP_OPERATION_NONE;
   a.file_operations	= GP_FILE_OPERATION_DELETE;
   a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;
   gp_abilities_list_append(list, a);
   /* fuji @xia slimshot is just a rebranded SmAL Ultrapocket */
   /* LB: 01/03 - not quite - it's got a different header */
   strcpy(a.model, "Fuji:Slimshot");
   gp_abilities_list_append(list, a);
   
   return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context) 
{
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

   gp_file_set_name (file, filename);

   switch (type) {
    case GP_FILE_TYPE_NORMAL:
      result = ultrapocket_getpicture(camera->port,context,&data,&size,filename);
      gp_file_set_mime_type (file, GP_MIME_PPM);
      break;
    case GP_FILE_TYPE_RAW:
#if 0      
      return GP_ERROR;
      result = ultrapocket_getpicture(camera->port,context,file);
      gp_file_set_mime_type (file, GP_MIME_RAW);
      break;
#endif      
    case GP_FILE_TYPE_PREVIEW:
    default:
      return (GP_ERROR_NOT_SUPPORTED);
   }
   if (result < 0)
     return result;
   
   CHECK_RESULT(gp_file_set_data_and_size (file, data, size));
   
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

   CHECK_RESULT(ultrapocket_deletefile(camera->port, filename));
   
   return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char* folder, void *data,
 	         GPContext *context)
{
   Camera *camera = data;
   if (strcmp (folder, "/"))
     return (GP_ERROR_DIRECTORY_NOT_FOUND);
   return ultrapocket_deleteall(&camera->port);
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
   
   result =  ultrapocket_getpicsoverview(&camera->port, context, &count, list);
   if (result != GP_OK)
     return result;
   
   return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context) 
{
   camera->functions->exit                 = camera_exit;
   camera->functions->about                = camera_about;
   gp_filesystem_set_list_funcs (camera->fs, file_list_func,NULL, camera);
   gp_filesystem_set_file_funcs (camera->fs, get_file_func,delete_file_func, camera);
   gp_filesystem_set_folder_funcs (camera->fs,NULL,delete_all_func, NULL,NULL,camera);
   
   /* don't need to do any exciting init stuff until we get pic numbers */
   return GP_OK;
}
