/* agfa-cl20.c
 *
 * Copyright (C) 2002-2003 by Philipp Poeml and Dennis Noordsij,
 *                            http://cl20.poeml.de/
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>
#include <gphoto2-library.h>
#include <gphoto2-result.h>

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

static struct {
	char *model;
	int usb_vendor;
	int usb_product;
} models[] = {
	{ "Agfa ePhoto CL20", 0x06bd, 0x0404 },
	{ NULL, 0, 0 }
};


static int to_camera(int a)
{
	int b = a / 10;
	int c = a % 10;
	return ((b * 16) + c);
}

static int from_camera(int a)
{
	int b = a / 16;
	int c = a % 16;
	return ((b * 10) + c);
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

	GP_DEBUG(" * camera_abilities()");

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
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int n = -1;
	char raw[ 500000 ];
	char ppm[ 500000 ];
	int size = -1;
	unsigned char hb, lb;
	unsigned long j;
	unsigned int app1len = -1;
	unsigned char resolution;

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
		gp_port_read(camera->port, raw, 0x100);

		hb = (unsigned char)*(raw + 0x06);
		lb = (unsigned char)*(raw + 0x05);
		size = (unsigned int)(hb * 256) + (unsigned int)(lb);

		resolution = (unsigned char)*(raw + 0x11);
		if (resolution == 1) {

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&lb,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&lb,0x0001);

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x000A,NULL,0x0);
		for (j = 0; j < size; j++)
		   gp_port_read(camera->port, raw+(j*0x100), 0x100);
		printf("Done reading image!\n");
		GP_DEBUG(" *DONE READING IMAGE!");

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&lb,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&lb,0x0001);

		size = size * 0x100;

		lb = (unsigned char)*(raw + 0x05);
		hb = (unsigned char)*(raw + 0x04);
		app1len = (unsigned int)(hb * 256) + (unsigned int)(lb);

		printf("App1 Length is 0x%x\n", app1len);

		printf("Setting JFIF header\n");

		raw[3] = 0xe0;
		raw[4] = 0x00;
		raw[5] = 0x10;
		raw[6] = 'J';
		raw[7] = 'F';
		raw[8] = 'I';
		raw[9] = 'F';
		raw[10] = 0x00;
		raw[11] = 0x01;
		raw[12] = 0x01;
		raw[13] = 0x00;
		raw[14] = 0x00;
		raw[15] = 0x01;
		raw[16] = 0x00;
		raw[17] = 0x01;
		raw[18] = 0x00;
		raw[19] = 0x00;

		printf("Doing memmove\n");

		memmove(&raw[20],
		       &raw[app1len + 4],
		       (unsigned int)(size - app1len - 2));

		size = size - app1len + 24;

		printf("Done!!\n");

		gp_file_set_mime_type(file, GP_MIME_JPEG);
		gp_file_set_name(file, filename);
		gp_file_append(file, raw, size);

		break;

		} else {

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&lb,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&lb,0x0001);

		printf("Reading %d blocks\n", size);

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x000B,NULL,0x0);
		if (size < 100) {
			for (j = 0; j < size; j++)
		   		gp_port_read(camera->port, raw + (j * 0x100), 0x100);
		} else {
			for (j = 0; j < 100; j++)
		  	 	gp_port_read(camera->port, raw + (j * 0x100), 0x100);
		}
		GP_DEBUG(" *DONE READING IMAGE!");

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&lb,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&lb,0x0001);

		size = size * 0x100;

		{
			unsigned int thumb_start = 9 + 0x1a0;
			unsigned char temp1, temp2, temp3, temp4;
			unsigned char Y, Cb, Cr;
			signed int R, G, B;
			unsigned int pixel_count = 0;
			unsigned int offset = 0;

			sprintf(ppm, "P3\n128 96\n255\n");
			offset = offset + 14;

			temp1 = raw[ thumb_start + pixel_count ];
			printf("First victim is %d 0x%x\n", temp1, temp1);

			while (pixel_count < (128*96*2)) {
				temp1 = raw[ thumb_start + pixel_count ];
				temp2 = raw[ thumb_start + pixel_count + 1 ];
				temp3 = raw[ thumb_start + pixel_count + 2 ];
				temp4 = raw[ thumb_start + pixel_count + 3 ];
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

				sprintf(ppm + offset , "%03d %03d %03d\n", R, G, B);
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

				sprintf(ppm + offset, "%03d %03d %03d\n", R,G,B);
				offset = offset + 4 + 4 + 4;
			}

			size = offset;

			gp_file_set_mime_type(file, GP_MIME_PPM);
			gp_file_set_name(file, filename);
			gp_file_append(file, ppm, size);
		}

		break;
		}
	case GP_FILE_TYPE_RAW:
		GP_DEBUG(" * REQUEST FOR RAW IMAGE");

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x0008,NULL,0x0);
		gp_port_read(camera->port, raw, 0x100);

		hb = (unsigned char)*(raw + 0x06);
		lb = (unsigned char)*(raw + 0x05);
		size = (unsigned int)(hb * 256) + (unsigned int)(lb);

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&lb,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&lb,0x0001);

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x000A,NULL,0x0);
		for (j = 0; j < size; j++) {
		   gp_port_read(camera->port, raw + (j * 0x100), 0x100);
		}
		GP_DEBUG(" *DONE READING IMAGE!");

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&lb,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&lb,0x0001);

		size = size * 0x100;

		printf("Done!!\n");

		gp_file_set_mime_type(file, GP_MIME_RAW);
		gp_file_set_name(file, filename);
		gp_file_append(file, raw, size);

		break;
	case GP_FILE_TYPE_NORMAL:
		GP_DEBUG(" * REQUEST FOR NORMAL IMAGE");

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x0008,NULL,0x0);
		gp_port_read(camera->port, raw, 0x100);

		hb = (unsigned char)*(raw + 0x06);
		lb = (unsigned char)*(raw + 0x05);
		size = (unsigned int)(hb * 256) + (unsigned int)(lb);

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&lb,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&lb,0x0001);

		gp_port_usb_msg_write(camera->port,0x0A,to_camera(n),0x000A,NULL,0x0);
		for (j = 0; j < size; j++) {
		   gp_port_read(camera->port, raw + (j * 0x100), 0x100);
		}
		printf("Done reading image!\n");
		GP_DEBUG(" *DONE READING IMAGE!");

		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x0521,&lb,0x0001);
		gp_port_usb_msg_read(camera->port,0x00,0x0000,0x8000,&lb,0x0001);

		size = size * 0x100;

		lb = (unsigned char)*(raw + 0x05);
		hb = (unsigned char)*(raw + 0x04);
		app1len = (unsigned int)(hb * 256) + (unsigned int)(lb);

		printf("App1 Length is 0x%x\n", app1len);

		printf("Setting JFIF header\n");

		raw[3] = 0xe0;
		raw[4] = 0x00;
		raw[5] = 0x10;
		raw[6] = 'J';
		raw[7] = 'F';
		raw[8] = 'I';
		raw[9] = 'F';
		raw[10] = 0x00;
		raw[11] = 0x01;
		raw[12] = 0x01;
		raw[13] = 0x00;
		raw[14] = 0x00;
		raw[15] = 0x01;
		raw[16] = 0x00;
		raw[17] = 0x01;
		raw[18] = 0x00;
		raw[19] = 0x00;
#if 0
		if (n == 1) {
			unsigned int loop = 0;
			int done = -1;
			unsigned int first_c4 = 0;
			unsigned int second_c4 = 0;

			printf("First picture, always problematic\n");

			loop = app1len + 5;
			last = raw[ loop ];
			next = raw[ loop + 1];
			while (done < 1) {
				last = raw[ loop ];
				next = raw[ loop + 1];
				while ((last != 0xff) && (next != 0xc4)) {
					last = next;
					loop++;
					next = raw[ loop ];
					printf("Comparing 0x%x and 0x%x\n", last, next);
				}
				if (done == -1)
					first_c4 = loop;
				else
					second_c4 = loop;
				done++;
				loop++;
			}

			memmove(&raw[first_c4], &raw[second_c4],
				(unsigned int)(size - (second_c4 - first_c4)));
			raw[first_c4 + 3] = 0x00;
		}
#endif
		printf("Doing memmove\n");

		memmove(&raw[20],
		       &raw[app1len + 4],
		       (unsigned int)(size - app1len - 2));

		size = size - app1len + 24;

		printf("Done!!\n");

		gp_file_set_mime_type(file, GP_MIME_JPEG);
		gp_file_set_name(file, filename);
		gp_file_append(file, raw, size);

		break;
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

	char 	* indata = calloc(1, 0x100);
	int 	  count  = -10;
	int	  cf = 0;

	gp_port_usb_msg_write(camera->port,0x02, 0x0000, 0x0007, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x0A, 0x0000, 0x0000, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x02, 0x0000, 0x0007, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x0A, 0x0000, 0x0001, NULL, 0x0000 );
	gp_port_read( camera->port, indata, 0x100 );

	count = (int)(*(indata + 0x10));
	if (count == 0xFFFFFFFF)
		cf = -1;
	else
		cf = 1;

	count = ((int)(*(indata + 0x16)) - 1);
	count = from_camera( count );

	if (cf == 1)
		sprintf(summary->text, "Camera is using CompactFlash and has taken %d pictures", count);
	return (GP_OK);
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	/*
	 * If you would like to tell the user some information about how
	 * to use the camera or the driver, this is the place to do.
	 */

	GP_DEBUG(" * camera_manual()");

	return (GP_OK);
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
	unsigned short resolution;
	char sbr;
	char * indata = calloc(1, 0x100);

	GP_DEBUG(" * get_info_func()");

	/* Get the file info here and write it into <info> */

	n = gp_filesystem_number( camera->fs, folder, filename, context);

	info->file.fields = GP_FILE_INFO_TYPE;
	strcpy( info->file.type, GP_MIME_JPEG );

	gp_port_usb_msg_write(camera->port,0x0A,to_camera(n+1),0x0008,NULL,0x0);
	gp_port_read(camera->port, indata, 0x100);

	resolution = (unsigned short)*(indata + 0x11);
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
		info->preview.fields = GP_FILE_INFO_TYPE;
		strcpy( info->preview.type, GP_MIME_PPM);
		info->preview.width = 128;
		info->preview.height = 96;
		info->file.width = 1024;
		info->file.height = 768;
	}

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera 	* camera = data;
	char 	* indata = calloc(1, 0x100);
	int 	  count  = -10;

	gp_port_usb_msg_write(camera->port,0x02, 0x0000, 0x0007, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x0A, 0x0000, 0x0000, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x02, 0x0000, 0x0007, NULL, 0x0000 );
	gp_port_usb_msg_write(camera->port,0x0A, 0x0000, 0x0001, NULL, 0x0000 );
	gp_port_read( camera->port, indata, 0x100 );

	count = ((int)(*(indata + 0x16)) - 1);
	count = from_camera( count );

	gp_list_populate( list, "pic_%04i.jpg", count);
	free(indata);

	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	char single_byte_return = 'X';

	GP_DEBUG(" * camera_init()");

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func,
				      NULL, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL,
				      camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      NULL, camera);
	gp_filesystem_set_folder_funcs (camera->fs, NULL,
					NULL, NULL, NULL, camera);

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
	 * wether the PRINT button is depressed. Either way, it
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
