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
#  define ngettext(String1,String2,Count) ((Count==1)?String1:String2)
#endif

#include <gphoto2/gphoto2-port.h>
#include "sonix.h"
#include "sakar-avi-header.h"
#define GP_MODULE "sonix"

static const struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"DC31VC", GP_DRIVER_STATUS_EXPERIMENTAL, 0x0c45, 0x8000},	
        {"Sakar Digital Keychain 11199", GP_DRIVER_STATUS_EXPERIMENTAL, 
	    0x0c45, 0x8003},
        {"Mini Shotz ms-350", GP_DRIVER_STATUS_EXPERIMENTAL, 0x0c45, 0x8008},
        {"Vivitar Vivicam3350B", GP_DRIVER_STATUS_EXPERIMENTAL, 0x0c45, 0x800a},
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
       			a.file_operations   = GP_FILE_OPERATION_DELETE|
					GP_FILE_OPERATION_PREVIEW;
       			gp_abilities_list_append (list, a);
    	}
    	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	int ret = 0;
	if(!camera->pl->sonix_init_done) 
		ret = sonix_init(camera->port, camera->pl);
	if ( ret != GP_OK) {
		free(camera->pl);
		return ret;
	}	
	if (!camera->pl->num_pics) 
		sonix_exit(camera->port);
        sprintf (summary->text,ngettext(
		"Sonix camera.\nThere is %i photo in it.\n",
		"Sonix camera.\nThere are %i photos in it.\n",
		camera->pl->num_pics
	), camera->pl->num_pics);
    	return GP_OK;
}

static int camera_manual (Camera *camera, CameraText *manual, GPContext *context) 
{
	strcpy(manual->text, 
	_(
	"This driver supports some cameras that use the Sonix sn9c2028 chip.\n"
	"The following operations are supported:\n"
	"   - thumbnails for a GUI frontend\n"
	"   - full images in PPM format\n"
	"   - delete all images\n"
	"   - delete last image (may not work on all Sonix cameras)\n"
	"   - image capture to camera (		ditto		)\n"
	"If present on the camera, video clip frames are downloaded \n"
	"as consecutive still photos or as AVI files, depending on the model.\n"
	"Thumbnails for AVIs are still photos made from the first frame.\n" 
	"A single image cannot be deleted unless it is the last one.\n"
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
	int i = 0;
	char name[16];
	int(avitype);
	int ret = 0;
	if(!camera->pl->sonix_init_done) 
		ret = sonix_init(camera->port, camera->pl);
	if ( ret != GP_OK) {
		free(camera->pl);
		return ret;
	}	
	if(!camera->pl->num_pics) {
		sonix_exit(camera->port);
		return GP_OK;
	}
	for (i=0; i<camera->pl->num_pics; i++) {
		avitype = (camera->pl->size_code[i])&8;
		if (avitype) {
			sprintf (name, "sonix%03i.avi", i+1);
			avitype = 0;
		} else 
			sprintf (name, "sonix%03i.ppm", i+1);
	    	gp_list_append (list, name, NULL);
    	}


	return GP_OK;}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
    	Camera *camera = user_data; 
    	int w, h = 0, buffersize = 0, i, k, rawsize = 0;
    	unsigned char *data = NULL; 
    	unsigned char  *ppm, *ptr, *avi=NULL, *frame_data = NULL;
	unsigned char *p_data = NULL, *p_buf = NULL;
	unsigned char gtable[256];
	unsigned int numframes = 0;
	unsigned int endpoint, bytes_used = 0;
	unsigned int framedatasize[1024];
	unsigned int framestart[1024];	
	unsigned int size = 0, frame_size = 0;
	unsigned int avitype = 0;
	unsigned int offset=0;
	unsigned int cam_offset;	
	char name[16];
	int ret = 0;
	
    	GP_DEBUG ("Downloading pictures!\n");
	if(!camera->pl->sonix_init_done) 
		ret = sonix_init(camera->port, camera->pl);
	if ( ret != GP_OK) {
		free(camera->pl);
		return ret;
	}	

	if(!camera->pl->num_pics) {
		sonix_exit(camera->port);
		return GP_OK;
	}	

	cam_offset = camera->pl->offset;

    	/* Get the number of the photo on the camera */
	k = gp_filesystem_number (camera->fs, "/", filename, context); 
	if (k < GP_OK) return k;

	if(type == GP_FILE_TYPE_EXIF) return GP_ERROR_FILE_EXISTS;

	i = camera->pl->size_code[k];
    	switch (i) { 
	case 0: w=352; h = 288; break; 	/* always uncompressed? */
	case 9: avitype = 1;		/* multiframe, always uncompressed? */
	case 1: w=176; h = 144; break;	/* always uncompressed? */
	case 0x0a: avitype = 1;		/* multiframe, always compressed? */	
	case 2: w=640; h = 480; break;	/* always compressed?   */
	case 0x0b: avitype = 1;		/* multiframe, always compressed? */
	case 3: w=320; h = 240; break;	/* always compressed?   */
	default:  GP_DEBUG ("Size code unknown\n"); 
		
	    return (GP_ERROR_NOT_SUPPORTED);
	}	    

	GP_DEBUG( "avitype is %d\n", avitype);
	GP_DEBUG ("height of picture %i is %i\n", k+1,h); 		
	rawsize = sonix_read_data_size (camera->port, k);
	GP_DEBUG("rawsize = 0x%x = %i\n", rawsize, rawsize);
	if(rawsize%0x40) 
		buffersize = rawsize - (rawsize%0x40) + 0x40;
	else 
		buffersize = rawsize;	
	data = calloc (buffersize+64, 1);
	if (!data) return GP_ERROR_NO_MEMORY;
	gp_port_read(camera->port, (char *)data, buffersize);

	switch(type) {
	case GP_FILE_TYPE_NORMAL:
		/* AVI files need separate treatment */
		if (avitype) {
			frame_size = w*h;
			p_data = data;
			/* count end of frame markers to know the number of 
			 *frames and the size and starting offset of each */
			endpoint = 0;
			numframes = 0;
			framestart[0] = 0;
			p_buf = data;
			while (p_buf < data + buffersize+64) {
				endpoint = 12;
				while ( !((p_buf[endpoint-12]==0xff) &&
						(p_buf[endpoint-11]==0xff) &&
						(p_buf[endpoint-10]==0x00) &&
						(p_buf[endpoint-9]==0xc4) &&
						(p_buf[endpoint-8]==0xc4) ) &&
						(endpoint < buffersize +64 
							    - bytes_used)) 
					endpoint ++ ;
				if ( !((p_buf[endpoint-12]==0xff) &&
						(p_buf[endpoint-11]==0xff) &&
						(p_buf[endpoint-10]==0x00) &&
						(p_buf[endpoint-9]==0xc4) &&
						(p_buf[endpoint-8]==0xc4) ) ) {
					GP_DEBUG("Finished counting frames!\n");
					break;    
				}
				framedatasize[numframes] = endpoint;
				framestart[numframes+1] = 
					framestart[numframes] + endpoint;
				p_buf += endpoint;		
				bytes_used += endpoint;
				numframes ++;
			}
			GP_DEBUG("number of frames is %i\n",numframes);
			frame_data = malloc(frame_size);
			if (!frame_data) {
				free (frame_data);
				return -1;
			}
			size = (numframes)*(3*frame_size+ 8) + 224;
			GP_DEBUG("size = %i\n", size);
			avi = malloc(size);
			if (!avi) {free(avi); return -1;}
			memset(avi, 0, size);

			memcpy (avi,SakarAviHeader,224);
			avi[0x04] = (size -4)&0xff;
			avi[0x05] = ((size -4)&0xff00) >> 8;
			avi[0x06] = ((size -4)&0xff0000) >> 16;
			avi[0x07] = ((size -4)&0xff000000) >> 24;
			avi[0x40] = w&0xff;
			avi[0x41] = (w>>8)&0xff;	
			avi[0x44] = h&0xff;
			avi[0x45] = (h>>8)&0xff;	
			avi[0xb0] = w&0xff;
			avi[0xb1] = (w>>8)&0xff;
			avi[0xb4] = h&0xff;
			avi[0xb5] = (h>>8)&0xff;	
			avi[0x30] = numframes&0xff;
			avi[0x8c] = numframes&0xff;
			ptr=avi;
			ptr += 224;

			for (i=0; i < numframes; i++) {
				memcpy(ptr,SakarAviFrameHeader,8);
				ptr[4] = (3*frame_size)&0xff;
				ptr[5] = ((3*frame_size)&0xff00) >> 8;
				ptr[6] = ((3*frame_size)&0xff0000) >> 16;
				ptr +=8;
				GP_DEBUG("Doing frame number %i\n",i+1);
				memset (frame_data, 0, frame_size);
				offset = 0x40*(framestart[i]/0x40)+0x40;
				if (offset == framestart[i] + 0x40)
					offset -= 0x40;
				GP_DEBUG("framestart[%i] = 0x%x\n", i, 
						framestart[i]);
				GP_DEBUG("offset = 0x%x\n", offset);

				switch (camera->pl->post) {
				case DECOMP|REVERSE:
					sonix_decode (frame_data, 
					data+offset+cam_offset, w, h); 
					sonix_cols_reverse(frame_data, w, h);
					gp_bayer_decode(frame_data, w, h, ptr, 
							    BAYER_TILE_RGGB);
					break;
				case DECOMP:
					sonix_decode (frame_data, 
					data+offset+cam_offset, w, h); 
					sonix_rows_reverse(frame_data, w, h);
					gp_bayer_decode(frame_data, w, h, ptr, 
							    BAYER_TILE_GRBG);
					break;

				default:	 
					memcpy(frame_data, data +offset+cam_offset, 
							frame_size);
					sonix_rows_reverse(frame_data, w, h);
					gp_bayer_decode(frame_data, w, h, ptr, 
							    BAYER_TILE_GRBG);
				}	
				gp_gamma_fill_table(gtable, .65);
				gp_gamma_correct_single(gtable, ptr, w * h);
				p_data += framedatasize[i] + 0x40;
				ptr += 3*frame_size;
			}
			gp_file_set_mime_type (file, GP_MIME_AVI);
			gp_file_set_name (file, name); 
			gp_file_set_data_and_size (file, (char *)avi, size);
		
			avitype = 0;
			free(data);		
			return GP_OK;		
		} 
		
		
	case GP_FILE_TYPE_PREVIEW:
		if (k == camera->pl->num_pics - 1) {
			camera->pl->sonix_init_done = 1;
			sonix_exit(camera->port);
		}
		/* For an AVI file, we make a PPM thumbnail of the 
		 * first frame. Otherwise, still photos are processed 
		 * here, and also the thumbnails for them. 
		 */

		p_data = malloc (w * h);
		if (!p_data) {free (data); return GP_ERROR_NO_MEMORY;}
		memset (p_data, 0, w * h);


		ppm = malloc (w * h * 3 + 256); /* room for data + header */
		if (!ppm) {
			free (data);
			free (p_data);
			return GP_ERROR_NO_MEMORY;
		}
    		sprintf ((char *)ppm,
			"P6\n"
			"# CREATOR: gphoto2, SONIX library\n"
			"%d %d\n"
			"255\n", w, h);
		ptr = ppm + strlen ((char *)ppm);
		size = strlen ((char *)ppm) + (w * h * 3);

		switch (camera->pl->post)
		case DECOMP|REVERSE:
			/* Images for Vivicam 3350b are upside down. */
			if (camera->pl->post&REVERSE) {
			sonix_decode (p_data, data+offset+cam_offset, w, h);
			sonix_byte_reverse(p_data, w*h);
			gp_bayer_decode (p_data, w , h , ptr, 
						    BAYER_TILE_BGGR);
			break; 
		case DECOMP:
			sonix_decode (p_data, data+offset, w, h);
			gp_bayer_decode (p_data, w , h , ptr, 
						    BAYER_TILE_RGGB);
			break;
		default:
			memcpy (p_data, data+offset, w*h);
			gp_bayer_decode (p_data, w , h , ptr, BAYER_TILE_RGGB);
	
		}
	
		gp_gamma_fill_table(gtable, .65);
		gp_gamma_correct_single(gtable, ptr, w * h);
		gp_file_set_mime_type (file, GP_MIME_PPM);
    		gp_file_set_name (file, filename); 
		gp_file_set_data_and_size (file, (char *)ppm, size);

		free (p_data);
		free (data);
		
	    	return GP_OK;

	case GP_FILE_TYPE_RAW: 
		gp_file_set_mime_type(file, GP_MIME_RAW);
		gp_file_set_name(file, filename);
		gp_file_append( file, (char *)data, rawsize);
		free(data);
		GP_DEBUG("rawsize= 0x%x = %i\n", rawsize, rawsize);
		return GP_OK;

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
	int ret = 0;
	if(!camera->pl->sonix_init_done) 
		ret = sonix_init(camera->port, camera->pl);
	if ( ret != GP_OK) {
		free(camera->pl);
		return ret;
	}	

	sonix_delete_all_pics (camera->port);
	return GP_OK;
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int k;
	int ret = 0;
	if(!camera->pl->sonix_init_done) 
		ret = sonix_init(camera->port, camera->pl);
	if ( ret != GP_OK) {
		free(camera->pl);
		return ret;
	}	
	if ((camera->pl->fwversion == 0x0a))
		return GP_ERROR_NOT_SUPPORTED;
	k = gp_filesystem_number (camera->fs, "/", filename, context);     
	if (k+1 != camera->pl->num_pics)
		return GP_ERROR_NOT_SUPPORTED;
	else 	
	    sonix_delete_last (camera->port);
	    camera->pl->num_pics -= 1;
	return GP_OK;
}


static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	int ret = 0;
	if(!camera->pl->sonix_init_done) 
		ret = sonix_init(camera->port, camera->pl);
	if ( ret != GP_OK) {
		free(camera->pl);
		return ret;
	}	

	if ((camera->pl->fwversion == 0x0a))
		return GP_ERROR_NOT_SUPPORTED;
	if (camera->pl->full)
		return GP_ERROR_NO_MEMORY;
	sonix_capture_image(camera->port);	
	return GP_OK;
}


/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("Sonix camera_exit");
	sonix_exit(camera->port);
	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}
	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = NULL,
	.get_info_func = NULL,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.put_file_func = NULL, 
	.delete_all_func = delete_all_func,
};

int
camera_init(Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	CameraAbilities abilities;
	int ret = 0;

	ret = gp_camera_get_abilities(camera,&abilities);
	if (ret < 0) return ret;	
	GP_DEBUG("product number is 0x%x\n", abilities.usb_product);


	/* First, set up all the function pointers */
        camera->functions->capture              = camera_capture;
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

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
	camera->pl->num_pics = 0;
	camera->pl->full = 1;
	camera->pl->avitype = 0;
	camera->pl->sonix_init_done = 0;
	camera->pl->post = 0;
	switch (abilities.usb_product) {
		case 0x8003:
			camera->pl->post = 0;
			camera->pl->offset = 8;
			break; 
		case 0x8008:
			camera->pl->post = DECOMP;
			camera->pl->offset = 0;
			break;
		case 0x8000:	/* Please report if inaccurate ! */
		case 0x800a:
			camera->pl->post = DECOMP|REVERSE;
			camera->pl->offset = 8;
			break;
		default:
			return(GP_ERROR);
	}
	GP_DEBUG("post code is 0x%x\n", camera->pl->post);
	/* Connect to the camera only if something is actually being done,
	 * because otherwise the Sakar Digital Keychain camera is put into 
	 * some kind of active mode when gtkam is run, and will not leave 
	 * this mode even after gtkam is exited. 
	 */
	
	return GP_OK;
}
