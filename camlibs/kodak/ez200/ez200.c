/* ez200.c
 *
 * Copyright (C) 2004 Bucas Jean-Francois <jfbucas@tuxfamily.org>
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
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#define PING 		0x05
#define STATUS		0x06
#define PICTURE		0x08
#define ERASE		0x09
#define PICTURE_HEAD	0x0b
#define ACTIVE		0xe0

#define JPG_HEADER_SIZE 0x24D   /* 589 */
#define HEADER_SIZE 0x26F
#define DATA_HEADER_SIZE 0x200

typedef unsigned char Info;

typedef enum {
	MODEL_MINI
} Model;

#define GP_MODULE "ez200"

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

#define WRITE gp_port_usb_msg_write
#define READ  gp_port_usb_msg_read

static int
ez200_wait_status_ok(GPPort *port) {
	char c = 0;

	do READ(port, STATUS, 0, 0, &c, 1);
	while (c != 0); /* Wait */
	return GP_OK;
}

/* enter photo mode  */
static int
ez200_init (GPPort *port, Model *model, Info *info) {
	char c = 0;

	GP_DEBUG("Running ez200_init");

	WRITE(port, ACTIVE, 0, 1, NULL, 0);  /*  index = 1; */
	ez200_wait_status_ok(port);

	/*  get number of pictures */
	READ(port, PICTURE, 0, 0, &c, 1);
	memcpy (info, &c, 1);
	GP_DEBUG("number of pics : %i", c);
        return GP_OK;
}

/*  quit photo mode  */
static int
ez200_exit (GPPort *port) {
	WRITE(port, ACTIVE, 0, 0, NULL, 0);  /* index = 0; */
	ez200_wait_status_ok(port);
	return GP_OK;
}

#define ez200_get_num_pics(info) info[0]

static int
ez200_get_picture_size (GPPort *port, int n) {
	unsigned char c[4];
        unsigned int size = 0;
	memset (c,0,sizeof(c));

	GP_DEBUG("Running ez200_get_picture_size");

	READ(port, PICTURE, n, 1, (char *)c, 3);
	size = (int)c[0] + (int)c[1]*0x100 + (int)c[2]*0x10000;

	GP_DEBUG(" size of picture %i is 0x%x = %i byte(s)", n, size, size);
	if ( (size >= 0xfffff ) ) { return GP_ERROR; }
	return size;
}

static int
ez200_read_data (GPPort *port, char *data, int size) {
	int MAX_BULK = 0x1000;

	/* Read Data by blocks */
	while(size > 0) {
		int len = (size>MAX_BULK)?MAX_BULK:size;
	        gp_port_read  (port, data, len);
		data += len;
		size -= len;
	}
        return 1;
}

static int
ez200_read_picture_data (GPPort *port, char *data, int size, int n) {
	char c[4];

	memset(c,0,sizeof(c));
	/* ask picture n transfert */
    	READ(port, PICTURE, n, 1, c, 3);
        ez200_read_data (port, data, size);
        return GP_OK;
}


/* after read_picture_data, we can retrieve the header by : */
static int
ez200_read_picture_header(GPPort *port, char *data) {
	/*  lecture de l'entete :: read header of picture  */
    	READ(port, PICTURE_HEAD, 3, 3, data, HEADER_SIZE);
	return GP_OK;
}

struct _CameraPrivateLibrary {
	Model model;
	Info info[2];
};

static const struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"Kodak EZ200", GP_DRIVER_STATUS_PRODUCTION, 0x040a, 0x0300},
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

    	for (i = 0; models[i].name; i++) {
        	memset (&a, 0, sizeof(a));
       		strcpy (a.model, models[i].name);
       		a.status = models[i].status;
       		a.port   = GP_PORT_USB;
       		a.usb_vendor = models[i].idVendor;
       		a.usb_product= models[i].idProduct;
		a.operations = GP_OPERATION_NONE;
       		a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;
       		a.file_operations   = GP_FILE_OPERATION_NONE;
       		gp_abilities_list_append (list, a);
    	}
    	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context) {
    	sprintf (summary->text,_("Your USB camera is a Kodak EZ200.\n"
			"Number of PICs = %i\n"
       			), ez200_get_num_pics(camera->pl->info));
	return GP_OK;
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context) {
    	strcpy (about->text, _("Kodak EZ200 driver\nBucas Jean-Francois <jfbucas@tuxfamily.org>\n"));
    	return GP_OK;
}

/*
 * doesnt work, just creates a empty file.
static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
                GPContext *context) {
	unsigned char c[3];
	int size;
	char *data;
	WRITE(camera->port,0,0x8003,0,NULL,0);
	WRITE(camera->port,5,0x0000,0,NULL,0);

        return GP_OK;
}
*/


static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
                 GPContext *context) {
        Camera *camera = data;

	WRITE (camera->port, ERASE, 0, 1, NULL, 0);
	ez200_wait_status_ok(camera->port);
        return GP_OK;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context) {
	Camera *camera = data;

	return gp_list_populate (list, "ez200_pic%03i.jpg", ez200_get_num_pics (camera->pl->info));
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
    	Camera *camera = user_data;
        int n, len;
	char *data, *data_start;

	n = gp_filesystem_number(camera->fs, "/", filename, context);
	if (n<GP_OK)
		return n;

	len = ez200_get_picture_size (camera->port, n);
	GP_DEBUG("len = %i", len);
	if (len < GP_OK) return len;

	data = (char *)malloc(len + HEADER_SIZE + 1);
	if (!data) return GP_ERROR_NO_MEMORY;

	data_start = data + (HEADER_SIZE - DATA_HEADER_SIZE);
	GP_DEBUG("data - data_start : %p %p : %lx",data, data_start, (long) (data_start - data));

    	ez200_read_picture_data   (camera->port, data_start, len, n);
	ez200_read_picture_header (camera->port, data);
	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
	case GP_FILE_TYPE_NORMAL:
		gp_file_set_mime_type (file, GP_MIME_JPEG);
		gp_file_set_data_and_size (file, data, len + HEADER_SIZE + 1);
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

static int
camera_exit (Camera *camera, GPContext *context) {
	GP_DEBUG ("Kodak EZ200 camera_exit");
	ez200_exit (camera->port);
	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}
	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.delete_all_func = delete_all_func,
};

int
camera_init(Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	int ret = 0;

	/* First, set up all the function pointers */
	camera->functions->summary	= camera_summary;
	camera->functions->about	= camera_about;
	camera->functions->exit		= camera_exit;
	/*camera->functions->capture	= camera_capture;*/

	GP_DEBUG ("Initializing Kodak EZ200");
	ret = gp_port_get_settings(camera->port, &settings);
	if (ret < 0) return ret;

	switch (camera->port->type) {
	case GP_PORT_USB:
		settings.usb.config	= 0;
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

	GP_DEBUG("interface = %i", settings.usb.interface);
	GP_DEBUG("inep = %x", settings.usb.inep);
	GP_DEBUG("outep = %x", settings.usb.outep);

        /* Tell the CameraFilesystem where to get lists from */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));

	/* Connect to the camera */
	ez200_init (camera->port, &camera->pl->model, camera->pl->info);
	GP_DEBUG("fin_camera_init");
	return GP_OK;
}
