/* agfa-cl20.c
 *
 * Copyright (C) 2002-2003 by Philipp Poeml and Dennis Noordsij,
 *                            http://cl20.poeml.de/
 *
 * 16-Mar-2004 - Dennis - Modified as follows:
 * 	- fix compactflash detection
 * 	- fix number of picturs detection
 * 	- get rid of the heavy stack usage and use the heap instead
 * 	- fix USB download size
 *
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>

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

#define GP_MODULE

static const struct {
	char *model;
	int usb_vendor;
	int usb_product;
} models[] = {
	{ "Agfa ePhoto CL20", 0x06bd, 0x0404 },
	{ NULL, 0, 0 }
};




/* Convert from 123 to 0x123 (dec:123 to hex:123
 * Limited to 4 digits, i.e. max 9999 to 0x9999
 */

static unsigned short to_camera(unsigned short a)
{
	unsigned short ret = 0;

	ret = ret + (a / 1000) * 0x1000;
	a = a % 1000;
	ret = ret + ((a / 100) * 0x100);
	a = a % 100;
	ret = ret + ((a / 10) * 0x10);
	a = a % 10;
	ret = ret + a;

	return ret;
}


/* Convert from 0x123 to 123 (hex:123 to dec:123
 * Limited to 4 digits, i.e. max 0x9999 to 9999
 */

static unsigned short from_camera(unsigned short a)
{
	unsigned short ret = 0;

	ret = ret + ((a / 0x1000) * 1000);
	a = a % 0x1000;
	ret = ret + ((a / 0x100) * 100);
	a = a % 0x100;
	ret = ret + ((a / 0x10) * 10);
	a = a % 0x10;
	ret = ret + a;

	return ret;
}

int
camera_id (CameraText *id)
{
	strcpy(id->text, "agfa_cl20");

	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;
	char *ptr;
	int   x = 0;

	/* GP_DEBUG(" * camera_abilities()"); */

	ptr = models[x].model;
	while (ptr) {
		memset(&a, 0, sizeof(a));
		strcpy(a.model, ptr);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port   = GP_PORT_USB;
	/*	a.speed[0] = 0;	*/
		a.operations        = 	GP_OPERATION_NONE;
		a.file_operations   = 	GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW;
		a.folder_operations = 	GP_FOLDER_OPERATION_DELETE_ALL;

		a.usb_vendor = models[x].usb_vendor;
		a.usb_product = models[x].usb_product;

		gp_abilities_list_append(list, a);

		ptr = models[++x].model;
	}

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG(" * camera_exit()");
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *privdata,
	       GPContext *context)
{
	Camera *camera = privdata;
	int n = -1;
	unsigned int size = 0;
	unsigned char hb, lb;
	unsigned long j;
	unsigned int app1len = -1;
	unsigned char resolution;
	unsigned char indata[ 0x100 ];

	unsigned char * result;
	unsigned char * ptr;

	/* unsigned char last, next; */

	GP_DEBUG(" * get_file_func()");

	/*
	 * Get the file from the camera. Use gp_file_set_mime_type,
	 * gp_file_set_data_and_size, etc.
	 */

	n = gp_filesystem_number(camera->fs, folder, filename, context) + 1;

	switch(type) {
	case GP_FILE_TYPE_PREVIEW:
		GP_DEBUG(" * REQUEST FOR A PREVIEW");

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x0008,NULL,0x0);
		gp_port_read(camera->port, (char*)indata, 0x100);

		size = indata[ 5 ] + (indata[ 6 ] * 0xFF) + 3;

		resolution = (unsigned char)indata[ 17 ];
		if (resolution == 1) {
			char dummy;

			result = calloc((size + 1), 0x100);
			ptr = result;

			gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&dummy,0x0001);
			gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&dummy,0x0001);

			gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x000A,NULL,0x0);

			for (j = 0; j < size; j++) {
		   		gp_port_read(camera->port, (char*)ptr, 0x100);
				ptr += 0x100;
			}

			gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&dummy,0x0001);
			gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&dummy,0x0001);

			size = size * 0x100;

			lb = (unsigned char)*(result + 0x05);
			hb = (unsigned char)*(result + 0x04);
			app1len = (unsigned int)(hb * 256) + (unsigned int)(lb);
			if ((app1len < 4) || (app1len > size - 4 - 20)) {
				free (result);
				GP_DEBUG("app1len %d is larger than size %d", app1len, size);
				return GP_ERROR_CORRUPTED_DATA;
			}

			result[3] = 0xe0;
			result[4] = 0x00;
			result[5] = 0x10;
			result[6] = 'J';
			result[7] = 'F';
			result[8] = 'I';
			result[9] = 'F';
			result[10] = 0x00;
			result[11] = 0x01;
			result[12] = 0x01;
			result[13] = 0x00;
			result[14] = 0x00;
			result[15] = 0x01;
			result[16] = 0x00;
			result[17] = 0x01;
			result[18] = 0x00;
			result[19] = 0x00;

			memmove(&result[20],
			       &result[app1len + 4],
			       (unsigned int)(size - app1len - 4));

			size = size - app1len + 20 + 4;

			gp_file_set_mime_type(file, GP_MIME_JPEG);
			gp_file_append(file, (char*)result, size);

			free(result);

			break;

		} else {
			unsigned char * data;
			char dummy;

			gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&dummy,0x0001);
			gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&dummy,0x0001);

			data = (unsigned char *)calloc(size, 0x100);
			ptr = data;

			gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x000B,NULL,0x0);
			if (size < 100) {
				for (j = 0; j < size; j++) {
			   		gp_port_read(camera->port, (char*)ptr, 0x100);
					ptr += 0x100;
				}
			} else {
				for (j = 0; j < 100; j++) {
			  	 	gp_port_read(camera->port, (char*)ptr, 0x100);
					ptr += 0x100;
				}
			}

			gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&dummy,0x0001);
			gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&dummy,0x0001);

			size = size * 0x100;

			if (size < 128*96*2*4 + 9 + 0x1a0) {
				GP_DEBUG("size %d smaller than expected %d", size, 128*96*2*4 + 9 + 0x1a0);
				return GP_ERROR_CORRUPTED_DATA;
			}

			{
				unsigned int thumb_start = 9 + 0x1a0;
				unsigned char temp1, temp2, temp3, temp4;
				unsigned char Y, Cb, Cr;
				signed int R, G, B;
				unsigned int pixel_count = 0;
				unsigned int offset = 0;

				result = calloc(1, 128 * 96 * 4 * 4 + 100 );

				memcpy(result, "P3\n128 96\n255\n", 14);
				offset = offset + 14;

				temp1 = data[ thumb_start + pixel_count ];

				while (pixel_count < (128*96*2)) {

					temp1 = data[ thumb_start + pixel_count ];
					temp2 = data[ thumb_start + pixel_count + 1 ];
					temp3 = data[ thumb_start + pixel_count + 2 ];
					temp4 = data[ thumb_start + pixel_count + 3 ];
					pixel_count = pixel_count + 4;

					Y = temp1 + 128;
					Cb = temp3 + 128;
					Cr = temp4 + 128;

					R = Y + (1.402 * (Cr-128));
					G = Y - (0.34414 * (Cb-128)) - (0.71414 * (Cr-128));
					B = Y + (1.772 * (Cb-128));
					if (R > 255) R = 255;
					if (R < 0) R = 0;
					if (G > 255) G = 255;
					if (G < 0) G = 0;
					if (B > 255) B = 255;
					if (B < 0) B = 0;

					sprintf((char*)(result + offset), "%03d %03d %03d\n", R, G, B);
					offset = offset + 4 + 4 + 4;

					Y = temp2 + 128;

					R = Y + (1.402 * (Cr-128));
					G = Y - (0.34414 * (Cb-128)) - (0.71414 * (Cr-128));
					B = Y + (1.772 * (Cb-128));
					if (R > 255) R = 255;
					if (R < 0) R = 0;
					if (G > 255) G = 255;
					if (G < 0) G = 0;
					if (B > 255) B = 255;
					if (B < 0) B = 0;

					sprintf((char*)(result + offset), "%03d %03d %03d\n", R,G,B);
					offset = offset + 4 + 4 + 4;
			}

			size = offset;

			gp_file_set_mime_type(file, GP_MIME_PPM);
			gp_file_append(file, (char*)result, size);

			free( result );
			free( data );
		}

		break;
		}

	case GP_FILE_TYPE_RAW: {
		char dummy;
		GP_DEBUG(" * REQUEST FOR RAW IMAGE");

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x0008,NULL,0x0);
		gp_port_read(camera->port, (char*)indata, 0x100);

		size = indata[ 5 ] + (indata[ 6 ] * 0xFF) + 3;
		result = calloc(size, 0x100);
		ptr = result;

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&dummy,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&dummy,0x0001);

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x000A,NULL,0x0);
		for (j = 0; j < size; j++) {
		   gp_port_read(camera->port, (char*)ptr, 0x100);
		   ptr += 100;
		}
		GP_DEBUG(" *DONE READING IMAGE!");

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&dummy,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&dummy,0x0001);

		size = size * 0x100;

		gp_file_set_mime_type(file, GP_MIME_RAW);
		gp_file_append(file, (char*)result, size);

		free( result );

		break;
	}
	case GP_FILE_TYPE_NORMAL: {
		char dummy;
		GP_DEBUG(" * REQUEST FOR NORMAL IMAGE");

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x0008,NULL,0x0);
		gp_port_read(camera->port, (char*)indata, 0x100);

		size = indata[ 5 ] + (indata[ 6 ] * 0xFF) + 3;

		result = calloc((size+1), 0x100);
		ptr = result;

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&dummy,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&dummy,0x0001);

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x000A,NULL,0x0);

		for (j = 0; j < size; j++) {
	   		gp_port_read(camera->port, (char*)ptr, 0x100);
			ptr += 0x100;
		}

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&dummy,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&dummy,0x0001);

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&dummy,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&dummy,0x0001);
		size = size * 0x100;


		lb = *(result + 0x05);
		hb = *(result + 0x04);
		app1len = (unsigned int)(hb * 256) + (unsigned int)(lb);

		if ((app1len < 4) || (size < app1len+4)) {
			GP_DEBUG("size %d smaller than expected %d", size, app1len+2);
			free(result);
			return GP_ERROR_CORRUPTED_DATA;
		}
		GP_DEBUG("size %d app1len %d", size, app1len);

		result[3] = 0xe0;
		result[4] = 0x00;
		result[5] = 0x10;
		result[6] = 'J';
		result[7] = 'F';
		result[8] = 'I';
		result[9] = 'F';
		result[10] = 0x00;
		result[11] = 0x01;
		result[12] = 0x01;
		result[13] = 0x00;
		result[14] = 0x00;
		result[15] = 0x01;
		result[16] = 0x00;
		result[17] = 0x01;
		result[18] = 0x00;
		result[19] = 0x00;

		memmove(&result[20],
		       &result[app1len + 4],
		       (unsigned int)(size - app1len - 4));

		size = size - app1len + 24;

		gp_file_set_mime_type(file, GP_MIME_JPEG);
		gp_file_append(file, (char*)result, size);

		free(result);

		break;
	}
	default:
		GP_DEBUG(" * NOT SUPPORTED");
		return GP_ERROR_NOT_SUPPORTED;
	}


	return (GP_OK);
}

#if 0
static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;

	GP_DEBUG(" * delete_file_func()");

	/* Delete the file from the camera. */

	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;

	GP_DEBUG(" * delete_all_func()");

	/*
	 * Delete all files in the given folder. If your camera doesn't have
	 * such a functionality, just don't implement this function.
	 */

	return (GP_OK);
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	/*
	 * Capture a preview and return the data in the given file (again,
	 * use gp_file_set_data_and_size, gp_file_set_mime_type, etc.).
	 * libgphoto2 assumes that previews are NOT stored on the camera's
	 * disk. If your camera does, please delete it from the camera.
	 */

	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	/*
	 * Capture an image and tell libgphoto2 where to find it by filling
	 * out the path.
	 */

	return (GP_OK);
}
#endif

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	/*
	 * Fill out the summary with some information about the current
	 * state of the camera (like pictures taken, etc.).
	 */

	unsigned char  indata[0x100];
	unsigned short count  = 0;
	int has_compact_flash = 0;

	gp_port_usb_msg_write(camera->port,0x02, 0x0000, 0x0007, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x0A, 0x0000, 0x0000, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x02, 0x0000, 0x0007, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x0A, 0x0000, 0x0001, NULL, 0x0000 );
	gp_port_read( camera->port, (char*)indata, 0x100 );

	count = from_camera( indata[22] + (indata[23]*0x100) );
	if (count > 0) {
		count--;
		has_compact_flash = 1;
	} else {
		has_compact_flash = 0;
	}

	if (has_compact_flash == 0) {
		sprintf( summary->text, _("Camera appears to not be using CompactFlash storage\n"
						"Unfortunately we do not support that at the moment :-(\n"));
	} else {
		sprintf( summary->text, _("Camera has taken %d pictures, and is using CompactFlash storage.\n"), count);
	}
	return GP_OK;
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("agfa_cl20\n"
			       "The Agfa CL20 Linux Driver People!\n"
			       "     Email us at cl20@poeml.de    \n"
			       " Visit us at http://cl20.poeml.de "));

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;
	unsigned char resolution;
	char sbr;
	unsigned char indata[ 0x100 ];

	GP_DEBUG(" * get_info_func()");

	/* Get the file info here and write it into <info> */

	n = gp_filesystem_number( camera->fs, folder, filename, context) + 1;

	info->file.fields = GP_FILE_INFO_TYPE;
	strcpy( info->file.type, GP_MIME_JPEG );

	gp_port_usb_msg_write(camera->port,0x0A, to_camera( n ) ,0x0008,NULL,0x0);
	gp_port_read(camera->port, (char*)indata, 0x100);

	resolution = indata[ 17 ];
	gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&sbr,1);
	gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&sbr,1);

	if (resolution == 1) {
		info->file.width  = 512;
		info->file.height = 384;
		info->preview.fields = GP_FILE_INFO_TYPE;
		strcpy( info->preview.type, GP_MIME_JPEG);
		info->preview.width = 512;
		info->preview.height = 384;
	} else if (resolution == 3) {
		info->file.width = 1024;
		info->file.height = 768;
		info->preview.fields = GP_FILE_INFO_TYPE;
		strcpy( info->preview.type, GP_MIME_PPM);
		info->preview.width = 128;
		info->preview.height = 96;
	} else if (resolution == 5) {

		/* In case you are wondering why this resolution is also 1024x768, it is
		 * because that really IS the resolution. If you were using the official
		 * (windows) software, it would be automagically scaled to appear as if
		 * you took a higher resolution image.
		 * However, high resolution pictures are taken with a lower compression
		 * ratio, which is why the high resolution pictures are roughly twice the
		 * size of the medium resolution ones, despite also being 1024x768
		 * Nevertheless, this is a 0.80 Megapixel camera, no more !!! No matter
		 * what the marketing says!
		 */

		info->preview.fields = GP_FILE_INFO_TYPE;
		strcpy( info->preview.type, GP_MIME_PPM);
		info->preview.width = 128;
		info->preview.height = 96;
		info->file.width = 1024;
		info->file.height = 768;
	} else {
		printf("Invalid resolution found, this should never happen.\n"
			   "Please try resetting the camera, then try again.\n");
		return GP_ERROR;
	}

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera 	* camera = data;
	unsigned char  indata[ 0x100 ];
	unsigned short count  = 0;

	gp_port_usb_msg_write(camera->port,0x02, 0x0000, 0x0007, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x0A, 0x0000, 0x0000, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x02, 0x0000, 0x0007, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x0A, 0x0000, 0x0001, NULL, 0x0000 );
	gp_port_read( camera->port, (char*)indata, 0x100 );

	count = from_camera( indata[22] + (indata[23]*0x100) );
	if (count > 0)
		count--;
	return gp_list_populate( list, "pic_%04i.jpg", count);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
};

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	char single_byte_return = 'X';

	GP_DEBUG(" * camera_init()");

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->summary              = camera_summary;
        camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	/*
	 * The port is already provided with camera->port (and
	 * already open). You just have to use functions like
	 * gp_port_timeout_set, gp_port_settings_get, gp_port_settings_set.
	 */

	gp_port_get_settings( camera->port, &settings );
	switch (camera->port->type) {
		case GP_PORT_SERIAL:
			return ( GP_ERROR );
		case GP_PORT_USB:
			settings.usb.config = 1;
			settings.usb.interface = 1;
			settings.usb.inep = 2;
			break;
		default:
			return ( GP_ERROR );
	}

	gp_port_set_settings(camera->port, settings );

	/* Camera should return either 0x00 or 0x08, depending on
	 * whether the PRINT button is depressed. Either way, it
	 * should NOT stay 'X'
	 */

	gp_port_usb_msg_read( camera->port,
		0x00,
		0x0000,
		0x8985,
		&single_byte_return, 1 );

	if ((single_byte_return == 0) || (single_byte_return == 8))
		return (GP_OK);
	else
		return (GP_ERROR_MODEL_NOT_FOUND);
}
