/* library.c
 *
 * Copyright (C) 2003 Theodore Kilgore <kilgota@auburn.edu>
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

#define _BSD_SOURCE

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <bayer.h>
#include <gamma.h>

#include <gphoto2/gphoto2.h>

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

#include "aox.h"
#include <gphoto2/gphoto2-port.h>

#define GP_MODULE "aox"

struct _CameraPrivateLibrary {
	Model model;
	Info info[2];
};

static struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"Concord EyeQMini_1", GP_DRIVER_STATUS_EXPERIMENTAL, 0x03e8, 0x2182},
        {"Concord EyeQMini_2", GP_DRIVER_STATUS_EXPERIMENTAL, 0x03e8, 0x2180},
        {"D-MAX DM3588", GP_DRIVER_STATUS_EXPERIMENTAL, 0x03e8, 0x2130},        
	{NULL,0,0,0}
};

int
camera_id (CameraText *id)
{
    	strcpy (id->text, "Aox chipset camera");
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
       		a.folder_operations = GP_FOLDER_OPERATION_NONE;
       		a.file_operations   = GP_FILE_OPERATION_PREVIEW; 
       		gp_abilities_list_append (list, a);
    	}
    	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{

	int num_lo_pics =aox_get_num_lo_pics(camera->pl->info);
	int num_hi_pics =aox_get_num_hi_pics(camera->pl->info);	

    	sprintf (summary->text,_("Your USB camera has an Aox chipset.\n" 
			"Number of lo-res PICs = %i\n"
			"Number of hi-res PICs = %i\n"	
			"Number of PICs = %i\n"
       			), num_lo_pics, num_hi_pics, num_lo_pics+num_hi_pics);  

    	return GP_OK;
}


static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
    	strcpy (about->text, _("Aox generic driver\n"
			    "Theodore Kilgore <kilgota@auburn.edu>\n"));
    	return GP_OK;
}

/*************** File and Downloading Functions *******************/

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
        Camera *camera = data; 
	int num_lo_pics = aox_get_num_lo_pics (camera->pl->info);
	int num_hi_pics = aox_get_num_hi_pics (camera->pl->info);
	int n = num_hi_pics + num_lo_pics;
	char name[20];
	int i;	
	/* Low-resolution pictures are always downloaded first. We do not know 
	 * yet how to process them, so they will remain in RAW format. */
	
	for (i=0; i< num_lo_pics; i++){
		snprintf( name, sizeof(name), "aox_pic%03i.raw", i+1 );
		gp_list_append(list, name, NULL);	
	}

	for (i = num_lo_pics; i < n; i++){
		snprintf( name, sizeof(name), "aox_pic%03i.ppm", i+1 );		
		gp_list_append(list, name, NULL);	
	}
    	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
    	Camera *camera = user_data; 

/* The camera will always download the low-resolution pictures first, if any.
 * As those are compressed, they are not of fixed size. Unfortunately, the 
 * compression method is not known. 
 * For a high-resolution picture, the size is always the same. 
 * Every picture file has a header, which is of length 0x98. The high-
 * resolution pictures are just Bayer data; their headers will be discarded.  
 */

        int i, j, k, n, num_lo_pics, num_hi_pics, w = 0, h = 0;  	
	unsigned char temp;
	unsigned char *data;
	unsigned char *p_data = NULL;
	unsigned char *output = NULL;     
	int len;
	int header_len;
	char header[128];	
	unsigned char gtable[256];

	k = gp_filesystem_number(camera->fs, "/", filename, context);


	num_lo_pics = aox_get_num_lo_pics(camera->pl->info);
	num_hi_pics = aox_get_num_hi_pics(camera->pl->info);


	GP_DEBUG("There are %i compressed photos\n", num_lo_pics);
	GP_DEBUG("There are %i hi-res photos\n", num_hi_pics);

	if ( (k < num_lo_pics) ) { 
		n = k; 
		w = 320;
		h = 240;
	} else {
		n = k - num_lo_pics;
		w = 640;	
		h = 480;
	}

	len = aox_get_picture_size (camera->port, num_lo_pics, 
						num_hi_pics, n, k);
	GP_DEBUG("len = %i\n", len);
	data = malloc(len);
	if (!data) {
		printf("Malloc failed\n"); return 0;}
    	aox_read_picture_data (camera->port, (char *)data, len, n);

	switch (type) {
	case GP_FILE_TYPE_EXIF:
		free (data);
		return (GP_ERROR_FILE_EXISTS);

	case GP_FILE_TYPE_PREVIEW:
	case GP_FILE_TYPE_NORMAL:
		if ((w == 320)) {
			gp_file_detect_mime_type (file); /* Detected as "raw"*/
			gp_file_set_data_and_size (file, (char *)data, len);
			gp_file_adjust_name_for_mime_type (file);
			break;
		}
		if ((w == 640)){
			/* Stripping useless header */
			p_data = data + 0x98;
			/* Picture is mirror-imaged.*/
    			for (i = 0; i < h; ++i) {
				for (j = 0 ; j < w/2; j++) { 
        				temp = p_data[w*i +j];
        				p_data[w*i +j] = p_data[w*i+ w -1 -j];
        				p_data[w*i + w  - 1 - j] = temp;
				}
    			}    	
			/* Not only this, but some columns are 
			 * interchanged, too. */
			for (i = 0; i < w*h/4; i++) {
				temp = p_data[4*i +1];
				p_data[4*i + 1] = p_data[4*i+2];
				p_data[4*i+2] = temp;
			}
			/* And now create a ppm file, with our own header */
			header_len = snprintf(header, 127, 
				"P6\n" 
				"# CREATOR: gphoto2, aox library\n" 
				"%d %d\n" 
				"255\n", w, h);

			output = malloc(3*w*h);
			if(!output) {
				free(output);
				return GP_ERROR_NO_MEMORY;
			}	
			if (camera->pl->model == AOX_MODEL_DMAX)
			    gp_bayer_decode (p_data, w, h, 
					output, BAYER_TILE_RGGB);
			else
			    gp_bayer_decode (p_data, w, h, 
					output, BAYER_TILE_GRBG);
			/* gamma correction of .70 may not be optimal. */
			gp_gamma_fill_table (gtable, .65);
			gp_gamma_correct_single (gtable, output, w * h);
    			gp_file_set_mime_type (file, GP_MIME_PPM);
    			gp_file_append (file, header, header_len);
			gp_file_append (file, (char *)output, 3*w*h);
		}
		free (data);
		free (output);
		return GP_OK;
	case GP_FILE_TYPE_RAW:
		gp_file_set_data_and_size (file, (char *)data, len);
		gp_file_set_mime_type (file, GP_MIME_RAW);
		gp_file_adjust_name_for_mime_type(file);
		break;
	default:
		free (data);
		return (GP_ERROR_NOT_SUPPORTED); 	
	}
	return GP_OK;
}

/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("Aox camera_exit");

	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func
}; 

int
camera_init(Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	CameraAbilities abilities;
	int ret = 0;

	/* First, set up all the function pointers */
	camera->functions->summary      = camera_summary;
	camera->functions->about        = camera_about;
	camera->functions->exit	    = camera_exit;

	GP_DEBUG ("Initializing the camera\n");
	ret = gp_port_get_settings(camera->port,&settings);
	if (ret < 0) return ret; 

        ret = gp_camera_get_abilities(camera,&abilities);                       
        if (ret < 0) return ret;                                                
        GP_DEBUG("product number is 0x%x\n", abilities.usb_product);

	switch (camera->port->type) {
		case GP_PORT_SERIAL:
			return ( GP_ERROR );
		case GP_PORT_USB:
			settings.usb.config = 1;
			settings.usb.altsetting = 0;
			settings.usb.interface = 1;
			settings.usb.inep = 0x84;
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
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));

	switch(abilities.usb_product) {
	case 0x2130:
		camera->pl->model = AOX_MODEL_DMAX;
		break;
	default:
		camera->pl->model = AOX_MODEL_MINI;
	}
	/* Connect to the camera */
	aox_init (camera->port, &camera->pl->model, camera->pl->info);

	return GP_OK;
}
