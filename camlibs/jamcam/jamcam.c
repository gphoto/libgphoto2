/****************************************************************/
/* jamcam.c - Gphoto2 library for the KBGear JamCam v2 and v3   */
/*                                                              */
/* Copyright 2001 Chris Pinkham                                 */
/*                                                              */
/* Author: Chris Pinkham <cpinkham@infi.net>                    */
/*                                                              */
/* This library is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU Library General  */
/* Public License as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any     */
/* later version.                                               */
/*                                                              */
/* This library is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU Library General Public License for     */
/* more details.                                                */
/*                                                              */
/* You should have received a copy of the GNU Library General   */
/* Public License along with this library; if not, write to the */
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2.h>
#include <bayer.h>
#include <gamma.h>

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

#include "library.h"

#define GP_MODULE "jamcam"
#define TIMEOUT	      2000

#define JAMCAM_VERSION "0.6"
#define JAMCAM_LAST_MOD "11/28/2001 14:51 EST"

/* define what cameras we support */
static const struct {
	char *model;
	int usb_vendor;
	int usb_product;
} models[] = {
	{ "KBGear:JamCam", 0x084E, 0x0001 },
	{ NULL, 0, 0 }
};


int camera_id (CameraText *id)
{
	/* GP_DEBUG ("* camera_id"); */

	strcpy (id->text, "jamcam");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list)
{
	int x = 0;
	char *ptr;
	CameraAbilities a;

	/* GP_DEBUG ("* camera_abilities"); */

	ptr = models[x].model;
	while (ptr) {
		memset(&a, 0, sizeof(a));
		strcpy (a.model, ptr );
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port     = GP_PORT_SERIAL | GP_PORT_USB;
		a.speed[0] = 57600;
		a.speed[1] = 0;

		/* fixme, need to set these operations lists to correct values */
		a.operations        = 	GP_OPERATION_NONE;
		a.file_operations   = 	GP_FILE_OPERATION_PREVIEW;

		a.folder_operations = 	GP_FOLDER_OPERATION_NONE;

		a.usb_vendor = models[x].usb_vendor;
		a.usb_product = models[x].usb_product;

		gp_abilities_list_append (list, a);

		ptr = models[++x].model;
	}

	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context)
{
	Camera *camera = data;
	int count;

	GP_DEBUG ("* file_list_func");
	GP_DEBUG ("*** folder: %s", folder);

	CHECK (count = jamcam_file_count (camera));
	CHECK (gp_list_populate (list, "pic_%04i.ppm", count));

	return (GP_OK);
}

static int get_info_func (CameraFilesystem *fs, const char *folder,
		const char *filename, CameraFileInfo *info, void *data,
		GPContext *context)
{
	Camera *camera = data;
	int n;
	struct jamcam_file *jc_file;

	GP_DEBUG ("* get_info_func");
	GP_DEBUG ("*** folder: %s", folder);
	GP_DEBUG ("*** filename: %s",filename);

	/* Get the file number from the CameraFileSystem */
	CHECK (n = gp_filesystem_number (camera->fs, folder,
					 filename, context));

	jc_file = jamcam_file_info( camera, n );

	/* fixme, get file size also */
	info->file.fields = GP_FILE_INFO_TYPE;
	strcpy (info->file.type, GP_MIME_PPM);
	info->file.width = jc_file->width;
	info->file.height = jc_file->height;

	info->preview.fields = GP_FILE_INFO_TYPE;
	strcpy (info->preview.type, GP_MIME_PPM);
	info->preview.width = 80;
	info->preview.height = 60;

	return (GP_OK);
}

static int camera_exit (Camera *camera, GPContext *context)
{
	GP_DEBUG ("* camera_exit");

	jamcam_file_count (camera);
	return (GP_OK);
}

#define CHECK_free(result) {		\
	int res;			\
	res = result; 			\
	if (res < 0)  { 		\
		free(raw); free(ppm);	\
		return (res);		\
	}				\
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
	char *raw, *ppm;
	unsigned char gtable[256];
	char *ptr;
	int size = 0, n = 0;
	int width, height;
	struct jamcam_file *jc_file;

	GP_DEBUG ("* camera_file_get");
	GP_DEBUG ("*** folder: %s", folder);
	GP_DEBUG ("*** filename: %s",filename);
	GP_DEBUG ("*** type: %d", type);

	CHECK (n = gp_filesystem_number (camera->fs, folder,
					 filename, context));

	if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
		return (GP_ERROR_CANCEL);

	raw = malloc(640*480 * 3);
	ppm = malloc(640*480 * 3 + 200);
	
	switch (type) {
	case GP_FILE_TYPE_PREVIEW:

		CHECK_free (jamcam_request_thumbnail (camera, file, raw, &size, n, context));


		width = 80;
		height = 60;

		sprintf( ppm,
			"P6\n"
			"# CREATOR: gphoto2, jamcam library\n"
			"%d %d\n"
			"255\n", width, height );

		ptr = ppm + strlen( ppm );

		size = strlen( ppm ) + ( height * width * 3 );

		gp_bayer_decode((unsigned char *)raw, width, height,
				(unsigned char *)ptr, BAYER_TILE_GBRG );
		gp_gamma_fill_table( gtable, 0.5 );
		gp_gamma_correct_single( gtable, (unsigned char *)ptr, height * width );

		CHECK_free (gp_file_set_mime_type (file, GP_MIME_PPM));
		CHECK_free (gp_file_append (file, ppm, size));
		break;

	case GP_FILE_TYPE_NORMAL:
		CHECK_free (jamcam_request_image (camera, file, raw, &size, n, context));

		jc_file = jamcam_file_info (camera, n);

		sprintf( ppm,
			"P6\n"
			"# CREATOR: gphoto2, jamcam library\n"
			"%d %d\n"
			"255\n", jc_file->width, jc_file->height );

		ptr = ppm + strlen( ppm );

		size = strlen( ppm ) + ( jc_file->width * jc_file->height * 3 );

		gp_bayer_decode((unsigned char *)raw, jc_file->width, jc_file->height,
				(unsigned char *)ptr, BAYER_TILE_GBRG );
		gp_gamma_fill_table( gtable, 0.5 );
		gp_gamma_correct_single( gtable, (unsigned char *)ptr, jc_file->width * jc_file->height );

		CHECK_free (gp_file_set_mime_type (file, GP_MIME_PPM));
		CHECK_free (gp_file_append (file, ppm, size));
		break;

	case GP_FILE_TYPE_RAW:
		CHECK_free (jamcam_request_image (camera, file, raw, &size, n, context));
		CHECK_free (gp_file_set_mime_type (file, GP_MIME_RAW));
		CHECK_free (gp_file_append (file, raw, size));
		break;
	default:
		free(raw); free(ppm);
		return (GP_ERROR_NOT_SUPPORTED);
	}
	free(raw); free(ppm);
	return (GP_OK);
}

static int camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	int count;
	char tmp[1024];

	GP_DEBUG ("* camera_summary");

	/* possibly get # pics, mem free, etc. */
	count = jamcam_file_count (camera);

	sprintf (tmp, _("Frames Taken     : %4d\n"), count);
	strcat (summary->text, tmp );

	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	GP_DEBUG ("* camera_about");

	strcpy (about->text,
		_("jamcam library v" JAMCAM_VERSION
		" " JAMCAM_LAST_MOD "\n"
		"Chris Pinkham <cpinkham@infi.net>\n"
		"Support for KBGear JamCam v2.0 & v3.0 digital cameras\n"
		"based on reverse engineering serial protocol.\n"
		"\n"));

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func,
};

int camera_init (Camera *camera, GPContext *context)
{
	int count;
	GPPortSettings settings;

	GP_DEBUG ("* camera_init");
	GP_DEBUG ("   * jamcam library for Gphoto2 by Chris Pinkham <cpinkham@infi.net>");
	GP_DEBUG ("   * jamcam library v%s, %s", JAMCAM_VERSION, JAMCAM_LAST_MOD );

	/* First, set up all the function pointers */
	camera->functions->exit 	= camera_exit;
	camera->functions->summary	= camera_summary;
	camera->functions->about 	= camera_about;

	CHECK (gp_port_get_settings (camera->port, &settings));
	switch( camera->port->type ) {
	case GP_PORT_SERIAL:
		settings.serial.speed 	 = 57600;
		settings.serial.bits 	 = 8;
		settings.serial.parity 	 = 0;
		settings.serial.stopbits = 1;
		break;
	case GP_PORT_USB:
		/* Use the defaults the core parsed */
		break;
	default:
		fprintf( stderr, "Unknown port type: %d\n", camera->port->type );
		return ( GP_ERROR );
		break;
	}
	CHECK (gp_port_set_settings (camera->port, settings));
	CHECK (gp_port_set_timeout (camera->port, TIMEOUT));

	/* check to see if camera is really there */
	CHECK (jamcam_enq (camera));

	/* get number of images */
	CHECK (count = jamcam_file_count (camera));

	/* Set up the CameraFilesystem */
	return gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
}

