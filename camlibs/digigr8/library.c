/* library.c for libgphoto2/camlibs/digigr8
 *
 * Copyright (C) 2005 - 2010 Theodore Kilgore <kilgota@auburn.edu>
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

#define _DEFAULT_SOURCE

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libgphoto2/bayer.h>
#include <libgphoto2/gamma.h>


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

#include "digigr8.h"

#include <gphoto2/gphoto2-port.h>

#define GP_MODULE "digigr8"

static const struct {
	char *name;
	CameraDriverStatus status;
	unsigned short idVendor;
	unsigned short idProduct;
} models[] = {
	{"Digigr8",		GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x905c},
	{"Cobra Digital Camera DC150", GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770,
									0x905c},
	{"Che-Ez Snap SNAP-U",	GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x905c},
	{"DC-N130t",		GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x905C},
	{"Soundstar TDC-35",	GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x905c},
	{"Nexxtech Mini Digital Camera", GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770,
									0x905c},
	{"Vivitar Vivicam35",	GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x905c},
	{"Praktica Slimpix",	GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x905c},
	{"ZINA Mini Digital Keychain Camera",	GP_DRIVER_STATUS_EXPERIMENTAL,
							    0x2770, 0x905c},
	{"Pixie Princess Jelly-Soft",		GP_DRIVER_STATUS_EXPERIMENTAL,
							    0x2770, 0x905c},
	{"Sakar Micro Digital 2428x",		GP_DRIVER_STATUS_EXPERIMENTAL,
							    0x2770, 0x905c},
	{"Stop & Shop 87096",		GP_DRIVER_STATUS_EXPERIMENTAL,
							    0x2770, 0x905c},
	{"Jazz JDC9",		GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x905c},
	{"Aries Digital Keychain Camera, ITEM 128986",
				GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x905c},
	{"Disney pix micro",	GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9050},
	{"Lego Bionicle",	GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9051},
	{"Barbie Camera (Digital Blue)",	GP_DRIVER_STATUS_EXPERIMENTAL,
							    0x2770, 0x9051},
	/* from IRC reporter, adam@piggz.co.uk */
	{"Disney pix micro 2",	GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9052},
	{"Suprema Digital Keychain Camera",	GP_DRIVER_STATUS_EXPERIMENTAL,
								0x2770, 0x913d},
	{"Sakar 28290 and 28292  Digital Concepts Styleshot",
						GP_DRIVER_STATUS_EXPERIMENTAL,
							0x2770, 0x913d},
	{"Sakar 23070  Crayola Digital Camera", GP_DRIVER_STATUS_EXPERIMENTAL,
							0x2770, 0x913d},
	{"Sakar 92045  Spiderman",    GP_DRIVER_STATUS_EXPERIMENTAL,
							0x2770, 0x913d},
	{NULL,0,0,0}
};

int
camera_id(CameraText *id)
{
	strncpy (id->text, "SQ905C chipset camera",32);
	return GP_OK;
}


int
camera_abilities(CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].name; i++) {
		memset (&a, 0, sizeof(a));
		strncpy (a.model, models[i].name,32);
		a.status = models[i].status;
		a.port   = GP_PORT_USB;
		a.speed[0] = 0;
		a.usb_vendor = models[i].idVendor;
		a.usb_product= models[i].idProduct;
		if (a.status == GP_DRIVER_STATUS_EXPERIMENTAL)
			a.operations = GP_OPERATION_NONE;
		else
			a.operations = GP_OPERATION_CAPTURE_PREVIEW;
		a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;
		a.file_operations   = GP_FILE_OPERATION_PREVIEW
					+ GP_FILE_OPERATION_RAW;
		gp_abilities_list_append (list, a);
	}
	return GP_OK;
}

static int
camera_summary(Camera *camera, CameraText *summary, GPContext *context)
{
	if (!camera->pl->init_done)
		digi_init (camera->port, camera->pl);
	snprintf (summary->text, 100,
			("Your USB camera seems to have an SQ905C chipset.\n"
			"The total number of pictures in it is %i\n"),
				camera->pl->nb_entries);
	return GP_OK;
}

static int camera_manual(Camera *camera, CameraText *manual,
							GPContext *context)
{
	strncpy(manual->text,
	_(
	"For cameras with insides from S&Q Technologies, which have the \n"
	"USB Vendor ID 0x2770 and Product ID 0x905C, 0x9050, 0x9051,\n"
	"0x9052, or 0x913D.  Photos are saved in PPM format.\n\n"
	"Some of these cameras allow software deletion of all photos.\n"
	"Others do not. No supported camera can do capture-image. All\n"
	"can do capture-preview (image captured and sent to computer).\n"
	"If delete-all does work for your camera, then capture-preview will\n"
	"have the side-effect that it also deletes what is on the camera.\n\n"
	"File uploading is not supported for these cameras. Also, none of the\n"
	"supported cameras allow deletion of individual photos by use of a\n"
	"software command.\n"
	), 700);
	return (GP_OK);
}


static int
camera_about(Camera *camera, CameraText *about, GPContext *context)
{
	strncpy (about->text, _("sq905C generic driver\n"
			    "Theodore Kilgore <kilgota@auburn.edu>\n"),64);
	return GP_OK;
}

/*************** File and Downloading Functions *******************/


static int
file_list_func(CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
	Camera *camera = data;
	int n;
	if(!camera->pl->init_done)
		digi_init (camera->port, camera->pl);
	GP_DEBUG ("List files in %s\n", folder);
	n = camera->pl->nb_entries;
	gp_list_populate(list, "pict%03i.ppm", n);
	return GP_OK;
}


static int
get_file_func(CameraFilesystem *fs, const char *folder, const char *filename,
		CameraFileType type, CameraFile *file, void *user_data,
							GPContext *context)
{
	int status = GP_OK;
	Camera *camera = user_data;
	unsigned int b;
	unsigned int w, h;
	int k, next;
	unsigned char comp_ratio;
	unsigned char lighting;
	unsigned char *data = NULL;
	unsigned char *p_data = NULL;
	unsigned char *ppm;
	unsigned char *ptr;
	unsigned char gtable[256];
	int size;

	if (!camera->pl->init_done)
		digi_init (camera->port, camera->pl);

	/* Get the entry number of the photo on the camera */
	k = gp_filesystem_number (camera->fs, "/", filename, context);

	if (GP_FILE_TYPE_EXIF ==type) return GP_ERROR_FILE_EXISTS;

	if (GP_FILE_TYPE_RAW!=type && GP_FILE_TYPE_NORMAL
				    != type && GP_FILE_TYPE_PREVIEW != type) {
		return GP_ERROR_NOT_SUPPORTED;
	}

	next = camera->pl->last_fetched_entry +1;
	while (next < k) {
		b = digi_get_data_size (camera->pl, next);
		data = malloc(b);
		if(!data) return GP_ERROR_NO_MEMORY;
		digi_read_picture_data (camera->port, data, b, next);
		free(data);
		next ++;
	}

	comp_ratio = digi_get_comp_ratio (camera->pl, k);
	w = digi_get_picture_width (camera->pl, k);
	switch (w) {
	case 176: h = 144; break;
	case 640: h = 480; break;
	case 320: h = 240; break;
	default:  h = 288; break;
	}
	lighting = camera->pl->catalog[k*0x10+0x0b];
	b = digi_get_data_size (camera->pl, k);
	if (!b) {
		GP_DEBUG("Photo number %i deleted?\n",k+1);
		camera->pl->last_fetched_entry = k;
		return GP_OK;
	}
	if (b < w*h) {
		GP_DEBUG("need %d bytes, supposed to read only %d", w*h, b);
		return GP_ERROR;
	}
	data = malloc (b);
	if(!data) return GP_ERROR_NO_MEMORY;

	GP_DEBUG("Fetch entry %i\n", k);
	digi_read_picture_data (camera->port, data, b, k);
	camera->pl->last_fetched_entry = k;

	if (GP_FILE_TYPE_RAW == type) {	/* type is GP_FILE_TYPE_RAW */
		size = b;
		gp_file_set_mime_type (file, GP_MIME_RAW);
		gp_file_append(file, (char *)data, size);
		/* Save photo's catalog entry as a footer for the raw file */
		gp_file_append(file, (char *)camera->pl->catalog
						+ k * 0x10, 0x10);
		/* Reset camera when done, for more graceful exit. */
		if (k +1 == camera->pl->nb_entries) {
			digi_rewind (camera->port, camera->pl);
		}
		free(data);
		return(GP_OK);
	}

	/*
	 * Now put the data into a PPM image file.
	 */

	ppm = malloc (w * h * 3 + 256); /* room for data + header */
	if (!ppm) {
		status = GP_ERROR_NO_MEMORY;
		goto end;
	}
	snprintf ((char *)ppm, 64,
			"P6\n"
			"# CREATOR: gphoto2, SQ905C library\n"
			"%d %d\n"
			"255\n", w, h);
	size = strlen ((char *)ppm);
	ptr = ppm + size;
	size = size + (w * h * 3);
	GP_DEBUG ("size = %i\n", size);
	p_data = calloc(w , h);
	if (!p_data) {
		status =  GP_ERROR_NO_MEMORY;
		free (ppm);
		goto end;
	}
	if(comp_ratio) {
		digi_decompress (p_data, data, w, h);
	} else
		memcpy(p_data, data, w * h);
	GP_DEBUG("w %d, h %d, size %d", w, h, size);
	gp_ahd_decode(p_data, w , h , ptr, BAYER_TILE_BGGR);
	free(p_data);
	digi_postprocess(w, h, ptr);
	if (lighting < 0x40) {
	GP_DEBUG(
		"Low light condition. Using default gamma. \
						No white balance.\n");
		gp_gamma_fill_table (gtable, .65);
		gp_gamma_correct_single(gtable,ptr,w*h);
	} else
		white_balance (ptr, w*h, 1.1);
	gp_file_set_mime_type (file, GP_MIME_PPM);
	gp_file_set_data_and_size (file, (char *)ppm, size);
	/* Reset camera when done, for more graceful exit. */
	if (k + 1 == camera->pl->nb_entries) {
		digi_rewind (camera->port, camera->pl);
	}
 end:
	free(data);
	return status;
}

static int
delete_all_func(CameraFilesystem *fs, const char *folder, void *data,
						GPContext *context)
{
	Camera *camera = data;
	if (!camera->pl->delete_all)
		return GP_ERROR_NOT_SUPPORTED;
	if (!camera->pl->init_done)
		digi_init(camera->port, camera->pl);
	digi_delete_all(camera->port, camera->pl);
	return GP_OK;
}

static int
camera_capture_preview(Camera *camera, CameraFile *file, GPContext *context)

{
	unsigned char get_size[0x50];
	unsigned char *raw_data;
	unsigned char *frame_data;
	unsigned char *ppm, *ptr;
	char lighting;
	unsigned char gtable[256];
	int size;
	int w = 320;
	int h = 240;
	int b;

	digi_reset (camera->port);
	gp_port_usb_msg_write (camera->port, 0x0c, 0x1440, 0x110f, NULL, 0);
	gp_port_read(camera->port, (char *)get_size, 0x50);
	GP_DEBUG("get_size[0x40] = 0x%x\n", get_size[0x40]);
	lighting = get_size[0x48];
        b = get_size[0x40] | get_size[0x41] << 8 | get_size[0x42] << 16
						| get_size[0x43]<<24;
	GP_DEBUG("b = 0x%x\n", b);
	raw_data = malloc(b);

	if(!raw_data)
		return GP_ERROR_NO_MEMORY;

	if (!((gp_port_read(camera->port, (char *)raw_data, b)==b))) {
		free(raw_data);
		GP_DEBUG("Error in reading data\n");
		return GP_ERROR;
	}
	frame_data = calloc(w , h);
	if (!frame_data) {
		free(raw_data);
		return GP_ERROR_NO_MEMORY;
	}
	digi_decompress (frame_data, raw_data, w, h);
	free(raw_data);
	/* Now put the data into a PPM image file. */
	ppm = malloc (w * h * 3 + 256);
	if (!ppm) {
		free(frame_data);
		return GP_ERROR_NO_MEMORY;
	}
	snprintf ((char *)ppm, 64,
		"P6\n"
		"# CREATOR: gphoto2, SQ905C library\n"
		"%d %d\n"
		"255\n", w, h);
	ptr = ppm + strlen ((char*)ppm);
	size = strlen ((char*)ppm) + (w * h * 3);
	GP_DEBUG ("size = %i\n", size);
	gp_ahd_decode (frame_data, w , h , ptr, BAYER_TILE_BGGR);
	free(frame_data);
        if (lighting < 0x40) {
		GP_DEBUG(
		"Low light condition. Default gamma. No white balance.\n");
		gp_gamma_fill_table (gtable, .65);
		gp_gamma_correct_single(gtable,ptr,w*h);
	} else
		white_balance(ptr, w * h, 1.1);
	gp_file_set_mime_type(file, GP_MIME_PPM);
	gp_file_set_data_and_size(file, (char *)ppm, size);
	digi_reset(camera->port);
	return(GP_OK);
}

/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG("SQ camera_exit");
	digi_reset(camera->port);

	if (camera->pl) {
		free (camera->pl->catalog);
		free (camera->pl);
		camera->pl = NULL;
	}
	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.delete_all_func = delete_all_func
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

	/* Now, set up all the function pointers */
	camera->functions->summary	= camera_summary;
        camera->functions->manual	= camera_manual;
	camera->functions->about	= camera_about;
	camera->functions->capture_preview
					= camera_capture_preview;
	camera->functions->exit		= camera_exit;

	GP_DEBUG ("Initializing the camera\n");

	ret = gp_port_get_settings(camera->port,&settings);
	if (ret < 0)
		return ret;
	ret = gp_port_set_settings(camera->port,settings);
	if (ret < 0)
		return ret;

	/* Tell the CameraFilesystem where to get lists from */
	gp_filesystem_set_funcs(camera->fs, &fsfuncs, camera);
	camera->pl = malloc(sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return GP_ERROR_NO_MEMORY;
	camera->pl->catalog = NULL;
	camera->pl->nb_entries = 0;
	switch (abilities.usb_product) {
	case 0x9050:
	case 0x9051:
	case 0x9052:
		camera->pl->delete_all = 1;
		break;
	default:
		camera->pl->delete_all = 0;
	}
	camera->pl->init_done=0;

	/* Do digi_init() only if needed for the requested operation. */

	return GP_OK;
}
