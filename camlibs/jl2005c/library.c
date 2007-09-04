/* library.c
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

#include "jl2005c.h"
#include <gphoto2/gphoto2-port.h>

#define GP_MODULE "jl2005c"

struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"Argus DC1512e", GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
/*        {"American Idol", GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0224},*/
	{NULL,0,0,0}
};

int
camera_id (CameraText *id)
{
    	strcpy (id->text, "Argus DC1512e");
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
       		a.file_operations   = GP_FILE_OPERATION_PREVIEW 
					+ GP_FILE_OPERATION_RAW; 
       		gp_abilities_list_append (list, a);
    	}
    	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	int num_pics;
	num_pics = camera->pl->nb_entries;
	GP_DEBUG("camera->pl->nb_entries = %i\n",camera->pl->nb_entries);
    	sprintf (summary->text,_("This camera contains a Jeilin JL2005C chipset.\n" 
			"The number of photos in it is %i. \n"), num_pics);  
    	return GP_OK;
}


static int camera_manual (Camera *camera, CameraText *manual, GPContext *context) 
{
	strcpy(manual->text, 
	_(
        "This driver supports cameras with Jeilin jl3005c chip \n"
	"These cameras do not support deletion of photos, nor uploading\n"
	"of data. \n"
	"Decoding of compressed photos may or may not work well\n" 
	"and does not work equally well for all supported cameras.\n"
	"Photo data processing for Argus QuickClix is NOT SUPPORTED.\n"
	"If present on the camera, video clip frames are downloaded \n"
	"as consecutive still photos.\n"
	"For further details please consult libgphoto2/camlibs/README.jl2005c\n"
	)); 

	return (GP_OK);
}


static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
    	strcpy (about->text, _("jl2005c camera library\n"
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
	n = camera->pl->nb_entries;
    	gp_list_populate (list, "jl_%03i.ppm", n);
    	return GP_OK;
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	info->file.fields = GP_FILE_INFO_TYPE;
	strcpy (info->file.type, GP_MIME_PPM);

	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
    	Camera *camera = user_data; 
  	int w, h = 0, b = 0, k;
    	unsigned char *pic_data; 
	int res_code;
	unsigned char photo_code, compressed;
	unsigned long start_of_photo;
	unsigned int blocksize = 0;
	int filled = 0;

    	GP_DEBUG ("Downloading pictures!\n");
	/* These are cheap cameras. There ain't no EXIF data. */
	if (GP_FILE_TYPE_EXIF == type) return GP_ERROR_FILE_EXISTS;


    	/* Get the number of the photo on the camera */
	k = gp_filesystem_number (camera->fs, "/", filename, context); 
    	/* Determine the resolution setting from the PAT table */
	photo_code = camera->pl->info[48+16*k+2];
	res_code = photo_code & 0x0f;
	/* Compression presence or absence is seen here, and is given again 
	 * by the camera, in the header of raw data for each photo.
	 */
    	compressed = (photo_code >> 4) & 0x0f;
	switch (res_code) {
	case 0: 
		w = 640; 
		h = 480; 
		break;
	case 1: 
		w = 320;
		h = 240; 
		break;
	default:  
		GP_DEBUG("width is unknown\n");
		return GP_ERROR; 
	}
	
	GP_DEBUG ("height is %i\n", h); 		
    
	b = jl2005c_get_pic_data_size(camera->pl->info, k);
	GP_DEBUG("b = %i = 0x%x bytes\n", b,b);
	start_of_photo = jl2005c_get_start_of_photo(camera->pl->info, k);	
	GP_DEBUG("start_of_photo number %i = 0x%lx \n", k,start_of_photo);
	pic_data = malloc (b+1);
	if (!pic_data) return GP_ERROR_NO_MEMORY;
    	memset (pic_data, 0, b+1);
	GP_DEBUG ("buffersize b = %i = 0x%x bytes\n", b+1,b+1); 
	pic_data[b] = photo_code;
	GP_DEBUG("photo_code = %02x and pic_data[b+1] = %02x\n", photo_code, pic_data[b]);

	/* Camera downloads in blocks of 0xfa00, with last block possibly short. 
	 * So first we set up the permanent cache (if not done already).
	 */
	
	if (! camera->pl->data_cache ) {
		camera->pl->data_cache = malloc (0xfa00);
	}
	if (! camera->pl->data_cache ) {
		GP_DEBUG ("no cache memory allocated!\n");
		return GP_ERROR_NO_MEMORY;
	}
	
	/* Is there data in the cache, or not? If yes, read from it into the 
	 * current photo, immediately. Update settings. But first a sanity check. 
	 */

	if (start_of_photo != camera->pl->bytes_put_away) {
		GP_DEBUG("photo number %i starts in a funny place!\n",k);
		return (GP_ERROR);
	}
	/* OK, good. */

	if (camera->pl->bytes_read_from_camera > start_of_photo) {
		if(start_of_photo + b <= camera->pl->bytes_read_from_camera) {
			memcpy(pic_data, camera->pl->data_cache
			    + (start_of_photo%0xfa00)
			    , b);
			camera->pl->bytes_put_away += b;
			/* Photo data is contained in what is already downloaded. 
			 * Jump immediately to process the photo. 
		         */
		} else {

			filled = camera->pl->bytes_read_from_camera 
				    - start_of_photo;

			memcpy(pic_data, camera->pl->data_cache
				    + (start_of_photo%0xfa00), 
				    filled);

			camera->pl->bytes_put_away += filled;
		}
	}	
	while (camera->pl->bytes_put_away < start_of_photo + b ) {

		camera->pl->data_to_read = camera->pl->total_data_in_camera
			    - camera->pl->bytes_read_from_camera;
		blocksize = 0xfa00;
		if (camera->pl->data_to_read < blocksize)
			blocksize = camera->pl->data_to_read; 
		GP_DEBUG("blocksize = 0x%x\n", blocksize);
		if(blocksize)
			jl2005c_get_picture_data (
					    camera->port, 
					    (char *) camera->pl->data_cache, 
					    blocksize);
		camera->pl->bytes_read_from_camera += blocksize;
		if (camera->pl->bytes_read_from_camera >= start_of_photo + b ) {
			GP_DEBUG("THIS ONE?\n");
			memcpy(pic_data+filled, camera->pl->data_cache, 
						b - filled);
			camera->pl->bytes_put_away += b - filled;
			break;
		} else {	
			GP_DEBUG("THIS ONE??\n");		    
			if (!blocksize)
				break;
			memcpy(pic_data+filled, camera->pl->data_cache, blocksize);
			camera->pl->bytes_put_away += blocksize;
			filled += blocksize;
		}
	}



	if (GP_FILE_TYPE_RAW == type) {
		memcpy (pic_data+b, &photo_code,1);
		GP_DEBUG("photo_code = %02x and pic_data[b+1] = %02x\n", 
						photo_code, pic_data[b]);
		gp_file_set_mime_type(file, GP_MIME_RAW);
		gp_file_set_name(file, filename);
		gp_file_set_data_and_size(file, (char *)pic_data , b+1 );
		return GP_OK;
	} else return GP_ERROR_NOT_SUPPORTED;
	
	

        return GP_OK;
}






/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("jl2005c camera_exit");
	jl2005c_reset(camera, camera->port);
	gp_port_close(camera->port);
	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}
	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func
};

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
		/* inep 0x84 used to initialize. Then switch to 0x82! */
			settings.usb.inep = 0x84;
			settings.usb.outep = 0x03;
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
	/* Connect to the camera */
	camera->pl->bytes_read_from_camera = 0;
	camera->pl->total_data_in_camera=0;
	camera->pl->data_to_read = 0;
	camera->pl->data_used_from_block = 0;
	camera->pl->bytes_put_away = 0;
	camera->pl->data_cache = NULL;
	jl2005c_init (camera,camera->port, camera->pl);

	return GP_OK;
}
