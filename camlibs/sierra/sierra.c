/* sierra.c:
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sourceforge.net>
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

#include "config.h"
#include "sierra.h"

#include <stdlib.h>
#include <_stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>

#include "sierra-desc.h"
#include "library.h"

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
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define GP_MODULE "sierra"

static int get_jpeg_data(const char *data, int data_size, char **jpeg_data, int *jpeg_size);

/* Useful markers */
static const char JPEG_SOI_MARKER[]  = { (char)0xFF, (char)0xD8, '\0' };
static const char JPEG_SOF_MARKER[]  = { (char)0xFF, (char)0xD9, '\0' };
static const char JPEG_APP1_MARKER[] = { (char)0xFF, (char)0xE1, '\0' };
static const char TIFF_SOI_MARKER[]  = { (char)0x49, (char)0x49, (char)0x2A, (char)0x00, (char)0x08, '\0' };

static struct {
	const char *manuf;
	const char *model;
	SierraModel sierra_model;
	int  usb_vendor;
	int  usb_product;
	SierraFlags  flags;
	struct CameraDesc const *cam_desc;
} sierra_cameras[] = {
	/* Manufacturer, Camera Model, Sierra Model,
	   USBvendor id, USB product id,
	   USB wrapper protocol, CameraDesc) */ 
	{"Agfa", "ePhoto 307", 	SIERRA_MODEL_DEFAULT, 	0, 0,
		SIERRA_NO_51, NULL},
	{"Agfa", "ePhoto 780", 	SIERRA_MODEL_DEFAULT, 	0, 0, 0, NULL },
	{"Agfa", "ePhoto 780C",	SIERRA_MODEL_DEFAULT,	0, 0,
		SIERRA_NO_51 | SIERRA_LOW_SPEED, NULL },
	{"Agfa", "ePhoto 1280",	SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Agfa", "ePhoto 1680", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	/* unclear, if driven by fuji driver or by sierra driver */
	{"Apple", "QuickTake 200 (Sierra Mode)", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Chinon", "ES-1000", 	SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Epson", "PhotoPC 500", SIERRA_MODEL_EPSON,	0, 0, 0, NULL },
	{"Epson", "PhotoPC 550", SIERRA_MODEL_EPSON,	0, 0, 0, NULL },
	{"Epson", "PhotoPC 600", SIERRA_MODEL_EPSON,	0, 0, 0, NULL },
	{"Epson", "PhotoPC 650", SIERRA_MODEL_EPSON,	0, 0,
					SIERRA_NO_51 | SIERRA_LOW_SPEED, NULL },
	{"Epson", "PhotoPC 700", SIERRA_MODEL_EPSON,	0, 0, SIERRA_NO_51, NULL },
	{"Epson", "PhotoPC 800", SIERRA_MODEL_EPSON,	0, 0, 0, NULL },
	{"Epson", "PhotoPC 850z", SIERRA_MODEL_EPSON,     0x4b8, 0x402, 0, NULL},
	{"Epson", "PhotoPC 3000z", SIERRA_MODEL_CAM_DESC,	0x4b8, 0x403, 0, &ep3000z_cam_desc},
	{"Nikon", "CoolPix 100", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Nikon", "CoolPix 300", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Nikon", "CoolPix 700", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Nikon", "CoolPix 800", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Nikon", "CoolPix 880",SIERRA_MODEL_CAM_DESC,	0x04b0, 0x0103, 0,
							&cp880_cam_desc},
	{"Nikon", "CoolPix 900", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Nikon", "CoolPix 900S", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Nikon", "CoolPix 910", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Nikon", "CoolPix 950", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Nikon", "CoolPix 950S", SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Nikon", "CoolPix 990", SIERRA_MODEL_DEFAULT,	0x04b0, 0x0102, 0, NULL},
	{"Nikon", "CoolPix 995",   SIERRA_MODEL_CAM_DESC, 0x04b0, 0x0104,
						SIERRA_WRAP_USB_OLYMPUS,
							&cp995_cam_desc},

	{"Nikon", "CoolPix 2500 (Sierra Mode)", SIERRA_MODEL_CAM_DESC, 0x04b0, 0x0108,
						SIERRA_WRAP_USB_NIKON | SIERRA_NO_51,
							&cp2500_cam_desc},

	{"Nikon", "CoolPix 2100 (Sierra Mode)", SIERRA_MODEL_CAM_DESC, 0x04b0, 0x0116,
						SIERRA_WRAP_USB_NIKON | SIERRA_NO_51,
							&cp2500_cam_desc},

	{"Nikon", "CoolPix 4300 (Sierra Mode)", SIERRA_MODEL_CAM_DESC, 0x04b0, 0x010e,
						SIERRA_WRAP_USB_NIKON | SIERRA_NO_51,
							&cp4300_cam_desc},
	{"Nikon", "CoolPix 3500 (Sierra Mode)", SIERRA_MODEL_CAM_DESC, 0x04b0, 0x0110,
						SIERRA_WRAP_USB_NIKON | SIERRA_NO_51,
							&cp880_cam_desc},
	{"Nikon", "D100 (Sierra Mode)", SIERRA_MODEL_CAM_DESC, 0x04b0, 0x0401,
						SIERRA_WRAP_USB_NIKON | SIERRA_NO_51,
							&cp880_cam_desc},
	{"Olympus", "D-100Z", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-200L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-220L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-300L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-320L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-330R", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-340L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-340R", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-360L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-400L Zoom", SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-450Z", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-460Z", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-500L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-600L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-600XL", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "D-620L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-400", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-400L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-410", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-410L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-420", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-420L", 	SIERRA_MODEL_OLYMPUS,	0, 0, SIERRA_MID_SPEED, NULL },
	{"Olympus", "C-700UZ",	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x105,
					SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
	{"Olympus", "C-750UZ",	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x105,
					SIERRA_WRAP_USB_OLYMPUS, &oly750uz_cam_desc},
	{"Olympus", "C-770UZ",	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x105,
					SIERRA_WRAP_USB_OLYMPUS, &oly750uz_cam_desc},
	{"Olympus", "C-800", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-800L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-820", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-820L", 	SIERRA_MODEL_OLYMPUS,	0, 0,
					SIERRA_MID_SPEED, NULL },
	{"Olympus", "C-830L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-840L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-860L",	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-900 Zoom", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-900L Zoom", SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },

	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1082569&group_id=8874&atid=358874 
	 * strangely only works with low speed
	 */
	{"Olympus", "C-990 Zoom", 	SIERRA_MODEL_OLYMPUS,	0, 0, SIERRA_LOW_SPEED, NULL },
	{"Olympus", "C-1000L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-1400L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-1400XL", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-2000Z", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-2020Z",	SIERRA_MODEL_OLYMPUS,	0, 0, 0, NULL },
	{"Olympus", "C-2040Z", 	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x105,
					SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
	{"Olympus", "C-2100UZ", SIERRA_MODEL_CAM_DESC, 0x07b4, 0x100, 0,
					&oly3040_cam_desc},
	{"Olympus", "C-2500L",     SIERRA_MODEL_OLYMPUS,   0, 0, 0, NULL },
	/*
	 * Note: The 3000z can't be auto selected, as it has the same USB
	 * id as the 2040 (and others, but the others are still using the
	 * the 3040 desc), so you have to set the camera "model" via the
	 * front end (gtkam or gphoto2 command line) in order to use it.
	 */
	{"Olympus", "C-3000Z", 	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x100, 0,
							&oly3000z_cam_desc},
	{"Olympus", "C-3020Z", SIERRA_MODEL_CAM_DESC,  0x07b4, 0x105,
					SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
	{"Olympus", "C-3030Z", SIERRA_MODEL_CAM_DESC,	0x07b4, 0x100, 0,
							&oly3040_cam_desc},
	{"Olympus", "C-3040Z",     SIERRA_MODEL_CAM_DESC,  0x07b4, 0x105,
					SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
	{"Olympus", "C-4040Z",     SIERRA_MODEL_CAM_DESC,  0x07b4, 0x105,
					SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
	{"Olympus", "C-5050Z", 	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x105,
					SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
	{"Olympus", "SP-500UZ", 	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x109,
				SIERRA_WRAP_USB_OLYMPUS, &olysp500uz_cam_desc},
	{"Olympus", "C-370Z", 	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x109,
				SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
	{"Olympus", "X-450", 	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x109,
				SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
	{"Olympus", "D-535Z", 	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x109,
				SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
	{"Olympus", "fe-200", 	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x109,
				SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
/*
	{"Olympus", "E-520", 	SIERRA_MODEL_CAM_DESC,	0x07b4, 0x110,
				SIERRA_WRAP_USB_OLYMPUS, &oly3040_cam_desc},
*/
	{"Panasonic", "Coolshot NV-DCF5E", SIERRA_MODEL_DEFAULT, 0, 0, 0, NULL },

	{"Pentax", "Optio 450", SIERRA_MODEL_DEFAULT, 0x0a17,0x0007, SIERRA_WRAP_USB_PENTAX, NULL },
	{"Pentax", "Optio 33WR", SIERRA_MODEL_DEFAULT, 0x0a17,0x0009, SIERRA_WRAP_USB_PENTAX, NULL },
	/* has only very bare reactions
	{"Pentax", "Optio W90 (Sierra Mode)", SIERRA_MODEL_DEFAULT, 0x0a17,0x00f6, SIERRA_WRAP_USB_PENTAX, NULL },
	 */

	{"Polaroid", "PDC 640", 	SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Polaroid", "PDC 2300Z", SIERRA_MODEL_DEFAULT, 0x0546, 0x0daf,
					SIERRA_SKIP_INIT, NULL },
	{"Sanyo", "DSC-X300", 	SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Sanyo", "DSC-X350", 	SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Sanyo", "VPC-G200", 	SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Sanyo", "VPC-G200EX", 	SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Sanyo", "VPC-G210", 	SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	{"Sanyo", "VPC-G250", 	SIERRA_MODEL_DEFAULT,	0, 0, SIERRA_NO_51, NULL },
	{"Sierra Imaging", "SD640",SIERRA_MODEL_DEFAULT,	0, 0, 0, NULL },
	/*
	 * Added by Sean Bruno on July 22, 2004 to support the Toshiba
	 * cameras below.
	 */
	{"Toshiba", "PDR-M60", SIERRA_MODEL_DEFAULT, 0x1132, 0x4332,
		SIERRA_NO_51 | SIERRA_NO_REGISTER_40 | SIERRA_NO_USB_CLEAR,
		NULL},
	{"Toshiba", "PDR-M61", SIERRA_MODEL_DEFAULT, 0x1132, 0x4335,
		SIERRA_NO_51 | SIERRA_NO_REGISTER_40 | SIERRA_NO_USB_CLEAR,
		NULL},
	{"Toshiba", "PDR-M65", SIERRA_MODEL_DEFAULT, 0x1132, 0x4334,
		SIERRA_NO_51 | SIERRA_NO_REGISTER_40 | SIERRA_NO_USB_CLEAR,
		NULL},
	{NULL, NULL, 0,	0, 0, 0, NULL}
};

static struct {
	SierraSpeed speed;
	int bit_rate;
} SierraSpeeds[] = {
	{SIERRA_SPEED_9600  ,   9600},
	{SIERRA_SPEED_19200 ,  19200},
	{SIERRA_SPEED_38400 ,  38400},
	{SIERRA_SPEED_57600 ,  57600},
	{SIERRA_SPEED_115200, 115200},
	{0, 0}
};

int camera_id (CameraText *id) 
{
	strcpy(id->text, "sierra");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) 
{
	int x;
	CameraAbilities a;

	for (x = 0; sierra_cameras[x].manuf; x++) {
		memset(&a, 0, sizeof(a));
		strcpy (a.model, sierra_cameras[x].manuf);
		strcat (a.model, ":");
		strcat (a.model, sierra_cameras[x].model);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port     = GP_PORT_SERIAL;
		if ((sierra_cameras[x].usb_vendor  > 0) &&
		    (sierra_cameras[x].usb_product > 0)) {
#ifdef linux
			/* The USB SCSI generic passthrough driver only works on Linux currently */
			if (sierra_cameras[x].flags & (SIERRA_WRAP_USB_OLYMPUS|SIERRA_WRAP_USB_PENTAX|SIERRA_WRAP_USB_NIKON))
			    a.port |= GP_PORT_USB_SCSI;
                        else
#endif
			    a.port |= GP_PORT_USB;
		}
		a.speed[0] = 9600;
		a.speed[1] = 19200;
		a.speed[2] = 38400;
		if (sierra_cameras[x].flags & SIERRA_LOW_SPEED) {
			/*
			 * Camera supports 9600 -> 38400 
			 */
			a.speed[3] = 0;
		} else {
			a.speed[3] = 57600;
			if (sierra_cameras[x].flags & SIERRA_MID_SPEED) {
				/*
				 * Camera supports 9600 -> 57600.
				 */
				a.speed[4] = 0;
			} else {
				a.speed[4] = 115200;
				a.speed[5] = 0;
			}
		}
		a.operations        = 	GP_OPERATION_CAPTURE_IMAGE |
					GP_OPERATION_CAPTURE_PREVIEW |
					GP_OPERATION_CONFIG;
		a.file_operations   = 	GP_FILE_OPERATION_DELETE | 
					GP_FILE_OPERATION_PREVIEW |
					GP_FILE_OPERATION_AUDIO;
		a.folder_operations = 	GP_FOLDER_OPERATION_DELETE_ALL |
					GP_FOLDER_OPERATION_PUT_FILE;
		a.usb_vendor  = sierra_cameras[x].usb_vendor;
		a.usb_product = sierra_cameras[x].usb_product;
		gp_abilities_list_append (list, a);
	}

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
	       void *data, GPContext *context)
{
	Camera *camera = data;

	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_list_files (camera, folder, list, context));
	return (camera_stop (camera, context));
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder,
		      CameraList *list, void *data, GPContext *context)
{
	Camera *camera = data;

	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_list_folders (camera, folder, list, context));
	return (camera_stop (camera, context));
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;
	SierraPicInfo i;

	/*
	 * Get the file number from the CameraFilesystem. We need numbers
	 * starting with 1.
	 */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename,
					 context));
	n++;

	info->file.fields    = GP_FILE_INFO_NONE;
	info->preview.fields = GP_FILE_INFO_NONE;
	info->audio.fields   = GP_FILE_INFO_NONE;
	info->file.permissions = GP_FILE_PERM_READ;

	/*
	 * Get information about this image. Don't make this fatal as
	 * at least the "Agfa ePhoto307" doesn't support this command.
	 */
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_change_folder (camera, folder, context));
	memset (&i, 0, sizeof (SierraPicInfo));
	CHECK_STOP (camera, sierra_get_pic_info (camera, n, &i, context));
	/* Size of file */
	if (i.size_file) {
		info->file.fields |= GP_FILE_INFO_SIZE;
		info->file.size = i.size_file;
	}

	/* Size of preview */
	if (i.size_preview) {
		info->preview.fields |= GP_FILE_INFO_SIZE;
		info->preview.size = i.size_preview;
	}

	/* Audio data? */
	if (i.size_audio) {
		/* Size */
		info->audio.size = i.size_audio;
		info->audio.fields |= GP_FILE_INFO_SIZE;

		/* Type */
		strcpy (info->audio.type, GP_MIME_WAV);
		info->audio.fields |= GP_FILE_INFO_TYPE;
	}

#if 0 /* need to add GMT -> local conversion first */
	if (i.date) {
		info->file.fields |= GP_FILE_INFO_MTIME;
		info->file.mtime = i.date; /* FIXME: convert from GMT to localtime */
	}
#endif
	/* Type of image and preview? */
	if (strstr (filename, ".MOV") != NULL) {
		strcpy (info->file.type, GP_MIME_QUICKTIME);
		strcpy (info->preview.type, GP_MIME_JPEG);
	} else if (strstr (filename, ".TIF") != NULL) {
		strcpy (info->file.type, GP_MIME_TIFF);
		strcpy (info->preview.type, GP_MIME_TIFF);
	} else {
		strcpy (info->file.type, GP_MIME_JPEG);
		strcpy (info->preview.type, GP_MIME_JPEG);
	}
	info->file.fields    |= GP_FILE_INFO_TYPE;
	info->preview.fields |= GP_FILE_INFO_TYPE;

	/* Image protected? */
	info->file.fields |= GP_FILE_INFO_PERMISSIONS;
	if (i.locked == SIERRA_LOCKED_NO)
		info->file.permissions |= GP_FILE_PERM_DELETE;

	return (camera_stop (camera, context));
}

static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;
	SierraPicInfo i;

	CHECK (n = gp_filesystem_number (camera->fs, folder, filename, context));
	n++;

	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_change_folder (camera, folder, context));
	CHECK_STOP (camera, sierra_get_pic_info (camera, n, &i, context));

	if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
		if (info.file.permissions & GP_FILE_PERM_DELETE) {
			if (i.locked == SIERRA_LOCKED_YES) {
				CHECK_STOP (camera, sierra_set_locked (camera,
						n, SIERRA_LOCKED_NO, context));
			}
		} else {
			if (i.locked == SIERRA_LOCKED_NO) {
				CHECK_STOP (camera, sierra_set_locked (camera,
						n, SIERRA_LOCKED_YES, context));
			}
		}
	}

	return (camera_stop (camera, context));
}

int
camera_start (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	unsigned int i;
	SierraSpeed speed;

	GP_DEBUG ("Establishing connection");

	switch (camera->port->type) {
	case GP_PORT_SERIAL:

		/* If needed, change speed. */
		CHECK (gp_port_get_settings (camera->port, &settings));
		if (camera->pl->speed != settings.serial.speed) {
			for (i = 0; SierraSpeeds[i].bit_rate; i++)
				if (camera->pl->speed ==
						SierraSpeeds[i].bit_rate)
					break;
			if (SierraSpeeds[i].bit_rate)
				speed = SierraSpeeds[i].speed;
			else {
				GP_DEBUG ("Invalid speed %i. Using 19200 "
					"(default).", camera->pl->speed);
				speed = SIERRA_SPEED_19200;
			}
			CHECK (sierra_set_speed (camera, speed, context));
		}
		break;

	case GP_PORT_USB:
	case GP_PORT_USB_SCSI:
		CHECK (gp_port_set_timeout (camera->port, 5000));
		break;
	default:
		break;
	}

	return (GP_OK);
}

int
camera_stop (Camera *camera, GPContext *context) 
{
	GP_DEBUG ("Closing connection");

	switch (camera->port->type) {
	case GP_PORT_SERIAL:
		CHECK (sierra_set_speed (camera, SIERRA_SPEED_19200, context));
		break;
	default:
		break;
	}

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context) 
{
	GP_DEBUG ("sierra camera_exit");

	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
        int regd, n;
	char *jpeg_data = NULL;
	int jpeg_size;
	const char *data, *mime_type;
	long unsigned int size;
	int download_size, audio_info[8];
	unsigned int transferred;

	/*
	 * Get the file number from the CameraFileSystem.
	 * We need file numbers starting with 1.
	 */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename, context));
	n++;

	/* In which register do we have to look for data? */
	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
	case GP_FILE_TYPE_EXIF:
		regd = 15;
		break;
	case GP_FILE_TYPE_NORMAL:
		regd = 14;
		break;
	case GP_FILE_TYPE_AUDIO:
		regd = 44;
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	/* Set the working folder */
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_change_folder (camera, folder, context));

	/*
	 * We need the file size in order to display progress information.
	 * Since we _can_ run without progress information, make failures
	 * here not prevent the download - but the failure should still
	 * show up in any logs.
	 *
	 * Some cameras do not return anything useful for register 47, so
	 * don't try to use it at all (via sierra_get_pic_info) here.
	 */
	download_size = 0;
	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
	case GP_FILE_TYPE_EXIF:
		CHECK_STOP (camera, sierra_get_size(camera, 13, n, &download_size, context));
		break;
	case GP_FILE_TYPE_NORMAL:
		CHECK_STOP (camera, sierra_get_size(camera, 12, n, &download_size, context));
		break;
	case GP_FILE_TYPE_AUDIO:
		CHECK_STOP (camera, sierra_get_string_register (camera, 43, n, NULL,
			(unsigned char *) &audio_info, &transferred, context));
		if (transferred == 0) 
			download_size = 0;
		else
			download_size = audio_info[0];
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	/* Get the file */
	CHECK_STOP (camera, sierra_get_string_register (camera, regd, n,
					file, NULL, (unsigned int *)&download_size,  context));
	if (download_size == 0) {
		/*
		 * Assume a failure. At least coolpix 880 returns no error
		 * and no data when asked to download audio.
		 */
		return (GP_ERROR_NOT_SUPPORTED);
	}
	CHECK (camera_stop (camera, context));

	/* Now get the data and do some post-processing */
	CHECK (gp_file_get_data_and_size (file, &data, &size));
	switch (type) {
	case GP_FILE_TYPE_PREVIEW:

		/* * Thumbnails are always JPEG files (as far as we know...) */
		CHECK (gp_file_set_mime_type (file, GP_MIME_JPEG));

		/* 
		 * Some camera (e.g. Epson 3000z) send the EXIF application
		 * data as thumbnail of a picture. The JPEG file need to be
		 * extracted. Equally for movies, the JPEG thumbnail is
		 * contained within a .MOV file. It needs to be extracted.
		 */
		get_jpeg_data (data, size, &jpeg_data, &jpeg_size);

		if (jpeg_data) {
			/* Append data to the output file */
			gp_file_set_data_and_size (file, jpeg_data, jpeg_size);
		} else {
			/* No valid JPEG data found */
			return GP_ERROR_CORRUPTED_DATA;
		}

		break;
	case GP_FILE_TYPE_NORMAL:

		/*
		 * Detect the mime type. If the resulting mime type is
		 * GP_MIME_RAW, manually set GP_MIME_QUICKTIME.
		 */
		CHECK (gp_file_detect_mime_type (file));
		CHECK (gp_file_get_mime_type (file, &mime_type));
		if (!strcmp (mime_type, GP_MIME_RAW))
			CHECK (gp_file_set_mime_type (file, GP_MIME_QUICKTIME));
		break;
	
	case GP_FILE_TYPE_AUDIO:
		CHECK (gp_file_set_mime_type (file, GP_MIME_WAV));
		break;

	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;
	int count;

	GP_DEBUG (
			 "*** sierra_folder_delete_all");
	GP_DEBUG ("*** folder: %s", folder);

	/* Set the working folder and delete all pictures there */
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_change_folder (camera, folder, context));
	CHECK_STOP (camera, sierra_delete_all (camera, context));

	/*
	 * Mick Grant <mickgr@drahthaar.clara.net> found out that his
	 * Nicon CoolPix 880 won't have deleted any picture at this point.
	 * It seems that those cameras just acknowledge the command but do
	 * nothing in the end. Therefore, we have to count the number of 
	 * pictures that are now on the camera. If there indeed are any 
	 * pictures, return an error. libgphoto2 will then try to manually
	 * delete them one-by-one.
	 */
	CHECK_STOP (camera, sierra_get_int_register (camera, 10, &count, context));
	if (count > 0)
		return (GP_ERROR);

	return (camera_stop (camera, context));
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context) 
{
	Camera *camera = data;
	int n;
	unsigned int id;

	GP_DEBUG ("*** sierra_file_delete");
	GP_DEBUG ("*** folder: %s", folder);
	GP_DEBUG ("*** filename: %s", filename);

	id = gp_context_progress_start (context, 4, "%s", filename);
	/*
	 * XXX the CHECK() macro's don't allow for cleanup on error.
	 */
	/* Get the file number from the CameraFilesystem */
	gp_context_progress_update (context, id, 0);
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename, context));
	/* Set the working folder and delete the file */
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_change_folder (camera, folder, context));
	/*
	 * The following command takes a while, and there is no way to add
	 * a nice progress bar, since it is a single sierra_action call.
	 * so multiple gp_context_progress_update () calls can not add 
	 * anything.
	 */
	CHECK_STOP (camera, sierra_delete (camera, n + 1, context));
	CHECK (camera_stop (camera, context));
	gp_context_progress_stop (context, id);
	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context) 
{
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_capture (camera, type, path, context));
	CHECK (camera_stop (camera, context));

	return (GP_OK);
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_capture_preview (camera, file, context));
	CHECK (camera_stop (camera, context));

	return (GP_OK);
}

static int
put_file_func (CameraFilesystem * fs, const char *folder, const char *filename,
	CameraFileType type, CameraFile * file, void *data, GPContext *context)
{
	Camera *camera = data;
	char *picture_folder;
	int ret;
	const char *data_file;
	long unsigned int data_size;
	int available_memory;

	GP_DEBUG ("*** put_file_func");
	GP_DEBUG ("*** folder: %s", folder);
	GP_DEBUG ("*** filename: %s", filename);

	if (type != GP_FILE_TYPE_NORMAL)
		return GP_ERROR_BAD_PARAMETERS;
	
	/* Check the size */
	CHECK (gp_file_get_data_and_size (file, &data_file, &data_size));
	if ( data_size == 0 ) {
		gp_context_error (context,
			_("The file to be uploaded has a null length"));
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Initialize the camera */
	CHECK (camera_start (camera, context));

	/* Check the battery capacity */
	CHECK (sierra_check_battery_capacity (camera, context));

	/* Check the available memory */
	CHECK (sierra_get_memory_left(camera, &available_memory, context));
	if (available_memory < data_size) {
		gp_context_error (context,
				    _("Not enough memory available on the memory card"));
		return GP_ERROR_NO_MEMORY;
	}

	/* Get the name of the folder containing the pictures */
	if ( (ret = sierra_get_picture_folder(camera, &picture_folder)) != GP_OK ) {
		gp_context_error (context,
				    _("Cannot retrieve the name of the folder containing the pictures"));
		return ret;
	}

	/* Check the destination folder is the folder containing the pictures.
	   Otherwise, the upload is not supported by the camera. */
	if ( strcmp(folder, picture_folder) ) {
		gp_context_error (context, _("Upload is supported into the '%s' folder only"),
				    picture_folder);
		free(picture_folder);
		return GP_ERROR_NOT_SUPPORTED;
	}
	free(picture_folder);

	/* It is not required to set the destination folder: the command
	   will be ignored by the camera and the uploaded file will be put
	   into the picure folder */

	/* Upload the file */
	CHECK_STOP (camera, sierra_upload_file (camera, file, context));

	return (camera_stop (camera, context));
}

#ifdef UNUSED_CODE
static void dump_register (Camera *camera, GPContext *context)
{
	int ret, value, i;
	const char *description[] = {
		"?",				/* 0 */
		"resolution",
		"date",
		"shutter speed",
		"current image number",
		"aperture",
		"color mode",
		"flash mode",
		"?",
		"?",
		"frames taken",			/* 10 */
		"frames left",
		"size of current image",
		"size of thumbnail of current image",
		"current preview (captured)",
		"?",
		"battery life",
		"?",
		"?",
		"brightness/contrast",
		"white balance",		/* 20 */
		"?",
		"camera id",
		"auto off (host)",
		"auto off (field)",
		"serial number",
		"software revision",
		"?",
		"memory left",
		"?",
		"?",				/* 30 */
		"?",
		"?",
		"lens mode",
		"?",
		"lcd brightness",
		"?",
		"?",
		"lcd auto off",
		"?",
		"?",				/* 40 */
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?",				/* 50 */
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?",				/* 60 */
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"spot metering mode",		/* 70 */
		"?",
		"zoom",
		"?", "?", "?", "?", "?", "?", 
		"current filename",
		"?",				/* 80 */
		"?", "?",
		"number of folders in current folder/folder number",
		"current folder name",
		"?", "?", "?", "?", "?",
		"?",				/* 90 */
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?", 				/* 100 */
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?", 				/* 110 */
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?",				/* 120 */
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?"				/* 130 */
	};
		

	GP_DEBUG ("*** Register:");
	for (i = 0; i < 128; i++) {
		ret = sierra_get_int_register (camera, i, &value, context);
		if (ret >= 0)
			GP_DEBUG (
					 "***  %3i: %12i (%s)", i, value, 
					 description [i]);
	}
}
#endif

static int
camera_get_config_olympus (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *child;
	CameraWidget *section;
	char t[1024];
	int ret, value;
	float float_value;

	GP_DEBUG ("*** camera_get_config_olympus");

	CHECK (camera_start (camera, context));

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);
	gp_widget_new (GP_WIDGET_SECTION, _("Picture Settings"), &section);
	gp_widget_append (*window, section);

	/* Resolution */
	GP_DEBUG ("Resolution...");
	ret = sierra_get_int_register (camera, 1, &value, context);
	GP_DEBUG ("... '%s'.", gp_result_as_string (ret));
        if (ret >= 0) {

		gp_widget_new (GP_WIDGET_RADIO, _("Resolution"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Standard"));
		gp_widget_add_choice (child, _("High"));
		gp_widget_add_choice (child, _("Best"));
		
                switch (value) {
		case 0: strcpy (t, _("Auto"));
			break;
                case 1: strcpy (t, _("Standard"));
                        break;
                case 2: strcpy (t, _("High"));
                        break;
                case 3: strcpy (t, _("Best"));
                        break;
                default:
                	sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Shutter Speed */
        ret = sierra_get_int_register (camera, 3, &value, context);
        if (ret >= 0) {
		
		gp_widget_new (GP_WIDGET_RANGE, 
			       _("Shutter Speed (microseconds, 0 auto)"), 
			       &child);
		gp_widget_set_range (child, 0, 2000000, 1);
		float_value = value;
		gp_widget_set_value (child, &float_value);
		gp_widget_append (section, child);
        }

	/* Aperture */
        ret = sierra_get_int_register (camera, 5, &value, context);
        if (ret >= 0) {

		gp_widget_new (GP_WIDGET_RADIO, _("Aperture"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Low"));
		gp_widget_add_choice (child, _("Medium"));
		gp_widget_add_choice (child, _("High"));

                switch (value) {
		case 0: strcpy (t, _("Auto"));
			break;
                case 1: strcpy (t, _("Low"));
                        break;
                case 2: strcpy (t, _("Medium"));
                        break;
                case 3: strcpy (t, _("High"));
                        break;
                default:
                        sprintf(t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Color Mode */
        ret = sierra_get_int_register (camera, 6, &value, context);
        if (ret >= 0) {
		/*
		 * Those values are for a C-2020 Z. If your model differs,
		 * we have to distinguish models here.
		 */
		gp_widget_new (GP_WIDGET_RADIO, _("Color Mode"), &child);
		gp_widget_add_choice (child, _("Normal"));
		gp_widget_add_choice (child, _("Black/White"));
		gp_widget_add_choice (child, _("Sepia"));
		gp_widget_add_choice (child, _("White Board"));
		gp_widget_add_choice (child, _("Black Board"));

                switch (value) {
		case 0: strcpy (t, _("Normal"));
			break;
                case 1: strcpy (t, _("Black/White"));
                        break;
                case 2: strcpy (t, _("Sepia"));
                        break;
		case 3: strcpy (t, _("White Board"));
			break;
		case 4: strcpy (t, _("Black Board"));
			break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Flash Mode */
	ret = sierra_get_int_register (camera, 7, &value, context);
        if (ret >= 0) {

		gp_widget_new (GP_WIDGET_RADIO, _("Flash Mode"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Force"));
		gp_widget_add_choice (child, _("Off"));
		gp_widget_add_choice (child, _("Red-eye Reduction"));
		gp_widget_add_choice (child, _("Slow Sync"));

                switch (value) {
                case 0: strcpy (t, _("Auto"));
                        break;
                case 1: strcpy (t, _("Force"));
                        break;
                case 2: strcpy (t, _("Off"));
                        break;
                case 3: strcpy (t, _("Red-eye Reduction"));
                        break;
                case 4: strcpy (t, _("Slow Sync"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Brightness/Contrast */
        ret = sierra_get_int_register (camera, 19, &value, context);
        if (ret >= 0) {

		gp_widget_new (GP_WIDGET_RADIO, _("Brightness/Contrast"), 
			       &child);
		gp_widget_add_choice (child, _("Normal"));
		gp_widget_add_choice (child, _("Bright+"));
		gp_widget_add_choice (child, _("Bright-"));
		gp_widget_add_choice (child, _("Contrast+"));
		gp_widget_add_choice (child, _("Contrast-"));

                switch (value) {
                case 0: strcpy (t, _("Normal"));
                        break;
                case 1: strcpy (t, _("Bright+"));
                        break;
                case 2: strcpy (t, _("Bright-"));
                        break;
                case 3: strcpy (t, _("Contrast+"));
                        break;
                case 4: strcpy (t, _("Contrast-"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* White Balance */
        ret = sierra_get_int_register (camera, 20, &value, context);
        if (ret >= 0) {
		
		gp_widget_new (GP_WIDGET_RADIO, _("White Balance"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Skylight"));
		gp_widget_add_choice (child, _("Fluorescent"));
		gp_widget_add_choice (child, _("Tungsten"));
		gp_widget_add_choice (child, _("Cloudy"));

                switch (value) {
                case 0: strcpy (t, _("Auto"));
                        break;
                case 1: strcpy (t, _("Skylight"));
                        break;
                case 2: strcpy (t, _("Fluorescent"));
                        break;
                case 3: strcpy (t, _("Tungsten"));
                        break;
                case 255:
                        strcpy (t, _("Cloudy"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Lens Mode */
        ret = sierra_get_int_register (camera, 33, &value, context);
        if (ret >= 0) {

		gp_widget_new (GP_WIDGET_RADIO, _("Lens Mode"), &child);
		gp_widget_add_choice (child, _("Macro"));
		gp_widget_add_choice (child, _("Normal"));
		gp_widget_add_choice (child, _("Infinity/Fish-eye"));

                switch (value) {
                case 1: strcpy (t, _("Macro"));
                        break;
                case 2: strcpy (t, _("Normal"));
                        break;
                case 3: strcpy (t, _("Infinity/Fish-eye"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Spot Metering Mode */
	ret = sierra_get_int_register (camera, 70, &value, context);
	if (ret >= 0) {
		
		gp_widget_new (GP_WIDGET_RADIO, _("Spot Metering Mode"), 
			       &child);
		gp_widget_add_choice (child, _("On"));
		gp_widget_add_choice (child, _("Off"));

		switch (value) {
		case 5:	strcpy (t, _("Off"));
			break;
		case 3: strcpy (t, _("On"));
			break;
		default:
			sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
		}
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
	}

	/* Zoom */
	ret = sierra_get_int_register (camera, 72, &value, context);
	if (ret >= 0) {

		gp_widget_new (GP_WIDGET_RADIO, _("Zoom"), &child);
		gp_widget_add_choice (child, _("1x"));
		gp_widget_add_choice (child, _("1.6x"));
		gp_widget_add_choice (child, _("2x"));
		gp_widget_add_choice (child, _("2.5x"));

		switch (value) {
		case 0: strcpy (t, _("1x"));
			break;
		case 8: strcpy (t, _("2x"));
			break;
		case 520:
			strcpy (t, _("1.6x"));
			break;
		case 1032:
			strcpy (t, _("2.5x"));
			break;
		default:
			sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
		}
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
	}

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Settings"), &section);
	gp_widget_append (*window, section);

	/* Auto Off (host) */
	ret = sierra_get_int_register (camera, 23, &value, context);
	if (ret >= 0) {
		
		gp_widget_new (GP_WIDGET_RANGE, _("Auto Off (host) "
				       "(in seconds)"), &child);
		gp_widget_set_info (child, _("How long will it take until the "
				    "camera powers off when connected to the "
				    "computer?"));
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* Auto Off (field) */
	ret = sierra_get_int_register (camera, 24, &value, context);
	if (ret >= 0) {

		gp_widget_new (GP_WIDGET_RANGE, _("Auto Off (field) "
				       "(in seconds)"), &child);
		gp_widget_set_info (child, _("How long will it take until the "
				    "camera powers off when not connected to "
				    "the computer?"));
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* LCD Brightness */
	ret = sierra_get_int_register (camera, 35, &value, context);
	if (ret >= 0) {
		
		gp_widget_new (GP_WIDGET_RANGE, _("LCD Brightness"), &child);
		gp_widget_set_range (child, 1, 7, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* LCD Auto Off */
	ret = sierra_get_int_register (camera, 38, &value, context);
	if (ret >= 0) {

		gp_widget_new (GP_WIDGET_RANGE, _("LCD Auto Off (in "
				       "seconds)"), &child);
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	return (camera_stop (camera, context));
}

static int
camera_set_config_olympus (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *child;
	char *value;
	int i = 0;
	float float_value;

	GP_DEBUG ("*** camera_set_config");

	CHECK (camera_start (camera, context));

	/* Resolution */
	GP_DEBUG ("*** setting resolution");
	if ((gp_widget_get_child_by_label (window, _("Resolution"), &child)
		>= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Standard")) == 0) {
			i = 1;
		} else if (strcmp (value, _("High")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Best")) == 0) {
			i = 3;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 1, i, context));
	}
	
	/* Shutter Speed */
	GP_DEBUG ("*** setting shutter speed");
	if ((gp_widget_get_child_by_label (window, 
		_("Shutter Speed (microseconds, 0 auto)"), &child) >= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &float_value);
		i = float_value;
		CHECK_STOP (camera, sierra_set_int_register (camera, 3, i, context));
	}

	/* Aperture */
	GP_DEBUG ("*** setting aperture");
	if ((gp_widget_get_child_by_label (window, _("Aperture"), &child) 
		>= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Low")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Medium")) == 0) {
			i = 2;
		} else if (strcmp (value, _("High")) == 0) {
			i = 3;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 5, i, context));
	}

	/* Color Mode */
	GP_DEBUG ("*** setting color mode");
	if ((gp_widget_get_child_by_label (window, _("Color Mode"), &child)
		>= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Normal")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Black/White")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Sepia")) == 0) {
			i = 2;
		} else if (strcmp (value, _("White Board")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Black Board")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 6, i, context));
	}

	/* Flash Mode */
	GP_DEBUG ("*** setting flash mode");
	if ((gp_widget_get_child_by_label (window, _("Flash Mode"), &child)
		>= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Force")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Off")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Red-eye Reduction")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Slow Sync")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 7, i, context));
	}

	/* Brightness/Contrast */
	GP_DEBUG (
			 "*** setting brightness/contrast");
	if ((gp_widget_get_child_by_label (window, _("Brightness/Contrast"), 
		&child) >= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Normal")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Bright+")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Bright-")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Contrast+")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Contrast-")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 19, i, context));
	}

	/* White Balance */
	GP_DEBUG ("*** setting white balance");
	if ((gp_widget_get_child_by_label (window, _("White Balance"), &child)
		>= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Skylight")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Fluorescent")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Tungsten")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Cloudy")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 20, i, context));
	}

	/* Lens Mode */
	GP_DEBUG ("*** setting lens mode");
	if ((gp_widget_get_child_by_label (window, _("Lens Mode"), &child)
		>= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
                if (strcmp (value, _("Macro")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Normal")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Infinity/Fish-eye")) == 0) {
			i = 3;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 33, i, context));
        }

	/* Spot Metering Mode */
	GP_DEBUG (
			 "*** setting spot metering mode");
	if ((gp_widget_get_child_by_label (window, _("Spot Metering Mode"), 
		&child) >= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("On")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Off")) == 0) {
			i = 5;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 33, i, context));
	}

	/* Zoom */
	GP_DEBUG ("*** setting zoom");
	if ((gp_widget_get_child_by_label (window, _("Zoom"), &child) 
		>= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, "1x") == 0) {
			i = 0;
		} else if (strcmp (value, "2x") == 0) {
			i = 8;
		} else if (strcmp (value, "1.6x") == 0) {
			i = 520;
		} else if (strcmp (value, "2.5x") == 0) {
			i = 1032;
		} else 
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 72, i, context));
	}

        /* Auto Off (host) */
	GP_DEBUG ("*** setting auto off (host)");
	if ((gp_widget_get_child_by_label (window, _("Auto Off (host) "
					  "(in seconds)"), &child) >= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
                CHECK_STOP (camera, sierra_set_int_register (camera, 23, i, context));
        }

        /* Auto Off (field) */
	GP_DEBUG (
			 "*** setting auto off (field)");
	if ((gp_widget_get_child_by_label (window, _("Auto Off (field) "
		"(in seconds)"), &child) >= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 24, i, context));
	}

        /* LCD Brightness */
	GP_DEBUG ("*** setting lcd brightness");
	if ((gp_widget_get_child_by_label (window, 
		_("LCD Brightness"), &child) >= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 35, i, context));
	}

        /* LCD Auto Off */
	GP_DEBUG ("*** setting lcd auto off");
	if ((gp_widget_get_child_by_label (window, 
		_("LCD Auto Off (in seconds)"), &child) >= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 38, i, context));
        }

	return (camera_stop (camera, context));
}


static int
camera_get_config_epson (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *child;
	CameraWidget *section;
	char t[1024];
	int ret, value;

	GP_DEBUG ("*** camera_get_config_epson");

	CHECK (camera_start (camera, context));

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);

	gp_widget_new (GP_WIDGET_SECTION, _("Shot Settings"), &section);
	gp_widget_append (*window, section);

	/* Aperture */
        ret = sierra_get_int_register (camera, 5, &value, context);
        if (ret >= 0) {
		gp_widget_new (GP_WIDGET_RADIO, _("Aperture"), &child);
		gp_widget_add_choice (child, _("F2"));
		gp_widget_add_choice (child, _("F2.3"));
		gp_widget_add_choice (child, _("F2.8"));
		gp_widget_add_choice (child, _("F4"));
		gp_widget_add_choice (child, _("F5.6"));
		gp_widget_add_choice (child, _("F8"));
		gp_widget_add_choice (child, _("auto"));
                switch (value) {
		case 0: strcpy (t, _("F2"));
			break;
                case 1: strcpy (t, _("F2.3"));
			break;
                case 2: strcpy (t, _("F2.8"));
			break;
                case 3: strcpy (t, _("F4"));    
			break;
		case 4: strcpy (t, _("F5.6"));  
			break;
		case 5: strcpy (t, _("F8"));    
			break;
		case 6: strcpy (t, _("auto"));  
			break;
                default:
			sprintf(t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Flash Mode */
	ret = sierra_get_int_register (camera, 7, &value, context);
        if (ret >= 0) {
		gp_widget_new (GP_WIDGET_RADIO, _("Flash Mode"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Force"));
		gp_widget_add_choice (child, _("Off"));
		gp_widget_add_choice (child, _("Red-eye Reduction"));
		gp_widget_add_choice (child, _("Slow Sync"));
                switch (value) {
                case 0: strcpy (t, _("Auto"));
                        break;
                case 1: strcpy (t, _("Force"));
                        break;
                case 2: strcpy (t, _("Off"));
                        break;
                case 3: strcpy (t, _("Red-eye Reduction"));
                        break;
                case 4: strcpy (t, _("Slow Sync"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* White Balance */
        ret = sierra_get_int_register (camera, 20, &value, context);
        if (ret >= 0) {
		gp_widget_new (GP_WIDGET_RADIO, _("White Balance"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Fixed"));
		gp_widget_add_choice (child, _("Custom"));

                switch (value) {
                case 0: strcpy (t, _("Auto"));
                        break;
                case 1: strcpy (t, _("Fixed"));
                        break;
                case 225: strcpy (t, _("Custom"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	gp_widget_new (GP_WIDGET_SECTION, _("Picture Settings"), &section);
	gp_widget_append (*window, section);

	/* Lens Mode */
        ret = sierra_get_int_register (camera, 33, &value, context);
        if (ret >= 0) {
		gp_widget_new (GP_WIDGET_RADIO, _("Lens Mode"), &child);
		gp_widget_add_choice (child, _("Macro"));
		gp_widget_add_choice (child, _("Normal"));
                switch (value) {
                case 1: strcpy (t, _("Macro"));
                        break;
                case 2: strcpy (t, _("Normal"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Resolution */
	ret = sierra_get_int_register (camera, 1, &value, context);
        if (ret >= 0) {
		gp_widget_new (GP_WIDGET_RADIO, _("Resolution"), &child);
		gp_widget_add_choice (child, _("standard"));
		gp_widget_add_choice (child, _("fine"));
		gp_widget_add_choice (child, _("superfine"));
		gp_widget_add_choice (child, _("HyPict"));
		
                switch (value) {
		case 1: strcpy (t, _("standard"));
			break;
                case 2: strcpy (t, _("fine"));
			break;
                case 3: strcpy (t, _("superfine"));
			break;
                case 34: strcpy (t, _("HyPict"));
			break;
                default:
			sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Color Mode */
	ret = sierra_get_int_register (camera, 6, &value, context);
	if (ret >= 0) {
		gp_widget_new (GP_WIDGET_RADIO, _("Color Mode"), &child);
		gp_widget_add_choice (child, _("color"));
		gp_widget_add_choice (child, _("black & white"));
		switch (value) {
		case 1: strcpy (t, _("color"));
			break;
		case 2: strcpy (t, _("black & white"));
			break;
		default:
			sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
		}
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
	}

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Settings"), &section);
	gp_widget_append (*window, section);

	/* Auto Off (host) */
	ret = sierra_get_int_register (camera, 23, &value, context);
	if (ret >= 0) {
		
		gp_widget_new (GP_WIDGET_RANGE, _("Auto Off (host) "
				       "(in seconds)"), &child);
		gp_widget_set_info (child, _("How long will it take until the "
				    "camera powers off when connected to the "
				    "computer?"));
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* Auto Off (field) */
	ret = sierra_get_int_register (camera, 24, &value, context);
	if (ret >= 0) {

		gp_widget_new (GP_WIDGET_RANGE, _("Auto Off (field) "
				       "(in seconds)"), &child);
		gp_widget_set_info (child, _("How long will it take until the "
				    "camera powers off when not connected to "
				    "the computer?"));
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* Language */
	ret = sierra_get_int_register (camera, 53, &value, context);
        if (ret >= 0) {
		gp_widget_new (GP_WIDGET_RADIO, _("Language"), &child);
		gp_widget_add_choice (child, _("Korean"));
		gp_widget_add_choice (child, _("English"));
		gp_widget_add_choice (child, _("French"));
		gp_widget_add_choice (child, _("German"));
		gp_widget_add_choice (child, _("Italian"));
		gp_widget_add_choice (child, _("Japanese"));
		gp_widget_add_choice (child, _("Spanish"));
		gp_widget_add_choice (child, _("Portugese"));
                switch (value) {
                case 1: strcpy (t, _("Korean"));
                        break;
                case 3: strcpy (t, _("English"));
                        break;
                case 4: strcpy (t, _("French"));
                        break;
                case 5: strcpy (t, _("German"));
                        break;
                case 6: strcpy (t, _("Italian"));
                        break;
		case 7: strcpy (t, _("Japanese"));
			break;
		case 8: strcpy (t, _("Spanish"));
			break;
		case 9: strcpy (t, _("Portugese"));
			break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Date & Time */
	ret = sierra_get_int_register (camera, 2, &value, context);
	if (ret >= 0) {
		gp_widget_new (GP_WIDGET_DATE, _("Date & Time"), &child);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	return (camera_stop (camera, context));
}

static int
camera_set_config_epson (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *child;
	char *value;
	int i = 0;

	GP_DEBUG ("*** camera_set_config_epson");

	CHECK (camera_start (camera, context));

	/* Aperture */
	GP_DEBUG ("*** setting aperture");
	if ((gp_widget_get_child_by_label (window, _("Aperture"), &child) 
	     >= 0) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("F2")) == 0) {
			i = 0;
		} else if (strcmp (value, _("F2.3")) == 0) {
			i = 1;
		} else if (strcmp (value, _("F2.8")) == 0) {
			i = 2;
		} else if (strcmp (value, _("F4")) == 0) {
			i = 3;
		} else if (strcmp (value, _("F5.6")) == 0) {
			i = 4;
		} else if (strcmp (value, _("F8")) == 0) {
			i = 5;
		} else if (strcmp (value, _("auto")) == 0) {
			i = 6;			
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 5, i, context));
	}

	/* Flash Mode */
	GP_DEBUG ("*** setting flash mode");
	if ((gp_widget_get_child_by_label (window, _("Flash Mode"), &child)
	     >= 0) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Force")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Off")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Red-eye Reduction")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Slow Sync")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 7, i, context));
	}

	/* White Balance */
	GP_DEBUG ("*** setting white balance");
	if ((gp_widget_get_child_by_label (window, _("White Balance"), &child)
	     >= 0) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Fixed")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Custom")) == 0) {
			i = 225;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 20, i, context));
	}

	/* Lens Mode */
	GP_DEBUG ("*** setting lens mode");
	if ((gp_widget_get_child_by_label (window, _("Lens Mode"), &child)
		>= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
                if (strcmp (value, _("Macro")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Normal")) == 0) {
			i = 2;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 33, i, context));
        }

	/* Resolution */
	GP_DEBUG ("*** setting resolution");
	if ((gp_widget_get_child_by_label (window, _("Resolution"), &child)
	     >= 0) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("standard")) == 0) {
			i = 1;
		} else if (strcmp (value, _("fine")) == 0) {
			i = 2;
		} else if (strcmp (value, _("superfine")) == 0) {
			i = 3;
		} else if (strcmp (value, _("HyPict")) == 0) {
			i = 34;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 1, i, context));
	}
	
	/* Color Mode */
	GP_DEBUG ("*** setting color mode");
	if ((gp_widget_get_child_by_label (window, _("Color Mode"), &child)
	     >= 0) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("color")) == 0) {
			i = 1;
		} else if (strcmp (value, _("black & white")) == 0) {
			i = 2;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 6, i, context));
	}

        /* Auto Off (host) */
	GP_DEBUG ("*** setting auto off (host)");
	if ((gp_widget_get_child_by_label (window, _("Auto Off (host) "
					  "(in seconds)"), &child) >= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
                CHECK_STOP (camera, sierra_set_int_register (camera, 23, i, context));
        }

        /* Auto Off (field) */
	GP_DEBUG (
			 "*** setting auto off (field)");
	if ((gp_widget_get_child_by_label (window, _("Auto Off (field) "
		"(in seconds)"), &child) >= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 24, i, context));
	}

	/* Language */
	GP_DEBUG ("*** setting language");
	if ((gp_widget_get_child_by_label (window, _("Language"), &child)
	     >= 0) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Korean")) == 0) {
			i = 1;
		} else if (strcmp (value, _("English")) == 0) {
			i = 3;
		} else if (strcmp (value, _("French")) == 0) {
			i = 4;
		} else if (strcmp (value, _("German")) == 0) {
			i = 5;
		} else if (strcmp (value, _("Italian")) == 0) {
			i = 6;
		} else if (strcmp (value, _("Japanese")) == 0) {
			i = 7;
		} else if (strcmp (value, _("Spanish")) == 0) {
			i = 8;
		} else if (strcmp (value, _("Portugese")) == 0) {
			i = 9;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 53, i, context));
	}

	/* Date & Time */
	GP_DEBUG ("*** setting date");
	if ((gp_widget_get_child_by_label (window, _("Date & Time"), &child)
		>= 0) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 2, i, context));
	}

	return (camera_stop (camera, context));
}


static int
camera_get_config_default (Camera *camera, CameraWidget **window, GPContext *context)
{
	return camera_get_config_olympus (camera, window, context);
}


static int
camera_set_config_default (Camera *camera, CameraWidget *window, GPContext *context)
{
	return camera_set_config_olympus (camera, window, context);
}


static int
camera_summary (Camera *camera, CameraText *summary, GPContext *c) 
{
	char buf[1024*32];
	int v, r;
	char t[1024];

	GP_DEBUG ("*** sierra camera_summary");

	CHECK (camera_start (camera, c));

	strcpy(buf, "");

	/*
	 * This is a non-fatal check if a memory card is present. Some
	 * cameras don't understand this command and error out here.
	 *
	 * At least on PhotoPC 3000z, if no card is present near all
	 * retrieved info are either unreadable or invalid...
	 */
	if (!(camera->pl->flags & SIERRA_NO_51)) {
		r = sierra_get_int_register(camera, 51, &v, c);
		if ((r >= 0) && (v == 1)) {
			strcpy (buf, _("Note: no memory card present, some"
				       " values may be invalid\n"));
			strcpy (summary->text, buf);
		}
	}
	/* Get all the string-related info */

	if (sierra_get_string_register (camera, 27, 0, NULL,
					(unsigned char *)t, (unsigned int *)&v, c) >= 0)
		sprintf (buf+strlen(buf), _("Camera Model: %s\n"), t);
	if (sierra_get_string_register (camera, 48, 0, NULL,
					(unsigned char *)t, (unsigned int *)&v, c) >= 0)
		sprintf (buf+strlen(buf), _("Manufacturer: %s\n"), t);
	if (sierra_get_string_register (camera, 22, 0, NULL,
					(unsigned char *)t, (unsigned int *)&v, c) >= 0)
		sprintf (buf+strlen(buf), _("Camera ID: %s\n"), t);
	if (sierra_get_string_register (camera, 25, 0, NULL,
					(unsigned char *)t, (unsigned int *)&v, c) >= 0)
		sprintf (buf+strlen(buf), _("Serial Number: %s\n"), t);
	if (sierra_get_string_register (camera, 26, 0, NULL,
					(unsigned char *)t, (unsigned int *)&v, c) >= 0)
		sprintf (buf+strlen(buf), _("Software Rev.: %s\n"), t);

	/* Get all the integer information */
	if (camera->pl->flags & SIERRA_NO_REGISTER_40) {
		if (sierra_get_int_register(camera, 10, &v, c) >= 0)
			sprintf (buf+strlen(buf), _("Frames Taken: %i\n"), v);
	} else {
		if (sierra_get_int_register(camera, 40, &v, c) >= 0)
			sprintf (buf+strlen(buf), _("Frames Taken: %i\n"), v);
	}
	if (sierra_get_int_register(camera, 11, &v, c) >= 0)
		sprintf (buf+strlen(buf), _("Frames Left: %i\n"), v);
	if (sierra_get_int_register(camera, 16, &v, c) >= 0)
		sprintf (buf+strlen(buf), _("Battery Life: %i\n"), v);
	if (sierra_get_int_register(camera, 28, &v, c) >= 0)
		sprintf (buf+strlen(buf), _("Memory Left: %i bytes\n"), v);

	/* Get date */
	if (sierra_get_int_register (camera, 2, &v, c) >= 0) {
		time_t vtime = v;
		sprintf (buf+strlen(buf), _("Date: %s"), ctime (&vtime));
	}
	strcpy (summary->text, buf);
	return (camera_stop (camera, c));
}

static int
storage_info_func (CameraFilesystem *fs,
                CameraStorageInformation **sinfos,
                int *nrofsinfos,
                void *data, GPContext *c
) {
        Camera 			*camera = data;
        CameraStorageInformation*sif;
	int			v;
	char			t[1024];

	GP_DEBUG ("*** sierra storage_info");

	CHECK (camera_start (camera, c));

	sif = malloc(sizeof(CameraStorageInformation));
	if (!sif)
		return GP_ERROR_NO_MEMORY;
	*sinfos = sif;
	*nrofsinfos = 1;

	sif->fields  = GP_STORAGEINFO_BASE;
	strcpy (sif->basedir, "/");
	sif->fields |= GP_STORAGEINFO_STORAGETYPE;
	sif->type    = GP_STORAGEINFO_ST_REMOVABLE_RAM;
	sif->fields |= GP_STORAGEINFO_ACCESS;
	sif->access  = GP_STORAGEINFO_AC_READWRITE;
	sif->fields |= GP_STORAGEINFO_FILESYSTEMTYPE;
	sif->fstype  = GP_STORAGEINFO_FST_DCF;

	if (sierra_get_string_register (camera, 25, 0, NULL,
					(unsigned char *)t, (unsigned int *)&v, c) >= 0) {
		sif->fields |= GP_STORAGEINFO_LABEL;
		strcpy (sif->label, t);
	}

	if (sierra_get_int_register(camera, 11, &v, c) >= 0) {
		sif->fields |= GP_STORAGEINFO_FREESPACEIMAGES;
		sif->freeimages = v;
	}
	if (sierra_get_int_register(camera, 28, &v, c) >= 0) {
		sif->fields |= GP_STORAGEINFO_FREESPACEKBYTES;
		sif->freekbytes = v/1024;
	}
	return camera_stop (camera, c);
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context) 
{
	GP_DEBUG ("*** sierra camera_manual");

	switch (camera->pl->model) {
	case SIERRA_MODEL_CAM_DESC:
		if (camera->pl->cam_desc->manual == NULL) {
			strcpy (manual->text, _("No camera manual available.\n"));
		} else {
			strcpy (manual->text, _(camera->pl->cam_desc->manual));
		}
		break;
	case SIERRA_MODEL_EPSON:
		snprintf (manual->text, sizeof(manual->text),
			_("Some notes about Epson cameras:\n"
			  "- Some parameters are not controllable remotely:\n"
			  "  * zoom\n"
			  "  * focus\n"
			  "  * custom white balance setup\n"
			  "- Configuration has been reverse-engineered with\n"
			  "  a PhotoPC 3000z, if your camera acts differently\n"
			  "  please send a mail to %s (in English)\n"), MAIL_GPHOTO_DEVEL);
		break;
	case SIERRA_MODEL_OLYMPUS:
	default:
		strcpy (manual->text, 
		  _("Some notes about Olympus cameras (and others?):\n"
		    "(1) Camera Configuration:\n"
		    "    A value of 0 will take the default one (auto).\n"
		    "(2) Olympus C-3040Z (and possibly also the C-2040Z\n"
		    "    and others) have a USB PC Control mode. To switch\n"
		    "    to this mode, turn on the camera, open\n"
		    "    the memory card access door and then press and\n"
		    "    hold both of the menu and LCD buttons until the\n"
		    "    camera control menu appears. Set it to ON.\n"
		    "(3) If you switch the 'LCD mode' to 'Monitor' or\n"
		    "    'Normal', don't forget to switch it back to 'Off'\n"
		    "    before disconnecting. Otherwise you cannot use\n"
		    "    the camera's buttons. If you end up in this\n"
		    "    state, you should reconnect the camera to the\n"
		    "    PC and switch LCD to 'Off'."));
		break;
	}

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context) 
{
	GP_DEBUG ("*** sierra camera_about");
	
	strcpy (about->text, 
		_("sierra SPARClite library\n"
		"Scott Fritzinger <scottf@unr.edu>\n"
		"Support for sierra-based digital cameras\n"
		"including Olympus, Nikon, Epson, and Pentax.\n"
		"\n"
		"Thanks to Data Engines (www.dataengines.com)\n"
		"for the use of their Olympus C-3030Z for USB\n"
		"support implementation."));

	return (GP_OK);
}

/**
 * get_jpeg_data:
 * @data: input data table
 * @data_size: table size
 * @jpeg_data: return JPEG data table (NULL if no valid data is found).
 *             To be free by the the caller.
 * @jpeg_size: return size of the jpeg_data table.
 *
 * Extract JPEG data form the provided input data (looking for the SOI
 * and SOF markers in the input data table)
 *
 * Returns: error code (either GP_OK or GP_ERROR_CORRUPTED_DATA)
 */
int get_jpeg_data(const char *data, int data_size, char **jpeg_data, int *jpeg_size)
{
	int i, ret_status;
	const char *soi_marker = NULL, *sof_marker = NULL;

	for(i=0 ; i<data_size ; i++) {
		if (memcmp(data+i, JPEG_SOI_MARKER, 2) == 0)
			soi_marker = data + i;
		if (memcmp(data+i, JPEG_SOF_MARKER, 2) == 0)
			sof_marker = data + i;
	}
   
	if (soi_marker && sof_marker) {
		/* Valid JPEG data has been found: build the output table */
		*jpeg_size = sof_marker - soi_marker + 2;
		*jpeg_data = (char*) calloc(*jpeg_size, sizeof(char));
		memcpy(*jpeg_data, soi_marker, *jpeg_size);
		ret_status = GP_OK;
	}
	else {
		/* Cannot find valid JPEG data */
		*jpeg_size = 0;
		*jpeg_data = NULL;
		ret_status = GP_ERROR_CORRUPTED_DATA;
	}

	return ret_status;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
	.get_info_func = get_info_func,
	.set_info_func = set_info_func,
	.get_file_func = get_file_func,
	.put_file_func = put_file_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func,
	.storage_info_func = storage_info_func,
};

int
camera_init (Camera *camera, GPContext *context) 
{
        int x = 0, ret, value;
        int vendor=0;
	GPPortSettings s;
	CameraAbilities a;

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->capture_preview      = camera_capture_preview;
        camera->functions->capture              = camera_capture;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	camera->pl = calloc (1, sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);

	camera->pl->model = SIERRA_MODEL_DEFAULT;
	camera->pl->first_packet = 1;
	camera->pl->flags = 0;

	/* Retrieve Camera information from name */
	gp_camera_get_abilities (camera, &a);
	for (x = 0; sierra_cameras[x].manuf; x++) {
		if (!strncmp (a.model, sierra_cameras[x].manuf,
			      strlen (sierra_cameras[x].manuf)) &&
		    !strcmp (a.model + strlen (sierra_cameras[x].manuf) + 1,
			     sierra_cameras[x].model)) {
			camera->pl->model = sierra_cameras[x].sierra_model;
			vendor = sierra_cameras[x].usb_product;
			camera->pl->flags = sierra_cameras[x].flags;
			camera->pl->cam_desc = sierra_cameras[x].cam_desc;
			break;
               }
	}

	switch (camera->pl->model) {
	case SIERRA_MODEL_EPSON:
		camera->functions->get_config = camera_get_config_epson;
		camera->functions->set_config = camera_set_config_epson;
		break;
	case SIERRA_MODEL_OLYMPUS:
		camera->functions->get_config = camera_get_config_olympus;
		camera->functions->set_config = camera_set_config_olympus;
		break;
	case SIERRA_MODEL_CAM_DESC:
		if (camera->pl->cam_desc == NULL) {
			GP_DEBUG ("*** sierra cam_desc NULL");
                        return (GP_ERROR_MODEL_NOT_FOUND);
		}
		camera->pl->flags |= camera->pl->cam_desc->flags;
		camera->functions->get_config = camera_get_config_cam_desc;
		camera->functions->set_config = camera_set_config_cam_desc;
		break;
	default:
		camera->functions->get_config = camera_get_config_default;
		camera->functions->set_config = camera_set_config_default;
		break;
	}
	CHECK_FREE (camera, gp_port_get_settings (camera->port, &s));
        switch (camera->port->type) {
        case GP_PORT_SERIAL:

		s.serial.bits     = 8;
		s.serial.parity   = 0;
		s.serial.stopbits = 1;

		/*
		 * Remember the speed. If no speed is given, we assume
		 * people want to use the highest one (i.e. 115200).
		 * However, not all machines support every speed. Check that.
		 */
		if (s.serial.speed)
			camera->pl->speed = s.serial.speed;
		else {
			int i;
			/* Find the speed into abilities.speeds array.
			 * Highest speeds are last ... so look backwards ...
			 */
			for (i=0;i<sizeof(a.speed)/sizeof(a.speed[0]) && a.speed[i];i++)
				/*empty*/;
			i--;
			for (; i >= 0 ; i--) {
				s.serial.speed = a.speed[i];
				if (gp_port_set_settings (camera->port, s) >= 0)
					break;
			}
			camera->pl->speed = (i>=0 ? a.speed[i] : 19200);
		}

		/* The camera defaults to 19200. */
                s.serial.speed = 19200;

                break;

        case GP_PORT_USB:
        case GP_PORT_USB_SCSI:

                /* Test if we have usb information */
                if (vendor == 0) {
                        free (camera->pl);
                        camera->pl = NULL;
                        return (GP_ERROR_MODEL_NOT_FOUND);
                }
		/* Use the defaults the core parsed */
                break;

        default:

                free (camera->pl);
                camera->pl = NULL;
                return (GP_ERROR_UNKNOWN_PORT);
        }

        CHECK_FREE (camera, gp_port_set_settings (camera->port, s));
        CHECK_FREE (camera, gp_port_set_timeout (camera->port, TIMEOUT));

	/*
	 * Send initialization sequence unless otherwise flagged. (The
	 * 'Polaroid PDC 2300Z' fails sierra_init as reported by William
	 * Bader <williambader@hotmail.com>).
	 */
	if (!(camera->pl->flags & SIERRA_SKIP_INIT))
		CHECK (sierra_init (camera, context));

        /* Establish a connection */
        CHECK_FREE (camera, camera_start (camera, context));

        /*
	 * USB cameras seem to need this request. If we don't request the
	 * contents of this register and directly proceed with checking for
	 * folder support, the camera doesn't send anything.
	 */
	sierra_get_int_register (camera, 1, &value, NULL);

#if 0
        /* FIXME??? What's that for? "Resetting folder system"? */
	sierra_set_int_register (camera, 83, -1, NULL);
#endif
	/* How to switch the coolpix 2500 between RAW / NORMAL
   return dsc->SetStr(0x16, raw_enabled ? "DIAG RAW" : "NIKON DIGITAL CAMERA" );
        ret = sierra_set_string_register (camera, 0x16, "NIKON DIGITAL CAMERA", strlen("NIKON DIGITAL CAMERA"), NULL);
	 */

        /* Folder support? */
	CHECK_STOP_FREE (camera, gp_port_set_timeout (camera->port, 50));
        ret = sierra_set_string_register (camera, 84, "\\", 1, NULL);
        if (ret != GP_OK) {
                camera->pl->folders = 0;
                GP_DEBUG ("*** folder support: no");
        } else {
		camera->pl->folders = 1;
                GP_DEBUG ("*** folder support: yes");
        }
        CHECK_STOP_FREE (camera, gp_port_set_timeout (camera->port, TIMEOUT));

        /* We start off not knowing where we are */
        strcpy (camera->pl->folder, "");
        CHECK_STOP_FREE (camera, gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera));

        CHECK (camera_stop (camera, context));

	GP_DEBUG ("****************** sierra initialization OK");
	return (GP_OK);
}

