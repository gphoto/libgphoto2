/* library.c
 *
 * Copyright (C) 2004 Theodore Kilgore <kilgota@auburn.edu>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
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

#include "mars.h"
#include <gphoto2/gphoto2-port.h>

#define GP_MODULE "mars"

struct _CameraPrivateLibrary {
	Info info[0x2000];
};

static const struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"Aiptek PenCam VGA+", GP_DRIVER_STATUS_TESTING, 0x08ca, 0x0111},
	{"Trust Spyc@m 100", GP_DRIVER_STATUS_TESTING, 0x08ca, 0x0110},
 	{"Emprex PCD3600", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010f},
	{"Vivitar Vivicam 55", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010f},
	{"Haimei Electronics HE-501A", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010e},
	{"Elta Medi@ digi-cam", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010e},
	{"Precision Mini, Model HA513A", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010f},
	{"Digital camera, CD302N", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010e},
	{"INNOVAGE Mini Digital, CD302N", GP_DRIVER_STATUS_TESTING, 0x093a, 0x010e},
	{"Argus DC-1610", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010f}, 
	{"Argus DC-1620", GP_DRIVER_STATUS_TESTING, 0x093a, 0x010f}, 
	{"Philips P44417B keychain camera", GP_DRIVER_STATUS_TESTING, 0x093a, 0x010e},
 	{"Sakar Digital no. 77379", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010f},
	{"ION digital camera", GP_DRIVER_STATUS_TESTING, 0x093a, 0x010f}, 
	{"Argus QuickClix", GP_DRIVER_STATUS_DEPRECATED, 0x093a, 0x010f},
	{"Pixart Gemini Keychain Camera", GP_DRIVER_STATUS_TESTING, 0x093a, 0x010e},
	{"Sakar Digital no. 56379 Spyshot", GP_DRIVER_STATUS_TESTING, 0x093a, 0x010e},
	{"Sakar no. 1638x CyberPix", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010f},
	{"Shift3", GP_DRIVER_STATUS_EXPERIMENTAL, 0x093a, 0x010e},
	{"Vivitar Mini Digital Camera", GP_DRIVER_STATUS_TESTING, 0x093a, 0x010e},
	{NULL,0,0,0}
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
       		a.file_operations   = GP_FILE_OPERATION_PREVIEW 
					| GP_FILE_OPERATION_RAW; 
       		gp_abilities_list_append (list, a);
    	}
    	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	int num_pics = mars_get_num_pics(camera->pl->info);

    	sprintf (summary->text,ngettext(
		"Mars MR97310 camera.\nThere is %i photo in it.\n",
    		"Mars MR97310 camera.\nThere are %i photos in it.\n",
		num_pics), num_pics
	);  
    	return GP_OK;
}

static int camera_manual (Camera *camera, CameraText *manual, GPContext *context) 
{
	strcpy(manual->text, 
	_(
        "This driver supports cameras with Mars MR97310 chip (and direct\n"
        "equivalents ??Pixart PACx07?\?).\n"
	"These cameras do not support deletion of photos, nor uploading\n"
	"of data.\n"
	"Decoding of compressed photos may or may not work well\n" 
	"and does not work equally well for all supported cameras.\n"
	"Photo data processing for Argus QuickClix is NOT SUPPORTED.\n"
	"If present on the camera, video clip frames are downloaded \n"
	"as consecutive still photos.\n"
	"For further details please consult libgphoto2/camlibs/README.\n"
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
	int i = 0, n;
	char name[16];

	n = mars_get_num_pics(camera->pl->info);
	for (i=0; i < n; i++) {
		if ((camera->pl->info[8*i]&0x0f) == 1) {
			sprintf (name, "mr%03isnd.wav", i+1);
		} else 
			sprintf (name, "mr%03ipic.ppm", i+1);
	    	gp_list_append (list, name, NULL);
    	}
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
  	int w=0, h = 0, b = 0, k;
    	unsigned char *data; 
    	unsigned char  *ppm;
	unsigned char *p_data = NULL;
	unsigned char gtable[256], photo_code, res_code, compressed;
	unsigned char audio = 0;
	unsigned char *ptr;
	int size = 0, raw_size = 0;
	float gamma_factor;

    	GP_DEBUG ("Downloading pictures!\n");

	/* These are cheap cameras. There ain't no EXIF data. */
	if (GP_FILE_TYPE_EXIF == type) return GP_ERROR_FILE_EXISTS;


    	/* Get the number of the photo on the camera */
	k = gp_filesystem_number (camera->fs, "/", filename, context); 
    	/* Determine the resolution setting from the PAT table */
	photo_code = camera->pl->info[8*k];
	res_code = photo_code & 0x0f;
	/* Compression presence or absence is seen here, and is given again 
	 * by the camera, in the header of raw data for each photo.
	 */
    	compressed = (photo_code >> 4) & 2;
	switch (res_code) {
	case 0: w = 176; h = 144; break;
	case 1: audio = 1; break;
	case 2: w = 352; h = 288; break;
	case 6: w = 320; h = 240; break;
	case 8: w = 640; h = 480; break;
	default:  w = 640; h = 480; 
	}
	
	GP_DEBUG ("height is %i\n", h); 		
    
	b = mars_get_pic_data_size(camera->pl->info, k);
	raw_size = b;
	/* Now increase b from "actual size" to _downloaded_ size. */
	b = ((b+ 0x1b0)/0x2000 + 1) * 0x2000;

	data = malloc (b);
	if (!data) return GP_ERROR_NO_MEMORY;
    	memset (data, 0, b);
	GP_DEBUG ("buffersize= %i = 0x%x butes\n", b,b); 

	mars_read_picture_data (camera, camera->pl->info, 
					    camera->port, (char *)data, b, k);
	/* The first 128 bytes are junk, so we toss them.*/
	memmove(data, data+128, b - 128);
	if (audio) {	
		p_data = malloc (raw_size+256);
		if (!p_data) {free (data); return GP_ERROR_NO_MEMORY;}
		memset (p_data, 0, raw_size+256);
    		sprintf ((char *)p_data, "RIFF");
		p_data[4] = (raw_size+36)&0xff;
		p_data[5] = ((raw_size+36)>>8)&0xff;
		p_data[6] = ((raw_size+36)>>16)&0xff;
		p_data[7] = ((raw_size+36)>>24)&0xff;
    		sprintf ((char *)p_data+8, "WAVE");\
    		sprintf ((char *)p_data+12, "fmt ");
		p_data[16] = 0x10;
		p_data[20] = 1;
		p_data[22] = 1;
		p_data[24] = 0x40;
		p_data[25] = 0x1F;
		p_data[28] = 0x40;
		p_data[29] = 0x1F;
		p_data[32] = 1;
		p_data[34] = 8;
    		sprintf ((char *)p_data+36, "data");
		p_data[40] = (raw_size)&0xff;
		p_data[41] = ((raw_size)>>8)&0xff;
		p_data[42] = ((raw_size)>>16)&0xff;
		p_data[43] = ((raw_size)>>24)&0xff;		
		memcpy (p_data+44, data, raw_size);
		gp_file_set_mime_type(file, GP_MIME_WAV);
		gp_file_set_data_and_size(file, (char *)p_data , raw_size+44);
		return GP_OK;
	}

	if (GP_FILE_TYPE_RAW == type) {

		/* We keep the SOF marker. Actual data starts at byte 12.
		 * Byte 6, upper nibble, is a code for compression mode. We 
		 * use the lower nibble to store the resolution code.
		 * Then it is possible to know "everything" from a raw file.
		 * Purpose of the info in bytes 7 thru 11 is currently unknown. 
		 * A "raw" audio file will also have the WAV header prepended.
		 * So do nothing to an audio file. 
		 */
		if (!audio) 
			data[6] = (data[6] | res_code);
		gp_file_set_mime_type(file, GP_MIME_RAW);
		gp_file_set_data_and_size(file, (char *)data , raw_size );
		return GP_OK;
	}

	p_data = malloc (w * h);
	if (!p_data) {free (data); return GP_ERROR_NO_MEMORY;}
	memset (p_data, 0, w * h);
		
	if (compressed) {
		mars_decompress (data + 12, p_data, w, h);
	}
	else memcpy (p_data, data + 12, w*h);
	gamma_factor = sqrt((float)data[7]/100.); 
	if (gamma_factor <= .60)
		gamma_factor = .60;
	free(data);

	/* Now put the data into a PPM image file. */

	ppm = malloc (w * h * 3 + 256);  /* Data + header */
	if (!ppm) {free (p_data); return GP_ERROR_NO_MEMORY;}
	memset (ppm, 0, w * h * 3 + 256);
    	sprintf ((char *)ppm,
		"P6\n"
		"# CREATOR: gphoto2, Mars library\n"
		"%d %d\n"
		"255\n", w, h);
	ptr = ppm + strlen ((char *)ppm);	
	size = strlen ((char *)ppm) + (w * h * 3);
	GP_DEBUG ("size = %i\n", size);
	gp_ahd_decode (p_data, w , h , ptr, BAYER_TILE_RGGB);
	gp_gamma_fill_table (gtable, gamma_factor );
	gp_gamma_correct_single (gtable, ptr, w * h);
	mars_white_balance (ptr, w*h, 1.4, gamma_factor);
        gp_file_set_mime_type (file, GP_MIME_PPM);
	gp_file_set_data_and_size (file, (char *)ppm, size);
	free (p_data);

        return GP_OK;
}

/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("Mars camera_exit");
	mars_reset(camera->port);
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
		/*	settings.usb.inep = 0x82;	*/
		/* inep 0x83 used for commands. Switch to 0x82 for data! */
			settings.usb.inep = 0x83;
			settings.usb.outep = 0x04;
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
	mars_init (camera,camera->port, camera->pl->info);

	return GP_OK;
}
