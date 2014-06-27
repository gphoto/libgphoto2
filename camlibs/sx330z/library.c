/* library.c *
 * Copyright 2002 Dominik Kuhlen <dkuhlen@fhm.edu>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>

#include "sx330z.h"


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
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif



#define GP_MODULE "sx330z"


#define TIMEOUT 2000

#define SX330Z_VERSION "0.1"
#define SX330Z_LAST_MOD "02.07.2002"


#define CR(res) { int r=(res);if (r<0) return (r);}

/*
 * supported cameras : 
 * - Traveler SX330z (Aldi cam)
 * - Other SX330z / SX3300z Models ?
 *   please report if you have others that are working
 */
 
static const struct
{
 char *model;
 int usb_vendor;
 int usb_product;
} models[]= {
 {"Traveler:SX330z",USB_VENDOR_TRAVELER,USB_PRODUCT_SX330Z},
 {"Maginon:SX330z",USB_VENDOR_TRAVELER,USB_PRODUCT_SX330Z},
 {"Skanhex:SX-330z",USB_VENDOR_TRAVELER,USB_PRODUCT_SX330Z},
 {"Medion:MD 9700",USB_VENDOR_TRAVELER,USB_PRODUCT_MD9700},
 {"Jenoptik:JD-3300z3",USB_VENDOR_TRAVELER,USB_PRODUCT_SX330Z},

 {"Traveler:SX410z",USB_VENDOR_TRAVELER,USB_PRODUCT_SX410Z},
 {"Concord:EyeQ 4330",USB_VENDOR_TRAVELER,USB_PRODUCT_SX410Z},
 {"Jenoptik:JD-4100z3",USB_VENDOR_TRAVELER,USB_PRODUCT_SX410Z},
 {"Medion:MD 6000",USB_VENDOR_TRAVELER,USB_PRODUCT_SX410Z},
 {"Maginon SX-410z",USB_VENDOR_TRAVELER,USB_PRODUCT_SX410Z},
 {"Lifetec:LT 5995",USB_VENDOR_TRAVELER,USB_PRODUCT_SX410Z},
 {NULL,0,0}
};


/*
 * camera abilities 
 */
int 
camera_abilities (CameraAbilitiesList *list)
{
 int i;
 CameraAbilities a;
 for(i=0;models[i].model;i++)
 {
  memset(&a,0,sizeof(CameraAbilities));
  strcpy(a.model,models[i].model);
  a.usb_vendor  = models[i].usb_vendor;
  a.usb_product = models[i].usb_product;
  a.status      = GP_DRIVER_STATUS_EXPERIMENTAL;
  a.port        = GP_PORT_USB;
  a.speed[0]    = 0;
  a.operations  = GP_OPERATION_NONE;
  a.file_operations = GP_FILE_OPERATION_PREVIEW|
     			GP_FILE_OPERATION_DELETE|
                        GP_FILE_OPERATION_EXIF;
  a.folder_operations = GP_FOLDER_OPERATION_NONE; 
  CR(gp_abilities_list_append(list,a));
 } /* all models... */
 return(GP_OK);
} /* camera_abilities */




/*
 *	file list function 
 */
static int 
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	CameraFileInfo info;
	int32_t tpages=0;
	int pcnt,ecnt;		/* pagecounter, entrycounter*/
	struct traveler_toc_page toc;
	int id;
 
	/* get number of TOC pages */
	CR (sx330z_get_toc_num_pages (camera, context, &tpages));
	/* Read the TOC pages */
	id = gp_context_progress_start (context, tpages, _("Getting "
			"information on %i files..."), tpages);
	for (pcnt = 0; pcnt < tpages; pcnt++) {
		CR (sx330z_get_toc_page (camera, context, &toc, pcnt));
		for (ecnt = 0; ecnt < toc.numEntries; ecnt++) {
			char fn[20];

			info.audio.fields = GP_FILE_INFO_NONE;
			info.preview.fields = GP_FILE_INFO_TYPE;
			strcpy (info.preview.type, GP_MIME_EXIF);
			info.file.fields = GP_FILE_INFO_SIZE |
					   GP_FILE_INFO_TYPE |
					   GP_FILE_INFO_PERMISSIONS;
			info.file.size = toc.entries[ecnt].size;
			info.file.permissions = GP_FILE_PERM_READ |
						GP_FILE_PERM_DELETE;
			strcpy (info.file.type,GP_MIME_JPEG); 
			sprintf (fn, "%.12s", toc.entries[ecnt].name);

			/*
			 * Append directly to the filesystem instead of to
			 * the list, because we have additional information.
			 */
			gp_filesystem_append (camera->fs, folder, fn, context);
			gp_filesystem_set_info_noop (camera->fs, folder, fn,
						     info, context);
		}
		gp_context_progress_update (context, id, pcnt);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
	}
	
	gp_context_progress_stop (context, id);

	return (GP_OK);
}


static int 
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
	char *data = NULL;
	unsigned long int size = 0;
 
	switch (type) {
	case GP_FILE_TYPE_EXIF:
		gp_file_set_mime_type (file, GP_MIME_RAW);
		CR (sx330z_get_data (camera, context, filename, &data,
				     &size, SX_THUMBNAIL));
		break;
	case GP_FILE_TYPE_RAW:
	case GP_FILE_TYPE_NORMAL:
		gp_file_set_mime_type (file, GP_MIME_JPEG);
		CR (sx330z_get_data (camera, context, filename, &data,
				     &size, SX_IMAGE));
		break;
	case GP_FILE_TYPE_PREVIEW:
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
	gp_file_set_data_and_size (file, data, size);
	
	return (GP_OK);
}


/*
 * 	Delete file function (working ..)
 */
static int 
del_file_func(CameraFilesystem *fs,const char *folder,const char *filename,
   	void *user_data,GPContext *context)
{
 Camera *camera=user_data;
 /*int id;*/
 if (strcmp(folder,"/")) return(GP_ERROR_DIRECTORY_NOT_FOUND);
 GP_DEBUG("Deleting : %s",filename);
 return(sx330z_delete_file(camera,context,filename));
} /* delete  file func */


/*
 * Camera ID 
 */
int 
camera_id(CameraText *id)
{
 strcpy(id->text,"Traveler SX330z");
 return (GP_OK);
} /* camera id */




/*
 * Camera about 
 */
static int 
camera_about(Camera *camera,CameraText *about,GPContext *context)
{
 strcpy(about->text,_("(Traveler) SX330z Library (And other Aldi-cams).\n"
	"Even other Vendors like Jenoptik, Skanhex, Maginon should work.\n"
	"Please send bugreports and comments.\n"
	"Dominik Kuhlen <kinimod@users.sourceforge.net>\n"));
 return(GP_OK);
} /* camera about */


/*
 * camera_exit
 * release allocated memory
 */
int 
camera_exit(Camera *camera, GPContext *context)
{
 if (camera->pl)
  free(camera->pl);
 return(GP_OK);
}


static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = del_file_func
};

/*
 * OK, lets get serious !
 */
int 
camera_init(Camera *camera,GPContext *context)
{
 GPPortSettings settings;
 CameraAbilities abilities;
 /* try to contact the camera ...*/
 /*CR(gp_port_get_settings(camera->port,&settings));*/
 
 camera->functions->about=camera_about; 
 camera->functions->exit=camera_exit;
 gp_port_get_settings(camera->port,&settings);
 if (camera->port->type!=GP_PORT_USB)
 {
  gp_context_error(context,_("sx330z is USB only"));
  return(GP_ERROR_UNKNOWN_PORT);
 }
/* GP_DEBUG("camera_init : in = %x",camera->port->settings.usb.inep);*/
 CR(gp_port_set_settings(camera->port,settings));
 CR(gp_port_set_timeout(camera->port,TIMEOUT)); 

 CR(gp_filesystem_set_funcs(camera->fs, &fsfuncs, camera));
 
 camera->pl=malloc(sizeof(CameraPrivateLibrary));
 if (!camera->pl) 
   return(GP_ERROR_NO_MEMORY);

 CR(gp_camera_get_abilities(camera, &abilities));
 camera->pl->usb_product=abilities.usb_product; /* some models differ in Thumbnail size */
/* GP_DEBUG("sx330z Camera_init : sx init %04x",camera->pl->usb_product); */
 return(sx330z_init(camera,context));

} /* camera init */

