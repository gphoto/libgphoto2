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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

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

#include "jl2005a.h"
#include <gphoto2/gphoto2-port.h>

#define GP_MODULE "jl2005a"

struct {
	char *name;
	CameraDriverStatus status;
	unsigned short idVendor;
	unsigned short idProduct;
} models[] = {
	{"American Idol Keychain Camera", GP_DRIVER_STATUS_TESTING,
							    0x0979, 0x0224},
	{"NogaNet TDC-15", GP_DRIVER_STATUS_TESTING, 0x0979, 0x0224},
	{"Cobra DC125", GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0224},
	{NULL,0,0,0}
};

int
camera_id (CameraText *id)
{
	strcpy (id->text, "JL2005A camera");
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
	sprintf (summary->text,_("This camera contains a Jeilin JL2005A chipset.\n"
			"The number of photos in it is %i. \n"), num_pics);
	return GP_OK;
}


static int camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	strcpy(manual->text,
	_(
        "This driver supports cameras with Jeilin jl2005a chip \n"
	"These cameras do not support deletion of photos, nor uploading\n"
	"of data. \n"
	"Decoding of compressed photos may or may not work well\n"
	"and does not work equally well for all supported cameras.\n"
	"If present on the camera, video clip frames are downloaded \n"
	"as consecutive still photos.\n"
	"For further details please consult libgphoto2/camlibs/README.jl2005a\n"
	));

	return (GP_OK);
}


static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("jl2005a camera library\n"
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
	int status = GP_OK;
	unsigned int w, h = 0;
	unsigned int i, j;
	int k;
	unsigned int b = 0;
	int compressed = 0;
	unsigned char header[5] = "\xff\xff\xff\xff\x55";
	unsigned int size;
	unsigned char *data;
	unsigned char *image_start;
	unsigned char *p_data=NULL;
	unsigned char *ppm=NULL, *ptr=NULL;
	unsigned char gtable[256];
	unsigned char temp;

	GP_DEBUG ("Downloading pictures!\n");

	/* These are cheap cameras. There ain't no EXIF data. So kill this. */
	if (GP_FILE_TYPE_EXIF == type) return GP_ERROR_FILE_EXISTS;
	/* Get the number of the photo on the camera */
	k = gp_filesystem_number (camera->fs, "/", filename, context);
	GP_DEBUG ("Filesystem number is %i\n",k);
	b = jl2005a_get_pic_data_size(camera->port, k);
	GP_DEBUG("b = %i = 0x%x bytes\n", b,b);
	w = jl2005a_get_pic_width(camera->port);
	GP_DEBUG ("width is %i\n", w);
	h = jl2005a_get_pic_height(camera->port);
	GP_DEBUG ("height is %i\n", h);

	/* sanity check against bad usb devices */
	if ((w ==0) || (w > 1024) || (h == 0) || (h > 1024)) {
		GP_DEBUG ("width / height not within sensible range");
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (b < w*h+5) {
		GP_DEBUG ("b is %i, while w*h+5 is %i\n", b, w*h+5);
		return GP_ERROR_CORRUPTED_DATA;
	}

	/* Image data to be downloaded contains header and footer bytes */
	data = malloc (b+14);
	if (!data) return GP_ERROR_NO_MEMORY;

	jl2005a_read_picture_data (camera, camera->port, data, b+14);
	if (memcmp(header,data,5) != 0)
		/* Image data is corrupted! Repeat the operation. */
		jl2005a_read_picture_data (camera, camera->port, data, b+14);

	if (GP_FILE_TYPE_RAW == type) {
		gp_file_set_mime_type(file, GP_MIME_RAW);
		gp_file_set_data_and_size(file, (char *)data , b+14 );
		return GP_OK;
	}

	/* Now get ready to put the data into a PPM image file. */
	image_start=data+5;
	if (w == 176) {
		for (i=1; i < h-1; i +=4){
			for (j=1; j< w; j ++){
				temp=image_start[i*w+j];
				image_start[i*w+j] = image_start[(i+1)*w+j];
				image_start[(i+1)*w+j] = temp;
			}
		}
		if (h == 72) {
			compressed = 1;
			h = 144;
		}
	} else
		if (h == 144) {
			compressed = 1;
			h = 288;
		}

	/* sanity check the sizes, as the ahd bayer algorithm does not like very small height / width */
	if ((h < 72) || (w < 176)) {
		status = GP_ERROR_CORRUPTED_DATA;
		goto end;
	}
	p_data = calloc( w,h );
	if (!p_data) {
		status =  GP_ERROR_NO_MEMORY;
		goto end;
	}
	if (compressed) {
		/* compressed seems to mean half the lines */
		if (w/2*h > b+14) {
			free(p_data);
			status = GP_ERROR_CORRUPTED_DATA;
			goto end;
		}
		jl2005a_decompress (image_start, p_data, w, h);
	} else {
		if (w*h > b+14) {
			free(p_data);
			status = GP_ERROR_CORRUPTED_DATA;
			goto end;
		}
		memcpy(p_data, image_start, w*h);
	}
	ppm = malloc (w * h * 3 + 256); /* room for data and header */
	if (!ppm) {
		free(p_data);
		status = GP_ERROR_NO_MEMORY;
		goto end;
	}
	sprintf ((char *)ppm,
			"P6\n"
			"# CREATOR: gphoto2, JL2005A library\n"
			"%d %d\n"
			"255\n", w, h);
	size = strlen ((char *)ppm);
	ptr = ppm + size;
	size = size + (w * h * 3);
	GP_DEBUG ("size = %i, w = %d, h = %d\n", size, w,h );
	gp_ahd_decode (p_data, w , h, ptr, BAYER_TILE_BGGR);

	free(p_data);
	gp_gamma_fill_table (gtable, .65);
	gp_gamma_correct_single (gtable, ptr, w * h);
	gp_file_set_mime_type (file, GP_MIME_PPM);
	gp_file_set_data_and_size (file, (char *)ppm, size);
end:
	free(data);
	return status;
}


/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("jl2005a camera_exit");
	jl2005a_reset(camera, camera->port);
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
	camera->functions->summary	= camera_summary;
	camera->functions->about	= camera_about;
	camera->functions->exit		= camera_exit;

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
			/* inep 0x84 used for commands, 0x81 for data. */
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
	camera->pl->data_reg_accessed = 0;
	camera->pl->data_to_read = 0;
	camera->pl->data_used_from_block = 0;
	camera->pl->data_cache = NULL;
	jl2005a_init (camera,camera->port, camera->pl);

	return GP_OK;
}
