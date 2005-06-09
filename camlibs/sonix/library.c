/* library.c
 *
 * Copyright (C) 2005 Theodore Kilgore <kilgota@auburn.edu>
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

#include <gphoto2-port.h>
#include "sonix.h"
#define GP_MODULE "sonix"

struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"Vivitar Vivicam3350B", GP_DRIVER_STATUS_EXPERIMENTAL, 0x0c45, 0x800a},
        {"DC31VC", GP_DRIVER_STATUS_EXPERIMENTAL, 0x0c45, 0x8000},	
 	{NULL,0,0,0}
};

int
camera_id (CameraText *id)
{
    	strcpy (id->text, "Vivitar ViviCam3350B");
    	return GP_OK;
}

int
camera_abilities (CameraAbilitiesList *list)
{
    	int i;    
    	CameraAbilities a;

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
       		a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;
       		a.file_operations   = GP_FILE_OPERATION_PREVIEW;
       		gp_abilities_list_append (list, a);
    	}
    	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	switch(camera->pl->num_pics) {
	case 1:
    	sprintf (summary->text,_("Sonix camera.\n" 
			"There is %i photo in it. \n"), camera->pl->num_pics);  
	break;
	default:
    	sprintf (summary->text,_("Sonix camera.\n" 
			"There are %i photos in it. \n"), camera->pl->num_pics);  
	}	

    	return GP_OK;
}


static int camera_manual (Camera *camera, CameraText *manual, GPContext *context) 
{
	strcpy(manual->text, 
	_(
	"This driver supports cameras with Sonix chip\n"
	"and should work with gtkam.\n"
	"The driver allows you to get\n"
	"   - thumbnails for gtkam\n"
	"   - full images in PPM format\n"
	"If present on the camera, video clip frames are downloaded \n"
	"as consecutive still photos.\n"
	"Capture not supported" 
	)); 

	return (GP_OK);
}




static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
    	strcpy (about->text, _("Sonix camera library\n"
			    "Theodore Kilgore <kilgota@auburn.edu>\n"));
    	return GP_OK;
}

/*************** File and Downloading Functions *******************/

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
        Camera *camera = data; 
    	gp_list_populate (list, "sonix%03i.ppm", camera->pl->num_pics);
    	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
    	Camera *camera = user_data; 
    	int w, h = 0, b = 0, i, k;
    	unsigned char *data; 
    	unsigned char  *ppm, *ptr;
	unsigned char *p_data = NULL;
	unsigned char temp;
	unsigned int size = 0;

    	GP_DEBUG ("Downloading pictures!\n");

    	/* Get the number of the photo on the camera */
	k = gp_filesystem_number (camera->fs, "/", filename, context); 

	if(type == GP_FILE_TYPE_EXIF) return GP_ERROR_FILE_EXISTS;


    	w = sonix_get_picture_width (camera->port, k);
    	switch (w) {
	case 640: h = 480; break;
	case 320: h = 240; break;
	default:  h = 480; break;
	}
	
	GP_DEBUG ("height of picture %i is %i\n", k+1,h); 		
	data = malloc (w*h);
	if (!data) return GP_ERROR_NO_MEMORY;
    	memset (data, 0, sizeof(data));

	p_data = malloc (w * h);
	if (!p_data) {free (data); return GP_ERROR_NO_MEMORY;}
	memset (p_data, 0, w * h);

	switch(type) {
	case GP_FILE_TYPE_PREVIEW:
	case GP_FILE_TYPE_NORMAL:
		b =sonix_read_picture_data (camera,  
					    camera->port, data, k);

		ppm = malloc (w * h * 3 + 256); /* room for data + header */
		if (!ppm) { return GP_ERROR_NO_MEMORY; }
    		sprintf (ppm,
			"P6\n"
			"# CREATOR: gphoto2, SONIX library\n"
			"%d %d\n"
			"255\n", w, h);
		ptr = ppm + strlen (ppm);
		size = strlen (ppm) + (w * h * 3);

		sonix_decode (p_data, data, w, h);
		/* Image is upside down. We fix. */
		for (i=0; i< w*h /2 ; ++i){
			temp = p_data[i];
			p_data[i] = p_data[w*h - 1 - i];
			p_data[w*h - 1 - i] = temp;
		}

		gp_bayer_decode (p_data, w , h , ptr, BAYER_TILE_GBRG);

    		gp_file_set_mime_type (file, GP_MIME_PPM);
    		gp_file_set_name (file, filename); 
		gp_file_set_data_and_size (file, ppm, size);

		free (p_data);
		free (data);

	        return GP_OK;

		break;	


	case GP_FILE_TYPE_RAW: 
		b =sonix_read_picture_data (camera, 
					    camera->port, data, k);
		gp_file_set_mime_type(file, GP_MIME_RAW);
		gp_file_set_name(file, filename);
		gp_file_append( file, data, b);
		free(data);
		GP_DEBUG("b= 0x%x = %i\n", b, b);
		return GP_OK;
		break;

	default:
		free(data);
		return	GP_ERROR_NOT_SUPPORTED;
	}	

        return GP_OK;
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data, 
		GPContext *context) 
{
	Camera *camera = data;
	sonix_delete_all_pics (camera->port);
	return GP_OK;
}

/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("Sonix camera_exit");
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
        camera->functions->manual	= camera_manual;
	camera->functions->summary      = camera_summary;
	camera->functions->about        = camera_about;
	camera->functions->exit	    = camera_exit;
   
	GP_DEBUG ("Initializing the camera\n");
	ret = gp_port_get_settings(camera->port,&settings);
	if (ret < 0) return ret; 

	switch (camera->port->type) {
		case GP_PORT_SERIAL:
			return ( GP_ERROR );
		case GP_PORT_USB:
			settings.usb.config = 1;
			settings.usb.altsetting = 0;
			settings.usb.interface = 0;
			settings.usb.inep = 0x82;
			settings.usb.outep =0x05;
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
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, 
		NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, 
		NULL, camera);
	gp_filesystem_set_folder_funcs (camera->fs, NULL, delete_all_func, 
		NULL, NULL, camera);

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
	camera->pl->num_pics = 0;
	camera->pl->full = 1;
	/* Connect to the camera */
	ret = sonix_init (camera->port, camera->pl);
	if ( ret != GP_OK) {
		free(camera->pl);
		return ret;
	}	
	
	
	return GP_OK;
}
