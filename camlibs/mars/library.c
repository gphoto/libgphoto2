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

#include "mars.h"
#include <gphoto2-port.h>

#define GP_MODULE "mars"

struct _CameraPrivateLibrary {
	Model model;
	Info info[0x2000];
};

struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"Aiptek PenCam VGA+", GP_DRIVER_STATUS_EXPERIMENTAL, 0x08ca, 0x0111},
 	{"Emprex PCD3600", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010f},
	{"Vivitar Vivicam 55", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010f},
	{"Haimei Electronics HE-501A", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010e},
	{"Elta Medi@ digi-cam", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010e},
 	{NULL,0,0}
};

int
camera_id (CameraText *id)
{
    	strcpy (id->text, "Aiptek PenCam VGA+");
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
	int num_pics;
	num_pics = mars_get_num_pics(camera->pl->info);
	switch(num_pics) {
	case 1:
    	sprintf (summary->text,_("Mars MR97310 camera.\n" 
			"There is %i photo in it. \n"), num_pics);  
	break;
	default:
    	sprintf (summary->text,_("Mars MR97310 camera.\n" 
			"There are %i photos in it. \n"), num_pics);  
	}	

    	return GP_OK;
}


static int camera_manual (Camera *camera, CameraText *manual, GPContext *context) 
{
	strcpy(manual->text, 
	_(
        "This driver supports cameras with Mars MR97310 chip (and direct\n"
        "equivalents ??Pixart PACx07??) and should work with gtkam.\n"
	"The driver allows you to get\n"
	"   - thumbnails for gtkam\n"
	"   - full images (640x480 or 320x240PPM format) (??352x288 & 176x144??)\n"
	"The camera does not support deletion of photos, nor uploading\n"
	"of data. This driver does not support the optional photo\n" 
	"compression mode.\n" 
	"If present on the camera, video clip frames are downloaded \n"
	"as consecutive still photos.\n"
	)); 

	return (GP_OK);
}




static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
    	strcpy (about->text, _("Mars MR97310 camera library\n"
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
	n = mars_get_num_pics (camera->pl->info);
    	gp_list_populate (list, "mrpic%03i.ppm", n);
    	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
    	Camera *camera = user_data; 
    	int w, h = 0, b = 0, comp_ratio, k;
	int x, y, c;
    	unsigned char *data; 
    	unsigned char  *ppm;
	unsigned char *p_data = NULL;
	unsigned char gtable[256];
	char *ptr;
	int size = 0;
	float gamma_factor;

    	GP_DEBUG ("Downloading pictures!\n");

    	/* Get the number of the photo on the camera */
	k = gp_filesystem_number (camera->fs, "/", filename, context); 
    	/* Resolution must be checked for each individual picture. */
    	comp_ratio = mars_get_comp_ratio (camera->pl->info, k);
    	w = mars_get_picture_width (camera->pl->info, k);
    	switch (w) {
	case 640: h = 480; break;
	case 352: h = 288; break;
	case 320: h = 240; break;
	case 176: h = 144; break;
	default:  h = 480; break;
	}
	
	GP_DEBUG ("height is %i\n", h); 		
    
    	b = w * h / comp_ratio;
	data = malloc (b+140);
	if (!data) return GP_ERROR_NO_MEMORY;
    	memset (data, 0, b+140);

	GP_DEBUG ("buffer= %i\n", b); 

	p_data = malloc (w * h);
	if (!p_data) {free (data); return GP_ERROR_NO_MEMORY;}
	memset (p_data, 0, w * h);

    	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		mars_read_picture_data (camera, camera->pl->info, 
					    camera->port, data, b, k);
		break;

	case GP_FILE_TYPE_PREVIEW:
		mars_read_picture_data (camera, camera->pl->info,
					    camera->port, data, b, k);
		break;

	default:
		free (p_data);
		free (data);
		return GP_ERROR_NOT_SUPPORTED;
    	}

	switch (comp_ratio) {
	case 3:
		/* FIXME: This function right now does nothing in particular */
		mars_decompress (data, p_data, b, w, h);
		free (data);
		break; 	
	default:	/* "default" means comp_ratio = 1 */ 		
		    memcpy (p_data, data + 140, w*h);		 	
		    memcpy (p_data+w*h - 139, data, 140);
		    /* Is this where the misplaced junk goes? Who knows? */
		free (data);
	}

	/* Now put the data into a PPM image file. */

	ppm = malloc (w * h * 3 + 256);  /* Data + header */
	if (!ppm) {free (p_data); return GP_ERROR_NO_MEMORY;}
	memset (ppm, 0, w * h * 3 + 256);
    	sprintf (ppm,
		"P6\n"
		"# CREATOR: gphoto2, Mars library\n"
		"%d %d\n"
		"255\n", w, h);
	ptr = ppm + strlen (ppm);	
	size = strlen (ppm) + (w * h * 3);
	GP_DEBUG ("size = %i\n", size);

	gp_bayer_decode (p_data, w , h , ptr, BAYER_TILE_RGGB);
	gamma_factor = 1. - (float)mars_get_gamma(camera->pl->info, k)/256.;
	if( (gamma_factor < .3 ) ) gamma_factor = .3;
	if( (gamma_factor > .5 ) ) gamma_factor = .5;
	gp_gamma_fill_table (gtable, gamma_factor );
	gp_gamma_correct_single (gtable, ptr, w * h);

	/*
	 * gamma of .50 arrived at by experimentation. Maybe could be 
	 * improved? Well, here is a guess for improvement.
	 */
	
	if (GP_FILE_TYPE_PREVIEW==type) {
		for (y=0; y<h/8; y++) {
			for (x=0; x<w/8; x++) {
				for (c=0; c<3; c++) {
					ptr[(y*w/8+x)*3 +c] = ptr[(y*8*w+x*8)*3 +c];
				}
			}
		}
	}
	
	
	 
        gp_file_set_mime_type (file, GP_MIME_PPM);
        gp_file_set_name (file, filename); 
	gp_file_set_data_and_size (file, ppm, size);
	free (p_data);

        return GP_OK;
}

/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("Mars camera_exit");
	mars_reset(camera->port);
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
			settings.usb.inep = 0x83;
			settings.usb.outep =0x04;
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
	mars_init (camera,camera->port, &camera->pl->model, camera->pl->info);

	return GP_OK;
}
