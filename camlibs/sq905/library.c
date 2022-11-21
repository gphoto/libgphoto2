/* library.c
 *
 * Copyright (C) 2003 Theodore Kilgore <kilgota@auburn.edu>
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

#include "sq905.h"

#include <gphoto2/gphoto2-port.h>

#define GP_MODULE "sq905"

static const struct {
	char *name;
	CameraDriverStatus status;
	unsigned short idVendor;
	unsigned short idProduct;
} models[] = {
	{"SQ chip camera",    GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Argus DC-1510",     GP_DRIVER_STATUS_PRODUCTION,   0x2770, 0x9120},
	{"Gear to go",        GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Mitek CD10" ,       GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Mitek CD30P",       GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"GTW Electronics",   GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Concord Eye-Q Easy",GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Concord Eye-Q Duo", GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Che-ez Snap",       GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"PockCam",           GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Magpix B350",       GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Jenoptik JDC 350",  GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Precision Mini Digital Camera",
	                      GP_DRIVER_STATUS_PRODUCTION , 0x2770 , 0x9120},
	{"iConcepts digital camera" ,
                          GP_DRIVER_STATUS_PRODUCTION , 0x2770 , 0x9120},
	{"Sakar Kidz Cam 86379",    GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770,
								    0x9120},
	{"ViviCam3350",       GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"ViviCam5B",         GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"DC-N130ta",         GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"SY-2107C",          GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Shark SDC-513",     GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Shark SDC-519",     GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Shark 2-in-1 Mini", GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Argus DC-1730",     GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x913c},
	{"Request Ultra Slim",
	                      GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{"Global Point Splash Mini (underwater camera)",
	                      GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120},
	{NULL,0,0,0}
};

int
camera_id (CameraText *id)
{
	strcpy (id->text, "SQ chipset camera");

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
			a.operations = GP_OPERATION_CAPTURE_PREVIEW;
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
	sprintf (summary->text,_("Your USB camera has a S&Q chipset.\n"
				"The total number of pictures taken is %i\n"
				"Some of these could be clips containing\n"
				"several frames\n"),

				camera->pl->nb_entries);

	return GP_OK;
}

static int camera_manual (Camera *camera, CameraText *manual,
						    GPContext *context)
{
	strcpy(manual->text,
	_(
	"For cameras with S&Q Technologies chip.\n"
	"Should work with gtkam. Photos will be saved in PPM format.\n\n"

	"All known S&Q cameras have two resolution settings. What\n"
	"those are, will depend on your particular camera.\n"
	"A few of these cameras allow deletion of all photos. Most do not.\n"
	"Uploading of data to the camera is not supported.\n"
	"The photo compression mode found on many of the S&Q\n"
	"cameras is supported, to some extent.\n"
	"If present on the camera, video clips are seen as subfolders.\n"
	"Gtkam will download these separately. When clips are present\n"
	"on the camera, there is a little triangle before the name of\n"
	"the camera. If no folders are listed, click on the little \n"
	"triangle to make them appear. Click on a folder to enter it\n"
	"and see the frames in it, or to download them. The frames will\n"
	"be downloaded as separate photos, with special names which\n"
	"specify from which clip they came. Thus, you may freely \n"
	"choose to save clip frames in separate directories. or not.\n"
	));

	return (GP_OK);
}



static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("sq905 generic driver\n"
			    "Theodore Kilgore <kilgota@auburn.edu>\n"));

	return GP_OK;
}

/*************** File and Downloading Functions *******************/


static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
	Camera *camera = data;
	int i,n;
	GP_DEBUG ("List files in %s\n", folder);
	if (0==strcmp(folder, "/")) {
		for (i=0, n=0; i<camera->pl->nb_entries; i++) {
			if (!sq_is_clip(camera->pl, i)) n++;
		}
		gp_list_populate(list, "pict%03i.ppm", n);
	} else {
		char format[16];
		n = atoi(folder+1+4);	/* '/' + 'clip' */
		snprintf(format, sizeof(format), "%03i_%%03i.ppm", n);
		for (i=0; i<camera->pl->nb_entries && n>0; i++) {
			if (sq_is_clip(camera->pl, i)) n--;
		}
		i--;
		if (!sq_is_clip(camera->pl, i))
			return GP_ERROR_DIRECTORY_NOT_FOUND;
		gp_list_populate(list, format,
				    sq_get_num_frames(camera->pl, i));
	}
	return GP_OK;
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int i,n;
	GP_DEBUG ("List folders in %s\n", folder);
	if (0==strcmp(folder, "/")) {
		for (i=0, n=0; i<camera->pl->nb_entries; i++) {
			if (sq_is_clip(camera->pl, i)) n++;
		}
		gp_list_populate (list, "clip%03i", n);

	}
	return GP_OK;
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
		      CameraFileInfo *info, void *data, GPContext *context)
{
	memset (info, 0, sizeof(CameraFileInfo));
	/* FIXME: fill in some stuff? */
	return (GP_OK);
}


static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
	int i, w, h, b, entry, frame, is_in_clip;
	int nb_frames, to_fetch;
	int do_preprocess;
	unsigned char comp_ratio;
	unsigned char *frame_data, *rawdata;
	unsigned char *ppm, *ptr;
	unsigned char gtable[256];
	int size;
	int this_cam_tile;

	if (GP_FILE_TYPE_EXIF ==type) return GP_ERROR_FILE_EXISTS;

	if (GP_FILE_TYPE_RAW!=type && GP_FILE_TYPE_NORMAL
				    !=type && GP_FILE_TYPE_PREVIEW!=type) {

		return GP_ERROR_NOT_SUPPORTED;
	}


	/* Get the entry number of the photo on the camera */
	entry = -1;
	if (0==strcmp(folder, "/")) {
		i = atoi(filename+4);
		do {
			do entry++;
			while (sq_is_clip(camera->pl, entry)
					&& entry<camera->pl->nb_entries);
			i--;
		}
		while (i>0);
		if (entry == camera->pl->nb_entries)
			return GP_ERROR_FILE_NOT_FOUND;
		frame = 0;
		is_in_clip = 0;
	} else {
		i = atoi(folder+1+4);
		do {
			do entry++;
			while (!sq_is_clip(camera->pl, entry) &&
						entry<camera->pl->nb_entries);
			i--;
		} while (i>0);
		if (entry == camera->pl->nb_entries)
			return GP_ERROR_DIRECTORY_NOT_FOUND;
		frame = atoi(filename+4)-1;
		if (frame >= sq_get_num_frames(camera->pl, entry))
			return GP_ERROR_FILE_NOT_FOUND;
		is_in_clip = 1;
	}

	GP_DEBUG ("Download file %s from %s, entry = %d, frame = %d\n",
					    filename, folder, entry, frame);

	/* Fetch entries until the one we need, and toss all before
	 * the one we need.
	 * TODO: Either find out how to use the location info in the catalog
	 * to download just the entry needed, or show it is as impossible as
	 * it seems to be.
	 */

	GP_DEBUG ("last entry was %d\n", camera->pl->last_fetched_entry);

	/* Change register to DATA, but only if necessary */
	if ((camera->pl->last_fetched_entry == -1)
	|| ((is_in_clip) && (frame == 0)) )

	sq_access_reg(camera->port, DATA);

	if (camera->pl->last_fetched_entry > entry) {
		sq_rewind(camera->port, camera->pl);
	}
	do_preprocess = 0;
	do {
		to_fetch = camera->pl->last_fetched_entry;
		if (to_fetch < entry) {
			to_fetch++;
			free(camera->pl->last_fetched_data);
			camera->pl->last_fetched_data = NULL;
		}
		nb_frames = sq_get_num_frames(camera->pl, to_fetch);
		comp_ratio = sq_get_comp_ratio (camera->pl, to_fetch);
		w = sq_get_picture_width (camera->pl, to_fetch);
		switch (w) {
		case 176: h = 144; break;
		case 640: h = 480; break;
		case 320: h = 240; break;
		default:  h = 288; break;
		}
		if (!comp_ratio) {
			sq_rewind(camera->port, camera->pl);
			return GP_ERROR;
		}
		b = nb_frames * w * h / comp_ratio;
		do_preprocess = 1;
		if (camera->pl->last_fetched_data) break;

		camera->pl->last_fetched_data = malloc ((long)nb_frames*w*h);
		if (!camera->pl->last_fetched_data) {
			sq_rewind(camera->port, camera->pl);
			return GP_ERROR_NO_MEMORY;
		}
		GP_DEBUG("Fetch entry %i\n", to_fetch);
		sq_read_picture_data
			    (camera->port, camera->pl->last_fetched_data, b);
		camera->pl->last_fetched_entry = to_fetch;
	} while (camera->pl->last_fetched_entry<entry);

	frame_data = camera->pl->last_fetched_data+(w*h)*frame/comp_ratio;
	/* sq_preprocess ( ) turns the photo right-side-up and for some
	 * models must also de-mirror the photo
	 */

	if (GP_FILE_TYPE_RAW!=type) {

		if (do_preprocess) {
			sq_preprocess(camera->pl->model, comp_ratio,
					is_in_clip, frame_data, w, h);
		}

		/*
		 * Now put the data into a PPM image file.
		 */
		ppm = malloc (w * h * 3 + 256); /* room for data + header */
		if (!ppm) { return GP_ERROR_NO_MEMORY; }
		sprintf ((char *)ppm,
			"P6\n"
			"# CREATOR: gphoto2, SQ905 library\n"
			"%d %d\n"
			"255\n", w, h);
		size = strlen ((char *)ppm);
		ptr = ppm + size;

			switch (camera->pl->model) {
			case SQ_MODEL_POCK_CAM:
			case SQ_MODEL_MAGPIX:
				this_cam_tile = BAYER_TILE_GBRG;
				break;
			default:
				this_cam_tile = BAYER_TILE_BGGR;
				break;
			}
		size = size + (w * h * 3);
		GP_DEBUG ("size = %i\n", size);
		if (comp_ratio>1) {
			rawdata = malloc ((long)w*h);
			if (!rawdata) {
				free (ppm);
				return GP_ERROR_NO_MEMORY;
			}
			sq_decompress (camera->pl->model, rawdata,
						frame_data, w, h);
			gp_gamma_fill_table (gtable, .65);
		} else {
			rawdata = frame_data;
			gp_gamma_fill_table (gtable, .55);
		}
		gp_ahd_decode (rawdata, w , h , ptr, this_cam_tile);
		gp_gamma_correct_single (gtable, ptr, w * h);

		gp_file_set_mime_type (file, GP_MIME_PPM);
		gp_file_set_data_and_size (file, (char *)ppm, size);
		if (rawdata != frame_data) free (rawdata);

	} else {	/* type is GP_FILE_TYPE_RAW */
		size = w*h/comp_ratio;
		rawdata = malloc (size+16);
		if (!rawdata) return GP_ERROR_NO_MEMORY;
		memcpy (rawdata, frame_data, size);
		memcpy (rawdata+size,camera->pl->catalog+16*entry,16);
		gp_file_set_mime_type (file, GP_MIME_RAW);
	        gp_file_set_data_and_size (file, (char *)rawdata, size+16);
	}
	/* Reset camera when done, for more graceful exit. */
	if ((!(is_in_clip)&&(entry +1 == camera->pl->nb_entries))
	|| ((is_in_clip)&& (frame +1 == nb_frames )))
		sq_reset (camera->port);

	return GP_OK;
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;

	GP_DEBUG(" * delete_all_func()");
	sq_delete_all(camera->port, camera->pl);

	return (GP_OK);
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	unsigned char *frame_data;
	unsigned char *ppm, *ptr;
	unsigned char gtable[256];
	int size;
	int w = 320;
	int h = 240;
	int b=0x12c40;

	camera->pl->last_fetched_data = malloc (b);
	if (!camera->pl->last_fetched_data) {
		sq_rewind(camera->port, camera->pl);
		return GP_ERROR_NO_MEMORY;
	}

        sq_access_reg(camera->port, CAPTURE);
	sq_read_picture_data (camera->port, camera->pl->last_fetched_data, b);
	frame_data = camera->pl->last_fetched_data + 0x40;
	sq_preprocess(camera->pl->model, 1, 0, frame_data, w, h);

	/* Now put the data into a PPM image file. */
	ppm = malloc (w * h * 3 + 256);
	if (!ppm)
		return GP_ERROR_NO_MEMORY;
	sprintf ((char *)ppm,
		"P6\n"
		"# CREATOR: gphoto2, SQ905 library\n"
		"%d %d\n"
		"255\n", w, h);
	ptr = ppm + strlen ((char*)ppm);
	size = strlen ((char*)ppm) + (w * h * 3);
	GP_DEBUG ("size = %i\n", size);
	switch (camera->pl->model) {
	case SQ_MODEL_POCK_CAM:
		gp_bayer_decode (frame_data, w , h , ptr, BAYER_TILE_GBRG);
		break;
	default:
		gp_bayer_decode (frame_data, w , h , ptr, BAYER_TILE_BGGR);
		break;
	}

	/* TO DO:
	 * Adapt some postprocessing routine to work here, because results
	 * can vary greatly, depending both on lighting conditions and on
	 * camera model.
	 */

	gp_gamma_fill_table (gtable, .5);
	gp_gamma_correct_single (gtable, ptr, w * h);
	gp_file_set_mime_type (file, GP_MIME_PPM);
	gp_file_set_data_and_size (file, (char *)ppm, size);

	sq_reset(camera->port);
        sq_access_reg(camera->port, CAPTURE);
	sq_reset(camera->port);

	return (GP_OK);
}

/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("SQ camera_exit");
	sq_reset (camera->port);

	if (camera->pl) {
		free (camera->pl->catalog);
		free (camera->pl->last_fetched_data);
		free (camera->pl);
		camera->pl = NULL;
	}

	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func,
	.delete_all_func = delete_all_func,
};
int
camera_init(Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	int ret = 0;

	/* First, set up all the function pointers */
	camera->functions->summary	= camera_summary;
	camera->functions->manual	= camera_manual;
	camera->functions->about	= camera_about;
	camera->functions->capture_preview
					= camera_capture_preview;
	camera->functions->exit		= camera_exit;

	GP_DEBUG ("Initializing the camera\n");

	ret = gp_port_get_settings(camera->port,&settings);
	if (ret < 0) return ret;

	ret = gp_port_set_settings(camera->port,settings);
	if (ret < 0) return ret;

	/* Tell the CameraFilesystem where to get lists from */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;
	camera->pl->model = 0;
	camera->pl->catalog = NULL;
	camera->pl->nb_entries = 0;
	camera->pl->last_fetched_entry = -1;
	camera->pl->last_fetched_data = NULL;

	/* Connect to the camera */
	ret = sq_init (camera->port, camera->pl);
	if (ret != GP_OK) {
		free(camera->pl);
		return ret;
	};

	return GP_OK;
}
