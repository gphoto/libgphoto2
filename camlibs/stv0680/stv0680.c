/*
 * STV0680 Vision Camera Chipset Driver
 * Copyright 2000 Adam Harrison <adam@antispin.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-library.h>

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

#include "stv0680.h"
#include "library.h"

#define GP_MODULE "stv0680"

static const struct camera_to_usb {
	  char *name;
	  unsigned short idVendor;
	  unsigned short idProduct;
	  int serial;
} camera_to_usb[] = {
        /* http://www.vvl.co.uk/products/co-processors/680/680.htm */
	{ "STM:USB Dual-mode camera",   0x0553, 0x0202, 1 },
	{ "STV0680",                    0x0553, 0x0202, 1 },

	/* You can search in google for them, using either:
	 * 	AAA 80 CIF
	 * or
	 * 	AAA 26 VGA
	 * It has been rebranded and rereleased dozens of times.
	 */

	/* According to: http://www.bazelet.com/generic74.html */
	{ "SpyPen:Axys",                0x0553, 0x0202, 0 },
	{ "SpyPen:Cleo",                0x0553, 0x0202, 0 },
	{ "SpyPen:Xion",                0x0553, 0x0202, 0 },
	{ "SpyPen:Memo",                0x0553, 0x0202, 0 },

	/* Tested by Priit Laes <amd@tt.ee> */
	{ "SpyPen:Luxo",                0x0553, 0x0202, 0 },

	/* http://www.iomagic.com/support/digitalcameras/magicimage400/magicimage400manual.htm */
	{ "IOMagic:MagicImage 400",     0x0553, 0x0202, 0 },
	/* http://www.pctekonline.com/phoebsmardig.html */
	{ "Phoebe:Smartcam",            0x0553, 0x0202, 0 },
	{ "QuickPix:QP1",               0x0553, 0x0202, 0 },
	{ "Hawking:DC120 Pocketcam",    0x0553, 0x0202, 0 },
	{ "Aiptek:PenCam Trio",         0x0553, 0x0202, 0 },
	/* reported by Klaus-M. Klingsporn */
	{ "Aiptek:PalmCam Trio",        0x0553, 0x0202, 0 },
	/* Made by Medion (ALDI hw reseller). Their homepage is broken, so
	 * no URL. This is the camera I have -Marcus. CIF */
	{ "Micromaxx:Digital Camera",   0x0553, 0x0202, 0 },

	/* http://www.digitaldreamco.com/shop/elegante.htm, VGA */
       	{ "DigitalDream:l'elegante", 0x0553, 0x0202, 0 },
	/* http://www.digitaldreamco.com/shop/espion.htm, CIF */
      	{ "DigitalDream:l'espion",   0x0553, 0x0202, 0 },
	/* http://www.digitaldreamco.com/shop/lesprit.html, CIF */
      	{ "DigitalDream:l'esprit",   0x0553, 0x0202, 0 },
	/* http://www.digitaldreamco.com/shop/laronde.htm, VGA */
      	{ "DigitalDream:la ronde",   0x0553, 0x0202, 0 },
	/* https://sf.net/tracker/index.php?func=detail&aid=1000498&group_id=8874&atid=358874 */
	{ "DigitalDream:l'espion XS", 0x1183, 0x0001, 0 },


	/* reported by jerry white */
	{ "Argus:DC-1500",		0x0553, 0x0202, 1 },
	/* reported by Philippe Libat <philippe@mandrakesoft.com>,
	 * serial only. */
	{ "Yahoo!Cam",			0x0   , 0x0   , 1 },

	{ "AEG:Snap 300",               0x0553, 0x0202, 0 },

	/* ALDI (german discounter) version */
	{ "Pencam:TEVION MD 9456",	0x0553, 0x0202, 0 },

	/* http://www.umax.de/digicam/AstraPen_SL.htm.
	 * There is an additional 100K (CIF), 300K (VGA) tag after the name. */
	{ "UMAX:AstraPen",              0x0553, 0x0202, 0 }, /* SV and SL */

	{ "Fuji:IX-1",                  0x0553, 0x0202, 0 }, /* Unconfirmed */
	/* Added for the konica e-mini by Roland Marcus Rutschmann<Rutschmann@gmx.de>*/
	{ "Konica:e-mini",              0x04c8, 0x0722, 0 },

	{ "Che-ez!:Babe",               0x0553, 0x0202, 0 },
	{ "Che-ez!:SPYZ",               0x0553, 0x0202, 0 },

	/* According to Sonic Tsunami <mmcburn86@earthlink.net> */
	/* http://www.timlex.com/cif.html */
	{ "Timlex:CP075",		0x0553, 0x0202, 1 },

	{ "Creative:Go Mini",		0x041e, 0x4007, 1 }
};

int camera_id (CameraText *id)
{
	strcpy(id->text, "STV0680");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;
	unsigned int i;

	/* 10/11/01/cw rewritten to add cameras via array */

	for (i = 0; i < sizeof(camera_to_usb) / sizeof(struct camera_to_usb); i++) {

		memset(&a, 0, sizeof(a));
		strcpy(a.model, camera_to_usb[i].name);

		a.port			= 0;
		/* serial status is experimental, usb status is testing */
		a.status		= GP_DRIVER_STATUS_EXPERIMENTAL;
		a.operations		= GP_OPERATION_CAPTURE_IMAGE;
		a.file_operations	= GP_FILE_OPERATION_PREVIEW;
		a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;

		if (camera_to_usb[i].idVendor) {
			a.status = GP_DRIVER_STATUS_PRODUCTION;
			a.port     |= GP_PORT_USB;
			a.operations |= GP_OPERATION_CAPTURE_PREVIEW;
			a.usb_vendor  = camera_to_usb[i].idVendor;
			a.usb_product = camera_to_usb[i].idProduct;
		}
		if (camera_to_usb[i].serial) {
			a.port     |= GP_PORT_SERIAL;
			a.speed[0] = 115200;
			a.speed[1] = 0;
		}
		gp_abilities_list_append(list, a);
	}

	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context)
{
	Camera *camera = data;
	int count, result;

	result = stv0680_file_count(camera->port, &count);
	if (result != GP_OK)
		return result;

	gp_list_populate(list, "image%03i.pnm", count);

	return (GP_OK);
}



static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
	int image_no, result;

	image_no = gp_filesystem_number(camera->fs, folder, filename, context);
	if(image_no < 0)
		return image_no;

	gp_file_set_mime_type (file, GP_MIME_PNM);
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		result = stv0680_get_image (camera->port, image_no, file);
		break;
	case GP_FILE_TYPE_RAW:
		result = stv0680_get_image_raw (camera->port, image_no, file);
		break;
	case GP_FILE_TYPE_PREVIEW:
		result = stv0680_get_image_preview (camera->port, image_no, file);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
	return result;
}

static int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
{
	int result;
	int count,oldcount;

	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);
	result = stv0680_file_count(camera->port,&oldcount);

	result = stv0680_capture(camera->port);
	if (result < 0)
		return result;

	/* Just added a new picture... */
	result = stv0680_file_count(camera->port,&count);
	if (count == oldcount)
	    return GP_ERROR; /* unclear what went wrong  ... hmm */
	strcpy(path->folder,"/");
	sprintf(path->name,"image%03i.pnm",count);

	/* Tell the filesystem about it */
	result = gp_filesystem_append (camera->fs, path->folder, path->name,
				       context);
	if (result < 0)
		return (result);

	return (GP_OK);
}

static int camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	char *data;
	int size, result;

	result = stv0680_capture_preview (camera->port, &data, &size);
	if (result < 0)
		return result;

	gp_file_set_mime_type (file, GP_MIME_PNM);
	gp_file_set_data_and_size (file, data, size);
	return (GP_OK);
}

static int camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	stv0680_summary(camera->port,summary->text);
	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text,
		_("STV0680\n"
		"Adam Harrison <adam@antispin.org>\n"
		"Driver for cameras using the STV0680 processor ASIC.\n"
		"Protocol reverse engineered using CommLite Beta 5\n"
		"Carsten Weinholz <c.weinholz@netcologne.de>\n"
		"Extended for Aiptek PenCam and other STM USB Dual-mode cameras."));

	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char* folder, void *data,
		 GPContext *context)
{
        Camera *camera = data;
	if (strcmp (folder, "/"))
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	return stv0680_delete_all(camera->port);
}

static int
storage_info_func (CameraFilesystem *fs,
                CameraStorageInformation **sinfos,
                int *nrofsinfos,
                void *data, GPContext *context
) {
    Camera *camera = (Camera*)data;
    GPPort *port = camera->port;
    struct stv680_camera_info caminfo;
    struct stv680_image_info imginfo;
    int ret;
    CameraStorageInformation *sinfo;

    /* Get Camera Information */
    if ((ret = stv0680_try_cmd(port, CMDID_GET_CAMERA_INFO,
				0, (void*)&caminfo, sizeof(caminfo)) < 0))
	return ret;

    sinfo = malloc(sizeof(CameraStorageInformation));
    if (!sinfo) return GP_ERROR_NO_MEMORY;

    *sinfos = sinfo;
    *nrofsinfos = 1;

    sinfo->fields  = GP_STORAGEINFO_BASE;
    strcpy(sinfo->basedir, "/");

    sinfo->fields |= GP_STORAGEINFO_ACCESS;
    sinfo->access  = GP_STORAGEINFO_AC_READONLY_WITH_DELETE;
    sinfo->fields |= GP_STORAGEINFO_STORAGETYPE;
    sinfo->type    = GP_STORAGEINFO_ST_FIXED_RAM;
    sinfo->fields |= GP_STORAGEINFO_FILESYSTEMTYPE;
    sinfo->fstype  = GP_STORAGEINFO_FST_GENERICFLAT;

    sinfo->fields |= GP_STORAGEINFO_MAXCAPACITY;
    if (caminfo.hardware_config & HWCONFIG_MEMSIZE_16MBIT)
        sinfo->capacitykbytes = 16*1024/8;
    else
        sinfo->capacitykbytes = 64*1024/8;

    if ((ret = stv0680_try_cmd(port, CMDID_GET_IMAGE_INFO, 0,
		    (void*)&imginfo, sizeof(imginfo))!=GP_OK))
	return ret;

    sinfo->fields |= GP_STORAGEINFO_FREESPACEIMAGES;
    sinfo->freeimages =
	    ((imginfo.maximages[0]<<8)|imginfo.maximages[1]) -
	    ((imginfo.index[0]<<8)|imginfo.index[1]);
    return GP_OK;
}


static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.delete_all_func = delete_all_func,
	.storage_info_func = storage_info_func
};

int camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;

        /* First, set up all the function pointers */
        camera->functions->summary              = camera_summary;
        camera->functions->about                = camera_about;
	camera->functions->capture_preview	= camera_capture_preview;
	camera->functions->capture		= camera_capture;

	gp_port_get_settings(camera->port, &settings);
	switch(camera->port->type) {
	case GP_PORT_SERIAL:
        	gp_port_set_timeout(camera->port, 1000);
		settings.serial.speed = 115200;
        	settings.serial.bits = 8;
        	settings.serial.parity = 0;
        	settings.serial.stopbits = 1;
		break;
	case GP_PORT_USB:
		/* Use the defaults the core parsed */
		break;
	default:
		return (GP_ERROR_UNKNOWN_PORT);
		break;
	}
	gp_port_set_settings(camera->port, settings);

	/* Set up the filesystem */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

        /* test camera */
        return stv0680_ping(camera->port);
}
