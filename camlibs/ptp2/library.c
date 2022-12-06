/* library.c
 *
 * Copyright (C) 2001-2005 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2021 Marcus Meissner <marcus@jet.franken.de>
 * Copyright (C) 2005 Hubert Figuiere <hfiguiere@teaser.fr>
 * Copyright (C) 2009 Axel Waggershauser <awagger@web.de>
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
#define _DARWIN_C_SOURCE
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
#include <langinfo.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

#include "libgphoto2/i18n.h"

#include "ptp.h"
#include "ptp-bugs.h"
#include "ptp-private.h"
#include "ptp-pack.c"
#include "olympus-wrap.h"

#ifdef HAVE_LIBWS232
#include <winsock2.h>
#endif

#define USB_START_TIMEOUT 8000
#define USB_CANON_START_TIMEOUT 1500	/* 1.5 seconds (0.5 was too low) */
#define USB_NORMAL_TIMEOUT 20000
static int normal_timeout = USB_NORMAL_TIMEOUT;
#define USB_TIMEOUT_CAPTURE 100000
static int capture_timeout = USB_TIMEOUT_CAPTURE;

#define	SET_CONTEXT(camera, ctx) ((PTPData *) camera->pl->params.data)->context = ctx
#define	SET_CONTEXT_P(p, ctx) ((PTPData *) p->data)->context = ctx

/* below macro makes a copy of fn without leading character ('/'),
 * removes the '/' at the end if present, and calls folder_to_handle()
 * function proviging as the first argument the string after the second '/'.
 * for example if fn is '/store_00010001/DCIM/somefolder/', the macro will
 * call folder_to_handle() with 'DCIM/somefolder' as the very first argument.
 * it's used to omit storage pseudofolder and remove trailing '/'
 */

#define find_folder_handle(params,fn,s,p)	{		\
		{						\
		int len=strlen(fn);				\
		char *backfolder=malloc(len);			\
		char *tmpfolder;				\
		memcpy(backfolder,fn+1, len);			\
		if (backfolder[len-2]=='/') backfolder[len-2]='\0';\
		if ((tmpfolder=strchr(backfolder+1,'/'))==NULL) tmpfolder="/";\
		p=folder_to_handle(params, tmpfolder+1,s,0,NULL);\
		free(backfolder);				\
		}						\
}

#define folder_to_storage(fn,s) {				\
		if (!strncmp(fn,"/"STORAGE_FOLDER_PREFIX,strlen(STORAGE_FOLDER_PREFIX)+1))							\
		{						\
			if (strlen(fn)<strlen(STORAGE_FOLDER_PREFIX)+8+1) \
				return (GP_ERROR);		\
			s = strtoul(fn + strlen(STORAGE_FOLDER_PREFIX)+1, NULL, 16);								\
		} else { 					\
			gp_context_error (context, _("You need to specify a folder starting with /store_xxxxxxxxx/"));				\
			return (GP_ERROR);			\
		}						\
}

typedef int (*getfunc_t)(CameraFilesystem*, const char*, const char *, CameraFileType, CameraFile *, void *, GPContext *);
typedef int (*putfunc_t)(CameraFilesystem*, const char*, CameraFile*, void*, GPContext*);

struct special_file {
	char		*name;
	getfunc_t	getfunc;
	putfunc_t	putfunc;
};

static unsigned int nrofspecial_files = 0;
static struct special_file *special_files = NULL;

static int
add_special_file (char *name, getfunc_t getfunc, putfunc_t putfunc) {
	C_MEM (special_files = realloc (special_files, sizeof(special_files[0])*(nrofspecial_files+1)));
	C_MEM (special_files[nrofspecial_files].name = strdup(name));
	special_files[nrofspecial_files].putfunc = putfunc;
	special_files[nrofspecial_files].getfunc = getfunc;
	nrofspecial_files++;
	return (GP_OK);
}

#define STORAGE_FOLDER_PREFIX		"store_"

int
translate_ptp_result (uint16_t result)
{
	switch (result) {
	case PTP_RC_OK:				return GP_OK;
	case PTP_RC_ParameterNotSupported:	return GP_ERROR_BAD_PARAMETERS;
	case PTP_RC_OperationNotSupported:	return GP_ERROR_NOT_SUPPORTED;
	case PTP_RC_DeviceBusy:			return GP_ERROR_CAMERA_BUSY;
	case PTP_ERROR_NODEVICE:		return GP_ERROR_IO_USB_FIND;
	case PTP_ERROR_TIMEOUT:			return GP_ERROR_TIMEOUT;
	case PTP_ERROR_CANCEL:			return GP_ERROR_CANCEL;
	case PTP_ERROR_BADPARAM:		return GP_ERROR_BAD_PARAMETERS;
	case PTP_ERROR_IO:
	case PTP_ERROR_DATA_EXPECTED:
	case PTP_ERROR_RESP_EXPECTED:		return GP_ERROR_IO;
	default:				return GP_ERROR;
	}
}

uint16_t
translate_gp_result_to_ptp (int gp_result)
{
	switch (gp_result) {
	case GP_OK:				return PTP_RC_OK;
	case GP_ERROR_BAD_PARAMETERS:		return PTP_RC_ParameterNotSupported;
	case GP_ERROR_NOT_SUPPORTED:		return PTP_RC_OperationNotSupported;
	case GP_ERROR_CAMERA_BUSY:		return PTP_RC_DeviceBusy;
	case GP_ERROR_IO_USB_FIND:		return PTP_ERROR_NODEVICE;
	case GP_ERROR_TIMEOUT:			return PTP_ERROR_TIMEOUT;
	case GP_ERROR_CANCEL:			return PTP_ERROR_CANCEL;
	case GP_ERROR_IO:			return PTP_ERROR_IO;
	default:				return PTP_RC_GeneralError;
	}
}

static void
print_debug_deviceinfo (PTPParams *params, PTPDeviceInfo *di)
{
	unsigned int i;

	GP_LOG_D ("Device info:");
	GP_LOG_D ("Manufacturer: %s",di->Manufacturer);
	GP_LOG_D ("  Model: %s", di->Model);
	GP_LOG_D ("  device version: %s", di->DeviceVersion);
	GP_LOG_D ("  serial number: '%s'",di->SerialNumber);
	GP_LOG_D ("Vendor extension ID: 0x%08x",di->VendorExtensionID);
	GP_LOG_D ("Vendor extension version: %d",di->VendorExtensionVersion);
	GP_LOG_D ("Vendor extension description: %s",di->VendorExtensionDesc);
	GP_LOG_D ("Functional Mode: 0x%04x",di->FunctionalMode);
	GP_LOG_D ("PTP Standard Version: %d",di->StandardVersion);
	GP_LOG_D ("Supported operations:");
	for (i=0; i<di->OperationsSupported_len; i++)
		GP_LOG_D ("  0x%04x (%s)", di->OperationsSupported[i], ptp_get_opcode_name (params, di->OperationsSupported[i]));
	GP_LOG_D ("Events Supported:");
	for (i=0; i<di->EventsSupported_len; i++)
		GP_LOG_D ("  0x%04x (%s)", di->EventsSupported[i], ptp_get_event_code_name (params, di->EventsSupported[i]));
	GP_LOG_D ("Device Properties Supported:");
	for (i=0; i<di->DevicePropertiesSupported_len; i++) {
		const char *ptpname = ptp_get_property_description (params, di->DevicePropertiesSupported[i]);
		GP_LOG_D ("  0x%04x (%s)", di->DevicePropertiesSupported[i], ptpname ? ptpname : "Unknown DPC code");
	}
}

/* struct timeval is simply two long int values, so passing it by value is not expensive.
 * It is most likely going to be inlined anyway and therefore 'free'. Passing it by value
 * leads to a cleaner interface. */
static struct timeval
time_now() {
	struct timeval curtime;
	gettimeofday (&curtime, NULL);
	return curtime;
}

static int
time_since (const struct timeval start) {
	struct timeval curtime = time_now();
	return ((curtime.tv_sec - start.tv_sec)*1000)+((curtime.tv_usec - start.tv_usec)/1000);
}

static int
waiting_for_timeout (int *current_wait, struct timeval start, int timeout) {
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        int time_to_timeout = timeout - time_since (start);

	if (time_to_timeout <= 0) /* we timed out already ... */
		return 0;
        *current_wait += 50; /* increase sleep time by 50ms per cycle */
        if (*current_wait > 200)
                *current_wait = 200; /* 200ms is the maximum sleep time */
        if (*current_wait > time_to_timeout)
                *current_wait = time_to_timeout; /* never sleep 'into' the timeout */
        if (*current_wait > 0)
                usleep (*current_wait * 1000);
        return *current_wait > 0;
#else
	/* Wait always timeout during fuzzing! */
	return 0;
#endif
}

/* Changes the ptp deviceinfo with additional hidden information available,
 * or stuff that requires special tricks
 */
int
fixup_cached_deviceinfo (Camera *camera, PTPDeviceInfo *di) {
	CameraAbilities a;
	PTPParams	*params = &camera->pl->params;

        gp_camera_get_abilities(camera, &a);

	/* Panasonic GH5, GC9 */
	if (    (di->VendorExtensionID == PTP_VENDOR_PANASONIC) &&
		(camera->port->type == GP_PORT_USB) &&
		(a.usb_product == 0x2382)
	) {
		C_MEM (di->OperationsSupported = realloc(di->OperationsSupported,sizeof(di->OperationsSupported[0])*(di->OperationsSupported_len + 9)));
		di->OperationsSupported[di->OperationsSupported_len+0] = PTP_OC_PANASONIC_GetProperty;
		di->OperationsSupported[di->OperationsSupported_len+1]  = PTP_OC_PANASONIC_SetProperty;
		di->OperationsSupported[di->OperationsSupported_len+2]  = PTP_OC_PANASONIC_ListProperty;
		di->OperationsSupported[di->OperationsSupported_len+3]  = PTP_OC_PANASONIC_InitiateCapture;
		di->OperationsSupported[di->OperationsSupported_len+4]  = PTP_OC_PANASONIC_Liveview;
		di->OperationsSupported[di->OperationsSupported_len+5]  = PTP_OC_PANASONIC_LiveviewImage;
		di->OperationsSupported[di->OperationsSupported_len+6]  = PTP_OC_PANASONIC_MovieRecControl;
		di->OperationsSupported[di->OperationsSupported_len+7]  = PTP_OC_PANASONIC_GetLiveViewParameters;
		di->OperationsSupported[di->OperationsSupported_len+8]  = PTP_OC_PANASONIC_SetLiveViewParameters;
		di->OperationsSupported_len += 9;
	}

	/* Panasonic hack */
	if (	(di->VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
		(camera->port->type == GP_PORT_USB) &&
		(a.usb_vendor == 0x04da)
	) {
		PTPPropertyValue propval;
		/* Panasonic changes its device info if the MTP Initiator
		 * is set, and e.g. adds DeleteObject.
		 * (found in Windows USB traces) */

		if (!ptp_property_issupported(params, PTP_DPC_MTP_SessionInitiatorInfo))
			return GP_OK;

		propval.str = "Windows/6.2.9200 MTPClassDriver/6.2.9200.16384";

		C_PTP (ptp_setdevicepropvalue (params, PTP_DPC_MTP_SessionInitiatorInfo, &propval, PTP_DTC_STR));
		C_PTP (ptp_getdeviceinfo (params, di));
		return GP_OK;
	}
	/* SIGMA FP */
	if (	(di->VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
		(camera->port->type == GP_PORT_USB) &&
		(a.usb_vendor == 0x1003)
	) {
		di->VendorExtensionID = PTP_VENDOR_GP_SIGMAFP;
		return GP_OK;
	}

	/* LEICA */
	if (    (di->VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
		(camera->port->type == GP_PORT_USB) &&
		(a.usb_vendor == 0x1a98)
	) {
		di->VendorExtensionID = PTP_VENDOR_GP_LEICA;
	}

	/* XML style Olympus E series control. internal deviceInfos is encoded in XML. */
	if (	di->Manufacturer && !strcmp(di->Manufacturer,"OLYMPUS") &&
		(params->device_flags & DEVICE_FLAG_OLYMPUS_XML_WRAPPED)
	) {
		PTPDeviceInfo	ndi, newdi, *outerdi;
		unsigned int	i;

		C_PTP (ptp_getdeviceinfo (params, &params->outer_deviceinfo));
		outerdi = &params->outer_deviceinfo;

		C_PTP (ptp_olympus_getdeviceinfo (&camera->pl->params, &ndi));

		/* Now merge the XML (inner) and outer (PictBridge) Deviceinfo. */
		memcpy (&newdi, outerdi, sizeof(PTPDeviceInfo));

		/* dup the strings */
		if (outerdi->VendorExtensionDesc)	C_MEM (newdi.VendorExtensionDesc = strdup (outerdi->VendorExtensionDesc));
		if (outerdi->Manufacturer)		C_MEM (newdi.Manufacturer = strdup (outerdi->Manufacturer));
		if (outerdi->Model)			C_MEM (newdi.Model = strdup (outerdi->Model));
		if (outerdi->DeviceVersion)		C_MEM (newdi.DeviceVersion = strdup (outerdi->DeviceVersion));
		if (outerdi->SerialNumber)		C_MEM (newdi.SerialNumber = strdup (outerdi->SerialNumber));

		/* Dup and merge the lists */
#define DI_MERGE(x) \
		C_MEM (newdi.x = calloc(sizeof(outerdi->x[0]),(ndi.x##_len + outerdi->x##_len)));\
		for (i = 0; i < outerdi->x##_len ; i++) 					\
			newdi.x[i] = outerdi->x[i];						\
		for (i = 0; i < ndi.x##_len ; i++)						\
			newdi.x[i+outerdi->x##_len] = ndi.x[i];					\
		newdi.x##_len = ndi.x##_len + outerdi->x##_len;

		DI_MERGE(OperationsSupported);
		DI_MERGE(EventsSupported);
		DI_MERGE(DevicePropertiesSupported);
		DI_MERGE(CaptureFormats);
		DI_MERGE(ImageFormats);

		/* libgphoto2 specific for usage in config trees */
		newdi.VendorExtensionID = PTP_VENDOR_GP_OLYMPUS;

		GP_LOG_D ("Dumping Olympus Deviceinfo");


		print_debug_deviceinfo (params, &newdi);
		ptp_free_DI (di);
		memcpy (di, &newdi, sizeof(newdi));
		return GP_OK;
	}

	/* for USB class matches on unknown cameras that were matches with PTP generic... */
	if (!a.usb_vendor && di->Manufacturer) {
		if (strstr (di->Manufacturer,"Canon"))
			a.usb_vendor = 0x4a9;
		if (strstr (di->Manufacturer,"Nikon"))
			a.usb_vendor = 0x4b0;
		if (strstr (di->Manufacturer,"FUJIFILM"))
			a.usb_vendor = 0x4cb;
	}
	/* Switch the PTP vendor, so that the vendor specific sets become available. */
	if (	(di->VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
		di->Manufacturer
	) {
		if (strstr (di->Manufacturer,"Canon"))
			di->VendorExtensionID = PTP_VENDOR_CANON;
		if (strstr (di->Manufacturer,"Nikon"))
			di->VendorExtensionID = PTP_VENDOR_NIKON;
	}

	/* Newer Canons say that they are MTP devices. Restore Canon vendor extid. */
	if (	(	(di->VendorExtensionID == PTP_VENDOR_MICROSOFT)  ||
			(di->VendorExtensionID == PTP_VENDOR_MTP)
		) &&
		(	(camera->port->type == GP_PORT_USB) ||
			(camera->port->type == GP_PORT_PTPIP)
		) &&
		(a.usb_vendor == 0x4a9)
	) {
		/*camera->pl->bugs |= PTP_MTP;*/
		di->VendorExtensionID = PTP_VENDOR_CANON;
	}

	/* Newer Nikons (D40) say that they are MTP devices. Restore Nikon vendor extid. */
	if (	(	(di->VendorExtensionID == PTP_VENDOR_MICROSOFT) ||
			(di->VendorExtensionID == PTP_VENDOR_MTP)
		) &&
		(	(camera->port->type == GP_PORT_USB) ||
			(camera->port->type == GP_PORT_PTPIP)
		) &&
		(a.usb_vendor == 0x4b0)
	) {
		/*camera->pl->bugs |= PTP_MTP;*/
		di->VendorExtensionID = PTP_VENDOR_NIKON;
	}

	/* Fuji S5 Pro mostly, make its vendor set available. */
	if (	(di->VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
		(camera->port->type == GP_PORT_USB) &&
		(a.usb_vendor == 0x4cb) &&
		strstr(di->VendorExtensionDesc,"fujifilm.co.jp: 1.0;")
	) {
		/*camera->pl->bugs |= PTP_MTP;*/
		di->VendorExtensionID = PTP_VENDOR_FUJI;
	}

	if (    di->Manufacturer && !strcmp(di->Manufacturer,"OLYMPUS") && !strncmp(di->Model,"E-M",3)  ) {
		GP_LOG_D ("Setting Olympus VendorExtensionID to PTP_VENDOR_GP_OLYMPUS_OMD");
		di->VendorExtensionID = PTP_VENDOR_GP_OLYMPUS_OMD;
	}

	if (di->VendorExtensionID == PTP_VENDOR_FUJI) {
		C_MEM (di->DevicePropertiesSupported = realloc(di->DevicePropertiesSupported,sizeof(di->DevicePropertiesSupported[0])*(di->DevicePropertiesSupported_len + 60)));
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+0] = PTP_DPC_ExposureTime;
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+1] = PTP_DPC_FNumber;
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+2] = 0xd38c;	/* PC Mode */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+3] = 0xd171;	/* Focus control */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+4] = 0xd21c;	/* Needed for X-T2? */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+5] = 0xd347;	/* Focus Position */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+6] = PTP_DPC_FUJI_LensZoomPos;
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+7] = 0xd242;
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+8] = PTP_DPC_FUJI_LiveViewImageSize; /* xt3 confirmed */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+9] = 0xd168; /* video out on/off (unconfirmed) */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+10] = PTP_DPC_FUJI_LiveViewImageQuality; /* xt3 confirmed */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+11] = PTP_DPC_FUJI_ForceMode; /* on xt3 set by webcam app to 1 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+12] = 0xd16e; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+13] = 0xd372; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+14] = 0xd020; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+15] = 0xd022; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+16] = 0xd023; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+17] = 0xd024; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+18] = 0xd025; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+19] = 0xd026; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+20] = 0xd027; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+21] = 0xd029; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+22] = 0xd16f; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+23] = 0xd02f; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+24] = 0xd395; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+25] = 0xd320; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+26] = 0xd321; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+27] = 0xd322; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+28] = 0xd323; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+29] = 0xd346; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+30] = 0xd34a; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+31] = 0xd34b; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+32] = 0xd34d; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+33] = 0xd351; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+34] = 0xd35e; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+35] = 0xd173; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+36] = 0xd365; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+37] = 0xd366; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+38] = 0xd374; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+39] = 0xd310; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+40] = 0xd359; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+41] = 0xd375; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+42] = 0xd376; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+43] = 0xd36e; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+44] = 0xd33f; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+45] = 0xd364; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+46] = 0xd34e; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+47] = 0xd02e; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+48] = 0xd36d; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+49] = 0xd38a; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+50] = 0xd36a; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+51] = 0xd36b; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+52] = 0xd36f; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+53] = 0xd370; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+54] = 0xd222; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+55] = 0xd223; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+56] = 0xd38c; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+57] = 0xd38d; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+58] = 0xd38e; /* seen on xt3 */
		di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+59] = 0xd17b; /* seen on xt3 */
		di->DevicePropertiesSupported_len += 60;

		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_FUJI_GetDeviceInfo)) {
			uint16_t	*props;
			unsigned int	numprops;

			C_PTP (ptp_fuji_getdeviceinfo (params, &props, &numprops));
			free (di->DevicePropertiesSupported);

			di->DevicePropertiesSupported		= props;
			di->DevicePropertiesSupported_len	= numprops;
		}

	}

	/* Nikon DSLR hide its newer opcodes behind another vendor specific query,
	 * do that and merge it into the generic PTP deviceinfo. */
	if (di->VendorExtensionID == PTP_VENDOR_NIKON) {
		unsigned int i;
		unsigned int nikond;

		/* Nikon V* and J* advertise the new Nikon stuff, but only do the generic
		 * PTP capture. FIXME: could use flags. */
		if (params->deviceinfo.Model && (
			(params->deviceinfo.Model[0]=='J') ||
			(params->deviceinfo.Model[0]=='V') ||
			((params->deviceinfo.Model[0]=='S') && strlen(params->deviceinfo.Model) < 3)	/* S1 - S2 currently */
				/* but not S7000 */
			)
		) {
			if (!NIKON_1(&camera->pl->params)) {
				GP_LOG_E ("if camera is Nikon 1 series, camera should probably have flag NIKON_1 set. report that to the libgphoto2 project");
				camera->pl->params.device_flags |= PTP_NIKON_1;
			}
			C_MEM (di->OperationsSupported = realloc(di->OperationsSupported,sizeof(di->OperationsSupported[0])*(di->OperationsSupported_len + 3)));
			/* Nikon J5 does not advertise the PTP_OC_NIKON_InitiateCaptureRecInMedia cmd ... gnh */
			/* logic: If we have one 0x920x command, we will probably have 0x9207 too. and getvendorpropcodes ... */

			/* Marcus note: GetVendorPropCodes crashes at least the V1, the J1 and J2.
			 * but it works on the J3, J4...
 			 * V1: crashes protocol flow
 			 * J1: crashes protocol flow
 			 * J2: crashes protocol flow
 			 * J3: works
 			 * J4: works
			 * J5: reports invalid opcode
			 */
			if (	!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_InitiateCaptureRecInMedia) &&
				 ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_StartLiveView)
			) {
				di->OperationsSupported[di->OperationsSupported_len+0] = PTP_OC_NIKON_InitiateCaptureRecInMedia;
				di->OperationsSupported_len++;
			}

			if (ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_InitiateCaptureRecInMedia)) {
				di->OperationsSupported[di->OperationsSupported_len+0] = PTP_OC_NIKON_GetVendorPropCodes;
				di->OperationsSupported_len++;
			}

			/* V1 and J1 are a less reliable then the newer 1 versions, no changecamera mode, no getevent, no initiatecapturerecinsdram */
			if (	!strcmp(params->deviceinfo.Model,"V1") ||
				!strcmp(params->deviceinfo.Model,"S1") ||
				!strcmp(params->deviceinfo.Model,"J1")
			) {
				/* on V1 and J1 even the 90c7 getevents does not work */
				/* V1: see https://github.com/gphoto/libgphoto2/issues/569 */
				/* J1: see https://github.com/gphoto/libgphoto2/issues/716 */
				/* S1: see https://github.com/gphoto/libgphoto2/issues/845 */
				for (i=0;i<di->OperationsSupported_len;i++) {
					if (di->OperationsSupported[i] == PTP_OC_NIKON_GetEvent) {
						GP_LOG_D("On Nikon S1/J1/V1: disable NIKON_GetEvent as its unreliable");
						di->OperationsSupported[i] = PTP_OC_GetDeviceInfo; /* overwrite */
					}
					if (di->OperationsSupported[i] == PTP_OC_NIKON_InitiateCaptureRecInSdram) {
						GP_LOG_D("On Nikon S1/J1/V1: disable NIKON_InitiateCaptureRecInSdram as its unreliable");
						di->OperationsSupported[i] = PTP_OC_InitiateCapture; /* overwrite */
					}
					if (!strcmp(params->deviceinfo.Model,"S1") &&
						(di->OperationsSupported[i] == PTP_OC_NIKON_InitiateCaptureRecInMedia))
					{
						GP_LOG_D("On Nikon S1: disable NIKON_InitiateCaptureRecInMedia as its unreliable");
						di->OperationsSupported[i] = PTP_OC_InitiateCapture; /* overwrite */
					}
				}
			} else {
				di->OperationsSupported[di->OperationsSupported_len+0] = PTP_OC_NIKON_ChangeCameraMode;
				di->OperationsSupported_len++;
			}
		}
		if (params->deviceinfo.Model && !strcmp(params->deviceinfo.Model,"COOLPIX A")) {
			/* The A also hides some commands from us ... */
			if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_GetVendorPropCodes)) {
				C_MEM (di->OperationsSupported = realloc(di->OperationsSupported,sizeof(di->OperationsSupported[0])*(di->OperationsSupported_len + 10)));
				di->OperationsSupported[di->OperationsSupported_len+0] = PTP_OC_NIKON_GetVendorPropCodes;
				di->OperationsSupported[di->OperationsSupported_len+1]  = PTP_OC_NIKON_GetEvent;
				di->OperationsSupported[di->OperationsSupported_len+2]  = PTP_OC_NIKON_AfDrive;
				di->OperationsSupported[di->OperationsSupported_len+3]  = PTP_OC_NIKON_ChangeCameraMode;
				di->OperationsSupported[di->OperationsSupported_len+4]  = PTP_OC_NIKON_DeviceReady;
				di->OperationsSupported[di->OperationsSupported_len+5]  = PTP_OC_NIKON_StartLiveView;
				di->OperationsSupported[di->OperationsSupported_len+6] = PTP_OC_NIKON_EndLiveView;
				di->OperationsSupported[di->OperationsSupported_len+7] = PTP_OC_NIKON_GetLiveViewImg;
				di->OperationsSupported[di->OperationsSupported_len+8] = PTP_OC_NIKON_ChangeAfArea;
				di->OperationsSupported[di->OperationsSupported_len+9] = PTP_OC_NIKON_InitiateCaptureRecInMedia;
				di->OperationsSupported_len += 10;
			}
		}
		if (params->deviceinfo.Model && (sscanf(params->deviceinfo.Model,"D%d", &nikond)))
		{
			if ((nikond >= 3000) && (nikond < 3199)) {
				GP_LOG_D("The D3xxx series hides commands from us ... ");
				/* Most commands we guessed do not work (anymore). One user searched for those, remove the ones we do not have.
				 * https://github.com/gphoto/libgphoto2/issues/140
				 */
				if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_GetVendorPropCodes)) {
					C_MEM (di->OperationsSupported = realloc(di->OperationsSupported,sizeof(di->OperationsSupported[0])*(di->OperationsSupported_len + 6)));
					di->OperationsSupported[di->OperationsSupported_len+0]  = PTP_OC_NIKON_AfDrive;
					di->OperationsSupported[di->OperationsSupported_len+1]  = PTP_OC_NIKON_DeviceReady;
					di->OperationsSupported[di->OperationsSupported_len+2]  = PTP_OC_NIKON_GetPreviewImg;
					di->OperationsSupported[di->OperationsSupported_len+3] = PTP_OC_NIKON_MfDrive;
					di->OperationsSupported[di->OperationsSupported_len+4] = PTP_OC_NIKON_ChangeAfArea;
					di->OperationsSupported[di->OperationsSupported_len+5] = PTP_OC_NIKON_AfDriveCancel;
					di->OperationsSupported_len += 6;
				}
			}
			if ((nikond >= 3200) && (nikond < 3299)) {
				GP_LOG_D("The D3200 hides commands from us ... adding some D7100 ones");
				if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_GetVendorPropCodes)) {
					/* see https://github.com/gphoto/gphoto2/issues/331 and https://github.com/gphoto/gphoto2/issues/332 */
					C_MEM (di->OperationsSupported = realloc(di->OperationsSupported,sizeof(di->OperationsSupported[0])*(di->OperationsSupported_len + 14)));
					di->OperationsSupported[di->OperationsSupported_len+0]  = PTP_OC_NIKON_GetEvent;
					di->OperationsSupported[di->OperationsSupported_len+1]  = PTP_OC_NIKON_InitiateCaptureRecInSdram;
					di->OperationsSupported[di->OperationsSupported_len+2]  = PTP_OC_NIKON_AfDrive;
					di->OperationsSupported[di->OperationsSupported_len+3]  = PTP_OC_NIKON_ChangeCameraMode;
					di->OperationsSupported[di->OperationsSupported_len+4]  = PTP_OC_NIKON_DeviceReady;
					di->OperationsSupported[di->OperationsSupported_len+5]  = PTP_OC_NIKON_AfCaptureSDRAM;
					di->OperationsSupported[di->OperationsSupported_len+6]  = PTP_OC_NIKON_DelImageSDRAM;

					di->OperationsSupported[di->OperationsSupported_len+7]  = PTP_OC_NIKON_GetPreviewImg;
					di->OperationsSupported[di->OperationsSupported_len+8]  = PTP_OC_NIKON_StartLiveView;
					di->OperationsSupported[di->OperationsSupported_len+9]  = PTP_OC_NIKON_EndLiveView;
					di->OperationsSupported[di->OperationsSupported_len+10] = PTP_OC_NIKON_GetLiveViewImg;	/* confirmed works */
					di->OperationsSupported[di->OperationsSupported_len+11] = PTP_OC_NIKON_ChangeAfArea;
					di->OperationsSupported[di->OperationsSupported_len+12] = PTP_OC_NIKON_InitiateCaptureRecInMedia;	/* works to some degree */
					di->OperationsSupported[di->OperationsSupported_len+13] = PTP_OC_NIKON_AfDriveCancel;
					/* probably more */
					di->OperationsSupported_len += 14;

				}
			}
			if ((nikond >= 3300) && (nikond < 3999)) {
				GP_LOG_D("The D3xxx series hides commands from us ... adding all D7100 ones");
				if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_GetVendorPropCodes)) {
					C_MEM (di->OperationsSupported = realloc(di->OperationsSupported,sizeof(di->OperationsSupported[0])*(di->OperationsSupported_len + 19)));
					di->OperationsSupported[di->OperationsSupported_len+0]  = PTP_OC_NIKON_GetVendorPropCodes;
					di->OperationsSupported[di->OperationsSupported_len+1]  = PTP_OC_NIKON_GetEvent;
					di->OperationsSupported[di->OperationsSupported_len+2]  = PTP_OC_NIKON_InitiateCaptureRecInSdram;
					di->OperationsSupported[di->OperationsSupported_len+3]  = PTP_OC_NIKON_AfDrive;
					di->OperationsSupported[di->OperationsSupported_len+4]  = PTP_OC_NIKON_ChangeCameraMode;
					di->OperationsSupported[di->OperationsSupported_len+5]  = PTP_OC_NIKON_DeviceReady;
					di->OperationsSupported[di->OperationsSupported_len+6]  = PTP_OC_NIKON_AfCaptureSDRAM;
					di->OperationsSupported[di->OperationsSupported_len+7]  = PTP_OC_NIKON_DelImageSDRAM;

					di->OperationsSupported[di->OperationsSupported_len+8]  = PTP_OC_NIKON_GetPreviewImg;
					di->OperationsSupported[di->OperationsSupported_len+9]  = PTP_OC_NIKON_StartLiveView;
					di->OperationsSupported[di->OperationsSupported_len+10] = PTP_OC_NIKON_EndLiveView;
					di->OperationsSupported[di->OperationsSupported_len+11] = PTP_OC_NIKON_GetLiveViewImg;
					di->OperationsSupported[di->OperationsSupported_len+12] = PTP_OC_NIKON_MfDrive;
					di->OperationsSupported[di->OperationsSupported_len+13] = PTP_OC_NIKON_ChangeAfArea;
					di->OperationsSupported[di->OperationsSupported_len+14] = PTP_OC_NIKON_InitiateCaptureRecInMedia;
					di->OperationsSupported[di->OperationsSupported_len+15] = PTP_OC_NIKON_AfDriveCancel;
					di->OperationsSupported[di->OperationsSupported_len+16] = PTP_OC_NIKON_StartMovieRecInCard;
					di->OperationsSupported[di->OperationsSupported_len+17] = PTP_OC_NIKON_EndMovieRec;
					di->OperationsSupported[di->OperationsSupported_len+18] = PTP_OC_NIKON_TerminateCapture;
					/* probably more */
					di->OperationsSupported_len += 19;

				}
			}
		}

		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_GetVendorPropCodes)) {
			uint16_t  	*xprops;
			unsigned int	xsize;

			if (PTP_RC_OK == LOG_ON_PTP_E (ptp_nikon_get_vendorpropcodes (&camera->pl->params, &xprops, &xsize))) {
				di->DevicePropertiesSupported = realloc(di->DevicePropertiesSupported,sizeof(di->DevicePropertiesSupported[0])*(di->DevicePropertiesSupported_len + xsize));
				if (!di->DevicePropertiesSupported) {
					free (xprops);
					C_MEM (di->DevicePropertiesSupported);
				}
				for (i=0;i<xsize;i++)
					di->DevicePropertiesSupported[i+di->DevicePropertiesSupported_len] = xprops[i];
				di->DevicePropertiesSupported_len += xsize;
				free (xprops);
			}
		}

		/* For nikon 1 j5, they have blanked this space */
		if (camera->pl->params.device_flags & PTP_NIKON_1) {
			for (i=0;i<di->DevicePropertiesSupported_len;i++)
				if ((di->DevicePropertiesSupported[i] & 0xf000) == 0xf000)
					break;
			/* The J5 so far goes up to 0xf01c */
#define NIKON_1_ADDITIONAL_DEVPROPS 29
			if (i==di->DevicePropertiesSupported_len) {
				di->DevicePropertiesSupported = realloc(di->DevicePropertiesSupported,sizeof(di->DevicePropertiesSupported[0])*(di->DevicePropertiesSupported_len + NIKON_1_ADDITIONAL_DEVPROPS+3));
				if (!di->DevicePropertiesSupported) {
					C_MEM (di->DevicePropertiesSupported);
				}
				for (i=0;i<NIKON_1_ADDITIONAL_DEVPROPS;i++)
					di->DevicePropertiesSupported[i+di->DevicePropertiesSupported_len] = 0xf000 | i;

				/* is returned by the J5, but readonly */
				di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+NIKON_1_ADDITIONAL_DEVPROPS] = PTP_DPC_NIKON_ExposureTime;	// OKAY in J5
				di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+NIKON_1_ADDITIONAL_DEVPROPS+1] = PTP_DPC_NIKON_LiveViewProhibitCondition;	// hack
				di->DevicePropertiesSupported[di->DevicePropertiesSupported_len+NIKON_1_ADDITIONAL_DEVPROPS+2] = PTP_DPC_NIKON_LiveViewStatus;	// hack

				di->DevicePropertiesSupported_len += NIKON_1_ADDITIONAL_DEVPROPS + 3;
			}
		}

#if 0
		if (!ptp_operation_issupported(&camera->pl->params, 0x9207)) {
			C_MEM (di->OperationsSupported = realloc(di->OperationsSupported,sizeof(di->OperationsSupported[0])*(di->OperationsSupported_len + 2)));
			di->OperationsSupported[di->OperationsSupported_len+0] = PTP_OC_NIKON_InitiateCaptureRecInSdram;
			di->OperationsSupported[di->OperationsSupported_len+1] = PTP_OC_NIKON_AfCaptureSDRAM;
			di->OperationsSupported_len+=2;
		}
#endif
	}

	/* Mostly for PTP/IP mode */
	if (di->VendorExtensionID == PTP_VENDOR_MTP && di->Manufacturer && !strcmp(di->Manufacturer,"Sony Corporation")) {
		di->VendorExtensionID = PTP_VENDOR_SONY;
	}

	/* Sony DSLR also hide its newer opcodes behind another vendor specific query,
	 * do that and merge it into the generic PTP deviceinfo. */
	if (di->VendorExtensionID == PTP_VENDOR_SONY) {
		unsigned int i;

		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_SONY_GetSDIOGetExtDeviceInfo)) {
			int opcodes = 0, propcodes = 0, events = 0, j,k,l;
			uint16_t  	*xprops;
			unsigned int	xsize;
			PTPPropertyValue propval;

			C_PTP (ptp_sony_sdioconnect (&camera->pl->params, 1, 0, 0));
			C_PTP (ptp_sony_sdioconnect (&camera->pl->params, 2, 0, 0));
			C_PTP (ptp_sony_get_vendorpropcodes (&camera->pl->params, &xprops, &xsize));

			for (i=0;i<xsize;i++) {
				switch (xprops[i] & 0x7000) {
				case 0x1000: opcodes++; break;
				case 0x4000: events++; break;
				case 0x5000: propcodes++; break;
				default:
					GP_LOG_E ("ptp_sony_get_vendorpropcodes() unknown opcode %x", xprops[i]);
					break;
				}
			}
			C_MEM (di->DevicePropertiesSupported = realloc(di->DevicePropertiesSupported,sizeof(di->DevicePropertiesSupported[0])*(di->DevicePropertiesSupported_len + propcodes)));
			C_MEM (di->OperationsSupported       = realloc(di->OperationsSupported,      sizeof(di->OperationsSupported[0])*(di->OperationsSupported_len + opcodes)));
			C_MEM (di->EventsSupported           = realloc(di->EventsSupported,          sizeof(di->EventsSupported[0])*(di->EventsSupported_len + events)));
			j = 0; k = 0; l = 0;
			for (i=0;i<xsize;i++) {
				GP_LOG_D ("sony code: %x", xprops[i]);
				switch (xprops[i] & 0x7000) {
				case 0x1000:
					di->OperationsSupported[(k++)+di->OperationsSupported_len] = xprops[i];
					break;
				case 0x4000:
					di->EventsSupported[(l++)+di->EventsSupported_len] = xprops[i];
					break;
				case 0x5000:
					di->DevicePropertiesSupported[(j++)+di->DevicePropertiesSupported_len] = xprops[i];
					break;
				default:
					break;
				}
			}
			di->DevicePropertiesSupported_len += propcodes;
			di->EventsSupported_len += events;
			di->OperationsSupported_len += opcodes;
			free (xprops);
			C_PTP (ptp_sony_sdioconnect (&camera->pl->params, 3, 0, 0));

			propval.i8 = 1; /* application */
			LOG_ON_PTP_E (ptp_sony_setdevicecontrolvaluea (params, PTP_DPC_SONY_PriorityMode, &propval, PTP_DTC_INT8));

			/* remember for sony zv-1 hack */
			params->starttime = time_now();
		}
		/* Sony QX */
		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_SONY_QX_Connect)) {
			int opcodes = 0, propcodes = 0, events = 0, j,k,l;
			uint16_t  	*xprops;
			unsigned int	xsize;

			C_PTP (ptp_sony_qx_connect (&camera->pl->params, 1, 0xda01, 0xda01));
			C_PTP (ptp_sony_qx_connect (&camera->pl->params, 2, 0xda01, 0xda01));

			C_PTP (ptp_sony_qx_get_vendorpropcodes (&camera->pl->params, &xprops, &xsize));

			for (i=0;i<xsize;i++) {
				switch (xprops[i] & 0x7000) {
				case 0x1000: opcodes++; break;
				case 0x4000: events++; break;
				case 0x5000: propcodes++; break;
				default:
					GP_LOG_E ("ptp_qx_sony_get_vendorpropcodes() unknown opcode %x", xprops[i]);
					break;
				}
			}
			C_MEM (di->DevicePropertiesSupported = realloc(di->DevicePropertiesSupported,sizeof(di->DevicePropertiesSupported[0])*(di->DevicePropertiesSupported_len + propcodes)));
			C_MEM (di->OperationsSupported       = realloc(di->OperationsSupported,      sizeof(di->OperationsSupported[0])*(di->OperationsSupported_len + opcodes)));
			C_MEM (di->EventsSupported           = realloc(di->EventsSupported,          sizeof(di->EventsSupported[0])*(di->EventsSupported_len + events)));
			j = 0; k = 0; l = 0;
			for (i=0;i<xsize;i++) {
				GP_LOG_D ("sony code: %x", xprops[i]);
				switch (xprops[i] & 0x7000) {
				case 0x1000:
					di->OperationsSupported[(k++)+di->OperationsSupported_len] = xprops[i];
					break;
				case 0x4000:
					di->EventsSupported[(l++)+di->EventsSupported_len] = xprops[i];
					break;
				case 0x5000:
					di->DevicePropertiesSupported[(j++)+di->DevicePropertiesSupported_len] = xprops[i];
					break;
				default:
					break;
				}
			}
			di->DevicePropertiesSupported_len += propcodes;
			di->EventsSupported_len += events;
			di->OperationsSupported_len += opcodes;
			free (xprops);
			C_PTP (ptp_sony_qx_connect (&camera->pl->params, 3, 0xda01, 0xda01));

			C_PTP (ptp_sony_qx_getalldevicepropdesc (&camera->pl->params));
		}
	}
#if 0 /* Marcus: not regular ptp properties, not queryable via getdevicepropertyvalue */
	if (di->VendorExtensionID == PTP_VENDOR_CANON) {
		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EOS_GetDeviceInfoEx)) {
			PTPCanonEOSDeviceInfo  	eosdi;
			int i;

			C_PTP (ptp_canon_eos_getdeviceinfo (&camera->pl->params, &eosdi));
			C_MEM (di->DevicePropertiesSupported = realloc(di->DevicePropertiesSupported,sizeof(di->DevicePropertiesSupported[0])*(di->DevicePropertiesSupported_len + eosdi.DevicePropertiesSupported_len)));
			for (i=0;i<eosdi.DevicePropertiesSupported_len;i++)
				di->DevicePropertiesSupported[i+di->DevicePropertiesSupported_len] = eosdi.DevicePropertiesSupported[i];
			di->DevicePropertiesSupported_len += eosdi.DevicePropertiesSupported_len;
		}
	}
#endif
	return GP_OK;
}

static uint16_t
nikon_wait_busy(PTPParams *params, int waitms, int timeout) {
	uint16_t	res;
	int		tries;

	/* wait either 1 second, or 50 tries */
	if (waitms)
		tries=timeout/waitms;
	else
		tries=50;

	do {
		res = ptp_nikon_device_ready(params);
                if (    (res != PTP_RC_DeviceBusy) &&
                        (res != PTP_RC_NIKON_Bulb_Release_Busy)
                ) {
			if (res == PTP_RC_NIKON_Silent_Release_Busy)	/* seems to mean something like "not relevant" ... will repeat forever */
				return PTP_RC_OK;
			return res;
		}
		if (waitms) usleep(waitms*1000)/*wait a bit*/;
	} while (tries--);
	return res;
}


static struct {
	const char *model;
	unsigned short usb_vendor;
	unsigned short usb_product;
	unsigned long device_flags;
} models[] = {
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
	/*
	 * The very first PTP camera (with special firmware only), also
	 * called "PTP Prototype", may report non PTP interface class
	 */
	{"Kodak:DC240 (PTP mode)",  0x040a, 0x0121, 0},
	/*
	 * Old DC-4800 firmware reported custom interface class, so we have
	 * to detect it by product/vendor IDs
	 */
	{"Kodak:DC4800", 0x040a, 0x0160, 0},
	/* Below other camers known to be detected by interface class */

	{"Kodak:DX3900", 0x040a, 0x0170, 0},
	{"Kodak:MC3",    0x040a, 0x0400, 0},
	/* reported by Ken Moffat */
	{"Kodak:Z7590",  0x040a, 0x0403, 0},
	{"Kodak:DX3500", 0x040a, 0x0500, 0},
	{"Kodak:DX3600", 0x040a, 0x0510, 0},
	{"Kodak:DX3215", 0x040a, 0x0525, 0},
	{"Kodak:DX3700", 0x040a, 0x0530, 0},
	{"Kodak:CX4230", 0x040a, 0x0535, 0},
	{"Kodak:LS420",  0x040a, 0x0540, 0},
	{"Kodak:DX4900", 0x040a, 0x0550, 0},
	{"Kodak:DX4330", 0x040a, 0x0555, 0},
	{"Kodak:CX4200", 0x040a, 0x0560, 0},
	{"Kodak:CX4210", 0x040a, 0x0560, 0},
	{"Kodak:LS743",  0x040a, 0x0565, 0},
	/* both above with different product IDs
	   normal/retail versions of the same model */
	{"Kodak:CX4300", 0x040a, 0x0566, 0},
	{"Kodak:CX4310", 0x040a, 0x0566, 0},
	{"Kodak:LS753",  0x040a, 0x0567, 0},
	{"Kodak:LS443",  0x040a, 0x0568, 0},
	{"Kodak:LS663",  0x040a, 0x0569, 0},
	{"Kodak:DX6340", 0x040a, 0x0570, 0},
	{"Kodak:CX6330", 0x040a, 0x0571, 0},
	{"Kodak:DX6440", 0x040a, 0x0572, 0},
	{"Kodak:CX6230", 0x040a, 0x0573, 0},
	{"Kodak:CX6200", 0x040a, 0x0574, 0},
	{"Kodak:DX6490", 0x040a, 0x0575, 0},
	{"Kodak:DX4530", 0x040a, 0x0576, 0},
	{"Kodak:DX7630", 0x040a, 0x0577, 0},
	{"Kodak:CX7300", 0x040a, 0x0578, 0},
	{"Kodak:CX7310", 0x040a, 0x0578, 0},
	{"Kodak:CX7220", 0x040a, 0x0579, 0},
	{"Kodak:CX7330", 0x040a, 0x057a, 0},
	{"Kodak:CX7430", 0x040a, 0x057b, 0},
	{"Kodak:CX7530", 0x040a, 0x057c, 0},
	{"Kodak:DX7440", 0x040a, 0x057d, 0},
	/* c300 Pau Rodriguez-Estivill <prodrigestivill@yahoo.es> */
	{"Kodak:C300",   0x040a, 0x057e, 0},
	{"Kodak:DX7590", 0x040a, 0x057f, 0},
	{"Kodak:Z730",   0x040a, 0x0580, 0},
	{"Kodak:CX6445", 0x040a, 0x0584, 0},
	/* Francesco Del Prete <italyanker@gmail.com> */
	{"Kodak:M893 IS",0x040a, 0x0585, 0},
	{"Kodak:CX7525", 0x040a, 0x0586, 0},
	/* a sf bugreporter */
	{"Kodak:Z700",   0x040a, 0x0587, 0},
	/* EasyShare Z740, Benjamin Mesing <bensmail@gmx.net> */
	{"Kodak:Z740",   0x040a, 0x0588, 0},
	/* EasyShare C360, Guilherme de S. Pastore via Debian */
 	{"Kodak:C360",   0x040a, 0x0589, 0},
	/* Giulio Salani <ilfunambolo@gmail.com> */
	{"Kodak:C310",   0x040a, 0x058a, 0},
	/* Brandon Sharitt */
	{"Kodak:C330",   0x040a, 0x058c, 0},
	/* c340 Maurizio Daniele <hayabusa@portalis.it> */
	{"Kodak:C340",   0x040a, 0x058d, 0},
	{"Kodak:V530",   0x040a, 0x058e, 0},
	/* v550 Jon Burgess <jburgess@uklinux.net> */
	{"Kodak:V550",   0x040a, 0x058f, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1618878&group_id=8874 */
	{"Kodak:V570",   0x040a, 0x0591, 0},
	{"Kodak:P850",   0x040a, 0x0592, 0},
	{"Kodak:P880",   0x040a, 0x0593, 0},
	/* Илья Розановский <rozanovskii.ilia@gmail.com> */
	{"Kodak:Z8612 IS",0x040a, 0x0595, 0},
	/* https://launchpad.net/distros/ubuntu/+source/libgphoto2/+bug/67532 */
	{"Kodak:C530",   0x040a, 0x059a, 0},
	/* Ivan Baldo, http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=387998 */
	{"Kodak:CD33",   0x040a, 0x059c, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=208874&aid=1565144&group_id=8874 */
	{"Kodak:Z612",   0x040a, 0x059d, 0},
	/* David D. Huff Jr. <David.Huff@computer-critters.com> */
	{"Kodak:Z650",   0x040a, 0x059e, 0},
	/* Sonja Krause-Harder */
	{"Kodak:M753",   0x040a, 0x059f, 0},
	/* irc reporter */
	{"Kodak:V603",   0x040a, 0x05a0, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1547142&group_id=8874&atid=358874 */
	{"Kodak:C533",   0x040a, 0x05a2, 0},
	/* Marc Santhoff <M.Santhoff@t-online.de> */
	{"Kodak:C643",   0x040a, 0x05a7, 0},
	/* Eric Kibbee <eric@kibbee.ca> */
	{"Kodak:C875",   0x040a, 0x05a9, 0},
	/* https://launchpad.net/bugs/64146 */
	{"Kodak:C433",   0x040a, 0x05aa, 0},
	/* https://launchpad.net/bugs/64146 */
	{"Kodak:V705",   0x040a, 0x05ab, 0},
	/* https://launchpad.net/bugs/67532 */
	{"Kodak:V610",   0x040a, 0x05ac, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1861193&group_id=8874&atid=358874 */
	{"Kodak:M883",   0x040a, 0x05ad, 0},
	/* from Thomas <tomtechguy@gmail.com> */
	{"Kodak:C743",   0x040a, 0x05ae, 0},
	/* via IRC */
	{"Kodak:C653",   0x040a, 0x05af, 0},
	/* "William L. Thomson Jr." <wlt@obsidian-studios.com> */
	{"Kodak:Z710",	 0x040a, 0x05b3, 0},
	/* Nicolas Brodu <nicolas.brodu@free.fr> */
	{"Kodak:Z712 IS",0x040a, 0x05b4, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1904224&group_id=8874&atid=358874 */
	{"Kodak:Z812 IS",0x040a, 0x05b5, 0},
	/* */
	{"Kodak:C613",   0x040a, 0x05b7, 0},
	/* msp@debian.org https://bugs.kde.org/show_bug.cgi?id=190795 */
	{"Kodak:V803",   0x040a, 0x05b8, 0},
	/* via IRC */
	{"Kodak:C633",   0x040a, 0x05ba, 0},
	/* https://bugs.launchpad.net/bugs/203402 */
	{"Kodak:ZD710",	 0x040a, 0x05c0, 0},
	/* https://bugs.launchpad.net/ubuntu/+source/libgphoto2/+bug/385432 */
	{"Kodak:M863",	 0x040a, 0x05c1, 0},
	/* Peter F Bradshaw <pfb@exadios.com> */
	{"Kodak:C813",	 0x040a, 0x05c3, 0},
	/* reported by Christian Le Corre <lecorrec@gmail.com> */
	{"Kodak:C913",   0x040a, 0x05c6, 0},
	/* IRC reporter */
	{"Kodak:Z950",   0x040a, 0x05cd, 0},
	/* reported by Jim Nelson <jim@yorba.org> */
	{"Kodak:M1063",  0x040a, 0x05ce, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=2889451&group_id=8874&atid=358874 */
	{"Kodak:Z915",   0x040a, 0x05cf, 0},
	/* l.w.winter@web.de */
	{"Kodak:M531",   0x040a, 0x0600, 0},
	/* rschweikert@novell.com */
	{"Kodak:C183",   0x040a, 0x060b, 0},
	/* Oleh Malyi <astroclubzp@gmail.com> */
	{"Kodak:Z990",   0x040a, 0x0613, 0},
	/* ra4veiV6@lavabit.com */
	{"Kodak:C1530",  0x040a, 0x0617, 0},
	/* via email to gphoto-devel */
	{"Kodak:M531 2nd id",	0x040a, 0x0665, 0},

	/* HP PTP cameras */
#if 0
	/* 0x4002 seems to be the mass storage ID, which various forums suggest. -Marcus */
	{"HP:PhotoSmart ... ", 		 0x03f0, 0x4002, 0},
#endif
	{"HP:PhotoSmart 812 (PTP mode)", 0x03f0, 0x4202, 0},
	{"HP:PhotoSmart 850 (PTP mode)", 0x03f0, 0x4302, 0},
	/* HP PhotoSmart 935: T. Kaproncai, 25 Jul 2003*/
	{"HP:PhotoSmart 935 (PTP mode)", 0x03f0, 0x4402, 0},
	/* HP:PhotoSmart 945: T. Jelbert, 2004/03/29	*/
	{"HP:PhotoSmart 945 (PTP mode)", 0x03f0, 0x4502, 0},
	{"HP:PhotoSmart C500 (PTP mode)", 0x03f0, 0x6002, 0},
	{"HP:PhotoSmart 318 (PTP mode)", 0x03f0, 0x6302, 0},
	{"HP:PhotoSmart 612 (PTP mode)", 0x03f0, 0x6302, 0},
	{"HP:PhotoSmart 715 (PTP mode)", 0x03f0, 0x6402, 0},
	{"HP:PhotoSmart 120 (PTP mode)", 0x03f0, 0x6502, 0},
	{"HP:PhotoSmart 320 (PTP mode)", 0x03f0, 0x6602, 0},
	{"HP:PhotoSmart 720 (PTP mode)", 0x03f0, 0x6702, 0},
	{"HP:PhotoSmart 620 (PTP mode)", 0x03f0, 0x6802, 0},
	{"HP:PhotoSmart 735 (PTP mode)", 0x03f0, 0x6a02, 0},
	{"HP:PhotoSmart 707 (PTP mode)", 0x03f0, 0x6b02, 0},
	{"HP:PhotoSmart 733 (PTP mode)", 0x03f0, 0x6c02, 0},
	{"HP:PhotoSmart 607 (PTP mode)", 0x03f0, 0x6d02, 0},
	{"HP:PhotoSmart 507 (PTP mode)", 0x03f0, 0x6e02, 0},
        {"HP:PhotoSmart 635 (PTP mode)", 0x03f0, 0x7102, 0},
	/* report from Federico Prat Villar <fprat@lsi.uji.es> */
	{"HP:PhotoSmart 43x (PTP mode)", 0x03f0, 0x7202, 0},
	{"HP:PhotoSmart M307 (PTP mode)", 0x03f0, 0x7302, 0},
	{"HP:PhotoSmart 407 (PTP mode)",  0x03f0, 0x7402, 0},
	{"HP:PhotoSmart M22 (PTP mode)",  0x03f0, 0x7502, 0},
	{"HP:PhotoSmart 717 (PTP mode)",  0x03f0, 0x7602, 0},
	{"HP:PhotoSmart 817 (PTP mode)",  0x03f0, 0x7702, 0},
	{"HP:PhotoSmart 417 (PTP mode)",  0x03f0, 0x7802, 0},
	{"HP:PhotoSmart 517 (PTP mode)",  0x03f0, 0x7902, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1365941&group_id=8874&atid=108874 */
	{"HP:PhotoSmart M415 (PTP mode)", 0x03f0, 0x7a02, 0},
	/* irc contact, YGingras */
	{"HP:PhotoSmart M23 (PTP mode)",  0x03f0, 0x7b02, 0},
	{"HP:PhotoSmart 217 (PTP mode)",  0x03f0, 0x7c02, 0},
	/* irc contact */
	{"HP:PhotoSmart 317 (PTP mode)",  0x03f0, 0x7d02, 0},
	{"HP:PhotoSmart 818 (PTP mode)",  0x03f0, 0x7e02, 0},
	/* Robin <diilbert.atlantis@gmail.com> */
	{"HP:PhotoSmart M425 (PTP mode)", 0x03f0, 0x8002, 0},
	{"HP:PhotoSmart M525 (PTP mode)", 0x03f0, 0x8102, 0},
	{"HP:PhotoSmart M527 (PTP mode)", 0x03f0, 0x8202, 0},
	{"HP:PhotoSmart M725 (PTP mode)", 0x03f0, 0x8402, 0},
	{"HP:PhotoSmart M727 (PTP mode)", 0x03f0, 0x8502, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1584447&group_id=8874&atid=358874 */
	{"HP:PhotoSmart R927 (PTP mode)", 0x03f0, 0x8702, 0},
	/* R967 - Far Jump <far.jmp@gmail.com> */
	{"HP:PhotoSmart R967 (PTP mode)", 0x03f0, 0x8802, 0},
	{"HP:PhotoSmart E327 (PTP mode)", 0x03f0, 0x8b02, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1589879&group_id=8874  */
	{"HP:PhotoSmart E427 (PTP mode)", 0x03f0, 0x8c02, 0},
	/* Martin Laberge <mlsoft@videotron.ca> */
	{"HP:PhotoSmart M737 (PTP mode)", 0x03f0, 0x9602, 0},
	/* https://bugs.launchpad.net/bugs/178916 */
	{"HP:PhotoSmart R742 (PTP mode)", 0x03f0, 0x9702, 0},
	/* private email */
	{"HP:PhotoSmart E331 (PTP mode)", 0x03f0, 0x9a02, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1814147&group_id=8874&atid=358874 */
	{"HP:PhotoSmart M547 (PTP mode)", 0x03f0, 0x9b02, 0},

	/* Most Sony PTP cameras use the same product/vendor IDs. */
	{"Sony:PTP",                  0x054c, 0x004e, 0},
	{"Sony:DSC-H1 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-H2 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-H5 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-N2 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-P5 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-P10 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-F707V (PTP mode)", 0x054c, 0x004e, 0},
	{"Sony:DSC-F717 (PTP mode)",  0x054c, 0x004e, 0},
	{"Sony:DSC-F828 (PTP mode)",  0x054c, 0x004e, 0},
	{"Sony:DSC-P30 (PTP mode)",   0x054c, 0x004e, 0},
	/* P32 reported on May 1st by Justin Alexander <justin (at) harshangel.com> */
	{"Sony:DSC-P31 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P32 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P41 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P43 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P50 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P51 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P52 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P71 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P72 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P73 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P92 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P93 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-P100 (PTP mode)",  0x054c, 0x004e, 0},
	{"Sony:DSC-P120 (PTP mode)",  0x054c, 0x004e, 0},
	{"Sony:DSC-P200 (PTP mode)",  0x054c, 0x004e, 0},
	{"Sony:DSC-R1 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-S40 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-S60 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-S75 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-S85 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-T1 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-T3 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-T10 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-U20 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-V1 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-W1 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-W12 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-W35 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-W55 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:MVC-CD300 (PTP mode)", 0x054c, 0x004e, 0},
	{"Sony:MVC-CD500 (PTP mode)", 0x054c, 0x004e, 0},
	{"Sony:DSC-U10 (PTP mode)",   0x054c, 0x004e, 0},
	/* "Riccardo (C10uD)" <c10ud.dev@gmail.com> */
	{"Sony:DSC-S730 (PTP mode)",  0x054c, 0x0296, 0},
	{"Sony:DSC-S780 (PTP mode)",  0x054c, 0x0296, 0},
	/* Fernando Santoro <fernando.lopezjr@gmail.com> */
	{"Sony:DSC-A100 (PTP mode)",  0x054c, 0x02c0, 0},
	/* Sam Tseng <samtz1223@gmail.com> */
	/* this seems not to have a separate control mode id, see https://github.com/gphoto/libgphoto2/issues/288 */
	{"Sony:DSC-A900 (PTP mode)",  0x054c, 0x02e7, PTP_CAP},
	/* new id?! Reported by Ruediger Oertel. */
	{"Sony:DSC-W200 (PTP mode)",  0x054c, 0x02f8, 0},
	/* Martin Vala <vala.martin@gmail.com> */
	{"Sony:SLT-A350 (PTP mode)",   0x054c, 0x0321, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1946931&group_id=8874&atid=308874 */
	{"Sony:DSC-W130 (PTP mode)",  0x054c, 0x0343, 0},
	/* https://sourceforge.net/p/gphoto/support-requests/115/ */
	{"Sony:DSC-HX5V (PTP mode)",  0x054c, 0x0491, 0},
	/* tux droid <gnutuxdroid@gmail.com> */
	{"Sony:SLT-A55 (PTP mode)",   0x054c, 0x04a3, 0},
	/* em33kay@gmail.com */
	{"Sony:NEX5 (PTP mode)",      0x054c, 0x04a5, 0},
	/* http://sourceforge.net/tracker/?func=detail&atid=358874&aid=3515558&group_id=8874 */
	{"Sony:SLT-A35 (PTP mode)",   0x054c, 0x04a7, 0},
	/* t.ludewig@gmail.com */
	{"Sony:DSC-RX100 (PTP mode)", 0x054c, 0x052a, 0},
	/* Stuart Nettleton <snettlet@gmail.com> */
	{"Sony:DSC-RX1 (PTP mode)",   0x054c, 0x052b, 0},
	/* Maptz <m13.maptz@gmail.com> */
	{"Sony:DSC-W510 (PTP mode)",  0x054c, 0x053c, 0},
	/* Rudi */
	{"Sony:DSC-HX100V (PTP mode)",0x054c, 0x0543, 0},
	/* t.ludewig@gmail.com */
	{"Sony:SLT-A65V (PTP mode)",  0x054c, 0x0574, 0},
	/* Irina Iakovleva <irina.bunger@gmail.com> */
	{"Sony:SLT-A77V (PTP mode)",  0x054c, 0x0577, 0},
	/* Jose Velez <jvelez00@gmail.com> */
	{"Sony:NEX-7 (PTP mode)",     0x054c, 0x057d, 0},

	/* email */
	{"Sony:HDR-PJ710V (PTP mode)",0x054c, 0x05f7, 0},
	/* https://sourceforge.net/p/libmtp/bugs/1459/ */
	{"Sony:HDR-PJ260VE (PTP mode)",0x054c, 0x0603, 0},

	/* Jean-Christophe Clavier <jcclavier@free.fr> */
	{"Sony:DSC-HX20V (PTP mode)", 0x054c, 0x061c, 0},
	/* t.ludewig@gmail.com */
	{"Sony:DSC-HX200V (PTP mode)",0x054c, 0x061f, 0},

	/* https://sourceforge.net/p/gphoto/feature-requests/424/ */
	{"Sony:SLT-A57", 	      0x054c, 0x0669, 0},

	/* Abhishek Anand <ajaxor1@gmail.com> */
	{"Sony:SLT-A37", 	      0x054c, 0x0669, 0},

	/* https://sourceforge.net/p/gphoto/feature-requests/480/ */
	{"Sony:NEX-5R", 	      0x054c, 0x066f, 0},

	/* Mike Breeuwer <info@mikebreeuwer.com> */
	{"Sony:SLT-A99v", 	      0x054c, 0x0675, 0},

	/* Ramon Carlos Garcia Bruna <ramongarciabruna@gmail.com> */
	{"Sony:NEX-6", 	      	      0x054c, 0x0678, 0},

	/* t.ludewig@gmail.com */
	{"Sony:DSC-HX300 (PTP mode)", 0x054c, 0x06ee, 0},

	/* t.ludewig@gmail.com */
	{"Sony:NEX-3N (PTP mode)",    0x054c, 0x072f, 0},

	/* Thorsten Ludewig <t.ludewig@gmail.com> */
	{"Sony:SLT-A58",	      0x054c, 0x0736, 0},
	/* Marcus Meissner */
	{"Sony:SLT-A58 (Control)",    0x054c, 0x0737, PTP_CAP},

	/* Thorsten Ludewig <t.ludewig@gmail.com> */
	{"Sony:DSC-RX100M2",	      0x054c, 0x074b, 0},
	/* Thorsten Ludewig <t.ludewig@gmail.com> */
	{"Sony:Alpha-A3000",	      0x054c, 0x074e, 0},

	/* bertrand.chambon@free.fr */
	{"Sony:Alpha-A68 (MTP)",      0x054c, 0x0779, 0},
	/* https://github.com/gphoto/libgphoto2/issues/70 */
	{"Sony:Alpha-A6300 (MTP)",    0x054c, 0x077a, 0},

	/* gphoto-devel  email report */
	{"Sony:DSC-HX80",	      0x054c, 0x0780, 0},

	/* https://github.com/gphoto/libgphoto2/issues/190 */
	{"Sony:Alpha-A6500",	      0x054c, 0x0784, 0},

	/* bertrand.chambon@free.fr */
	{"Sony:Alpha-A68 (Control)",  0x054c, 0x079b, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/70 */
	{"Sony:Alpha-A6300 (Control)",0x054c, 0x079c, PTP_CAP|PTP_CAP_PREVIEW},

	/* Anja Stock at SUSE */
	{"Sony:DSC-RX10M3 (Control)",  	0x054c, 0x079d, PTP_CAP|PTP_CAP_PREVIEW},

	/* Elijah Parker <mail@timelapseplus.com> */
	{"Sony:Alpha-A99 M2 (Control)", 0x054c, 0x079e, PTP_CAP|PTP_CAP_PREVIEW},

	/* Elijah Parker <mail@timelapseplus.com> */
	{"Sony:DSC-RX100V (Control)", 0x054c, 0x07a3, PTP_CAP},
	/* hanes442@icloud.com */
	{"Sony:DSC-RX100M5 (Control)",0x054c, 0x07a3, PTP_CAP},


	/* Elijah Parker <mail@timelapseplus.com> */
	/* https://github.com/gphoto/libgphoto2/issues/190 */
	{"Sony:Alpha-A6500 (Control)", 0x054c, 0x07a4, PTP_CAP|PTP_CAP_PREVIEW},

	/* Elijah Parker <mail@timelapseplus.com> */
	/* https://sourceforge.net/p/gphoto/support-requests/127/ */
	{"Sony:Alpha-A5000 (Control)", 0x054c, 0x07c6, PTP_CAP},

	/* jackden@gmail.com */
	{"Sony:DSC-RX100M6 (MTP)",  0x054c, 0x0830, 0},

	/* https://sourceforge.net/p/libmtp/support-requests/246/ */
	{"Sony:DSC-HX400V (MTP)",      0x054c, 0x08ac, 0},

	/* https://sourceforge.net/p/libmtp/bugs/1310/ */
	{"Sony:DSC-HX60V (MTP)",      0x054c, 0x08ad, 0},

	/* https://github.com/gphoto/libgphoto2/issues/355 */
	{"Sony:DSC-WX350 (MTP)",      0x054c, 0x08b0, 0},

	/* Sascha Peilicke at SUSE */
	{"Sony:Alpha-A6000 (MTP)",    0x054c, 0x08b7, 0},

	/* https://sourceforge.net/p/gphoto/feature-requests/456/ */
	{"Sony:Alpha-A7S (MTP)",      0x054c, 0x08e2, 0},

	/* David Farrier <farrier@iglou.com> */
	{"Sony:RX100M3 (MTP)",        0x054c, 0x08e3, 0},

	/* via email to gphoto-devel */
	{"Sony:DSC-WX220 (MTP)",      0x054c, 0x08d7, 0},

	/* Markus Oertel */
	{"Sony:Alpha-A5100 (MTP)",    0x054c, 0x08e7, 0},

	/* Andre Crone, andre@elysia.nl */
	{"Sony:Alpha-A7 (Control)",   0x054c, 0x094c, PTP_CAP},

	/* https://sourceforge.net/p/gphoto/feature-requests/442/ */
	/* no preview: https://github.com/gphoto/gphoto2/issues/106#issuecomment-381424159 */
	{"Sony:Alpha-A7r (Control)",  0x054c, 0x094d, PTP_CAP},

	/* preview was confirmed not to work. */
	{"Sony:Alpha-A6000 (Control)",  0x054c, 0x094e, PTP_CAP},

	/* Nick Clarke <nick.clarke@gmail.com> */
	{"Sony:Alpha-A77 M2 (Control)", 0x054c, 0x0953, PTP_CAP|PTP_CAP_PREVIEW},

	/* http://sourceforge.net/p/gphoto/feature-requests/456/ */
	{"Sony:Alpha-A7S (Control)",    0x054c, 0x0954, PTP_CAP|PTP_CAP_PREVIEW},

	/* Markus Oertel */
	/* Preview confirmed by Adrian Schroeter, preview might need the firmware getting updated */
	{"Sony:Alpha-A5100 (Control)",  0x054c, 0x0957, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/343  */
	{"Sony:Alpha-A7III (Control)",  0x054c, 0x096f, PTP_CAP|PTP_CAP_PREVIEW},

	/* Adrian Schroeter */
	{"Sony:ILCE-7R M2 (MTP)",        	0x054c, 0x09e7, 0},

	/* https://sourceforge.net/p/gphoto/feature-requests/472/ */
	{"Sony:DSC-HX90V (MTP)",        	0x054c, 0x09e8, 0},

	/* titan232@gmail.com */
	{"Sony:ILCE-7M2 (Control)",     	0x054c, 0x0a6a, PTP_CAP|PTP_CAP_PREVIEW},

	/* Andre Crone, andre@elysia.nl */
	{"Sony:Alpha-A7r II (Control)",		0x054c, 0x0a6b, PTP_CAP|PTP_CAP_PREVIEW},

	/* Andre Crone <andre@elysia.nl> */
	{"Sony:DSC-RX100M4",          		0x054c, 0x0a6d, PTP_CAP|PTP_CAP_PREVIEW},

	/* via email to gphoto-devel */
	{"Sony:Alpha-RX1R II (Control)",	0x054c,0x0a70, PTP_CAP|PTP_CAP_PREVIEW},

	/* Andre Crone <andre@elysia.nl>, adjusted */
	{"Sony:Alpha-A7S II (Control)",		0x054c,0x0a71, PTP_CAP|PTP_CAP_PREVIEW},

	/* brandonlampert@gmail.com */
	{"Sony:DSC-QX30U",			0x054c,0x0a77, PTP_CAP_PREVIEW},

	/* Demo7up <demo7up@gmail.com> */
	{"Sony:UMC-R10C",			0x054c,0x0a79, 0},

	{"Sony:DSC-RX0 (MTP)", 			0x054c, 0x0bfd, 0},

	/* https://github.com/gphoto/libgphoto2/issues/230 */
	{"Sony:Alpha-A7R III (MTP mode)",	0x054c,0x0c00, 0},

	/* https://github.com/gphoto/libgphoto2/issues/343 */
	{"Sony:Alpha-A7 III (MTP mode)",	0x054c,0x0c03, 0},

	/* andreas@harth.org */
	{"Sony:ZV-1", 				0x054c,0x0c1b, 0},

	/* Elijah Parker, mail@timelapseplus.com */
	{"Sony:Alpha-A9 (Control)",     	0x054c, 0x0c2a, PTP_CAP|PTP_CAP_PREVIEW},

	/* Richard Brown at SUSE */
	{"Sony:Alpha-RX10M4 (Control)", 	0x054c,0x0c2f, PTP_CAP|PTP_CAP_PREVIEW},

	{"Sony:DSC-RX0 (PC Control)",		0x054c, 0x0c32, PTP_CAP|PTP_CAP_PREVIEW},

	/* Elijah Parker, mail@timelapseplus.com */
	/* https://github.com/gphoto/libgphoto2/issues/230 */
	{"Sony:Alpha-A7r III (PC Control)",	0x054c, 0x0c33, PTP_CAP|PTP_CAP_PREVIEW}, /* FIXME: crosscheck */
	{"Sony:Alpha-A7 III (PC Control)",	0x054c, 0x0c34, PTP_CAP|PTP_CAP_PREVIEW}, /* FIXME: crosscheck */

	/* jackden@gmail.com */
	{"Sony:DSC-RX100M6 (PC Control)",  	0x054c, 0x0c38, PTP_CAP|PTP_CAP_PREVIEW},

	/* andreas@harth.org */
	{"Sony:ZV-1 (PC Control)",		0x054c, 0x0c44, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/419 */
	{"Sony:DSC RX0 II (PC Control)",	0x054c, 0x0ca6, PTP_CAP|PTP_CAP_PREVIEW},
	/* orbital sailor <gamerdude1080@hotmail.com> */
	{"Sony:ILCE-6400 (PC Control)",		0x054c, 0x0caa, PTP_CAP|PTP_CAP_PREVIEW},
	/* Elias Asikainen <elias.asikainen@sulzerschmid.ch> */
	{"Sony:DSC-RX100M7 (PC Control)",	0x054c, 0x0cae, PTP_CAP|PTP_CAP_PREVIEW},
	/* Mikael Ståldal <mikael@staldal.nu> */
	{"Sony:DSC-RX100M5A (MTP)",		0x054c, 0x0cb1, 0},
	{"Sony:DSC-RX100M5A (PC Control)",	0x054c, 0x0cb2, PTP_CAP|PTP_CAP_PREVIEW},

	/* Elijah Parker, mail@timelapseplus.com */
	{"Sony:DSC-A7r IV (Control)",		0x054c, 0x0ccc, PTP_CAP|PTP_CAP_PREVIEW},

	/* Ingvar Stepanyan <rreverser@google.com> */
	{"Sony:Alpha-A6600 (MTP)",	0x054c, 0x0d0f, 0},
	{"Sony:Alpha-A6600 (PC Control)",	0x054c, 0x0d10, PTP_CAP|PTP_CAP_PREVIEW},

	/* Elijah Parker, mail@timelapseplus.com */
	{"Sony:DSC-A7S III (Control)",		0x054c, 0x0d18, PTP_CAP|PTP_CAP_PREVIEW},

	/* via email */
	{"Sony:ILCE-7C (Control)",		0x054c, 0x0d2b, PTP_CAP|PTP_CAP_PREVIEW},

	/* via email */
	{"Sony:ZV-E10 (Control)",		0x054c, 0x0d97, PTP_CAP|PTP_CAP_PREVIEW},
	
	/* Thomas Schaad, napstertom@gmail.com */
	{"Sony:ILCE-7RM3A (PC Control)",	0x054c, 0x0d9b, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/749 */
	{"Sony:ILCE-7RM4A (PC Control)",	0x054c, 0x0d9f, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/pull/782 */
	{"Sony:Alpha-A7 IV (MTP mode)",		0x054c, 0x0da6, 0},
	{"Sony:Alpha-A7 IV (PC Control)",	0x054c, 0x0da7, PTP_CAP|PTP_CAP_PREVIEW},

	/* Nikon Coolpix 2500: M. Meissner, 05 Oct 2003 */
	{"Nikon:Coolpix 2500 (PTP mode)", 0x04b0, 0x0109, 0},
	/* Nikon Coolpix 5700: A. Tanenbaum, 29 Oct 2002 */
	/* no capture complete: https://sourceforge.net/tracker/index.php?func=detail&aid=3018517&group_id=8874&atid=108874 */
	{"Nikon:Coolpix 5700 (PTP mode)", 0x04b0, 0x010d, PTP_CAP|PTP_NO_CAPTURE_COMPLETE},
	/* Nikon Coolpix 4500: T. Kaproncai, 22 Aug 2003 */
	{"Nikon:Coolpix 4500 (PTP mode)", 0x04b0, 0x010b, 0},
	/* Nikon Coolpix 4300: Marco Rodriguez, 10 dic 2002 */
	{"Nikon:Coolpix 4300 (PTP mode)", 0x04b0, 0x010f, 0},
	/* Nikon Coolpix 3500: M. Meissner, 07 May 2003 */
	{"Nikon:Coolpix 3500 (PTP mode)", 0x04b0, 0x0111, 0},
	/* Nikon Coolpix 885: S. Anderson, 19 nov 2002 */
	{"Nikon:Coolpix 885 (PTP mode)",  0x04b0, 0x0112, 0},
	/* Nikon Coolpix 5000, Firmware v1.7 or later */
	{"Nikon:Coolpix 5000 (PTP mode)", 0x04b0, 0x0113, PTP_CAP|PTP_NO_CAPTURE_COMPLETE},
	/* Nikon Coolpix 3100 */
	{"Nikon:Coolpix 3100 (PTP mode)", 0x04b0, 0x0115, 0},
	/* Nikon Coolpix 2100 */
	{"Nikon:Coolpix 2100 (PTP mode)", 0x04b0, 0x0117, 0},
	/* Nikon Coolpix 5400: T. Kaproncai, 25 Jul 2003 */
	{"Nikon:Coolpix 5400 (PTP mode)", 0x04b0, 0x0119, PTP_CAP|PTP_NO_CAPTURE_COMPLETE},
	/* Nikon Coolpix 3700: T. Ehlers, 18 Jan 2004 */
	{"Nikon:Coolpix 3700 (PTP mode)", 0x04b0, 0x011d, PTP_CAP|PTP_NO_CAPTURE_COMPLETE},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2110825&group_id=8874&atid=108874 */
	{"Nikon:Coolpix 8700 (PTP mode)", 0x04b0, 0x011f, 0},
	/* Nikon Coolpix 3200 */
	{"Nikon:Coolpix 3200 (PTP mode)", 0x04b0, 0x0121, 0},
	/* Nikon Coolpix 2200 */
	{"Nikon:Coolpix 2200 (PTP mode)", 0x04b0, 0x0122, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	/* Jonathan Marten <jonathanmarten@users.sf.net>
	 * https://sourceforge.net/p/gphoto/bugs/968/ */
	{"Nikon:Coolpix 2200v1.1 (PTP mode)", 0x04b0, 0x0123, PTP_CAP|PTP_NO_CAPTURE_COMPLETE},

	/* Harry Reisenleiter <harrylr@earthlink.net> */
	{"Nikon:Coolpix 8800 (PTP mode)", 0x04b0, 0x0127, PTP_CAP},
	/* Nikon Coolpix 4800 */
	{"Nikon:Coolpix 4800 (PTP mode)", 0x04b0, 0x0129, 0},
	/* Nikon Coolpix SQ: M. Holzbauer, 07 Jul 2003 */
	/* and https://github.com/gphoto/libgphoto2/issues/29 */
	{"Nikon:Coolpix 4100 (PTP mode)", 0x04b0, 0x012d, PTP_CAP|PTP_NO_CAPTURE_COMPLETE},
	/* Nikon Coolpix 5600: Andy Shevchenko, 11 Aug 2005 */
	{"Nikon:Coolpix 5600 (PTP mode)", 0x04b0, 0x012e, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* 4600: Martin Klaffenboeck <martin.klaffenboeck@gmx.at> */
	{"Nikon:Coolpix 4600 (PTP mode)", 0x04b0, 0x0130, 0},
	/* 4600: Roberto Costa <roberto.costa@ensta.org>, 22 Oct 2006 */
	{"Nikon:Coolpix 4600a (PTP mode)", 0x04b0, 0x0131,PTP_CAP|PTP_NO_CAPTURE_COMPLETE},
	{"Nikon:Coolpix 5900 (PTP mode)", 0x04b0, 0x0135, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1846012&group_id=8874&atid=358874 */
	{"Nikon:Coolpix 7900 (PTP mode)", 0x04b0, 0x0137, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* Egil Kvaleberg: USB says "NIKON DSC E7600-PTP" */
	{"Nikon:Coolpix 7600 (PTP mode)", 0x04b0, 0x0139, PTP_CAP},

	{"Nikon:Coolpix P1 (PTP mode)",   0x04b0, 0x0140, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* Marcus Meissner */
	{"Nikon:Coolpix P2 (PTP mode)",   0x04b0, 0x0142, PTP_CAP|PTP_NO_CAPTURE_COMPLETE},
	/* Richard SCHNEIDER <Richard.SCHNEIDER@tilak.at> */
	{"Nikon:Coolpix S4 (PTP mode)",   0x04b0, 0x0144, 0},
	/* Lowe, John Michael <jomlowe@iupui.edu> */
	{"Nikon:Coolpix S2 (PTP mode)",   0x04b0, 0x014e, 0},
	{"Nikon:Coolpix S6 (PTP mode)",   0x04b0, 0x014e, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=3114250&group_id=8874&atid=358874 */
	{"Nikon:Coolpix S7c (PTP mode)",  0x04b0, 0x0157, 0},
	/* Ole Aamot <ole@gnome.org> */
	{"Nikon:Coolpix P5000 (PTP mode)",0x04b0, 0x015b, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* Peter Pregler <Peter_Pregler@email.com> */
	{"Nikon:Coolpix S500 (PTP mode)", 0x04b0, 0x015d, 0},
	{"Nikon:Coolpix L12 (PTP mode)",  0x04b0, 0x015f, PTP_CAP},
	/* Marius Groeger <marius.groeger@web.de> */
	{"Nikon:Coolpix S200 (PTP mode)", 0x04b0, 0x0161, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* Submitted on IRC by kallepersson */
	{"Nikon:Coolpix P5100 (PTP mode)", 0x04b0, 0x0163, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2589245&group_id=8874&atid=108874 */
	{"Nikon:Coolpix P50 (PTP mode)",  0x04b0, 0x0169, 0},
	/* Clodoaldo <clodoaldo.pinto.neto@gmail.com> via
         * https://bugs.kde.org/show_bug.cgi?id=315268 */
	{"Nikon:Coolpix P80 (PTP mode)",  0x04b0, 0x016b, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	/* TJ <wxtofly@gmail.com> */
	{"Nikon:Coolpix P80 v1.1 (PTP mode)",  0x04b0, 0x016c, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	/* http://sourceforge.net/tracker/index.php?func=detail&aid=2951663&group_id=8874&atid=358874 */
	{"Nikon:Coolpix P6000 (PTP mode)",0x04b0, 0x016f, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/*   http://bugs.debian.org/520752 */
	{"Nikon:Coolpix S60 (PTP mode)",  0x04b0, 0x0171, 0},
	/* Mike Strickland <livinwell@georgianatives.net> */
	{"Nikon:Coolpix P90 (PTP mode)",  0x04b0, 0x0173, 0},
	/* https://github.com/gphoto/gphoto2/issues/214 */
	{"Nikon:Coolpix L100 (PTP mode)", 0x04b0, 0x0174, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* Christoph Muehlmann <c.muehlmann@nagnag.de> */
	{"Nikon:Coolpix S220 (PTP mode)", 0x04b0, 0x0177, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* */
	{"Nikon:Coolpix S225 (PTP mode)", 0x04b0, 0x0178, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	/* Ryan Nestor <ryan@monadnock.org> */
	{"Nikon:Coolpix P100 (PTP mode)", 0x04b0, 0x017d, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* Štěpán Němec <stepnem@gmail.com> */
	{"Nikon:Coolpix P7000 (PTP mode)",0x04b0, 0x017f, PTP_CAP|PTP_NO_CAPTURE_COMPLETE},

	/* http://sourceforge.net/tracker/index.php?func=detail&aid=3019787&group_id=8874&atid=358874 */
	/* probably no need for nikon_broken_Cap as it worked without this flag for the user */
	{"Nikon:Coolpix L110 (PTP mode)", 0x04b0, 0x017e, PTP_CAP},

	/* miguel@rozsas.eng.br */
	{"Nikon:Coolpix P500 (PTP mode)", 0x04b0, 0x0184, PTP_CAP},
	/* Graeme Wyatt <graeme.wyatt@nookawarra.com> */
	{"Nikon:Coolpix L120 (PTP mode)", 0x04b0, 0x0185, PTP_CAP},
	/* Kévin Ottens <ervin@ipsquad.net> */
	{"Nikon:Coolpix S9100 (PTP mode)",0x04b0, 0x0186, PTP_CAP},

	/* johnnolan@comcast.net */
	{"Nikon:Coolpix AW100 (PTP mode)",0x04b0, 0x0188, PTP_CAP},

	/* Dale Pontius <DEPontius@edgehp.net> */
	{"Nikon:Coolpix P7100 (PTP mode)",0x04b0, 0x018b, PTP_CAP},

	/* "Dr. Ing. Dieter Jurzitza" <dieter.jurzitza@t-online.de> */
	{"Nikon:Coolpix 9400  (PTP mode)",0x04b0, 0x0191, PTP_CAP},

	/* t.ludewig@gmail.com */
	{"Nikon:Coolpix L820  (PTP mode)",0x04b0, 0x0192, PTP_CAP},
	/* https://sourceforge.net/p/gphoto/feature-requests/429/ */
	{"Nikon:Coolpix S9500 (PTP mode)",0x04b0, 0x0193, PTP_CAP},

	/* LeChuck <ofernandez84@gmail.com> */
	{"Nikon:Coolpix AW110 (PTP mode)",0x04b0, 0x0194, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	/* https://github.com/gphoto/libgphoto2/issues/116 */
	{"Nikon:Coolpix AW130 (PTP mode)",0x04b0, 0x0198, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	/* https://github.com/gphoto/libgphoto2/issues/150 */
	{"Nikon:Coolpix P900 (PTP mode)", 0x04b0, 0x019c, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	/* https://github.com/gphoto/libgphoto2/issues/141 */
	{"Nikon:Coolpix A900 (PTP mode)", 0x04b0, 0x019e, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	/* https://github.com/gphoto/libgphoto2/issues/138 */
	{"Nikon:KeyMission 360 (PTP mode)",0x04b0, 0x019f, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	{"Nikon:Coolpix SQ (PTP mode)",   0x04b0, 0x0202, 0},
	/* lars marowski bree, 16.8.2004 */
	{"Nikon:Coolpix 4200 (PTP mode)", 0x04b0, 0x0204, 0},
	/* Nikon Coolpix 5200: Andy Shevchenko, 18 Jul 2005 */
	{"Nikon:Coolpix 5200 (PTP mode)", 0x04b0, 0x0206, 0},
	/* https://launchpad.net/bugs/63473 */
	{"Nikon:Coolpix L1 (PTP mode)",   0x04b0, 0x0208, 0},
	{"Nikon:Coolpix P4 (PTP mode)",   0x04b0, 0x020c, PTP_CAP},
	/* Bo Masser <bo@massers.se> */
	{"Nikon:Coolpix S620 (PTP mode)", 0x04b0, 0x021c, 0},
	{"Nikon:Coolpix S6000 (PTP mode)",0x04b0, 0x021e, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=3135935&group_id=8874&atid=358874 */
	{"Nikon:Coolpix S8000 (PTP mode)",0x04b0, 0x021f, 0},
	/* Aleksej Serdjukov <deletesoftware@yandex.ru> */
	{"Nikon:Coolpix S5100 (PTP mode)",0x04b0, 0x0220, 0},
	/* wlady.cs@gmail.com */
	{"Nikon:Coolpix P300 (PTP mode)", 0x04b0, 0x0221, 0},
	/* t.ludewig@gmail.com */
	{"Nikon:Coolpix S8200",           0x04b0, 0x0222, 0},
	{"Nikon:Coolpix P510 (PTP mode)", 0x04b0, 0x0223, 0},
	/* Bernhard Schiffner <bernhard@schiffner-limbach.de> */
	{"Nikon:Coolpix P7700 (PTP mode)",0x04b0, 0x0225, 0},

	/* t.ludewig@gmail.com */
	/* N CP A seems capture capable, but does not list vendor commands */
	/* Reports 0x400d aka CaptureComplete event ... but has no
	 * vendor commands? yeah right ... */
	/* It might be similar to the 1? lets try ... Marcus 20140706 */
	{"Nikon:Coolpix A (PTP mode)",	  0x04b0, 0x0226, PTP_CAP|PTP_NIKON_1}, /* PTP_CAP */

	/* Jonas Stein <news@jonasstein.de> */
	{"Nikon:Coolpix P330 (PTP mode)", 0x04b0, 0x0227, PTP_CAP},

	/* Malcolm Lee <mallee@mallee45.ukfsn.org> */
	{"Nikon:Coolpix P7800 (PTP mode)", 0x04b0, 0x0229, 0},

	/* t.ludewig@gmail.com */
	/* Also reports 0x400d aka CaptureComplete event ... but has no
	 * vendor commands? yeah right... */
	{"Nikon:Coolpix P520 (PTP mode)", 0x04b0, 0x0228, 0}, /* PTP_CAP */

	/* checamon <checamon@gmail.com> */
	{"Nikon:Coolpix B700 (PTP mode)", 0x04b0, 0x0231, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/477 */
	{"Nikon:Coolpix P1000 (PTP mode)",0x04b0, 0x0232, PTP_CAP/*|PTP_CAP_PREVIEW*/},

	/* Nikon Coolpix 2000 */
	{"Nikon:Coolpix 2000 (PTP mode)", 0x04b0, 0x0302, 0},
	/* From IRC reporter. */
	{"Nikon:Coolpix L4 (PTP mode)",   0x04b0, 0x0305, 0},
	/* from Magnus Larsson */
	{"Nikon:Coolpix L11 (PTP mode)",  0x04b0, 0x0309, 0},
	/* From IRC reporter. */
	{"Nikon:Coolpix L10 (PTP mode)",  0x04b0, 0x030b, 0},
	/* Philippe ENTZMANN <philippe@phec.net> */
	{"Nikon:Coolpix P60 (PTP mode)",  0x04b0, 0x0311, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* Stas Timokhin <st@ngs.ru> */
	{"Nikon:Coolpix L16 (PTP mode)",  0x04b0, 0x0315, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2977303&group_id=8874&atid=358874 */
	{"Nikon:Coolpix L20 (PTP mode)",  0x04b0, 0x0317, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2947644&group_id=8874&atid=108874 */
	{"Nikon:Coolpix L19 (PTP mode)",  0x04b0, 0x0318, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* IRC reporter */
	{"Nikon:Coolpix S3000 (PTP mode)",0x04b0, 0x031b, PTP_CAP},
	/* christian.moll@tudor.lu */
	{"Nikon:Coolpix S3100 (PTP mode)",0x04b0, 0x0320, PTP_CAP},
	/* Teemu rytilahti of KDE */
	{"Nikon:Coolpix S2500 (PTP mode)",0x04b0, 0x0321, PTP_CAP},
	/* Fabio <ctrlaltca@gmail.com> */
	{"Nikon:Coolpix L23 (PTP mode)",  0x04b0, 0x0324, PTP_CAP},
	/* Novell bugzilla 852551 */
	{"Nikon:Coolpix S4300 (PTP mode)",0x04b0, 0x0329, PTP_CAP/*?*/},
	/* "M.-A. DARCHE" <ma.darche@cynode.org> . gets capturecomplete events nicely */
	{"Nikon:Coolpix S3300 (PTP mode)",0x04b0, 0x032a, PTP_CAP},
	/* Mdasoh Kyaeppd at IRC */
	{"Nikon:Coolpix S6300 (PTP mode)",0x04b0, 0x032c, PTP_CAP},
	/* sakax <sakamotox@gmail.com> */
	{"Nikon:Coolpix S2600 (PTP mode)",0x04b0, 0x032d, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	/* dougvj@gmail.com */
	{"Nikon:Coolpix L810  (PTP mode)",0x04b0, 0x032f, PTP_CAP},

	/* Borja Latorre <borja.latorre@csic.es> */
	{"Nikon:Coolpix S3200",		  0x04b0, 0x0334, PTP_CAP},


	/* t.ludewig@gmail.com */
	{"Nikon:Coolpix S01",  		  0x04b0, 0x0337, PTP_CAP},

	/* https://sourceforge.net/p/gphoto/bugs/971/ */
	{"Nikon:Coolpix S2700", 	  0x04b0, 0x033f, PTP_CAP},

	/* Jeremy Harkcom <jeremy@harkcom.co.uk> */
	{"Nikon:Coolpix L27",		  0x04b0, 0x0343, PTP_CAP},

	/* t.ludewig@gmail.com */
	{"Nikon:Coolpix S02",  		  0x04b0, 0x0346, PTP_CAP},

	/* t.ludewig@gmail.com */
	/* seems not to report events? but has full liveview caps_ */
	{"Nikon:Coolpix S9700", 	  0x04b0, 0x034b, PTP_CAP|PTP_CAP_PREVIEW|PTP_NIKON_BROKEN_CAP},

	/* David Shawcross <david@sitevisuals.com> */
	{"Nikon:Coolpix S6800", 	  0x04b0, 0x0350, PTP_CAP},

	/* Tomasz Lis <mefistotelis@gmail.com> */
	{"Nikon:Coolpix S3600", 	  0x04b0, 0x0353, PTP_CAP},

	/* Laurence Praties <laurence.praties@i-abra.com> */
	{"Nikon:Coolpix L840",		  0x04b0, 0x035a, PTP_CAP|PTP_NO_CAPTURE_COMPLETE},

	/* David Shawcross <david@sitevisuals.com> */
	{"Nikon:Coolpix S3700", 	  0x04b0, 0x035c, PTP_CAP},

	/* David Shawcross <david@sitevisuals.com> */
	{"Nikon:Coolpix S2900", 	  0x04b0, 0x035e, PTP_CAP},

	/* https://sourceforge.net/p/libmtp/bugs/1743/ */
	{"Nikon:Coolpix L340", 	  	  0x04b0, 0x0361, PTP_CAP},

	/* via email to gphoto-devel */
	{"Nikon:Coolpix B500", 	  	  0x04b0, 0x0362, PTP_CAP},

	/* Krystal Puga <krystalvp@gmail.com> */
	{"Nikon:KeyMission 170", 	  0x04b0, 0x0364, PTP_CAP},
	/* https://github.com/gphoto/libgphoto2/issues/780 */
	{"Nikon:P950", 	  		  0x04b0, 0x036d, PTP_CAP|PTP_CAP_PREVIEW|PTP_NIKON_BROKEN_CAP},

	/* Nikon D100 has a PTP mode: westin 2002.10.16 */
	{"Nikon:DSC D100 (PTP mode)",     0x04b0, 0x0402, 0},
	/* D2H SLR in PTP mode from Steve Drew <stevedrew@gmail.com> */
	{"Nikon:D2H SLR (PTP mode)",      0x04b0, 0x0404, 0},
	{"Nikon:DSC D70 (PTP mode)",      0x04b0, 0x0406, PTP_CAP},
	/* Justin Case <justin_case@gmx.net> */
	{"Nikon:D2X SLR (PTP mode)",      0x04b0, 0x0408, PTP_CAP},
	/* Niclas Gustafsson (nulleman @ sf) */
	{"Nikon:D50 (PTP mode)",          0x04b0, 0x040a, PTP_CAP}, /* no hidden props */
	/* Didier Gasser-Morlay <didiergm@gmail.com> */
	{"Nikon:D2Hs (PTP mode)",	  0x04b0, 0x040c, PTP_CAP},
	{"Nikon:DSC D70s (PTP mode)",     0x04b0, 0x040e, PTP_CAP},
	/* Jana Jaeger <jjaeger.suse.de> */
	{"Nikon:DSC D200 (PTP mode)",     0x04b0, 0x0410, PTP_CAP},
	/* Christian Deckelmann @ SUSE */
	{"Nikon:DSC D80 (PTP mode)",      0x04b0, 0x0412, PTP_CAP},
	/* Huy Hoang <hoang027@umn.edu> */
	{"Nikon:DSC D40 (PTP mode)",      0x04b0, 0x0414, PTP_CAP},
	/* Mark de Ruijter <mark@ridersoft.net> */
	{"Nikon:DSC D2Xs (PTP mode)",     0x04b0, 0x0416, PTP_CAP},
	/* Luca Gervasi <luca.gervasi@gmail.com> */
	{"Nikon:DSC D40x (PTP mode)",     0x04b0, 0x0418, PTP_CAP},
	/* Andreas Jaeger <aj@suse.de>.
	 * Marcus: MTP Proplist does not return objectsizes ... useless. */
	{"Nikon:DSC D300 (PTP mode)",	  0x04b0, 0x041a, PTP_CAP},
	/* Pat Shanahan, http://sourceforge.net/tracker/index.php?func=detail&aid=1924511&group_id=8874&atid=358874 */
	{"Nikon:D3 (PTP mode)",		  0x04b0, 0x041c, PTP_CAP},
	/* irc reporter Benjamin Schindler */
	{"Nikon:DSC D60 (PTP mode)",	  0x04b0, 0x041e, PTP_CAP},

	/* pbj304 pbj@ecs.soton.ac.uk */
	{"Nikon:DSC D3x (PTP mode)",	  0x04b0, 0x0420, PTP_CAP|PTP_CAP_PREVIEW},

	/* Will Stephenson at SUSE and wstephenson@flickr */
	{"Nikon:DSC D90 (PTP mode)",	  0x04b0, 0x0421, PTP_CAP|PTP_CAP_PREVIEW},
	/* Borrowed D700 by deckel / marcus at SUSE */
	{"Nikon:DSC D700 (PTP mode)",	  0x04b0, 0x0422, PTP_CAP|PTP_CAP_PREVIEW},
	/* Stephan Barth at SUSE */
	{"Nikon:DSC D5000 (PTP mode)",    0x04b0, 0x0423, PTP_CAP|PTP_CAP_PREVIEW},
	/* IRC reporter */
	{"Nikon:DSC D3000 (PTP mode)",    0x04b0, 0x0424, PTP_CAP},
	/* Andreas Dielacher <andreas.dielacher@gmail.com> */
	{"Nikon:DSC D300s (PTP mode)",    0x04b0, 0x0425, PTP_CAP|PTP_CAP_PREVIEW},
	/* Matthias Blaicher <blaicher@googlemail.com> */
	{"Nikon:DSC D3s (PTP mode)",      0x04b0, 0x0426, PTP_CAP|PTP_CAP_PREVIEW},
	/* SWPLinux IRC reporter... does not have liveview -lowend model. */
	{"Nikon:DSC D3100 (PTP mode)",	  0x04b0, 0x0427, PTP_CAP},
	/* http://sourceforge.net/tracker/?func=detail&atid=358874&aid=3140014&group_id=8874 */
	{"Nikon:DSC D7000 (PTP mode)",    0x04b0, 0x0428, PTP_CAP|PTP_CAP_PREVIEW},

	/* IRC Reporter popolon */
	{"Nikon:DSC D5100 (PTP mode)",    0x04b0, 0x0429, PTP_CAP|PTP_CAP_PREVIEW},

	/* "François G." <francois@webcampak.com> */
	{"Nikon:DSC D800",	          0x04b0, 0x042a, PTP_CAP|PTP_CAP_PREVIEW},

	/* Christian Deckelmann of Xerox */
	{"Nikon:DSC D4",	          0x04b0, 0x042b, PTP_CAP|PTP_CAP_PREVIEW},

	/* Lihuijun <lihuiplus@hotmail.com> */
	{"Nikon:DSC D3200",	          0x04b0, 0x042c, PTP_CAP|PTP_CAP_PREVIEW},

	/* t.ludewig@gmail.com */
	{"Nikon:DSC D600",	          0x04b0, 0x042d, PTP_CAP|PTP_CAP_PREVIEW},
	/* Roderick Stewart <roderick.stewart@gmail.com> */
	{"Nikon:DSC D800E",               0x04b0, 0x042e, PTP_CAP|PTP_CAP_PREVIEW},
	/* Simeon Pilgrim <simeon.pilgrim@gmail.com> */
	{"Nikon:DSC D5200",               0x04b0, 0x042f, PTP_CAP|PTP_CAP_PREVIEW},

	/* t.ludewig@gmail.com */
	{"Nikon:DSC D7100",               0x04b0, 0x0430, PTP_CAP|PTP_CAP_PREVIEW},

	/* Kirill Bogdanenk <kirill.bogdanenko@gmail.com> via kde bug 336523 */
	{"Nikon:DSC D5300",               0x04b0, 0x0431, PTP_CAP|PTP_CAP_PREVIEW},

	/* Andre Crone <andre@elysia.nl> */
	{"Nikon:DSC Df",                  0x04b0, 0x0432, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://sourceforge.net/p/gphoto/feature-requests/449/ */
	{"Nikon:DSC D3300",               0x04b0, 0x0433, PTP_CAP|PTP_CAP_PREVIEW},

	/* Andre Crone <andre@elysia.nl> */
	{"Nikon:DSC D610",                0x04b0, 0x0434, PTP_CAP|PTP_CAP_PREVIEW},

	/* Andre Crone <andre@elysia.nl> */
	{"Nikon:DSC D4s",		  0x04b0, 0x0435, PTP_CAP|PTP_CAP_PREVIEW},

	/* Jeremie FROLI <jfroli@webmail.alten.fr> */
	{"Nikon:DSC D810",                0x04b0, 0x0436, PTP_CAP|PTP_CAP_PREVIEW},

	/*Jürgen Blumenschein <blumenschein@huntington-info.eu> */
	{"Nikon:DSC D750",                0x04b0, 0x0437, PTP_CAP|PTP_CAP_PREVIEW},

	/* Jeffrey Wilson <colgs3b@gmail.com> */
	{"Nikon:DSC D5500",		  0x04b0, 0x0438, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/83 */
	{"Nikon:DSC D7200",		  0x04b0, 0x0439, PTP_CAP|PTP_CAP_PREVIEW},

	/* Laurent Zylberman <l.zylberman@graphix-images.com> */
	{"Nikon:DSC D5",		  0x04b0, 0x043a, PTP_CAP|PTP_CAP_PREVIEW},

	/* Andre Crone <andre@elysia.nl> */
	{"Nikon:DSC D810A",               0x04b0, 0x043b, PTP_CAP|PTP_CAP_PREVIEW},

	/* bob@360degreeviews.com */
	{"Nikon:DSC D500",                0x04b0, 0x043c, PTP_CAP|PTP_CAP_PREVIEW},

	/* Chris P <cpeace@gmail.com> */
	{"Nikon:DSC D3400",               0x04b0, 0x043d, PTP_CAP|PTP_CAP_PREVIEW},

	/* Phil Stephenson <filstephenson@gmail.com> */
	{"Nikon:DSC D5600",               0x04b0, 0x043f, PTP_CAP|PTP_CAP_PREVIEW},

	/* Satoshi Kubota */
	{"Nikon:DSC D7500",               0x04b0, 0x0440, PTP_CAP|PTP_CAP_PREVIEW},

	/* Andre Crone <visuals@elysia.nl> */
	{"Nikon:DSC D850",                0x04b0, 0x0441, PTP_CAP|PTP_CAP_PREVIEW},

	/* Horshack ?? <horshack@live.com> */
	{"Nikon:Z7",                	  0x04b0, 0x0442, PTP_CAP|PTP_CAP_PREVIEW},

	/* Marcus Meissner */
	{"Nikon:Z6",                	  0x04b0, 0x0443, PTP_CAP|PTP_CAP_PREVIEW},

	/* Daniel Baertschi <daniel@avisec.ch> */
	{"Nikon:Z50",                	  0x04b0, 0x0444, PTP_CAP|PTP_CAP_PREVIEW},

	/* Schreiber, Steve via Gphoto-devel */
	{"Nikon:DSC D3500",		  0x04b0, 0x0445, PTP_CAP|PTP_CAP_PREVIEW},

	/* timelapse-VIEW */
	{"Nikon:DSC D780",		  0x04b0, 0x0446, PTP_CAP|PTP_CAP_PREVIEW},

	/* by email */
	{"Nikon:DSC D6",		  0x04b0, 0x0447, PTP_CAP|PTP_CAP_PREVIEW},

	/* Thomas Schaad */
	{"Nikon:Z5",                      0x04b0, 0x0448, PTP_CAP|PTP_CAP_PREVIEW},

	/* Fahrion <fahrion.2600@gmail.com> */
	{"Nikon:Z7_2",                	  0x04b0, 0x044b, PTP_CAP|PTP_CAP_PREVIEW},

	/* Thomas Schaad <tom@avisec.ch> */
	{"Nikon:Z6_2",                	  0x04b0, 0x044c, PTP_CAP|PTP_CAP_PREVIEW},
	{"Nikon:Zfc",                     0x04b0, 0x044f, PTP_CAP|PTP_CAP_PREVIEW},
	/* Stefan Weiberg at SUSE */
	{"Nikon:Z9",			  0x04b0, 0x0450, PTP_CAP|PTP_CAP_PREVIEW},

	/* Z://github.com/gphoto/libgphoto2/pull/750#issuecomment-1189987634 */
	{"Nikon:Z30",			  0x04b0, 0x0452, PTP_CAP|PTP_CAP_PREVIEW},

	/* http://sourceforge.net/tracker/?func=detail&aid=3536904&group_id=8874&atid=108874 */
	/* https://github.com/gphoto/libgphoto2/issues/569 */
	{"Nikon:V1",    		  0x04b0, 0x0601, PTP_CAP|PTP_NIKON_1},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=3556403&group_id=8874 */
	{"Nikon:J1",    		  0x04b0, 0x0602, PTP_CAP|PTP_NIKON_1},
	/* https://bugzilla.novell.com/show_bug.cgi?id=814622 Martin Caj at SUSE */
	{"Nikon:J2",    		  0x04b0, 0x0603, PTP_CAP|PTP_NIKON_1},
	/* https://sourceforge.net/p/gphoto/feature-requests/432/ */
	{"Nikon:V2",    		  0x04b0, 0x0604, PTP_CAP|PTP_NIKON_1},
	/* Ralph Schindler <ralph@ralphschindler.com> */
	{"Nikon:J3",    		  0x04b0, 0x0605, PTP_CAP|PTP_NIKON_1},
	/* Markus Karlhuber <markus.karlhuber@gmail.com> */
	{"Nikon:S1",    		  0x04b0, 0x0606, PTP_CAP|PTP_NIKON_1},
	/* Tarjei Husøy <git@thusoy.com> */
	{"Nikon:S2",              0x04b0, 0x0608, PTP_CAP|PTP_NIKON_1},
	/* Raj Kumar <raj@archive.org> */
	{"Nikon:J4",    		  0x04b0, 0x0609, PTP_CAP|PTP_NIKON_1},
	/* Alexander Smith <a.smith@precisionhawk.com> */
	{"Nikon:V3",    		  0x04b0, 0x060a, PTP_CAP|PTP_NIKON_1},

	/* Wolfgang Goetz <Wolfgang.ztoeG@web.de> */
	{"Nikon:J5",    		  0x04b0, 0x060b, PTP_CAP|PTP_NIKON_1},

#if 0
	/* Thomas Luzat <thomas.luzat@gmx.net> */
	/* this was reported as not working, mass storage only:
	 * http://sourceforge.net/tracker/index.php?func=detail&aid=1847471&group_id=8874&atid=108874
	{"Panasonic:DMC-FZ20 (alternate id)", 0x04da, 0x2372, 0},
	*/
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1350226&group_id=8874&atid=208874 */
	{"Panasonic:DMC-LZ2",             0x04da, 0x2372, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=1405541&group_id=8874&atid=358874 */
	{"Panasonic:DMC-LC1",             0x04da, 0x2372, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1275100&group_id=8874&atid=358874 */
	{"Panasonic:Lumix FZ5",           0x04da, 0x2372, 0},
#endif

	/* so the GX8 reports the same USB ids as others, but has capture support. See debuglog. */
	{"Panasonic:DMC-GX8",             0x04da, 0x2374, PTP_CAP|PTP_CAP_PREVIEW},
	{"Panasonic:DMC-FZ20",            0x04da, 0x2374, 0},
	{"Panasonic:DMC-FZ45",            0x04da, 0x2374, 0},
	{"Panasonic:DMC-FZ38",            0x04da, 0x2374, 0},
	{"Panasonic:DMC-FZ50",            0x04da, 0x2374, 0},
	{"Panasonic:DMC-LS2",             0x04da, 0x2374, 0},
	/* from Tomas Herrdin <tomas.herrdin@swipnet.se> */
	{"Panasonic:DMC-LS3",             0x04da, 0x2374, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2070864&group_id=8874&atid=358874 */
	{"Panasonic:DMC-TZ15",		  0x04da, 0x2374, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=2950529&group_id=8874 */
	{"Panasonic:DMC-FS62",		  0x04da, 0x2374, 0},
	{"Panasonic:DMC-TZ18",		  0x04da, 0x2374, 0},
	/* Jean Ribière <jean.ribiere@orange.fr> */
	{"Panasonic:DMC-TZ8",		  0x04da, 0x2374, 0},
	{"Panasonic:DMC-LX7",		  0x04da, 0x2374, 0},
	/* Constantin B <klochto@gmail.com> */
	{"Panasonic:DMC-GF1",             0x04da, 0x2374, 0},

	{"Panasonic:DC-GH5",		  0x04da, 0x2382, PTP_CAP|PTP_CAP_PREVIEW},


	/* Søren Krarup Olesen <sko@acoustics.aau.dk> */
	{"Leica:D-LUX 2",		  0x04da, 0x2375, 0},
	/* Graham White <g.graham.white@gmail.com> */
	{"Leica:SL",			  0x04da, 0x2041, 0},

	/* http://callendor.zongo.be/wiki/OlympusMju500 */
	{"Olympus:mju 500",               0x07b4, 0x0113, 0},

        /* Olympus wrap test code */
	{"Olympus:E series (Control)",	  0x07b4, 0x0110, PTP_OLYMPUS_XML},

#if 0 /* talks PTP via SCSI vendor command backchannel, like above. */
	{"Olympus:E-410 (UMS 2 mode)",    0x07b4, 0x0118, 0}, /* not XML wrapped */
#endif

	/* From VICTOR <viaaurea@yahoo.es> */
	{"Olympus:C-350Z",                0x07b4, 0x0114, 0},
	{"Olympus:D-560Z",                0x07b4, 0x0114, 0},
	{"Olympus:X-250",                 0x07b4, 0x0114, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1349900&group_id=8874&atid=108874 */
	{"Olympus:C-310Z",                0x07b4, 0x0114, 0},
	{"Olympus:D-540Z",                0x07b4, 0x0114, 0},
	{"Olympus:X-100",                 0x07b4, 0x0114, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=1442115&group_id=8874&atid=358874 */
	{"Olympus:C-55Z",                 0x07b4, 0x0114, 0},
	{"Olympus:C-5500Z",               0x07b4, 0x0114, 0},

	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1272944&group_id=8874 */
	{"Olympus:IR-300",                0x07b4, 0x0114, 0},

	/* Marcus */
	{"Olympus:FE4000",                0x07b4, 0x0116, 0},
	{"Olympus:X920",                  0x07b4, 0x0116, 0},
	{"Olympus:X925",                  0x07b4, 0x0116, 0},
	{"Olympus:VR360",                 0x07b4, 0x0116, 0},

	/* Jonas Rani Hatem <jr@hatem.be> */
	{"Olympus:TG-620",                0x07b4, 0x0125, 0},

	/* t.ludewig@gmail.com */
	{"Olympus:SP-720UZ",		  0x07b4, 0x012f, 0},
	{"Olympus:E-PL5",		  0x07b4, 0x012f, 0},
	/* Rafał Bryndza <abigor82@gmail.com> */
	{"Olympus:E-M5",		  0x07b4, 0x012f, 0},
	/* Richard Wonka <richard.wonka@gmail.com> */
	{"Olympus:E-M5 Mark II",	  0x07b4, 0x0130, PTP_CAP|PTP_CAP_PREVIEW},
	/* same id also on E-M1 */
	{"Olympus:E-M1 Mark II",	  0x07b4, 0x0130, PTP_CAP|PTP_CAP_PREVIEW},
	{"Olympus:E-M1",	  	  0x07b4, 0x0130, PTP_CAP|PTP_CAP_PREVIEW},

	/* from timelapse-VIEW */
	{"Olympus:E-M1 MII",  	  	  0x07b4, 0x0135, PTP_CAP|PTP_CAP_PREVIEW},

	/* IRC report */
	{"Casio:EX-Z120",                 0x07cf, 0x1042, 0},
	/* Andrej Semen (at suse) */
	{"Casio:EX-S770",                 0x07cf, 0x1049, 0},
	/* https://launchpad.net/bugs/64146 */
	{"Casio:EX-Z700",                 0x07cf, 0x104c, 0},
	/* IRC Reporter */
	{"Casio:EX-Z65",                  0x07cf, 0x104d, 0},

	/* Juan Carlos Bretal Fernandez <juanc.bretal@gmail.com> */
	{"Casio:EX-ZR700",                0x07cf, 0x117a, 0},

	/* (at least some) newer Canon cameras can be switched between
	 * PTP and "normal" (i.e. Canon) mode
	 * Canon PS G3: A. Marinichev, 20 nov 2002
	 */
	{"Canon:PowerShot S45 (PTP mode)",      0x04a9, 0x306d, PTPBUG_DELETE_SENDS_EVENT},
		/* 0x306c is S45 in normal (canon) mode */
	{"Canon:PowerShot G3 (PTP mode)",       0x04a9, 0x306f, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
		/* 0x306e is G3 in normal (canon) mode */
	{"Canon:PowerShot S230 (PTP mode)",     0x04a9, 0x3071, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
		/* 0x3070 is S230 in normal (canon) mode */
	{"Canon:Digital IXUS v3 (PTP mode)",    0x04a9, 0x3071, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
		/* it's the same as S230 */

	{"Canon:Digital IXUS II (PTP mode)",    0x04a9, 0x3072, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot SD100 (PTP mode)",    0x04a9, 0x3072, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A70 (PTP)",           0x04a9, 0x3073, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A60 (PTP)",           0x04a9, 0x3074, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
		/* IXUS 400 has the same PID in both modes, Till Kamppeter */
	{"Canon:Digital IXUS 400 (PTP mode)",   0x04a9, 0x3075, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot S400 (PTP mode)",	0x04a9, 0x3075, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A300 (PTP mode)",     0x04a9, 0x3076, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot S50 (PTP mode)",      0x04a9, 0x3077, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot G5 (PTP mode)",       0x04a9, 0x3085, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Elura 50 (PTP mode)",           0x04a9, 0x3087, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:MVX3i (PTP mode)",              0x04a9, 0x308d, PTPBUG_DELETE_SENDS_EVENT},
		/* 0x3084 is the EOS 300D/Digital Rebel in normal (canon) mode */
	{"Canon:EOS 300D (PTP mode)",           0x04a9, 0x3099, 0},
	{"Canon:EOS Digital Rebel (PTP mode)",  0x04a9, 0x3099, 0},
	{"Canon:EOS Kiss Digital (PTP mode)",   0x04a9, 0x3099, 0},
	{"Canon:PowerShot A80 (PTP)",           0x04a9, 0x309a, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS i (PTP mode)",     0x04a9, 0x309b, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot S1 IS (PTP mode)",    0x04a9, 0x309c, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:MV750i (PTP mode)",    		0x04a9, 0x30a0, PTPBUG_DELETE_SENDS_EVENT},
	/* Canon Elura 65, provolone on #gphoto on 2006-12-17 */
	{"Canon:Elura 65 (PTP mode)",           0x04a9, 0x30a5, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Powershot S70 (PTP mode)",      0x04a9, 0x30b1, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Powershot S60 (PTP mode)",      0x04a9, 0x30b2, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Powershot G6 (PTP mode)",       0x04a9, 0x30b3, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS 500 (PTP mode)",   0x04a9, 0x30b4, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot S500 (PTP mode)",     0x04a9, 0x30b4, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A75 (PTP mode)",      0x04a9, 0x30b5, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot SD110 (PTP mode)",    0x04a9, 0x30b6, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS IIs (PTP mode)",   0x04a9, 0x30b6, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A400 (PTP mode)",     0x04a9, 0x30b7, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A310 (PTP mode)",     0x04a9, 0x30b8, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A85 (PTP mode)",      0x04a9, 0x30b9, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS 430 (PTP mode)",   0x04a9, 0x30ba, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
 	{"Canon:PowerShot S410 (PTP mode)",     0x04a9, 0x30ba, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
 	{"Canon:PowerShot A95 (PTP mode)",      0x04a9, 0x30bb, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
 	{"Canon:EOS 10D (PTP mode)",      	0x04a9, 0x30bc, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Digital IXUS 40 (PTP mode)",    0x04a9, 0x30bf, PTPBUG_DELETE_SENDS_EVENT},
#if 0
	/* the 30c0 id cannot remote capture, use normal mode. */
 	{"Canon:PowerShot SD200 (PTP mode)",    0x04a9, 0x30c0, PTPBUG_DELETE_SENDS_EVENT},
 	{"Canon:Digital IXUS 30 (PTP mode)",    0x04a9, 0x30c0, PTPBUG_DELETE_SENDS_EVENT},
#endif
 	{"Canon:PowerShot A520 (PTP mode)",     0x04a9, 0x30c1, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A510 (PTP mode)",     0x04a9, 0x30c2, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:EOS 1D Mark II (PTP mode)",     0x04a9, 0x30ea, 0},
 	{"Canon:EOS 20D (PTP mode)",            0x04a9, 0x30ec, 0},
	/* 30ef is the ID in explicit PTP mode.
	 * 30ee is the ID with the camera in Canon mode, but the camera reacts to
	 * PTP commands according to:
	 * https://sourceforge.net/tracker/?func=detail&atid=108874&aid=1394326&group_id=8874
	 * They need to have different names.
	 */
	{"Canon:EOS 350D (PTP mode)",           0x04a9, 0x30ee, 0},
	{"Canon:EOS 350D",                      0x04a9, 0x30ef, 0},
	{"Canon:PowerShot S2 IS (PTP mode)",    0x04a9, 0x30f0, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot SD430 (PTP mode)",    0x04a9, 0x30f1, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Digital IXUS Wireless (PTP mode)",0x04a9, 0x30f1, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Digital IXUS 700 (PTP mode)",   0x04a9, 0x30f2, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot SD500 (PTP mode)",    0x04a9, 0x30f2, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	/* reported by Gilles Dartiguelongue <dartigug@esiee.fr> */
	{"Canon:Digital IXUS iZ (PTP mode)",    0x04a9, 0x30f4, PTPBUG_DELETE_SENDS_EVENT},
	/* A340, Andreas Stempfhuber <andi@afulinux.de> */
	{"Canon:PowerShot A430 (PTP mode)",     0x04a9, 0x30f8, PTPBUG_DELETE_SENDS_EVENT},
	/* Conan Colx, A410, gphoto-Feature Requests-1342538 */
	{"Canon:PowerShot A410 (PTP mode)",     0x04a9, 0x30f9, PTPBUG_DELETE_SENDS_EVENT},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1411976&group_id=8874&atid=358874 */
	{"Canon:PowerShot S80 (PTP mode)",      0x04a9, 0x30fa, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	/* A620, Tom Roelz */
	{"Canon:PowerShot A620 (PTP mode)",     0x04a9, 0x30fc, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	/* A610, Andriy Kulchytskyy <whoops@ukrtop.com> */
	{"Canon:PowerShot A610 (PTP mode)",     0x04a9, 0x30fd, PTPBUG_DELETE_SENDS_EVENT},
	/* Irc Reporter */
	{"Canon:PowerShot SD630 (PTP mode)",	0x04a9, 0x30fe, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Digital IXUS 65 (PTP mode)",	0x04a9, 0x30fe, PTPBUG_DELETE_SENDS_EVENT},
	/* Rob Lensen <rob@bsdfreaks.nl> */
	{"Canon:Digital IXUS 55 (PTP mode)",    0x04a9, 0x30ff, 0},
	{"Canon:PowerShot SD450 (PTP mode)",    0x04a9, 0x30ff, 0},
 	{"Canon:Optura 600 (PTP mode)",         0x04a9, 0x3105, 0},
	/* Jeff Mock <jeff@mock.com> */
 	{"Canon:EOS 5D (PTP mode)",             0x04a9, 0x3102, 0},
	/* Nick Richards <nick@nedrichards.com> */
	{"Canon:Digital IXUS 50 (PTP mode)",    0x04a9, 0x310e, PTPBUG_DELETE_SENDS_EVENT},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1640547&group_id=8874&atid=358874 */
	{"Canon:PowerShot A420 (PTP mode)",     0x04a9, 0x310f, PTPBUG_DELETE_SENDS_EVENT},
	/* Some Canon 400D do not have the infamous PTP bug, but some do.
	 * see http://bugs.kde.org/show_bug.cgi?id=141577 -Marcus */
	{"Canon:EOS 400D (PTP mode)",           0x04a9, 0x3110, PTP_CAP},
	{"Canon:EOS Digital Rebel XTi (PTP mode)", 0x04a9, 0x3110, PTP_CAP},
	{"Canon:EOS Kiss Digital X (PTP mode)", 0x04a9, 0x3110, PTP_CAP},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1456391&group_id=8874 */
	{"Canon:EOS 30D (PTP mode)",            0x04a9, 0x3113, PTP_CAP},
	{"Canon:Digital IXUS 900Ti (PTP mode)", 0x04a9, 0x3115, 0},
	{"Canon:PowerShot SD900 (PTP mode)",    0x04a9, 0x3115, 0},
	{"Canon:Digital IXUS 750 (PTP mode)",   0x04a9, 0x3116, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot A700 (PTP mode)",     0x04a9, 0x3117, PTPBUG_DELETE_SENDS_EVENT},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1498577&group_id=8874&atid=358874 */
	{"Canon:PowerShot SD700 (PTP mode)",    0x04a9, 0x3119, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Digital IXUS 800 (PTP mode)",   0x04a9, 0x3119, PTPBUG_DELETE_SENDS_EVENT},
	/* Gert Vervoort <gert.vervoort@hccnet.nl> */
	{"Canon:PowerShot S3 IS (PTP mode)",    0x04a9, 0x311a, PTP_CAP|PTP_CAP_PREVIEW},
	/* David Goodenough <david.goodenough at linkchoose.co.uk> */
	{"Canon:PowerShot A540 (PTP mode)",     0x04a9, 0x311b, PTPBUG_DELETE_SENDS_EVENT},
	/* Irc reporter */
	{"Canon:Digital IXUS 60 (PTP mode)",    0x04a9, 0x311c, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot SD600 (PTP mode)",    0x04a9, 0x311c, PTPBUG_DELETE_SENDS_EVENT},
	/* Harald Dunkel <harald.dunkel@t-online.de> */
	{"Canon:PowerShot G7 (PTP mode)",	0x04a9, 0x3125, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	/* Ales Kozumplik <kozumplik@gmail.com> */
	{"Canon:PowerShot A530 (PTP mode)",     0x04a9, 0x3126, PTPBUG_DELETE_SENDS_EVENT},
	/* Jerome Vizcaino <vizcaino_jerome@yahoo.fr> */
	{"Canon:Digital IXUS 850 IS (PTP mode)",0x04a9, 0x3136, PTPBUG_DELETE_SENDS_EVENT},
	/* https://launchpad.net/bugs/64146 */
	{"Canon:PowerShot SD40 (PTP mode)",	0x04a9, 0x3137, PTPBUG_DELETE_SENDS_EVENT},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1565043&group_id=8874&atid=358874 */
	{"Canon:PowerShot A710 IS (PTP mode)",  0x04a9, 0x3138, PTPBUG_DELETE_SENDS_EVENT},
	/* Thomas Roelz at SUSE, MTP proplist does not work (hangs) */
	{"Canon:PowerShot A640 (PTP mode)",     0x04a9, 0x3139, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A630 (PTP mode)",     0x04a9, 0x313a, PTPBUG_DELETE_SENDS_EVENT},
	/* Deti Fliegl.
	 * Marcus: supports MTP proplists, but these are 2 times slower than regular
	 * data retrieval. */
	{"Canon:EOS 450D (PTP mode)",    	0x04a9, 0x3145, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:EOS Rebel XSi (PTP mode)",    	0x04a9, 0x3145, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:EOS Kiss X2 (PTP mode)",    	0x04a9, 0x3145, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},
	/* reported by Ferry Huberts */
	{"Canon:EOS 40D (PTP mode)",    	0x04a9, 0x3146, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},

	/* reported by: gphoto@lunkwill.org */
	{"Canon:EOS 1D Mark III (PTP mode)",	0x04a9, 0x3147, PTP_CAP},

	{"Canon:PowerShot S5 IS (PTP mode)",    0x04a9, 0x3148, PTP_CAP|PTP_CAP_PREVIEW},
	/* AlannY <alanny@starlink.ru> */
	{"Canon:PowerShot A460 (PTP mode)",	0x04a9, 0x3149, PTP_CAP|PTP_CAP_PREVIEW},
	/* Tobias Blaser <tblaser@gmx.ch> */
	{"Canon:Digital IXUS 950 IS (PTP mode)",0x04a9, 0x314b, PTPBUG_DELETE_SENDS_EVENT},
	/* https://bugs.launchpad.net/bugs/206627 */
	{"Canon:PowerShot SD850 (PTP mode)",	0x04a9, 0x314b, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot A570 IS (PTP mode)",  0x04a9, 0x314c, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot A560 (PTP mode)", 	0x04a9, 0x314d, PTPBUG_DELETE_SENDS_EVENT},
	/* mailreport from sec@dschroeder.info */
	{"Canon:Digital IXUS 75 (PTP mode)",    0x04a9, 0x314e, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot SD750 (PTP mode)",    0x04a9, 0x314e, PTPBUG_DELETE_SENDS_EVENT},
	/* Marcus: MTP Proplist does not work at all here, it just hangs */
	{"Canon:Digital IXUS 70 (PTP mode)",    0x04a9, 0x314f, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot SD1000 (PTP mode)",   0x04a9, 0x314f, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot A550 (PTP mode)",     0x04a9, 0x3150, PTPBUG_DELETE_SENDS_EVENT},
	/* https://launchpad.net/bugs/64146 */
	{"Canon:PowerShot A450 (PTP mode)",     0x04a9, 0x3155, PTPBUG_DELETE_SENDS_EVENT},
	/* Harald Dunkel <harald.dunkel@t-online.de> */
	{"Canon:PowerShot G9 (PTP mode)",       0x04a9, 0x315a, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	/* Barrie Stott <zen146410@zen.co.uk> */
	{"Canon:PowerShot A650IS (PTP mode)",   0x04a9, 0x315b, PTP_CAP|PTP_CAP_PREVIEW},
	/* Roger Lynn <roger@rilynn.demon.co.uk> */
	{"Canon:PowerShot A720 IS (PTP mode)",	0x04a9, 0x315d, PTPBUG_DELETE_SENDS_EVENT},
	/* Mats Petersson <mats.petersson@ltu.se> */
	{"Canon:Powershot SX100 IS (PTP mode)",	0x04a9, 0x315e, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	/* "[AvataR]" <public.avatar@gmail.com> */
	{"Canon:Digital IXUS 960 IS (PTP mode)",0x04a9, 0x315f, PTPBUG_DELETE_SENDS_EVENT},
	/* Ruben Vandamme <vandamme.ruben@belgacom.net> */
	{"Canon:Digital IXUS 860 IS",		0x04a9, 0x3160, PTPBUG_DELETE_SENDS_EVENT},

	/* Christian P. Schmidt" <schmidt@digadd.de> */
	{"Canon:Digital IXUS 970 IS",		0x04a9, 0x3173, PTPBUG_DELETE_SENDS_EVENT},

	/* Martin Lasarsch at SUSE. MTP_PROPLIST returns just 0 entries */
	{"Canon:Digital IXUS 90 IS",		0x04a9, 0x3174, PTPBUG_DELETE_SENDS_EVENT},
	/* Daniel Moyne <daniel.moyne@free.fr> */
	{"Canon:Powershot SD790 IS",		0x04a9, 0x3174, PTPBUG_DELETE_SENDS_EVENT},

	/* https://sourceforge.net/tracker/?func=detail&aid=2722422&group_id=8874&atid=358874 */
	{"Canon:Digital IXUS 85 IS",		0x04a9, 0x3174, PTPBUG_DELETE_SENDS_EVENT},

	/* Theodore Kilgore <kilgota@banach.math.auburn.edu> */
	{"Canon:PowerShot SD770 IS",		0x04a9, 0x3175, PTPBUG_DELETE_SENDS_EVENT},

	/* Olaf Hering at SUSE */
	{"Canon:PowerShot A590 IS",		0x04a9, 0x3176, PTPBUG_DELETE_SENDS_EVENT},

	/* Dmitriy Khanzhin <jinn@altlinux.org> */
	{"Canon:PowerShot A580",		0x04a9, 0x3177, PTPBUG_DELETE_SENDS_EVENT},

	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2602638&group_id=8874&atid=108874 */
	{"Canon:PowerShot A470",		0x04a9, 0x317a, PTPBUG_DELETE_SENDS_EVENT},
	/* Michael Plucik <michaelplucik@googlemail.com> */
	{"Canon:EOS 1000D",			0x04a9, 0x317b, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},

	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1910010&group_id=8874 */
	{"Canon:Digital IXUS 80 IS",		0x04a9, 0x3184, PTPBUG_DELETE_SENDS_EVENT},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2391422&group_id=8874&atid=358874 */
	{"Canon:Powershot SD1100 IS",		0x04a9, 0x3184, PTPBUG_DELETE_SENDS_EVENT},
	/* Hubert Mercier <hubert.mercier@unilim.fr> */
	{"Canon:PowerShot SX10 IS",		0x04a9, 0x318d, PTPBUG_DELETE_SENDS_EVENT},

	/* Novell Bugzilla 852551, Dean Martin <deano_ferrari@hotmail.com> */
	{"Canon:PowerShot A1000 IS",		0x04a9, 0x318e, PTPBUG_DELETE_SENDS_EVENT},

	/* Paul Tinsley */
	{"Canon:PowerShot G10",			0x04a9, 0x318f, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},

	/* Nikolay Rysev <nrysev@gmail.com> */
	{"Canon:PowerShot A2000 IS",		0x04a9, 0x3191, PTPBUG_DELETE_SENDS_EVENT},
	/* Chris Rodley <chris@takeabreak.co.nz> */
	{"Canon:PowerShot SX110 IS",		0x04a9, 0x3192, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},

	/* Kurt Garloff at SUSE */
	{"Canon:Digital IXUS 980 IS",		0x04a9, 0x3193, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot SD990",		0x04a9, 0x3193, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:IXY 3000 IS",			0x04a9, 0x3193, PTPBUG_DELETE_SENDS_EVENT},

	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2750875&group_id=8874&atid=358874 */
	{"Canon:PowerShot SD880 IS",		0x04a9, 0x3196, PTPBUG_DELETE_SENDS_EVENT},

	/* IRC Reporter */
	{"Canon:EOS 5D Mark II",		0x04a9, 0x3199, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},
	/* Thomas Tanner <thomas@tannerlab.com> */
	{"Canon:EOS 7D",			0x04a9, 0x319a, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},
	/* mitch <debianuser@mll.dissimulo.com> */
	{"Canon:EOS 50D",			0x04a9, 0x319b, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},

	/* https://bugs.launchpad.net/ubuntu/+source/libgphoto2/+bug/569419 */
	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:PowerShot D10",			0x04a9, 0x31bc, PTPBUG_DELETE_SENDS_EVENT},

	/* Carsten Grohmann <carstengrohmann@gmx.de> */
	{"Canon:Digital IXUS 110 IS",		0x04a9, 0x31bd, PTPBUG_DELETE_SENDS_EVENT},

	/* Willy Tarreau <w@1wt.eu> */
	{"Canon:PowerShot A2100 IS",		0x04a9, 0x31be, PTPBUG_DELETE_SENDS_EVENT},

	{"Canon:PowerShot A480",		0x04a9, 0x31bf, PTPBUG_DELETE_SENDS_EVENT},

	/* IRC Reporter */
	{"Canon:PowerShot SX200 IS",		0x04a9, 0x31c0, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2789326&group_id=8874&atid=358874 */
	{"Canon:Digital IXUS 990 IS",		0x04a9, 0x31c1, PTPBUG_DELETE_SENDS_EVENT},
	/* Michael Ole Olsen <gnu@gmx.net> */
	{"Canon:PowerShot SD970 IS",		0x04a9, 0x31c1, PTPBUG_DELETE_SENDS_EVENT},
	/* https://sourceforge.net/tracker/?func=detail&aid=2769511&group_id=8874&atid=208874 */
	/* Do not confuse with PowerShot SX100IS */
	{"Canon:Digital IXUS 100 IS",           0x04a9, 0x31c2, PTPBUG_DELETE_SENDS_EVENT},
	/* https://sourceforge.net/tracker/?func=detail&aid=2769511&group_id=8874&atid=208874 */
	{"Canon:PowerShot SD780 IS",		0x04a9, 0x31c2, PTPBUG_DELETE_SENDS_EVENT},
	/* Matthew Vernon <matthew@sel.cam.ac.uk> */
	{"Canon:PowerShot A1100 IS",		0x04a9, 0x31c3, PTPBUG_DELETE_SENDS_EVENT},
	/* Joshua Hoke <jdh@people.homeip.net> */
	{"Canon:Powershot SD1200 IS",		0x04a9, 0x31c4, PTPBUG_DELETE_SENDS_EVENT},
	/* RalfGesellensetter <rgx@gmx.de> */
	{"Canon:Digital IXUS 95 IS",		0x04a9, 0x31c4, PTPBUG_DELETE_SENDS_EVENT},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2796275&group_id=8874&atid=358874 */
	{"Canon:EOS 500D",			0x04a9, 0x31cf, PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:EOS Rebel T1i",			0x04a9, 0x31cf, PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:EOS Kiss X3",			0x04a9, 0x31cf, PTP_CAP|PTP_CAP_PREVIEW},

	/* Peter Lawrence <peter@pjlcentral.com> */
	{"Canon:EOS 1D Mark IV",		0x04a9, 0x31d0, PTP_CAP|PTP_CAP_PREVIEW},

	/* From: Franck GIRARDIN - OPTOCONCEPT <fgirardin@optoconcept.com> */
	{"Canon:PowerShot G11",			0x04a9, 0x31df, 0},

	/* The Wanderer <wanderer@fastmail.fm> */
	{"Canon:PowerShot SX120 IS",		0x04a9, 0x31e0, PTPBUG_DELETE_SENDS_EVENT},
	/* via libmtp */
	{"Canon:PowerShot SX20 IS",		0x04a9, 0x31e4, PTPBUG_DELETE_SENDS_EVENT},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=2918540&group_id=8874&atid=358874 */
	{"Canon:IXY 220 IS",			0x04a9, 0x31e6, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Digital IXUS 120 IS",		0x04a9, 0x31e6, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot SD940 IS",		0x04a9, 0x31e6, PTPBUG_DELETE_SENDS_EVENT},
	/* IRC reporter */
	{"Canon:EOS 550D",			0x04a9, 0x31ea, PTP_CAP|PTP_CAP_PREVIEW},
	/* Mauricio Pasquier Juan <mauricio@pasquierjuan.com.ar> */
	{"Canon:Rebel T2i",			0x04a9, 0x31ea, PTP_CAP|PTP_CAP_PREVIEW},

	/* Andrés Farfán <nafraf@linuxmail.org> */
	{"Canon:Powershot A495",		0x04a9, 0x31ef, PTPBUG_DELETE_SENDS_EVENT},

	/* ErVito on IRC */
	{"Canon:PowerShot A3100 IS",		0x04a9, 0x31f1, PTPBUG_DELETE_SENDS_EVENT},

	{"Canon:PowerShot A3000 IS",		0x04a9, 0x31f2, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Digital IXUS 130",		0x04a9, 0x31f3, PTPBUG_DELETE_SENDS_EVENT},
	/* Mark Voorhies <mvoorhie@yahoo.com> */
	{"Canon:PowerShot SD1300 IS",		0x04a9, 0x31f4, PTPBUG_DELETE_SENDS_EVENT},
	/* Juergen Weigert */
	{"Canon:PowerShot SX210 IS",		0x04a9, 0x31f6, PTPBUG_DELETE_SENDS_EVENT},
	/* name correct? https://bugs.launchpad.net/ubuntu/+source/gvfs/+bug/1296275?comments=all */
	{"Canon:Digital IXUS 300 HS",		0x04a9, 0x31f7, PTPBUG_DELETE_SENDS_EVENT},
	/* via email */
	{"Canon:PowerShot G12",			0x04a9, 0x320f, PTPBUG_DELETE_SENDS_EVENT},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=3153412&group_id=8874&atid=358874 */
	{"Canon:PowerShot SX130 IS",		0x04a9, 0x3211, PTPBUG_DELETE_SENDS_EVENT},
	/* novell bugzilla 871944 */
	{"Canon:PowerShot S95",			0x04a9, 0x3212, PTPBUG_DELETE_SENDS_EVENT},
	/* IRC Reporter */
	{"Canon:EOS 60D",			0x04a9, 0x3215, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=3312353&group_id=8874 */
	{"Canon:EOS 1100D",			0x04a9, 0x3217, PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Rebel T3",			0x04a9, 0x3217, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=3310995&group_id=8874 */
	{"Canon:EOS 600D",			0x04a9, 0x3218, PTP_CAP|PTP_CAP_PREVIEW},
	/* us alias from Andre Crone <andre@elysia.nl> */
	{"Canon:Rebel T3i",			0x04a9, 0x3218, PTP_CAP|PTP_CAP_PREVIEW},

	/* Brian L Murphy <brian.l.murphy@gmail.com> */
	{"Canon:EOS 1D X",			0x04a9, 0x3219, PTP_CAP|PTP_CAP_PREVIEW},

	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:IXUS 310IS",			0x04a9, 0x3225, PTPBUG_DELETE_SENDS_EVENT},

	/* analemma88@gmail.com */
	{"Canon:PowerShot A800",		0x04a9, 0x3226, PTPBUG_DELETE_SENDS_EVENT},

	/* Juha Pesonen <juha.e.pesonen@gmail.com> */
	{"Canon:PowerShot SX230HS",		0x04a9, 0x3228, PTPBUG_DELETE_SENDS_EVENT},

	/* Andrés Farfán <nafraf@linuxmail.org> */
	{"Canon:PowerShot A2200",		0x04a9, 0x322a, PTPBUG_DELETE_SENDS_EVENT},

	/* http://sourceforge.net/tracker/?func=detail&atid=358874&aid=3572477&group_id=8874 */
	{"Canon:PowerShot SX220HS",		0x04a9, 0x322c, PTPBUG_DELETE_SENDS_EVENT},

	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:PowerShot G1 X",		0x04a9, 0x3233, PTPBUG_DELETE_SENDS_EVENT},
	/* Dean Martin <deano_ferrari@hotmail.com>, Novell Bugzilla 852551 */
	{"Canon:PowerShot SX150 IS",		0x04a9, 0x3234, PTPBUG_DELETE_SENDS_EVENT},

	/* IRC reporter */
	{"Canon:PowerShot S100",		0x04a9, 0x3236, PTPBUG_DELETE_SENDS_EVENT},
	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:PowerShot SX40HS",		0x04a9, 0x3238, PTPBUG_DELETE_SENDS_EVENT},
	/* Axel Waggershauser <awagger@web.de> */
	{"Canon:EOS 5D Mark III",		0x04a9, 0x323a, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},
	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:EOS 650D",			0x04a9, 0x323b, PTP_CAP|PTP_CAP_PREVIEW},
	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:Rebel T4i",			0x04a9, 0x323b, PTP_CAP|PTP_CAP_PREVIEW},

	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:EOS M",				0x04a9, 0x323d, 0/*PTP_CAP|PTP_CAP_PREVIEW ... might be unknown opcodes -Marcus */},
	/* via https://bugs.kde.org/show_bug.cgi?id=311393 */
	{"Canon:PowerShot A1300IS",		0x04a9, 0x323e, PTPBUG_DELETE_SENDS_EVENT},
	/* https://forums.opensuse.org/showthread.php/493692-canon-usb-camera-a810-not-detected */
	{"Canon:PowerShot A810",		0x04a9, 0x323f, PTPBUG_DELETE_SENDS_EVENT},

	/* Giulio Fidente <gfidente@gmail.com> */
	{"Canon:IXUS 125HS",			0x04a9, 0x3241, PTPBUG_DELETE_SENDS_EVENT},
	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:PowerShot A4000IS",		0x04a9, 0x3243, PTPBUG_DELETE_SENDS_EVENT},
	/* Peter Ivanyi <ipeter88@gmail.com> */
	{"Canon:PowerShot SX260HS",		0x04a9, 0x3244, PTPBUG_DELETE_SENDS_EVENT},
	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:PowerShot SX240HS",		0x04a9, 0x3245, PTPBUG_DELETE_SENDS_EVENT},
	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:PowerShot A2400IS",		0x04a9, 0x3249, PTPBUG_DELETE_SENDS_EVENT},
	/* Coro Fe <corofecoro@gmail.com> */
	{"Canon:PowerShot A2300IS",		0x04a9, 0x324a, PTPBUG_DELETE_SENDS_EVENT},

	/* "François G." <francois@webcampak.com> */
	{"Canon:EOS 6D",			0x04a9, 0x3250, PTP_CAP|PTP_CAP_PREVIEW},
	/* Andre Crone <andre@elysia.nl> */
	{"Canon:EOS 1D C",			0x04a9, 0x3252, PTP_CAP|PTP_CAP_PREVIEW},
	/* Thorsten Ludewig <t.ludewig@gmail.com> */
	{"Canon:EOS 70D",			0x04a9, 0x3253, PTP_CAP|PTP_CAP_PREVIEW},

	/* Thorsten Ludewig <t.ludewig@gmail.com> */
	{"Canon:PowerShot G15",			0x04a9, 0x3258, PTPBUG_DELETE_SENDS_EVENT},

	/* Andrés Farfán <nafraf@linuxmail.org> */
	{"Canon:PowerShot SX160IS",		0x04a9, 0x325a, PTPBUG_DELETE_SENDS_EVENT},

	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:PowerShot S110 (PTP Mode)",	0x04a9, 0x325b, PTPBUG_DELETE_SENDS_EVENT},
	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:PowerShot SX500IS",		0x04a9, 0x325c, PTPBUG_DELETE_SENDS_EVENT},

	/* Andrés Farfán <nafraf@linuxmail.org> */
	{"Canon:PowerShot SX280HS",		0x04a9, 0x325f, PTPBUG_DELETE_SENDS_EVENT},

	/* Marcus Meissner */
	{"Canon:PowerShot A3500IS",		0x04a9, 0x3261, PTPBUG_DELETE_SENDS_EVENT},

	/* Canon Powershot A2600 */
	{"Canon:PowerShot A2600",		0x04a9, 0x3262, PTPBUG_DELETE_SENDS_EVENT},

	/* Andrés Farfán <nafraf@linuxmail.org> */
	{"Canon:PowerShot A1400",		0x04a9, 0x3264, PTPBUG_DELETE_SENDS_EVENT},
	/* https://bugs.launchpad.net/ubuntu/+source/gvfs/+bug/1296275?comments=all  */
	{"Canon:Digital IXUS 255HS",		0x04a9, 0x3268, PTPBUG_DELETE_SENDS_EVENT},

	/* t.ludewig@gmail.com */
	{"Canon:EOS 7D MarkII",			0x04a9, 0x326f, PTP_CAP|PTP_CAP_PREVIEW},

	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:EOS 100D",			0x04a9, 0x3270, PTP_CAP|PTP_CAP_PREVIEW},

	/* Ronny Kalusniok <ladiko@gmail.com> */
	{"Canon:PowerShot A2500",		0x04a9, 0x3271, PTPBUG_DELETE_SENDS_EVENT},

	/* "T. Ludewig" <t.ludewig@gmail.com> */
	{"Canon:EOS 700D",			0x04a9, 0x3272, PTP_CAP|PTP_CAP_PREVIEW},

	/* Alexey Kryukov <amkryukov@gmail.com> */
	{"Canon:EOS M2",			0x04a9, 0x3273, PTP_CAP|PTP_CAP_PREVIEW},

	/* Harald Krause <haraldkrause@gmx.com> */
	{"Canon:PowerShot G16",			0x04a9, 0x3274, 0},

	/* From: Gerwin Voorsluijs <g.m.voorsluijs@online.nl> */
	{"Canon:PowerShot S120",		0x04a9, 0x3275, PTPBUG_DELETE_SENDS_EVENT},

	/* Andrés Farfán <nafraf@linuxmail.org> */
	{"Canon:PowerShot SX170 IS",		0x04a9, 0x3276, PTPBUG_DELETE_SENDS_EVENT},

	/* Andrés Farfán <nafraf@linuxmail.org> */
	{"Canon:PowerShot SX510 HS",		0x04a9, 0x3277, PTPBUG_DELETE_SENDS_EVENT},

	/* Wolfram Sang <wsa@kernel.org> */
	{"Canon:Digital IXUS 132",		0x04a9, 0x327d, PTPBUG_DELETE_SENDS_EVENT},

	/* thinkgareth <thinkgareth@users.sf.net> */
	{"Canon:EOS 1200D",			0x04a9, 0x327f, PTP_CAP|PTP_CAP_PREVIEW},

	/* Iain Paton <ipaton0@gmail.com> */
	{"Canon:EOS 760D",			0x04a9, 0x3280, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/92 */
	{"Canon:EOS 5D Mark IV",		0x04a9, 0x3281, PTP_CAP|PTP_CAP_PREVIEW|PTP_DONT_CLOSE_SESSION},

	/* Marcus parents  ... preview works a bit, capture on a full moon */
	{"Canon:PowerShot SX600 HS",		0x04a9, 0x3286, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},

	/* https://sourceforge.net/p/gphoto/feature-requests/445/ */
	{"Canon:PowerShot Elph135",		0x04a9, 0x3288, PTPBUG_DELETE_SENDS_EVENT},

	/* Reed Johnson <rmjohns1@gmail.com> */
	{"Canon:PowerShot Elph340HS",		0x04a9, 0x3289, PTPBUG_DELETE_SENDS_EVENT},

	/* Jesper Pedersen <jesper.pedersen@comcast.net */
	{"Canon:EOS 1D X MarkII",		0x04a9, 0x3292, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/60 */
	/* needs dont close session */
	{"Canon:EOS 80D",			0x04a9, 0x3294, PTP_CAP|PTP_CAP_PREVIEW|PTP_DONT_CLOSE_SESSION},
	/* Andre Crone <andre@elysia.nl */
	{"Canon:EOS 5DS",			0x04a9, 0x3295, PTP_CAP|PTP_CAP_PREVIEW|PTP_DONT_CLOSE_SESSION},
	/* Nykhedimus S <nykhedimus@gmail.com> */
	{"Canon:EOS M3",			0x04a9, 0x3299, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	/* Shaul Badusa <shaulikoo@gmail.com> */
	{"Canon:PowerShot SX60HS",		0x04a9, 0x329a, PTPBUG_DELETE_SENDS_EVENT},
	/* pravsripad@gmail.com */
	{"Canon:PowerShot SX520 HS",		0x04a9, 0x329b, PTPBUG_DELETE_SENDS_EVENT},

	/* sparkycoladev@gmail.com */
	{"Canon:PowerShot G7 X",		0x04a9, 0x329d, PTP_CAP|PTP_CAP_PREVIEW|PTP_DONT_CLOSE_SESSION},

	/* Sascha Wolff <sascha.wolff1@gmail.com> */
	{"Canon:PowerShot SX530 HS",		0x04a9, 0x329f, PTP_CAP|PTP_CAP_PREVIEW|PTP_DONT_CLOSE_SESSION},

	/* Marcus Meissner <marcus@jet.franken.de> */
	{"Canon:EOS M10",			0x04a9, 0x32a0, PTP_CAP|PTP_CAP_PREVIEW},

	/* Johannes Goecke <jg-ml@web.de> */
	{"Canon:EOS 750D",			0x04a9, 0x32a1, PTP_CAP|PTP_CAP_PREVIEW},

	/* Tomas.Linden@helsinki.fi */
	{"Canon:PowerShot G3 X",		0x04a9, 0x32a8, PTP_CAP|PTP_CAP_PREVIEW},
	/* Kiss Tamas <kisst@bgk.bme.hu> */
	{"Canon:IXUS 165",			0x04a9, 0x32a9, PTPBUG_DELETE_SENDS_EVENT},

	/* Gong <elainegong2010@gmail.com> */
	{"Canon:IXUS 160",			0x04a9, 0x32aa, PTPBUG_DELETE_SENDS_EVENT},

	/* Gerald Leung <gerald@geraldleung.ca> */
	{"Canon:PowerShot ELPH 350 HS",		0x04a9, 0x32ab, PTPBUG_DELETE_SENDS_EVENT},

	/* Andre Crone <andre@elysia.nl */
	{"Canon:EOS 5DS R",			0x04a9, 0x32af, PTP_CAP|PTP_CAP_PREVIEW|PTP_DONT_CLOSE_SESSION},

	/* Erwin.Segerer@gmx.de */
	{"Canon:PowerShot G5X",			0x04a9, 0x32b3, PTP_CAP|PTP_CAP_PREVIEW},

	/* Barney Livingston <barney.livingston@lobsterpictures.tv> */
	{"Canon:EOS 1300D",			0x04a9, 0x32b4, PTP_CAP|PTP_CAP_PREVIEW},
	/* Rebel T6 is the same camera. Jasem Mutlaq <mutlaqja@ikarustech.com> */
	{"Canon:EOS Rebel T6",			0x04a9, 0x32b4, PTP_CAP|PTP_CAP_PREVIEW},

	/* Jim Howard <jh.xsnrg@gmail.com> */
	{"Canon:EOS M5",			0x04a9, 0x32bb, PTP_CAP|PTP_CAP_PREVIEW},

	/* Hubert F */
	{"Canon:PowerShot G7 X Mark II",        0x04a9, 0x32bc, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/325 */
	{"Canon:PowerShot SX540 HS",		0x04a9, 0x32be, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/84 */
	{"Canon:Digital IXUS 180",		0x04a9, 0x32c0, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/316 */
	{"Canon:SX 720HS",			0x04a9, 0x32c2, PTP_CAP|PTP_CAP_PREVIEW},

	/* Sagufta Kapadia <sagufta.kapadia@gmail.com> */
	{"Canon:SX 620HS",			0x04a9, 0x32c3, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/235 */
	{"Canon:EOS M6",			0x04a9, 0x32c5, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/379 */
	{"Canon:PowerShot G9 X Mark II",	0x04a9, 0x32c7, PTP_CAP|PTP_CAP_PREVIEW},

	/* Viktors Berstis <gpjm@berstis.com> */
	{"Canon:EOS Rebel T7i",			0x04a9, 0x32c9, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/338 */
	{"Canon:EOS 800D",			0x04a9, 0x32c9, PTP_CAP|PTP_CAP_PREVIEW},

	/* Thomas Schaaf <thomas.schaaf@komola.de> */
	{"Canon:EOS 6d Mark II",		0x04a9, 0x32ca, PTP_CAP|PTP_CAP_PREVIEW},

	/* Daniel Muller, jednatel SourcePaint s.r.o. <dan@sourcepaint.cz> */
	{"Canon:EOS 77D",			0x04a9, 0x32cb, PTP_CAP|PTP_CAP_PREVIEW},

	/* "Lacy Rhoades" <lacy@colordeaf.net> */
	{"Canon:EOS 200D",          		0x04a9, 0x32cc, PTP_CAP|PTP_CAP_PREVIEW},

	/* Geza Lore <gezalore@gmail.com> */
	{"Canon:EOS M100",          		0x04a9, 0x32d1, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/58 */
	{"Canon:EOS M50",          		0x04a9, 0x32d2, PTP_CAP|PTP_CAP_PREVIEW},

	/* Marcus Meissner */
	{"Canon:Digital IXUS 185",          	0x04a9, 0x32d4, 0},

	/* Slavko Kocjancic <eslavko@gmail.com> */
	{"Canon:Digital PowerShot SX730HS",	0x04a9, 0x32d6, PTP_CAP|PTP_CAP_PREVIEW},

	/* Jasem Mutlaq <mutlaqja@ikarustech.com> */
	{"Canon:EOS 4000D",			0x04a9, 0x32d9, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},

	/* Elijah Parker <mail@timelapseplus.com> */
	{"Canon:EOS R",          		0x04a9, 0x32da, PTP_CAP|PTP_CAP_PREVIEW|PTP_DONT_CLOSE_SESSION},

	/* Christian Muehlhaeuser <muesli@gmail.com> */
	{"Canon:EOS 2000D",			0x04a9, 0x32e1, PTP_CAP|PTP_CAP_PREVIEW|PTP_DONT_CLOSE_SESSION|PTPBUG_DELETE_SENDS_EVENT},
	/* https://github.com/gphoto/gphoto2/issues/247, from logfile */
	{"Canon:EOS 1500D",          		0x04a9, 0x32e1, PTP_CAP|PTP_CAP_PREVIEW|PTP_DONT_CLOSE_SESSION|PTPBUG_DELETE_SENDS_EVENT},

	/* from Enno at SUSE */
	{"Canon:EOS RP",          		0x04a9, 0x32e2, PTP_CAP|PTP_CAP_PREVIEW|PTP_DONT_CLOSE_SESSION},

	/* https://github.com/gphoto/libgphoto2/issues/316 */
	{"Canon:PowerShot SX740 HS",		0x04a9, 0x32e4, PTP_CAP|PTP_CAP_PREVIEW},

	/*Marc Wetli <wetli@egoshooting.com> */
	{"Canon:EOS M6 Mark II",		0x04a9, 0x32e7, PTP_CAP|PTP_CAP_PREVIEW},

	/* id from timelapse-view */
	{"Canon:EOS 1D X MarkIII",		0x04a9, 0x32e8, PTP_CAP|PTP_CAP_PREVIEW},

	/* Roland Förg <roland.foerg@arcor.de> */
	{"Canon:EOS 250D",			0x04a9, 0x32e9, PTP_CAP|PTP_CAP_PREVIEW},

	/* Matthias <matthias@mail-s.eu> */
	{"Canon:EOS 90D",			0x04a9, 0x32ea, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/gphoto2/issues/347 */
	{"Canon:PowerShot SX70 HS",		0x04a9, 0x32ee, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/614 */
	{"Canon:EOS M200",			0x04a9, 0x32ef, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/781 */
	{"Canon:EOS Rebel T8i",			0x04a9, 0x32f1, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/810 */
	{"Canon:EOS 850D",			0x04a9, 0x32f1, PTP_CAP|PTP_CAP_PREVIEW},
	/* from timelapse-view */
	{"Canon:EOS R5",			0x04a9, 0x32f4, PTP_CAP|PTP_CAP_PREVIEW},
	/* Steve Rencontre <steve@rsn-tech.co.uk> */
	{"Canon:EOS R6",			0x04a9, 0x32f5, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/832 */
	{"Canon:EOS R7",			0x04a9, 0x32f7, PTP_CAP|PTP_CAP_PREVIEW},
	/* Marc Wetli */
	{"Canon:EOS R10",			0x04a9, 0x32f8, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/642 */
	{"Canon:EOS M50m2",			0x04a9, 0x32f9, PTP_CAP|PTP_CAP_PREVIEW},
	/* Ingmar Rieger via email */
	{"Canon:EOS R5 C",			0x04a9, 0x3303, PTP_CAP|PTP_CAP_PREVIEW},

	/* Konica-Minolta PTP cameras */
	{"Konica-Minolta:DiMAGE A2 (PTP mode)",        0x132b, 0x0001, 0},
	{"Konica-Minolta:DiMAGE Z2 (PictBridge mode)", 0x132b, 0x0007, 0},
	{"Konica-Minolta:DiMAGE X21 (PictBridge mode)",0x132b, 0x0009, 0},
	{"Konica-Minolta:DiMAGE Z3 (PictBridge mode)", 0x132b, 0x0018, 0},
	{"Konica-Minolta:DiMAGE A200 (PictBridge mode)",0x132b, 0x0019, 0},
	{"Konica-Minolta:DiMAGE Z5 (PictBridge mode)", 0x132b, 0x0022, 0},
	{"Konica-Minolta:DiMAGE Z6 (PictBridge mode)", 0x132b, 0x0033, 0},

	/* Fuji PTP cameras */
	{"Fuji:FinePix S7000",			0x04cb, 0x0142, 0},
	{"Fuji:FinePix A330",			0x04cb, 0x014a, 0},
	/* Hans-Joachim Baader <hjb@pro-linux.de> */
	{"Fuji:FinePix S9500",			0x04cb, 0x018f, 0},
	{"Fuji:FinePix E900",			0x04cb, 0x0193, 0},
	{"Fuji:FinePix F30",			0x04cb, 0x019b, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1620750&group_id=8874 */
	{"Fuji:FinePix S6500fd",		0x04cb, 0x01bf, 0},
	/* https://launchpad.net/bugs/89743 */
	{"Fuji:FinePix F20",			0x04cb, 0x01c0, 0},
	/* launchpad 67532 */
	{"Fuji:FinePix F31fd",			0x04cb, 0x01c1, 0},
	/* http://sourceforge.net/tracker/?func=detail&atid=358874&aid=2881948&group_id=8874 */
	{"Fuji:S5 Pro",			        0x04cb, 0x01c3, PTP_CAP},
	{"Fuji:FinePix S5700",			0x04cb, 0x01c4, 0},
	{"Fuji:FinePix F40fd",			0x04cb, 0x01c5, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1800289&group_id=8874&atid=358874 */
	{"Fuji:FinePix A820",			0x04cb, 0x01c6, 0},
	/* Louis Byrne <louisbyrneca@hotmail.com> */
	{"Fuji:FinePix A610",			0x04cb, 0x01d0, 0},
	/* g4@catking.net */
	{"Fuji:FinePix A800",			0x04cb, 0x01d2, 0},
	/* Gerhard Schmidt <gerd@dg4fac.de> */
	{"Fuji:FinePix A920",			0x04cb, 0x01d3, 0},
	/* Teppo Jalava <tjjalava@gmail.com> */
	{"Fuji:FinePix F50fd",			0x04cb, 0x01d4, 0},
	/* IRC reporter */
	{"Fuji:FinePix S5800",			0x04cb, 0x01d7, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=108874&aid=1945259&group_id=8874 */
	{"Fuji:FinePix Z100fd",			0x04cb, 0x01d8, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=2017171&group_id=8874&atid=358874 */
	{"Fuji:FinePix S100fs",			0x04cb, 0x01db, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2772199&group_id=8874&atid=358874 */
	{"Fuji:FinePix S1000fd",		0x04cb, 0x01dd, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2203316&group_id=8874&atid=358874 */
	{"Fuji:FinePix F100fd",			0x04cb, 0x01e0, 0},
	/*https://sourceforge.net/tracker/index.php?func=detail&aid=2820380&group_id=8874&atid=358874 */
	{"Fuji:FinePix F200 EXR",		0x04cb, 0x01e4, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2993118&group_id=8874&atid=358874  */
	{"Fuji:FinePix F60fd",			0x04cb, 0x01e6, 0},
	/* Gerhard Schmidt <gerd@dg4fac.de> */
	{"Fuji:FinePix S2000HD",		0x04cb, 0x01e8, 0},
	{"Fuji:FinePix S1500",			0x04cb, 0x01ef, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=3018889&group_id=8874&atid=358874 */
	{"Fuji:FinePix F70 EXR",		0x04cb, 0x01fa, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=3111790&group_id=8874&atid=358874 */
	{"Fuji:Fujifilm A220",			0x04cb, 0x01fe, 0},
	/* NoOp <glgxg@sbcglobal.net> */
	{"Fuji:FinePix S1800",			0x04cb, 0x0200, 0},
	/* Luke Symes <allsymes@gmail.com> */
	{"Fuji:FinePix Z35",			0x04cb, 0x0201, 0},
	/* "Steven A. McIntosh" <mcintosh@cotterochan.co.uk> */
	{"Fuji:FinePix S2500HD",		0x04cb, 0x0209, 0},
	/* Erik Hahn <erik_hahn@gmx.de> */
	{"Fuji:FinePix F80EXR",			0x04cb, 0x020e, 0},
	/* salsaman <salsaman@gmail.com> */
	{"Fuji:FinePix Z700EXR",		0x04cb, 0x020d, 0},
	/* https://bugs.launchpad.net/ubuntu/+source/gvfs/+bug/1311953 */
	{"Fuji:FinePix AV-150",			0x04cb, 0x021b, 0},
	/* Gregor Voss <gregor.voss@gmx.de> */
	{"Fuji:FinePix H20EXR",			0x04cb, 0x022d, 0},
	/* https://bugs.launchpad.net/ubuntu/+source/gvfs/+bug/1296275?comments=all  */
	{"Fuji:FinePix T200",			0x04cb, 0x0233, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=3556692&group_id=8874&atid=108874 */
	{"Fuji:FinePix S2950",			0x04cb, 0x0240, 0},
	/* https://sourceforge.net/p/gphoto/bugs/974/ */
	{"Fuji:FinePix JX370",			0x04cb, 0x0250, 0},
	/* Luis Arias <kaaloo@gmail.com> */
	{"Fuji:FinePix X10",			0x04cb, 0x0263, 0},
	/* t.ludewig@gmail.com */
	{"Fuji:FinePix S4300",			0x04cb, 0x0265, 0},
	/* t.ludewig@gmail.com */
	{"Fuji:FinePix X-S1",			0x04cb, 0x026e, 0},
	/* t.ludewig@gmail.com */
	{"Fuji:FinePix HS30EXR",		0x04cb, 0x0271, 0},
	/* Marcel Bonnet <marcelbonnet@gmail.com> */
	{"Fuji:FinePix S2980",			0x04cb, 0x027d, 0},
	/* Michael Martin <mischel88@web.de> */
	{"Fuji:FinePix X-E1",			0x04cb, 0x0283, 0},
	/* t.ludewig@gmail.com */
	{"Fuji:FinePix XF1",			0x04cb, 0x0288, 0},
	/* Steve Dahl <dahl@goshawk.com> */
	{"Fuji:FinePix S4850",			0x04cb, 0x0298, 0},
	/* Larry D. DeMaris" <demarisld@gmail.com> */
	{"Fuji:FinePix SL1000",			0x04cb, 0x029c, 0},
	/* t.ludewig@gmail.com */
	{"Fuji:FinePix X20",			0x04cb, 0x02a6, 0},
	/* https://sourceforge.net/p/libmtp/bugs/1040/ */
	{"Fuji:Fujifilm X-E2",			0x04cb, 0x02b5, 0},
	/* https://github.com/gphoto/libgphoto2/issues/283 */
	{"Fuji:Fujifilm X-M1",			0x04cb, 0x02b6, 0},
	/* luigiwriter2@gmail.com */
	{"Fuji:FinePix S8600 ",			0x04cb, 0x02b9, 0},
	/* https://github.com/gphoto/libgphoto2/issues/281 */
	{"Fuji:Fujifilm X70",			0x04cb, 0x02ba, 0},
	/* Vladimir K <enewsletters@inbox.ru>, https://github.com/gphoto/libgphoto2/issues/283 */
	{"Fuji:Fujifilm X-T1",			0x04cb, 0x02bf, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/281 */
	{"Fuji:Fujifilm X30",			0x04cb, 0x02c1, 0},
	/* https://sourceforge.net/p/gphoto/feature-requests/464/ */
	{"Fuji:Fujifilm X-A2",			0x04cb, 0x02c6, 0},
	/* https://github.com/gphoto/libgphoto2/issues/32 */
	{"Fuji:Fujifilm X-T10",			0x04cb, 0x02c8, 0},
	/* with new updated firmware 4.0, https://github.com/gphoto/libgphoto2/issues/220 */
	{"Fuji:Fujifilm X-Pro2",		0x04cb, 0x02cb, PTP_CAP|PTP_CAP_PREVIEW},
	/* with new updated firmware 1.1 */
	{"Fuji:Fujifilm X-T2",			0x04cb, 0x02cd, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/283 */
	{"Fuji:Fujifilm X100F",			0x04cb, 0x02d1, 0},
	/* https://github.com/gphoto/libgphoto2/issues/133 */
	{"Fuji:GFX 50 S",			0x04cb, 0x02d3, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/170 */
	{"Fuji:Fujifilm X-T20",			0x04cb, 0x02d4, 0},
	/* Рустем Валиев <rustvt@gmail.com> */
	{"Fuji:Fujifilm X-A5",			0x04cb, 0x02d5, 0},
	/* Daniel Queen <dqueen510@gmail.com> */
	/* USB Raw Converter/Backup Restore mode, firmware version 1.20 or newer */
	{"Fuji:Fujifilm X-E3",			0x04cb, 0x02d6, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/283 */
	{"Fuji:Fujifilm X-H1",			0x04cb, 0x02d7, PTP_CAP|PTP_CAP_PREVIEW},
	/* Seth Cohen <forwardthinking.llc@gmail.com> */
	{"Fuji:GFX 50 R",			0x04cb, 0x02dc, PTP_CAP|PTP_CAP_PREVIEW},
	/* Stefan Weiberg at SUSE */
	{"Fuji:Fujifilm X-T3",			0x04cb, 0x02dd, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/gphoto2/issues/256 */
	{"Fuji:Fujifilm GFX100",		0x04cb, 0x02de, PTP_CAP|PTP_CAP_PREVIEW},
	/* Bruno Filho at SUSE (currently not working with cpature, but shows variables) */
	/* so far looks like the low end X-T30 does not support tethering, https://www.dpreview.com/forums/thread/4451199 */
	{"Fuji:Fujifilm X-T30",			0x04cb, 0x02e3, PTP_CAP_PREVIEW},
	/* via email to gphoto-devel on Apr 2 2021 */
	{"Fuji:Fujifilm X-Pro3",		0x04cb, 0x02e4, PTP_CAP|PTP_CAP_PREVIEW},
	{"Fuji:Fujifilm X100V",			0x04cb, 0x02e5, PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/505 */
	{"Fuji:Fujifilm X-T4",			0x04cb, 0x02e6, PTP_CAP|PTP_CAP_PREVIEW},
	/* via email */
	{"Fuji:Fujifilm X-E4",			0x04cb, 0x02e8, 0},
	/* via https://sourceforge.net/p/gphoto/feature-requests/491/ */
	{"Fuji:Fujifilm GFX 100S (2nd)",	0x04cb, 0x02e9, PTP_CAP|PTP_CAP_PREVIEW},
	/* via timelapse-view */
	{"Fuji:Fujifilm GFX 100S",		0x04cb, 0x02ea, PTP_CAP|PTP_CAP_PREVIEW},
	/* https://github.com/gphoto/libgphoto2/issues/603 */
	{"Fuji:Fujifilm X-S10",			0x04cb, 0x02ea, PTP_CAP_PREVIEW},	/* only webcam mode apparently */
	{"Fuji:Fujifilm X-H2",			0x04cb, 0x02f2, PTP_CAP|PTP_CAP_PREVIEW},

	{"Ricoh:Caplio R5 (PTP mode)",          0x05ca, 0x0110, 0},
	{"Ricoh:Caplio GX (PTP mode)",          0x05ca, 0x0325, 0},
	{"Sea & Sea:5000G (PTP mode)",		0x05ca, 0x0327, 0},
	/* Michael Steinhauser <mistr@online.de> */
	{"Ricoh:Caplio R1v (PTP mode)",		0x05ca, 0x032b, 0},
	{"Ricoh:Caplio R3 (PTP mode)",          0x05ca, 0x032f, 0},
	/* Arthur Butler <arthurbutler@otters.ndo.co.uk> */
	{"Ricoh:Caplio RR750 (PTP mode)",	0x05ca, 0x033d, 0},
	/* Gerald Pfeifer at SUSE */
	{"Sea & Sea:2G (PTP mode)",		0x05ca, 0x0353, 0},

	/* Marcus Meissner */
	{"Ricoh:Theta m15 (PTP mode)",		0x05ca, 0x0365, 0},

	/* "Lacy Rhoades" <lacy@colordeaf.net> */
	{"Ricoh:Theta S (PTP mode)",		0x05ca, 0x0366, 0},
	{"Ricoh:Theta SC (PTP mode)",		0x05ca, 0x0367, 0},

	/* https://github.com/libmtp/libmtp/pull/68  */
	{"Ricoh:Theta V (PTP mode)",		0x05ca, 0x0368, 0},
	{"Ricoh:Theta Z1 (PTP mode)",		0x05ca, 0x036d, 0},

	/* Rollei dr5  */
	{"Rollei:dr5 (PTP mode)",               0x05ca, 0x220f, 0},

	/* Ricoh Caplio GX 8 */
	{"Ricoh:Caplio GX 8 (PTP mode)",        0x05ca, 0x032d, 0},

	/* Arda Kaan <ardakaan@gmail.com> */
	{"Ricoh:WG-M2 (PTP mode)",        	0x25fb, 0x210b, 0},

	/* Pentax cameras */
	{"Pentax:Optio 43WR",                   0x0a17, 0x000d, 0},
	/* Stephan Barth at SUSE */
	{"Pentax:Optio W90",                    0x0a17, 0x00f7, 0},

	/* gphoto:feature-requests 452. yes, weird vendor. */
	{"Pentax:K3 (PTP Mode)",		0x25fb, 0x0165, 0},
	/* https://github.com/gphoto/gphoto2/issues/469 */
	{"Pentax:K1 (PTP Mode)",		0x25fb, 0x0179, 0},
	/* https://github.com/gphoto/gphoto2/issues/459 */
	{"Pentax:K3 II (PTP Mode)",		0x25fb, 0x017b, 0},
	/* Keld Henningsen <drawsacircle@hotmail.com */
	{"Pentax:K70 (PTP Mode)",		0x25fb, 0x017d, 0},

	/* via email to gphoto-devel */
	{"Pentax:KP (PTP Mode)",		0x25fb, 0x017f, 0},

	{"Sanyo:VPC-C5 (PTP mode)",             0x0474, 0x0230, 0},
	/* https://github.com/gphoto/libgphoto2/issues/497 */
	{"Sanyo:VPC-FH1 (PTP mode)",            0x0474, 0x02e5, 0},

	/* from Mike Meyer <mwm@mired.org>. Does not support MTP. */
	{"Apple:iPhone (PTP mode)",		0x05ac, 0x1290, 0},
	/* IRC reporter adjusted info */
	{"Apple:iPod Touch (PTP mode)",		0x05ac, 0x1291, 0},
	/* irc reporter. MTP based. */
	{"Apple:iPhone 3G (PTP mode)",		0x05ac, 0x1292, 0},
	/* Marco Michna at SUSE */
	{"Apple:iPod Touch 2G (PTP mode)",	0x05ac, 0x1293, 0},
	/* Mark Lehrer <mark@knm.org> */
	{"Apple:iPhone 3GS (PTP mode)",		0x05ac, 0x1294, 0},

	/* Rasmus P */
	{"Apple:iPhone 4 (PTP mode)",		0x05ac, 0x1297, 0},

	{"Apple:iPod Touch 3rd Gen (PTP mode)",	0x05ac, 0x1299, 0},
	{"Apple:iPad (PTP mode)",		0x05ac, 0x129a, 0},

	/* Don Cohen <don-sourceforge-xxzw@isis.cs3-inc.com> */
	{"Apple:iPhone 4S (PTP mode)",		0x05ac, 0x12a0, 0},

	/* grinchdee@gmail.com */
	{"Apple:iPhone 5 (PTP mode)",		0x05ac, 0x12a8, 0},

	/* chase.thunderstrike@gmail.com */
	{"Apple:iPad Air",			0x05ac, 0x12ab, 0},

	/* https://sourceforge.net/tracker/index.php?func=detail&aid=1869653&group_id=158745&atid=809061 */
	{"Pioneer:DVR-LX60D",			0x08e4, 0x0142, 0},

	/* https://sourceforge.net/tracker/index.php?func=detail&aid=1680029&group_id=8874&atid=108874 */
	{"Nokia:N73",				0x0421, 0x0488, 0},

	/* gbakos@astro.princeton.edu */
	{"Samsung:NX1",				0x04e8,	0x140c, 0},
	/* IRC reporter */
	{"Samsung:S5620",			0x04e8,	0x684a, 0},

	/* Israel Barrientos <jbarrien@gmail.com> */
	{"Samsung:NX1000",			0x04e8,	0x1384, 0},

	/* This is a camera ... reported by TAN JIAN QI <JQTAN1@e.ntu.edu.sg */
	{"Samsung:EK-GC100",			0x04e8,	0x6866, 0},

	/* 522903503@qq.com */
	{"Sigma:fp",				0x1003,	0xc432, PTP_CAP|PTP_CAP_PREVIEW},

	/* Bernhard Wagner <me@bernhardwagner.net> */
	{"Leica:M9",				0x1a98,	0x0002, PTP_CAP},

	/* Christopher Kao <christopherkao@icloud.com> */
	{"Leica:SL (Typ 601)",			0x1a98,	0x2041, PTP_CAP|PTP_CAP_PREVIEW},

	/* https://github.com/gphoto/libgphoto2/issues/105 */
	{"Parrot:Sequoia",			0x19cf,	0x5039, PTP_CAP},

	{"GoPro:HERO" ,				0x2672, 0x000c, 0},
	{"GoPro:HERO4 Silver" , 		0x2672, 0x000d, 0 },

	/* https://sourceforge.net/p/gphoto/support-requests/130/ */
	{"GoPro:HERO 4",			0x2672,	0x000e, 0},
	/* Tomas Zigo <tomas.zigo@slovanet.sk> */
	{"GoPro:HERO 3+",			0x2672,	0x0011, 0},
	/* https://sourceforge.net/u/drzap/libmtp/ci/39841e9a15ed250a0121ae4a139b2a950a07f08c/ */
	{"GoPro:HERO +",			0x2672,	0x0021, 0},
	/* from libmtp */
	{"GoPro:HERO5 Black",			0x2672, 0x0027, 0},
	{"GoPro:HERO5 Session",			0x2672, 0x0029, 0},

	/* https://sourceforge.net/p/libmtp/feature-requests/239/ */
	{"GoPro:HERO6 Black",			0x2672, 0x0037, 0},
	/* Rasmus Larsson <larsson.rasmus@gmail.com> */
	{"GoPro:HERO7 White",			0x2672, 0x0042, 0},
	/* Marcus Meissner */
	{"GoPro:HERO7 Silver",			0x2672, 0x0043, 0},
	/* https://sourceforge.net/p/libmtp/feature-requests/284/ */
	{"GoPro:HERO7 Black",			0x2672, 0x0047, 0},
	/* https://sourceforge.net/p/libmtp/bugs/1858/ */
	{"GoPro:HERO8 Black",			0x2672, 0x0049, 0},
	{"GoPro:HERO9 Black",			0x2672, 0x004D, 0},
	/* https://github.com/libmtp/libmtp/issues/103 */
	{"GoPro:HERO10 Black",			0x2672, 0x0056, 0},
	/* https://github.com/libmtp/libmtp/issues/136 */
	{"GoPro:HERO11 Black",			0x2672, 0x0059, 0},
#endif
};

static struct {
	const char *model;
	unsigned long device_flags;
} ptpip_models[] = {
	{"PTP/IP Camera"	, PTP_CAP|PTP_CAP_PREVIEW},
	{"Ricoh Theta (WLAN)"	, PTP_CAP},
	{"Nikon DSLR (WLAN)"	, PTP_CAP|PTP_CAP_PREVIEW},
	{"Nikon 1 (WLAN)"	, PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon EOS (WLAN)"	, PTP_CAP|PTP_CAP_PREVIEW},
	{"Fuji X (WLAN)"	, PTP_CAP|PTP_CAP_PREVIEW},
};

#include "device-flags.h"
static struct {
	const char *vendor;
	unsigned short usb_vendor;
	const char *model;
	unsigned short usb_product;
	unsigned long flags;
} mtp_models[] = {
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#include "music-players.h"
#endif
};

static struct {
	uint16_t	format_code;
	uint16_t	vendor_code;
	const char *txt;
} object_formats[] = {
	{PTP_OFC_Undefined,		0, "application/x-unknown"},
	{PTP_OFC_Association,		0, "application/x-association"},
	{PTP_OFC_Script,		0, "application/x-script"},
	{PTP_OFC_Executable,		0, "application/octet-stream"},
	{PTP_OFC_Text,			0, GP_MIME_TXT},
	{PTP_OFC_HTML,			0, "text/html"},
	{PTP_OFC_DPOF,			0, "text/plain"},
	{PTP_OFC_AIFF,			0, "audio/x-aiff"},
	{PTP_OFC_WAV,			0, GP_MIME_WAV},
	{PTP_OFC_MP3,			0, "audio/mpeg"},
	{PTP_OFC_AVI,			0, GP_MIME_AVI},
	{PTP_OFC_MPEG,			0, "video/mpeg"},
	{PTP_OFC_ASF,			0, "video/x-ms-asf"},
	{PTP_OFC_QT,			0, "video/quicktime"},
	{PTP_OFC_EXIF_JPEG,		0, GP_MIME_JPEG},
	{PTP_OFC_TIFF_EP,		0, "image/x-tiffep"},
	{PTP_OFC_FlashPix,		0, "image/x-flashpix"},
	{PTP_OFC_BMP,			0, GP_MIME_BMP},
	{PTP_OFC_CIFF,			0, "image/x-ciff"},
	{PTP_OFC_Undefined_0x3806,	0, "application/x-unknown"},
	{PTP_OFC_GIF,			0, "image/gif"},
	{PTP_OFC_JFIF,			0, GP_MIME_JPEG},
	{PTP_OFC_PCD,			0, "image/x-pcd"},
	{PTP_OFC_PICT,			0, "image/x-pict"},
	{PTP_OFC_PNG,			0, GP_MIME_PNG},
	{PTP_OFC_Undefined_0x380C,	0, "application/x-unknown"},
	{PTP_OFC_TIFF,			0, GP_MIME_TIFF},
	{PTP_OFC_TIFF_IT,		0, "image/x-tiffit"},
	{PTP_OFC_JP2,			0, "image/x-jpeg2000bff"},
	{PTP_OFC_JPX,			0, "image/x-jpeg2000eff"},
	{PTP_OFC_DNG,			0, "image/x-adobe-dng"},
	{PTP_OFC_MTP_MP4,		0, "video/mp4"},

	{PTP_OFC_MTP_OGG,		PTP_VENDOR_MICROSOFT, "application/ogg"},
	{PTP_OFC_MTP_FLAC,		PTP_VENDOR_MICROSOFT, "audio/x-flac"},
	{PTP_OFC_MTP_MP2,		PTP_VENDOR_MICROSOFT, "video/mpeg"},
	{PTP_OFC_MTP_M4A,		PTP_VENDOR_MICROSOFT, "audio/x-m4a"},
	{PTP_OFC_MTP_3GP,		PTP_VENDOR_MICROSOFT, "audio/3gpp"},
	{PTP_OFC_MTP_WMV,		PTP_VENDOR_MICROSOFT, "video/x-wmv"},
	{PTP_OFC_MTP_WMA,		PTP_VENDOR_MICROSOFT, "audio/x-wma"},
	{PTP_OFC_MTP_WMV,		PTP_VENDOR_MICROSOFT, "video/x-ms-wmv"},
	{PTP_OFC_MTP_WMA,		PTP_VENDOR_MICROSOFT, "audio/x-ms-wma"},
	{PTP_OFC_MTP_AAC,		PTP_VENDOR_MICROSOFT, "audio/MP4A-LATM"},
	{PTP_OFC_MTP_XMLDocument,	PTP_VENDOR_MICROSOFT, "text/xml"},
	{PTP_OFC_MTP_MSWordDocument,	PTP_VENDOR_MICROSOFT, "application/msword"},
	{PTP_OFC_MTP_MSExcelSpreadsheetXLS, PTP_VENDOR_MICROSOFT, "vnd.ms-excel"},
	{PTP_OFC_MTP_MSPowerpointPresentationPPT, PTP_VENDOR_MICROSOFT, "vnd.ms-powerpoint"},
	{PTP_OFC_MTP_vCard2,		PTP_VENDOR_MICROSOFT, "text/directory"},
	{PTP_OFC_MTP_vCard3,		PTP_VENDOR_MICROSOFT, "text/directory"},
	{PTP_OFC_MTP_vCalendar1,	PTP_VENDOR_MICROSOFT, "text/calendar"},
	{PTP_OFC_MTP_vCalendar2,	PTP_VENDOR_MICROSOFT, "text/calendar"},
	{PTP_OFC_CANON_CRW,		PTP_VENDOR_CANON, "image/x-canon-cr2"},
	{PTP_OFC_CANON_CRW3,		PTP_VENDOR_CANON, "image/x-canon-cr2"},
	{PTP_OFC_CANON_CR3,		PTP_VENDOR_CANON, "image/x-canon-cr3"},
	{PTP_OFC_CANON_MOV,		PTP_VENDOR_CANON, "video/quicktime"},
	{PTP_OFC_CANON_CHDK_CRW,	PTP_VENDOR_CANON, "image/x-canon-cr2"},
	{PTP_OFC_SONY_RAW,		PTP_VENDOR_SONY, "image/x-sony-arw"},
	{0,				0, NULL}
};

static int
set_mimetype (CameraFile *file, uint16_t vendorcode, uint16_t ofc)
{
	int i;

	for (i = 0; object_formats[i].format_code; i++) {
		if (object_formats[i].vendor_code && /* 0 means generic */
		    (object_formats[i].vendor_code != vendorcode))
			continue;
		if (object_formats[i].format_code != ofc)
			continue;
		return gp_file_set_mime_type (file, object_formats[i].txt);
	}
	GP_LOG_D ("Failed to find mime type for %04x", ofc);
	return gp_file_set_mime_type (file, "application/x-unknown");
}

static void
strcpy_mime(char * dest, uint16_t vendor_code, uint16_t ofc) {
	int i;

	for (i = 0; object_formats[i].format_code; i++) {
		if (object_formats[i].vendor_code && /* 0 means generic */
		    (object_formats[i].vendor_code != vendor_code))
			continue;
		if (object_formats[i].format_code == ofc) {
			strcpy(dest, object_formats[i].txt);
			return;
		}
	}
	GP_LOG_D ("Failed to find mime type for %04x", ofc);
	strcpy(dest, "application/x-unknown");
}

static uint32_t
get_mimetype (Camera *camera, CameraFile *file, uint16_t vendor_code)
{
	int i;
	const char *mimetype;

	gp_file_get_mime_type (file, &mimetype);
	for (i = 0; object_formats[i].format_code; i++) {
		if (object_formats[i].vendor_code && /* 0 means generic */
		    (object_formats[i].vendor_code != vendor_code))
			continue;
		if (!strcmp(mimetype,object_formats[i].txt))
			return (object_formats[i].format_code);
	}
	GP_LOG_D ("Failed to find mime type for %s", mimetype);
	return (PTP_OFC_Undefined);
}

static void
#ifdef __GNUC__
__attribute__((__format__(printf,2,0)))
#endif
ptp_debug_func (void *data, const char *format, va_list args)
{
	gp_logv (GP_LOG_DEBUG, "ptp", format, args);
}

static void
#ifdef __GNUC__
__attribute__((__format__(printf,2,0)))
#endif
ptp_error_func (void *data, const char *format, va_list args)
{
	PTPData *ptp_data = data;
	char buf[2048];

	vsnprintf (buf, sizeof (buf), format, args);
	gp_context_error (ptp_data->context, "%s", buf);
}

static int
is_mtp_capable(Camera *camera) {
	PTPParams *params = &camera->pl->params;
	if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_MICROSOFT)
		return 1;
	/* these are also lowercase. */
	if (params->deviceinfo.Manufacturer && !strcmp(params->deviceinfo.Manufacturer,"motorola"))
		return 1;
	/*
	if (camera->pl->bugs & PTP_MTP)
		return 1;
	*/
	return 0;
}

int
camera_abilities (CameraAbilitiesList *list)
{
	unsigned int i;
	CameraAbilities a;

	for (i = 0; i < sizeof(models)/sizeof(models[0]); i++) {
		memset(&a, 0, sizeof(a));
		strcpy (a.model, models[i].model);
		a.status		= GP_DRIVER_STATUS_PRODUCTION;
		a.port			= GP_PORT_USB;
		a.speed[0]		= 0;
		a.usb_vendor		= models[i].usb_vendor;
		a.usb_product		= models[i].usb_product;
		a.device_type		= GP_DEVICE_STILL_CAMERA;
		a.operations		= GP_OPERATION_NONE;

		/* for now */
		if (models[i].device_flags & PTP_OLYMPUS_XML)
			a.status	= GP_DRIVER_STATUS_EXPERIMENTAL;

		if (models[i].device_flags & PTP_CAP) {
			a.operations |= GP_OPERATION_CAPTURE_IMAGE | GP_OPERATION_CONFIG;

			/* Only Nikon *D* and *Z* cameras for now -Marcus */
			if (	(models[i].usb_vendor == 0x4b0) &&
				(strchr(models[i].model,'D') || strchr(models[i].model,'Z'))
			)
				a.operations |= GP_OPERATION_TRIGGER_CAPTURE;
			/* Also enable trigger capture for EOS capture */
			if (	(models[i].usb_vendor == 0x4a9) &&
				(strstr(models[i].model,"EOS") || strstr(models[i].model,"Rebel"))
			)
				a.operations |= GP_OPERATION_TRIGGER_CAPTURE;
			/* Sony Alpha are also trigger capture capable */
			if (	models[i].usb_vendor == 0x54c)
				a.operations |= GP_OPERATION_TRIGGER_CAPTURE;

			/* Olympus test  trigger capture capable */
			if (	models[i].usb_vendor == 0x7b4)
				a.operations |= GP_OPERATION_TRIGGER_CAPTURE;
#if 0
			/* SX 100 IS ... works in sdram, not in card mode */
			if (	(models[i].usb_vendor == 0x4a9) &&
				(models[i].usb_product == 0x315e)
			)
				a.operations |= GP_OPERATION_TRIGGER_CAPTURE;
#endif
		}
		if (models[i].device_flags & PTP_CAP_PREVIEW)
			a.operations |= GP_OPERATION_CAPTURE_PREVIEW;
		a.file_operations	= GP_FILE_OPERATION_PREVIEW |
					GP_FILE_OPERATION_DELETE;
		a.folder_operations	= GP_FOLDER_OPERATION_PUT_FILE |
					GP_FOLDER_OPERATION_MAKE_DIR |
					GP_FOLDER_OPERATION_REMOVE_DIR;
		if (models[i].usb_vendor == 0x4b0) {
			/* make it clear nikons cannot upload */
			a.folder_operations	&= ~GP_FOLDER_OPERATION_PUT_FILE;
		}
		CR (gp_abilities_list_append (list, a));
	}
	for (i = 0; i < sizeof(mtp_models)/sizeof(mtp_models[0]); i++) {
		memset(&a, 0, sizeof(a));
		sprintf (a.model, "%s:%s", mtp_models[i].vendor, mtp_models[i].model);
		a.status		= GP_DRIVER_STATUS_PRODUCTION;
		a.port			= GP_PORT_USB;
		a.speed[0]		= 0;
		a.usb_vendor		= mtp_models[i].usb_vendor;
		a.usb_product		= mtp_models[i].usb_product;
		a.operations		= GP_OPERATION_NONE;
		a.device_type		= GP_DEVICE_AUDIO_PLAYER;
		a.file_operations	= GP_FILE_OPERATION_DELETE;
		a.folder_operations	= GP_FOLDER_OPERATION_PUT_FILE |
					GP_FOLDER_OPERATION_MAKE_DIR |
					GP_FOLDER_OPERATION_REMOVE_DIR;
		CR (gp_abilities_list_append (list, a));
	}

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "USB PTP Class Camera");
	a.status = GP_DRIVER_STATUS_TESTING;
	a.port   = GP_PORT_USB;
	a.speed[0] = 0;
	a.usb_class = 6;
	a.usb_subclass = 1;
	a.usb_protocol = 1;
	a.operations =	GP_OPERATION_CAPTURE_IMAGE | /*GP_OPERATION_TRIGGER_CAPTURE |*/
		        GP_OPERATION_CAPTURE_PREVIEW |
			GP_OPERATION_CONFIG;
	a.file_operations   = GP_FILE_OPERATION_PREVIEW|
				GP_FILE_OPERATION_DELETE;
	a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE
		| GP_FOLDER_OPERATION_MAKE_DIR |
		GP_FOLDER_OPERATION_REMOVE_DIR;
	a.device_type       = GP_DEVICE_STILL_CAMERA;
	CR (gp_abilities_list_append (list, a));
	memset(&a, 0, sizeof(a));
	strcpy(a.model, "MTP Device");
	a.status = GP_DRIVER_STATUS_TESTING;
	a.port   = GP_PORT_USB;
	a.speed[0] = 0;
	a.usb_class = 666;
	a.usb_subclass = -1;
	a.usb_protocol = -1;
	a.operations        = GP_OPERATION_NONE;
	a.file_operations   = GP_FILE_OPERATION_DELETE;
	a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE
		| GP_FOLDER_OPERATION_MAKE_DIR |
		GP_FOLDER_OPERATION_REMOVE_DIR;
	a.device_type       = GP_DEVICE_AUDIO_PLAYER;
	CR (gp_abilities_list_append (list, a));

	for (i = 0; i < sizeof(ptpip_models)/sizeof(ptpip_models[0]); i++) {
		memset(&a, 0, sizeof(a));
		strcpy(a.model, ptpip_models[i].model);
		a.status 		= GP_DRIVER_STATUS_TESTING;
		if (strstr(ptpip_models[i].model,"Fuji"))
			a.status 		= GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port   		= GP_PORT_PTPIP;
		a.operations 		= GP_OPERATION_CONFIG;
		if (ptpip_models[i].device_flags & PTP_CAP)
			a.operations 	|= GP_OPERATION_CAPTURE_IMAGE;
		if (ptpip_models[i].device_flags & PTP_CAP_PREVIEW)
			a.operations 	|= GP_OPERATION_CAPTURE_PREVIEW;
		a.file_operations   =	GP_FILE_OPERATION_PREVIEW	|
					GP_FILE_OPERATION_DELETE;
		a.folder_operations =	GP_FOLDER_OPERATION_PUT_FILE	|
					GP_FOLDER_OPERATION_MAKE_DIR	|
					GP_FOLDER_OPERATION_REMOVE_DIR;
		a.device_type       = GP_DEVICE_STILL_CAMERA;
		CR (gp_abilities_list_append (list, a));
	}

	return (GP_OK);
}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "PTP");

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	int exit_result = PTP_RC_OK;
	int exit_gp_result = GP_OK;
	if (camera->pl!=NULL) {
		PTPParams *params = &camera->pl->params;
		PTPContainer event;
		SET_CONTEXT_P(params, context);

		switch (params->deviceinfo.VendorExtensionID) {
		case PTP_VENDOR_CANON:
			/* Disable EOS capture now, also end viewfinder mode. */
			if (params->eos_captureenabled) {
				if (camera->pl->checkevents) {
					PTPCanon_changes_entry entry;

					if ((exit_result = ptp_check_eos_events (params)) != PTP_RC_OK)
						goto exitfailed;
					while (ptp_get_one_eos_event (params, &entry)) {
						GP_LOG_D ("missed EOS ptp type %d", entry.type);
						if (entry.type == PTP_CANON_EOS_CHANGES_TYPE_UNKNOWN)
							free (entry.u.info);
					}
					camera->pl->checkevents = 0;
				}
				if (params->inliveview && ptp_operation_issupported(params, PTP_OC_CANON_EOS_TerminateViewfinder))
					if ((exit_result = ptp_canon_eos_end_viewfinder (params)) != PTP_RC_OK)
						goto exitfailed;

				if ((exit_gp_result = camera_unprepare_capture (camera, context)) < GP_OK)	/* note: gets gphoto resultcodes, not ptp retcodes */
					goto exitfailed;
			}
			/* this switches the display back on ... */
			if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_SetRemoteMode)) {
				C_PTP (ptp_canon_eos_setremotemode(params, 1));
			}
			break;
		case PTP_VENDOR_NIKON:
			if (ptp_operation_issupported(params, PTP_OC_NIKON_EndLiveView))
				if ((exit_result = ptp_nikon_end_liveview (params)) != PTP_RC_OK)
					goto exitfailed;
			params->inliveview = 0;

			/* get the Nikon out of control mode again */
			if (params->controlmode && ptp_operation_issupported(params,PTP_OC_NIKON_ChangeCameraMode)) {
				ptp_nikon_changecameramode (params, 0);
				params->controlmode = 0;
			}
			break;
		case PTP_VENDOR_SONY:
#if 0
			/* if we call this, the camera shuts down on close in MTP mode */
			if (ptp_operation_issupported(params, 0x9280)) {
				C_PTP (ptp_sony_9280(params, 0x4,0,5,0,0,0,0));
			}
#endif
			break;
		case PTP_VENDOR_FUJI:
			CR (camera_unprepare_capture (camera, context));
			break;
		case PTP_VENDOR_GP_OLYMPUS_OMD: {
			PTPPropertyValue propval;

			propval.u16 = 0;
			CR (ptp_setdevicepropvalue (params, 0xD052, &propval, PTP_DTC_UINT16));
			break;
		}
		case PTP_VENDOR_GP_LEICA:
			if (ptp_operation_issupported(params, PTP_OC_LEICA_LECloseSession)) {
				if ((exit_result = ptp_leica_leclosesession (params)) != PTP_RC_OK)
					goto exitfailed;
			}
			break;
		}

		if (camera->pl->checkevents)
			ptp_check_event (params);
		while (ptp_get_one_event (params, &event))
			GP_LOG_D ("missed ptp event 0x%x (param1=%x)", event.Code, event.Param1);

		/* 2016 EOS cameras do not like that and report 0x2005 on all following opcodes */
		if (!DONT_CLOSE_SESSION(params)) {
			/* close ptp session */
			ptp_closesession (params);
		}
exitfailed:
		ptp_free_params(params);

#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
		/* close iconv converters */
		if (params->cd_ucs2_to_locale != (iconv_t)-1) iconv_close(params->cd_ucs2_to_locale);
		if (params->cd_locale_to_ucs2 != (iconv_t)-1) iconv_close(params->cd_locale_to_ucs2);
#endif

		if (camera->port->type == GP_PORT_PTPIP) {
			ptp_ptpip_disconnect (params);
		}

		free (params->data);
		free (camera->pl); /* also frees params */
		params = NULL;
		camera->pl = NULL;
	}
	/* This code hangs USB 3 devices after the first bulk image transmission.
         * For some unknown reason. */
	if (0 && (camera->port!=NULL) && (camera->port->type == GP_PORT_USB)) {
		/* clear halt */
		gp_port_usb_clear_halt
				(camera->port, GP_PORT_USB_ENDPOINT_IN);
		gp_port_usb_clear_halt
				(camera->port, GP_PORT_USB_ENDPOINT_OUT);
		gp_port_usb_clear_halt
				(camera->port, GP_PORT_USB_ENDPOINT_INT);
	}
#if defined(HAVE_LIBWS232) && defined(WIN32)
	else if ((camera->port!=NULL) && camera->port->type != GP_PORT_PTPIP) {
		WSACleanup();
	}
#endif
	if (exit_gp_result != GP_OK)
		return exit_gp_result;
	return translate_ptp_result(exit_result);
}

static int
camera_about (Camera *camera, CameraText *text, GPContext *context)
{
	/* Note that we are not a so called 'Licensed Implementation' of MTP
	 * ... (for a LI you need express approval from Microsoft etc.)
	 */
	snprintf (text->text, sizeof(text->text),
	 _("PTP2 driver\n"
	   "(c) 2001-2005 by Mariusz Woloszyn <emsi@ipartners.pl>.\n"
	   "(c) 2003-%d by Marcus Meissner <marcus@jet.franken.de>.\n"
	   "This driver supports cameras that support PTP or PictBridge(tm), and\n"
	   "Media Players that support the Media Transfer Protocol (MTP).\n"
	   "\n"
	   "Enjoy!"), 2021);
	return (GP_OK);
}

static void debug_objectinfo(PTPParams *params, uint32_t oid, PTPObjectInfo *oi);

/* Add new object to internal driver structures. issued when creating
 * folder, uploading objects, or captured images.
 */
static int get_folder_from_handle (Camera *camera, uint32_t storage, uint32_t handle, char *folder);
static int
add_object (Camera *camera, uint32_t handle, GPContext *context)
{
	PTPObject *ob;
	PTPParams *params = &camera->pl->params;

	C_PTP (ptp_object_want (params, handle, 0, &ob));
	return GP_OK;
}

static int
add_object_to_fs_and_path (Camera *camera, uint32_t handle, CameraFilePath *path, GPContext *context)
{
	PTPObject	*ob;
	PTPParams	*params = &camera->pl->params;
	CameraFileInfo	info;

	C_PTP (ptp_object_want (params, handle, PTPOBJECT_OBJECTINFO_LOADED, &ob));

	strcpy  (path->name,  ob->oi.Filename);
	sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)ob->oi.StorageID);

	get_folder_from_handle (camera, ob->oi.StorageID, ob->oi.ParentObject, path->folder);
	/* might invalidate ob, so refetch */
	C_PTP (ptp_object_want (params, handle, PTPOBJECT_OBJECTINFO_LOADED, &ob));
	/* delete last / or we get confused later. */
	path->folder[ strlen(path->folder)-1 ] = '\0';

	if (ob->oi.ObjectFormat == PTP_OFC_Association)
		/* FIXME: gp_filesystem_reset */
		return GP_OK;
	/* The gp_filesystem_append function only appends files */
	CR ( gp_filesystem_append (camera->fs, path->folder, path->name, context));

	/* fetch ob pointer again, as gp_filesystem_append can change the object list */
	C_PTP (ptp_object_want (params, handle, PTPOBJECT_OBJECTINFO_LOADED, &ob));

	memset (&info, 0, sizeof (info));
	/* we also get the fs info for free, so just set it */
	info.file.fields = GP_FILE_INFO_TYPE |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE | GP_FILE_INFO_MTIME;
	strcpy_mime (info.file.type, params->deviceinfo.VendorExtensionID, ob->oi.ObjectFormat);
	info.file.width		= ob->oi.ImagePixWidth;
	info.file.height	= ob->oi.ImagePixHeight;
	info.file.size		= ob->oi.ObjectCompressedSize;
	info.file.mtime		= time(NULL);

	info.preview.fields = GP_FILE_INFO_TYPE |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE;
	strcpy_mime (info.preview.type, params->deviceinfo.VendorExtensionID, ob->oi.ThumbFormat);
	info.preview.width	= ob->oi.ThumbPixWidth;
	info.preview.height	= ob->oi.ThumbPixHeight;
	info.preview.size	= ob->oi.ThumbCompressedSize;
	GP_LOG_D ("setting fileinfo in fs");
	return gp_filesystem_set_info_noop(camera->fs, path->folder, path->name, info, context);
}

/* find JPEGs in data blobs helper ... as most preview data is encapsulated */
static int
save_jpeg_in_data_to_preview(const unsigned char *data, unsigned long size, CameraFile *file)
{
	unsigned char *startptr = NULL;
	unsigned char *endptr = NULL;

	/* look for the JPEG SOI marker (0xFFD8) in data */
	startptr = (unsigned char*)memchr(data, 0xff, size);
	while(startptr && ((startptr+1) < (data + size))) {
		if(*(startptr + 1) == 0xd8) { /* SOI found */
			break;
		} else { /* go on looking (starting at next byte) */
			startptr++;
			startptr = (unsigned char*)memchr(startptr, 0xff, size - (startptr - data));
		}
	}
	if(!startptr)	/* no SOI -> no JPEG */
		return GP_ERROR;

	endptr = (unsigned char*)memchr(startptr+1, 0xff, size-(startptr-data)-1);
	while(endptr && ((endptr+1) < (data + size))) {
		if(*(endptr + 1) == 0xd9) { /* EOI found */
			endptr += 2;
			break;
		} else { /* go on looking (starting at next byte) */
			endptr++;
			endptr = (unsigned char*)memchr(endptr, 0xff, size - (endptr-data));
		}
	}
	if(!endptr) /* no SOI -> no JPEG */
		return GP_ERROR;

	gp_file_append (file, (char*)startptr, endptr - startptr);
	gp_file_set_mime_type (file, GP_MIME_JPEG);
	gp_file_set_name (file, "preview.jpg");
	gp_file_set_mtime (file, time(NULL));
	return GP_OK;
}


static int
camera_capture_stream_preview (Camera *camera, CameraFile *file, GPContext *context) {
	PTPParams		*params = &camera->pl->params;
	PTPPropertyValue	propval;
	PTPStreamInfo		sinfo;
	unsigned char		*data;
	unsigned int		size;

	C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_EnabledStreams, &propval, PTP_DTC_UINT32));
	if (!(propval.u32 & 1)) {	/* video enabled already ? */
		propval.u32 = 1;
		C_PTP (ptp_setdevicepropvalue (params, PTP_DPC_EnabledStreams, &propval, PTP_DTC_UINT32));
	}
	C_PTP (ptp_getstreaminfo (params, 1, &sinfo));

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_GP_LEICA) &&
		(ptp_operation_issupported(params, PTP_OC_LEICA_LEGetStreamData))
	) {
		/* ptp header + header before ff d8
		0000  58 1c 00 00 02 00 25 90-0b 00 00 00 02 01 00 00  X.....%.........
		0010  46 1c 00 00 00 00 3e 1c-00 00 00 00 b4 00 00 00  F.....>.........
		0020  01 00 00 00 04 00 00 00-d4 30 00 00 02 00 00 00  .........0......
		0030  04 00 00 00 d4 30 00 00-03 00 00 00 04 00 00 00  .....0..........
		0040  53 06 00 00 04 00 00 00-40 00 00 00 00 00 00 00  S.......@.......
		0050  00 00 00 00 00 00 00 00-00 00 00 00 70 17 00 00  ............p...
		0060  a0 0f 00 00 00 00 00 00-00 00 00 00 70 17 00 00  ............p...
		0070  a0 0f 00 00 00 00 00 00-00 00 00 00 70 17 00 00  ............p...
		0080  a0 0f 00 00 fe ff ff ff-fe ff ff ff 05 00 00 00  ................
		0090  04 00 00 00 00 00 00 00-06 00 00 00 10 00 00 00  ................
		00a0  00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  ................
		00b0  07 00 00 00 04 00 00 00-00 00 00 00 00 00 00 00  ................
		00c0  ff d8
		*/

		C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_VideoFormat, &propval, PTP_DTC_UINT32));

		C_PTP (ptp_leica_getstreamdata (params, &data, &size));
		if (propval.u32 == 0x47504a4d) { /* MJPG */
			int		ret;

			ret = save_jpeg_in_data_to_preview (data, size, file);
			if(ret < GP_OK) { /* no SOI -> no JPEG */
				free (data);
				gp_context_error (context, _("Sorry, your Nikon camera does not seem to return a JPEG image in LiveView mode"));
				return GP_ERROR;
			}
		} else {
			gp_file_append (file, (char*)data, size);
		}
		gp_file_set_mtime (file, time(NULL));
		free (data);
		SET_CONTEXT_P(params, NULL);
		return GP_OK;
	}
	if (ptp_operation_issupported(params, PTP_OC_GetStream)) {
		C_PTP (ptp_getstream (params, &data, &size));
		gp_file_append (file, (char*)data, size);
		free (data);
		SET_CONTEXT_P(params, NULL);
		return GP_OK;
	}
	SET_CONTEXT_P(params, NULL);
	return GP_ERROR_NOT_SUPPORTED;
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	unsigned char	*data = NULL, *jpgStartPtr = NULL;
	uint32_t	size = 0;
	uint16_t	ret;
	PTPParams *params = &camera->pl->params;

	camera->pl->checkevents = TRUE;
	switch (params->deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_CANON:
		/* Canon PowerShot / IXUS preview mode */
		if (ptp_operation_issupported(params, PTP_OC_CANON_ViewfinderOn)) {
			SET_CONTEXT_P(params, context);
			/* check if we need to prepare capture */
			if (!params->canon_event_mode)
				CR (camera_prepare_capture (camera, context));
			if (!params->canon_viewfinder_on) { /* enable on demand, but just once */
				C_PTP_REP_MSG (ptp_canon_viewfinderon (params),
					       _("Canon enable viewfinder failed"));
				params->canon_viewfinder_on = 1;
			}
			C_PTP_REP_MSG (ptp_canon_getviewfinderimage (params, &data, &size),
				       _("Canon get viewfinder image failed"));
			gp_file_append ( file, (char*)data, size );
			free (data);
			gp_file_set_mime_type (file, GP_MIME_JPEG);     /* always */
			/* Add an arbitrary file name so caller won't crash */
			gp_file_set_name (file, "canon_preview.jpg");
			gp_file_set_mtime (file, time(NULL));
			SET_CONTEXT_P(params, NULL);
			return GP_OK;
		}
		/* Canon EOS DSLR preview mode */
		if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_GetViewFinderData)) {
			PTPPropertyValue	val;
			/* FIXME: this might cause a focusing pass and take seconds. 20 was not
			 * enough (would be 0.2 seconds, too short for the mirror up operation.). */
			/* The EOS 100D takes 1.2 seconds */
			PTPDevicePropDesc       dpd;
			int			try = 0;
			struct timeval		event_start;

			SET_CONTEXT_P(params, context);

			if (!params->eos_captureenabled)
				camera_prepare_capture (camera, context);
			memset (&dpd,0,sizeof(dpd));

			/* do not set it everytime, it will cause delays */
			ret = ptp_canon_eos_getdevicepropdesc (params, PTP_DPC_CANON_EOS_EVFMode, &dpd);
			if ((ret == PTP_RC_OK) && (dpd.CurrentValue.u16 != 1)) {
				/* 0 means off, 1 means on */
				val.u16 = 1;
				ret = ptp_canon_eos_setdevicepropvalue (params, PTP_DPC_CANON_EOS_EVFMode, &val, PTP_DTC_UINT16);
				/* in movie mode we get busy, but can proceed */
				if ((ret != PTP_RC_OK) && (ret != PTP_RC_DeviceBusy))
					C_PTP_MSG (ret, "setval of evf enable to 1 failed (curval is %d)!", dpd.CurrentValue.u16);
			}
			ptp_free_devicepropdesc (&dpd);
			/* do not set it everytime, it will cause delays */
			ret = ptp_canon_eos_getdevicepropdesc (params, PTP_DPC_CANON_EOS_EVFOutputDevice, &dpd);
			/* see config.c what kind of values we have ... it seems to be a mask. bit 0 is TFT, bit 1 PC, bit 2 MOBILE, bit 3 MOBILE2? */
			/* so lets see it only if it does not have any bit set (discounted bit 0) */
			if ((ret == PTP_RC_OK) && ((dpd.CurrentValue.u32 & ~1) == 0)) {
				/* 2 means PC, 1 means TFT */
				val.u32 = 2;
				C_PTP_MSG (ptp_canon_eos_setdevicepropvalue (params, PTP_DPC_CANON_EOS_EVFOutputDevice, &val, PTP_DTC_UINT32),
					   "setval of evf outputmode to 2 failed (curval is %d)!", dpd.CurrentValue.u32);
			}
			ptp_free_devicepropdesc (&dpd);

			/* Otherwise the camera will auto-shutdown */
			if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_KeepDeviceOn)) C_PTP (ptp_canon_eos_keepdeviceon (params));

			params->inliveview = 1;
			event_start = time_now();
			do {
				unsigned char	*xdata;
				/* Poll for camera events, but just call
				 * it once and do not drain the queue now */
				C_PTP (ptp_check_eos_events (params));

				ret = ptp_canon_eos_get_viewfinder_image (params , &data, &size);
				if ((ret == 0xa102) || (ret == PTP_RC_DeviceBusy)) { /* means "not there yet" ... so wait */
					/* wait 3 seconds at most ... use a bit of backoff logic for cameras where we should not drain compute. */
					usleep((++try)*5*1000);
					if (time_since (event_start) < 3*1000)
						continue;
/*
					if (waiting_for_timeout (&back_off_wait, event_start, 3*1000))
						continue;
*/
				}
				C_PTP_MSG (ret, "get_viewfinder_image failed");

				/* returns multiple blobs, they are usually structured as
				 * uint32 len
				 * uint32 type
				 * ... data ...
				 *
				 * 1: JPEG preview
				 */

				xdata = data;
				GP_LOG_D ("total size: len=%d", size);
				while ((xdata-data) < size) {
					uint32_t	len  = dtoh32a(xdata);
					uint32_t	type = dtoh32a(xdata+4);

					/* 4 byte len of jpeg data, 4 byte type */
					/* JPEG blob */
					/* stuff */
					GP_LOG_D ("get_viewfinder_image header: len=%d type=%d", len, type);
					switch (type) {
					default:
						if (len > (size-(xdata-data))) {
							len = size;
							GP_LOG_E ("len=%d larger than rest size %ld", len, (size-(xdata-data)));
						}
						GP_LOG_DATA ((char*)xdata, len, "get_viewfinder_image header:");
						xdata = xdata+len;
						continue;
					case 9:
					case 1:
					case 11:
						if (len > (size-(xdata-data))) {
							len = size;
							GP_LOG_E ("len=%d larger than rest size %ld", len, (size-(xdata-data)));
							break;
						}
						gp_file_append ( file, (char*)xdata+8, len-8 );
						/* type 1 is JPEG (regular), type 9 is in movie mode */

						gp_file_set_mime_type (file, ((type == 1) || (type == 11)) ? GP_MIME_JPEG : GP_MIME_RAW);

						/* Add an arbitrary file name so caller won't crash */
						gp_file_set_name (file, "preview.jpg");

						/* dump the rest of the blobs */
						xdata = xdata+len;
						while ((xdata-data) < size) {
							len  = dtoh32a(xdata);
							type = dtoh32a(xdata+4);

							if (len > (size-(xdata-data))) {
								len = size;
								GP_LOG_E ("len=%d larger than rest size %ld", len, (size-(xdata-data)));
								break;
							}
							GP_LOG_D ("get_viewfinder_image header: len=%d type=%d", len, type);
							GP_LOG_DATA ((char*)xdata, len, "get_viewfinder_image header:");
							xdata = xdata+len;
						}
						free (data);
						SET_CONTEXT_P(params, NULL);
						return GP_OK;
					}
				}
				return GP_ERROR;
			} while (1);
			GP_LOG_E ("get_viewfinder_image failed after all tries with ret: 0x%x\n", ret);
			SET_CONTEXT_P(params, NULL);
			return translate_ptp_result (ret);
		}
		gp_context_error (context, _("Sorry, your Canon camera does not support Canon Viewfinder mode"));
		return GP_ERROR_NOT_SUPPORTED;
	case PTP_VENDOR_NIKON: {
		PTPPropertyValue	value;
		int 			tries, firstimage = 0;

		if (!ptp_operation_issupported(params, PTP_OC_NIKON_StartLiveView)) {
			gp_context_error (context,
				_("Sorry, your Nikon camera does not support LiveView mode"));
			return GP_ERROR_NOT_SUPPORTED;
		}
		SET_CONTEXT_P(params, context);

		/* Nilon V and J seem to like that */
		if (!params->controlmode && ptp_operation_issupported(params,PTP_OC_NIKON_ChangeCameraMode)) {
			ret = ptp_nikon_changecameramode (params, 1);
			/* FIXME: PTP_RC_NIKON_ChangeCameraModeFailed does not seem to be problematic */
			if (ret != PTP_RC_NIKON_ChangeCameraModeFailed)
				C_PTP_REP (ret);
			params->controlmode = 1;
		}

		ret = ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewStatus, &value, PTP_DTC_UINT8);
		if (ret != PTP_RC_OK)
			value.u8 = 0;

enable_liveview:
		if (!value.u8) {
			value.u8 = 1;
			if (have_prop(camera, params->deviceinfo.VendorExtensionID, PTP_DPC_NIKON_RecordingMedia))
				LOG_ON_PTP_E (ptp_setdevicepropvalue (params, PTP_DPC_NIKON_RecordingMedia, &value, PTP_DTC_UINT8));

			if (have_prop(camera, params->deviceinfo.VendorExtensionID, PTP_DPC_NIKON_LiveViewProhibitCondition)) {
				PTPPropertyValue	cond;

				C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewProhibitCondition, &cond, PTP_DTC_UINT32));

				if (cond.u32) {
					/* we could have multiple reasons, but just report the first one. by decreasing order of possibility */
					if (cond.u32 & (1<<8)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Battery exhausted")); return GP_ERROR; }
					if (cond.u32 & (1<<17)){ gp_context_error (context, _("Liveview cannot start: %s"),_("Temperature too high")); return GP_ERROR; }
					if (cond.u32 & (1<<9)) { gp_context_error (context, _("Liveview cannot start: %s"),_("TTL error")); return GP_ERROR; }
					if (cond.u32 & (1<<22)){ gp_context_error (context, _("Liveview cannot start: %s"),_("In Mirror-up operation")); return GP_ERROR; }
					if (cond.u32 & (1<<5)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Minimum aperture warning")); return GP_ERROR; }
					if (cond.u32 & (1<<15)){ gp_context_error (context, _("Liveview cannot start: %s"),_("Processing of shooting operation")); return GP_ERROR; }
					if (cond.u32 & (1<<2)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Sequence error")); return GP_ERROR; }
					if (cond.u32 & (1<<31)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Exposure Program Mode is not P/A/S/M")); return GP_ERROR; }
					if (cond.u32 & (1<<21)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Bulb warning")); return GP_ERROR; }
					if (cond.u32 & (1<<20)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Card unformatted")); return GP_ERROR; }
					if (cond.u32 & (1<<19)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Card error")); return GP_ERROR; }
					if (cond.u32 & (1<<18)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Card protected")); return GP_ERROR; }
					if (cond.u32 & (1<<14)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Recording destination card, but no card or card protected")); return GP_ERROR; }
					if (cond.u32 & (1<<12)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Pending unretrieved SDRAM image")); return GP_ERROR; }
					if (cond.u32 & (1<<12)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Pending unretrieved SDRAM image")); return GP_ERROR; }
					if (cond.u32 & (1<<4)) { gp_context_error (context, _("Liveview cannot start: %s"),_("Fully pressed button")); return GP_ERROR; }

					if (cond.u32 & (1<<24)){ gp_context_error (context, _("Liveview cannot start: %s"),_("Lens is retracting")); goto ignoreerror; }
					gp_context_error (context, _("Liveview cannot start: code 0x%08x"), cond.u32);
					return GP_ERROR;
				}
			}

ignoreerror:
			ret = ptp_nikon_start_liveview (params);
			if ((ret != PTP_RC_OK) && (ret != PTP_RC_DeviceBusy))
				C_PTP_REP_MSG (ret, _("Nikon enable liveview failed"));

			/* wait up to 1 second */
			C_PTP_REP_MSG (nikon_wait_busy(params,20,2000), _("Nikon enable liveview failed"));
			params->inliveview = 1;
			firstimage = 1;
		}
		/* nikon 1 special */
		if (value.u8 && !params->inliveview) {
			ret = ptp_nikon_start_liveview (params);
			if ((ret != PTP_RC_OK) && (ret != PTP_RC_DeviceBusy))
				C_PTP_REP_MSG (ret, _("Nikon enable liveview failed"));

			C_PTP_REP_MSG (nikon_wait_busy(params,20,2000), _("Nikon enable liveview failed"));
			params->inliveview = 1;
		}
		tries = 20;
		while (tries--) {
			ret = ptp_nikon_get_liveview_image (params , &data, &size);
			if (ret == PTP_RC_NIKON_NotLiveView) {
				/* this happens on the D7000 after 14000 frames... reenable liveview */
				params->inliveview = 0;
				value.u8 = 0;
				goto enable_liveview;
			}
			if (ret == PTP_RC_OK) {
				int res;

				if (firstimage) {
					/* the first image on the S9700 is corrupted. so just skip the first image */
					firstimage = 0;
					free (data);
					continue;
				}
				/* look for the JPEG SOI marker (0xFFD8) in data */
				res = save_jpeg_in_data_to_preview (data, size, file);
				free (data); /* FIXME: perhaps handle the 128 byte header data too. */
				if(res < GP_OK) { /* no SOI -> no JPEG */
					gp_context_error (context, _("Sorry, your Nikon camera does not seem to return a JPEG image in LiveView mode"));
					return GP_ERROR;
				}
				break;
			}
			if (ret == PTP_RC_DeviceBusy) {
				GP_LOG_D ("busy, retrying after a bit of wait, try %d", tries);
				usleep(10*1000);
				continue;
			}
			SET_CONTEXT_P(params, NULL);
			return translate_ptp_result (ret);
		}
#if 0
		C_PTP_REP_MSG (ptp_nikon_end_liveview (params),
			       _("Nikon disable liveview failed"));
#endif
		SET_CONTEXT_P(params, NULL);
		return GP_OK;
	}
	case PTP_VENDOR_SONY: {
		/* same for Alpha series and QX */
		uint32_t	preview_object = 0xffffc002; /* this is where the liveview image is accessed */
		unsigned char	*ximage = NULL;
		int		tries = 50;
		int		res, oldtimeout;

#if 0
		/* this times out, with 0.3 seconds wait ... bad */
		ptp_check_event (params); 	/* will stall for some reason */
#endif
		CR (gp_port_get_timeout (camera->port, &oldtimeout));
		CR (gp_port_set_timeout (camera->port, 1*1000));
		do {
			PTPObjectInfo oi;

			/* This state can persist for up to 1 second on the ZV-1 */
			ret = ptp_getobjectinfo(params, preview_object, &oi);
			if (ret == PTP_RC_InvalidObjectHandle) {
				usleep(50*1000);
				continue;
			}
			if (ret == PTP_RC_OK)
				ptp_free_objectinfo(&oi);

			ret = ptp_getobject_with_size(params, preview_object, &ximage, &size);
			if (ret == PTP_RC_OK)
				break;
			if (ret != PTP_RC_AccessDenied) /* we get those when we are too fast */
				C_PTP (ret);
			usleep(20*1000);
		} while (tries--);
		CR (gp_port_set_timeout (camera->port, oldtimeout));

		jpgStartPtr = ximage;
		/* There is an initial blob, and we had a case where 0xff 0xd8 was in the initial blob
		 * https://github.com/gphoto/gphoto2/issues/389
		 * as the data starts with an apparent offset into the data to the JPEG, try to use that
		 */
		if (size > 4) {
			unsigned int offset = ximage[0] | (ximage[1] << 8) | (ximage[2] << 16) | (ximage[3] << 24);
			if ((offset+1 < size) && (ximage[offset] == 0xff) && (ximage[offset+1] == 0xd8))
				jpgStartPtr = ximage + offset;
		}

		res = save_jpeg_in_data_to_preview(jpgStartPtr, size - (jpgStartPtr-ximage), file);
		free (ximage); /* FIXME: perhaps handle the 128 byte header data too. */

		if(res < GP_OK) { /* no SOI -> no JPEG */
			gp_context_error (context, _("Sorry, your Sony camera does not seem to return a JPEG image in LiveView mode"));
			return GP_ERROR;
		}
		SET_CONTEXT_P(params, NULL);
		return GP_OK;
	}
	case PTP_VENDOR_FUJI: {
		PTPObjectInfo	oi;
		uint32_t	preview_object = 0x80000001; /* this is where the liveview image is accessed */
		unsigned char	*ximage = NULL;
		int		tries = 10;

		if (params->jpgfd) {
			int res;

			C_PTP (ptp_fujiptpip_jpeg (params, &ximage, &size));

			/* there is a bit of header in front ... skip it */
			res = save_jpeg_in_data_to_preview(ximage, size, file);
			free (ximage);

			SET_CONTEXT_P(params, NULL);
			return res;
		}

		while (tries--) {
			ret = ptp_getobjectinfo (params, preview_object, &oi);
			if (ret == PTP_RC_OK) {
				ptp_free_objectinfo(&oi);
				break;
			}
			if (ret == PTP_RC_InvalidObjectHandle) {
				/* 1000 x 10 tries was not enough for the S10 ... make the wait a bit longer
				 * see https://github.com/gphoto/libgphoto2/issues/603 */
				usleep(5*1000);
				continue;
			}
			C_PTP_REP (ret);
		}

		if(ret != PTP_RC_OK) {
			tries = 5;
			while (tries--) {
				ret = ptp_initiateopencapture(params, 0x00000000, 0x00000000);
				if (ret == PTP_RC_OK) {
					params->opencapture_transid = params->transaction_id-1;
					params->inliveview = 1;
					usleep(100*1000); /* this basically waits until the first object is there. */
					break;
				}
				usleep(200*1000);
			}
		}

		tries = 20;
		do {
			ret = ptp_getobject_with_size(params, preview_object, &ximage, &size);
			if (ret == PTP_RC_OK)
				break;
			if(ret == PTP_RC_DeviceBusy) {
				usleep(1000);
				continue;
			}

			if (ret != PTP_RC_AccessDenied) /* we get those when we are too fast */ /* priobably pasted from nikon, check? */
				C_PTP (ret);
		} while (tries--);
		C_PTP_REP (ptp_deleteobject(params, preview_object, 0));

		/* Fuji Liveview returns FF D8 ... FF D9 ... so no meta data wrapped around the jpeg data */
		gp_file_append (file, (char*)ximage, size);
		free (ximage);

		gp_file_set_mime_type (file, GP_MIME_JPEG);
		gp_file_set_name (file, "preview.jpg");
		gp_file_set_mtime (file, time(NULL));

		SET_CONTEXT_P(params, NULL);
		return GP_OK;
	}
	case PTP_VENDOR_PANASONIC: {
		unsigned char	*ximage = NULL;
		int		tries = 25;
		int		res;

		if(!params->inliveview) {
			C_PTP_REP(ptp_panasonic_liveview(params, 1));
			params->inliveview = 1;
			usleep(100000);
		}

		for (;;) {
			tries--;
			if(tries <= 0)
				return translate_ptp_result (ret);
			ret = ptp_panasonic_liveview_image (params, &ximage, &size);
			if(ret == PTP_RC_DeviceBusy) {
				usleep(40000);
				continue;
			} else {
				break;
			}
		}
		/* look for the JPEG SOI marker (0xFFD8) in data */
		res = save_jpeg_in_data_to_preview (ximage, size, file);
		free (ximage); /* FIXME: perhaps handle the 128 byte header data too. */
		if(res < GP_OK) { /* no SOI -> no JPEG */
			gp_context_error (context, _("Sorry, your Panasonic camera does not seem to return a JPEG image in LiveView mode"));
			return GP_ERROR;
		}
		SET_CONTEXT_P(params, NULL);
		return GP_OK;
	}
	case PTP_VENDOR_GP_OLYMPUS_OMD: {
		unsigned char		*ximage = NULL;
		PTPPropertyValue	value;
		int			tries = 25;

		ret = ptp_getdevicepropvalue (params, PTP_DPC_OLYMPUS_LiveViewModeOM, &value, PTP_DTC_UINT32);
		if (ret != PTP_RC_OK)
			value.u32 = 0;

		if (value.u32 != 67109632) {	/* 0x04000300 */
			value.u32 = 67109632;
			LOG_ON_PTP_E (ptp_setdevicepropvalue (params, PTP_DPC_OLYMPUS_LiveViewModeOM, &value, PTP_DTC_UINT32));

			params->inliveview = 1;
		}

		for(;;) {
			tries--;
			if(tries <= 0) {
				return ret;
			}
			ret = ptp_olympus_liveview_image (params, &ximage, &size);
			if(ret == PTP_RC_DeviceBusy || size < 1024) {
				usleep(40000);
				continue;
			} else {
				break;
			}
		}

		gp_file_append (file, (char*)ximage, size);
		free (ximage);

		gp_file_set_mime_type (file, GP_MIME_JPEG);
		gp_file_set_name (file, "preview.jpg");
		gp_file_set_mtime (file, time(NULL));

		SET_CONTEXT_P(params, NULL);
		return GP_OK;
	}

	case PTP_VENDOR_GP_SIGMAFP: {
		unsigned char	*ximage = NULL;
		int		res;

		size = 0;

		C_PTP (ptp_sigma_fp_liveview_image (params, &ximage, &size));
		if (size < 4) return GP_ERROR;

		/* look for the JPEG SOI marker (0xFFD8) in data */
		res = save_jpeg_in_data_to_preview (ximage, size, file);
		free (ximage);
		if(res < GP_OK) { /* no SOI -> no JPEG */
			gp_context_error (context, _("Sorry, your Panasonic camera does not seem to return a JPEG image in LiveView mode"));
			return GP_ERROR;
		}
		SET_CONTEXT_P(params, NULL);
		return GP_OK;
	}
	default:
		break;
	}
	/* Check if we can do PTP 1.1 stream method */
	if (	ptp_operation_issupported(params,PTP_OC_GetStreamInfo) &&
		ptp_property_issupported(params, PTP_DPC_SupportedStreams)
	)  {
		PTPPropertyValue propval;

		C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_SupportedStreams, &propval, PTP_DTC_UINT32));
		if (propval.u32 & 1) /* camera does Video streams */
			return camera_capture_stream_preview (camera, file, context);
		/* fallthrough */
	}
	return GP_ERROR_NOT_SUPPORTED;
}

static int
get_folder_from_handle (Camera *camera, uint32_t storage, uint32_t handle, char *folder) {
	PTPObject	*ob;
	PTPParams 	*params = &camera->pl->params;

	GP_LOG_D ("(%x,%x,%s)", storage, handle, folder);
	if (handle == PTP_HANDLER_ROOT)
		return GP_OK;

	C_PTP (ptp_object_want (params, handle, PTPOBJECT_OBJECTINFO_LOADED, &ob));
	CR (get_folder_from_handle (camera, storage, ob->oi.ParentObject, folder));
	/* now ob could be invalid, since we might have reallocated params->objects */
	ptp_object_want (params, handle, PTPOBJECT_OBJECTINFO_LOADED, &ob);
	strcat (folder, ob->oi.Filename);
	strcat (folder, "/");
	return (GP_OK);
}

static int
add_objectid_and_upload (Camera *camera, CameraFilePath *path, GPContext *context,
	uint32_t newobject, PTPObjectInfo *oi) {
	int			ret;
	PTPParams		*params = &camera->pl->params;
	CameraFile		*file = NULL;
	unsigned char		*ximage = NULL;
	CameraFileInfo		info;

	ret = gp_file_new(&file);
	if (ret!=GP_OK) return ret;
	gp_file_set_mtime (file, time(NULL));
	set_mimetype (file, params->deviceinfo.VendorExtensionID, oi->ObjectFormat);
	C_PTP_REP (ptp_getobject(params, newobject, &ximage));

	GP_LOG_D ("setting size");
	ret = gp_file_set_data_and_size(file, (char*)ximage, oi->ObjectCompressedSize);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	GP_LOG_D ("append to fs");
	ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
        if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	GP_LOG_D ("adding filedata to fs");
	ret = gp_filesystem_set_file_noop(camera->fs, path->folder, path->name, GP_FILE_TYPE_NORMAL, file, context);
        if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}

	/* We have now handed over the file, disclaim responsibility by unref. */
	gp_file_unref (file);

	memset (&info, 0, sizeof (info));
	/* we also get the fs info for free, so just set it */
	info.file.fields = GP_FILE_INFO_TYPE |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE | GP_FILE_INFO_MTIME;
	strcpy_mime (info.file.type, params->deviceinfo.VendorExtensionID, oi->ObjectFormat);
	info.file.width		= oi->ImagePixWidth;
	info.file.height	= oi->ImagePixHeight;
	info.file.size		= oi->ObjectCompressedSize;
	info.file.mtime		= time(NULL);

	info.preview.fields = GP_FILE_INFO_TYPE |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE;
	strcpy_mime (info.preview.type, params->deviceinfo.VendorExtensionID, oi->ThumbFormat);
	info.preview.width	= oi->ThumbPixWidth;
	info.preview.height	= oi->ThumbPixHeight;
	info.preview.size	= oi->ThumbCompressedSize;
	GP_LOG_D ("setting fileinfo in fs");
	return gp_filesystem_set_info_noop(camera->fs, path->folder, path->name, info, context);
}

/**
 * camera_nikon_capture:
 * params:      Camera*			- camera object
 *              CameraCaptureType type	- type of object to capture
 *              CameraFilePath *path    - filled out with filename and folder on return
 *              uint32_t af             - use autofocus or not.
 *              uint32_t sdram          - capture to sdram or not
 *              GPContext* context      - gphoto context for this operation
 *
 * This function captures an image using special Nikon capture to SDRAM.
 * The object(s) do(es) not appear in the "objecthandles" array returned by GetObjectHandles,
 * so we need to download them here immediately.
 *
 * Return values: A gphoto return code.
 * Upon success CameraFilePath *path contains the folder and filename of the captured
 * image.
 */
static int
camera_nikon_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		uint32_t af, int sdram, GPContext *context)
{
	PTPObjectInfo		oi;
	PTPParams		*params = &camera->pl->params;
	PTPDevicePropDesc	propdesc;
	PTPPropertyValue	propval;
	int			i, ret, burstnumber = 1, done, tries;
	uint32_t		newobject;
	int			back_off_wait = 0;
	struct timeval          capture_start;
	int			loops;
	PTPContainer		*storedevents = NULL;
	unsigned int		nrstoredevents = 0;
	PTPContainer		event;


	if (type != GP_CAPTURE_IMAGE)
		return GP_ERROR_NOT_SUPPORTED;

	if (params->deviceinfo.VendorExtensionID!=PTP_VENDOR_NIKON)
		return GP_ERROR_NOT_SUPPORTED;

	/* Nilon V and J seem to like that */
	if (!params->controlmode && ptp_operation_issupported(params,PTP_OC_NIKON_ChangeCameraMode)) {
		ret = ptp_nikon_changecameramode (params, 1);
		/* FIXME: PTP_RC_NIKON_ChangeCameraModeFailed does not seem to be problematic */
		if (ret != PTP_RC_NIKON_ChangeCameraModeFailed)
			C_PTP_REP (ret);
		params->controlmode = 1;
	}

	if (	!ptp_operation_issupported(params,PTP_OC_NIKON_InitiateCaptureRecInSdram) &&
		!ptp_operation_issupported(params,PTP_OC_NIKON_AfCaptureSDRAM) &&
		!ptp_operation_issupported(params,PTP_OC_NIKON_InitiateCaptureRecInMedia)
	) {
		gp_context_error(context,
               	_("Sorry, your camera does not support Nikon capture"));
		return GP_ERROR_NOT_SUPPORTED;
	}

	if (	ptp_property_issupported(params, PTP_DPC_StillCaptureMode)	&&
		(PTP_RC_OK == ptp_getdevicepropdesc (params, PTP_DPC_StillCaptureMode, &propdesc))) {
		PTPDevicePropDesc       burstdesc;

		if ((propdesc.DataType == PTP_DTC_UINT16)			&&
		    (propdesc.CurrentValue.u16 == 2) /* Burst Mode */		&&
		    ptp_property_issupported(params, PTP_DPC_BurstNumber)	&&
		    (PTP_RC_OK == ptp_getdevicepropdesc (params, PTP_DPC_BurstNumber, &burstdesc))) {
			if (burstdesc.DataType == PTP_DTC_UINT16) {
				burstnumber = burstdesc.CurrentValue.u16;
				GP_LOG_D ("burstnumber %d", burstnumber);
			}
			ptp_free_devicepropdesc (&burstdesc);
		    }
		ptp_free_devicepropdesc (&propdesc);
	}

	/* if in liveview mode, we have to run non-af capture */
	params->inliveview = 0;
	if (ptp_property_issupported (params, PTP_DPC_NIKON_LiveViewStatus) && ptp_operation_issupported(params,PTP_OC_NIKON_StartLiveView)) {
		ret = ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewStatus, &propval, PTP_DTC_UINT8);
		if (ret == PTP_RC_OK)
			params->inliveview = propval.u8;
		if (params->inliveview) af = 0;
	}

	if (NIKON_1(params) && ptp_operation_issupported(params,PTP_OC_NIKON_StartLiveView)) { /* V1 does not have startliveview */
		ret = ptp_nikon_start_liveview (params);
		if ((ret != PTP_RC_OK) && (ret != PTP_RC_DeviceBusy))
			C_PTP_REP_MSG(ret, _("Failed to enable liveview on a Nikon 1, but it is required for capture"));
		/* OK or busy, try to proceed ... */
		C_PTP_REP_MSG (nikon_wait_busy(params,20,2000), _("Nikon enable liveview failed"));
	}

	/* before we start real capture, move the current hw event queue to our local queue */
	while (ptp_get_one_event(params, &event)) {
		GP_LOG_D ("saving event queue before capture: event.Code is %x / param %lx", event.Code, (unsigned long)event.Param1);
		ptp_add_event_queue (&storedevents, &nrstoredevents, &event);
	}
	free(storedevents); storedevents = NULL;


	if (ptp_operation_issupported(params, PTP_OC_NIKON_InitiateCaptureRecInMedia)) {
		/* we assume for modern cameras this event method works to avoid longer waits */
		params->event90c7works = 1;

		loops = 100;
		do {
			ret = ptp_nikon_capture2 (params, af, sdram);
			/* Nikon 1 ... if af is 0, it reports PTP_RC_NIKON_InvalidStatus */
			if (!af && ((ret == PTP_RC_NIKON_InvalidStatus))) {
				ret = ptp_nikon_capture2 (params, 1, sdram);
				if (ret == PTP_RC_OK)
					break;
			}

			if (	(ret == PTP_RC_DeviceBusy) ||
				/* this is seen on Nikon V3 */
				(ret == PTP_RC_NIKON_InvalidStatus) ||
				(ret == 0xa207)	/* on Nikon D850 */
			)  usleep(2000);
		} while (((ret == PTP_RC_DeviceBusy) || (ret ==  PTP_RC_NIKON_InvalidStatus)) && loops--);
		goto capturetriggered;
	}

	if (!params->inliveview && af && ptp_operation_issupported(params,PTP_OC_NIKON_AfCaptureSDRAM)) {
		loops = 100;
		do {
			ret = ptp_nikon_capture_sdram(params);
		} while ((ret == PTP_RC_DeviceBusy) && (loops--));
	} else {
		loops = 100;
		do {
			ret = ptp_nikon_capture(params, 0xffffffff);
		} while ((ret == PTP_RC_DeviceBusy) && (loops--));
	}

capturetriggered:
	if (ret != PTP_RC_OK) {
		/* store back all the queued events back to the hw event queue before returning. */
		/* we do not do this in all error edge cases currently, only the ones that can trigger often */
		ptp_add_events (params, storedevents, nrstoredevents);
		free(storedevents); storedevents = NULL;
		C_PTP_REP (ret);
	}

	CR (gp_port_set_timeout (camera->port, capture_timeout));

	C_PTP_REP (nikon_wait_busy (params, 100, 1000*1000)); /* lets wait 1000 seconds (D780 can do 900seconds exposures) */

	newobject = 0xffff0001;
	done = 0; tries = 100;
	capture_start = time_now();
	do {
		/* Just busy loop until the camera is ready again. */
		/* and wait for the 0xc101 event */
		C_PTP_REP (ptp_check_event (params));
		while (ptp_get_one_event(params, &event)) {
			GP_LOG_D ("event.Code is %x / param %lx", event.Code, (unsigned long)event.Param1);
			switch (event.Code) {
			case PTP_EC_Nikon_ObjectAddedInSDRAM:
				done |= 3;
				newobject = event.Param1;
				if (!newobject) newobject = 0xffff0001;
				break;
			case PTP_EC_ObjectRemoved:
				ptp_remove_object_from_cache(params, event.Param1);
				gp_filesystem_reset (camera->fs);
				break;
			case PTP_EC_ObjectAdded: {
				PTPObject	*ob;

				/* if we got one object already, put it into the queue */
				/* e.g. for NEF+RAW capture */
				if (newobject != 0xffff0001) {
					ptp_add_event_queue (&storedevents, &nrstoredevents, &event);
					done = 3;
					break;
				}
				/* we register the object in the internal storage, and we also need to
				 * to find out if it is just a new directory (/DCIM/NEWENTRY/) created
				 * during capture or the actual image. */
				ret = ptp_object_want (params, event.Param1, PTPOBJECT_OBJECTINFO_LOADED, &ob);
				if (ret != PTP_RC_OK)
					break;
				/* if a new directory was added, not a file ... just continue.
				 * happens when the camera starts with an empty card. */
				if (ob->oi.ObjectFormat == PTP_OFC_Association) {
					/* libgphoto2 vfs does not notice otherwise */
					gp_filesystem_reset (camera->fs);
					break;
				}
				newobject = event.Param1;
				done |= 2;
				break;
			}
			case PTP_EC_Nikon_CaptureCompleteRecInSdram:
			case PTP_EC_CaptureComplete:
				if (params->inliveview) {
					GP_LOG_D ("Capture complete ... restarting liveview");
					ret = ptp_nikon_start_liveview (params);
				}
				done |= 1;
				break;
			default:
				GP_LOG_D ("UNHANDLED event.Code is %x / param %lx, DEFER", event.Code, (unsigned long)event.Param1);
				ptp_add_event_queue (&storedevents, &nrstoredevents, &event);
				break;
			}
		}
		/* we got both capturecomplete and objectadded ... leave */
		if (done == 3)
			break;
		/* just got capturecomplete ... wait a bit for a objectadded (Nikon 1) */
		if (done == 1) {
			if (!tries--)
				break;
		}
		gp_context_idle (context);
		/* do not drain all of the DSLRs compute time */
	} while ((done != 3) && waiting_for_timeout (&back_off_wait, capture_start, 70*1000)); /* 70 seconds */

	/* add all the queued events back to the event queue */
	ptp_add_events (params, storedevents, nrstoredevents);
	free(storedevents); storedevents = NULL;

	/* Maximum image time is 30 seconds, but NR processing might take 25 seconds ... so wait longer.
	 * see https://github.com/gphoto/libgphoto2/issues/94 */

	if (!newobject) newobject = 0xffff0001;

	CR (gp_port_set_timeout (camera->port, normal_timeout));

	/* This loop handles single and burst capture.
	 * It also handles SDRAM and also CARD capture.
	 * In Burst/SDRAM we need to download everything at once
	 * In all SDRAM modes we download and store it in the virtual fs.
	 * in Burst/CARD we add just the 1st as object, but do not download it yet.
	 */
	for (i=0;i<burstnumber;i++) {
		/* In Burst mode, the image is always 0xffff0001.
		 * The firmware just gives us one after the other for the same ID
		 * Not so for the D700 :/
		 */
		C_PTP (ptp_getobjectinfo (params, newobject, &oi));

		debug_objectinfo(params, newobject, &oi);

		if (oi.ParentObject == 0) { /* Capture to SDRAM */
			GP_LOG_E ("Parentobject of newobject 0x%x is 0x%x now?", (unsigned int)newobject, (unsigned int)oi.ParentObject);
			/* Happens on Nikon D70, we get Storage ID 0. So fake one. */
			if (oi.StorageID == 0) {
				strcpy (path->folder, "/");
			} else {
				sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx",(unsigned long)oi.StorageID);
			}
			if (oi.ObjectFormat != PTP_OFC_EXIF_JPEG) {
				/* the Nikon Coolpix P2 says "TIFF" and gives us "JPG". weird. */
				if (oi.Filename && strstr(oi.Filename,".JPG")) {
					GP_LOG_D ("rewriting %04x to JPEG %04x for %s", oi.ObjectFormat,PTP_OFC_EXIF_JPEG,oi.Filename);
					oi.ObjectFormat = PTP_OFC_EXIF_JPEG;
				}
			}

			if (oi.ObjectFormat != PTP_OFC_EXIF_JPEG) {
				GP_LOG_D ("raw? ofc is 0x%04x, name is %s", oi.ObjectFormat,oi.Filename);
				sprintf (path->name, "capt%04d.nef", params->capcnt++);
			} else {
				sprintf (path->name, "capt%04d.jpg", params->capcnt++);
			}
			ret = add_objectid_and_upload (camera, path, context, newobject, &oi);
			ptp_free_objectinfo(&oi);
			if (ret != GP_OK) {
				GP_LOG_E ("failed to add object\n");
				return ret;
			}
	/* this does result in 0x2009 (invalid object id) with the D90 ... curiuos
			ret = ptp_nikon_delete_sdram_image (params, newobject);
	 */
			if (!params->deletesdramfails) {
				ret = ptp_deleteobject (params, newobject, 0);
				if (ret != PTP_RC_OK) {
					GP_LOG_E ("deleteobject(%x) failed: %x", newobject, ret);
					if ((ret == PTP_RC_InvalidObjectHandle)  || (ret == PTP_RC_StoreNotAvailable))
						params->deletesdramfails = 1;
					else
						ret = ptp_nikon_delete_sdram_image (params, newobject);
					if (ret != PTP_RC_OK)
						GP_LOG_E ("deleteobjectinsdram(%x) failed too: %x", newobject, ret);
					if (ret == PTP_RC_InvalidObjectHandle)
						params->deletesdramfails = 1;
				}
			}
		} else { /* capture to card branch */
			ret = add_object_to_fs_and_path (camera, newobject, path, context);
			/* this has handled adding an association earlier */
			ptp_free_objectinfo(&oi);
			return ret;
		}
	}
	ptp_check_event (params);
	return GP_OK;
}

/* 90 seconds timeout in ms ... (for long cycles)
 * while the max shutterspeed is 30seconds, there is also postprocessing of 30seconds happening
 * in e.g. https://github.com/gphoto/libgphoto2/issues/503
 */
#define EOS_CAPTURE_TIMEOUT (90*1000)

/* This is currently the capture method used by the EOS 400D
 * ... in development.
 */
static int camera_trigger_canon_eos_capture (Camera *camera, GPContext *context);
static int
camera_canon_eos_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	int			ret;
	PTPParams		*params = &camera->pl->params;
	uint32_t		newobject = 0x0;
	PTPCanon_changes_entry	entry;
	CameraFile		*file = NULL;
	CameraFileInfo		info;
	PTPObjectInfo		oi;
	int			back_off_wait = 0;
	struct timeval          capture_start;
	char			*mime;

	if (!	(ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease) ||
		 ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn))
	) {
		gp_context_error (context,
		_("Sorry, your Canon camera does not support Canon EOS Capture"));
		return GP_ERROR_NOT_SUPPORTED;
	}

	capture_start = time_now();

	CR (camera_trigger_canon_eos_capture (camera, context));

	newobject = 0;
	memset (&oi, 0, sizeof(oi));
	do {
		C_PTP_REP_MSG (ptp_check_eos_events (params),
			       _("Canon EOS Get Changes failed"));
		while (ptp_get_one_eos_event (params, &entry)) {
			/* if we got at least one event from the last event polling, we reset our back_off_wait counter */
			back_off_wait = 0;
			GP_LOG_D ("entry type %04x", entry.type);
			switch (entry.type) {
			case PTP_CANON_EOS_CHANGES_TYPE_UNKNOWN:
				GP_LOG_D ("entry unknown: %s", entry.u.info);
				free (entry.u.info);
				continue; /* in loop ... do not poll while draining the queue */
			case PTP_CANON_EOS_CHANGES_TYPE_OBJECTTRANSFER:
				GP_LOG_D ("Found new object! OID 0x%x, name %s", (unsigned int)entry.u.object.oid, entry.u.object.oi.Filename);
				newobject = entry.u.object.oid;
				memcpy (&oi, &entry.u.object.oi, sizeof(oi));
				break;
			case PTP_CANON_EOS_CHANGES_TYPE_OBJECTREMOVED:
				GP_LOG_D ("Found removed object. OID 0x%x", (unsigned int)entry.u.object.oid);
				ptp_remove_object_from_cache(params, entry.u.object.oid);
				gp_filesystem_reset (camera->fs);
				break;
			case PTP_CANON_EOS_CHANGES_TYPE_OBJECTINFO: {
				int res;

				/* just add it to the filesystem, and return in CameraPath */
				GP_LOG_D ("Found new object! OID 0x%x, name %s", (unsigned int)entry.u.object.oid, entry.u.object.oi.Filename);
				/* We have some form of objectinfo in entry.u.object.oi already, but we let the
				 * code refetch it via regular GetObjectInfo */

				res = add_object_to_fs_and_path (camera, entry.u.object.oid, path, context);
				if (res < GP_OK)
					break;
				memcpy (&oi, &entry.u.object.oi, sizeof(oi));

				if  (entry.u.object.oi.ObjectFormat == PTP_OFC_Association)
					continue;
				gp_filesystem_append (camera->fs, path->folder, path->name, context);
				newobject = entry.u.object.oid;
				break;/* for RAW+JPG mode capture, we just return the first image for now, and
				       * let wait_for_event get the rest. */
			default:
				GP_LOG_D("unhandled eos change: %d", entry.type);
				break;
			}
			}
			if (newobject)
				break;
		}
		if (newobject)
			break;

		/* not really proven to help keep it on */
		if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_KeepDeviceOn)) C_PTP_REP (ptp_canon_eos_keepdeviceon (params));
		gp_context_idle (context);
	} while (waiting_for_timeout (&back_off_wait, capture_start, EOS_CAPTURE_TIMEOUT));

	if (newobject == 0)
		return GP_ERROR;
	GP_LOG_D ("object has OFC 0x%x", oi.ObjectFormat);

	if (oi.StorageID) /* all done above */
		return GP_OK;

	strcpy  (path->folder,"/");
	sprintf (path->name, "capt%04d.", params->capcnt++);
	CR (gp_file_new(&file));
	if (oi.ObjectFormat == PTP_OFC_CANON_CRW || oi.ObjectFormat == PTP_OFC_CANON_CRW3) {
		mime = GP_MIME_CRW;
		strcat(path->name, "cr2");
		gp_file_set_mime_type (file, GP_MIME_CRW);
	} else if (oi.ObjectFormat == PTP_OFC_CANON_CR3) {
		mime = GP_MIME_CR3;
		strcat(path->name, "cr3");
		gp_file_set_mime_type (file, GP_MIME_CR3);
	} else {
		mime = GP_MIME_JPEG;
		strcat(path->name, "jpg");
		gp_file_set_mime_type (file, GP_MIME_JPEG);
	}
	gp_file_set_mtime (file, time(NULL));

	GP_LOG_D ("trying to get object size=0x%lx", (unsigned long)oi.ObjectCompressedSize);

#define BLOBSIZE 1*1024*1024
	/* the EOS R does not like 5MB, but likes 1MB */
	/* Trying to read this in 1 block might be the cause of crashes of newer EOS */
	{
		uint32_t	offset = 0;

		while (offset < oi.ObjectCompressedSize) {
			uint32_t	xsize = oi.ObjectCompressedSize - offset;
			unsigned char	*ximage = NULL;

			if (xsize > BLOBSIZE)
				xsize = BLOBSIZE;
			C_PTP_REP (ptp_getpartialobject (params, newobject, offset, xsize, &ximage, &xsize));
			gp_file_append (file, (char*)ximage, xsize);
			free (ximage);
			offset += xsize;
		}
	}
	/*old C_PTP_REP (ptp_canon_eos_getpartialobject (params, newobject, 0, oi.ObjectCompressedSize, &ximage));*/
#undef BLOBSIZE


	C_PTP_REP (ptp_canon_eos_transfercomplete (params, newobject));
/*
	old:
	ret = gp_file_set_data_and_size(file, (char*)ximage, oi.ObjectCompressedSize);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
*/
	ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	ret = gp_filesystem_set_file_noop(camera->fs, path->folder, path->name, GP_FILE_TYPE_NORMAL, file, context);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	memset(&info, 0, sizeof(info));
	/* We also get the fs info for free, so just set it */
	info.file.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_SIZE | GP_FILE_INFO_MTIME;
	strcpy (info.file.type, mime);
	info.file.size		= oi.ObjectCompressedSize;
	info.file.mtime		= time(NULL);

	gp_filesystem_set_info_noop(camera->fs, path->folder, path->name, info, context);
	/* We have now handed over the file, disclaim responsibility by unref. */
	gp_file_unref (file);
	return GP_OK;
}

static int
camera_olympus_xml_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	uint16_t	ret;
	int		res;
	PTPParams	*params = &camera->pl->params;

	GP_LOG_D ("olympus capture");

	/* C_PTP (ptp_olympus_capture (params, 3)); */
	C_PTP (ptp_generic_no_data (params, PTP_OC_OLYMPUS_Capture, 1, 3));

	while (1) {
		PTPContainer event;

		ret = ptp_check_event(params);
		if (ret != PTP_RC_OK)
			break;

		event.Code = 0;
		while (ptp_get_one_event (params, &event)) {
			GP_LOG_D ("capture 1: got event 0x%x (param1=%x)", event.Code, event.Param1);
			if (event.Code == PTP_EC_Olympus_CaptureComplete) break;
		}
		if (event.Code == PTP_EC_Olympus_CaptureComplete)
			break;
	}
	/* C_PTP (ptp_olympus_capture (params, 0)); */
	C_PTP (ptp_generic_no_data (params, PTP_OC_OLYMPUS_Capture, 1, 0));

	/* 0x1a000002 object id */
	while (1) {
		PTPContainer event;
		uint32_t	assochandle = 0;

		ret = ptp_check_event(params);
		if (ret != PTP_RC_OK)
			break;

		event.Code = 0;
		while (ptp_get_one_event (params, &event)) {
			GP_LOG_D ("capture 2: got event 0x%x (param1=%x)", event.Code, event.Param1);
			if (event.Code == PTP_EC_RequestObjectTransfer) {
				PTPObjectInfo oi;

				C_PTP_MSG (ptp_getobjectinfo (params, event.Param1, &oi),
					   "capture 2: no objectinfo for 0x%x", event.Param1);
				debug_objectinfo(params, event.Param1, &oi);
				/* We get usually
				 * 0x1a000001 - folder
				 * 0x1a000002 - image within that folder
				 */

				/* remember for later deletion */
				if (oi.ObjectFormat == PTP_OFC_Association) {
					assochandle = event.Param1;
					ptp_free_objectinfo(&oi);
					continue;
				}

				if (oi.ObjectFormat == PTP_OFC_EXIF_JPEG) {
					sprintf (path->folder,"/");
					sprintf (path->name, "capt%04d.jpg", params->capcnt++);
					res = add_objectid_and_upload (camera, path, context, event.Param1, &oi);

					ret = ptp_deleteobject (params, event.Param1, PTP_OFC_EXIF_JPEG);
					if (ret != PTP_RC_OK)
						GP_LOG_E ("capture 2: delete image %08x, ret 0x%04x", event.Param1, ret);
					ret = ptp_deleteobject (params, assochandle, PTP_OFC_Association);
					if (ret != PTP_RC_OK)
						GP_LOG_E ("capture 2: delete folder %08x, ret 0x%04x", assochandle, ret);
					ptp_free_objectinfo(&oi);
					return res;
				}
				ptp_free_objectinfo(&oi);
				GP_LOG_E ("capture 2: unknown OFC 0x%04x for 0x%x", oi.ObjectFormat, event.Param1);
			}
		}
	}
	return GP_ERROR;
}

/* To use:
 *	gphoto2 --set-config capture=on --config --capture-image
 *	gphoto2  -f /store_80000001 -p 1
 *		will download a file called "VirtualObject"
 */
static int
camera_canon_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	PTPObjectInfo		oi;
	int			found, ret, timeout, sawcapturecomplete = 0, viewfinderwason = 0;
	PTPParams		*params = &camera->pl->params;
	uint32_t		newobject = 0x0;
	PTPPropertyValue	propval;
	PTPContainer		event;
	char 			buf[1024];
	int			xmode = CANON_TRANSFER_CARD;
	struct timeval		event_start;

	if (!ptp_operation_issupported(params, PTP_OC_CANON_InitiateCaptureInMemory)) {
		gp_context_error (context,
		_("Sorry, your Canon camera does not support Canon Capture initiation"));
		return GP_ERROR_NOT_SUPPORTED;
	}

	/* did not call --set-config capture=on, do it for user */
	CR (camera_prepare_capture (camera, context));

	if (!params->canon_event_mode) {
		propval.u16 = 0;
	        ret = ptp_getdevicepropvalue(params, PTP_DPC_CANON_EventEmulateMode, &propval, PTP_DTC_UINT16);
		if (ret == PTP_RC_OK) params->canon_event_mode = propval.u16;
	}

	if (ptp_property_issupported(params, PTP_DPC_CANON_CaptureTransferMode)) {
		if ((GP_OK == gp_setting_get("ptp2","capturetarget",buf)) && !strcmp(buf,"sdram"))
			propval.u16 = xmode = CANON_TRANSFER_MEMORY;
		else
			propval.u16 = xmode = CANON_TRANSFER_CARD;

		if (xmode == CANON_TRANSFER_CARD) {
			PTPStorageIDs storageids;

			ret = ptp_getstorageids(params, &storageids);
			if (ret == PTP_RC_OK) {
				unsigned int k, stgcnt = 0;
				for (k=0;k<storageids.n;k++) {
					if (!(storageids.Storage[k] & 0xffff)) continue;
					if (storageids.Storage[k] == 0x80000001) continue;
					stgcnt++;
				}
				if (!stgcnt) {
					GP_LOG_D ("Assuming no CF card present - switching to MEMORY Transfer.");
					propval.u16 = xmode = CANON_TRANSFER_MEMORY;
				}
				free (storageids.Storage);
			}
		}
		LOG_ON_PTP_E (ptp_setdevicepropvalue(params, PTP_DPC_CANON_CaptureTransferMode, &propval, PTP_DTC_UINT16));
	}

	if (params->canon_viewfinder_on) { /* disable during capture ... reenable later on. */
		C_PTP_REP_MSG (ptp_canon_viewfinderoff (params),
			       _("Canon disable viewfinder failed"));
		viewfinderwason = 1;
		params->canon_viewfinder_on = 0;
	}
	C_PTP_REP_MSG (ptp_check_event (params),
		       _("Canon Capture failed"));

#if 0
	/* FIXME: For now, to avoid flash during debug */
	propval.u8 = 0;
	ret = ptp_setdevicepropvalue(params, PTP_DPC_CANON_FlashMode, &propval, PTP_DTC_UINT8);
#endif
	C_PTP_REP_MSG (ptp_canon_initiatecaptureinmemory (params),
		       _("Canon Capture failed"));
	sawcapturecomplete = 0;
	/* Checking events in stack. */
	event_start = time_now();
	found = FALSE;

	gp_port_get_timeout (camera->port, &timeout);
	CR (gp_port_set_timeout (camera->port, capture_timeout));
	while (time_since (event_start) < capture_timeout) {
		gp_context_idle (context);
		/* Make sure we do not poll USB interrupts after the capture complete event.
		 * MacOS libusb 1 has non-timing out interrupts so we must avoid event reads that will not
		 * result in anything.
		 */
		ret = ptp_check_event (params);
		if (ret != PTP_RC_OK)
			break;

		if (!ptp_get_one_event (params, &event)) {
			/* FIXME: wait a bit? */
			usleep(20*1000);
			continue;
		}
		GP_LOG_D ("Event: nparams=0x%X, code=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", event.Nparam,event.Code,event.Transaction_ID, event.Param1, event.Param2, event.Param3);
		switch (event.Code) {
		case PTP_EC_ObjectRemoved:
			ptp_remove_object_from_cache(params, event.Param1);
			gp_filesystem_reset (camera->fs);
			break;
		case PTP_EC_ObjectAdded: {
			/* add newly created object to internal structures. this hopefully just is a new folder */
			PTPObject	*ob;

			GP_LOG_D ("Event ObjectAdded, object handle=0x%X.", newobject);

			ret = ptp_object_want (params, event.Param1, PTPOBJECT_OBJECTINFO_LOADED, &ob);
			if (ret != PTP_RC_OK)
				break;
			/* this might be just the folder add, ignore that. */
			if (ob->oi.ObjectFormat == PTP_OFC_Association) {
				/* new directory ... mark fs as to be refreshed */
				gp_filesystem_reset (camera->fs); /* FIXME: implement more lightweight folder add */
				break;
			} else {
				/* new file */
				newobject = event.Param1;
				/* FALLTHROUGH */
			}
		}
			/* FALLTHROUGH */
		case PTP_EC_CANON_RequestObjectTransfer: {
			int j;

			newobject = event.Param1;
			GP_LOG_D ("Event PTP_EC_CANON_RequestObjectTransfer, object handle=0x%X.", newobject);
			/* drain the event queue further */
			for (j=0;j<2;j++) {
				ret = ptp_check_event (params);
				while (ptp_get_one_event (params, &event) && !sawcapturecomplete) {
					GP_LOG_D ("evdata: L=0x%X, C=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", event.Nparam,event.Code,event.Transaction_ID, event.Param1, event.Param2, event.Param3);
					if (event.Code == PTP_EC_CaptureComplete)
						sawcapturecomplete = 1;
				}
				if (sawcapturecomplete)
					break;
				usleep(20*1000);
			}
			/* Marcus: Not sure if we really needs this. This refocuses the camera.
			   ret = ptp_canon_reset_aeafawb(params,7);
			 */
			found = TRUE;
			break;
		}
		case PTP_EC_CaptureComplete:
			GP_LOG_D ("Event: Capture complete.");
			sawcapturecomplete = 1;
			break;
		default:
			GP_LOG_D ("Event unhandled: nparams=0x%X, code=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X",
				event.Nparam, event.Code, event.Transaction_ID, event.Param1, event.Param2, event.Param3
			);
			break;
		}
		if (found == TRUE)
			break;
	}
	CR (gp_port_set_timeout (camera->port, timeout));
	/* Catch event, attempt 2 */
	while (!sawcapturecomplete) {
		ret = ptp_check_event (params);
		if (ret != PTP_RC_OK)
			break;
		while (ptp_get_one_event (params, &event)) {
			if (event.Code==PTP_EC_CaptureComplete) {
				GP_LOG_D ("Event: capture complete(2).");
				sawcapturecomplete = 1;
				break;
			}
		}
		/* FIXME: wait backoff */
		GP_LOG_D ("Event: 0x%X (2)", event.Code);
	}
	if (!found) {
	    GP_LOG_D ("ERROR: Capture timed out!");
	    return GP_ERROR_TIMEOUT;
	}
	if (viewfinderwason) { /* disable during capture ... reenable later on. */
		viewfinderwason = 0;
		C_PTP_REP_MSG (ptp_canon_viewfinderon (params),
			       _("Canon enable viewfinder failed"));
		params->canon_viewfinder_on = 1;
	}

	/* FIXME: handle multiple images (as in BurstMode) */
	C_PTP (ptp_getobjectinfo (params, newobject, &oi));

	if (oi.ParentObject != 0) {
		if (xmode != CANON_TRANSFER_CARD) {
			fprintf (stderr,"parentobject is 0x%x, but not in card mode?\n", oi.ParentObject);
		}
		ret = add_object_to_fs_and_path (camera, newobject, path, context);
		/* FIXME: consistent handling of association add ... perhaps try another event loop turnaroundloop turnaround  */
		if (oi.ObjectFormat == PTP_OFC_Association)
			gp_filesystem_reset (camera->fs);
		ptp_free_objectinfo(&oi);
		return ret;
	} else {
		if (xmode == CANON_TRANSFER_CARD) {
			fprintf (stderr,"parentobject is 0, but not in memory mode?\n");
		}
		sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx",(unsigned long)oi.StorageID);
		sprintf (path->name, "capt%04d.jpg", params->capcnt++);
		ret = add_objectid_and_upload (camera, path, context, newobject, &oi);
		ptp_free_objectinfo(&oi);
		return ret;
	}
}

static int
camera_sony_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
{
	PTPParams	*params = &camera->pl->params;
	PTPPropertyValue propval;
	PTPContainer	event;
	PTPObjectInfo	oi;
	uint32_t	newobject = 0;
	PTPDevicePropDesc	dpd;
	struct timeval	event_start;
	int		ret;

	if (params->deviceinfo.Model && (
		!strcmp(params->deviceinfo.Model, "ZV-E10")		||
		!strcmp(params->deviceinfo.Model, "ZV-1")		||
		!strcmp(params->deviceinfo.Model, "DSC-RX100M7")	||
		!strcmp(params->deviceinfo.Model, "ILCE-7RM4")		||
		!strcmp(params->deviceinfo.Model, "ILCE-7RM4A")		||
		!strcmp(params->deviceinfo.Model, "DSC-RX0M2")		||
		!strcmp(params->deviceinfo.Model, "ILCE-7C")
	)) {
		/* For some as yet unknown reason the ZV-1, the RX100M7 and the A7 R4 need around 3 seconds startup time
		 * to be able to capture. I looked for various trigger events or property changes on the ZV-1
		 * but nothing worked except waiting.
		 * This might not be required when having manual focusing according to https://github.com/gphoto/gphoto2/issues/349
		 */

		while (time_since (params->starttime) < 2500) {
			/* drain the queue first */
			if (ptp_get_one_event(params, &event)) {
				GP_LOG_D ("during event.code=%04x Param1=%08x", event.Code, event.Param1);
				continue;
			}
			/* wait for events and poll property data */
			C_PTP (ptp_check_event (params));

			C_PTP (ptp_sony_getalldevicepropdesc (params)); /* avoid caching */
		}
	}

	/* regular code */

	C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_CompressionSetting, &dpd));

	GP_LOG_D ("dpd.CurrentValue.u8 = %x", dpd.CurrentValue.u8);
	GP_LOG_D ("dpd.FactoryDefaultValue.u8 = %x", dpd.FactoryDefaultValue.u8);

	if (dpd.CurrentValue.u8 == 0)
		dpd.CurrentValue.u8 = dpd.FactoryDefaultValue.u8;
	if (dpd.CurrentValue.u8 == 0x13) {
		GP_LOG_D ("expecting raw+jpeg capture");
	}
	/* half-press */
	propval.u16 = 2;
	C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_AutoFocus, &propval, PTP_DTC_UINT16));

	/* full-press */
	propval.u16 = 2;
	C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_Capture, &propval, PTP_DTC_UINT16));

	/* Check if we are in manual focus to skip the wait for focus */
	C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_FocusMode, &dpd));
	if (dpd.CurrentValue.u16 != 1) { /* 1 is Manual .. no need to wait there for focusing */

		/* Now hold down the shutter button for a bit. We probably need to hold it as long as it takes to
		* get focus, indicated by the 0xD213 property. But hold it for at most 1 second.
		*/

		GP_LOG_D ("holding down shutterbutton");
		event_start = time_now();
		do {
			/* needed on older cameras like the a58, check for events ... */
			C_PTP (ptp_check_event (params));
			if (ptp_get_one_event(params, &event)) {
				GP_LOG_D ("during event.code=%04x Param1=%08x", event.Code, event.Param1);
				if (	(event.Code == PTP_EC_Sony_PropertyChanged) &&
					(event.Param1 == PTP_DPC_SONY_FocusFound)
				) {
					GP_LOG_D ("SONY FocusFound change received, 0xd213... ending press");
					break;
				}
				if (event.Code == PTP_EC_Sony_ObjectAdded) {
					newobject = event.Param1;
					GP_LOG_D ("SONY ObjectAdded received, ending wait");
					break;
				}
			}

			/* Alternative code in case we miss the event */

			C_PTP (ptp_sony_getalldevicepropdesc (params)); /* avoid caching */
			C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_FocusFound, &dpd));
			GP_LOG_D ("DEBUG== 0xd213 after shutter press = %d", dpd.CurrentValue.u8);
			/* if prop 0xd213 = 2 or 3 (for rx0), the focus seems to be achieved */
			if (dpd.CurrentValue.u8 == 2 || dpd.CurrentValue.u8 == 3) {
				GP_LOG_D ("SONY Property change seen, 0xd213... ending press");
				break;
			}

		} while (time_since (event_start) < 1000);
	}
	GP_LOG_D ("releasing shutterbutton");

	/* release full-press */
	propval.u16 = 1;
	C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_Capture, &propval, PTP_DTC_UINT16));

	/* release half-press */
	propval.u16 = 1;
	C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_AutoFocus, &propval, PTP_DTC_UINT16));

	GP_LOG_D ("waiting for image availability");
	event_start = time_now();
	do {
		/* break if we got it from above focus wait already for some reason, seen on A6000 */
		if (newobject) break;
#if 1
		/* needed on older cameras like the a58, check for events ... */
		/* This would be unsafe if we get an out-of-order event with no objects present, but
		 * we drained all events above */
		C_PTP (ptp_check_event_queue (params));
		if (ptp_get_one_event(params, &event)) {
			GP_LOG_D ("during event.code=%04x Param1=%08x", event.Code, event.Param1);
			if (event.Code == PTP_EC_Sony_ObjectAdded) {
				newobject = event.Param1;
				GP_LOG_D ("SONY ObjectAdded received, ending wait");
				break;
			}
		}
#endif

		C_PTP (ptp_sony_getalldevicepropdesc (params)); /* avoid caching */
		C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_ObjectInMemory, &dpd));
		GP_LOG_D ("DEBUG== 0xd215 after capture = %d", dpd.CurrentValue.u16);

		/* if prop 0xd215 > 0x8000, the object in RAM is available at location 0xffffc001 */
		/* This variable also turns to 1 , but downloading then will crash the firmware
		 * we seem to need to wait for 0x8000 */
		if (dpd.CurrentValue.u16 >= 0x8000) {
			GP_LOG_D ("SONY ObjectInMemory count change to 0x%x seen, ending wait", dpd.CurrentValue.u16);
			newobject = 0xffffc001;
			if (dpd.CurrentValue.u16 == 0x8001) {
				/* Also synthesize a capture complete event, if its just 1 image. */
				event.Code = PTP_EC_CaptureComplete;
				event.Nparam = 0;
				ptp_add_event (params, &event);
			}
			break;
		}

	/* 30 seconds are maximum capture time currently, so use 30 seconds + 5 seconds image saving at most. */
	} while (time_since (event_start) < 35000);
	GP_LOG_D ("ending image availability");

	/* If the camera does not report object presence, it will crash if we access 0xffffc001 ... */
	if (!newobject) {
		GP_LOG_E("no object found during event polling. perhaps no focus...");
		return GP_ERROR;
	}
	/* FIXME: handle multiple images (as in BurstMode) */
	C_PTP (ptp_getobjectinfo (params, newobject, &oi));

	sprintf (path->folder,"/");
	if (oi.ObjectFormat == PTP_OFC_SONY_RAW)
		sprintf (path->name, "capt%04d.arw", params->capcnt++);
	else
		sprintf (path->name, "capt%04d.jpg", params->capcnt++);
	ret = add_objectid_and_upload (camera, path, context, newobject, &oi);
	ptp_free_objectinfo (&oi);
	return ret;
}

static int
camera_sony_qx_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
{
	PTPParams	*params = &camera->pl->params;
	PTPPropertyValue propval;
	PTPObjectInfo	oi;
	uint32_t	newobject = 0;
	struct timeval	event_start;
	PTPDevicePropDesc	dpd;
	int		res;

	C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_QX_OperatingMode, &dpd));

	while (dpd.CurrentValue.u8 != 2) {
		propval.u8 = 2; /* 2 is Still Capture */
		C_PTP (ptp_sony_qx_setdevicecontrolvaluea (params, PTP_DPC_SONY_QX_OperatingMode, &propval, PTP_DTC_UINT8));
		/* lets try to get the props ... */
		C_PTP (ptp_sony_qx_getalldevicepropdesc (params)); /* avoid caching */
		C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_QX_OperatingMode, &dpd));
	}

#if 0

	C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_CompressionSetting, &dpd));

	GP_LOG_D ("dpd.CurrentValue.u8 = %x", dpd.CurrentValue.u8);
	GP_LOG_D ("dpd.FactoryDefaultValue.u8 = %x", dpd.FactoryDefaultValue.u8);

	if (dpd.CurrentValue.u8 == 0)
		dpd.CurrentValue.u8 = dpd.FactoryDefaultValue.u8;
	if (dpd.CurrentValue.u8 == 0x13) {
		GP_LOG_D ("expecting raw+jpeg capture");
	}
#endif
	/* half-press (S1 press?) */
	propval.u16 = 2;
	C_PTP (ptp_sony_qx_setdevicecontrolvalueb (params, PTP_DPC_SONY_QX_AutoFocus, &propval, PTP_DTC_UINT16));

	event_start = time_now();
	do {
		unsigned char	*object = NULL;
		uint16_t	ret;

		ret = ptp_getobject (params, 0xffffc002, &object);
		if (ret == PTP_RC_AccessDenied)
			break;	/* indicator that image is there */
		free (object);
	} while (time_since (event_start) < 500);

	/* full-press (S2 press?) */
	propval.u16 = 2;
	C_PTP (ptp_sony_qx_setdevicecontrolvalueb (params, PTP_DPC_SONY_QX_Capture, &propval, PTP_DTC_UINT16));


	/* C Sharp code now does 300ms wait */

#if 0
	/* Check if we are in manual focus to skip the wait for focus */
	C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_FocusMode, &dpd));
	if (dpd.CurrentValue.u16 != 1) { /* 1 is Manual .. no need to wait there for focusing */

		/* Now hold down the shutter button for a bit. We probably need to hold it as long as it takes to
		* get focus, indicated by the 0xD213 property. But hold it for at most 1 second.
		*/
#endif
		GP_LOG_D ("holding down shutterbutton for 1 second... ");
		event_start = time_now();
		do {
			unsigned char *object = NULL;
			uint16_t	ret;

			C_PTP (ptp_generic_getdevicepropdesc (params, 0xd6e0, &dpd));

			if (dpd.CurrentValue.u8 != 0)
				break;

			ret = ptp_getobject (params, 0xffffc002, &object);
			if (ret == PTP_RC_AccessDenied)
				break;	/* indicator that image is there */
			free (object);

			/* Alternative code in case we miss the event */

			C_PTP (ptp_sony_qx_getalldevicepropdesc (params)); /* avoid caching */
		} while (time_since (event_start) < 500);
#if 0
	}
#endif
	GP_LOG_D ("releasing shutterbutton");

	/* release full-press */
	propval.u16 = 1;
	C_PTP (ptp_sony_qx_setdevicecontrolvalueb (params, PTP_DPC_SONY_QX_Capture, &propval, PTP_DTC_UINT16));

	event_start = time_now();
	do {
		unsigned char	*object = NULL;
		uint16_t	ret;

		ret = ptp_getobject (params, 0xffffc002, &object);
		if (ret == PTP_RC_AccessDenied)
			break;	/* indicator that image is there */
		free (object);
	} while (time_since (event_start) < 500);


	/* release half-press */
	propval.u16 = 1;
	C_PTP (ptp_sony_qx_setdevicecontrolvalueb (params, PTP_DPC_SONY_QX_AutoFocus, &propval, PTP_DTC_UINT16));


	/* csharp now waits for 1 second */

	GP_LOG_D ("waiting for image availability");
	event_start = time_now();
	do {
		/* break if we got it from above focus wait already for some reason, seen on A6000 */

		oi.ObjectFormat = 0;
		C_PTP (ptp_getobjectinfo (params, 0xffffc001, &oi));
		if (oi.ObjectFormat) {
			newobject = 0xffffc001;
			break;
		}
		ptp_free_objectinfo (&oi);

		C_PTP (ptp_sony_qx_getalldevicepropdesc (params)); /* avoid caching */
		/* 30 seconds are maximum capture time currently, so use 30 seconds + 5 seconds image saving at most. */
	} while (time_since (event_start) < 35000);
	GP_LOG_D ("ending image availability");

	/* If the camera does not report object presence, it will crash if we access 0xffffc001 ... */
	if (!newobject) {
		GP_LOG_E("no object found during event polling. try anyway 0xffffc001...");
		newobject = 0xffffc001;
		/*return GP_ERROR;*/
	}
	/* FIXME: handle multiple images (as in BurstMode) */

	GP_LOG_D ("waiting for ffffc001 objectinfo availability");
	/* QX software also seems to do it, queries objectinfo and needs several tries */
	event_start = time_now();
	do {
		oi.ObjectFormat = 0;
		C_PTP (ptp_getobjectinfo (params, newobject, &oi));
		if (oi.ObjectFormat)
			break;

		C_PTP (ptp_sony_qx_getalldevicepropdesc (params)); /* avoid caching */
	} while (!oi.ObjectFormat && (time_since (event_start) < 5000));

	sprintf (path->folder,"/");
	if (oi.ObjectFormat == PTP_OFC_SONY_RAW)
		sprintf (path->name, "capt%04d.arw", params->capcnt++);
	else
		sprintf (path->name, "capt%04d.jpg", params->capcnt++);
	res = add_objectid_and_upload (camera, path, context, newobject, &oi);
	ptp_free_objectinfo (&oi);
	return res;
}

static int
camera_fuji_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
{
	PTPParams		*params = &camera->pl->params;
	PTPPropertyValue	propval;
	PTPObjectHandles	handles, beforehandles;
	int			gotone;
	uint32_t		newobject = 0, newobject2 = 0;
	PTPContainer		event, newevent;
	struct timeval		event_start;
	int			back_off_wait = 0;
	unsigned int		i, waittime = 35*1000;

	GP_LOG_D ("camera_fuji_capture");

	if (params->inliveview) {
		GP_LOG_D ("terminating running liveview");
		params->inliveview = 0;
		C_PTP (ptp_terminateopencapture (params,params->opencapture_transid));
	}

	C_PTP (ptp_getobjecthandles (params, PTP_HANDLER_SPECIAL, 0x000000, 0x000000, &beforehandles));

	C_PTP_REP (ptp_getdevicepropvalue (params, PTP_DPC_ExposureTime, &propval, PTP_DTC_UINT32));
	if (propval.u32 >= 10079368) {/* if longer than 10 seconds, switch to 2 hours wait max. (we could also add a translation table here if needed) */
		switch (propval.u32) {
		/* 2 times to allow for black picture, + safety time */
		case 64000180: waittime = 2*60*60*1000 + 1*60*1000;break;
		case 64000150: waittime = 2*30*60*1000 + 1*60*1000;break;
		case 64000120: waittime = 2*15*60*1000 + 1*60*1000;break;
		case 64000090: waittime = 2*8*60*1000 + 1*30*1000;break;
		case 64000060: waittime = 2*4*60*1000 + 1*30*1000;break;
		case 64000030: waittime = 2*2*60*1000 + 1*30*1000;break;
		case 64000000: waittime = 2*1*60*1000 + 1*15*1000;break;
		case 50796833: waittime = 2*1*50*1000 + 1*15*1000;break; /* 50s */
		case 40317473: waittime = 2*1*40*1000 + 1*15*1000;break; /* 40s */
		case 32000000: waittime = 2*1*30*1000 + 1*15*1000;break; /* 30s */
		case 25398416: waittime = 2*1*25*1000 + 1*15*1000;break; /* 25s */
		case 20158736: waittime = 2*1*20*1000 + 1*15*1000;break; /* 20s */
		case 16000000: waittime = 2*1*16*1000 + 1*10*1000;break; /* 15s */
		case 12699208: waittime = 2*1*16*1000 + 1*10*1000;break; /* 15s */
		case 10079368: waittime = 2*1*16*1000 + 1*10*1000;break; /* 15s */
		default:
			GP_LOG_D("unknown exposure time %d, waiting 2 hours", propval.u32);
			waittime = 2*61*60*1000;
			break;
		}
	}

	/* focus */
	propval.u16 = 0x0200;
	C_PTP_REP (ptp_setdevicepropvalue (params, 0xd208, &propval, PTP_DTC_UINT16));

	C_PTP_REP (ptp_initiatecapture(params, 0x00000000, 0x00000000));

	/* poll camera until it is ready */
	propval.u16 = 0x0001;
	while (propval.u16 == 0x0001) {
		C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_FUJI_AFStatus, &propval, PTP_DTC_UINT16));
		GP_LOG_D ("XXX Ready to shoot? %X", propval.u16);
	}

	/* 2 - means OK apparently, 3 - means failed and initiatecapture will get busy. */
	if (propval.u16 == 3) { /* reported on out of focus */
		gp_context_error (context, _("Fuji Capture failed: Perhaps no auto-focus?"));
		return GP_ERROR;
	}

	/* shoot */
	propval.u16 = 0x0304;
	C_PTP_REP (ptp_setdevicepropvalue (params, 0xd208, &propval, PTP_DTC_UINT16));


	C_PTP_REP(ptp_initiatecapture(params, 0x00000000, 0x00000000));

	event_start = time_now();
	do {
		uint16_t ret, count = 0;
		uint16_t *events = NULL;

		C_PTP (ptp_fuji_getevents (params, &events, &count));
		/* FIXME: should do something with those if needed */
		free (events);

		/* FIXME: Marcus ... I need to review this when I get hands on a camera ... the objecthandles loop needs to go */
		/* Reporter in https://github.com/gphoto/libgphoto2/issues/133 says only 1 event ever is sent, so this does not work */
		/* there is a ObjectAdded event being sent */

		/* Marcus: X-Pro2 in current setup also sends just 1 event for the first capture, then none.
		C_PTP_REP (ptp_setdevicepropvalue (params, 0xd208, &propval, PTP_DTC_UINT16));
		 * We might be missing something after capture.
		 * But we need to drain the event queue, otherwise wait_event will see this ObjectAdded event again. */

		C_PTP_REP (ptp_check_event (params));

		gotone = 0;
		while (ptp_get_one_event(params, &event)) {
			PTPObject	*ob;

			switch (event.Code) {
				case PTP_EC_ObjectAdded:
				case PTP_EC_FUJI_ObjectAdded:
					GP_LOG_D ("Event Code %04x, Param 1 %08x", event.Code, event.Param1);
					ret = ptp_object_want (params, event.Param1, PTPOBJECT_OBJECTINFO_LOADED, &ob);
					if (ret != PTP_RC_OK) {
						GP_LOG_E ("object added, but not found?");
						continue;
					}
					/* A directory was added, like initial DCIM/100NIKON or so. */
					if (ob->oi.ObjectFormat == PTP_OFC_Association) {
						GP_LOG_D ("new object 0x%08x is a directory, continuing", event.Param1);
						break;
					}
					if (newobject) {
						/* we already got one image ... means a secondary image in RAW+JPG. Push this to the event queue. */
						newevent.Code = PTP_EC_ObjectAdded;
						newevent.Param1 = newobject;
						ptp_add_event (params, &newevent);
					}
					newobject = event.Param1;
					GP_LOG_D ("newobject 0x%08x", newobject);
					/* we found a new file */
					strcpy  (path->name,  ob->oi.Filename);
					sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)ob->oi.StorageID);
					get_folder_from_handle (camera, ob->oi.StorageID, ob->oi.ParentObject, path->folder);
					/* above might invalidate "ob" pointer, so refetch it */
					ptp_object_want (params, event.Param1, PTPOBJECT_OBJECTINFO_LOADED, &ob);

					/* delete last / or we get confused later. */
					path->folder[ strlen(path->folder)-1 ] = '\0';
					add_objectid_and_upload (camera, path, context, newobject, &ob->oi);
					/* we need to proceed to download all images, in cases of RAW+JPG capture */
					gotone = 1;
					break;
				default:
					GP_LOG_D ("unexpected unhandled event Code %04x, Param 1 %08x", event.Code, event.Param1);
					break;
			}
		}
		if (gotone)  {
			free (beforehandles.Handler);
			return GP_OK;
		}

		/* If we got no event seconds duplicate the nikon broken capture, as we do not know how to get events yet */

		GP_LOG_D ("XXXX missing fuji objectadded events workaround");

		/* clear path, so we get defined results even without object info */
		path->name[0]   = '\0';
		path->folder[0] = '\0';

		C_PTP (ptp_getobjecthandles (params, PTP_HANDLER_SPECIAL, 0x000000, 0x000000, &handles));

		/* if (handles.n == params->handles.n)
		 *	continue;
		 * While this is a potential optimization, lets skip it for now.
		 */
		newobject = 0;
		for (i=0;i<handles.n;i++) {
			unsigned int 	j;
			PTPObject	*ob;

			/* look if we saw the objecthandle before capture */
			for (j=0;j<beforehandles.n;j++)
				if (beforehandles.Handler[j] == handles.Handler[i])
					break;
			if (j != beforehandles.n)
				continue;
			GP_LOG_D ("new object 0x%08x found", handles.Handler[i]);
			gotone = 1;

			ret = ptp_object_want (params, handles.Handler[i], PTPOBJECT_OBJECTINFO_LOADED, &ob);
			if (ret != PTP_RC_OK) {
				GP_LOG_E ("object added, but not found?");
				continue;
			}
			/* A directory was added, like initial DCIM/100NIKON or so. */
			if (ob->oi.ObjectFormat == PTP_OFC_Association) {
				GP_LOG_D ("new object 0x%08x is a directory, continuing", handles.Handler[i]);
				continue;
			}
			if (newobject) {
				/* we already got one image ... means a secondary image in RAW+JPG. Push this to the event queue. */
				event.Code = PTP_EC_ObjectAdded;
				event.Param1 = newobject;
				ptp_add_event (params, &event);
			}
			newobject = handles.Handler[i];
			GP_LOG_D ("newobject 0x%08x, newobject2 0x%08x", newobject, newobject2);
			/* we found a new file */
			strcpy  (path->name,  ob->oi.Filename);
			sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)ob->oi.StorageID);
			get_folder_from_handle (camera, ob->oi.StorageID, ob->oi.ParentObject, path->folder);
			/* above might invalidate "ob" pointer, so refetch it */
			ptp_object_want (params, newobject, PTPOBJECT_OBJECTINFO_LOADED, &ob);
			/* delete last / or we get confused later. */
			path->folder[ strlen(path->folder)-1 ] = '\0';
			add_objectid_and_upload (camera, path, context, newobject, &ob->oi);
			/* we need to proceed to download all images, in cases of RAW+JPG capture */
		}
		free (handles.Handler);

		if (gotone)  {
			free (beforehandles.Handler);
			return GP_OK;
		}
	}  while (waiting_for_timeout (&back_off_wait, event_start, waittime));
	free (beforehandles.Handler);
	return GP_ERROR;
}

static int
camera_panasonic_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
{
	PTPParams	*params = &camera->pl->params;
	PTPContainer	event;
	uint32_t	newobject = 0;
	struct timeval	event_start;
	int		back_off_wait = 0;
	PTPObject	*ob = NULL;

	//C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_CompressionSetting, &dpd));

	//GP_LOG_D ("dpd.CurrentValue.u8 = %x", dpd.CurrentValue.u8);
	//GP_LOG_D ("dpd.FactoryDefaultValue.u8 = %x", dpd.FactoryDefaultValue.u8);

	//if (dpd.CurrentValue.u8 == 0)
	//	dpd.CurrentValue.u8 = dpd.FactoryDefaultValue.u8;
	//if (dpd.CurrentValue.u8 == 0x13) {
	//	GP_LOG_D ("expecting raw+jpeg capture");
	//}

	uint32_t currentVal;
	uint16_t valuesize;
	uint32_t waitMS = 1000;

	ptp_panasonic_getdeviceproperty(params, PTP_DPC_PANASONIC_ShutterSpeed, &valuesize, &currentVal);

	float f;
	if(currentVal == 0xFFFFFFFF) {
		waitMS = 1000;
	} else if(currentVal & 0x80000000) {
		currentVal &= ~0x80000000;
		f = (float) currentVal;
		waitMS = (uint32_t)f + 1000;
	} else {
		waitMS = 1000;
	}


	// clear out old events
	GP_LOG_D ("**** GH5: checking old events...");
	C_PTP_REP (ptp_check_event (params));
	GP_LOG_D ("**** GH5: draining old events...");
	while (ptp_get_one_event(params, &event));

	GP_LOG_D ("**** GH5: trigger capture...");
	C_PTP_REP (ptp_panasonic_capture(params));

	usleep(waitMS * 1000);

	event_start = time_now();

	do {
		GP_LOG_D ("**** GH5: checking for new object...");
		C_PTP_REP (ptp_check_event (params));

		while (ptp_get_one_event(params, &event)) {
			switch (event.Code) {
			case 0xC101:
				ptp_panasonic_9401(params, event.Param1); // not sure if this is needed or what this does (following LUMIXTether)
			case 0xC107:
				//event_start = time_now(); // still working...
				break;
			case PTP_EC_PANASONIC_ObjectAdded:
			case PTP_EC_PANASONIC_ObjectAddedSDRAM: /* marcus: check if this works */
				newobject = event.Param1;
				C_PTP_REP (ptp_object_want (params, newobject, PTPOBJECT_OBJECTINFO_LOADED, &ob));

				if (ob->oi.ObjectFormat != PTP_OFC_Association)
					goto downloadfile;
				newobject = 0;
				/* new folder ... wait a bit longer */
				break;
			default:
				GP_LOG_D ("unexpected unhandled event Code %04x, Param 1 %08x", event.Code, event.Param1);
				break;
			}
		}
	}  while (waiting_for_timeout (&back_off_wait, event_start, 65000)); /* wait for 66 seconds after busy is no longer signaled */

downloadfile:

	usleep(50000); // it seems there can be an error if the object is accessed too soon

	path->name[0]='\0';
	path->folder[0]='\0';

	if (newobject != 0) /* NOTE: association add handled */
		return add_object_to_fs_and_path (camera, newobject, path, context);
	return GP_ERROR;
}

static int
camera_olympus_omd_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
{
	PTPParams	*params = &camera->pl->params;
	PTPContainer	event;
	uint32_t	newobject = 0;
	struct timeval	event_start;
	int	     	back_off_wait = 0;
	PTPPropertyValue propval;

	// clear out old events
	//C_PTP_REP (ptp_check_event (params));
	//while (ptp_get_one_event(params, &event));

	C_PTP_REP (ptp_getdevicepropvalue (params, PTP_DPC_OLYMPUS_CaptureTarget, &propval, PTP_DTC_UINT16));
	C_PTP_REP (ptp_olympus_omd_capture(params));

	usleep(100);

	event_start = time_now();
	do {
		C_PTP_REP (ptp_check_event (params));

		while (ptp_get_one_event(params, &event)) {
			switch (event.Code) {
			case PTP_EC_Olympus_ObjectAdded:
			case PTP_EC_Olympus_ObjectAdded_New:	/* seen in newer traces, https://github.com/gphoto/gphoto2/issues/310 */
			case PTP_EC_ObjectAdded:
				newobject = event.Param1;
				goto downloadfile;
			default:
				GP_LOG_D ("unexpected unhandled event Code %04x, Param 1 %08x", event.Code, event.Param1);
				break;
			}
		}
	}  while (waiting_for_timeout (&back_off_wait, event_start, 65000)); /* wait for 0.5 seconds after busy is no longer signaled */

downloadfile:

	path->name[0]='\0';
	path->folder[0]='\0';

	if (newobject != 0) /* FIXME: does not handle assocation adds I think */
		return add_object_to_fs_and_path (camera, newobject, path, context);
	return GP_ERROR;
}

static int
camera_sigma_fp_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
{
	PTPParams		*params = &camera->pl->params;
	unsigned char		*data = NULL;
	unsigned int		size = 0;
	CameraFile		*file;
	int			ret, tries;
	SIGMAFP_PictFileInfo2Ex	pictfileinfoex2;
	SIGMAFP_CaptureStatus	captstatus;

	C_PTP_REP (ptp_sigma_fp_getcapturestatus(params, 0, &captstatus));
	GP_LOG_D("status before snap 0x%04x", captstatus.status);

	C_PTP_REP (ptp_sigma_fp_snap(params, 1, 1));

	tries = 50;
	while (tries--) {
		C_PTP_REP (ptp_sigma_fp_getcapturestatus(params, 0, &captstatus));
		GP_LOG_D("trying ... status 0x%04x", captstatus.status);
		if ((captstatus.status & 0xf000) == 0x6000) {	/* failure */
			if (captstatus.status == 0x6001) {
				gp_context_error (context, _("Capture failed: No focus."));
				return GP_ERROR;
			}
			return GP_ERROR;
		}
		if ((captstatus.status & 0xf000) == 0x8000) {	/* success */
			break;
		}
		if (captstatus.status == 0x0002) break;	/* success ? */
		if (captstatus.status == 0x0005) break;	/* success */

		usleep(20*1000);

	}
	C_PTP_REP (ptp_sigma_fp_getpictfileinfo2(params, &pictfileinfoex2));

	C_PTP_REP (ptp_sigma_fp_getbigpartialpictfile(params, pictfileinfoex2.fileaddress, 0, pictfileinfoex2.filesize, &data, &size));

	C_PTP_REP (ptp_sigma_fp_clearimagedbsingle(params, captstatus.imageid));

	sprintf (path->name, "%s%s", pictfileinfoex2.name, pictfileinfoex2.fileext);
	strcpy (path->folder,"/");

	ret = gp_file_new (&file);
	if (ret != GP_OK) {
		free (data);
		return ret;
	}

	ret = gp_file_append (file, (char*)data+4, size-4);
	free (data);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}

	ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	ret = gp_filesystem_set_file_noop(camera->fs, path->folder, path->name, GP_FILE_TYPE_NORMAL, file, context);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	return GP_OK;

#if 0
	CameraFileInfo info;
	info.file.fields = GP_FILE_INFO_TYPE |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE | GP_FILE_INFO_MTIME;
	strcpy_mime (info.file.type, params->deviceinfo.VendorExtensionID, ob->oi.ObjectFormat);
	info.file.width		= ob->oi.ImagePixWidth;
	info.file.height	= ob->oi.ImagePixHeight;
	info.file.size		= size;
	info.file.mtime		= time(NULL);

	info.preview.fields = GP_FILE_INFO_TYPE |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE;
	strcpy_mime (info.preview.type, params->deviceinfo.VendorExtensionID, ob->oi.ThumbFormat);
	info.preview.width	= ob->oi.ThumbPixWidth;
	info.preview.height	= ob->oi.ThumbPixHeight;
	info.preview.size	= ob->oi.ThumbCompressedSize;
	GP_LOG_D ("setting fileinfo in fs");
	return gp_filesystem_set_info_noop(camera->fs, path->folder, path->name, info, context);
#endif
}


static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	PTPContainer		event;
	PTPParams		*params = &camera->pl->params;
	uint32_t		newobject = 0x0;
	int			done,tries;
	PTPObjectHandles	beforehandles;
	uint16_t		ptpres;

	/* adjust if we ever do sound or movie capture */
	if (type != GP_CAPTURE_IMAGE)
		return GP_ERROR_NOT_SUPPORTED;

	SET_CONTEXT_P(params, context);
	camera->pl->checkevents = TRUE;

	/* first, draing existing events if the caller did not do it. */
	while (ptp_get_one_event(params, &event)) {
		GP_LOG_D ("draining unhandled event Code %04x, Param 1 %08x", event.Code, event.Param1);
	}

	/* Do not use the enhanced capture methods for now. */
	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) && NIKON_BROKEN_CAP(params))
		goto fallback;

	/* 3rd gen style nikon capture, can do both sdram and card */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		 ptp_operation_issupported(params, PTP_OC_NIKON_InitiateCaptureRecInMedia)
	) {
		char buf[1024];
		int sdram = 0;
		int af = 1;

		if ((GP_OK != gp_setting_get("ptp2","capturetarget",buf)) || !strcmp(buf,"sdram"))
			sdram = 1;
		if ((GP_OK != gp_setting_get("ptp2","autofocus",buf)) || !strcmp(buf,"off"))
			af = 0;

		return camera_nikon_capture (camera, type, path, af, sdram, context);
	}

	/* 1st gen, 2nd gen nikon capture only go to SDRAM */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		(ptp_operation_issupported(params, PTP_OC_NIKON_InitiateCaptureRecInSdram) ||
		 ptp_operation_issupported(params, PTP_OC_NIKON_AfCaptureSDRAM)
	)) {
		int ret = GP_ERROR_NOT_SUPPORTED;
		char buf[1024];
		int af = 1;

		if ((GP_OK != gp_setting_get("ptp2","autofocus",buf)) || !strcmp(buf,"off"))
			af = 0;
		if ((GP_OK != gp_setting_get("ptp2","capturetarget",buf)) || !strcmp(buf,"sdram"))
			ret = camera_nikon_capture (camera, type, path, af, 1, context);
		if (ret != GP_ERROR_NOT_SUPPORTED)
			 return ret;
		/* for CARD capture and unsupported combinations, fall through */
	}

	if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_GP_OLYMPUS_OMD)
		return camera_olympus_omd_capture (camera, type, path, context);

	if (params->device_flags & DEVICE_FLAG_OLYMPUS_XML_WRAPPED)
		return camera_olympus_xml_capture (camera, type, path, context);

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_InitiateCaptureInMemory)
	) {
		return camera_canon_capture (camera, type, path, context);
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		(	ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease) ||
			ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn)
		)
	) {
		return camera_canon_eos_capture (camera, type, path, context);
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) &&
		ptp_operation_issupported(params, PTP_OC_SONY_SetControlDeviceB)
	) {
		return camera_sony_capture (camera, type, path, context);
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) &&
		ptp_operation_issupported(params, PTP_OC_SONY_QX_SetControlDeviceB)
	) {
		return camera_sony_qx_capture (camera, type, path, context);
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_FUJI) &&
		ptp_operation_issupported(params, PTP_OC_InitiateCapture)
	) {
		return camera_fuji_capture (camera, type, path, context);
	}

	if (    (params->deviceinfo.VendorExtensionID == PTP_VENDOR_PANASONIC) &&
		ptp_operation_issupported(params, PTP_OC_PANASONIC_InitiateCapture)
	) {
		return camera_panasonic_capture (camera, type, path, context);
	}

	if (    (params->deviceinfo.VendorExtensionID == PTP_VENDOR_GP_SIGMAFP) &&
		ptp_operation_issupported(params, PTP_OC_SIGMA_FP_Snap)
	) {
		return camera_sigma_fp_capture (camera, type, path, context);
	}

	if (!ptp_operation_issupported(params,PTP_OC_InitiateCapture)) {
		gp_context_error(context,
               	_("Sorry, your camera does not support generic capture"));
		return GP_ERROR_NOT_SUPPORTED;
	}

fallback:
	/* broken capture ... we detect what was captured using added objects
	 * via the getobjecthandles array. here get the before state */
	if ((params->deviceinfo.VendorExtensionID==PTP_VENDOR_NIKON) && NIKON_BROKEN_CAP(params))
		C_PTP (ptp_getobjecthandles (params, PTP_HANDLER_SPECIAL, 0x000000, 0x000000, &beforehandles));

	/* A capture may take longer than the standard 8 seconds.
	 * The G5 for instance does, or in dark rooms ...
	 * Even 16 seconds might not be enough. (Marcus)
	 */
	/* ptp_initiatecapture() returns immediately, only the event
	 * indicating that the capure has been completed may occur after
	 * few seconds. moving down the code. (kil3r)
	 */
	CR (gp_port_set_timeout (camera->port, capture_timeout));
	ptpres = LOG_ON_PTP_E (ptp_initiatecapture(params, 0x00000000, 0x00000000));
	/* the V1 reports general error to us, but has actually captured ... so just ignore GeneralError. */
	if ((ptpres != PTP_RC_OK) && (ptpres != PTP_RC_GeneralError)) {
		CR (gp_port_set_timeout (camera->port, normal_timeout));
		C_PTP (ptpres);
	}
	/* A word of comments is worth here.
	 * After InitiateCapture camera should report with ObjectAdded event
	 * all newly created objects. However there might be more than one
	 * newly created object. There a two scenarios here, which may occur
	 * both at the time.
	 * 1) InitiateCapture trigers capture of more than one object if the
	 * camera is in burst mode for example.
	 * 2) InitiateCapture creates a number of objects, but not all
	 * objects represents images. This happens when the camera creates a
	 * folder for newly captured image(s). This may happen with the
	 * fresh, formatted flashcard or in burs mode if the camera is
	 * configured to create a dedicated folder for a burst of pictures.
	 * The newly created folder (an association object) is reported
	 * before the images that are stored after its creation.
	 * Thus we set CameraFilePath to the path to last object reported by
	 * the camera.
	 */

	/* The Nikon way: Does not send AddObject event ... so try to detect it by checking what objects
	 * were added. */
	if ((params->deviceinfo.VendorExtensionID==PTP_VENDOR_NIKON) && NIKON_BROKEN_CAP(params)) {
		PTPObjectHandles	handles;

		tries = 5;
            	GP_LOG_D ("PTPBUG_NIKON_BROKEN_CAPTURE bug workaround");
		while (tries--) {
			unsigned int i;
			uint16_t ret = ptp_getobjecthandles (params, PTP_HANDLER_SPECIAL, 0x000000, 0x000000, &handles);
			if (ret != PTP_RC_OK)
				break;

			/* if (handles.n == params->handles.n)
			 *	continue;
			 * While this is a potential optimization, lets skip it for now.
			 */
			newobject = 0;
			for (i=0;i<handles.n;i++) {
				unsigned int 	j;
				PTPObject	*ob;

				/* look if we saw the objecthandle before capture */
				for (j=0;j<beforehandles.n;j++)
					if (beforehandles.Handler[j] == handles.Handler[i])
						break;
				if (j != beforehandles.n)
					continue;

				ret = ptp_object_want (params, handles.Handler[i], PTPOBJECT_OBJECTINFO_LOADED, &ob);
				if (ret != PTP_RC_OK) {
					GP_LOG_E ("object added, but not found?");
					continue;
				}
				/* A directory was added, like initial DCIM/100NIKON or so. */
				if (ob->oi.ObjectFormat == PTP_OFC_Association)
					continue;
				newobject = handles.Handler[i];
				/* we found a new file */
				break;
			}
			free (handles.Handler);
			if (newobject)
				break;
			C_PTP_REP (ptp_check_event (params));
			sleep(1);
		}
		free (beforehandles.Handler);
		if (!newobject)
            		GP_LOG_D ("PTPBUG_NIKON_BROKEN_CAPTURE no new file found after 5 seconds?!?");
		goto out;
	}

	CR (gp_port_set_timeout (camera->port, normal_timeout));

	/* The standard defined way ... wait for some capture related events. */
	/* The Nikon 1 series emits ObjectAdded occasionally after
	 * the CaptureComplete event, while others do it the other way
	 * round. Handle that case with some bitmask. */
	done = 0; tries = 20;
	while (done != 3) {
		uint16_t ret;

		C_PTP_REP (ptp_wait_event (params));

		if (!ptp_get_one_event(params, &event)) {
			usleep(1000);
			if (done & 1) /* only wait 20 rounds for objectadded */
				if (!tries--)
					break;
			continue;
		}
		GP_LOG_D ("event.Code is %04x / param %08x", event.Code, event.Param1);

		switch (event.Code) {
		case PTP_EC_ObjectRemoved:
			ptp_remove_object_from_cache(params, event.Param1);
			gp_filesystem_reset (camera->fs);
			break;
		case PTP_EC_ObjectAdded: {
			PTPObject	*ob;

			/* add newly created object to internal structures */
			ret = ptp_object_want (params, event.Param1, PTPOBJECT_OBJECTINFO_LOADED, &ob);
			if (ret != PTP_RC_OK)
				break;

			/* this might be just the folder add, ignore that. */
			if (ob->oi.ObjectFormat == PTP_OFC_Association) {
				/* new directory ... mark fs as to be refreshed */
				gp_filesystem_reset (camera->fs);
			} else {
				newobject = event.Param1;
				done |= 2;
				if (NO_CAPTURE_COMPLETE(params))
					done|=1;
			}
			break;
		}
		case PTP_EC_StoreFull:
			gp_context_error (context, _("Camera memorycard ran full during capture."));
			return GP_ERROR_NO_MEMORY;
		case PTP_EC_CaptureComplete:
			done |= 1;
			break;
		default:
			GP_LOG_D ("Received event 0x%04x, ignoring (please report).",event.Code);
			/* done = 1; */
			break;
		}
	}
out:
	/* clear path, so we get defined results even without object info */
	path->name[0]='\0';
	path->folder[0]='\0';

	if (newobject != 0) /* FIXME: check association handling */
		return add_object_to_fs_and_path (camera, newobject, path, context);
	return GP_ERROR;
}

static int
camera_trigger_canon_eos_capture (Camera *camera, GPContext *context)
{
	PTPParams		*params = &camera->pl->params;
	int			ret;
	PTPCanon_changes_entry	entry;
	int			back_off_wait = 0;
	uint32_t		result;
	struct timeval		focus_start;
	PTPDevicePropDesc	dpd;

	GP_LOG_D ("camera_trigger_canon_eos_capture");

	if (!params->eos_captureenabled)
		camera_prepare_capture (camera, context);
	else
		CR( camera_canon_eos_update_capture_target(camera, context, -1));

	/* Get the initial bulk set of event data, otherwise
	 * capture might return busy. */
	ptp_check_eos_events (params);
	while (ptp_get_one_eos_event (params, &entry))
		GP_LOG_D("discarding event type %d", entry.type);

	if (params->eos_camerastatus == 1)
		return GP_ERROR_CAMERA_BUSY;

	if (have_eos_prop(params, PTP_VENDOR_CANON, PTP_DPC_CANON_EOS_CaptureDestination)) {
                C_PTP (ptp_canon_eos_getdevicepropdesc (params, PTP_DPC_CANON_EOS_CaptureDestination, &dpd));
                if (dpd.CurrentValue.u32 == PTP_CANON_EOS_CAPTUREDEST_HD) {
			C_PTP (ptp_canon_eos_getdevicepropdesc (params, PTP_DPC_CANON_EOS_AvailableShots, &dpd));
			if (dpd.CurrentValue.u32 < 100) {
				/* Tell the camera we have enough free space on the PC */
				if (!params->uilocked)
					ptp_canon_eos_setuilock(params);
				LOG_ON_PTP_E (ptp_canon_eos_pchddcapacity(params, 0x0fffffff, 0x00001000, 0x00000001));
				if (!params->uilocked)
					ptp_canon_eos_resetuilock(params);
			}
		}
	}

	if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn)) {
		if (!is_canon_eos_m (params)) {
			/* Regular EOS */
			int 			manualfocus = 0, foundfocusinfo = 0;

			/* are we in manual focus mode ... value would be 3 */
			if (PTP_RC_OK == ptp_canon_eos_getdevicepropdesc (params, PTP_DPC_CANON_EOS_FocusMode, &dpd)) {
				if ((dpd.DataType == PTP_DTC_UINT16) && (dpd.CurrentValue.u16 == 3)) {
					manualfocus = 1;
					/* will do 1 pass through the focusing loop for good measure */
					GP_LOG_D("detected manual focus. skipping focus detection logic");
				}
			}
			ret = GP_OK;
			/* half press now - initiate focusing and wait for result */
			C_PTP_REP_MSG (ptp_canon_eos_remotereleaseon (params, 1, 0), _("Canon EOS Half-Press failed"));

			focus_start = time_now();
			do {
				int foundevents = 0;

				C_PTP_REP_MSG (ptp_check_eos_events (params), _("Canon EOS Get Changes failed"));
				while (ptp_get_one_eos_event (params, &entry)) {
					foundevents = 1;
					GP_LOG_D("focusing - read event type %d", entry.type);
					if (entry.type == PTP_CANON_EOS_CHANGES_TYPE_FOCUSINFO) {
						GP_LOG_D("focusinfo content: %s", entry.u.info);
						foundfocusinfo = 1;
						if (strstr(entry.u.info,"0000200")) {
							gp_context_error (context, _("Canon EOS Capture failed to release: Perhaps no focus?"));
							ret = GP_ERROR;
						}
					}
					if (	(entry.type == PTP_CANON_EOS_CHANGES_TYPE_PROPERTY) &&
						(entry.u.propid == PTP_DPC_CANON_EOS_FocusInfoEx)
					) {
						if (PTP_RC_OK == ptp_canon_eos_getdevicepropdesc (params, PTP_DPC_CANON_EOS_FocusInfoEx, &dpd)) {
							GP_LOG_D("focusinfo prop content: %s", dpd.CurrentValue.str);
							if (!strstr(dpd.CurrentValue.str,"select={}")) /* select={} means "no focus yet" */
								foundfocusinfo = 1;
							ptp_free_devicepropdesc (&dpd);
						}
					}
				}
				/* We found focus information, so half way pressing has finished! */
				if (foundfocusinfo)
					break;
				/* for manual focus, at least wait until we get events */
				if (manualfocus && foundevents)
					break;
				/* when doing manual focus, wait at most 0.1 seconds */
				if (manualfocus && (time_since (focus_start) >= 100))
					break;
			} while (waiting_for_timeout (&back_off_wait, focus_start, 2*1000)); /* wait 2 seconds for focus */

			if (!foundfocusinfo && !manualfocus) {
				GP_LOG_E("no focus info?\n");
			}
			if (ret != GP_OK) {
				C_PTP_REP_MSG (ptp_canon_eos_remotereleaseoff (params, 1), _("Canon EOS Half-Release failed"));
				return ret;
			}
			/* full press now */

			C_PTP_REP_MSG (ptp_canon_eos_remotereleaseon (params, 2, 0), _("Canon EOS Full-Press failed"));
			/* no event check between */
			/* full release now */
			C_PTP_REP_MSG (ptp_canon_eos_remotereleaseoff (params, 2), _("Canon EOS Full-Release failed"));
			ptp_check_eos_events (params);

			/* half release now */
			C_PTP_REP_MSG (ptp_canon_eos_remotereleaseoff (params, 1), _("Canon EOS Half-Release failed"));
		} else {
			/* Canon EOS M series */
			int button = 0, eos_m_focus_done = 0;

			C_PTP_REP_MSG (ptp_canon_eos_remotereleaseon (params, 3, 0), _("Canon EOS M Full-Press failed"));
			focus_start = time_now();
			/* check if the capture was successful (the result is reported as a set of OLCInfoChanged events) */
			do {
				ptp_check_eos_events (params);
				while (ptp_get_one_eos_event (params, &entry)) {
					GP_LOG_D ("entry type %04x", entry.type);
					if (entry.type == PTP_CANON_EOS_CHANGES_TYPE_UNKNOWN && entry.u.info && sscanf (entry.u.info, "Button %d", &button)) {
						GP_LOG_D ("Button %d", button);
						switch (button) {
							/* Indicates a successful Half-Press(?) on M2, where it
							 * would precede any other value (unless in MF mode).
							 * So skip it and look for another button reported */
							case 2:
							/* Reported in the self-timer mode during the delay
							 * period. May be followed by 1, 3 or 4 */
							case 7:
								continue;
							/* Full-Press successful */
							case 4:
								eos_m_focus_done = 1;
								break;
							/* 3 indicates a Full-Press fail on M2 */
							/* 1 is a "normal" state, reported after a release.
							   On M10 also indicates a Full-Press fail */
							default:
								eos_m_focus_done = 1;
								gp_context_error (context, _("Canon EOS M Capture failed: Perhaps no focus?"));
						}
						break;
					}
				}
			} while (!eos_m_focus_done && waiting_for_timeout (&back_off_wait, focus_start, 2*1000)); /* wait 2 seconds for focus */
			/* full release now (even if the press has failed) */
			C_PTP_REP_MSG (ptp_canon_eos_remotereleaseoff (params, 3), _("Canon EOS M Full-Release failed"));
			ptp_check_eos_events (params);
			/* NB: no error is returned in case of button == 7, which means
			 * the timer is still working, but no AF fail has been reported */
			if (button < 4)
				return GP_ERROR;
		}
	} else {
		C_PTP_REP_MSG (ptp_canon_eos_capture (params, &result),
			       _("Canon EOS Capture failed"));

		if ((result & 0x7000) == 0x2000) { /* also happened */
			gp_context_error (context, _("Canon EOS Capture failed: %x"), result);
			return translate_ptp_result (result);
		}
		GP_LOG_D ("result is %d", result);
		switch (result) {
		case 0: /* OK */
			break;
		case 1: gp_context_error (context, _("Canon EOS Capture failed to release: Perhaps no focus?"));
			return GP_ERROR;
		case 3: gp_context_error (context, _("Canon EOS Capture failed to release: Perhaps mirror up?"));
			return GP_ERROR;
		case 7: gp_context_error (context, _("Canon EOS Capture failed to release: Perhaps no more memory on card?"));
			return GP_ERROR_NO_MEMORY;
		case 8: gp_context_error (context, _("Canon EOS Capture failed to release: Card read-only?"));
			return GP_ERROR_NO_MEMORY;
		default:gp_context_error (context, _("Canon EOS Capture failed to release: Unknown error %d, please report."), result);
			return GP_ERROR;
		}
	}
	return GP_OK;
}

static int
camera_trigger_capture (Camera *camera, GPContext *context)
{
	PTPParams	*params = &camera->pl->params;
	uint16_t	ret;
	char		buf[1024];
	int		sdram = 0;
	int		af = 1;

	GP_LOG_D ("camera_trigger_capture");

	SET_CONTEXT_P(params, context);

	/* If there is no capturetarget set yet, the default is "sdram" */
	if (GP_OK != gp_setting_get("ptp2","capturetarget",buf))
		strcpy (buf, "sdram");

	if (!strcmp(buf,"sdram"))
		sdram = 1;

	if ((GP_OK != gp_setting_get("ptp2","autofocus",buf)) || !strcmp(buf,"off"))
		af = 0;

	GP_LOG_D ("Triggering capture to %s, autofocus=%d", buf, af);

	/* Nilon V and J seem to like that */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		!params->controlmode &&
		ptp_operation_issupported(params,PTP_OC_NIKON_ChangeCameraMode)
	) {
		ret = ptp_nikon_changecameramode (params, 1);
		/* FIXME: PTP_RC_NIKON_ChangeCameraModeFailed does not seem to be problematic */
		if (ret != PTP_RC_NIKON_ChangeCameraModeFailed)
			C_PTP_REP (ret);
		params->controlmode = 1;
	}

	/* On Nikon 1 series, the liveview must be enabled before capture works ... not on V1 which does not have it. */
	if (NIKON_1(params) && ptp_operation_issupported(params,PTP_OC_NIKON_StartLiveView)) {
		ret = ptp_nikon_start_liveview (params);
		if ((ret != PTP_RC_OK) && (ret != PTP_RC_DeviceBusy))
			C_PTP_REP_MSG(ret, _("Failed to enable liveview on a Nikon 1, but it is required for capture"));
		/* OK or busy, try to proceed ... */
	}

	/* Nikon 2 */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		ptp_operation_issupported(params, PTP_OC_NIKON_InitiateCaptureRecInMedia)
	) {
		/* If in liveview mode, we have to run non-af capture */
		int inliveview = 0;
		int tries;
		PTPPropertyValue propval;

		C_PTP_REP (ptp_check_event (params));
		C_PTP_REP (nikon_wait_busy (params, 100, 2000)); /* lets wait 2 seconds */
		C_PTP_REP (ptp_check_event (params));

		if (ptp_property_issupported (params, PTP_DPC_NIKON_LiveViewStatus)) {
			ret = ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewStatus, &propval, PTP_DTC_UINT8);
			if (ret == PTP_RC_OK)
				inliveview = propval.u8;

			if (inliveview) af = 0;
		}

		tries = 200;
		do {
			ret = ptp_nikon_capture2 (params, af, sdram);
			if (ret == PTP_RC_OK)
				break;

			/* Nikon 1 ... if af is 0, it reports PTP_RC_NIKON_InvalidStatus */
			if (!af && ((ret == PTP_RC_NIKON_InvalidStatus))) {
				ret = ptp_nikon_capture2 (params, 1, sdram);
				if (ret == PTP_RC_OK)
					break;
			}

			/* busy means wait and the invalid status might go away */
			if ((ret != PTP_RC_DeviceBusy) && (ret != PTP_RC_NIKON_InvalidStatus) && (ret != 0xa207)) /* a207 on Nikon D850 */
				return translate_ptp_result (ret);

			usleep(2000);
			/* sleep a bit perhaps ? or check events? */
		} while (tries--);

		/* busyness will be reported during the whole of the exposure time. */
		C_PTP_REP (nikon_wait_busy (params, 100, 1000*1000)); /* lets wait 1000 seconds (D780 can do 900 second exposures) */
		return GP_OK;
	}

	/* Nikon */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		(ptp_operation_issupported(params, PTP_OC_NIKON_InitiateCaptureRecInSdram) ||
		 ptp_operation_issupported(params, PTP_OC_NIKON_AfCaptureSDRAM)
		)
		&& sdram
	) {
		/* If in liveview mode, we have to run non-af capture */
		int inliveview = 0;
		PTPPropertyValue propval;

		C_PTP_REP (ptp_check_event (params));
		C_PTP_REP (nikon_wait_busy (params, 20, 2000));
		C_PTP_REP (ptp_check_event (params));

		if (ptp_property_issupported (params, PTP_DPC_NIKON_LiveViewStatus)) {
			ret = ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewStatus, &propval, PTP_DTC_UINT8);
			if (ret == PTP_RC_OK)
				inliveview = propval.u8;
		}

		do {
			if (!inliveview && af && ptp_operation_issupported (params,PTP_OC_NIKON_AfCaptureSDRAM))
				ret = ptp_nikon_capture_sdram (params);
			else
				ret = ptp_nikon_capture (params, 0xffffffff);
			if ((ret != PTP_RC_OK) && (ret != PTP_RC_DeviceBusy))
				return translate_ptp_result (ret);
		} while (ret == PTP_RC_DeviceBusy);

		C_PTP_REP (nikon_wait_busy (params, 100, 1000*1000)); /* lets wait 1000 seconds (D780 can do 900 second exposures) */
		return GP_OK;
	}

	/* Canon EOS */
	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
	     (	ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease) ||
	     	ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn))
	)
		return camera_trigger_canon_eos_capture (camera, context);

	/* Canon Powershot */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_InitiateCaptureInMemory)
	) {
		uint16_t xmode;
		/*int viewfinderwason = 0;*/
		PTPPropertyValue propval;

		if (!ptp_property_issupported(params, PTP_DPC_CANON_FlashMode)) {
			/* did not call --set-config capture=on, do it for user */
			CR (camera_prepare_capture (camera, context));
			if (!ptp_property_issupported(params, PTP_DPC_CANON_FlashMode)) {
				gp_context_error (context,
				_("Sorry, initializing your camera did not work. Please report this."));
				return GP_ERROR_NOT_SUPPORTED;
			}
		}

		if (ptp_property_issupported(params, PTP_DPC_CANON_CaptureTransferMode)) {
			if (sdram)
				propval.u16 = xmode = CANON_TRANSFER_MEMORY;
			else
				propval.u16 = xmode = CANON_TRANSFER_CARD;

			if (xmode == CANON_TRANSFER_CARD) {
				PTPStorageIDs storageids;

				ret = ptp_getstorageids(params, &storageids);
				if (ret == PTP_RC_OK) {
					unsigned int k, stgcnt = 0;

					for (k=0;k<storageids.n;k++) {
						if (!(storageids.Storage[k] & 0xffff)) continue;
						if (storageids.Storage[k] == 0x80000001) continue;
						stgcnt++;
					}
					if (!stgcnt) {
						GP_LOG_D ("Assuming no CF card present - switching to MEMORY Transfer.");
						propval.u16 = xmode = CANON_TRANSFER_MEMORY;
					}
					free (storageids.Storage);
				}
			}
			LOG_ON_PTP_E (ptp_setdevicepropvalue(params, PTP_DPC_CANON_CaptureTransferMode, &propval, PTP_DTC_UINT16));
		}

		if (params->canon_viewfinder_on) { /* disable during capture ... reenable later on. */
			C_PTP_REP_MSG (ptp_canon_viewfinderoff (params),
				       _("Canon disable viewfinder failed"));
			/*viewfinderwason = 1;*/
			params->canon_viewfinder_on = 0;
		}

	#if 0
		/* FIXME: For now, to avoid flash during debug */
		propval.u8 = 0;
		ret = ptp_setdevicepropvalue(params, PTP_DPC_CANON_FlashMode, &propval, PTP_DTC_UINT8);
	#endif
		while (1) {
			ret = ptp_canon_initiatecaptureinmemory (params);
			if (ret == PTP_RC_OK)
				break;
			if (ret == PTP_RC_DeviceBusy) {
				GP_LOG_D ("Canon Powershot busy ... retrying...");
				gp_context_idle (context);
				ptp_check_event (params);
				usleep(10000); /* 10 ms  ... fixme: perhaps experimental backoff? */
				continue;
			}
			C_PTP_REP_MSG (ret, _("Canon Capture failed"));
		}
		GP_LOG_D ("Canon Powershot capture triggered...");
		return GP_OK;
	}

	/* Sony Alpha */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) &&
		ptp_operation_issupported(params, PTP_OC_SONY_SetControlDeviceB)
	) {
		PTPPropertyValue	propval;
		struct timeval		event_start;
		PTPContainer		event;
		PTPDevicePropDesc	dpd;

		/* half-press */
		propval.u16 = 2;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_AutoFocus, &propval, PTP_DTC_UINT16));

		/* full-press */
		propval.u16 = 2;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_Capture, &propval, PTP_DTC_UINT16));

		/* Wait for focus only in automatic focus mode */
		C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_FocusMode, &dpd));
		if (dpd.CurrentValue.u16 != 1) { /* 1 is Manual .. no need to wait there for focusing */
			/* Now hold down the shutter button for a bit. We probably need to hold it as long as it takes to
			* get focus, indicated by the 0xD213 property. But hold it for at most 1 second.
			*/

			GP_LOG_D ("holding down shutterbutton");
			event_start = time_now();
			do {
				/* needed on older cameras like the a58, check for events ... */
				C_PTP (ptp_check_event (params));
				if (ptp_get_one_event(params, &event)) {
					GP_LOG_D ("during event.code=%04x Param1=%08x", event.Code, event.Param1);
					if (	(event.Code == PTP_EC_Sony_PropertyChanged) &&
						(event.Param1 == PTP_DPC_SONY_FocusFound)
					) {
						GP_LOG_D ("SONY FocusFound change received, 0xd213... ending press");
						break;
					}
				}

				/* Alternative code in case we miss the event */

				C_PTP (ptp_sony_getalldevicepropdesc (params)); /* avoid caching */
				C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_FocusFound, &dpd));
				GP_LOG_D ("DEBUG== 0xd213 after shutter press = %d", dpd.CurrentValue.u8);
				/* if prop 0xd213 = 2 or 3 (for rx0), the focus seems to be achieved */
				if (dpd.CurrentValue.u8 == 2 || dpd.CurrentValue.u8 == 3) {
					GP_LOG_D ("SONY Property change seen, 0xd213... ending press");
					break;
				}
			} while (time_since (event_start) < 1000);
		}
		ptp_free_devicepropdesc(&dpd);
		GP_LOG_D ("releasing shutterbutton");

		/* release full-press */
		propval.u16 = 1;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_Capture, &propval, PTP_DTC_UINT16));

		/* release half-press */
		propval.u16 = 1;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_AutoFocus, &propval, PTP_DTC_UINT16));

		return GP_OK;
	}

	/* Sony QX */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) &&
		ptp_operation_issupported(params, PTP_OC_SONY_QX_SetControlDeviceB)
	) {
		PTPPropertyValue	propval;
		struct timeval		event_start;
		PTPContainer		event;
		PTPDevicePropDesc	dpd;

		/* half-press */
		propval.u16 = 2;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_QX_AutoFocus, &propval, PTP_DTC_UINT16));

		/* full-press */
		propval.u16 = 2;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_QX_Capture, &propval, PTP_DTC_UINT16));

			GP_LOG_D ("holding down shutterbutton for 2 seconds");
			event_start = time_now();
			do {
				/* needed on older cameras like the a58, check for events ... */
				C_PTP (ptp_check_event (params));
				if (ptp_get_one_event(params, &event)) {
					GP_LOG_D ("during event.code=%04x Param1=%08x", event.Code, event.Param1);
				}

				/* Alternative code in case we miss the event */

				C_PTP (ptp_sony_qx_getalldevicepropdesc (params)); /* avoid caching */
				C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_FocusFound, &dpd));
			} while (time_since (event_start) < 2000);

		GP_LOG_D ("releasing shutterbutton");

		/* release full-press */
		propval.u16 = 1;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_QX_Capture, &propval, PTP_DTC_UINT16));

		/* release half-press */
		propval.u16 = 1;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_QX_AutoFocus, &propval, PTP_DTC_UINT16));

		return GP_OK;
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_FUJI) &&
		ptp_operation_issupported(params, PTP_OC_InitiateCapture)
	) {
		PTPPropertyValue	propval;

		/* focus */
		propval.u16 = 0x0200;
		C_PTP_REP (ptp_setdevicepropvalue (params, 0xd208, &propval, PTP_DTC_UINT16));
		C_PTP_REP(ptp_initiatecapture(params, 0x00000000, 0x00000000));

		/* poll camera until it is ready */
		propval.u16 = 0x0001;
		while (propval.u16 == 0x0001) {
			C_PTP_REP (ptp_getdevicepropvalue (params, PTP_DPC_FUJI_AFStatus, &propval, PTP_DTC_UINT16));
		}

		/* shoot */
		propval.u16 = 0x0304;
		C_PTP_REP (ptp_setdevicepropvalue (params, 0xd208, &propval, PTP_DTC_UINT16));
		C_PTP_REP(ptp_initiatecapture(params, 0x00000000, 0x00000000));

		/* poll camera until it is ready */
		propval.u16 = 0x0000;
		while (propval.u16 == 0x0000) {
			usleep(1000);
			C_PTP_REP (ptp_getdevicepropvalue (params, PTP_DPC_FUJI_CurrentState, &propval, PTP_DTC_UINT16));
		}
		return GP_OK;
	}

#if 0
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease)
	) {
		return camera_canon_eos_capture (camera, type, path, context);
	}
#endif

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_PANASONIC) &&
		ptp_operation_issupported(params, PTP_OC_PANASONIC_InitiateCapture)
	) {
		PTPContainer	event;

		// Fetch and discard any pending events.
		C_PTP_REP (ptp_check_event (params));
		while (ptp_get_one_event(params, &event))
			; // Do nothing with the events

		// Trigger the actual capture.
		return translate_ptp_result(ptp_panasonic_capture(params));
	}

	if (!ptp_operation_issupported(params,PTP_OC_InitiateCapture)) {
		gp_context_error(context,
               	_("Sorry, your camera does not support generic capture"));
		return GP_ERROR_NOT_SUPPORTED;
	}
	C_PTP_REP (ptp_initiatecapture(params, 0x00000000, 0x00000000));
	return GP_OK;
}

static int
camera_wait_for_event (Camera *camera, int timeout,
		       CameraEventType *eventtype, void **eventdata,
		       GPContext *context) {
	PTPContainer	event;
	PTPParams	*params = &camera->pl->params;
	uint32_t	newobject = 0x0;
	CameraFilePath	*path;
	uint16_t	ret;
	struct timeval	event_start;
	CameraFile	*file;
	CameraFileInfo	info;
	char		*ximage, *mime;
	int		back_off_wait = 0;
	int		oldtimeout;

	SET_CONTEXT(camera, context);
	GP_LOG_D ("waiting for events timeout %d ms", timeout);
	memset (&event, 0, sizeof(event));
	*eventtype = GP_EVENT_TIMEOUT;
	*eventdata = NULL;

	if (params->device_flags & DEVICE_FLAG_OLYMPUS_XML_WRAPPED) {
		GP_LOG_D ("olympus setcameracontrolmode 2\n");
		C_PTP_REP (ptp_olympus_setcameracontrolmode (params, 2));
	}

	event_start = time_now();
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
	        (ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease) ||
	     	 ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn))
	) {

		if (!params->eos_captureenabled)
			camera_prepare_capture (camera, context);
		do {
			PTPCanon_changes_entry	entry;

			/* keep device alive */
			if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_KeepDeviceOn)) C_PTP (ptp_canon_eos_keepdeviceon (params));

			C_PTP_REP_MSG (ptp_check_eos_events (params),
				       _("Canon EOS Get Changes failed"));
			while (ptp_get_one_eos_event (params, &entry)) {
				back_off_wait = 0;
				GP_LOG_D ("entry type %04x", entry.type);
				switch (entry.type) {
				case PTP_CANON_EOS_CHANGES_TYPE_OBJECTTRANSFER:
					GP_LOG_D ("Found new object! OID 0x%x, name %s", (unsigned int)entry.u.object.oid, entry.u.object.oi.Filename);
					free (entry.u.object.oi.Filename);

					newobject = entry.u.object.oid;

					C_MEM (path = malloc(sizeof(CameraFilePath)));
					path->name[0]='\0';
					strcpy (path->folder,"/");
					ret = gp_file_new(&file);
					if (ret!=GP_OK) return ret;
					sprintf (path->name, "capt%04d.", params->capcnt++);
					if ((entry.u.object.oi.ObjectFormat == PTP_OFC_CANON_CRW) || (entry.u.object.oi.ObjectFormat == PTP_OFC_CANON_CRW3)) {
						strcat(path->name, "cr2");
						gp_file_set_mime_type (file, GP_MIME_CRW);
						mime = GP_MIME_CRW;
					} else if (entry.u.object.oi.ObjectFormat == PTP_OFC_CANON_CR3) {
						strcat(path->name, "cr3");
						gp_file_set_mime_type (file, GP_MIME_CR3);
						mime = GP_MIME_CR3;
					} else {
						strcat(path->name, "jpg");
						gp_file_set_mime_type (file, GP_MIME_JPEG);
						mime = GP_MIME_JPEG;
					}
					gp_file_set_mtime (file, time(NULL));

					GP_LOG_D ("trying to get object size=0x%lx", (unsigned long)entry.u.object.oi.ObjectCompressedSize);

#define BLOBSIZE 1*1024*1024
					/* Trying to read this in 1 block might be the cause of crashes of newer EOS */
					{
						uint32_t	offset = 0;

						while (offset < entry.u.object.oi.ObjectCompressedSize) {
							uint32_t	xsize = entry.u.object.oi.ObjectCompressedSize - offset;
							unsigned char	*yimage = NULL;

							if (xsize > BLOBSIZE)
								xsize = BLOBSIZE;
							C_PTP_REP (ptp_getpartialobject (params, newobject, offset, xsize, &yimage, &xsize));
							gp_file_append (file, (char*)yimage, xsize);
							free (yimage);
							offset += xsize;
						}
					}
					/*old C_PTP_REP (ptp_canon_eos_getpartialobject (params, newobject, 0, oi.ObjectCompressedSize, &ximage));*/
					/* C_PTP_REP (ptp_canon_eos_getpartialobject (params, newobject, 0, entry.u.object.oi.ObjectCompressedSize, (unsigned char**)&ximage));*/
#undef BLOBSIZE
					C_PTP_REP (ptp_canon_eos_transfercomplete (params, newobject));

/*
					ret = gp_file_set_data_and_size(file, (char*)ximage, entry.u.object.oi.ObjectCompressedSize);
					if (ret != GP_OK) {
						gp_file_free (file);
						return ret;
					}
*/

					ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
					if (ret != GP_OK) {
						gp_file_free (file);
						return ret;
					}
					ret = gp_filesystem_set_file_noop(camera->fs, path->folder, path->name, GP_FILE_TYPE_NORMAL, file, context);
					if (ret != GP_OK) {
						gp_file_free (file);
						return ret;
					}
					memset(&info, 0, sizeof(info));
					/* We also get the fs info for free, so just set it */
					info.file.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_SIZE | GP_FILE_INFO_MTIME;
					strcpy (info.file.type, mime);
					info.file.size		= entry.u.object.oi.ObjectCompressedSize;
					info.file.mtime		= time(NULL);

					gp_filesystem_set_info_noop(camera->fs, path->folder, path->name, info, context);
					*eventtype = GP_EVENT_FILE_ADDED;
					*eventdata = path;
					/* We have now handed over the file, disclaim responsibility by unref. */
					gp_file_unref (file);
					return GP_OK;
				case PTP_CANON_EOS_CHANGES_TYPE_OBJECTCONTENT_CHANGE: {
					PTPObject *ob;

					GP_LOG_D ("Found object content changed! OID 0x%x", (unsigned int)entry.u.object.oid);
					newobject = entry.u.object.oid;
					/* It might have gone away in the meantime */
					if (PTP_RC_OK != ptp_object_want(params, newobject, PTPOBJECT_OBJECTINFO_LOADED, &ob))
						break;
					debug_objectinfo(params, newobject, &ob->oi);

					C_MEM (path = malloc(sizeof(CameraFilePath)));
					path->name[sizeof(path->name)-1] = '\0';
					strncpy  (path->name, ob->oi.Filename, sizeof (path->name)-1);

					sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)ob->oi.StorageID);
					get_folder_from_handle (camera, ob->oi.StorageID, ob->oi.ParentObject, path->folder);
					/* ob might now be invalid! */
					/* delete last / or we get confused later. */
					path->folder[ strlen(path->folder)-1 ] = '\0';

					*eventtype = GP_EVENT_FILE_CHANGED;
					*eventdata = path;
					return GP_OK;
					}
				case PTP_CANON_EOS_CHANGES_TYPE_OBJECTINFO:
				case PTP_CANON_EOS_CHANGES_TYPE_OBJECTINFO_CHANGE: {
					PTPObject	*ob;

					/* just add it to the filesystem, and return in CameraPath */
					GP_LOG_D ("Found new objectinfo! OID 0x%x, name %s", (unsigned int)entry.u.object.oid, entry.u.object.oi.Filename);
					if (	(entry.type == PTP_CANON_EOS_CHANGES_TYPE_OBJECTINFO_CHANGE) &&
						(PTP_RC_OK != ptp_object_find (params, entry.u.object.oid, &ob))
					) {
						GP_LOG_D ("not found in cache, assuming deleted already.");
						break;
					}
					newobject = entry.u.object.oid;
					C_MEM (path = malloc(sizeof(CameraFilePath)));
					ret = add_object_to_fs_and_path (camera, newobject, path, context);
					free (entry.u.object.oi.Filename);
					if (ret != GP_OK) {
						free (path);
						return ret;
					}
					if (entry.u.object.oi.ObjectFormat == PTP_OFC_Association) {	/* not sure if we would get folder changed */
						*eventtype = GP_EVENT_FOLDER_ADDED;
						gp_filesystem_reset (camera->fs);
					} else {
						*eventtype = (entry.type == PTP_CANON_EOS_CHANGES_TYPE_OBJECTINFO) ? GP_EVENT_FILE_ADDED : GP_EVENT_FILE_CHANGED;
						if (*eventtype == GP_EVENT_FILE_CHANGED) {
							ob->flags &= ~PTPOBJECT_OBJECTINFO_LOADED;
							C_PTP_REP (ptp_object_want (params, newobject, PTPOBJECT_OBJECTINFO_LOADED, &ob));
							gp_filesystem_set_info_dirty (camera->fs, path->folder, path->name, context);
						}
					}
					*eventdata = path;
					return GP_OK;
				}
				case PTP_CANON_EOS_CHANGES_TYPE_PROPERTY: {
					char			*name, *content;
					PTPDevicePropDesc	dpd;

					*eventtype = GP_EVENT_UNKNOWN;
					if (PTP_DPC_CANON_EOS_FocusInfoEx == entry.u.propid) {
						if (PTP_RC_OK == ptp_canon_eos_getdevicepropdesc (params, PTP_DPC_CANON_EOS_FocusInfoEx, &dpd)) {
							C_MEM (*eventdata = malloc(strlen("FocusInfo ")+strlen(dpd.CurrentValue.str)+1));
							sprintf (*eventdata, "FocusInfo %s", dpd.CurrentValue.str);
							ptp_free_devicepropdesc (&dpd);
							return GP_OK;
						}
					}
					/* cached devprop should have been flushed I think... */
					C_PTP_REP (ptp_canon_eos_getdevicepropdesc (params, entry.u.propid, &dpd));

					dpd.DevicePropertyCode = entry.u.propid;
					ret = camera_lookup_by_property(camera, &dpd, &name, &content, context);
					if (ret == GP_OK) {
						C_MEM (*eventdata = malloc(strlen("PTP Property 0123 changed, \"\" to \"\"")+strlen(name)+1+strlen(content?content:"")));
						sprintf (*eventdata, "PTP Property %04x changed, \"%s\" to \"%s\"", entry.u.propid, name, content?content:"");
						free (name);
						free (content);
					} else {
						C_MEM (*eventdata = malloc(strlen("PTP Property 0123 changed")+1));
						sprintf (*eventdata, "PTP Property %04x changed", entry.u.propid);
					}
					ptp_free_devicepropdesc (&dpd);
					return GP_OK;
				}
				case PTP_CANON_EOS_CHANGES_TYPE_CAMERASTATUS:
					/* if we do capture stuff, camerastatus will turn to 0 when done */
					if (entry.u.status == 0) {
						*eventtype = GP_EVENT_CAPTURE_COMPLETE;
						*eventdata = NULL;
					} else {
						*eventtype = GP_EVENT_UNKNOWN;
						C_MEM (*eventdata = malloc(strlen("Camera Status 123456789012345")+1));
						sprintf (*eventdata, "Camera Status %d", entry.u.status);
					}
					return GP_OK;
				case PTP_CANON_EOS_CHANGES_TYPE_FOCUSINFO:
					*eventtype = GP_EVENT_UNKNOWN;
					C_MEM (*eventdata = malloc(strlen("Focus Info 12345678901234567890123456789")+1));
					sprintf (*eventdata, "Focus Info %s", entry.u.info);
					return GP_OK;
				case PTP_CANON_EOS_CHANGES_TYPE_FOCUSMASK:
					*eventtype = GP_EVENT_UNKNOWN;
					C_MEM (*eventdata = malloc(strlen("Focus Mask 12345678901234567890123456789")+1));
					sprintf (*eventdata, "Focus Mask %s", entry.u.info);
					return GP_OK;
				case PTP_CANON_EOS_CHANGES_TYPE_UNKNOWN:
					/* only return if interesting stuff happened */
					if (entry.u.info) {
						*eventtype = GP_EVENT_UNKNOWN;
						*eventdata = entry.u.info; /* take over the allocated string allocation ownership */
						return GP_OK;
					}
					/* continue otherwise */
					break;
				case PTP_CANON_EOS_CHANGES_TYPE_OBJECTREMOVED:
					ptp_remove_object_from_cache(params, entry.u.object.oid);
					gp_filesystem_reset (camera->fs);
					*eventtype = GP_EVENT_UNKNOWN;
					C_MEM (*eventdata = malloc(strlen("Object Removed")+1));
					sprintf (*eventdata, "ObjectRemoved");
					return GP_OK;
				default:
					GP_LOG_D ("Unhandled EOS event 0x%04x", entry.type);
					break;
				}
			}
			gp_context_idle (context);
		} while (waiting_for_timeout (&back_off_wait, event_start, timeout));

		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_CheckEvent)
	) {
		while (1) {
			C_PTP_REP (ptp_check_event (params));
			if (ptp_get_one_event(params, &event)) {
				GP_LOG_D ("canon event: nparam=0x%X, C=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", event.Nparam,event.Code,event.Transaction_ID, event.Param1, event.Param2, event.Param3);
				switch (event.Code) {
				case PTP_EC_CANON_RequestObjectTransfer: {
					PTPObjectInfo	oi;

					newobject = event.Param1;
					GP_LOG_D ("PTP_EC_CANON_RequestObjectTransfer, object handle=0x%X.",newobject);
					/* FIXME: handle multiple images (as in BurstMode) */
					C_PTP (ptp_getobjectinfo (params, newobject, &oi));

					if (oi.ParentObject != 0) {
						C_MEM (path = malloc (sizeof(CameraFilePath)));
						ret = add_object_to_fs_and_path (camera, newobject, path, context);
						if (ret != GP_OK) {
							ptp_free_objectinfo (&oi);
							free (path);
							return ret;
						}
					} else {
						C_MEM (path = malloc (sizeof(CameraFilePath)));
						sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx",(unsigned long)oi.StorageID);
						sprintf (path->name, "capt%04d.jpg", params->capcnt++);
						add_objectid_and_upload (camera, path, context, newobject, &oi);
					}
					ptp_free_objectinfo (&oi);
					*eventdata = path;
					*eventtype = GP_EVENT_FILE_ADDED;
					return GP_OK;
				}
				case PTP_EC_CANON_ShutterButtonPressed0:
				/*case PTP_EC_CANON_ShutterButtonPressed1: This seems to be sent without a press on S3 IS, likely some other event reason */
				{
					C_MEM (path = malloc(sizeof(CameraFilePath)));
					ret = camera_canon_capture (camera, GP_CAPTURE_IMAGE, path, context);
					if (ret != GP_OK) {
						free (path);
						break;
					}
					*eventdata = path;
					*eventtype = GP_EVENT_FILE_ADDED;
					return GP_OK;
				}
				default:
					break;
				}
				goto handleregular;
			}
			if (time_since (event_start) >= timeout)
				break;
			gp_context_idle (context);
		}
		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		ptp_operation_issupported(params, PTP_OC_NIKON_GetEvent)
	) {
		do {
			C_PTP_REP (ptp_check_event (params));
			while (ptp_get_one_event (params, &event)) {
				back_off_wait = 0;
				GP_LOG_D ("event.Code is %x / param %lx", event.Code, (unsigned long)event.Param1);
				switch (event.Code) {
				case PTP_EC_ObjectAdded: {
					PTPObject	*ob;
					uint16_t	ofc;

					if (!event.Param1 || (event.Param1 == 0xffff0001))
						goto downloadnow;

#if 0
					/* if we have the object already loaded, no need to add it here */
					/* but in dual mode capture at empty startup we can
					 * encounter that the second image is not loaded */
					if (PTP_RC_OK == ptp_object_find(params, event.Param1, &ob))
						continue;
#endif

					ret = ptp_object_want (params, event.Param1, PTPOBJECT_OBJECTINFO_LOADED, &ob);
					if (ret != PTP_RC_OK) {
						*eventtype = GP_EVENT_UNKNOWN;
						C_MEM (*eventdata = strdup ("object added not found (already deleted)"));
						break;
					}
					debug_objectinfo(params, event.Param1, &ob->oi);

					C_MEM (path = malloc(sizeof(CameraFilePath)));
					path->name[0]='\0';
					path->folder[0]='\0';

					ofc = ob->oi.ObjectFormat;

					if (ob->oi.StorageID == 0) {
						/* We would always get the same filename,
						 * which will confuse the frontends */
						if (strstr(ob->oi.Filename,".NEF"))
							sprintf (path->name, "capt%04d.nef", params->capcnt++);
						else
							sprintf (path->name, "capt%04d.jpg", params->capcnt++);
						free (ob->oi.Filename);
						C_MEM (ob->oi.Filename = strdup (path->name));
						strcpy (path->folder,"/");
						goto downloadnow;
					}

					CR (add_object_to_fs_and_path (camera, event.Param1, path, context));

					if (ofc == PTP_OFC_Association) { /* new folder! */
						*eventtype = GP_EVENT_FOLDER_ADDED;
						*eventdata = path;
						gp_filesystem_reset (camera->fs); /* FIXME: implement more lightweight folder add */
						/* if this was the last current event ... stop and return the folder add */
						return GP_OK;
					} else {
						*eventtype = GP_EVENT_FILE_ADDED;
						*eventdata = path;
						return GP_OK;
					}
					break;
				}
				case PTP_EC_Nikon_ObjectAddedInSDRAM: {
					PTPObjectInfo	oi;
downloadnow:
					newobject = event.Param1;
					if (!newobject) newobject = 0xffff0001;
					ret = ptp_getobjectinfo (params, newobject, &oi);
					if (ret != PTP_RC_OK)
						continue;
					debug_objectinfo(params, newobject, &oi);
					C_MEM (path = malloc(sizeof(CameraFilePath)));
					path->name[0]='\0';
					strcpy (path->folder,"/");
					ret = gp_file_new(&file);
					if (ret!=GP_OK) return ret;
					if (oi.ObjectFormat != PTP_OFC_EXIF_JPEG) {
						GP_LOG_D ("raw? ofc is 0x%04x, name is %s", oi.ObjectFormat,oi.Filename);
						sprintf (path->name, "capt%04d.nef", params->capcnt++);
						gp_file_set_mime_type (file, "image/x-nikon-nef"); /* FIXME */
					} else {
						sprintf (path->name, "capt%04d.jpg", params->capcnt++);
						gp_file_set_mime_type (file, GP_MIME_JPEG);
					}
					gp_file_set_mtime (file, time(NULL));

					GP_LOG_D ("trying to get object size=0x%lx", (unsigned long)oi.ObjectCompressedSize);
					C_PTP_REP (ptp_getobject (params, newobject, (unsigned char**)&ximage));
					ret = gp_file_set_data_and_size(file, (char*)ximage, oi.ObjectCompressedSize);
					if (ret != GP_OK) {
						ptp_free_objectinfo (&oi);
						gp_file_free (file);
						return ret;
					}
					ptp_free_objectinfo (&oi);
					ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
					if (ret != GP_OK) {
						gp_file_free (file);
						return ret;
					}
					ret = gp_filesystem_set_file_noop(camera->fs, path->folder, path->name, GP_FILE_TYPE_NORMAL, file, context);
					if (ret != GP_OK) {
						gp_file_free (file);
						return ret;
					}
					*eventtype = GP_EVENT_FILE_ADDED;
					*eventdata = path;
					/* We have now handed over the file, disclaim responsibility by unref. */
					gp_file_unref (file);
					return GP_OK;
				}
				case PTP_EC_Nikon_CaptureCompleteRecInSdram:
				case PTP_EC_CaptureComplete:
					if (params->inliveview) {
						GP_LOG_D ("Capture complete ... restarting liveview");
						ret = ptp_nikon_start_liveview (params);
					}
					*eventtype = GP_EVENT_CAPTURE_COMPLETE;
					*eventdata = NULL;
					return GP_OK;
				default:
					goto handleregular;
				}
			}
			gp_context_idle (context);
		} while (waiting_for_timeout (&back_off_wait, event_start, timeout));

		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) &&
		ptp_operation_issupported(params, PTP_OC_SONY_SetControlDeviceB)
	) {
		PTPObjectInfo		oi;
		PTPDevicePropDesc	dpd;

		do {
			/* Check if the captured image counter was bumped before checking events to
			 * avoid the timeout ... */

			/* Check if there are pending images for download. If yes, synthesize a FILE ADDED event */
			C_PTP (ptp_sony_getalldevicepropdesc (params)); /* avoid caching */
			C_PTP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_ObjectInMemory, &dpd));
			GP_LOG_D ("DEBUG== 0xd215 after capture = %d", dpd.CurrentValue.u16);

			/* if prop 0xd215 is above 0x8000, the object in RAM is available at location 0xffffc001 */
			if (dpd.CurrentValue.u16 > 0x8000) {
				GP_LOG_D ("SONY ObjectInMemory count change seen, retrieving file");

				newobject = 0xffffc001;
				ret = ptp_getobjectinfo (params, newobject, &oi);
				if (ret != PTP_RC_OK)
					goto sonyout;
				debug_objectinfo(params, newobject, &oi);
				C_MEM (path = malloc(sizeof(CameraFilePath)));
				path->name[0]='\0';
				strcpy (path->folder,"/");
				ret = gp_file_new(&file);
				if (ret != GP_OK) {
					ptp_free_devicepropdesc (&dpd);
					ptp_free_objectinfo (&oi);
					return ret;
				}
				if (oi.ObjectFormat != PTP_OFC_EXIF_JPEG) {
					GP_LOG_D ("raw? ofc is 0x%04x, name is %s", oi.ObjectFormat,oi.Filename);
					sprintf (path->name, "capt%04d.arw", params->capcnt++);
					gp_file_set_mime_type (file, "image/x-sony-arw"); /* FIXME */
				} else {
					sprintf (path->name, "capt%04d.jpg", params->capcnt++);
					gp_file_set_mime_type (file, GP_MIME_JPEG);
				}
				gp_file_set_mtime (file, time(NULL));

				GP_LOG_D ("trying to get object size=0x%lx", (unsigned long)oi.ObjectCompressedSize);
				C_PTP_REP (ptp_getobject (params, newobject, (unsigned char**)&ximage));
				ret = gp_file_set_data_and_size(file, (char*)ximage, oi.ObjectCompressedSize);
				if (ret != GP_OK) {
					gp_file_free (file);
					ptp_free_devicepropdesc (&dpd);
					ptp_free_objectinfo (&oi);
					return ret;
				}
				ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
				if (ret != GP_OK) {
					gp_file_free (file);
					ptp_free_devicepropdesc (&dpd);
					ptp_free_objectinfo (&oi);
					return ret;
				}
				ret = gp_filesystem_set_file_noop(camera->fs, path->folder, path->name, GP_FILE_TYPE_NORMAL, file, context);
				if (ret != GP_OK) {
					gp_file_free (file);
					ptp_free_devicepropdesc (&dpd);
					ptp_free_objectinfo (&oi);
					return ret;
				}

				memset (&info, 0, sizeof (info));
				/* we also get the fs info for free, so just set it */
				info.file.fields = GP_FILE_INFO_TYPE |
						GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
						GP_FILE_INFO_SIZE | GP_FILE_INFO_MTIME;
				strcpy_mime (info.file.type, params->deviceinfo.VendorExtensionID, oi.ObjectFormat);
				info.file.width		= oi.ImagePixWidth;
				info.file.height	= oi.ImagePixHeight;
				info.file.size		= oi.ObjectCompressedSize;
				info.file.mtime		= time(NULL);

				info.preview.fields = GP_FILE_INFO_TYPE |
						GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
						GP_FILE_INFO_SIZE;
				strcpy_mime (info.preview.type, params->deviceinfo.VendorExtensionID, oi.ThumbFormat);
				info.preview.width	= oi.ThumbPixWidth;
				info.preview.height	= oi.ThumbPixHeight;
				info.preview.size	= oi.ThumbCompressedSize;
				GP_LOG_D ("setting fileinfo in fs");
				gp_filesystem_set_info_noop(camera->fs, path->folder, path->name, info, context);

				*eventtype = GP_EVENT_FILE_ADDED;
				*eventdata = path;
				/* We have now handed over the file, disclaim responsibility by unref. */
				gp_file_unref (file);

				/* Synthesize a capture complete event, if this was the last image. */
				if (dpd.CurrentValue.u16 == 0x8001) {
					event.Code = PTP_EC_CaptureComplete;
					event.Nparam = 0;
					ptp_add_event (params, &event);
				}
				ptp_free_devicepropdesc (&dpd);
				ptp_free_objectinfo (&oi);
				return GP_OK;
			}
sonyout:
			ptp_free_devicepropdesc (&dpd);
			/* If not, check for events and handle them */
			if (time_since(event_start) > timeout-100) {
				/* if there is less than 0.1 seconds, just check the
				 * queue. with libusb1 this can still make progress,
				 * as above bulk calls will check and queue new ptp events
				 * async */
				C_PTP_REP (ptp_check_event_queue(params));
			} else {
				C_PTP_REP (ptp_check_event(params));
			}
			if (ptp_get_one_event (params, &event))
				goto handleregular;
			gp_context_idle (context);
		} while (waiting_for_timeout (&back_off_wait, event_start, timeout));

		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_FUJI) &&
		ptp_property_issupported(params, PTP_DPC_FUJI_CurrentState)
	) {
		/* current strategy ... as the camera (currently) does not send us ObjectAdded events for some reason...
		 * we just synthesize them for the generic PTP event handler code */
		do {
			PTPObjectHandles	handles;
			unsigned int		i;

			if (ptp_get_one_event (params, &event))
				goto handleregular;
			C_PTP (ptp_getobjecthandles (params, PTP_HANDLER_SPECIAL, 0x000000, 0x000000, &handles));
			for (i=handles.n;i--;) {
				PTPObject	*ob;
				PTPObjectInfo	oi;

				if (params->inliveview == 1 && handles.Handler[i] == 0x80000001) /* Ignore preview image object handle while liveview is active */
					continue;
				if (PTP_RC_OK == ptp_object_find (params, handles.Handler[i], &ob)) /* already have it */
					continue;
				/* might be a just deleted entry , seen in https://github.com/gphoto/gphoto2/issues/456 */
				memset (&oi,0,sizeof(oi));
				if (PTP_RC_DeviceBusy == ptp_getobjectinfo (params, handles.Handler[i], &oi))
					continue;
				ptp_free_objectinfo (&oi);
				event.Code = PTP_EC_ObjectAdded;
				event.Param1 = handles.Handler[i];
				free (handles.Handler);
				goto handleregular;
			}
			free (handles.Handler);
			C_PTP_REP (ptp_check_event(params));
		} while (waiting_for_timeout (&back_off_wait, event_start, timeout));
		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) &&
		ptp_operation_issupported(params, PTP_OC_SONY_QX_GetAllDevicePropData)
	) {
		do {
			PTPObjectInfo		oi;

			C_PTP (ptp_sony_qx_getalldevicepropdesc (params)); /* avoid caching */

			/* FIXME: here the code for image addition */

			/* If not, check for events and handle them */
			if (time_since(event_start) > timeout-100) {
				/* if there is less than 0.1 seconds, just check the
				 * queue. with libusb1 this can still make progress,
				 * as above bulk calls will check and queue new ptp events
				 * async */
				C_PTP_REP (ptp_check_event_queue(params));
			} else {
				C_PTP_REP (ptp_check_event(params));
			}
			if (ptp_get_one_event (params, &event))
				goto handleregular;
			oi.ObjectFormat = 0;
			C_PTP (ptp_getobjectinfo (params, 0xffffc001, &oi));
			if (oi.ObjectFormat) {
				debug_objectinfo(params, newobject, &oi);
				C_MEM (path = malloc(sizeof(CameraFilePath)));
				path->name[0]='\0';
				strcpy (path->folder,"/");
				ret = gp_file_new(&file);
				if (ret!=GP_OK)
					return ret;
				if (oi.ObjectFormat != PTP_OFC_EXIF_JPEG) {
					GP_LOG_D ("raw? ofc is 0x%04x, name is %s", oi.ObjectFormat,oi.Filename);
					sprintf (path->name, "capt%04d.arw", params->capcnt++);
					gp_file_set_mime_type (file, "image/x-sony-arw"); /* FIXME */
				} else {
					sprintf (path->name, "capt%04d.jpg", params->capcnt++);
					gp_file_set_mime_type (file, GP_MIME_JPEG);
				}
				gp_file_set_mtime (file, time(NULL));

				GP_LOG_D ("trying to get object size=0x%lx", (unsigned long)oi.ObjectCompressedSize);
				C_PTP_REP (ptp_getobject (params, newobject, (unsigned char**)&ximage));
				ret = gp_file_set_data_and_size(file, (char*)ximage, oi.ObjectCompressedSize);
				if (ret != GP_OK) {
					ptp_free_objectinfo (&oi);
					gp_file_free (file);
					return ret;
				}
				ptp_free_objectinfo (&oi);
				ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
				if (ret != GP_OK) {
					gp_file_free (file);
					return ret;
				}
				ret = gp_filesystem_set_file_noop(camera->fs, path->folder, path->name, GP_FILE_TYPE_NORMAL, file, context);
				if (ret != GP_OK) {
					gp_file_free (file);
					return ret;
				}
				*eventtype = GP_EVENT_FILE_ADDED;
				*eventdata = path;
				/* We have now handed over the file, disclaim responsibility by unref. */
				gp_file_unref (file);
				break;
			}

			gp_context_idle (context);
		} while (waiting_for_timeout (&back_off_wait, event_start, timeout));

		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}
	if 	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_GP_OLYMPUS_OMD)
	{

		do {
			C_PTP_REP (ptp_check_event (params));

			while (ptp_get_one_event(params, &event)) {
				GP_LOG_D ("received event Code %04x, Param 1 %08x", event.Code, event.Param1);
				switch (event.Code) {
				case PTP_EC_Olympus_ObjectAdded:
				case PTP_EC_Olympus_ObjectAdded_New:
				case PTP_EC_ObjectAdded:
					newobject = event.Param1;
					goto downloadomdfile;
				case PTP_EC_Olympus_DevicePropChanged:
				case PTP_EC_Olympus_DevicePropChanged_New: {
					char			*name, *content;
					PTPDevicePropDesc	dpd;

					*eventtype = GP_EVENT_UNKNOWN;
					/* cached devprop should hafve been flushed I think... */
					C_PTP_REP (ptp_generic_getdevicepropdesc (params, event.Param1&0xffff, &dpd));

					ret = camera_lookup_by_property(camera, &dpd, &name, &content, context);
					if (ret == GP_OK) {
						C_MEM (*eventdata = malloc(strlen("PTP Property 0123 changed, \"\" to \"\"")+strlen(name)+1+strlen(content?content:"")));
						sprintf (*eventdata, "PTP Property %04x changed, \"%s\" to \"%s\"", event.Param1 & 0xffff, name, content?content:"");
						free (name);
						free (content);
					} else {
						C_MEM (*eventdata = malloc(strlen("PTP Property 0123 changed")+1));
						sprintf (*eventdata, "PTP Property %04x changed", event.Param1 & 0xffff);
					}
					ptp_free_devicepropdesc (&dpd);
					return GP_OK;
				}
				default:
					*eventtype = GP_EVENT_UNKNOWN;
					C_MEM (*eventdata = malloc(strlen("PTP Event 0123, Param1 01234567")+1));
					sprintf (*eventdata, "PTP Event %04x, Param1 %08x", event.Code, event.Param1);
					return GP_OK;
				}
			}
		}  while (waiting_for_timeout (&back_off_wait, event_start, timeout));

downloadomdfile:
		C_MEM (path = malloc(sizeof(CameraFilePath)));
		path->name[0]='\0';
		path->folder[0]='\0';

		if (newobject != 0) {
			CR (add_object_to_fs_and_path (camera, newobject, path, context));

			*eventtype = GP_EVENT_FILE_ADDED;
			*eventdata = path;
			return GP_OK;
		}
		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}

	/* Wait for the whole timeout period */
	CR (gp_port_get_timeout (camera->port, &oldtimeout));
	CR (gp_port_set_timeout (camera->port, timeout));
	C_PTP_REP (ptp_wait_event(params));
	CR (gp_port_set_timeout (camera->port, oldtimeout));

	/* FIXME: Might be another error, but usually is a timeout */
	if (ptp_get_one_event (params, &event)) {
		goto handleregular;
	}

	*eventtype = GP_EVENT_TIMEOUT;
	return GP_OK;

handleregular:
	if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_PANASONIC) {
		switch (event.Code) {
		case PTP_EC_PANASONIC_ObjectAdded:
			GP_LOG_D ("PANASONIC_ObjectAdded -> PTP_EC_ObjectAdded");
			event.Code = PTP_EC_ObjectAdded;
			break;
		}
	}
	if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) {
		switch (event.Code) {
		/* We are handling this in the above Sony case. events might
		 * come to early (or too late, as we handled it above)
		 * and retrieving the image will crash the camera. */
		case PTP_EC_Sony_ObjectAdded: {
#if 0
			PTPObjectInfo	oi;

			C_MEM (path = malloc(sizeof(CameraFilePath)));
			C_PTP (ptp_getobjectinfo (params, event.Param1, &oi));

			sprintf (path->folder,"/");
			if (oi.ObjectFormat == PTP_OFC_SONY_RAW)
				sprintf (path->name, "capt%04d.arw", params->capcnt++);
			else
				sprintf (path->name, "capt%04d.jpg", params->capcnt++);

			CR (add_objectid_and_upload (camera, path, context, event.Param1, &oi));

			ptp_free_objectinfo (&oi);
			*eventtype = GP_EVENT_FILE_ADDED;
			*eventdata = path;
			return GP_OK;
#endif
			break;
		}
		case PTP_EC_Sony_PropertyChanged: /* same as DevicePropChanged, just go there */
			event.Code = PTP_EC_DevicePropChanged;
			break;
		}
		/*fallthrough*/
	}
	switch (event.Code) {
	case PTP_EC_CaptureComplete:
		*eventtype = GP_EVENT_CAPTURE_COMPLETE;
		*eventdata = NULL;
		break;
	case PTP_EC_ObjectAdded: {
		PTPObject	*ob;

		C_MEM (path = malloc(sizeof(CameraFilePath)));
		path->name[0]='\0';
		path->folder[0]='\0';

		CR (add_object_to_fs_and_path (camera, event.Param1, path, context));

		C_PTP_REP (ptp_object_want (params, event.Param1, PTPOBJECT_OBJECTINFO_LOADED, &ob));

		if (ob->oi.ObjectFormat == PTP_OFC_Association) { /* new folder! */
			*eventtype = GP_EVENT_FOLDER_ADDED;
			*eventdata = path;
			gp_filesystem_reset (camera->fs); /* FIXME: implement more lightweight folder add */
		} else {
			*eventtype = GP_EVENT_FILE_ADDED;
			*eventdata = path;
		}
		break;
	}
	case PTP_EC_DeviceInfoChanged:
		*eventtype = GP_EVENT_UNKNOWN;
		C_MEM (*eventdata = malloc(strlen("PTP Deviceinfo changed")+1));
		sprintf (*eventdata, "PTP Deviceinfo changed");
		C_PTP_REP (ptp_getdeviceinfo (params, &params->deviceinfo));
		CR (fixup_cached_deviceinfo (camera, &params->deviceinfo));
		print_debug_deviceinfo(params, &params->deviceinfo);
		break;
	case PTP_EC_DevicePropChanged: {
		char			*name, *content;
		PTPDevicePropDesc	dpd;

		*eventtype = GP_EVENT_UNKNOWN;
		/* cached devprop should have been flushed I think... */
		ret = ptp_generic_getdevicepropdesc (params, event.Param1&0xffff, &dpd);

		/* Nikon Z6 II reports a prop changed event 501c, but getdevicepropdesc fails with devicepropnot supported
		 * (reported via email)
		 */
		if (ret == PTP_RC_DevicePropNotSupported) {
			C_MEM (*eventdata = malloc(strlen("PTP Property 0123 changed")+1));
			sprintf (*eventdata, "PTP Property %04x changed", event.Param1 & 0xffff);
			break;
		}
		if (ret != PTP_RC_OK)
			C_PTP_REP (ret);

		ret = camera_lookup_by_property(camera, &dpd, &name, &content, context);
		if (ret == GP_OK) {
			C_MEM (*eventdata = malloc(strlen("PTP Property 0123 changed, \"\" to \"\"")+strlen(name)+1+strlen(content?content:"")));
			sprintf (*eventdata, "PTP Property %04x changed, \"%s\" to \"%s\"", event.Param1 & 0xffff, name, content?content:"");
			free (name);
			free (content);
		} else {
			C_MEM (*eventdata = malloc(strlen("PTP Property 0123 changed")+1));
			sprintf (*eventdata, "PTP Property %04x changed", event.Param1 & 0xffff);
		}
		ptp_free_devicepropdesc (&dpd);
		break;
	}
	case PTP_EC_ObjectInfoChanged: {
		PTPObject	*ob;
		uint32_t	oldstorage, oldparent;
		char		*oldfn;

		C_PTP_REP (ptp_object_want (params, event.Param1, 0, &ob));
		/* objectinfo might not even be loaded yet, but this could be 0 / NULL ... but the compare will trigger */
		oldparent = ob->oi.ParentObject;
		oldstorage = ob->oi.StorageID;
		oldfn = strdup(ob->oi.Filename?ob->oi.Filename:"<null>");

		ob->flags &= ~PTPOBJECT_OBJECTINFO_LOADED;


		C_MEM (path = malloc(sizeof(CameraFilePath)));
		path->name[0]='\0';
		path->folder[0]='\0';

		CR (add_object_to_fs_and_path (camera, event.Param1, path, context));

		C_PTP_REP (ptp_object_want (params, event.Param1, PTPOBJECT_OBJECTINFO_LOADED, &ob));

		/* did the location or name of the file changed in the filesystem? Then we can only reset the fs and refetch the layout */
		if (	(oldparent != ob->oi.ParentObject) &&
			(oldstorage != ob->oi.StorageID) &&
			(strcmp(oldfn,ob->oi.Filename) != 0)
		)
			gp_filesystem_reset (camera->fs);
		free (oldfn);

		if (ob->oi.ObjectFormat == PTP_OFC_Association) { /* new folder? would be weird here? */
			*eventtype = GP_EVENT_FOLDER_ADDED;
			*eventdata = path;
		} else {
			*eventtype = GP_EVENT_FILE_CHANGED;
			*eventdata = path;
			gp_filesystem_set_info_dirty (camera->fs, path->folder, path->name, context);
		}
		break;
	}
	case PTP_EC_ObjectRemoved:
		ptp_remove_object_from_cache(params, event.Param1);
		gp_filesystem_reset (camera->fs);
		*eventtype = GP_EVENT_UNKNOWN;
		C_MEM (*eventdata = malloc(strlen("PTP ObjectRemoved, Param1 01234567")+1));
		sprintf (*eventdata, "PTP ObjectRemoved, Param1 %08x", event.Param1);
		break;
	case PTP_EC_StoreAdded:
		gp_filesystem_reset (camera->fs);
		*eventtype = GP_EVENT_UNKNOWN;
		C_MEM (*eventdata = malloc(strlen("PTP StoreAdded, Param1 01234567")+1));
		sprintf (*eventdata, "PTP StoreAdded, Param1 %08x", event.Param1);
		break;
	case PTP_EC_StoreRemoved:
		gp_filesystem_reset (camera->fs);
		*eventtype = GP_EVENT_UNKNOWN;
		C_MEM (*eventdata = malloc(strlen("PTP StoreRemoved, Param1 01234567")+1));
		sprintf (*eventdata, "PTP StoreRemoved, Param1 %08x", event.Param1);
		break;
	default:
		*eventtype = GP_EVENT_UNKNOWN;
		C_MEM (*eventdata = malloc(strlen("PTP Event 0123, Param1 01234567")+1));
		sprintf (*eventdata, "PTP Event %04x, Param1 %08x", event.Code, event.Param1);
		break;
	}
	return GP_OK;
}

static int
snprintf_ptp_property (char *txt, int spaceleft, PTPPropertyValue *data, uint16_t dt)
{
	if (dt == PTP_DTC_STR)
		return snprintf (txt, spaceleft, "'%s'", data->str);
	if (dt & PTP_DTC_ARRAY_MASK) {
		unsigned int i;
		const char *origtxt = txt;
#define SPACE_LEFT (origtxt + spaceleft - txt)

		txt += snprintf (txt, SPACE_LEFT, "a[%d] ", data->a.count);
		for ( i=0; i<data->a.count; i++) {
			txt += snprintf_ptp_property (txt, SPACE_LEFT, &data->a.v[i], dt & ~PTP_DTC_ARRAY_MASK);
			if (i!=data->a.count-1)
				txt += snprintf (txt, SPACE_LEFT, ",");
		}
		return txt - origtxt;
#undef SPACE_LEFT
	} else {
		switch (dt) {
		case PTP_DTC_UNDEF:
			return snprintf (txt, spaceleft, "Undefined");
		case PTP_DTC_INT8:
			return snprintf (txt, spaceleft, "%d", data->i8);
		case PTP_DTC_UINT8:
			return snprintf (txt, spaceleft, "%u", data->u8);
		case PTP_DTC_INT16:
			return snprintf (txt, spaceleft, "%d", data->i16);
		case PTP_DTC_UINT16:
			return snprintf (txt, spaceleft, "%u", data->u16);
		case PTP_DTC_INT32:
			return snprintf (txt, spaceleft, "%d", data->i32);
		case PTP_DTC_UINT32:
			return snprintf (txt, spaceleft, "%u", data->u32);
		case PTP_DTC_INT64:
			return snprintf (txt, spaceleft, "%lu", data->u64);
		case PTP_DTC_UINT64:
			return snprintf (txt, spaceleft, "%ld", data->i64);
	/*
		PTP_DTC_INT128
		PTP_DTC_UINT128
	*/
		default:
			return snprintf (txt, spaceleft, "Unknown %x", dt);
		}
	}
	return 0;
}

#if 0 /* leave out ... is confusing -P downloads */
#pragma pack(1)
struct canon_theme_entry {
	uint16_t	unknown1;
	uint32_t	offset;
	uint32_t	length;
	uint8_t		name[8];
	char		unknown2[8];
};

static int
canon_theme_get (CameraFilesystem *fs, const char *folder, const char *filename,
		 CameraFileType type, CameraFile *file, void *data,
		 GPContext *context)
{
	Camera		*camera = (Camera*)data;
	PTPParams	*params = &camera->pl->params;
	unsigned char	*xdata;
	unsigned int	size;
	int i;
	struct canon_theme_entry	*ent;

	SET_CONTEXT(camera, context);

	C_PTP_REP (ptp_canon_get_customize_data (params, 1, &xdata, &size));

	C_PARAMS (size >= 42+sizeof(struct canon_theme_entry)*5);

	ent = (struct canon_theme_entry*)(xdata+42);
	for (i=0;i<5;i++) {
		fprintf(stderr,"entry %d: unknown1 = %x\n", i, ent[i].unknown1);
		fprintf(stderr,"entry %d: off = %d\n", i, ent[i].offset);
		fprintf(stderr,"entry %d: len = %d\n", i, ent[i].length);
		fprintf(stderr,"entry %d: name = %s\n", i, ent[i].name);
	}
	CR (gp_file_set_data_and_size (file, (char*)xdata, size));
	return (GP_OK);
}

static int
canon_theme_put (CameraFilesystem *fs, const char *folder, CameraFile *file,
		void *data, GPContext *context)
{
	/* not yet */
	return (GP_OK);
}
#endif

static int
nikon_curve_get (CameraFilesystem *fs, const char *folder, const char *filename,
	         CameraFileType type, CameraFile *file, void *data,
		 GPContext *context)
{
	Camera		*camera = (Camera*)data;
	PTPParams	*params = &camera->pl->params;
	unsigned char	*xdata;
	unsigned int	size;
	int		n;
	PTPNIKONCurveData	*tonecurve;
	char		*ntcfile;
	char		*charptr;
	double		*doubleptr;
	((PTPData *) camera->pl->params.data)->context = context;
	SET_CONTEXT(camera, context);

	C_PTP_REP (ptp_nikon_curve_download (params, &xdata, &size));

	tonecurve = (PTPNIKONCurveData *) xdata;
	C_MEM (ntcfile = malloc(2000));
	memcpy(ntcfile,"\x9d\xdc\x7d\x00\x65\xd4\x11\xd1\x91\x94\x44\x45\x53\x54\x00\x00\xff\x05\xbb\x02\x00\x00\x01\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x9d\xdc\x7d\x03\x65\xd4\x11\xd1\x91\x94\x44\x45\x53\x54\x00\x00\x00\x00\x00\x00\xff\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\x00\x00\x00\xff\x00\x00\x00\xff\x00\x00\x00", 92);
	doubleptr=(double *) &ntcfile[92];
	*doubleptr++ = (double) tonecurve->XAxisStartPoint/255;
	*doubleptr++ = (double) tonecurve->XAxisEndPoint/255;
	*doubleptr++ = (double) tonecurve->MidPointIntegerPart
			+ tonecurve->MidPointDecimalPart/100;
	*doubleptr++ = (double) tonecurve->YAxisStartPoint/255;
	*doubleptr++ = (double) tonecurve->YAxisEndPoint/255;
	charptr=(char*) doubleptr;
	*charptr++ = (char) tonecurve->NCoordinates;
	memcpy(charptr, "\x00\x00\x00", 3);
	charptr +=3;
	doubleptr = (double *) charptr;
	for(n=0;n<tonecurve->NCoordinates;n++) {
		*doubleptr = (double) tonecurve->CurveCoordinates[n].X/255;
		doubleptr = &doubleptr[1];
		*doubleptr = (double) tonecurve->CurveCoordinates[n].Y/255;
		doubleptr = &doubleptr[1];
	}
	*doubleptr++ = (double) 0;
	charptr = (char*) doubleptr;
	memcpy(charptr,"\x9d\xdc\x7d\x03\x65\xd4\x11\xd1\x91\x94\x44\x45\x53\x54\x00\x00\x01\x00\x00\x00\xff\x03\x00\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\x00\x00\x9d\xdc\x7d\x03\x65\xd4\x11\xd1\x91\x94\x44\x45\x53\x54\x00\x00\x02\x00\x00\x00\xff\x03\x00\x00\x00\x00\x00\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\x00\x00\x9d\xdc\x7d\x03\x65\xd4\x11\xd1\x91\x94\x44\x45\x53\x54\x00\x00\x03\x00\x00\x00\xff\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\xf0\x3f\x00\x00\x00\x00\x00\x00\x00\x00",429);
	charptr += 429;
	CR (gp_file_set_data_and_size (file, ntcfile, (long)charptr - (long)ntcfile));
	/* do not free ntcfile, it is managed by filesys now */
	free (xdata);
	return (GP_OK);
}

static int
nikon_curve_put (CameraFilesystem *fs, const char *folder, CameraFile *file,
		void *data, GPContext *context)
{
	/* not yet */
	return (GP_OK);
}

static int ptp_max(int a, int b) {
	if (a > b) return a;
	return b;
}

static int
camera_summary (Camera* camera, CameraText* summary, GPContext *context)
{
	unsigned int i, j;
	uint16_t ret;
	char *txt, *txt_marker;
	PTPParams *params = &(camera->pl->params);
	PTPDeviceInfo pdi;

	SET_CONTEXT(camera, context);

	txt = summary->text;

#define SPACE_LEFT ptp_max(0, summary->text + sizeof (summary->text) - txt)
#define APPEND_TXT( ... ) txt += snprintf (txt, SPACE_LEFT, __VA_ARGS__)

	APPEND_TXT (_("Manufacturer: %s\n"),params->deviceinfo.Manufacturer);
	APPEND_TXT (_("Model: %s\n"),params->deviceinfo.Model);
	APPEND_TXT (_("  Version: %s\n"),params->deviceinfo.DeviceVersion);
	if (params->deviceinfo.SerialNumber)
		APPEND_TXT (_("  Serial Number: %s\n"),params->deviceinfo.SerialNumber);
	if (params->deviceinfo.VendorExtensionID) {
		APPEND_TXT (_("Vendor Extension ID: 0x%x (%d.%d)\n"),
			params->deviceinfo.VendorExtensionID,
			params->deviceinfo.VendorExtensionVersion/100,
			params->deviceinfo.VendorExtensionVersion%100
		);
		if (params->deviceinfo.VendorExtensionDesc)
			APPEND_TXT (_("Vendor Extension Description: %s\n"),params->deviceinfo.VendorExtensionDesc);
	}
	if (params->deviceinfo.StandardVersion != 100)
		APPEND_TXT (_("PTP Standard Version: %d.%d\n"),
			params->deviceinfo.StandardVersion/100,
			params->deviceinfo.StandardVersion%100
		);
	if (params->deviceinfo.FunctionalMode)
		APPEND_TXT (_("Functional Mode: 0x%04x\n"),params->deviceinfo.FunctionalMode);

/* Dump Formats */
	APPEND_TXT (_("\nCapture Formats: "));

	for (i=0;i<params->deviceinfo.CaptureFormats_len;i++) {
		txt += ptp_render_ofc (params, params->deviceinfo.CaptureFormats[i], SPACE_LEFT, txt);
		if (i<params->deviceinfo.CaptureFormats_len-1)
			APPEND_TXT (" ");
	}
	APPEND_TXT ("\n");

	APPEND_TXT (_("Display Formats: "));
	for (i=0;i<params->deviceinfo.ImageFormats_len;i++) {
		txt += ptp_render_ofc (params, params->deviceinfo.ImageFormats[i], SPACE_LEFT, txt);
		if (i<params->deviceinfo.ImageFormats_len-1)
			APPEND_TXT (", ");
	}
	APPEND_TXT ("\n");

	/* PTP 1.1 streams */
	if (	ptp_operation_issupported(params,PTP_OC_GetStreamInfo) &&
		ptp_property_issupported(params, PTP_DPC_SupportedStreams)
	)  {
		PTPPropertyValue propval;

		ret = ptp_getdevicepropvalue (params, PTP_DPC_SupportedStreams, &propval, PTP_DTC_UINT32);
		if (ret == PTP_RC_OK) {
			APPEND_TXT (_("\nStreams:"));
			APPEND_TXT ("%08x", propval.u32);
			APPEND_TXT ("\n");
			for (i=0;i<31;i++) {
				PTPStreamInfo streaminfo;

				if ((1<<i) & propval.u32) {
					APPEND_TXT ("\t");
					switch (i) {
					case 0: APPEND_TXT (_("Video"));
						break;
					case 1: APPEND_TXT (_("Audio"));
						break;
					default: APPEND_TXT ("%d", i);
						break;
					}
					ret = ptp_getstreaminfo (params, i, &streaminfo);
					if (ret == PTP_RC_OK) {
						APPEND_TXT( ": dss: %lud, timeres: %lud, fhsize: %d, fmxsize: %d, phsize: %d, pmsize: %d, palign: %d\n",
							streaminfo.DatasetSize,
							streaminfo.TimeResolution,
							streaminfo.FrameHeaderSize,
							streaminfo.FrameMaxSize,
							streaminfo.PacketHeaderSize,
							streaminfo.PacketMaxSize,
							streaminfo.PacketAlignment
						);
					} else APPEND_TXT (": error 0x%04x on getstreaminfo\n", ret);
				}
			}
		}
	}

	if (is_mtp_capable (camera) &&
	    ptp_operation_issupported(params,PTP_OC_MTP_GetObjectPropsSupported)
	) {
		APPEND_TXT (_("Supported MTP Object Properties:\n"));
		for (i=0;i<params->deviceinfo.ImageFormats_len;i++) {
			uint16_t *props = NULL;
			uint32_t propcnt = 0;

			APPEND_TXT ("\t");
			txt += ptp_render_ofc (params, params->deviceinfo.ImageFormats[i], SPACE_LEFT, txt);
			APPEND_TXT ("/%04x:", params->deviceinfo.ImageFormats[i]);

			ret = ptp_mtp_getobjectpropssupported (params, params->deviceinfo.ImageFormats[i], &propcnt, &props);
			if (ret != PTP_RC_OK) {
				APPEND_TXT (_(" PTP error %04x on query"), ret);
			} else {
				for (j=0;j<propcnt;j++) {
					APPEND_TXT (" %04x/",props[j]);
					txt += ptp_render_mtp_propname(props[j],SPACE_LEFT,txt);
				}
				free(props);
			}
			APPEND_TXT ("\n");
		}
	}

/* Dump out dynamic capabilities */
	APPEND_TXT (_("\nDevice Capabilities:\n"));

	/* First line for file operations */
		APPEND_TXT (_("\tFile Download, "));
		if (ptp_operation_issupported(params,PTP_OC_DeleteObject))
			APPEND_TXT (_("File Deletion, "));
		else
			APPEND_TXT (_("No File Deletion, "));

		if (ptp_operation_issupported(params,PTP_OC_SendObject))
			APPEND_TXT (_("File Upload\n"));
		else
			APPEND_TXT (_("No File Upload\n"));

	/* Second line for capture */
		if (ptp_operation_issupported(params,PTP_OC_InitiateCapture))
			APPEND_TXT (_("\tGeneric Image Capture, "));
		else
			APPEND_TXT (_("\tNo Image Capture, "));
		if (ptp_operation_issupported(params,PTP_OC_InitiateOpenCapture))
			APPEND_TXT (_("Open Capture, "));
		else
			APPEND_TXT (_("No Open Capture, "));

		txt_marker = txt;
		switch (params->deviceinfo.VendorExtensionID) {
		case PTP_VENDOR_CANON:
			if (ptp_operation_issupported(params, PTP_OC_CANON_ViewfinderOn))
				APPEND_TXT (_("Canon Capture, "));
			if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease))
				APPEND_TXT (_("Canon EOS Capture, "));
			if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn))
				APPEND_TXT (_("Canon EOS Capture 2, "));
			break;
		case PTP_VENDOR_NIKON:
			if (ptp_operation_issupported(params, PTP_OC_NIKON_InitiateCaptureRecInSdram))
				APPEND_TXT (_("Nikon Capture 1, "));
			if (ptp_operation_issupported(params, PTP_OC_NIKON_AfCaptureSDRAM))
				APPEND_TXT (_("Nikon Capture 2, "));
			if (ptp_operation_issupported(params, PTP_OC_NIKON_InitiateCaptureRecInMedia))
				APPEND_TXT (_("Nikon Capture 3, "));
			break;
		case PTP_VENDOR_SONY:
			if (ptp_operation_issupported(params, PTP_OC_SONY_SetControlDeviceB))
				APPEND_TXT (_("Sony Capture, "));
			break;
		default:
			/* does not belong to good vendor ... needs another detection */
			if (params->device_flags & DEVICE_FLAG_OLYMPUS_XML_WRAPPED)
				APPEND_TXT (_("Olympus E XML Capture, "));
			break;
		}
		if (txt_marker == txt)
			APPEND_TXT (_("No vendor specific capture\n"));
		else
		{
			/* replace the trailing ", " with a "\n" */
			txt -= 2;
			APPEND_TXT ("\n");
		}

	/* Third line for Wifi support, but just leave it out if not there. */
		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		     ptp_operation_issupported(params, PTP_OC_NIKON_GetProfileAllData))
			APPEND_TXT (_("\tNikon Wifi support\n"));

		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		     ptp_operation_issupported(params, PTP_OC_CANON_GetMACAddress))
			APPEND_TXT (_("\tCanon Wifi support\n"));

/* Dump storage information */

	if (ptp_operation_issupported(params,PTP_OC_GetStorageIDs) &&
	    ptp_operation_issupported(params,PTP_OC_GetStorageInfo)
	) {
		APPEND_TXT (_("\nStorage Devices Summary:\n"));

		for (i=0; i<params->storageids.n; i++) {
			char tmpname[20], *s;

			PTPStorageInfo storageinfo;
			/* invalid storage, storageinfo might fail on it (Nikon D300s e.g.) */
			if ((params->storageids.Storage[i]&0x0000ffff)==0)
				continue;

			APPEND_TXT ("store_%08x:\n",(unsigned int)params->storageids.Storage[i]);

			C_PTP_REP (ptp_getstorageinfo(params, params->storageids.Storage[i], &storageinfo));
			APPEND_TXT (_("\tStorageDescription: %s\n"),
				storageinfo.StorageDescription?storageinfo.StorageDescription:_("None")
			);
			APPEND_TXT (_("\tVolumeLabel: %s\n"),
				storageinfo.VolumeLabel?storageinfo.VolumeLabel:_("None")
			);

			switch (storageinfo.StorageType) {
			case PTP_ST_Undefined: s = _("Undefined"); break;
			case PTP_ST_FixedROM: s = _("Builtin ROM"); break;
			case PTP_ST_RemovableROM: s = _("Removable ROM"); break;
			case PTP_ST_FixedRAM: s = _("Builtin RAM"); break;
			case PTP_ST_RemovableRAM: s = _("Removable RAM (memory card)"); break;
			default:
				snprintf(tmpname, sizeof(tmpname), _("Unknown: 0x%04x\n"), storageinfo.StorageType);
				s = tmpname;
				break;
			}
			APPEND_TXT (_("\tStorage Type: %s\n"), s);

			switch (storageinfo.FilesystemType) {
			case PTP_FST_Undefined: s = _("Undefined"); break;
			case PTP_FST_GenericFlat: s = _("Generic Flat"); break;
			case PTP_FST_GenericHierarchical: s = _("Generic Hierarchical"); break;
			case PTP_FST_DCF: s = _("Digital Camera Layout (DCIM)"); break;
			default:
				snprintf(tmpname, sizeof(tmpname), _("Unknown: 0x%04x\n"), storageinfo.FilesystemType);
				s = tmpname;
				break;
			}
			APPEND_TXT (_("\tFilesystemtype: %s\n"), s);

			switch (storageinfo.AccessCapability) {
			case PTP_AC_ReadWrite: s = _("Read-Write"); break;
			case PTP_AC_ReadOnly: s = _("Read-Only"); break;
			case PTP_AC_ReadOnly_with_Object_Deletion: s = _("Read Only with Object deletion"); break;
			default:
				snprintf(tmpname, sizeof(tmpname), _("Unknown: 0x%04x\n"), storageinfo.AccessCapability);
				s = tmpname;
				break;
			}
			APPEND_TXT (_("\tAccess Capability: %s\n"), s);
			APPEND_TXT (_("\tMaximum Capability: %llu (%lu MB)\n"),
				(unsigned long long)storageinfo.MaxCapability,
				(unsigned long)(storageinfo.MaxCapability/1024/1024)
			);
			APPEND_TXT (_("\tFree Space (Bytes): %llu (%lu MB)\n"),
				(unsigned long long)storageinfo.FreeSpaceInBytes,
				(unsigned long)(storageinfo.FreeSpaceInBytes/1024/1024)
			);
			APPEND_TXT (_("\tFree Space (Images): %d\n"), (unsigned int)storageinfo.FreeSpaceInImages);
			free (storageinfo.StorageDescription);
			free (storageinfo.VolumeLabel);
		}
	}

	APPEND_TXT (_("\nDevice Property Summary:\n"));
	/* The information is cached. However, the canon firmware changes
	 * the available properties in capture mode.
	 */
	C_PTP_REP (ptp_getdeviceinfo (params, &pdi));
	CR (fixup_cached_deviceinfo (camera, &pdi));
        for (i=0;i<pdi.DevicePropertiesSupported_len;i++) {
		PTPDevicePropDesc dpd;
		unsigned int dpc = pdi.DevicePropertiesSupported[i];
		const char *propname = ptp_get_property_description (params, dpc);

		if (propname) {
			/* string registered for i18n in ptp.c. */
			APPEND_TXT ("%s(0x%04x):", _(propname), dpc);
		} else {
			APPEND_TXT ("Property 0x%04x:", dpc);
		}


		/* Do not read the 0xd201 property (found on Creative Zen series).
		 * It seems to cause hangs.
		 */
		if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_MICROSOFT) {
			if (dpc == 0xd201) {
				APPEND_TXT (_(" not read out.\n"));
				continue;
			}
		}
#if 0 /* check is handled by the generic getter now */
		if (!ptp_operation_issupported(params, PTP_OC_GetDevicePropDesc)) {
			APPEND_TXT (_("cannot be queried.\n"));
			continue;
		}
#endif

		memset (&dpd, 0, sizeof (dpd));
		ret = ptp_generic_getdevicepropdesc (params, dpc, &dpd);
		if (ret == PTP_RC_OK) {
			switch (dpd.GetSet) {
			case PTP_DPGS_Get:    APPEND_TXT ("(%s) ", N_("read only")); break;
			case PTP_DPGS_GetSet: APPEND_TXT ("(%s) ", N_("readwrite")); break;
			default:              APPEND_TXT ("(%s) ", N_("Unknown"));
			}
			APPEND_TXT ("(type=0x%x) ",dpd.DataType);
			switch (dpd.FormFlag) {
			case PTP_DPFF_None:	break;
			case PTP_DPFF_Range: {
				APPEND_TXT ("Range [");
				txt += snprintf_ptp_property (txt, SPACE_LEFT, &dpd.FORM.Range.MinimumValue, dpd.DataType);
				APPEND_TXT (" - ");
				txt += snprintf_ptp_property (txt, SPACE_LEFT, &dpd.FORM.Range.MaximumValue, dpd.DataType);
				APPEND_TXT (", step ");
				txt += snprintf_ptp_property (txt, SPACE_LEFT, &dpd.FORM.Range.StepSize, dpd.DataType);
				APPEND_TXT ("] value: ");
				break;
			}
			case PTP_DPFF_Enumeration:
				APPEND_TXT ("Enumeration [");
				if ((dpd.DataType & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)
					APPEND_TXT ("\n\t");
				for (j = 0; j<dpd.FORM.Enum.NumberOfValues; j++) {
					txt += snprintf_ptp_property (txt, SPACE_LEFT, dpd.FORM.Enum.SupportedValue+j, dpd.DataType);
					if (j+1 != dpd.FORM.Enum.NumberOfValues) {
						APPEND_TXT (",");
						if ((dpd.DataType & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)
							APPEND_TXT ("\n\t");
					}
				}
				if ((dpd.DataType & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)
					APPEND_TXT ("\n\t");
				APPEND_TXT ("] value: ");
				break;
			}
			txt_marker = txt;
			txt += ptp_render_property_value(params, dpc, &dpd, SPACE_LEFT, txt);
			if (txt != txt_marker) {
				APPEND_TXT (" (");
				txt += snprintf_ptp_property (txt, SPACE_LEFT, &dpd.CurrentValue, dpd.DataType);
				APPEND_TXT (")");
			} else {
				txt += snprintf_ptp_property (txt, SPACE_LEFT, &dpd.CurrentValue, dpd.DataType);
			}
		} else {
			APPEND_TXT (_(" error %x on query."), ret);
		}
		APPEND_TXT ("\n");
		ptp_free_devicepropdesc (&dpd);
        }
	ptp_free_DI (&pdi);
	return (GP_OK);
#undef SPACE_LEFT
#undef APPEND_TXT
}

static uint32_t
find_child (PTPParams *params,const char *file,uint32_t storage,uint32_t handle,PTPObject **retob)
{
	unsigned int	i;
	uint16_t	ret;

	ret = ptp_list_folder (params, storage, handle);
	if (ret != PTP_RC_OK)
		return PTP_HANDLER_SPECIAL;

	for (i = 0; i < params->nrofobjects; i++) {
		PTPObject	*ob = &params->objects[i];
		uint32_t	oid = ob->oid;

		ret = PTP_RC_OK;
		if ((ob->flags & (PTPOBJECT_PARENTOBJECT_LOADED|PTPOBJECT_STORAGEID_LOADED)) != (PTPOBJECT_PARENTOBJECT_LOADED|PTPOBJECT_STORAGEID_LOADED))
			ret = ptp_object_want (params, oid, PTPOBJECT_PARENTOBJECT_LOADED|PTPOBJECT_STORAGEID_LOADED, &ob);
		if (ret != PTP_RC_OK) {
			/* NOTE: the "i" array entry might now be invalid, as object_want can remove objects from the list */
			GP_LOG_D("failed getting info of oid 0x%08x?", oid);
			/* could happen if file gets removed between */
			continue;
		}
		if ((ob->oi.StorageID==storage) && (ob->oi.ParentObject==handle)) {
			ret = ptp_object_want (params, oid, PTPOBJECT_OBJECTINFO_LOADED, &ob);
			if (ret != PTP_RC_OK) {
				GP_LOG_D("failed getting info of oid 0x%08x?", oid);
				/* could happen if file gets removed between */
				/* FIXME: should remove, but then we irritate our list */
				continue;
			}
			if (!strcmp (ob->oi.Filename,file)) {
				if (retob) *retob = ob;
				return oid;
			}
		}
	}
	/* else not found */
	return PTP_HANDLER_SPECIAL;
}

static uint32_t
folder_to_handle(PTPParams *params, const char *folder, uint32_t storage, uint32_t parent, PTPObject **retob)
{
	char 		*c;

	if (retob) *retob = NULL;
	if (!strlen(folder)) {
		/* was initially read, no need to reread */
		/* ptp_list_folder (params, storage, 0); */
		return PTP_HANDLER_ROOT;
	}
	if (!strcmp(folder,"/")) {
		/* was initially read, no need to reread */
		/* ptp_list_folder (params, storage, 0); */
		return PTP_HANDLER_ROOT;
	}

	c = strchr(folder,'/');
	if (c != NULL) {
		*c = 0;
		parent = find_child (params, folder, storage, parent, retob);
		if (parent == PTP_HANDLER_SPECIAL)
			GP_LOG_D("not found???");
		return folder_to_handle(params, c+1, storage, parent, retob);
	} else  {
		return find_child (params, folder, storage, parent, retob);
	}
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
    Camera *camera = (Camera *)data;
    PTPParams *params = &camera->pl->params;
    uint32_t parent, storage=0x0000000;
    unsigned int i, hasgetstorageids;
    SET_CONTEXT_P(params, context);
    unsigned int	lastnrofobjects = params->nrofobjects, redoneonce = 0;

    GP_LOG_D ("file_list_func(%s)", folder);

    /* There should be NO files in root folder */
    if (!strcmp(folder, "/"))
        return (GP_OK);

    if (!strcmp(folder, "/special")) {
	for (i=0; i<nrofspecial_files; i++)
		CR (gp_list_append (list, special_files[i].name, NULL));
	return (GP_OK);
    }

    /* compute storage ID value from folder patch */
    folder_to_storage(folder,storage);

    /* Get (parent) folder handle omitting storage pseudofolder */
    find_folder_handle(params,folder,storage,parent);

    C_PTP_REP (ptp_list_folder (params, storage, parent));
    GP_LOG_D ("after list folder");

    hasgetstorageids = ptp_operation_issupported(params,PTP_OC_GetStorageIDs);

retry:
    for (i = 0; i < params->nrofobjects; i++) {
	PTPObject	*ob;
	uint16_t	ret;
	uint32_t	oid;

	/* not our parent -> next */
	C_PTP_REP (ptp_object_want (params, params->objects[i].oid, PTPOBJECT_PARENTOBJECT_LOADED|PTPOBJECT_STORAGEID_LOADED, &ob));

	/* DANGER DANGER: i is now invalid as objects might have been inserted in the list! */

	if (ob->oi.ParentObject!=parent)
		continue;

	/* not on our storage devices -> next */
	if ((hasgetstorageids && (ob->oi.StorageID != storage)))
		continue;

	oid = ob->oid; /* ob might change or even become invalid in the function below */
	ret = ptp_object_want (params, oid, PTPOBJECT_OBJECTINFO_LOADED, &ob);
	if (ret != PTP_RC_OK) {
		/* we might raced another delete or ongoing addition, seen on a D810 */
		if (ret == PTP_RC_InvalidObjectHandle) {
			GP_LOG_D ("Handle %08x was in list, but not/no longer found via getobjectinfo.\n", oid);
			/* remove it for now, we will readd it later if we see it again. */
			ptp_remove_object_from_cache(params, oid);
			continue;
		}
		C_PTP_REP (ret);
	}
	/* Is a directory -> next */
	if (ob->oi.ObjectFormat == PTP_OFC_Association)
		continue;

	debug_objectinfo(params, ob->oid, &ob->oi);

	if (!ob->oi.Filename)
	    continue;

	if (1) {
	    /* HP Photosmart 850, the camera tends to duplicate filename in the list.
             * Original patch by clement.rezvoy@gmail.com */
	    /* search backwards, likely gets hits faster. */
	    /* FIXME Marcus: This is also O(n^2) ... bad for large directories. */
	    if (GP_OK == gp_list_find_by_name(list, NULL, ob->oi.Filename)) {
		GP_LOG_E (
			"Duplicate filename '%s' in folder '%s'. Ignoring nth entry.\n",
			ob->oi.Filename, folder);
		continue;
	    }
	}
	CR(gp_list_append (list, ob->oi.Filename, NULL));
    }

    /* Did we change the object tree list during our traversal? if yes, redo the scan. */
    if (params->nrofobjects != lastnrofobjects) {
	if (redoneonce++) {
		GP_LOG_E("list changed again on second pass, returning anyway");
		return GP_OK;
	}
	lastnrofobjects = params->nrofobjects;
	gp_list_reset(list);
	goto retry;
    }
    return GP_OK;
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	PTPParams *params = &((Camera *)data)->pl->params;
	unsigned int i, hasgetstorageids;
	uint32_t handler,storage;
	unsigned int redoneonce = 0, lastnrofobjects = params->nrofobjects;

	SET_CONTEXT_P(params, context);
	GP_LOG_D ("folder_list_func(%s)", folder);
	/* add storage pseudofolders in root folder */
	if (!strcmp(folder, "/")) {
		/* use the cached storageids. they should be valid after camera_init */
		if (ptp_operation_issupported(params,PTP_OC_GetStorageIDs)) {
			char fname[PTP_MAXSTRLEN];

			if (!params->storageids.n) {
				snprintf(fname, sizeof(fname), STORAGE_FOLDER_PREFIX"%08x",0x00010001);
				CR (gp_list_append (list, fname, NULL));
			}
			for (i=0; i<params->storageids.n; i++) {

				/* invalid storage, storageinfo might fail on it (Nikon D300s e.g.) */
				if ((params->storageids.Storage[i]&0x0000ffff)==0) continue;
				snprintf(fname, sizeof(fname),
					STORAGE_FOLDER_PREFIX"%08x",
					params->storageids.Storage[i]);
				CR (gp_list_append (list, fname, NULL));
			}
		} else {
			char fname[PTP_MAXSTRLEN];
			snprintf(fname, sizeof(fname),
					STORAGE_FOLDER_PREFIX"%08x",
					0xdeadbeef
			);
			gp_list_append (list, fname, NULL);
		}
		if (nrofspecial_files)
			CR (gp_list_append (list, "special", NULL));
		return (GP_OK);
	}

	if (!strcmp(folder, "/special")) {
		/* no folders in here */
		return (GP_OK);
	}

	/* compute storage ID value from folder path */
	folder_to_storage (folder,storage);

	/* Get folder handle omitting storage pseudofolder */
	find_folder_handle (params,folder,storage,handler);

	/* list this directory */
	C_PTP_REP (ptp_list_folder (params, storage, handler));

        GP_LOG_D ("after list folder (storage=0x%08x, handler=0x08%x)", storage, handler);

	/* Look for objects we can present as directories.
	 * Currently we specify *any* PTP association as directory.
	 */
	hasgetstorageids = ptp_operation_issupported(params,PTP_OC_GetStorageIDs);
retry:
	for (i = 0; i < params->nrofobjects; i++) {
		PTPObject	*ob;
		uint16_t	ret;
		uint32_t	handle;

		C_PTP_REP (ptp_object_want (params, params->objects[i].oid, PTPOBJECT_STORAGEID_LOADED|PTPOBJECT_PARENTOBJECT_LOADED, &ob));

		if (ob->oi.ParentObject != handler)
			continue;
		if (hasgetstorageids && (ob->oi.StorageID != storage))
			continue;

		handle = ob->oid;
		ret = ptp_object_want (params, handle, PTPOBJECT_OBJECTINFO_LOADED, &ob);
		if (ret != PTP_RC_OK) {
			/* we might raced another delete or ongoing addition, seen on a D810 */
			if (ret == PTP_RC_InvalidObjectHandle) {
				GP_LOG_D ("Handle %08x was in list, but not/no longer found via getobjectinfo.\n", handle);
				/* remove it for now, we will readd it later if we see it again. */
				ptp_remove_object_from_cache(params, handle);
				continue;
			}
			C_PTP_REP (ret);
		}
		if (ob->oi.ObjectFormat!=PTP_OFC_Association)
			continue;
		GP_LOG_D ("adding 0x%x / ob=%p to folder", ob->oid, ob);
		if (GP_OK == gp_list_find_by_name(list, NULL, ob->oi.Filename)) {
			GP_LOG_E ( "Duplicated foldername '%s' in folder '%s'. should not happen!\n", ob->oi.Filename, folder);
			continue;
		}
		CR (gp_list_append (list, ob->oi.Filename, NULL));
	}
	if (lastnrofobjects != params->nrofobjects) {
		if (redoneonce++) {
			GP_LOG_E("list changed again on second pass, returning anyway");
			return GP_OK;
		}
		lastnrofobjects = params->nrofobjects;
		gp_list_reset (list);
		goto retry;
	}
	return GP_OK;
}

/* To avoid roundtrips for querying prop desc
 * that are uninteresting for us we list all
 * that are exposed by PTP anyway (and are r/o).
 */
static unsigned short uninteresting_props [] = {
	PTP_OPC_StorageID,
	PTP_OPC_ObjectFormat,
	PTP_OPC_ProtectionStatus,
	PTP_OPC_ObjectSize,
	PTP_OPC_AssociationType,
	PTP_OPC_AssociationDesc,
	PTP_OPC_ParentObject
};

static int
ptp_mtp_render_metadata (
	PTPParams *params, uint32_t object_id, uint16_t ofc, CameraFile *file
) {
	uint16_t ret, *props = NULL;
	uint32_t propcnt = 0;
	unsigned int j;
	MTPProperties	*mprops;
	PTPObject	*ob;

	C_PTP (ptp_object_want (params, object_id, PTPOBJECT_MTPPROPLIST_LOADED, &ob));

	/* ... use little helper call to see if we missed anything in the global
	 * retrieval. */
	C_PTP (ptp_mtp_getobjectpropssupported (params, ofc, &propcnt, &props));

	mprops = ob->mtpprops;
	if (mprops) { /* use the fast method, without device access since cached.*/
		char		propname[256];
		char		text[256];
		unsigned int	i, n;

		for (j=0;j<ob->nrofmtpprops;j++) {
			MTPProperties		*xpl = &mprops[j];

			for (i=0;i<sizeof(uninteresting_props)/sizeof(uninteresting_props[0]);i++)
				if (uninteresting_props[i] == xpl->property)
					break;
			/* Is uninteresting. */
			if (i != sizeof(uninteresting_props)/sizeof(uninteresting_props[0]))
				continue;

			for(i=0;i<propcnt;i++) {
				/* Mark handled property as 0 */
				if (props[i] == xpl->property) {
					props[i]=0;
					break;
				}
			}

			n = ptp_render_mtp_propname(xpl->property, sizeof(propname), propname);
			gp_file_append (file, "<", 1);
			gp_file_append (file, propname, n);
			gp_file_append (file, ">", 1);

			switch (xpl->datatype) {
			default:sprintf (text, "Unknown type %d", xpl->datatype);
				break;
			case PTP_DTC_STR:
				snprintf (text, sizeof(text), "%s", xpl->propval.str?xpl->propval.str:"");
				break;
			case PTP_DTC_INT64:
				sprintf (text, "%ld", xpl->propval.i64);
				break;
			case PTP_DTC_INT32:
				sprintf (text, "%d", xpl->propval.i32);
				break;
			case PTP_DTC_INT16:
				sprintf (text, "%d", xpl->propval.i16);
				break;
			case PTP_DTC_INT8:
				sprintf (text, "%d", xpl->propval.i8);
				break;
			case PTP_DTC_UINT64:
				sprintf (text, "%lu", xpl->propval.u64);
				break;
			case PTP_DTC_UINT32:
				sprintf (text, "%u", xpl->propval.u32);
				break;
			case PTP_DTC_UINT16:
				sprintf (text, "%u", xpl->propval.u16);
				break;
			case PTP_DTC_UINT8:
				sprintf (text, "%u", xpl->propval.u8);
				break;
			}
			gp_file_append (file, text, strlen(text));
			gp_file_append (file, "</", 2);
			gp_file_append (file, propname, n);
			gp_file_append (file, ">\n", 2);
		}
		/* fallthrough */
	}

	for (j=0;j<propcnt;j++) {
		char			propname[256];
		char			text[256];
		PTPObjectPropDesc	opd;
		int 			i, n;

		if (!props[j]) continue; /* handle above */

		for (i=sizeof(uninteresting_props)/sizeof(uninteresting_props[0]);i--;)
			if (uninteresting_props[i] == props[j])
				break;
		if (i != -1) /* Is uninteresting. */
			continue;

		n = ptp_render_mtp_propname(props[j], sizeof(propname), propname);
		gp_file_append (file, "<", 1);
		gp_file_append (file, propname, n);
		gp_file_append (file, ">", 1);

		ret = LOG_ON_PTP_E (ptp_mtp_getobjectpropdesc (params, props[j], ofc, &opd));
		if (ret == PTP_RC_OK) {
			PTPPropertyValue	pv;
			ret = ptp_mtp_getobjectpropvalue (params, object_id, props[j], &pv, opd.DataType);
			if (ret != PTP_RC_OK) {
				sprintf (text, "failure to retrieve %x of oid %x, ret %x", props[j], object_id, ret);
			} else {
				switch (opd.DataType) {
				default:sprintf (text, "Unknown type %d", opd.DataType);
					break;
				case PTP_DTC_STR:
					snprintf (text, sizeof(text), "%s", pv.str?pv.str:"");
					break;
				case PTP_DTC_INT64:
					sprintf (text, "%ld", pv.i64);
					break;
				case PTP_DTC_INT32:
					sprintf (text, "%d", pv.i32);
					break;
				case PTP_DTC_INT16:
					sprintf (text, "%d", pv.i16);
					break;
				case PTP_DTC_INT8:
					sprintf (text, "%d", pv.i8);
					break;
				case PTP_DTC_UINT64:
					sprintf (text, "%lu", pv.u64);
					break;
				case PTP_DTC_UINT32:
					sprintf (text, "%u", pv.u32);
					break;
				case PTP_DTC_UINT16:
					sprintf (text, "%u", pv.u16);
					break;
				case PTP_DTC_UINT8:
					sprintf (text, "%u", pv.u8);
					break;
				}
			}
			gp_file_append (file, text, strlen(text));
		}
		gp_file_append (file, "</", 2);
		gp_file_append (file, propname, n);
		gp_file_append (file, ">\n", 2);

	}
	free(props);
	return (GP_OK);
}

/* To avoid roundtrips for querying prop desc if it is R/O
 * we list all that are by standard means R/O.
 */
static unsigned short readonly_props [] = {
	PTP_OPC_StorageID,
	PTP_OPC_ObjectFormat,
	PTP_OPC_ProtectionStatus,
	PTP_OPC_ObjectSize,
	PTP_OPC_AssociationType,
	PTP_OPC_AssociationDesc,
	PTP_OPC_ParentObject,
	PTP_OPC_PersistantUniqueObjectIdentifier,
	PTP_OPC_DateAdded,
	PTP_OPC_CorruptOrUnplayable,
	PTP_OPC_RepresentativeSampleFormat,
	PTP_OPC_RepresentativeSampleSize,
	PTP_OPC_RepresentativeSampleHeight,
	PTP_OPC_RepresentativeSampleWidth,
	PTP_OPC_RepresentativeSampleDuration
};

static int
ptp_mtp_parse_metadata (
	PTPParams *params, uint32_t object_id, uint16_t ofc, CameraFile *file
) {
	uint16_t ret, *props = NULL;
	uint32_t propcnt = 0;
	char	*filedata = NULL;
	unsigned long	filesize = 0;
	unsigned int j;

	if (gp_file_get_data_and_size (file, (const char**)&filedata, &filesize) < GP_OK)
		return (GP_ERROR);

	C_PTP (ptp_mtp_getobjectpropssupported (params, ofc, &propcnt, &props));

	for (j=0;j<propcnt;j++) {
		char			propname[256],propname2[256+4];
		char			*begin, *end, *content;
		PTPObjectPropDesc	opd;
		int 			i;
		PTPPropertyValue	pv;

		for (i=sizeof(readonly_props)/sizeof(readonly_props[0]);i--;)
			if (readonly_props[i] == props[j])
				break;
		if (i != -1) /* Is read/only */
			continue;
		ptp_render_mtp_propname(props[j], sizeof(propname), propname);
		sprintf (propname2, "<%s>", propname);
		begin= strstr (filedata, propname2);
		if (!begin) continue;
		begin += strlen(propname2);
		sprintf (propname2, "</%s>", propname);
		end = strstr (begin, propname2);
		if (!end) continue;
		*end = '\0';
		content = strdup(begin);
		if (!content) {
			free (props);
			C_MEM (content);
		}
		*end = '<';
		GP_LOG_D ("found tag %s, content %s", propname, content);
		ret = LOG_ON_PTP_E (ptp_mtp_getobjectpropdesc (params, props[j], ofc, &opd));
		if (ret != PTP_RC_OK) {
			free (content); content = NULL;
			continue;
		}
		if (opd.GetSet == 0) {
			GP_LOG_D ("Tag %s is read only, sorry.", propname);
			free (content); content = NULL;
			continue;
		}
		switch (opd.DataType) {
		default:GP_LOG_E ("mtp parser: Unknown datatype %d, content %s", opd.DataType, content);
			free (content); content = NULL;
			continue;
			break;
		case PTP_DTC_STR:
			pv.str = content;
			break;
		case PTP_DTC_INT32:
			sscanf (content, "%d", &pv.i32);
			break;
		case PTP_DTC_INT16:
			sscanf (content, "%hd", &pv.i16);
			break;
		case PTP_DTC_INT8:
			sscanf (content, "%hhd", &pv.i8);
			break;
		case PTP_DTC_UINT32:
			sscanf (content, "%u", &pv.u32);
			break;
		case PTP_DTC_UINT16:
			sscanf (content, "%hu", &pv.u16);
			break;
		case PTP_DTC_UINT8:
			sscanf (content, "%hhu", &pv.u8);
			break;
		}
		ret = ptp_mtp_setobjectpropvalue (params, object_id, props[j], &pv, opd.DataType);
		free (content); content = NULL;
	}
	free(props);
	return (GP_OK);
}

static int
mtp_get_playlist_string(
	Camera *camera, uint32_t object_id, char **xcontent, int *xcontentlen
) {
	PTPParams *params = &camera->pl->params;
	uint32_t	numobjects = 0, *objects = NULL;
	unsigned int	i, contentlen = 0;
	char		*content = NULL;

	C_PTP (ptp_mtp_getobjectreferences (params, object_id, &objects, &numobjects));

	for (i=0;i<numobjects;i++) {
		char		buf[4096];
		int		len;
		PTPObject 	*ob;

		memset(buf, 0, sizeof(buf));
		len = 0;
		object_id = objects[i];
		do {
			C_PTP (ptp_object_want (params, object_id, PTPOBJECT_OBJECTINFO_LOADED, &ob));
			/* make space for new filename */
			memmove (buf+strlen(ob->oi.Filename)+1, buf, len);
			memcpy (buf+1, ob->oi.Filename, strlen (ob->oi.Filename));
			buf[0] = '/';
			object_id = ob->oi.ParentObject;
			len = strlen(buf);
		} while (object_id != 0);
		memmove (buf+strlen("/store_00010001"), buf, len);
		sprintf (buf,"/store_%08x",(unsigned int)ob->oi.StorageID);
		buf[strlen(buf)]='/';
		len = strlen(buf);

		C_MEM (content = realloc (content, contentlen+len+1+1));
		strcpy (content+contentlen, buf);
		strcpy (content+contentlen+len, "\n");
		contentlen += len+1;
	}
	if (!content)
		C_MEM (content = malloc(1));
	if (xcontent)
		*xcontent = content;
	else
		free (content);
	*xcontentlen = contentlen;
	free (objects);
	return (GP_OK);
}

static int
mtp_put_playlist(
	Camera *camera, char *content, int contentlen, PTPObjectInfo *oi, GPContext *context
) {
	char 		*s = content;
	unsigned char	data[1];
	uint32_t	storage = 0, objectid, playlistid;
	uint32_t	*oids = NULL;
	int		nrofoids = 0;
	PTPParams 	*params = &camera->pl->params;

	while (*s) {
		char *t = strchr(s,'\n');
		char *fn, *filename;
		if (t) {
			C_MEM (fn = malloc (t-s+1));
			memcpy (fn, s, t-s);
			fn[t-s]='\0';
		} else {
			C_MEM (fn = strdup(s));
		}
		filename = strrchr (fn,'/');
		if (!filename) {
			free (fn);
			if (!t) break;
			s = t+1;
			continue;
		}
		*filename = '\0';filename++;

		/* compute storage ID value from folder patch */
		folder_to_storage(fn,storage);
		/* Get file number omitting storage pseudofolder */
		find_folder_handle(params, fn, storage, objectid);
		objectid = find_child(params, filename, storage, objectid, NULL);
		if (objectid != PTP_HANDLER_SPECIAL) {
			C_MEM (oids = realloc(oids, sizeof(oids[0])*(nrofoids+1)));
			oids[nrofoids] = objectid;
			nrofoids++;
		} else {
			/*fprintf (stderr,"%s/%s NOT FOUND!\n", fn, filename);*/
			GP_LOG_E ("Object %s/%s not found on device.", fn, filename);
		}
		free (fn);
		if (!t) break;
		s = t+1;
	}
	oi->ObjectCompressedSize = 1;
	oi->ObjectFormat = PTP_OFC_MTP_AbstractAudioVideoPlaylist;
	C_PTP_MSG (ptp_sendobjectinfo(&camera->pl->params, &storage, &oi->ParentObject, &playlistid, oi),
		   "failed sendobjectinfo of playlist.");
	C_PTP_MSG (ptp_sendobject(&camera->pl->params, (unsigned char*)data, 1),
		   "failed dummy sendobject of playlist.");
	C_PTP (ptp_mtp_setobjectreferences (&camera->pl->params, playlistid, oids, nrofoids));
	free (oids);
	/* update internal structures */
	return add_object(camera, playlistid, context);
}

static int
mtp_get_playlist(
	Camera *camera, CameraFile *file, uint32_t object_id, GPContext *context
) {
	char	*content;
	int	contentlen;

	CR (mtp_get_playlist_string( camera, object_id, &content, &contentlen));
	/* takes ownership of content */
	return gp_file_set_data_and_size (file, content, contentlen);
}

typedef struct {
	CameraFile	*file;
} PTPCFHandlerPrivate;

static uint16_t
gpfile_getfunc (PTPParams *params, void *xpriv,
	unsigned long wantlen, unsigned char *bytes,
	unsigned long *gotlen
) {
	PTPCFHandlerPrivate* priv = (PTPCFHandlerPrivate*)xpriv;
	int ret;
	size_t	gotlensize;

	ret = gp_file_slurp (priv->file, (char*)bytes, wantlen, &gotlensize);
	*gotlen = gotlensize;
	if (ret != GP_OK)
		return PTP_ERROR_IO;
	return PTP_RC_OK;
}

static uint16_t
gpfile_putfunc (PTPParams *params, void *xpriv,
	unsigned long sendlen, unsigned char *bytes
) {
	PTPCFHandlerPrivate* priv= (PTPCFHandlerPrivate*)xpriv;
	int ret;

	ret = gp_file_append (priv->file, (char*)bytes, sendlen);
	if (ret != GP_OK)
		return PTP_ERROR_IO;
	return PTP_RC_OK;
}

uint16_t
ptp_init_camerafile_handler (PTPDataHandler *handler, CameraFile *file) {
	PTPCFHandlerPrivate* priv = malloc (sizeof(PTPCFHandlerPrivate));
	if (!priv) return PTP_RC_GeneralError;
	handler->priv = priv;
	handler->getfunc = gpfile_getfunc;
	handler->putfunc = gpfile_putfunc;
	priv->file = file;
	return PTP_RC_OK;
}

uint16_t
ptp_exit_camerafile_handler (PTPDataHandler *handler) {
	free (handler->priv);
	return PTP_RC_OK;
}


static int
read_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	        CameraFileType type,
		uint64_t offset64, char *buf, uint64_t *size64,
		void *data, GPContext *context)
{
	Camera *camera = data;
	PTPParams *params = &camera->pl->params;
	/* Note that "image" points to unsigned chars whereas all the other
	 * functions which set image return pointers to chars.
	 * However, we calculate a number of unsigned values in this function,
	 * so we cannot make it signed either.
	 * Therefore, sometimes a "ximage" char* helper, since wild casts of pointers
	 * confuse the compilers aliasing mechanisms.
	 * If you do not like that, feel free to clean up the datatypes.
	 * (TODO for Marcus and 2.2 ;)
	 */
	uint32_t oid;
	uint32_t storage;
	uint64_t obj_size;
	uint32_t offset32 = offset64, size32 = *size64;
	PTPObject *ob;

	SET_CONTEXT_P(params, context);

	C_PARAMS_MSG (*size64 <= 0xffffffff, "size exceeds 32bit");
	C_PARAMS_MSG (strcmp (folder, "/special"), "file not found");

	if (!ptp_operation_issupported(params, PTP_OC_GetPartialObject) &&
		!(	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_MTP) &&
			ptp_operation_issupported(params, PTP_OC_ANDROID_GetPartialObject64)
		)
	)
		return GP_ERROR_NOT_SUPPORTED;

	if (!(  (params->deviceinfo.VendorExtensionID == PTP_VENDOR_MTP) &&
		ptp_operation_issupported(params, PTP_OC_ANDROID_GetPartialObject64)) && (offset64 > 0xffffffff) ) {
		GP_LOG_E ("Invalid parameters: offset exceeds 32 bits but the device doesn't support GetPartialObject64.");
		return GP_ERROR_NOT_SUPPORTED;
	}

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);
	/* Get file number omitting storage pseudofolder */
	find_folder_handle(params, folder, storage, oid);
	oid = find_child(params, filename, storage, oid, &ob);
	if (oid == PTP_HANDLER_SPECIAL) {
		gp_context_error (context, _("File '%s/%s' does not exist."), folder, filename);
		return GP_ERROR_BAD_PARAMETERS;
	}
	GP_LOG_D ("Reading %u bytes from file '%s' at offset %lu.", size32, filename, offset64);
	switch (type) {
	default:
		return GP_ERROR_NOT_SUPPORTED;
	case GP_FILE_TYPE_NORMAL: {
		uint16_t	ret;
		unsigned char	*xdata;

		/* We do not allow downloading unknown type files as in most
		cases they are special file (like firmware or control) which
		sometimes _cannot_ be downloaded. doing so we avoid errors.*/
		/* however this avoids the possibility to download files on
		 * Androids ... doh. Let reenable it again. */
		if (ob->oi.ObjectFormat == PTP_OFC_Association
	/*
			|| (ob->oi.ObjectFormat == PTP_OFC_Undefined &&
				((ob->oi.ThumbFormat == PTP_OFC_Undefined) ||
				 (ob->oi.ThumbFormat == 0)
			)
			) */
		)
			return (GP_ERROR_NOT_SUPPORTED);

		if (is_mtp_capable (camera) &&
		    (ob->oi.ObjectFormat == PTP_OFC_MTP_AbstractAudioVideoPlaylist))
			return (GP_ERROR_NOT_SUPPORTED);

		obj_size = ob->oi.ObjectCompressedSize;
		if (!obj_size)
			return GP_ERROR_NOT_SUPPORTED;

		if (offset64 >= obj_size) {
			*size64 = 0;
			return GP_OK;
		} else if (size32 + offset64 > obj_size) {
			size32 = obj_size - offset64;
		}

		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_MTP) &&
			ptp_operation_issupported(params, PTP_OC_ANDROID_GetPartialObject64)
		) {
			ret = ptp_android_getpartialobject64(params, oid, offset64, size32, &xdata, &size32);
		} else {
			ret = ptp_getpartialobject(params, oid, offset32, size32, &xdata, &size32);
		}
		if (ret == PTP_ERROR_CANCEL)
			return GP_ERROR_CANCEL;
		C_PTP_REP (ret);
		*size64 = size32;
		memcpy (buf, xdata, size32);
		free (xdata);
		/* clear the "new" flag on Canons */
		if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
			(ob->canon_flags & 0x20) &&
			ptp_operation_issupported(params,PTP_OC_CANON_SetObjectArchive)
		) {
			/* seems just a byte (0x20 - new) */
			/* can fail on some models, like S5. Ignore errors. */
			ret = LOG_ON_PTP_E (ptp_canon_setobjectarchive (params, oid, ob->canon_flags & ~0x20));
			if (ret == PTP_RC_OK)
				ob->canon_flags &= ~0x20;
		} else if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
			(ob->canon_flags & 0x20) &&
			ptp_operation_issupported(params,PTP_OC_CANON_EOS_SetObjectAttributes)
		) {
			/* seems just a byte (0x20 - new) */
			/* can fail on some models, like S5. Ignore errors. */
			ret = LOG_ON_PTP_E (ptp_canon_eos_setobjectattributes(params, oid, ob->canon_flags & ~0x20));
			if (ret == PTP_RC_OK)
				ob->canon_flags &= ~0x20;
		}
		}
		break;
	}
	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	/* Note that "image" points to unsigned chars whereas all the other
	 * functions which set image return pointers to chars.
	 * However, we calculate a number of unsigned values in this function,
	 * so we cannot make it signed either.
	 * Therefore, sometimes a "ximage" char* helper, since wild casts of pointers
	 * confuse the compilers aliasing mechanisms.
	 * If you do not like that, feel free to clean up the datatypes.
	 * (TODO for Marcus and 2.2 ;)
	 */
	uint32_t oid;
	uint64_t size;
	uint32_t storage;
	PTPObject *ob;
	PTPParams *params = &camera->pl->params;

	SET_CONTEXT_P(params, context);

#if 0
	/* The new Canons like to switch themselves off in the middle. */
	if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) {
		if (ptp_operation_issupported(params, PTP_OC_CANON_KeepDeviceOn))
			ptp_canon_keepdeviceon (params);
	}
#endif

	if (!strcmp (folder, "/special")) {
		unsigned int i;

		for (i=0;i<nrofspecial_files;i++)
			if (!strcmp (special_files[i].name, filename))
				return special_files[i].getfunc (fs, folder, filename, type, file, data, context);
		return (GP_ERROR_BAD_PARAMETERS); /* file not found */
	}

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);
	/* Get file number omitting storage pseudofolder */
	find_folder_handle(params, folder, storage, oid);
	oid = find_child(params, filename, storage, oid, &ob);
	if (oid == PTP_HANDLER_SPECIAL) {
		gp_context_error (context, _("File '%s/%s' does not exist."), folder, filename);
		return GP_ERROR_BAD_PARAMETERS;
	}
	if (ob->oi.ModificationDate != 0)
		gp_file_set_mtime (file, ob->oi.ModificationDate);
	else
		gp_file_set_mtime (file, ob->oi.CaptureDate);

	GP_LOG_D ("Getting file '%s'.", filename);
	switch (type) {
	case	GP_FILE_TYPE_EXIF: {
		uint32_t offset, xlen, maxbytes;
		unsigned char 	*ximage = NULL;

		/* Check if we have partial downloads. Otherwise we can just hope
		 * upstream downloads the whole image to get EXIF data. */
		if (!ptp_operation_issupported(params, PTP_OC_GetPartialObject))
			return (GP_ERROR_NOT_SUPPORTED);
		/* Device may hang is a partial read is attempted beyond the file */
		if (ob->oi.ObjectCompressedSize < 10)
			return (GP_ERROR_NOT_SUPPORTED);

		/* We only support JPEG / EXIF format ... others might hang. */
		if (ob->oi.ObjectFormat != PTP_OFC_EXIF_JPEG)
			return (GP_ERROR_NOT_SUPPORTED);

		/* Note: Could also use Canon partial downloads */
		C_PTP_REP (ptp_getpartialobject (params,
			oid, 0, 10, &ximage, &xlen));

		if (!((ximage[0] == 0xff) && (ximage[1] == 0xd8))) {	/* SOI */
			free (ximage);
			return (GP_ERROR_NOT_SUPPORTED);
		}
		if (!((ximage[2] == 0xff) && (ximage[3] == 0xe1))) {	/* App1 */
			free (ximage);
			return (GP_ERROR_NOT_SUPPORTED);
		}
		if (0 != memcmp(ximage+6, "Exif", 4)) {
			free (ximage);
			return (GP_ERROR_NOT_SUPPORTED);
		}
		offset = 2;
		maxbytes = (ximage[4] << 8) + ximage[5] + 2;
		free (ximage);
		ximage = NULL;
		C_PTP_REP (ptp_getpartialobject (params,
			oid, offset, maxbytes, &ximage, &xlen));
		CR (gp_file_set_data_and_size (file, (char*)ximage, xlen));
		break;
	}
	case	GP_FILE_TYPE_PREVIEW: {
		unsigned char	*ximage = NULL;
		unsigned int	xlen;
		char		buf[200];

		/* If thumb size is 0, and the ofc is not an image type (0x38xx or 0xb8xx)
		 * then there is no thumbnail at all... */
		size=ob->oi.ThumbCompressedSize;
		if((size==0) && (
			((ob->oi.ObjectFormat & 0x7800) != 0x3800) &&
			((ob->oi.ObjectFormat != PTP_OFC_CANON_CRW)) &&
			((ob->oi.ObjectFormat != PTP_OFC_CANON_MOV)) &&
			((ob->oi.ObjectFormat != PTP_OFC_CANON_MOV2)) &&
			((ob->oi.ObjectFormat != PTP_OFC_CANON_CRW3)) &&
			((ob->oi.ObjectFormat != PTP_OFC_CANON_CR3))
		))
			return GP_ERROR_NOT_SUPPORTED;

		if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON)	&&
			ptp_operation_issupported(params,PTP_OC_NIKON_GetLargeThumb)	&&
			(GP_OK == gp_setting_get("ptp2","thumbsize",buf))		&&
			!strcmp(buf,"large")
		) {
			C_PTP_REP (ptp_nikon_getlargethumb(params, oid, &ximage, &xlen));
		} else {
			C_PTP_REP (ptp_getthumb(params, oid, &ximage, &xlen));
		}

		set_mimetype (file, params->deviceinfo.VendorExtensionID, ob->oi.ThumbFormat);
		CR (gp_file_set_data_and_size (file, (char*)ximage, xlen));
		break;
	}
	case	GP_FILE_TYPE_METADATA:
		if (is_mtp_capable (camera) &&
		    ptp_operation_issupported(params,PTP_OC_MTP_GetObjectPropsSupported)
		)
			return ptp_mtp_render_metadata (params,oid,ob->oi.ObjectFormat,file);
		return (GP_ERROR_NOT_SUPPORTED);
	default: {
		if (ob->oi.ObjectFormat == PTP_OFC_Association)
			return GP_ERROR_NOT_SUPPORTED;

		/* We do not allow downloading unknown type files as in most
		 * cases they are special file (like firmware or control) which
		 * sometimes _cannot_ be downloaded. doing so we avoid errors.
		 * Allow all types from MTP devices, like phones.
		 */
		if (	!is_mtp_capable (camera) &&
			(ob->oi.ObjectFormat == PTP_OFC_Undefined &&
				((ob->oi.ThumbFormat == PTP_OFC_Undefined) ||
				 (ob->oi.ThumbFormat == 0)
			)
			)
		)
			return (GP_ERROR_NOT_SUPPORTED);

		if (is_mtp_capable (camera) &&
		    (ob->oi.ObjectFormat == PTP_OFC_MTP_AbstractAudioVideoPlaylist))
			return mtp_get_playlist (camera, file, oid, context);

		size=ob->oi.ObjectCompressedSize;
#define BLOBSIZE 1*1024*1024
		if (size > 0xffffffffUL) {	/* larger than 4GB */
			if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
				(ptp_operation_issupported(params,PTP_OC_NIKON_GetPartialObjectEx))
			) {
#define NIKONBLOBSIZE 10*1024*1024
				unsigned char	*ximage = NULL;
				uint64_t 	offset = 0;

				while (offset < size) {
					uint64_t	xsize = size - offset;
					uint32_t	xlen;

					if (xsize > NIKONBLOBSIZE)
						xsize = NIKONBLOBSIZE;
					C_PTP_REP (ptp_nikon_getpartialobjectex (params, oid, offset, xsize, &ximage, &xlen));
					gp_file_append (file, (char*)ximage, xlen);
					free (ximage);
					ximage = NULL;
					offset += xlen;
					if (!xlen) {
						GP_LOG_E ("nikon_getpartialobject loop: offset=%ld, size is %ld, xlen returned is 0?", offset, size);
						break;
					}
				}
				goto done;
			}
			/* fallthrough to the ptp_getobject method */
		}
		/* We also need this for Nikon D850 and very big RAWs (>40 MB) */
		/* Try the generic method first, EOS R does not like the second for some reason */
		if (	(ptp_operation_issupported(params,PTP_OC_GetPartialObject)) &&
			(size > BLOBSIZE) && (size <= 0xffffffffUL)
		) {
				unsigned char	*ximage = NULL;
				uint32_t 	offset = 0;

				while (offset < size) {
					uint32_t	xsize = size - offset;
					uint32_t	xlen;

					if (xsize > BLOBSIZE)
						xsize = BLOBSIZE;
					C_PTP_REP (ptp_getpartialobject (params, oid, offset, xsize, &ximage, &xlen));
					gp_file_append (file, (char*)ximage, xlen);
					free (ximage);
					ximage = NULL;
					offset += xlen;
					if (!xlen) {
						GP_LOG_E ("getpartialobject loop: offset=%d, size is %ld, xlen returned is 0?", offset, size);
						break;
					}
				}
				goto done;
		}
		/* EOS software uses 1MB blobs, use that too... EOS R does not like 5MB blobs */
		if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
			(ptp_operation_issupported(params,PTP_OC_CANON_EOS_GetPartialObject)) &&
			(size > BLOBSIZE)
		) {
				unsigned char	*ximage = NULL;
				uint32_t 	offset = 0;

				while (offset < size) {
					uint32_t	xsize = size - offset;

					if (xsize > BLOBSIZE)
						xsize = BLOBSIZE;
					C_PTP_REP (ptp_getpartialobject (params, oid, offset, xsize, &ximage, &xsize));
					gp_file_append (file, (char*)ximage, xsize);
					free (ximage);
					ximage = NULL;
					offset += xsize;
					if (!xsize) {
						GP_LOG_E ("getpartialobject loop: offset=%d, size is %ld, xlen returned is 0?", offset, size);
						break;
					}
				}
				goto done;
		}
#undef BLOBSIZE
		if (size) {
			uint16_t	ret;
			PTPDataHandler	handler;

			ptp_init_camerafile_handler (&handler, file);
			ret = ptp_getobject_to_handler(params, oid, &handler);
			ptp_exit_camerafile_handler (&handler);
			if (ret == PTP_ERROR_CANCEL)
				return GP_ERROR_CANCEL;
			C_PTP_REP (ret);
		} else {
			unsigned char *ximage = NULL;
			/* Do not download 0 sized files.
			 * It is not necessary and even breaks for some camera special files.
			 */
			C_MEM (ximage = malloc(1));
			CR (gp_file_set_data_and_size (file, (char*)ximage, size));
		}
done:

		/* clear the "new" flag on Canons */
		if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
			(ob->canon_flags & 0x20) &&
			ptp_operation_issupported(params,PTP_OC_CANON_SetObjectArchive)
		) {
			uint16_t	ret;

			/* seems just a byte (0x20 - new) */
			/* can fail on some models, like S5. Ignore errors. */
			ret = LOG_ON_PTP_E (ptp_canon_setobjectarchive (params, oid, ob->canon_flags & ~0x20));
			if (ret == PTP_RC_OK)
				ob->canon_flags &= ~0x20;
		} else if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
			(ob->canon_flags & 0x20) &&
			ptp_operation_issupported(params,PTP_OC_CANON_EOS_SetObjectAttributes)
		) {
			uint16_t	ret;

			/* seems just a byte (0x20 - new) */
			/* can fail on some models, like S5. Ignore errors. */
			ret = LOG_ON_PTP_E (ptp_canon_eos_setobjectattributes(params, oid, ob->canon_flags & ~0x20));
			if (ret == PTP_RC_OK)
				ob->canon_flags &= ~0x20;
		}
		break;
	}
	}
	return set_mimetype (file, params->deviceinfo.VendorExtensionID, ob->oi.ObjectFormat);
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
		CameraFileType type, CameraFile *file, void *data, GPContext *context)
{
	Camera *camera = data;
	PTPObjectInfo oi;
	uint32_t parent;
	uint32_t storage;
	uint32_t handle;
	unsigned long intsize;
	PTPParams* params = &camera->pl->params;

	SET_CONTEXT_P(params, context);
	camera->pl->checkevents = TRUE;

	GP_LOG_D ("folder=%s, filename=%s", folder, filename);

	if (!strcmp (folder, "/special")) {
		unsigned int i;

		for (i=0;i<nrofspecial_files;i++)
			if (!strcmp (special_files[i].name, filename))
				return special_files[i].putfunc (fs, folder, file, data, context);
		return (GP_ERROR_BAD_PARAMETERS); /* file not found */
	}
	memset(&oi, 0, sizeof (PTPObjectInfo));
	if (type == GP_FILE_TYPE_METADATA) {
		if (is_mtp_capable (camera) &&
		    ptp_operation_issupported(params,PTP_OC_MTP_GetObjectPropsSupported)
		) {
			uint32_t object_id;
			PTPObject	*ob;

			/* compute storage ID value from folder patch */
			folder_to_storage(folder,storage);

			/* Get file number omitting storage pseudofolder */
			find_folder_handle(params, folder, storage, object_id);
			object_id = find_child(params, filename, storage, object_id, &ob);
			if (object_id ==PTP_HANDLER_SPECIAL) {
				gp_context_error (context, _("File '%s/%s' does not exist."), folder, filename);
				return (GP_ERROR_BAD_PARAMETERS);
			}
			return ptp_mtp_parse_metadata (params,object_id,ob->oi.ObjectFormat,file);
		}
		gp_context_error (context, _("Metadata only supported for MTP devices."));
		return GP_ERROR_NOT_SUPPORTED;
	}
	C_PARAMS (type == GP_FILE_TYPE_NORMAL);
	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* get parent folder id omitting storage pseudofolder */
	find_folder_handle(params,folder,storage,parent);

	/* if you desire to put file to root folder, you have to use
	 * 0xffffffff instead of 0x00000000 (which means responder decide).
	 */
	if (parent==PTP_HANDLER_ROOT) parent=PTP_HANDLER_SPECIAL;

	/* We don't really want a file to exist with the same name twice. */
	handle = find_child (params, filename, storage, parent, NULL);
	if (handle != PTP_HANDLER_SPECIAL) {
		GP_LOG_D ("%s/%s exists.", folder, filename);
		return GP_ERROR_FILE_EXISTS;
	}

	oi.Filename=(char *)filename;
	oi.ObjectFormat = get_mimetype(camera, file, params->deviceinfo.VendorExtensionID);
	oi.ParentObject = parent;
	gp_file_get_mtime(file, &oi.ModificationDate);

	if (is_mtp_capable (camera) &&
	    (	strstr(filename,".zpl") || strstr(filename, ".pla") )) {
		char *object;
		gp_file_get_data_and_size (file, (const char**)&object, &intsize);
		return mtp_put_playlist (camera, object, intsize, &oi, context);
	}

	/* If the device is using PTP_VENDOR_EASTMAN_KODAK extension try
	 * PTP_OC_EK_SendFileObject.
	 */
	gp_file_get_data_and_size (file, NULL, &intsize);
	oi.ObjectCompressedSize = intsize;
	if ((params->deviceinfo.VendorExtensionID==PTP_VENDOR_EASTMAN_KODAK) &&
		(ptp_operation_issupported(params, PTP_OC_EK_SendFileObject)))
	{
		PTPDataHandler handler;
		C_PTP_REP (ptp_ek_sendfileobjectinfo (params, &storage,
			&parent, &handle, &oi));
		ptp_init_camerafile_handler (&handler, file);
		C_PTP_REP (ptp_ek_sendfileobject_from_handler (params, &handler, intsize));
		ptp_exit_camerafile_handler (&handler);
	} else if (ptp_operation_issupported(params, PTP_OC_SendObjectInfo)) {
		uint16_t	ret;
		PTPDataHandler handler;

		C_PTP_REP (ptp_sendobjectinfo (params, &storage,
			&parent, &handle, &oi));
		ptp_init_camerafile_handler (&handler, file);
		ret = ptp_sendobject_from_handler (params, &handler, intsize);
		ptp_exit_camerafile_handler (&handler);
		if (ret == PTP_ERROR_CANCEL)
			return (GP_ERROR_CANCEL);
		C_PTP_REP (ret);
	} else {
		GP_LOG_D ("The device does not support uploading files.");
		return GP_ERROR_NOT_SUPPORTED;
	}
	/* update internal structures */
	return add_object(camera, handle, context);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
			const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	uint32_t	oid;
	uint32_t	storage;
	PTPParams *params = &camera->pl->params;

	SET_CONTEXT_P(params, context);

	if (!strcmp (folder, "/special"))
		return GP_ERROR_NOT_SUPPORTED;

	/* virtual file created by Nikon special capture */
	if (	((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) ||
		 (params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) ||
		 (params->deviceinfo.VendorExtensionID == PTP_VENDOR_FUJI) ||
		 (params->deviceinfo.VendorExtensionID == PTP_VENDOR_GP_OLYMPUS_OMD) ||
		 (params->deviceinfo.VendorExtensionID == PTP_VENDOR_GP_SIGMAFP) ||
		 (params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) ||
		 (params->device_flags & DEVICE_FLAG_OLYMPUS_XML_WRAPPED)) &&
		!strncmp (filename, "capt", 4)
	)
		return GP_OK;
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_GP_SIGMAFP) &&
		!strncmp (filename,  "SDIM", 4)
	)
		return GP_OK;

	if (!ptp_operation_issupported(params, PTP_OC_DeleteObject))
		return GP_ERROR_NOT_SUPPORTED;

	camera->pl->checkevents = TRUE;
	C_PTP_REP (ptp_check_event (params));
	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);
	/* Get file number omitting storage pseudofolder */
	find_folder_handle(params, folder, storage, oid);
	oid = find_child(params, filename, storage, oid, NULL);

	/* in some cases we return errors ... just ignore them for now */
	LOG_ON_PTP_E (ptp_deleteobject(params, oid, 0));

	/* On some Canon firmwares, a DeleteObject causes a ObjectRemoved event
	 * to be sent. At least on Digital IXUS II and PowerShot A85. But
         * not on 350D.
	 */
	if (DELETE_SENDS_EVENT(params) &&
	    ptp_event_issupported(params, PTP_EC_ObjectRemoved)) {
		PTPContainer event;

		ptp_check_event (params); /* ignore errors */
		while (ptp_get_one_event (params, &event)) {
			if (event.Code == PTP_EC_ObjectRemoved)
				break;
			if (event.Code == PTP_EC_ObjectAdded) {
				PTPObject *ob;

				ptp_object_want (params, event.Param1, 0, &ob);
			}
		}
		/* FIXME: need to handle folder additions during capture-image-and-download */
 	}
	return (GP_OK);
}

static int
remove_dir_func (CameraFilesystem *fs, const char *folder,
			const char *foldername, void *data, GPContext *context)
{
	Camera *camera = data;
	uint32_t oid;
	uint32_t storage;
	PTPParams *params = &camera->pl->params;

	SET_CONTEXT_P(params, context);

	if (!ptp_operation_issupported(params, PTP_OC_DeleteObject))
		return GP_ERROR_NOT_SUPPORTED;
	camera->pl->checkevents = TRUE;
	C_PTP_REP (ptp_check_event (params));
	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);
	/* Get file number omitting storage pseudofolder */
	find_folder_handle(params, folder, storage, oid);
	oid = find_child(params, foldername, storage, oid, NULL);
	if (oid == PTP_HANDLER_SPECIAL)
		return GP_ERROR;
	C_PTP_REP (ptp_deleteobject(params, oid, 0));
	return (GP_OK);
}

static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo info, void *data, GPContext *context)
{
	Camera *camera = data;
	PTPObject *ob;
	uint32_t object_id;
	uint32_t storage;
	PTPParams *params = &camera->pl->params;

	SET_CONTEXT_P(params, context);

	C_PARAMS (strcmp (folder, "/special"));

	camera->pl->checkevents = TRUE;
	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);
	/* Get file number omitting storage pseudofolder */
	find_folder_handle(params, folder, storage, object_id);
	object_id = find_child(params, filename, storage, object_id, &ob);
	if (object_id == PTP_HANDLER_SPECIAL)
		return GP_ERROR;

	if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
		uint16_t newprot;

		if ((info.file.permissions & GP_FILE_PERM_DELETE) != GP_FILE_PERM_DELETE)
			newprot = PTP_PS_ReadOnly;
		else
			newprot = PTP_PS_NoProtection;
		if (ob->oi.ProtectionStatus != newprot) {
			if (!ptp_operation_issupported(params, PTP_OC_SetObjectProtection)) {
				gp_context_error (context, _("Device does not support setting object protection."));
				return (GP_ERROR_NOT_SUPPORTED);
			}
			C_PTP_REP_MSG (ptp_setobjectprotection (params, object_id, newprot),
				       _("Device failed to set object protection to %d"), newprot);
			ob->oi.ProtectionStatus = newprot; /* should actually reread objectinfo to be sure, but lets not. */
		}
		info.file.fields &= ~GP_FILE_INFO_PERMISSIONS;
		/* fall through */
	}
	/* ... no more cases implemented yet */
	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	PTPObject *ob;
	uint32_t oid, storage;
	PTPParams *params = &camera->pl->params;

	SET_CONTEXT_P(params, context);

	C_PARAMS (strcmp (folder, "/special"));

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);
	/* Get file number omitting storage pseudofolder */
	find_folder_handle(params, folder, storage, oid);
	oid = find_child(params, filename, storage, oid, &ob);
	if (oid == PTP_HANDLER_SPECIAL)
		return GP_ERROR;

	info->file.fields = GP_FILE_INFO_SIZE|GP_FILE_INFO_TYPE|GP_FILE_INFO_MTIME;
	info->file.size   = ob->oi.ObjectCompressedSize;

	if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) {
		info->file.fields |= GP_FILE_INFO_STATUS;
		if (ob->canon_flags & 0x20)
			info->file.status = GP_FILE_STATUS_NOT_DOWNLOADED;
		else
			info->file.status = GP_FILE_STATUS_DOWNLOADED;
	}

	/* MTP playlists have their own size calculation */
	if (is_mtp_capable (camera) &&
	    (ob->oi.ObjectFormat == PTP_OFC_MTP_AbstractAudioVideoPlaylist)) {
		int contentlen;
		CR (mtp_get_playlist_string (camera, oid, NULL, &contentlen));
		info->file.size = contentlen;
	}

	strcpy_mime (info->file.type, params->deviceinfo.VendorExtensionID, ob->oi.ObjectFormat);
	if (ob->oi.ModificationDate != 0) {
		info->file.mtime = ob->oi.ModificationDate;
	} else {
		info->file.mtime = ob->oi.CaptureDate;
	}

	switch (ob->oi.ProtectionStatus) {
	case PTP_PS_NoProtection:
		info->file.fields	|= GP_FILE_INFO_PERMISSIONS;
		info->file.permissions	 = GP_FILE_PERM_READ|GP_FILE_PERM_DELETE;
		break;
	case PTP_PS_ReadOnly:
		info->file.fields	|= GP_FILE_INFO_PERMISSIONS;
		info->file.permissions	 = GP_FILE_PERM_READ;
		break;
	default:
		GP_LOG_E ("mapping protection to gp perm failed, prot is %x", ob->oi.ProtectionStatus);
		break;
	}

	/* if object is an image */
	if ((ob->oi.ObjectFormat & 0x0800) != 0) {
		info->preview.fields = 0;
		strcpy_mime(info->preview.type, params->deviceinfo.VendorExtensionID, ob->oi.ThumbFormat);
		if (strlen(info->preview.type)) {
			info->preview.fields |= GP_FILE_INFO_TYPE;
		}
		if (ob->oi.ThumbCompressedSize) {
			info->preview.size   = ob->oi.ThumbCompressedSize;
			info->preview.fields |= GP_FILE_INFO_SIZE;
		}
		if (ob->oi.ThumbPixWidth) {
			info->preview.width  = ob->oi.ThumbPixWidth;
			info->preview.fields |= GP_FILE_INFO_WIDTH;
		}
		if (ob->oi.ThumbPixHeight) {
			info->preview.height  = ob->oi.ThumbPixHeight;
			info->preview.fields |= GP_FILE_INFO_HEIGHT;
		}
		if (ob->oi.ImagePixWidth) {
			info->file.width  = ob->oi.ImagePixWidth;
			info->file.fields |= GP_FILE_INFO_WIDTH;
		}
		if (ob->oi.ImagePixHeight) {
			info->file.height  = ob->oi.ImagePixHeight;
			info->file.fields |= GP_FILE_INFO_HEIGHT;
		}
	}
	return (GP_OK);
}

static int
make_dir_func (CameraFilesystem *fs, const char *folder, const char *foldername,
	       void *data, GPContext *context)
{
	Camera *camera = data;
	PTPObjectInfo oi;
	uint32_t parent;
	uint32_t storage;
	uint32_t handle;
	PTPParams* params = &camera->pl->params;

	if (!strcmp (folder, "/special"))
		return GP_ERROR_NOT_SUPPORTED;

	SET_CONTEXT_P(params, context);
	camera->pl->checkevents = TRUE;

	memset(&oi, 0, sizeof (PTPObjectInfo));
	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);
	/* get parent folder id omitting storage pseudofolder */
	find_folder_handle(params,folder,storage,parent);

	/* if you desire to make dir in 'root' folder, you have to use
	 * 0xffffffff instead of 0x00000000 (which means responder decide).
	 */
	if (parent==PTP_HANDLER_ROOT) parent=PTP_HANDLER_SPECIAL;

	handle = folder_to_handle (params, foldername, storage, parent, NULL);
	if (handle != PTP_HANDLER_SPECIAL)
		return GP_ERROR_DIRECTORY_EXISTS;

	oi.Filename=(char *)foldername;

	oi.ObjectFormat=PTP_OFC_Association;
	oi.ProtectionStatus=PTP_PS_NoProtection;
	oi.AssociationType=PTP_AT_GenericFolder;

	if ((params->deviceinfo.VendorExtensionID==
			PTP_VENDOR_EASTMAN_KODAK) &&
		(ptp_operation_issupported(params,
			PTP_OC_EK_SendFileObjectInfo)))
	{
		C_PTP_REP (ptp_ek_sendfileobjectinfo (params, &storage,
			&parent, &handle, &oi));
	} else if (ptp_operation_issupported(params, PTP_OC_SendObjectInfo)) {
		C_PTP_REP (ptp_sendobjectinfo (params, &storage,
			&parent, &handle, &oi));
	} else {
		GP_LOG_D ("The device does not support creating a folder.");
		return GP_ERROR_NOT_SUPPORTED;
	}
	/* update internal structures */
	return add_object(camera, handle, context);
}

static int
storage_info_func (CameraFilesystem *fs,
		CameraStorageInformation **sinfos,
		int *nrofsinfos,
		void *data, GPContext *context
) {
	Camera *camera 		= data;
	PTPParams *params 	= &camera->pl->params;
	PTPStorageInfo		si;
	PTPStorageIDs		sids;
	unsigned int		i,n;
	CameraStorageInformation*sif;

	if (!ptp_operation_issupported (params, PTP_OC_GetStorageIDs))
		return (GP_ERROR_NOT_SUPPORTED);

	SET_CONTEXT_P(params, context);
	C_PTP (ptp_getstorageids (params, &sids));
	n = 0;
	C_MEM (*sinfos = calloc (sids.n, sizeof (CameraStorageInformation)));
	for (i = 0; i<sids.n; i++) {
		sif = (*sinfos)+n;

		/* Invalid storage, storageinfo might cause hangs on it (Nikon D300s e.g.) */
		if ((sids.Storage[i]&0x0000ffff)==0) continue;

		C_PTP (ptp_getstorageinfo (params, sids.Storage[i], &si));
		sif->fields |= GP_STORAGEINFO_BASE;
		sprintf (sif->basedir, "/"STORAGE_FOLDER_PREFIX"%08x", sids.Storage[i]);

		if (si.VolumeLabel && strlen(si.VolumeLabel)) {
			sif->fields |= GP_STORAGEINFO_LABEL;
			strcpy (sif->label, si.VolumeLabel);
		}
		if (si.StorageDescription && strlen(si.StorageDescription)) {
			sif->fields |= GP_STORAGEINFO_DESCRIPTION;
			strcpy (sif->description, si.StorageDescription);
		}
		sif->fields |= GP_STORAGEINFO_STORAGETYPE;
		switch (si.StorageType) {
		case PTP_ST_Undefined:
			sif->type = GP_STORAGEINFO_ST_UNKNOWN;
			break;
		case PTP_ST_FixedROM:
			sif->type = GP_STORAGEINFO_ST_FIXED_ROM;
			break;
		case PTP_ST_FixedRAM:
			sif->type = GP_STORAGEINFO_ST_FIXED_RAM;
			break;
		case PTP_ST_RemovableRAM:
			sif->type = GP_STORAGEINFO_ST_REMOVABLE_RAM;
			break;
		case PTP_ST_RemovableROM:
			sif->type = GP_STORAGEINFO_ST_REMOVABLE_ROM;
			break;
		default:
			GP_LOG_D ("unknown storagetype 0x%x", si.StorageType);
			sif->type = GP_STORAGEINFO_ST_UNKNOWN;
			break;
		}
		sif->fields |= GP_STORAGEINFO_ACCESS;
		switch (si.AccessCapability) {
		case PTP_AC_ReadWrite:
			sif->access = GP_STORAGEINFO_AC_READWRITE;
			break;
		case PTP_AC_ReadOnly:
			sif->access = GP_STORAGEINFO_AC_READONLY;
			break;
		case PTP_AC_ReadOnly_with_Object_Deletion:
			sif->access = GP_STORAGEINFO_AC_READONLY_WITH_DELETE;
			break;
		default:
			GP_LOG_D ("unknown accesstype 0x%x", si.AccessCapability);
			sif->access = GP_STORAGEINFO_AC_READWRITE;
			break;
		}
		sif->fields |= GP_STORAGEINFO_FILESYSTEMTYPE;
		switch (si.FilesystemType) {
		default:
		case PTP_FST_Undefined:
			sif->fstype = GP_STORAGEINFO_FST_UNDEFINED;
			break;
		case PTP_FST_GenericFlat:
			sif->fstype = GP_STORAGEINFO_FST_GENERICFLAT;
			break;
		case PTP_FST_GenericHierarchical:
			sif->fstype = GP_STORAGEINFO_FST_GENERICHIERARCHICAL;
			break;
		case PTP_FST_DCF:
			sif->fstype = GP_STORAGEINFO_FST_DCF;
			break;
		}
		if (si.MaxCapability != 0xffffffff) {
			sif->fields |= GP_STORAGEINFO_MAXCAPACITY;
			sif->capacitykbytes = si.MaxCapability / 1024;
		}
		if (si.FreeSpaceInBytes != 0xffffffff) {
			sif->fields |= GP_STORAGEINFO_FREESPACEKBYTES;
			sif->freekbytes = si.FreeSpaceInBytes / 1024;
		}
		if (si.FreeSpaceInImages != 0xffffffff) {
			sif->fields |= GP_STORAGEINFO_FREESPACEIMAGES;
			sif->freeimages = si.FreeSpaceInImages;
		}
		free (si.StorageDescription);
		free (si.VolumeLabel);

		n++;
	}
	free (sids.Storage);
	*nrofsinfos = n;
	return (GP_OK);
}

static void
debug_objectinfo(PTPParams *params, uint32_t oid, PTPObjectInfo *oi) {
	GP_LOG_D ("ObjectInfo for '%s':", oi->Filename);
	GP_LOG_D ("  Object ID: 0x%08x", oid);
	GP_LOG_D ("  StorageID: 0x%08x", oi->StorageID);
	GP_LOG_D ("  ObjectFormat: 0x%04x", oi->ObjectFormat);
	GP_LOG_D ("  ProtectionStatus: 0x%04x", oi->ProtectionStatus);
	GP_LOG_D ("  ObjectCompressedSize: %ld", (unsigned long)oi->ObjectCompressedSize);
	GP_LOG_D ("  ThumbFormat: 0x%04x", oi->ThumbFormat);
	GP_LOG_D ("  ThumbCompressedSize: %d", oi->ThumbCompressedSize);
	GP_LOG_D ("  ThumbPixWidth: %d", oi->ThumbPixWidth);
	GP_LOG_D ("  ThumbPixHeight: %d", oi->ThumbPixHeight);
	GP_LOG_D ("  ImagePixWidth: %d", oi->ImagePixWidth);
	GP_LOG_D ("  ImagePixHeight: %d", oi->ImagePixHeight);
	GP_LOG_D ("  ImageBitDepth: %d", oi->ImageBitDepth);
	GP_LOG_D ("  ParentObject: 0x%08x", oi->ParentObject);
	GP_LOG_D ("  AssociationType: 0x%04x", oi->AssociationType);
	GP_LOG_D ("  AssociationDesc: 0x%08x", oi->AssociationDesc);
	GP_LOG_D ("  SequenceNumber: 0x%08x", oi->SequenceNumber);
	GP_LOG_D ("  ModificationDate: 0x%08x", (unsigned int)oi->ModificationDate);
	GP_LOG_D ("  CaptureDate: 0x%08x", (unsigned int)oi->CaptureDate);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func		= file_list_func,
	.folder_list_func	= folder_list_func,
	.get_info_func		= get_info_func,
	.set_info_func		= set_info_func,
	.get_file_func		= get_file_func,
	.read_file_func		= read_file_func,
	.del_file_func		= delete_file_func,
	.put_file_func		= put_file_func,
	.make_dir_func		= make_dir_func,
	.remove_dir_func	= remove_dir_func,
	.storage_info_func	= storage_info_func
};

int
camera_init (Camera *camera, GPContext *context)
{
    	CameraAbilities a;
	unsigned int i;
	int ret, tries = 0;
	PTPParams *params;
	char *curloc, *camloc;
	GPPortSettings	settings;
	uint32_t	sessionid;
	char		buf[20];
	int 		start_timeout = USB_START_TIMEOUT;
	int 		canon_start_timeout = USB_CANON_START_TIMEOUT;

	gp_port_get_settings (camera->port, &settings);
	/* Make sure our port is either USB or PTP/IP. */
	if (	(camera->port->type != GP_PORT_USB)	&&
		(camera->port->type != GP_PORT_PTPIP)	&&
		(camera->port->type != GP_PORT_USB_SCSI)
	) {
		gp_context_error (context, _("Currently, PTP is only implemented for "
			"USB and PTP/IP cameras currently, port type %x"), camera->port->type);
		return (GP_ERROR_UNKNOWN_PORT);
	}

	camera->functions->about = camera_about;
	camera->functions->exit = camera_exit;
	camera->functions->trigger_capture = camera_trigger_capture;
	camera->functions->capture = camera_capture;
	camera->functions->capture_preview = camera_capture_preview;
	camera->functions->summary = camera_summary;
	camera->functions->get_config = camera_get_config;
	camera->functions->get_single_config = camera_get_single_config;
	camera->functions->set_single_config = camera_set_single_config;
	camera->functions->set_config = camera_set_config;
	camera->functions->list_config = camera_list_config;
	camera->functions->wait_for_event = camera_wait_for_event;

	/* We need some data that we pass around */
	C_MEM (camera->pl = calloc (1, sizeof (CameraPrivateLibrary)));
	params = &camera->pl->params;
	params->debug_func = ptp_debug_func;
	params->error_func = ptp_error_func;
	C_MEM (params->data = calloc (1, sizeof (PTPData)));
	((PTPData *) params->data)->camera = camera;
	params->byteorder = PTP_DL_LE;
	if (params->byteorder == PTP_DL_LE)
		camloc = "UCS-2LE";
	else
		camloc = "UCS-2BE";

        gp_camera_get_abilities(camera, &a);

#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
	curloc = nl_langinfo (CODESET);
	if (!curloc) curloc="UTF-8";
	params->cd_ucs2_to_locale = iconv_open(curloc, camloc);
	params->cd_locale_to_ucs2 = iconv_open(camloc, curloc);
	if ((params->cd_ucs2_to_locale == (iconv_t) -1) ||
	    (params->cd_locale_to_ucs2 == (iconv_t) -1)) {
		GP_LOG_E ("Failed to create iconv converter.");
		/* we can fallback */
		/*return (GP_ERROR_OS_FAILURE);*/
	}
#endif

        for (i = 0; i<sizeof(models)/sizeof(models[0]); i++) {
            if ((a.usb_vendor == models[i].usb_vendor) &&
                (a.usb_product == models[i].usb_product)){
                params->device_flags = models[i].device_flags;
                break;
            }
	    /* do not run the funny MTP stuff on the cameras for now */
	    params->device_flags |= DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL;
	    params->device_flags |= DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST;
        }
	/* map the libmtp flags to ours. Currently its just 1 flag. */
        for (i = 0; i<sizeof(mtp_models)/sizeof(mtp_models[0]); i++) {
            if ((a.usb_vendor == mtp_models[i].usb_vendor) &&
                (a.usb_product == mtp_models[i].usb_product)) {
			params->device_flags = mtp_models[i].flags;
                break;
            }
        }


	switch (camera->port->type) {
	case GP_PORT_USB:
		params->sendreq_func	= ptp_usb_sendreq;
		params->senddata_func	= ptp_usb_senddata;
		params->getresp_func	= ptp_usb_getresp;
		params->getdata_func	= ptp_usb_getdata;
		params->event_wait	= ptp_usb_event_wait;
		params->event_check	= ptp_usb_event_check;
		params->event_check_queue	= ptp_usb_event_check_queue;
		params->cancelreq_func	= ptp_usb_control_cancel_request;
		params->maxpacketsize 	= settings.usb.maxpacketsize;
		GP_LOG_D ("maxpacketsize %d", settings.usb.maxpacketsize);
		if (params->device_flags & DEVICE_FLAG_OLYMPUS_XML_WRAPPED) {
#ifdef HAVE_LIBXML2
			GP_LOG_D ("Entering Olympus USB Mass Storage XML Wrapped Mode.\n");
			olympus_setup (params);
#else
			gp_context_error (context, _("Olympus wrapped XML support is currently only available with libxml2 support built in."));				\
			return GP_ERROR;
#endif
		}
		break;
	case GP_PORT_PTPIP: {
		GPPortInfo	info;
		char 		*xpath;

#if defined(HAVE_LIBWS232) && defined(WIN32)
		WORD wsaVersionWanted = MAKEWORD(1, 1);
		WSADATA wsaData;
		if (WSAStartup(wsaVersionWanted, &wsaData)) {
			GP_LOG_E("WSAStartup failed.");
			return GP_ERROR;
		}
#endif

		ret = gp_port_get_info (camera->port, &info);
		if (ret != GP_OK) {
			GP_LOG_E ("Failed to get port info?");
			return ret;
		}
		gp_port_info_get_path (info, &xpath);

		if (strstr(a.model,"Fuji")) {
			ret = ptp_fujiptpip_connect (params, xpath);
			if (ret != GP_OK) {
				GP_LOG_E ("Failed to connect.");
				return ret;
			}
			params->sendreq_func	= ptp_fujiptpip_sendreq;
			params->senddata_func	= ptp_fujiptpip_senddata;
			params->getresp_func	= ptp_fujiptpip_getresp;
			params->getdata_func	= ptp_fujiptpip_getdata;
			params->event_wait	= ptp_fujiptpip_event_wait;
			params->event_check	= ptp_fujiptpip_event_check;
			params->event_check_queue	= ptp_fujiptpip_event_check_queue;
		} else {
			ret = ptp_ptpip_connect (params, xpath);
			if (ret != GP_OK) {
				GP_LOG_E ("Failed to connect.");
				return ret;
			}
			params->sendreq_func	= ptp_ptpip_sendreq;
			params->senddata_func	= ptp_ptpip_senddata;
			params->getresp_func	= ptp_ptpip_getresp;
			params->getdata_func	= ptp_ptpip_getdata;
			params->event_wait	= ptp_ptpip_event_wait;
			params->event_check	= ptp_ptpip_event_check;
			params->event_check_queue	= ptp_ptpip_event_check_queue;
		}
		break;
	}
	default:
		break;
	}
	if (!params->maxpacketsize)
		params->maxpacketsize = 64; /* assume USB 1.0 */

	/* Read configurable timeouts */
#define XT(val,def) 					\
	if ((GP_OK == gp_setting_get("ptp2",#val,buf)))	\
		sscanf(buf, "%d", &val);		\
	if (!val) val = def;
	XT(normal_timeout,USB_NORMAL_TIMEOUT);
	XT(capture_timeout,USB_TIMEOUT_CAPTURE);

	/* Choose a shorter timeout on initial setup to avoid
	 * having the user wait too long.
	 */
	if (a.usb_vendor == 0x4a9) { /* CANON */
		/* our special canon friends get a shorter timeout, sinc ethey
		 * occasionally need 2 retries. */
		XT(canon_start_timeout,USB_CANON_START_TIMEOUT);
		CR (gp_port_set_timeout (camera->port, canon_start_timeout));
	} else {
		XT(start_timeout,USB_START_TIMEOUT);
		CR (gp_port_set_timeout (camera->port, start_timeout));
	}
#undef XT
	if ((GP_OK == gp_setting_get("ptp2","cachetime",buf))) {
		sscanf(buf, "%d", &params->cachetime);
		GP_LOG_D("read cachetime %d", params->cachetime);
	} else {
		params->cachetime = 2; /* 2 seconds */
	}

	/* Establish a connection to the camera */
	SET_CONTEXT(camera, context);

	tries = 0;
	sessionid = 1;
	while (1) {
		ret = LOG_ON_PTP_E (ptp_opensession (params, sessionid));
		if (ret == PTP_RC_SessionAlreadyOpened || ret == PTP_RC_OK)
			break;

		tries++;

		if (ret==PTP_RC_InvalidTransactionID) {
			sessionid++;
			/* Try a couple sessionids starting with 1 */
			if (tries < 10)
				continue;

			if (tries < 11 && camera->port->type == GP_PORT_USB) {
				/* Try whacking PTP device */
				ptp_usb_control_device_reset_request (&camera->pl->params);
				sessionid = 1;
				continue;
			}
		} else if ((ret == PTP_ERROR_RESP_EXPECTED) || (ret == PTP_ERROR_IO)) {
			/* Try whacking PTP device */
			if (tries < 3 && camera->port->type == GP_PORT_USB)
				ptp_usb_control_device_reset_request (params);
		}

		if (tries < 3)
			continue;

		/* FIXME: deviceinfo is not read yet ... (see macro)*/
		C_PTP_REP (ret);
	}
	/* We have cameras where a response takes 15 seconds(!), so make
	 * post init timeouts longer */
	CR (gp_port_set_timeout (camera->port, normal_timeout));

	if (params->device_flags & DEVICE_FLAG_OLYMPUS_XML_WRAPPED) {
		unsigned char	*data;
		unsigned int	len;
		PTPObjectInfo	oi;
		uint32_t	parenthandle,storagehandle, handle;

		GP_LOG_D ("Sending empty XDISCVRY.X3C file to camera ... ");

		storagehandle = 0x80000001;
		parenthandle = 0;
		handle = 0;

		memset (&oi, 0, sizeof (oi));
		oi.ObjectFormat		= PTP_OFC_Script;
		oi.StorageID 		= 0x80000001;
		oi.Filename 		= "XDISCVRY.X3C";
		oi.ObjectCompressedSize	= 0;
		C_PTP_REP (ptp_sendobjectinfo (params, &storagehandle, &parenthandle, &handle, &oi));

		GP_LOG_D ("olympus getcameraid\n");
		C_PTP_REP (ptp_olympus_getcameraid (params, &data, &len));

		GP_LOG_D ("olympus setcameracontrolmode\n");
		C_PTP_REP (ptp_olympus_setcameracontrolmode (params, 1));

		GP_LOG_D ("olympus opensession\n");
		C_PTP_REP (ptp_olympus_opensession (params, &data, &len));
	}

#if 1
	/* Special fuji wlan init code */
	if ((camera->port->type == GP_PORT_PTPIP)  && strstr(a.model,"Fuji")) {
		PTPPropertyValue        propval;
		GPPortInfo		info;
		char 			*xpath;

		ret = gp_port_get_info (camera->port, &info);
		if (ret != GP_OK) {
			GP_LOG_E ("Failed to get port info?");
			return ret;
		}
		gp_port_info_get_path (info, &xpath);

		params->byteorder = PTP_DL_LE;

		propval.u16 = 5;
		C_PTP_REP (ptp_setdevicepropvalue(params, PTP_DPC_FUJI_InitSequence, &propval, PTP_DTC_UINT16));

		/* We send back the version we get from the camera */
		C_PTP_REP (ptp_getdevicepropvalue(params, PTP_DPC_FUJI_AppVersion, &propval, PTP_DTC_UINT32));
		GP_LOG_D("FUJI AppVersion is %d", propval.u32);
		C_PTP_REP (ptp_setdevicepropvalue(params, PTP_DPC_FUJI_AppVersion, &propval, PTP_DTC_UINT32));

		C_PTP_REP (ptp_initiateopencapture(params, 0x00000000, 0x00000000)); /* this will get the event queue started */
		ptp_fujiptpip_init_event(params, xpath);
	}
#endif
	/* Seems HP does not like getdevinfo outside of session
	   although it's legal to do so */
	/* get device info */
	C_PTP_REP (ptp_getdeviceinfo(params, &params->deviceinfo));

	print_debug_deviceinfo(params, &params->deviceinfo);

	CR (fixup_cached_deviceinfo (camera,&params->deviceinfo));

	print_debug_deviceinfo(params, &params->deviceinfo);

	switch (params->deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_CANON:
#if 0
		if (ptp_operation_issupported(params, PTP_OC_CANON_ThemeDownload)) {
			add_special_file("startimage.jpg",	canon_theme_get, canon_theme_put);
			add_special_file("startsound.wav",	canon_theme_get, canon_theme_put);
			add_special_file("operation.wav",	canon_theme_get, canon_theme_put);
			add_special_file("shutterrelease.wav",	canon_theme_get, canon_theme_put);
			add_special_file("selftimer.wav",	canon_theme_get, canon_theme_put);
		}
#endif

		/* If CHDK is present and enabled, this will go and overwrite the function tables again */
		if (	ptp_operation_issupported(params, PTP_OC_CHDK) &&
			(GP_OK == gp_setting_get("ptp2","chdk",buf)) &&
			!strcmp(buf,"on")
		)
			return chdk_init (camera, context);

		if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_SetRemoteMode)) {
			if (is_canon_eos_m(params)) {
				int mode = 0x15;	/* default for EOS M and newer Powershot SX */

				if (!strcmp(params->deviceinfo.Model,"Canon EOS M6 Mark II")) mode = 0x1;
				if (!strcmp(params->deviceinfo.Model,"Canon PowerShot SX720 HS")) mode = 0x11;
				if (!strcmp(params->deviceinfo.Model,"Canon PowerShot SX600 HS")) mode = 0x11;

				/* according to reporter only needed in config.c part
				if (!strcmp(params->deviceinfo.Model,"Canon PowerShot G5 X")) mode = 0x11;
				*/
				C_PTP (ptp_canon_eos_setremotemode(params, mode));
				/* Setting remote mode changes device info on EOS M2,
				   so have to reget it */
				C_PTP (ptp_getdeviceinfo(&camera->pl->params, &camera->pl->params.deviceinfo));
				print_debug_deviceinfo(params, &params->deviceinfo);
				CR (fixup_cached_deviceinfo (camera, &camera->pl->params.deviceinfo));
				print_debug_deviceinfo(params, &params->deviceinfo);
			} else {
				C_PTP (ptp_canon_eos_setremotemode(params, 1));
			}
		}
		break;
	case PTP_VENDOR_NIKON:
		if (ptp_operation_issupported(params, PTP_OC_NIKON_CurveDownload))
			add_special_file("curve.ntc", nikon_curve_get, nikon_curve_put);
		break;
	case PTP_VENDOR_SONY:
		/* this seems to crash the HX100V and HX9V and NEX
		 * https://github.com/gphoto/libgphoto2/issues/85
		 * And locks up the ILCE-7M4
		 * https://github.com/gphoto/libgphoto2/pull/782
		 */
		if (	ptp_operation_issupported(params, 0x9280)	&&
			!strstr(params->deviceinfo.Model,"HX")		&&
			!strstr(params->deviceinfo.Model,"NEX")		&&
			!strstr(params->deviceinfo.Model,"QX")		&&
			!strstr(params->deviceinfo.Model,"ILCE-7M4")
		) {
#if 0
			C_PTP (ptp_sony_9280(params, 0x1,0,1,0,0));
			C_PTP (ptp_sony_9281(params, 0x1));	/* no data sent back */

			C_PTP (ptp_sony_9280(params, 0x1,0,2,0,0));
			C_PTP (ptp_sony_9281(params, 0x1));	/* no data sent back */

			C_PTP (ptp_sony_9280(params, 0x4,0,1,0,0));
			C_PTP (ptp_sony_9281(params, 0x4));	/* gets big data blob? */
			/* also tries this multiple times , but gets back 2006 error
			ptp_sony_9280(params, 0x5,0,1,0,0);
			*/
#endif
			/* This combination seems to reportedly work */
			C_PTP (ptp_sony_9280(params, 0x4, 2,2,0,0, 0x01,0x01));
			C_PTP (ptp_sony_9281(params, 0x4));
		}
		break;
	case PTP_VENDOR_FUJI:
		CR (camera_prepare_capture (camera, context));
		break;
	case PTP_VENDOR_GP_SIGMAFP:
		if (ptp_operation_issupported(params, 0x9035)) {
			unsigned char *xdata = NULL;
			unsigned int xsize = 0;
			C_PTP (ptp_sigma_fp_9035 (params, &xdata, &xsize));
			GP_LOG_DATA ((char*)xdata, xsize, "9035 output");
			free (xdata);

			C_PTP (ptp_sigma_fp_getcamcansetinfo5 (params, &xdata, &xsize));
			GP_LOG_DATA ((char*)xdata, xsize, "getcamcansetinfo5 output");
			free (xdata);

			C_PTP (ptp_sigma_fp_getdatagroup1 (params, &xdata, &xsize));
			GP_LOG_DATA ((char*)xdata, xsize, "getdatagroup1 output");
			free (xdata);

			C_PTP (ptp_sigma_fp_getdatagroup2 (params, &xdata, &xsize));
			GP_LOG_DATA ((char*)xdata, xsize, "getdatagroup2 output");
			free (xdata);

			C_PTP (ptp_sigma_fp_getdatagroup3 (params, &xdata, &xsize));
			GP_LOG_DATA ((char*)xdata, xsize, "getdatagroup3 output");
			free (xdata);

			C_PTP (ptp_sigma_fp_getdatagroup4 (params, &xdata, &xsize));
			GP_LOG_DATA ((char*)xdata, xsize, "getdatagroup4 output");
			free (xdata);

			C_PTP (ptp_sigma_fp_getdatagroup5 (params, &xdata, &xsize));
			GP_LOG_DATA ((char*)xdata, xsize, "getdatagroup5 output");
			free (xdata);

			C_PTP (ptp_sigma_fp_getdatagroup6 (params, &xdata, &xsize));
			GP_LOG_DATA ((char*)xdata, xsize, "getdatagroup6 output");
			free (xdata);

			C_PTP (ptp_sigma_fp_getcamdatagroupfocus (params, &xdata, &xsize));
			GP_LOG_DATA ((char*)xdata, xsize, "getdatagroupfocus output");
			free (xdata);

			C_PTP (ptp_sigma_fp_getcamdatagroupmovie (params, &xdata, &xsize));
			GP_LOG_DATA ((char*)xdata, xsize, "getdatagroupmovie output");
			free (xdata);
		}
		break;
	case PTP_VENDOR_GP_LEICA:
		if (ptp_operation_issupported(params, PTP_OC_LEICA_LEOpenSession)) {
			PTPDeviceInfo	pdi;

			C_PTP (ptp_leica_leopensession (params, 0));	/* arguments might not be complete */
			C_PTP_REP (ptp_getdeviceinfo (params, &pdi));
			CR (fixup_cached_deviceinfo (camera, &pdi));
			print_debug_deviceinfo(params, &params->deviceinfo);
		}
		break;
	default:
		break;
	}

#if 0
	/* new ptp 1.1 command, implemented in the newer iphone firmware */
	{
		unsigned char *data;
		ptp_getfilesystemmanifest (params, 0x00010001, 0, 0, &data);
	}
#endif

	/* read the root directory to avoid the "DCIM WRONG ROOT" bugs */
	CR (gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera));

	/* initialize the storage ids in Params */
	if ((!params->storageids.n) && (ptp_operation_issupported(params, PTP_OC_GetStorageIDs)))
		ptp_getstorageids(params, &params->storageids);

	if (a.usb_vendor == 0x05ac) {	/* Apple iOS 10.2 hack */
		int 		timeout;
		PTPContainer	event;

		/* Background:
		 * Untrusted computer gets a storage id of 0xfeedface.
		 * Once the ipad user allows / trusts the computer in a popup dialog,
		 * this storage removed and the actual storage is being addded
		 * StoreRemoved 0xfeedface and StoreAdded 0x.... events are seen.
		 */

		/* Wait for a valid storage id , starting with 0x0001???? */
		/* wait for 3 events or 9 seconds at most */
		tries = 3;
		gp_port_get_timeout (camera->port, &timeout);
		gp_port_set_timeout (camera->port, 3000);
		while (tries--) {
			/* 0xfeedface and 0x00000000 seem bad storageid values for iPhones */
			/* The event handling code in ptp.c will refresh the storage list if
			 * it sees the correct events */
			if (params->storageids.n && (
				(params->storageids.Storage[0] != 0xfeedface) &&
				(params->storageids.Storage[0] != 0x00000000)
			))
				break;
			C_PTP_REP (ptp_wait_event (params));
			while (ptp_get_one_event (params, &event)) {
				GP_LOG_D ("Initial ptp event 0x%x (param1=%x)", event.Code, event.Param1);
				/* the ptp stack should refresh the storage array */
				if (event.Code == PTP_EC_StoreAdded)
					break;
			}
		}
		gp_port_set_timeout (camera->port, timeout);
	}

	/* avoid doing this on the Sonys DSLRs in control mode, they hang. :( */

	if (params->deviceinfo.VendorExtensionID != PTP_VENDOR_SONY)
		ptp_list_folder (params, PTP_HANDLER_SPECIAL, PTP_HANDLER_SPECIAL);

	{
		unsigned int k;

		for (k=0;k<params->storageids.n;k++) {
			if (!(params->storageids.Storage[k] & 0xffff)) continue;
			if (params->storageids.Storage[k] == 0x80000001) continue;
			ptp_list_folder (params, params->storageids.Storage[k], PTP_HANDLER_SPECIAL);
		}
	}
	/* moved down here in case the filesystem needs to first be initialized as the Olympus app does */
	if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_GP_OLYMPUS_OMD) {
		unsigned int k;

		GP_LOG_D ("Initializing Olympus ... ");
		ptp_olympus_init_pc_mode(params);

		/* try to refetch the storage ids, set before only has 0x00000001 */
		if (params->storageids.n) {
			free (params->storageids.Storage);
			params->storageids.Storage = NULL;
			params->storageids.n = 0;
		}

		C_PTP (ptp_getstorageids(params, &params->storageids));

		/* refetch root */
		for (k=0;k<params->storageids.n;k++) {
			if (!(params->storageids.Storage[k] & 0xffff)) continue;
			if (params->storageids.Storage[k] == 0x80000001) continue;
			ptp_list_folder (params, params->storageids.Storage[k], PTP_HANDLER_SPECIAL);
		}

		/*
		if(params->storageids.n > 0) { // Olympus app gets storage info for first item, so emulating here
			PTPStorageInfo storageinfo;
			ptp_getstorageinfo(params, params->storageids.Storage[0], &storageinfo);
		}

		PTPPropertyValue        propval;
		if (!strncmp(params->deviceinfo.Model,"E-M5",4)) {
			ptp_olympus_init_pc_mode(params);
		}
		ptp_olympus_omd_init(params);
		propval.u16 = 2;
		ptp_setdevicepropvalue(params, 0xD078, &propval, PTP_DTC_UINT16);
		if (!strncmp(params->deviceinfo.Model,"E-M1",4)) {
			propval.u16 = 2; // save to camera's card
			ptp_setdevicepropvalue(params, 0xD0DC, &propval, PTP_DTC_UINT16);
			//propval.u16 = 1;
			//ptp_setdevicepropvalue(params, 0xD052, &propval, PTP_DTC_UINT16);
			ptp_check_event_handle (params, 0);
			ptp_olympus_init_pc_mode(params);
		}
		*/
	}
	SET_CONTEXT(camera, NULL);
	return GP_OK;
}
