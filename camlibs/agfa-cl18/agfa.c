/*
 * agfa.c
 *
 * Copyright (c) 2001 Vince Weaver <vince@deater.net>
 * 
 * based on the digita driver 
 * Copyright (c) 1999-2000 Johannes Erdfelt
 * Copyright (c) 1999-2000 VA Linux Systems
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#ifdef OS2
#include <db.h>
#endif
#include <netinet/in.h>

#include <gphoto2.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "commands.h"

#define CHECK(result) {int r = (result); if (r < 0) return (r);}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "agfa");

	return GP_OK;
}


struct {
	char *name;
	unsigned short idVendor;
	unsigned short idProduct;
	char serial;
} models[] = {
        {"Agfa CL18",0x06bd,0x0403,0},
        {NULL,0,0,0}
};

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities *a;

	for(i = 0; models[i].name; i++) {
		gp_abilities_new (&a);
		strcpy (a->model, models[i].name);
		a->port       = GP_PORT_USB;
		a->usb_vendor = models[i].idVendor;
		a->usb_product= models[i].idProduct;
		a->operations        = 	GP_OPERATION_CAPTURE_IMAGE;
		a->folder_operations = 	GP_FOLDER_OPERATION_NONE;
		a->file_operations   = 	GP_FILE_OPERATION_PREVIEW | 
					GP_FILE_OPERATION_DELETE;

		gp_abilities_list_append (list, a);
	}
	
	return GP_OK;
}

static int
camera_exit (Camera *camera)
{
	struct agfa_device *dev = camera->camlib_data;

	if (dev) {
		if (dev->file_list)
			free (dev->file_list);
		free (dev);
		camera->camlib_data = NULL;
	}

        return GP_OK;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list, 
		void *data)
{
	Camera *camera = data;
	struct agfa_device *dev = camera->camlib_data;
	int i;
	char temp_file[14];
	
	gp_debug_printf (GP_DEBUG_HIGH, "agfa", "file_list_func %s\n", folder);

	if (agfa_get_file_list (dev) < 0) {
		gp_debug_printf (GP_DEBUG_HIGH, "agfa", "Could not "
				 "agfa_get_file_list!");
		return GP_ERROR;
	}
	
	for (i = 0; i < dev->num_pictures; i++) {
		strncpy (temp_file, dev->file_list + (13 * i), 12);
		temp_file[12]=0;
		gp_list_append (list, temp_file, NULL);
	}
	
	return GP_OK;
}

static int
agfa_file_get (Camera *camera, const char *filename, int thumbnail,
	       unsigned char **data, int *size)
{
	struct agfa_device *dev = camera->camlib_data;
	int buflen, throwaway, result;

	gp_debug_printf (GP_DEBUG_LOW, "agfa", "Getting file '%s'...",
			 filename);
 
	/*
	 * Always have to check num photos,
	 * then pic size no matter what.  Otherwise
	 * the camera will stop responding
	 */
	CHECK (throwaway = agfa_photos_taken (dev));

	if (thumbnail)
		CHECK (buflen = agfa_get_thumb_size (dev, filename))
	else
		CHECK (buflen = agfa_get_pic_size (dev, filename));
	
	*data = malloc (buflen + 1);
	if (!*data)
		return (GP_ERROR_NO_MEMORY);
	memset (*data, 0, buflen);
	
	gp_frontend_progress (NULL, NULL, 0.00);
	
	if (thumbnail) {
		result = agfa_get_thumb (dev, filename, *data, buflen);
		if (result < 0) {
			free (*data);
			gp_debug_printf (GP_DEBUG_LOW, "agfa",
					 "agfa_get_thumb failed!");
			return (result);
		}
	} else {
		result = agfa_get_pic (dev, filename, *data, buflen);
		if (result < 0) {
			free (*data);
			gp_debug_printf (GP_DEBUG_LOW, "agfa",
					 "agfa_get_pic failed!");
			return (result);
		}
	}

	gp_frontend_progress (NULL, NULL, 1.00);

	if (size)
		*size = buflen;

	return GP_OK;
}

static int
camera_file_get (Camera *camera, const char *folder, const char *filename,
		 CameraFileType type, CameraFile *file)
{
	unsigned char *data = NULL;
	int size;
	
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		CHECK (agfa_file_get (camera, filename, 0, &data, &size));
		break;
	case GP_FILE_TYPE_PREVIEW:
		CHECK (agfa_file_get (camera, filename, 1, &data, &size));
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	if (!data)
		return GP_ERROR;
	
	gp_file_set_data_and_size (file, data, size);
	gp_file_set_name (file, filename);
	gp_file_set_mime_type (file, GP_MIME_JPEG);
	
	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary)
{
	struct agfa_device *dev = camera->camlib_data;
	int taken;
	
	CHECK (taken = agfa_photos_taken (dev));

	sprintf (summary->text, _("Number of pictures: %d"), taken);
	
	return GP_OK;
}

static int
camera_manual (Camera *camera, CameraText *manual)
{
	strcpy (manual->text, _("Manual Not Implemented Yet"));
	
	return GP_OK;
}

static int
camera_about (Camera *camera, CameraText *about)
{
	strcpy (about->text, _("Agfa CL18\nVince Weaver <vince@deater.net>\n"));
	
	return GP_OK;
}

/* Below contributed by Ben Hague <benhague@btinternet.com> */
static int
camera_capture (Camera *camera, int capture_type, CameraFilePath *path)
{
	struct agfa_device *dev = camera->camlib_data;

	/* 2001/08/30: Lutz Müller <urc8@rz.uni-karlsruhe.de
	 *
	 * This is broken. You should capture a picture here and store it
	 * in the camera. Then, indicate where to find the picture
	 * (don't forget to gp_filesystem_append the picture) and return.
	 *
	 * Please fix.
	 */

	return (agfa_capture (dev, path));
}


static int
camera_file_delete (Camera *camera, const char *folder, const char *filename)
{
	int result;
	struct agfa_device *dev=camera->camlib_data;

	gp_debug_printf (GP_DEBUG_LOW, "agfa", "Deleting '%s' in "
			 "'%s'...", folder, filename);

	/* Delete the picture on the camera */
        if (agfa_delete_picture (dev, filename) < 0)
                return GP_ERROR;

	/* Delete the picture on the filesystem */
	result = gp_filesystem_delete (camera->fs, folder, filename);
	if (result < 0)
		return (result);

	/* Update our file list */
        if (agfa_get_file_list (dev) < 0)
                return GP_ERROR;

        return GP_OK;
}

int
camera_init (Camera *camera)
{
	gp_port_settings settings;
	struct agfa_device *dev;
	int ret = 0;

       /* First, set up all the function pointers */
	camera->functions->exit         = camera_exit;
	camera->functions->file_get     = camera_file_get;
	camera->functions->summary      = camera_summary;
	camera->functions->manual       = camera_manual;
	camera->functions->about        = camera_about;
	camera->functions->file_delete  = camera_file_delete;
	camera->functions->capture      = camera_capture;
	
	gp_debug_printf (GP_DEBUG_LOW, "agfa", "Initializing the camera\n");

	switch (camera->port->type) {
	case GP_PORT_USB:

		/* Find the device */
		ret = gp_port_usb_find_device (camera->port,
					       camera->abilities->usb_vendor,
					       camera->abilities->usb_product);
		if (ret < 0)
			return (ret);

		settings.usb.inep = 0x83;
		settings.usb.outep = 0x02;
		settings.usb.config = 0;
		settings.usb.interface = 0;
		settings.usb.altsetting = 0;
		ret = gp_port_settings_set (camera->port, settings);
		if (ret < 0)
			return (ret);
		
		/* Open the port */
		ret = gp_port_open (camera->port);
		if (ret < 0)
			return (ret);
		break;
	case GP_PORT_SERIAL:
		return (GP_ERROR_IO_SUPPORTED_SERIAL);
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	dev = malloc(sizeof(*dev));
	if (!dev)
		return (GP_ERROR_NO_MEMORY);

	memset ((void *)dev, 0, sizeof (*dev));
	dev->gpdev = camera->port; 
	camera->camlib_data = dev;

	ret = agfa_reset (dev);
	if (ret < 0)
		return (ret);
	
	/* Tell the CameraFilesystem where to get lists from */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	
	return GP_OK;
}

