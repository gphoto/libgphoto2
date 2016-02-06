/* library.c

 *
 * Copyright (C) 2006-2010 Theodore Kilgore <kilgota@auburn.edu>
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

#include "jl2005bcd_decompress.h"
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
	{" JL2005B/C/D camera", GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"Argus DC1512e",       GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"Amazing Spiderman",   GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"Aries ATC-0017",      GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"Sakar no. 75379",     GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"Sakar no. 81890",     GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"Sakar Nickelodeon iCarly no. 88061", GP_DRIVER_STATUS_EXPERIMENTAL,
							0x0979, 0x0227},
	{"Sakar Dora the Explorer no. 88067", GP_DRIVER_STATUS_EXPERIMENTAL,
							0x0979, 0x0227},
	{"Sakar no. 91379",     GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"Sakar no. 98379",     GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"Sakar Kidz-Cam no. 88379", GP_DRIVER_STATUS_EXPERIMENTAL,
							       0x0979, 0x0227},
	{"Sakar clipshot no. 1169x", GP_DRIVER_STATUS_EXPERIMENTAL,
							       0x0979, 0x0227},
	{"Sakar Sticker Wizard no. 59379", GP_DRIVER_STATUS_EXPERIMENTAL,
							       0x0979, 0x0227},
	{"Sakar Star Wars kit no. 92022", GP_DRIVER_STATUS_EXPERIMENTAL,
							       0x0979, 0x0227},
	{"Jazwares Star Wars no. 15256", GP_DRIVER_STATUS_EXPERIMENTAL,
							       0x0979, 0x0227},
	{"Argus Bean Sprout",   GP_DRIVER_STATUS_EXPERIMENTAL,
							       0x0979, 0x0227},
	{"Global Point Clipster", GP_DRIVER_STATUS_EXPERIMENTAL,
							       0x0979, 0x0227},
	{"Global Point 3 in 1 Digital Fun Graffiti 00044",
				GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"Jazz JDK235", 	GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"DIGITAL MID#0020509 (no-name camera)",
				GP_DRIVER_STATUS_EXPERIMENTAL, 0x0979, 0x0227},
	{"Vivitar Freelance", GP_DRIVER_STATUS_EXPERIMENTAL,
							       0x0979, 0x0227},
	{"Sakar Hello Kitty no. 94009", GP_DRIVER_STATUS_EXPERIMENTAL,
							       0x0979, 0x0227},
	{NULL,0,0,0}
};

int
camera_id (CameraText *id)
{
	strcpy (id->text, "JL2005B/C/D camera");
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
	sprintf(summary->text,
			_("This camera contains a Jeilin JL2005%c chipset.\n"
			"The number of photos in it is %i. \n"),
			camera->pl->model, num_pics);
	return GP_OK;
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	strcpy(manual->text,
	_(
	"This driver supports cameras with Jeilin JL2005B or C or D  chip \n"
	"These cameras do not support deletion of photos, nor uploading\n"
	"of data. \n"
	"If present on the camera, video clip frames are downloaded \n"
	"as consecutive still photos.\n"
	"For more details please consult libgphoto2/camlibs/README.jl2005c\n"
	));

	return (GP_OK);
}


static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("jl2005bcd camera library\n"
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
	unsigned char *pic_data, *pic_buffer, *pic_output = NULL;
	int HEADERSIZE=16;
	int outputsize;
	unsigned long start_of_photo;
	unsigned int downloadsize = 0;
	int filled = 0;

	GP_DEBUG ("Downloading pictures!\n");
	if(!camera->pl->data_reg_opened)
	jl2005c_open_data_reg (camera, camera->port);
	/* These are cheap cameras. There ain't no EXIF data. */
	if (GP_FILE_TYPE_EXIF == type) return GP_ERROR_FILE_EXISTS;

	/* Get the number of the photo on the camera */
	k = gp_filesystem_number (camera->fs, "/", filename, context);
	h = camera->pl->table[16 * k + 4] << 3;
	w = camera->pl->table[16 * k + 5] << 3;

	GP_DEBUG ("height is %i\n", h);

	b = jl2005c_get_pic_data_size(camera->pl, camera->pl->table, k);
	GP_DEBUG("b = %i = 0x%x bytes\n", b, b);
	start_of_photo = jl2005c_get_start_of_photo(camera->pl,
						    camera->pl->table, k);
	GP_DEBUG("start_of_photo number %i = 0x%lx \n", k,start_of_photo);
	pic_buffer = malloc(b + HEADERSIZE);
	if (!pic_buffer) return GP_ERROR_NO_MEMORY;
	memset(pic_buffer, 0, b + HEADERSIZE);
	GP_DEBUG("buffersize b + 16 = %i = 0x%x bytes\n", b + 16, b + 16);
	/* Copy info line for photo from allocation table, as header */
	memcpy(pic_buffer, camera->pl->table + 16 * k, 16);
	pic_data = pic_buffer + HEADERSIZE;

	/*
	 * Camera can download in blocks of 0xfa00, with only the last block
	 * possibly smaller. So first we set up a cache of that size
	 * (if it is not set up already) to hold raw data. If one tries
	 * instead to download one photo at a time, the camera will misbehave;
	 * data will be lost or corrupted. The dog will bite you, too.
	 */

	if (!(camera->pl->data_cache)) {
		camera->pl->data_cache = malloc (MAX_DLSIZE);
	}
	if (!(camera->pl->data_cache)) {
		GP_DEBUG ("no cache memory allocated!\n");
		free (pic_buffer);
		return GP_ERROR_NO_MEMORY;
	}

	/* Is there data in the cache, or not? If yes, read from it into the
	 * current photo, immediately. Update settings. But first two sanity
	 * checks.
	 */

	if (start_of_photo < camera->pl->bytes_put_away) {
		GP_DEBUG("photo number %i starts in a funny place!\n",k);
		/* We need to start all over again to get this photo. */
		jl2005c_reset (camera, camera->port);
		jl2005c_init (camera, camera->port, camera->pl);
	}
	if (start_of_photo + b > camera->pl->total_data_in_camera) {
		GP_DEBUG ("Photo runs past end of data. Exiting. \n");
		GP_DEBUG ("Block size may be wrong for this camera\n");
		free (pic_buffer);
		return (GP_ERROR);
	}
	/*
	 * This while loop is entered if the photo number k-1 was not requested
	 * and thus has not been downloaded. The camera's rudimentary hardware
	 * obliges us to download all data consecutively and toss whatever
	 * portion of said data that we do not intend to use. The rudimentary
	 * hardware also does not like to stop downloading at the end of one
	 * photo and then to start on the next. It wants to keep getting data
	 * in size 0xfa00 increments, and only the last block can be smaller.
	 * To do otherwise will cause data to be lost or corrupted.
	 *
	 * Whoever tries to simplify this convoluted and ugly procedure is
	 * warned that the obvious simplifications, while much prettier,
	 * just won't work. A kutya harap.
	 */
	while (camera->pl->bytes_read_from_camera <= start_of_photo) {
		camera->pl->data_to_read = camera->pl->total_data_in_camera
			    - camera->pl->bytes_read_from_camera;
		downloadsize = MAX_DLSIZE;
		if (camera->pl->data_to_read < downloadsize)
			downloadsize = camera->pl->data_to_read;
		GP_DEBUG("downloadsize = 0x%x\n", downloadsize);
		if (downloadsize)
			jl2005c_read_data (
					    camera->port,
					    (char *) camera->pl->data_cache,
					    downloadsize);
		camera->pl->bytes_read_from_camera += downloadsize;
	}

	camera->pl->bytes_put_away = start_of_photo;

	if (camera->pl->bytes_read_from_camera > start_of_photo) {
		if(start_of_photo + b <= camera->pl->bytes_read_from_camera) {
			memcpy(pic_data, camera->pl->data_cache
					+ (start_of_photo % MAX_DLSIZE)
								, b);
			camera->pl->bytes_put_away += b;
			/*
			 * Photo data is contained in what is already
			 * downloaded.
			 * Jump immediately to process the photo.
			 */
		} else {
			/* Photo starts in one 0xfa00-sized download and ends
			 * in another */
			filled = camera->pl->bytes_read_from_camera
				    - start_of_photo;

			memcpy(pic_data, camera->pl->data_cache
					+ (start_of_photo % MAX_DLSIZE),
							filled);

			camera->pl->bytes_put_away += filled;
		}
	}
	while (camera->pl->bytes_put_away < start_of_photo + b ) {

		camera->pl->data_to_read = camera->pl->total_data_in_camera
			    - camera->pl->bytes_read_from_camera;
		downloadsize = MAX_DLSIZE;
		if (camera->pl->data_to_read < downloadsize)
			downloadsize = camera->pl->data_to_read;
		GP_DEBUG("downloadsize = 0x%x\n", downloadsize);
		if (downloadsize)
			jl2005c_read_data (
					    camera->port,
					    (char *) camera->pl->data_cache,
					    downloadsize);
		camera->pl->bytes_read_from_camera += downloadsize;

		if (camera->pl->bytes_read_from_camera >=
						start_of_photo + b ) {
			GP_DEBUG("THIS ONE?\n");
			memcpy(pic_data + filled, camera->pl->data_cache,
						b - filled);
			camera->pl->bytes_put_away += b - filled;
			break;
		} else {
			GP_DEBUG("THIS ONE??\n");
			if (!downloadsize)
				break;
			memcpy(pic_data + filled,
					camera->pl->data_cache, downloadsize);
			camera->pl->bytes_put_away += downloadsize;
			filled += downloadsize;
		}
	}

	if (type == GP_FILE_TYPE_RAW) {
		gp_file_set_mime_type(file, GP_MIME_RAW);
		gp_file_set_data_and_size(file, (char *)pic_buffer, b + 16);
		return GP_OK;
#ifdef HAVE_LIBJPEG
	} else if (type == GP_FILE_TYPE_PREVIEW) {
		if (!camera->pl->can_do_capture) {
			free (pic_buffer);
			return GP_ERROR_NOT_SUPPORTED;
		}
		outputsize = (pic_buffer[9] & 0xf0) * 192 + 256;
		GP_DEBUG("pic_buffer[9] is 0x%02x\n", pic_buffer[9]);
		GP_DEBUG("Thumbnail outputsize = 0x%x = %d\n", outputsize,
								outputsize);
		if (outputsize == 256) {
			GP_DEBUG("Frame %d has no thumbnail.\n", k);
			free (pic_buffer);
			return GP_OK;
		}
		pic_output = calloc(outputsize, 1);
		if (!pic_output) {
			free (pic_buffer);
			return GP_ERROR_NO_MEMORY;
		}
		outputsize = jl2005bcd_decompress(pic_output, pic_buffer,
								b + 16, 1);
		free (pic_buffer);
		if (outputsize < GP_OK) {
			free (pic_output);
			return outputsize;
		}
		GP_DEBUG("Thumbnail outputsize recalculated is 0x%x = %d\n",
						outputsize, outputsize);
		gp_file_set_mime_type(file, GP_MIME_PPM);
		gp_file_set_data_and_size(file, (char *)pic_output,
								outputsize);
	} else if (type == GP_FILE_TYPE_NORMAL) {
		outputsize = 3 * w * h + 256;
		pic_output = calloc(outputsize, 1);
		if (!pic_output) {
			free (pic_buffer);
			return GP_ERROR_NO_MEMORY;
		}
		outputsize = jl2005bcd_decompress(pic_output, pic_buffer,
								b + 16, 0);
		free (pic_buffer);
		if (outputsize < GP_OK) {
			free (pic_output);
			return outputsize;
		}
		gp_file_set_mime_type(file, GP_MIME_PPM);
		gp_file_set_data_and_size(file, (char *)pic_output,
								outputsize);
#endif
	} else {
		free (pic_buffer);
		return GP_ERROR_NOT_SUPPORTED;
	}

	return GP_OK;
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
                 GPContext *context)
{
	Camera *camera = data;

	GP_DEBUG(" * delete_all_func()");
	jl2005c_delete_all(camera, camera->port);

	return (GP_OK);
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
	.get_info_func = get_info_func,
	.delete_all_func = delete_all_func
};

int
camera_init (Camera *camera, GPContext *context)
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
	camera->pl->total_data_in_camera=0;
	camera->pl->data_to_read = 0;
	camera->pl->bytes_put_away = 0;
	camera->pl->data_reg_opened = 0;
	camera->pl->data_cache = NULL;
	camera->pl->init_done = 0;
	jl2005c_init (camera, camera->port, camera->pl);

	return GP_OK;
}
