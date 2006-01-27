/* library.c for libgphoto2/camlibs/clicksmart310
 *
 * Copyright (C) 2006 Theodore Kilgore <kilgota@auburn.edu>
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

#include <gphoto2.h>
#include <gphoto2-port.h>

#define JPEG_QCIF_FORMAT 0x22
#define JPEG_CIF_FORMAT 0x21
         
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

#include "clicksmart.h"
#define GP_MODULE "clicksmart310"

struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"Logitech Clicksmart 310", GP_DRIVER_STATUS_TESTING, 0x46d, 0x0900},
	{NULL,0,0,0}
};

int
camera_id (CameraText *id)
{
    	strcpy (id->text, "Logitech Clicksmart 310");
        
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
			a.operations = GP_OPERATION_NONE;
       		a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;
;
		a.file_operations   = GP_FILE_OPERATION_PREVIEW 
						+ GP_FILE_OPERATION_RAW; 
       		gp_abilities_list_append (list, a);
    	}

    	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
    	sprintf (summary->text,_("Your Logitech Clicksmart 310\n" 
				"has  %i pictures in it.\n"
				), 

				camera->pl->num_pics);  

    	return GP_OK;
}


static int camera_manual (Camera *camera, CameraText *manual, GPContext *context) 
{
	strcpy(manual->text, 
	_(
	"Two resolution settings, 352x288 and 176x144. Photo data \n"
	"is in JPEG format when downloaded and thus has no predetermined\n"
	"size. Therefore, the advertised maximum number of photos the\n"
	"camera can hold must be understood as an approximation.\n"
	"All gphoto2 options will work, except for the following which\n"
	"the hardware will not support:\n"
	"	Deletion of individual or selected photos (gphoto2 -d)\n"
	"	Capture (gphoto2 --capture or --capture-image)\n"
	"However, capture is possible using the webcam interface,\n" 
	"supported by the spca50x kernel module.\n"
	"GUI access using gtkam has been tested, and works. However,\n" 
	"the camera does not produce separate thumbnails. Since the images\n"
	"are in any event already small and of low resolution, the driver\n"
	"merely downloads the actual images to use as thumbnails.\n" 
	"The camera can shoot in 'video clip' mode. The resulting frames\n"
	"are saved here as a succession of still photos. The user can \n"
	"animate them using (for example) ImageMagick's 'animate' function.\n"
	"For more details on the camera's functions, please consult\n"
	"libgphoto2/camlibs/README.\n"
	)); 

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
    	strcpy (about->text, _("Logitech Clicksmart 310 driver\n"
			    "Theodore Kilgore <kilgota@auburn.edu>\n"));

    	return GP_OK;
}

/*************** File and Downloading Functions *******************/


static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
        Camera *camera = data; 
	int n;
	GP_DEBUG ("List files in %s\n", folder);
    	n = camera->pl->num_pics;	
	gp_list_populate(list, "cs%03i.jpeg", n);
	return GP_OK;
}


static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
    	Camera *camera = user_data; 
	int w=0, h=0, b=0; 
	int k, res;
	unsigned char *data; 
	unsigned char *jpeg_out = NULL;
	int file_size;
	unsigned char jpeg_format;

	/* Get the entry number of the photo on the camera */
    	k = gp_filesystem_number (camera->fs, "/", filename, context); 
    	
	if (GP_FILE_TYPE_EXIF ==type) return GP_ERROR_FILE_EXISTS;

	if (GP_FILE_TYPE_RAW!=type && GP_FILE_TYPE_NORMAL
				    !=type && GP_FILE_TYPE_PREVIEW!=type) {

		return GP_ERROR_NOT_SUPPORTED;
	}
	
	res = clicksmart_get_res_setting (camera->pl, k);

    	switch (res) {
	case 0: w = 352;
		h = 288; 
		jpeg_format = JPEG_CIF_FORMAT;
		break;
	case 1: 
	case 3:
		w = 176;
		h = 144;
		jpeg_format = JPEG_QCIF_FORMAT;
		break;
	default:  GP_DEBUG ( "Unknown resolution setting %i\n", res);
		return GP_ERROR;		
	}

	data = malloc (w*h);
	if(!data) return GP_ERROR_NO_MEMORY;

	GP_DEBUG("Fetch entry %i\n", k);
	b = clicksmart_read_pic_data (camera->pl, camera->port, data, k);

	if (GP_FILE_TYPE_RAW == type) {	/* type is GP_FILE_TYPE_RAW */
		gp_file_set_mime_type (file, GP_MIME_RAW);
		gp_file_set_name (file, filename);
	        gp_file_set_data_and_size (file, data, b);  
		/* Reset camera when done, for more graceful exit. */
		if (k +1 == camera->pl->num_pics) {
	    		clicksmart_reset (camera->port);	
		}
		return(GP_OK);
	}
		
	GP_DEBUG ("size = %i\n", b);	

	/* It looks as though o_size = b */
	/* It seems that qIndex is byte7, which is always 3, so I use that. */
	
	file_size = b + 589 + 1024 * 10;

	jpeg_out = malloc(file_size);
	if (!jpeg_out) return GP_ERROR_NO_MEMORY;

	GP_DEBUG("width:  %d, height:  %d, data size:  %d\n", w, h, b); 
	create_jpeg_from_data (jpeg_out, data, 3, w, h, jpeg_format, 
		    b, &file_size, 0, 0);

	gp_file_set_mime_type (file, GP_MIME_JPEG);
    	gp_file_set_name (file, filename); 
	gp_file_set_data_and_size (file, jpeg_out, file_size);
	/* Reset camera when done, for more graceful exit. */
	if (k +1 == camera->pl->num_pics) {
    		clicksmart_reset (camera->port);
	}
        return GP_OK;
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data, 
		GPContext *context) 
{
	Camera *camera = data;
	clicksmart_delete_all_pics (camera->port);
	return GP_OK;
}


/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("SQ camera_exit");

	if (camera->pl) {
		free (camera->pl->catalog);
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
	camera->functions->summary      = camera_summary;
        camera->functions->manual	= camera_manual;
	camera->functions->about        = camera_about;
	camera->functions->exit	    	= camera_exit;
   
	GP_DEBUG ("Initializing the camera\n");

	ret = gp_port_get_settings(camera->port,&settings);
	if (ret < 0) return ret; 
	switch (camera->port->type) {
		case GP_PORT_SERIAL:
			return ( GP_ERROR );
		case GP_PORT_USB:
			settings.usb.config = 1;
			settings.usb.altsetting = 0;
			settings.usb.inep = 0x82;
			settings.usb.outep =0x03;
			break;
		default:
			return ( GP_ERROR );
	}

 

	ret = gp_port_set_settings(camera->port,settings);
	if (ret < 0) return ret; 

        /* Tell the CameraFilesystem where to get lists from */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, 
					    NULL, camera);
        gp_filesystem_set_info_funcs (camera->fs, NULL, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);
	gp_filesystem_set_folder_funcs (camera->fs, NULL, delete_all_func, 
					    NULL, NULL, camera);
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;
	camera->pl->catalog = NULL;
	camera->pl->num_pics = 0;

	/* Connect to the camera */
	ret = clicksmart_init (camera->port, camera->pl);
	if (ret != GP_OK) {
		free(camera->pl);
		return ret;
	};


	return GP_OK;
}
