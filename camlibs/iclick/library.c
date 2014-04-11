/* library.c
 *
 * Copyright (C) 2004 Theodore Kilgore <kilgota@auburn.edu>,
 * Stephen Pollei <stephen_pollei@comcast.net>.
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

#define _BSD_SOURCE

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
#endif

#include "iclick.h"

#include <gphoto2/gphoto2-port.h>

#define GP_MODULE "iclick"

static struct {
   	char *name;
	CameraDriverStatus status;
   	unsigned short idVendor;
   	unsigned short idProduct;
} models[] = {
        {"iClick 5X",    GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9153},
	{NULL,0,0}
};

int
camera_id (CameraText *id)
{
    	strcpy (id->text, "iClick 5X");

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
		a.file_operations   = GP_FILE_OPERATION_PREVIEW + GP_FILE_OPERATION_RAW;
       		gp_abilities_list_append (list, a);
    	}

    	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
    	sprintf (summary->text,_("Your USB camera is an iClick 5X.\n"
				"The total number of pictures taken is %i\n"),

				camera->pl->nb_entries);

    	return GP_OK;
}

static int camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	strcpy(manual->text, 	
	_(
	"Information regarding cameras with ID 0x2770:0x9153.\n\n"
	"We do not recommend the use of a GUI program to access\n"
	"this camera, unless you are just having fun or trying to\n"
	"see if you can blow a fuse.\n"
	"For production use, try\n"
	"gphoto2 -P\n"
	"from the command line.\n"
	"Note: it is not possible to download video clips.\n")
	);

	return (GP_OK);
}



static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
    	strcpy (about->text, _("iClick 5X driver\n"
			    "Theodore Kilgore <kilgota@auburn.edu>\n"));

    	return GP_OK;
}

/*************** File and Downloading Functions *******************/


static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
        Camera *camera = data;
	int i;
	unsigned char buf[1024];
	GP_DEBUG ("List files in %s\n", folder);

	gp_list_reset (list);
	for (i = 0; i < camera->pl->nb_entries; i++) {
		snprintf((char *)buf, sizeof(buf), "img%03i.ppm", i + 1);
		gp_list_append (list, (char *)buf, NULL);
	}

	return GP_OK;
}




static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
    	Camera *camera = user_data;
	int entry, w, h; /* frame; */
	unsigned char *frame_data, *frame_ptr;
	unsigned char *ppm, *ptr;
	unsigned char gtable[256];
	int start;
	int datasize, hdrsize, ppmsize;
	int nb_frames=1;
	unsigned char buf[0x8000];

#if 0	/* libgphoto2 likes to search in vain for EXIF data, which isn't there.
	   It's a waste of time, and there should be a way to disable it
	   explicitly.  But there isn't.  Un-if this if you don't like it,
	   and you can still retrieve the first frame of a video via the
	   frame filename instead. */
	if (GP_FILE_TYPE_PREVIEW==type)
		return GP_ERROR_NOT_SUPPORTED;
#endif

	if (GP_FILE_TYPE_RAW!=type && GP_FILE_TYPE_NORMAL!=type
		&& GP_FILE_TYPE_PREVIEW!=type) {

		return GP_ERROR_NOT_SUPPORTED;
	}

	/* Get the entry number of the photo on the camera */
	entry = gp_filesystem_number (camera->fs, folder, filename, context);
	if (entry < GP_OK)
		return GP_ERROR_FILE_NOT_FOUND;

	GP_DEBUG ("Download file %s, entry = %d\n",
			filename, entry);

	if (entry >= camera->pl->nb_entries)
		return GP_ERROR_FILE_NOT_FOUND;

	/* Fetch entries until the one we need, and toss all but the one we need.
	 * TODO: Either find out how to use the location info in the catalog to
	 * download just the entry needed, or show it is as impossible as it seems.
	 */

	/* Change register to DATA, but only if necessary */
	if (camera->pl->data_offset == -1) {
		icl_access_reg(camera->port, DATA);

		/* Camera starts at the first picture.. */
		camera->pl->data_offset = icl_get_start (camera->pl, 0);
	}

	start = icl_get_start (camera->pl, entry);
	datasize = icl_get_size (camera->pl, entry);
	/* datasize exceeds the actual datasize by 0x100 bytes, which seems to be
	 * 0x100 bytes of filler at the beginning. For now we will treat this extra
	 * 0x100 bytes as junk and just ditch it.
	 */

	GP_DEBUG ("data offset at %d, picture at %d\n", camera->pl->data_offset, start);

	/* Rewind if we're past the requested picture */
	if (camera->pl->data_offset > start) {
		icl_rewind(camera->port, camera->pl);
	}

	/* Seek to the requested picture */
	while ((camera->pl->data_offset + 0x8000) < start) {
		icl_read_picture_data(camera->port, buf, 0x8000);
		camera->pl->data_offset += 0x8000;
	}
	if (camera->pl->data_offset < start) {
		icl_read_picture_data(camera->port, buf, start - camera->pl->data_offset);
		camera->pl->data_offset = start;
	}

	frame_data = malloc(datasize);
	if (!frame_data) return GP_ERROR_NO_MEMORY;
	icl_read_picture_data(camera->port, frame_data, datasize);
	camera->pl->data_offset += datasize;

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		if (icl_get_width_height (camera->pl, entry, &w, &h) >= GP_OK)
			break; /* Known format, process image */
		/* No previewing of raw data */
		free (frame_data);
		return GP_ERROR_NOT_SUPPORTED;
	case GP_FILE_TYPE_NORMAL:
		if (icl_get_width_height (camera->pl, entry, &w, &h) >= GP_OK)
			break; /* Known format, process image */
		/* Unsupported format, fallthrough to raw */
	case GP_FILE_TYPE_RAW:
		gp_file_set_mime_type (file, GP_MIME_RAW);
		gp_file_adjust_name_for_mime_type (file);
	        gp_file_set_data_and_size (file, (char *)frame_data, datasize);
		return (GP_OK);
	default:
		return GP_ERROR_NOT_SUPPORTED;
	}

	/* Write the frame(s) */
	snprintf((char *)buf, sizeof(buf),
		"P6\n"
		"# CREATOR: gphoto2, iClick library\n"
		"%d %d\n"
		"255\n", w, h);
	hdrsize = strlen((char *)buf);

	ppmsize = (hdrsize + w*h*3) * nb_frames;
	GP_DEBUG ("ppmsize = %i\n", ppmsize);

	ptr = ppm = malloc(ppmsize);

	frame_ptr = frame_data + 0x100;
	/* Here, we just threw away the "superfluous" first 0x100 bytes */
	memcpy(ptr, buf, hdrsize);
	ptr += hdrsize;

	gp_bayer_decode (frame_ptr, w , h , ptr, BAYER_TILE_GBRG);

	gp_gamma_fill_table (gtable, .5);
	/* The gamma factor is pure guesswork; shooting in the dark. This
	 * is the kind of thing which might be hidden in those 0x100 bytes.
	 */

	gp_gamma_correct_single (gtable, ptr, w * h);

	ptr += w*h*3;

	gp_file_set_mime_type (file, GP_MIME_PPM);
	gp_file_set_data_and_size (file, (char *)ppm, ppmsize);
	free (frame_data);
        return GP_OK;
}


/*************** Exit and Initialization Functions ******************/

static int
camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("iClick camera_exit");

	if (camera->pl->data_offset != -1)
		icl_rewind (camera->port, camera->pl);
	icl_reset (camera->port);

	if (camera->pl) {
		free (camera->pl->catalog);
		free (camera->pl);
		camera->pl = NULL;
	}

	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func
};

int
camera_init(Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	int ret = 0;

	/* First, set up all the function pointers */
	camera->functions->summary      = camera_summary;
        camera->functions->manual	= camera_manual;
	camera->functions->about        = camera_about;
	camera->functions->exit	    	= camera_exit;

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
	camera->pl->data_offset = -1;

	/* Connect to the camera */
	ret = icl_init (camera->port, camera->pl);
	if (ret != GP_OK) {
		free(camera->pl);
		return ret;
	}
	return GP_OK;
}
