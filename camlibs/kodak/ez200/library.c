/* library.c
 *
 * Copyright (C) 2004 Jean-François Bucas <jfbucas@tuxfamily.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <bayer.h>
#include <gamma.h>

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

#include "ez200.h"
#include <gphoto2-port.h>

#define GP_MODULE "ez200"

struct _CameraPrivateLibrary {
	Model model;
	Info info[2];
};

struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"Kodak EZ200", GP_DRIVER_STATUS_TESTING, 0x040a, 0x0300}, /* GP_DRIVER_STATUS_EXPERIMENTAL, */
	{NULL,0,0}
};

int
camera_id (CameraText *id)
{
    	strcpy (id->text, "Kodak EZ200 camera");
    	return GP_OK;
}

int
camera_abilities (CameraAbilitiesList *list)
{
    	int i;    
    	CameraAbilities a;

	GP_DEBUG("Begin_Abilities");

    	for (i = 0; models[i].name; i++) {
        	memset (&a, 0, sizeof(a));
       		strcpy (a.model, models[i].name);
       		a.status = models[i].status;
       		a.port   = GP_PORT_USB;
       		a.speed[0] = 0;
       		a.usb_vendor = models[i].idVendor;
       		a.usb_product= models[i].idProduct;
       		if (a.status == GP_DRIVER_STATUS_EXPERIMENTAL)
			a.operations = GP_OPERATION_NONE;
		else
			a.operations = GP_OPERATION_CAPTURE_IMAGE;
       		a.folder_operations = GP_FOLDER_OPERATION_NONE;
       		a.file_operations   = GP_FILE_OPERATION_NONE; 
       		gp_abilities_list_append (list, a);
    	}
	
	GP_DEBUG("End_Abilities");
	
    	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{

	int num_pics =ez200_get_num_pics(camera->pl->info);
	
	GP_DEBUG("Summary");

    	sprintf (summary->text,_("Your USB camera is a Kodak EZ200.\n" 
			"Number of PICs = %i\n"
       			), num_pics);  
	
	GP_DEBUG("End_Summary");
    	
	return GP_OK;
}


static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
    	strcpy (about->text, _("Kodak EZ200 driver\n"
			    "Bucas Jean-François <jfbucas@tuxfamily.org>\n"));
    	return GP_OK;
}

/*************** File and Downloading Functions *******************/

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
	Camera *camera = data; 
	int num_pics = ez200_get_num_pics (camera->pl->info);
	char name[18];
	int i;

	GP_DEBUG("file_list_start\n");
	
	for (i=0; i< num_pics; i++){
		sprintf( name, "ez200_pic%03i.jpg", i+1 );
		gp_list_append(list, name, NULL);	
	}
	GP_DEBUG("file_list_stop\n");

    	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
    	Camera *camera = user_data; 
        int n; 
	int len;
	char *data;
	char *data_start;

	n = gp_filesystem_number(camera->fs, "/", filename, context);

	len = ez200_get_picture_size (camera->port, n);
	GP_DEBUG("len = %i\n", len);
	data = (char *)malloc(len + HEADER_SIZE + 1);
	if (!data) return GP_ERROR_NO_MEMORY;

	data_start = data + (HEADER_SIZE - DATA_HEADER_SIZE);
	GP_DEBUG("data - data_start : %p %p : %x\n",data, data_start, data_start - data);
	
    	ez200_read_picture_data   (camera->port, data_start, len, n);
	ez200_read_picture_header (camera->port, data);

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
	case GP_FILE_TYPE_NORMAL:
		/*gp_file_set_mime_type (file, GP_MIME_JPEG);
		  gp_file_set_type (file, GP_MIME_JPEG);*/
		gp_file_set_data_and_size (file, data, len + HEADER_SIZE + 1);
		/*gp_file_adjust_name_for_mime_type (file);*/
		break;
	case GP_FILE_TYPE_RAW:
		gp_file_set_data_and_size (file, data, len);
		gp_file_set_mime_type (file, GP_MIME_RAW);
		gp_file_adjust_name_for_mime_type(file);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED); 	

	}

	return GP_OK;
}

/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("Kodak EZ200 camera_exit");
	
	ez200_exit (camera->port);
			
	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return GP_OK;
}

int
camera_init(Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	int ret = 0;

	/* First, set up all the function pointers */
	camera->functions->summary	= camera_summary;
	camera->functions->about	= camera_about;
	camera->functions->exit		= camera_exit;
   
	GP_DEBUG ("Initializing Kodak EZ200\n");
	ret = gp_port_get_settings(camera->port, &settings);
	if (ret < 0) return ret; 

	switch (camera->port->type) {
		case GP_PORT_USB:
			settings.usb.config	= 1;
			settings.usb.altsetting	= 0;
			settings.usb.interface	= 1;
			settings.usb.inep	= 0x82;
			settings.usb.outep	= 0x03;
			break;
		default:
			return ( GP_ERROR );
	}

	ret = gp_port_set_settings(camera->port,settings);
	if (ret < 0) return ret; 

	GP_DEBUG("interface = %i\n", settings.usb.interface);
	GP_DEBUG("inep = %x\n", settings.usb.inep);	
	GP_DEBUG("outep = %x\n", settings.usb.outep);

        /* Tell the CameraFilesystem where to get lists from */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));

	/* Connect to the camera */
	ez200_init (camera->port, &camera->pl->model, camera->pl->info);

	GP_DEBUG("fin_camera_init\n");	

	return GP_OK;
}
