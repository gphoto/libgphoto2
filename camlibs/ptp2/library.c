/* library.c
 *
 * Copyright (C) 2001-2005 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2010 Marcus Meissner <marcus@jet.franken.de>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#define _BSD_SOURCE
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#ifdef HAVE_ICONV
#include <langinfo.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

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

#include "ptp.h"
#include "ptp-bugs.h"
#include "ptp-private.h"
#include "ptp-pack.c"

#define GP_MODULE "PTP2"

#define USB_START_TIMEOUT 8000
#define USB_CANON_START_TIMEOUT 1500	/* 1.5 seconds (0.5 was too low) */
#define USB_NORMAL_TIMEOUT 20000
static int normal_timeout = USB_NORMAL_TIMEOUT;
#define USB_TIMEOUT_CAPTURE 100000
static int capture_timeout = USB_TIMEOUT_CAPTURE;

#define	SET_CONTEXT(camera, ctx) ((PTPData *) camera->pl->params.data)->context = ctx
#define	SET_CONTEXT_P(p, ctx) ((PTPData *) p->data)->context = ctx

#define CPR(context,result) {short r=(result); if (r!=PTP_RC_OK) {report_result ((context), r, params->deviceinfo.VendorExtensionID); return (translate_ptp_result (r));}}

#define CPR_free(context,result, freeptr) {\
			short r=(result);\
			if (r!=PTP_RC_OK) {\
				report_result ((context), r, params->deviceinfo.VendorExtensionID);\
				free(freeptr);\
				return (translate_ptp_result (r));\
			}\
}

#define CR(result) {int r=(result);if(r<0) return (r);}
/*
#define CR_free(result, freeptr) {\
			int r=(result);\
			if(r<0){\
				free(freeptr);\
				return(r);\
				}\
}
*/
/* below macro makes a copy of fn without leading character ('/'),
 * removes the '/' at the end if present, and calls folder_to_handle()
 * funtion proviging as the first argument the string after the second '/'.
 * for example if fn is '/store_00010001/DCIM/somefolder/', the macro will
 * call folder_to_handle() with 'DCIM/somefolder' as the very first argument.
 * it's used to omit storage pseudofolder and remove trailing '/'
 */

#define find_folder_handle(fn,s,p,d)	{			\
		{						\
		int len=strlen(fn);				\
		char *backfolder=malloc(len);			\
		char *tmpfolder;				\
		memcpy(backfolder,fn+1, len);			\
		if (backfolder[len-2]=='/') backfolder[len-2]='\0';\
		if ((tmpfolder=strchr(backfolder+1,'/'))==NULL) tmpfolder="/";\
		p=folder_to_handle(tmpfolder+1,s,0,(Camera *)d);\
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
int init_ptp_fs (Camera *camera, GPContext *context);

typedef int (*getfunc_t)(CameraFilesystem*, const char*, const char *, CameraFileType, CameraFile *, void *, GPContext *);
typedef int (*putfunc_t)(CameraFilesystem*, const char*, CameraFile*, void*, GPContext*);

struct special_file {
	char		*name;
	getfunc_t	getfunc;
	putfunc_t	putfunc;
};

static int nrofspecial_files = 0;
static struct special_file *special_files = NULL;

static int
add_special_file (char *name, getfunc_t getfunc, putfunc_t putfunc) {
	if (nrofspecial_files)
		special_files = realloc (special_files, sizeof(special_files[0])*(nrofspecial_files+1));
	else
		special_files = malloc (sizeof(special_files[0]));

	special_files[nrofspecial_files].name = strdup(name);
	if (!special_files[nrofspecial_files].name)
		return (GP_ERROR_NO_MEMORY);
	special_files[nrofspecial_files].putfunc = putfunc;
	special_files[nrofspecial_files].getfunc = getfunc;
	nrofspecial_files++;
	return (GP_OK);
}

#define STORAGE_FOLDER_PREFIX		"store_"

/* PTP error descriptions */
static struct {
	short n;
	short v;
	const char *txt;
} ptp_errors[] = {
	{PTP_RC_Undefined, 		0, N_("PTP Undefined Error")},
	{PTP_RC_OK, 			0, N_("PTP OK!")},
	{PTP_RC_GeneralError, 		0, N_("PTP General Error")},
	{PTP_RC_SessionNotOpen, 	0, N_("PTP Session Not Open")},
	{PTP_RC_InvalidTransactionID, 	0, N_("PTP Invalid Transaction ID")},
	{PTP_RC_OperationNotSupported, 	0, N_("PTP Operation Not Supported")},
	{PTP_RC_ParameterNotSupported, 	0, N_("PTP Parameter Not Supported")},
	{PTP_RC_IncompleteTransfer, 	0, N_("PTP Incomplete Transfer")},
	{PTP_RC_InvalidStorageId, 	0, N_("PTP Invalid Storage ID")},
	{PTP_RC_InvalidObjectHandle, 	0, N_("PTP Invalid Object Handle")},
	{PTP_RC_DevicePropNotSupported, 0, N_("PTP Device Prop Not Supported")},
	{PTP_RC_InvalidObjectFormatCode, 0, N_("PTP Invalid Object Format Code")},
	{PTP_RC_StoreFull, 		0, N_("PTP Store Full")},
	{PTP_RC_ObjectWriteProtected, 	0, N_("PTP Object Write Protected")},
	{PTP_RC_StoreReadOnly, 		0, N_("PTP Store Read Only")},
	{PTP_RC_AccessDenied,		0, N_("PTP Access Denied")},
	{PTP_RC_NoThumbnailPresent, 	0, N_("PTP No Thumbnail Present")},
	{PTP_RC_SelfTestFailed, 	0, N_("PTP Self Test Failed")},
	{PTP_RC_PartialDeletion, 	0, N_("PTP Partial Deletion")},
	{PTP_RC_StoreNotAvailable, 	0, N_("PTP Store Not Available")},
	{PTP_RC_SpecificationByFormatUnsupported,
				0, N_("PTP Specification By Format Unsupported")},
	{PTP_RC_NoValidObjectInfo, 	0, N_("PTP No Valid Object Info")},
	{PTP_RC_InvalidCodeFormat, 	0, N_("PTP Invalid Code Format")},
	{PTP_RC_UnknownVendorCode, 	0, N_("PTP Unknown Vendor Code")},
	{PTP_RC_CaptureAlreadyTerminated,
					0, N_("PTP Capture Already Terminated")},
	{PTP_RC_DeviceBusy, 		0, N_("PTP Device Busy")},
	{PTP_RC_InvalidParentObject, 	0, N_("PTP Invalid Parent Object")},
	{PTP_RC_InvalidDevicePropFormat, 0, N_("PTP Invalid Device Prop Format")},
	{PTP_RC_InvalidDevicePropValue, 0, N_("PTP Invalid Device Prop Value")},
	{PTP_RC_InvalidParameter, 	0, N_("PTP Invalid Parameter")},
	{PTP_RC_SessionAlreadyOpened, 	0, N_("PTP Session Already Opened")},
	{PTP_RC_TransactionCanceled, 	0, N_("PTP Transaction Canceled")},
	{PTP_RC_SpecificationOfDestinationUnsupported,
			0, N_("PTP Specification Of Destination Unsupported")},
	{PTP_RC_EK_FilenameRequired,	PTP_VENDOR_EASTMAN_KODAK, N_("PTP EK Filename Required")},
	{PTP_RC_EK_FilenameConflicts,	PTP_VENDOR_EASTMAN_KODAK, N_("PTP EK Filename Conflicts")},
	{PTP_RC_EK_FilenameInvalid,	PTP_VENDOR_EASTMAN_KODAK, N_("PTP EK Filename Invalid")},

	{PTP_RC_NIKON_HardwareError,	PTP_VENDOR_NIKON, N_("Hardware Error")},
	{PTP_RC_NIKON_OutOfFocus,	PTP_VENDOR_NIKON, N_("Out of Focus")},
	{PTP_RC_NIKON_ChangeCameraModeFailed, PTP_VENDOR_NIKON, N_("Change Camera Mode Failed")},
	{PTP_RC_NIKON_InvalidStatus,	PTP_VENDOR_NIKON, N_("Invalid Status")},
	{PTP_RC_NIKON_SetPropertyNotSupported, PTP_VENDOR_NIKON, N_("Set Property Not Supported")},
	{PTP_RC_NIKON_WbResetError,	PTP_VENDOR_NIKON, N_("Whitebalance Reset Error")},
	{PTP_RC_NIKON_DustReferenceError, PTP_VENDOR_NIKON, N_("Dust Reference Error")},
	{PTP_RC_NIKON_ShutterSpeedBulb, PTP_VENDOR_NIKON, N_("Shutter Speed Bulb")},
	{PTP_RC_NIKON_MirrorUpSequence, PTP_VENDOR_NIKON, N_("Mirror Up Sequence")},
	{PTP_RC_NIKON_CameraModeNotAdjustFNumber, PTP_VENDOR_NIKON, N_("Camera Mode Not Adjust FNumber")},
	{PTP_RC_NIKON_NotLiveView,	PTP_VENDOR_NIKON, N_("Not in Liveview")},
	{PTP_RC_NIKON_MfDriveStepEnd,	PTP_VENDOR_NIKON, N_("Mf Drive Step End")},
	{PTP_RC_NIKON_MfDriveStepInsufficiency,	PTP_VENDOR_NIKON, N_("Mf Drive Step Insufficiency")},
	{PTP_RC_NIKON_AdvancedTransferCancel, PTP_VENDOR_NIKON, N_("Advanced Transfer Cancel")},
	{PTP_RC_CANON_UNKNOWN_COMMAND,	PTP_VENDOR_CANON, N_("Unknown command")},
	{PTP_RC_CANON_OPERATION_REFUSED,PTP_VENDOR_CANON, N_("Operation refused")},
	{PTP_RC_CANON_LENS_COVER,	PTP_VENDOR_CANON, N_("Lens cover present")},
	{PTP_RC_CANON_BATTERY_LOW,	PTP_VENDOR_CANON, N_("Battery low")},
	{PTP_RC_CANON_NOT_READY,	PTP_VENDOR_CANON, N_("Camera not ready")},

	{PTP_ERROR_IO,		  0, N_("PTP I/O error")},
	{PTP_ERROR_CANCEL,	  0, N_("PTP Cancel request")},
	{PTP_ERROR_BADPARAM,	  0, N_("PTP Error: bad parameter")},
	{PTP_ERROR_DATA_EXPECTED, 0, N_("PTP Protocol error, data expected")},
	{PTP_ERROR_RESP_EXPECTED, 0, N_("PTP Protocol error, response expected")},
	{PTP_ERROR_TIMEOUT,       0, N_("PTP Timeout")},
	{0, 0, NULL}
};

void
report_result (GPContext *context, short result, short vendor)
{
	unsigned int i;

	for (i = 0; ptp_errors[i].txt; i++)
		if ((ptp_errors[i].n == result) && (
		    (ptp_errors[i].v == 0) || (ptp_errors[i].v == vendor)
		))
			gp_context_error (context, "%s", dgettext(GETTEXT_PACKAGE, ptp_errors[i].txt));
}

int
translate_ptp_result (short result)
{
	switch (result) {
	case PTP_RC_ParameterNotSupported:
		return (GP_ERROR_BAD_PARAMETERS);
	case PTP_RC_OperationNotSupported:
		return (GP_ERROR_NOT_SUPPORTED);
	case PTP_RC_DeviceBusy:
		return (GP_ERROR_CAMERA_BUSY);
	case PTP_ERROR_TIMEOUT:
		return (GP_ERROR_TIMEOUT);
	case PTP_ERROR_CANCEL:
		return (GP_ERROR_CANCEL);
	case PTP_ERROR_BADPARAM:
		return (GP_ERROR_BAD_PARAMETERS);
	case PTP_RC_OK:
		return (GP_OK);
	default:
		return (GP_ERROR);
	}
}

void
fixup_cached_deviceinfo (Camera *camera, PTPDeviceInfo *di) {
	CameraAbilities a;

        gp_camera_get_abilities(camera, &a);

	/* for USB class matches on unknown cameras... */
	if (!a.usb_vendor && di->Manufacturer) {
		if (strstr (di->Manufacturer,"Canon"))
			a.usb_vendor = 0x4a9;
		if (strstr (di->Manufacturer,"Nikon"))
			a.usb_vendor = 0x4b0;
	}
	if (	(di->VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
		di->Manufacturer
	) {
		camera->pl->bugs |= PTP_MTP;
		if (strstr (di->Manufacturer,"Canon"))
			di->VendorExtensionID = PTP_VENDOR_CANON;
		if (strstr (di->Manufacturer,"Nikon"))
			di->VendorExtensionID = PTP_VENDOR_NIKON;
	}

	/* Newer Canons say that they are MTP devices. Restore Canon vendor extid. */
	if (	(di->VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
		(camera->port->type == GP_PORT_USB) &&
		(a.usb_vendor == 0x4a9)
	) {
		camera->pl->bugs |= PTP_MTP;
		di->VendorExtensionID = PTP_VENDOR_CANON;
	}

	/* Newer Nikons (D40) say that they are MTP devices. Restore Nikon vendor extid. */
	if (	(di->VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
		(camera->port->type == GP_PORT_USB) &&
		(a.usb_vendor == 0x4b0)
	) {
		camera->pl->bugs |= PTP_MTP;
		di->VendorExtensionID = PTP_VENDOR_NIKON;
	}

	/* Fuji S5 Pro mostly */
	if (	(di->VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
		(camera->port->type == GP_PORT_USB) &&
		(a.usb_vendor == 0x4cb) &&
		strstr(di->VendorExtensionDesc,"fujifilm.co.jp: 1.0;")
	) {
		/*camera->pl->bugs |= PTP_MTP;*/
		di->VendorExtensionID = PTP_VENDOR_FUJI;
	}

	if (di->VendorExtensionID == PTP_VENDOR_NIKON) {
		int i;

		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_GetVendorPropCodes)) {
			uint16_t  	*xprops;
			unsigned int	xsize;
			uint16_t	ret;

			ret = ptp_nikon_get_vendorpropcodes (&camera->pl->params, &xprops, &xsize);
			if (ret == PTP_RC_OK) {
				di->DevicePropertiesSupported = realloc(di->DevicePropertiesSupported,sizeof(di->DevicePropertiesSupported[0])*(di->DevicePropertiesSupported_len + xsize));
				for (i=0;i<xsize;i++)
					di->DevicePropertiesSupported[i+di->DevicePropertiesSupported_len] = xprops[i];
				di->DevicePropertiesSupported_len += xsize;
				free (xprops);
			} else {
				gp_log (GP_LOG_ERROR, "ptp2/fixup", "ptp_nikon_get_vendorpropcodes() failed with 0x%04x", ret);
			}
		}
	}
#if 0 /* Marcus: not regular ptp properties, not queryable via getdevicepropertyvalue */
	if (di->VendorExtensionID == PTP_VENDOR_CANON) {
		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EOS_GetDeviceInfoEx)) {
			PTPCanonEOSDeviceInfo  	eosdi;
			int i;
			uint16_t	ret;

			ret = ptp_canon_eos_getdeviceinfo (&camera->pl->params, &eosdi);
			if (ret == PTP_RC_OK) {
				di->DevicePropertiesSupported = realloc(di->DevicePropertiesSupported,sizeof(di->DevicePropertiesSupported[0])*(di->DevicePropertiesSupported_len + eosdi.DevicePropertiesSupported_len));
				for (i=0;i<eosdi.DevicePropertiesSupported_len;i++)
					di->DevicePropertiesSupported[i+di->DevicePropertiesSupported_len] = eosdi.DevicePropertiesSupported[i];
				di->DevicePropertiesSupported_len += eosdi.DevicePropertiesSupported_len;
			} else {
				gp_log (GP_LOG_ERROR, "ptp2/fixup", "ptp_canon_get_deviceinfoex() failed with 0x%04x", ret);
			}
		}
	}
#endif
}

static struct {
	const char *model;
	unsigned short usb_vendor;
	unsigned short usb_product;
	unsigned long known_bugs;
} models[] = {
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

	/* HP PTP cameras */
#if 0
	/* 0x4002 seems to be the mass storage ID, which various forums suggest. -Marcus */
	{"HP:PhotoSmart ... ", 		 0x03f0, 0x4002, 0},
#endif
	{"HP:PhotoSmart 812 (PTP mode)", 0x03f0, 0x4202, 0},
	{"HP:PhotoSmart 850 (PTP mode)", 0x03f0, 0x4302, PTPBUG_DUPE_FILE},
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
	/* new id?! Reported by Ruediger Oertel. */
	{"Sony:DSC-W200 (PTP mode)",  0x054c, 0x02f8, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1946931&group_id=8874&atid=308874 */
	{"Sony:DSC-W130 (PTP mode)",  0x054c, 0x0343, 0},

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
	{"Nikon:Coolpix 5000 (PTP mode)", 0x04b0, 0x0113, PTP_CAP},
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
	/* Harry Reisenleiter <harrylr@earthlink.net> */
	{"Nikon:Coolpix 8800 (PTP mode)", 0x04b0, 0x0127, PTP_CAP},
	/* Nikon Coolpix 4800 */
	{"Nikon:Coolpix 4800 (PTP mode)", 0x04b0, 0x0129, 0},
	/* Nikon Coolpix SQ: M. Holzbauer, 07 Jul 2003 */
	{"Nikon:Coolpix 4100 (PTP mode)", 0x04b0, 0x012d, 0},
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
	{"Nikon:Coolpix L12 (PTP mode)",  0x04b0, 0x015f, 0},
	/* Marius Groeger <marius.groeger@web.de> */
	{"Nikon:Coolpix S200 (PTP mode)", 0x04b0, 0x0161, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* Submitted on IRC by kallepersson */
	{"Nikon:Coolpix P5100 (PTP mode)", 0x04b0, 0x0163, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2589245&group_id=8874&atid=108874 */
	{"Nikon:Coolpix P50 (PTP mode)",  0x04b0, 0x0169, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=2951663&group_id=8874&atid=358874 */
	{"Nikon:Coolpix P6000 (PTP mode)",0x04b0, 0x016f, 0},
	/*   http://bugs.debian.org/520752 */
	{"Nikon:Coolpix S60 (PTP mode)",  0x04b0, 0x0171, 0},
	/* Christoph Muehlmann <c.muehlmann@nagnag.de> */
	{"Nikon:Coolpix S220 (PTP mode)", 0x04b0, 0x0177, PTP_CAP|PTP_NIKON_BROKEN_CAP},
	/* */
	{"Nikon:Coolpix S225 (PTP mode)", 0x04b0, 0x0178, PTP_CAP|PTP_NIKON_BROKEN_CAP},

	/* Ryan Nestor <ryan@monadnock.org> */
	{"Nikon:Coolpix P100 (PTP mode)", 0x04b0, 0x017d, PTP_CAP|PTP_NO_CAPTURE_COMPLETE},

	/* http://sourceforge.net/tracker/index.php?func=detail&aid=3019787&group_id=8874&atid=358874 */
	/* probably no need for nikon_broken_Cap as it worked without this flag for the user */
	{"Nikon:Coolpix L110 (PTP mode)", 0x04b0, 0x017e, PTP_CAP},

	{"Nikon:Coolpix SQ (PTP mode)",   0x04b0, 0x0202, 0},
	/* lars marowski bree, 16.8.2004 */
	{"Nikon:Coolpix 4200 (PTP mode)", 0x04b0, 0x0204, 0},
	/* Nikon Coolpix 5200: Andy Shevchenko, 18 Jul 2005 */
	{"Nikon:Coolpix 5200 (PTP mode)", 0x04b0, 0x0206, 0},
	/* https://launchpad.net/bugs/63473 */
	{"Nikon:Coolpix L1 (PTP mode)",   0x04b0, 0x0208, 0},
	{"Nikon:Coolpix P4 (PTP mode)",   0x04b0, 0x020c, PTP_CAP},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=3135935&group_id=8874&atid=358874 */
	{"Nikon:Coolpix S8000 (PTP mode)",0x04b0, 0x021f, 0},
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
	/* Nikon D100 has a PTP mode: westin 2002.10.16 */
	{"Nikon:DSC D100 (PTP mode)",     0x04b0, 0x0402, 0},
	/* D2H SLR in PTP mode from Steve Drew <stevedrew@gmail.com> */
	{"Nikon:D2H SLR (PTP mode)",      0x04b0, 0x0404, 0},
	{"Nikon:DSC D70 (PTP mode)",      0x04b0, 0x0406, PTP_CAP},
	/* Justin Case <justin_case@gmx.net> */
	{"Nikon:D2X SLR (PTP mode)",      0x04b0, 0x0408, PTP_CAP},
	/* Niclas Gustafsson (nulleman @ sf) */
	{"Nikon:D50 (PTP mode)",          0x04b0, 0x040a, PTP_CAP}, /* no hidden props */
	{"Nikon:DSC D70s (PTP mode)",     0x04b0, 0x040e, PTP_CAP},
	/* Jana Jaeger <jjaeger.suse.de> */
	{"Nikon:DSC D200 (PTP mode)",     0x04b0, 0x0410, PTP_CAP},
	/* Christian Deckelmann @ SUSE */
	{"Nikon:DSC D80 (PTP mode)",      0x04b0, 0x0412, PTP_CAP},
	/* Huy Hoang <hoang027@umn.edu> */
	{"Nikon:DSC D40 (PTP mode)",      0x04b0, 0x0414, PTP_CAP},
	/* Luca Gervasi <luca.gervasi@gmail.com> */
	{"Nikon:DSC D40x (PTP mode)",     0x04b0, 0x0418, PTP_CAP},
	/* Andreas Jaeger <aj@suse.de>.
	 * Marcus: MTP Proplist does not return objectsizes ... useless. */
	{"Nikon:DSC D300 (PTP mode)",	  0x04b0, 0x041a, PTP_CAP},
	/* Pat Shanahan, http://sourceforge.net/tracker/index.php?func=detail&aid=1924511&group_id=8874&atid=358874 */
	{"Nikon:D3 (PTP mode)",		  0x04b0, 0x041c, PTP_CAP},
	/* irc reporter Benjamin Schindler */
	{"Nikon:DSC D60 (PTP mode)",	  0x04b0, 0x041e, PTP_CAP},
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
	/* http://sourceforge.net/tracker/?func=detail&atid=358874&aid=3140014&group_id=8874 */
	{"Nikon:DSC D7000 (PTP mode)",    0x04b0, 0x0428, PTP_CAP|PTP_CAP_PREVIEW},

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

	{"Panasonic:DMC-FZ20",            0x04da, 0x2374, 0},
	{"Panasonic:DMC-FZ38",            0x04da, 0x2374, 0},
	{"Panasonic:DMC-FZ50",            0x04da, 0x2374, 0},
	/* from Tomas Herrdin <tomas.herrdin@swipnet.se> */
	{"Panasonic:DMC-LS3",             0x04da, 0x2374, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2070864&group_id=8874&atid=358874 */
	{"Panasonic:DMC-TZ15",		  0x04da, 0x2374, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=2950529&group_id=8874 */
	{"Panasonic:DMC-FS62",		  0x04da, 0x2374, 0},
	/* Constantin B <klochto@gmail.com> */
	{"Panasonic:DMC-GF1",             0x04da, 0x2374, 0},

	/* Søren Krarup Olesen <sko@acoustics.aau.dk> */
	{"Leica:D-LUX 2",                 0x04da, 0x2375, 0},

	/* http://callendor.zongo.be/wiki/OlympusMju500 */
	{"Olympus:mju 500",               0x07b4, 0x0113, 0},
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

	/* IRC report */
	{"Casio:EX-Z120",                 0x07cf, 0x1042, 0},
	/* Andrej Semen (at suse) */
	{"Casio:EX-S770",                 0x07cf, 0x1049, 0},
	/* https://launchpad.net/bugs/64146 */
	{"Casio:EX-Z700",                 0x07cf, 0x104c, 0},
	/* IRC Reporter */
	{"Casio:EX-Z65",                  0x07cf, 0x104d, 0},

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
	{"Canon:Digital IXUS 40 (PTP mode)",    0x04a9, 0x30bf, PTPBUG_DELETE_SENDS_EVENT},
 	{"Canon:PowerShot SD200 (PTP mode)",    0x04a9, 0x30c0, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
 	{"Canon:Digital IXUS 30 (PTP mode)",    0x04a9, 0x30c0, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
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

	/* https://sourceforge.net/tracker/?func=detail&aid=2722422&group_id=8874&atid=358874 */
	{"Canon:Digital IXUS 85 IS",		0x04a9, 0x3174, PTPBUG_DELETE_SENDS_EVENT},

	/* Theodore Kilgore <kilgota@banach.math.auburn.edu> */
	{"Canon:PowerShot SD770 IS",		0x04a9, 0x3175, PTPBUG_DELETE_SENDS_EVENT},

	/* Olaf Hering at SUSE */
	{"Canon:PowerShot A590 IS",		0x04a9, 0x3176, PTPBUG_DELETE_SENDS_EVENT},
	
	/* Dmitriy Khanzhin <jinn@altlinux.org> */
	{"Canon:PowerShot A580",		0x04a9, 0x3177, PTPBUG_DELETE_SENDS_EVENT},

	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2602638&group_id=8874&atid=108874 */
	{"Canon:PowerShot A740",		0x04a9, 0x317a, PTP_CAP|PTPBUG_DELETE_SENDS_EVENT},
	/* Michael Plucik <michaelplucik@googlemail.com> */
	{"Canon:EOS 1000D",			0x04a9, 0x317b, PTP_CAP|PTP_CAP_PREVIEW|PTPBUG_DELETE_SENDS_EVENT},

	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1910010&group_id=8874 */
	{"Canon:Digital IXUS 80 IS",		0x04a9, 0x3184, PTPBUG_DELETE_SENDS_EVENT},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=2391422&group_id=8874&atid=358874 */
	{"Canon:Powershot SD1100 IS",		0x04a9, 0x3184, PTPBUG_DELETE_SENDS_EVENT},
	/* Hubert Mercier <hubert.mercier@unilim.fr> */
	{"Canon:PowerShot SX10 IS",		0x04a9, 0x318d, PTPBUG_DELETE_SENDS_EVENT},

	/* Paul Tinsley */
	{"Canon:PowerShot G10",			0x04a9, 0x318f, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},

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
	/* Mark Voorhies <mvoorhie@yahoo.com> */
	{"Canon:PowerShot SD1300 IS",		0x04a9, 0x31f4, PTPBUG_DELETE_SENDS_EVENT},
	/* Juergen Weigert */
	{"Canon:PowerShot SX210 IS",		0x04a9, 0x31f6, PTPBUG_DELETE_SENDS_EVENT},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=3153412&group_id=8874&atid=358874 */
	{"Canon:PowerShot SX130 IS",		0x04a9, 0x3211, PTPBUG_DELETE_SENDS_EVENT},
	/* IRC Reporter */
	{"Canon:EOS 60D",			0x04a9, 0x3215, PTP_CAP|PTP_CAP_PREVIEW},

	/* Konica-Minolta PTP cameras */
	{"Konica-Minolta:DiMAGE A2 (PTP mode)",        0x132b, 0x0001, 0},
	{"Konica-Minolta:DiMAGE Z2 (PictBridge mode)", 0x132b, 0x0007, 0},
	{"Konica-Minolta:DiMAGE X21 (PictBridge mode)",0x132b, 0x0009, 0},
	{"Konica-Minolta:DiMAGE Z3 (PictBridge mode)", 0x132b, 0x0018, 0},
	{"Konica-Minolta:DiMAGE A200 (PictBridge mode)",0x132b, 0x0019, 0},
	{"Konica-Minolta:DiMAGE Z5 (PictBridge mode)", 0x132b, 0x0022, 0},

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
	/* salsaman <salsaman@gmail.com> */
	{"Fuji:FinePix Z700EXR",		0x04cb, 0x020d, 0},

	{"Ricoh:Caplio R5 (PTP mode)",          0x05ca, 0x0110, 0},
	{"Ricoh:Caplio GX (PTP mode)",          0x05ca, 0x0325, 0},
	{"Sea & Sea:5000G (PTP mode)",		0x05ca, 0x0327, 0},
	/* Michael Steinhauser <mistr@online.de> */
	{"Ricoh Caplio:R1v (PTP mode)",		0x05ca, 0x032b, 0},
	{"Ricoh:Caplio R3 (PTP mode)",          0x05ca, 0x032f, 0},
	/* Arthur Butler <arthurbutler@otters.ndo.co.uk> */
	{"Ricoh:Caplio RR750 (PTP mode)",	0x05ca, 0x033d, 0},
	/* Gerald Pfeifer at SUSE */
	{"Sea & Sea:2G (PTP mode)",		0x05ca, 0x0353, 0},

	/* Rollei dr5  */
	{"Rollei:dr5 (PTP mode)",               0x05ca, 0x220f, 0},

	/* Ricoh Caplio GX 8 */
	{"Ricoh:Caplio GX 8 (PTP mode)",        0x05ca, 0x032d, 0},

	/* Pentax cameras */
	{"Pentax:Optio 43WR",                   0x0a17, 0x000d, 0},
	/* Stephan Barth at SUSE */
	{"Pentax:Optio W90",                    0x0a17, 0x00f7, 0},

	{"Sanyo:VPC-C5 (PTP mode)",             0x0474, 0x0230, 0},

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

	/* https://sourceforge.net/tracker/index.php?func=detail&aid=1869653&group_id=158745&atid=809061 */
	{"Pioneer:DVR-LX60D",			0x08e4, 0x0142, 0},

	/* https://sourceforge.net/tracker/index.php?func=detail&aid=1680029&group_id=8874&atid=108874 */
	{"Nokia:N73",				0x0421, 0x0488, 0},
};

#include "device-flags.h"
static struct {
	const char *vendor;
	unsigned short usb_vendor;
	const char *model;
	unsigned short usb_product;
	unsigned long flags;
} mtp_models[] = {
#include "music-players.h"
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
	{PTP_OFC_Text,			0, "text/plain"},
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

	{PTP_OFC_MTP_OGG,		PTP_VENDOR_MICROSOFT, "application/ogg"},
	{PTP_OFC_MTP_FLAC,		PTP_VENDOR_MICROSOFT, "audio/x-flac"},
	{PTP_OFC_MTP_MP2,		PTP_VENDOR_MICROSOFT, "video/mpeg"},
	{PTP_OFC_MTP_M4A,		PTP_VENDOR_MICROSOFT, "audio/x-m4a"},
	{PTP_OFC_MTP_MP4,		PTP_VENDOR_MICROSOFT, "video/mp4"},
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
	{PTP_OFC_CANON_MOV,		PTP_VENDOR_CANON, "video/quicktime"},
	{PTP_OFC_CANON_CHDK_CRW,	PTP_VENDOR_CANON, "image/x-canon-cr2"},
	{0,				0, NULL}
};

static int
set_mimetype (Camera *camera, CameraFile *file, uint16_t vendorcode, uint16_t ofc)
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
	gp_log (GP_LOG_DEBUG, "ptp2/setmimetype", "Failed to find mime type for %04x", ofc);
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
	gp_log (GP_LOG_DEBUG, "ptp2/strcpymimetype", "Failed to find mime type for %04x", ofc);
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
	gp_log (GP_LOG_DEBUG, "ptp2/strcpymimetype", "Failed to find mime type for %s", mimetype);
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
	if (camera->pl->bugs & PTP_MTP)
		return 1;
	return 0;
}

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
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
		if (models[i].known_bugs & PTP_CAP)
			a.operations |= GP_OPERATION_CAPTURE_IMAGE | GP_OPERATION_CONFIG;
		if (models[i].known_bugs & PTP_CAP_PREVIEW)
			a.operations |= GP_OPERATION_CAPTURE_PREVIEW;
		a.file_operations	= GP_FILE_OPERATION_PREVIEW |
					GP_FILE_OPERATION_DELETE;
		a.folder_operations	= GP_FOLDER_OPERATION_PUT_FILE |
					GP_FOLDER_OPERATION_MAKE_DIR |
					GP_FOLDER_OPERATION_REMOVE_DIR;
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
	a.operations =	GP_OPERATION_CAPTURE_IMAGE | GP_OPERATION_CONFIG;
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

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "PTP/IP Camera");
	a.status = GP_DRIVER_STATUS_TESTING;
	a.port   = GP_PORT_PTPIP;
	a.operations        =	GP_CAPTURE_IMAGE		|
				GP_OPERATION_CONFIG;
	a.file_operations   =	GP_FILE_OPERATION_PREVIEW	|
				GP_FILE_OPERATION_DELETE;
	a.folder_operations =	GP_FOLDER_OPERATION_PUT_FILE	|
				GP_FOLDER_OPERATION_MAKE_DIR	|
				GP_FOLDER_OPERATION_REMOVE_DIR;
	a.device_type       = GP_DEVICE_STILL_CAMERA;
	CR (gp_abilities_list_append (list, a));

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
	if (camera->pl!=NULL) {
		PTPParams *params = &camera->pl->params;
		PTPContainer event;
		SET_CONTEXT_P(params, context);
#ifdef HAVE_ICONV
		/* close iconv converters */
		iconv_close(params->cd_ucs2_to_locale);
		iconv_close(params->cd_locale_to_ucs2);
#endif
		/* Disable EOS capture now, also end viewfinder mode. */
		if (params->eos_captureenabled) {
			if (camera->pl->checkevents) {
				PTPCanon_changes_entry entry;

				ptp_check_eos_events (params);
				while (ptp_get_one_eos_event (params, &entry)) {
					gp_log (GP_LOG_DEBUG, "camera_exit", "missed EOS ptp type %d", entry.type);
					if (entry.type == PTP_CANON_EOS_CHANGES_TYPE_UNKNOWN)
						free (entry.u.info);
				}
				camera->pl->checkevents = 0;
			}
			if (params->eos_viewfinderenabled)
				ptp_canon_eos_end_viewfinder (params);
			camera_unprepare_capture (camera, context);
		}
		if (camera->pl->checkevents)
			ptp_check_event (params);
		while (ptp_get_one_event (params, &event))
			gp_log (GP_LOG_DEBUG, "camera_exit", "missed ptp event 0x%x (param1=%x)", event.Code, event.Param1);
		/* close ptp session */
		ptp_closesession (params);
		ptp_free_params(params);
		free (params->data);
		free (camera->pl); /* also frees params */
		params = NULL;
		camera->pl = NULL;
	}
	if ((camera->port!=NULL) && (camera->port->type == GP_PORT_USB)) {
		/* clear halt */
		gp_port_usb_clear_halt
				(camera->port, GP_PORT_USB_ENDPOINT_IN);
		gp_port_usb_clear_halt
				(camera->port, GP_PORT_USB_ENDPOINT_OUT);
		gp_port_usb_clear_halt
				(camera->port, GP_PORT_USB_ENDPOINT_INT);
	}
	return (GP_OK);
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
	   "Enjoy!"), 2010);
	return (GP_OK);
}

static inline int
handle_to_n (uint32_t handle, Camera *camera)
{
	int i;
	for (i = 0; i < camera->pl->params.handles.n; i++)
		if (camera->pl->params.handles.Handler[i]==handle) return i;
	/* else not found */
	return (PTP_HANDLER_SPECIAL);
}

static inline int
storage_handle_to_n (uint32_t storage, uint32_t handle, Camera *camera)
{
	int i;
	for (i = 0; i < camera->pl->params.handles.n; i++)
		if (	(camera->pl->params.handles.Handler[i] == handle) &&
			(camera->pl->params.objectinfo[i].StorageID == storage)
		)
			return i;
	/* else not found */
	return (PTP_HANDLER_SPECIAL);
}

static void debug_objectinfo(PTPParams *params, uint32_t oid, PTPObjectInfo *oi);

/* add new object to internal driver structures. issued when creating
folder, uploading objects, or captured images. */
static int
add_object (Camera *camera, uint32_t handle, GPContext *context)
{
	int 		n;
	uint16_t	ret;
	PTPParams *params = &camera->pl->params;

	/* increase number of objects */
	n = ++params->handles.n;
	/* realloc more space for camera->pl->params.objectinfo */
	params->objectinfo = (PTPObjectInfo*)
		realloc(params->objectinfo,
			sizeof(PTPObjectInfo)*n);
	/* realloc more space for params->handles.Handler */
	params->handles.Handler= (uint32_t *)
		realloc(params->handles.Handler,
			sizeof(uint32_t)*n);
	if (params->canon_flags) {
		/* FIXME: should init the value to the current object value. */
		params->canon_flags = realloc(params->canon_flags,sizeof(params->canon_flags[0])*n);
		params->canon_flags[n-1] = 0;
	}
	/* clear objectinfo entry for new object and assign new handler */
	memset(&params->objectinfo[n-1],0,sizeof(PTPObjectInfo));
	params->handles.Handler[n-1]=handle;
	/* get new obectinfo */
	ret = ptp_getobjectinfo(params, handle, &params->objectinfo[n-1]);
	if (ret != PTP_RC_OK) {
		params->handles.n--;
		return translate_ptp_result (ret);
	}
	debug_objectinfo (params, handle, &params->objectinfo[n-1]);
	return (GP_OK);
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	unsigned char	*data = NULL, *jpgStartPtr = NULL, *jpgEndPtr = NULL;
	uint32_t	size = 0;
	int ret;
	PTPParams *params = &camera->pl->params;

	camera->pl->checkevents = TRUE;
	switch (params->deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_CANON:
		/* Canon PowerShot / IXUS preview mode */
		if (ptp_operation_issupported(params, PTP_OC_CANON_ViewfinderOn)) {
			SET_CONTEXT_P(params, context);
			if (!params->canon_viewfinder_on) { /* enable on demand, but just once */
				ret = ptp_canon_viewfinderon (params);
				if (ret != PTP_RC_OK) {
					gp_context_error (context, _("Canon enable viewfinder failed: %d"), ret);
					SET_CONTEXT_P(params, NULL);
					return translate_ptp_result (ret);
				}
				params->canon_viewfinder_on = 1;
			}
			ret = ptp_canon_getviewfinderimage (params, &data, &size);
			if (ret != PTP_RC_OK) {
				gp_context_error (context, _("Canon get viewfinder image failed: %d"), ret);
				SET_CONTEXT_P(params, NULL);
				return translate_ptp_result (ret);
			}
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
			int 			tries = 20;
			PTPDevicePropDesc       dpd;

			SET_CONTEXT_P(params, context);

			if (!params->eos_captureenabled)
				camera_prepare_capture (camera, context);
			memset (&dpd,0,sizeof(dpd));
			/* do not set it everytime, it will cause delays */
			ret = ptp_canon_eos_getdevicepropdesc (params, PTP_DPC_CANON_EOS_EVFOutputDevice, &dpd);
			if ((ret != PTP_RC_OK) || (dpd.CurrentValue.u32 != 2)) {
				/* 2 means PC, 1 means TFT */
				val.u32 = 2;
				ret = ptp_canon_eos_setdevicepropvalue (params, PTP_DPC_CANON_EOS_EVFOutputDevice, &val, PTP_DTC_UINT32);
				if (ret != PTP_RC_OK) {
					gp_log (GP_LOG_ERROR,"ptp2_prepare_eos_preview", "setval of evf outputmode to 2 failed!");
					return translate_ptp_result (ret);
				}
			}
			ptp_free_devicepropdesc (&dpd);
			params->eos_viewfinderenabled = 1;
			while (tries--) {
				/* Poll for camera events, but just call
				 * it once and do not drain the queue now */
				ret = ptp_check_eos_events (params);
				if (ret != PTP_RC_OK)
					return translate_ptp_result (ret);

				ret = ptp_canon_eos_get_viewfinder_image (params , &data, &size);
				if (ret == PTP_RC_OK) {
					uint32_t	len = dtoh32a(data);

					/* 4 byte len of jpeg data, 4 byte unknown */
					/* JPEG blob */
					/* stuff */
					gp_log (GP_LOG_DEBUG,"ptp2_capture_eos_preview", "get_viewfinder_image unknown bytes: 0x%02x 0x%02x 0x%02x 0x%02x", data[4],data[5],data[6],data[7]);

					if (len > size) len = size;
					gp_file_append ( file, (char*)data+8, len );
					gp_file_set_mime_type (file, GP_MIME_JPEG);     /* always */
					/* Add an arbitrary file name so caller won't crash */
					gp_file_set_name (file, "preview.jpg");
					free (data);
					SET_CONTEXT_P(params, NULL);
					return GP_OK;
				} else {
					if (ret == 0xa102) { /* means "not there yet" ... so wait */
						ret = ptp_check_eos_events (params);
						if (ret != PTP_RC_OK)
							return translate_ptp_result (ret);
						gp_context_idle (context);
						usleep (50*1000);
						continue;
					}
					gp_log (GP_LOG_ERROR,"ptp2_capture_eos_preview", "get_viewfinder_image failed: 0x%x", ret);
					SET_CONTEXT_P(params, NULL);
					return translate_ptp_result (ret);
				}
			}
			gp_log (GP_LOG_ERROR,"ptp2_capture_eos_preview","get_viewfinder_image failed after 20 tries with ret: 0x%x\n", ret);
			SET_CONTEXT_P(params, NULL);
			return translate_ptp_result (ret);
		}
		gp_context_error (context, _("Sorry, your Canon camera does not support Canon Viewfinder mode"));
		return GP_ERROR_NOT_SUPPORTED;
	case PTP_VENDOR_NIKON: {
		PTPPropertyValue	value;

		if (!ptp_operation_issupported(params, PTP_OC_NIKON_StartLiveView)) {
			gp_context_error (context,
				_("Sorry, your Nikon camera does not support LiveView mode"));
			return GP_ERROR_NOT_SUPPORTED;
		}
		SET_CONTEXT_P(params, context);
		ret = ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewStatus, &value, PTP_DTC_UINT8);
		if (ret != PTP_RC_OK)
			value.u8 = 0;

		if (!value.u8) {
			value.u8 = 1;
			ret = ptp_setdevicepropvalue (params, PTP_DPC_NIKON_RecordingMedia, &value, PTP_DTC_UINT8);
			if (ret != PTP_RC_OK)
				gp_log (GP_LOG_DEBUG, "ptp2/nikon_preview", "set recordingmedia to 1 failed with 0x%04x", ret);
			ret = ptp_nikon_start_liveview (params);
			if (ret != PTP_RC_OK) {
				gp_context_error (context, _("Nikon enable liveview failed: %x"), ret);
				SET_CONTEXT_P(params, NULL);
				return translate_ptp_result (ret);
			}
			while (ptp_nikon_device_ready(params) != PTP_RC_OK) /* empty */;
		}

		ret = ptp_nikon_get_liveview_image (params , &data, &size);
		if (ret == PTP_RC_OK) {
			/* look for the JPEG SOI marker (0xFFD8) in data */
			jpgStartPtr = (unsigned char*)memchr(data, 0xff, size);
			while(jpgStartPtr && ((jpgStartPtr+1) < (data + size))) {
				if(*(jpgStartPtr + 1) == 0xd8) { /* SOI found */
					break;
				} else { /* go on looking (starting at next byte) */
					jpgStartPtr++;
					jpgStartPtr = (unsigned char*)memchr(jpgStartPtr, 0xff, data + size - jpgStartPtr);
				}
			}
			if(!jpgStartPtr) { /* no SOI -> no JPEG */
				gp_context_error (context, _("Sorry, your Nikon camera does not seem to return a JPEG image in LiveView mode"));
				return GP_ERROR;
			}
			/* if SOI found, start looking for EOI marker (0xFFD9) one byte after SOI
			   (just to be sure we will not go beyond the end of the data array) */
			jpgEndPtr = (unsigned char*)memchr(jpgStartPtr+1, 0xff, data+size-jpgStartPtr-1);
			while(jpgEndPtr && ((jpgEndPtr+1) < (data + size))) {
				if(*(jpgEndPtr + 1) == 0xd9) { /* EOI found */
					jpgEndPtr += 2;
					break;
				} else { /* go on looking (starting at next byte) */
					jpgEndPtr++;
					jpgEndPtr = (unsigned char*)memchr(jpgEndPtr, 0xff, data + size - jpgEndPtr);
				}
			}
			if(!jpgEndPtr) { /* no EOI -> no JPEG */
				gp_context_error (context, _("Sorry, your Nikon camera does not seem to return a JPEG image in LiveView mode"));
				return GP_ERROR;
			}
			gp_file_append (file, (char*)jpgStartPtr, jpgEndPtr-jpgStartPtr);
			free (data); /* FIXME: perhaps handle the 128 byte header data too. */
			gp_file_set_mime_type (file, GP_MIME_JPEG);     /* always */
			/* Add an arbitrary file name so caller won't crash */
			gp_file_set_name (file, "preview.jpg");
			gp_file_set_mtime (file, time(NULL));
		} else {
			SET_CONTEXT_P(params, NULL);
			return translate_ptp_result (ret);
		}
#if 0
		ret = ptp_nikon_end_liveview (params);
		if (ret != PTP_RC_OK) {
			gp_context_error (context, _("Nikon disable liveview failed: %x"), ret);
			SET_CONTEXT_P(params, NULL);
			//return GP_ERROR;
		}
#endif
		SET_CONTEXT_P(params, NULL);
		return GP_OK;
	}
	default:
		break;
	}
	return GP_ERROR_NOT_SUPPORTED;
}

static int
get_folder_from_handle (Camera *camera, uint32_t storage, uint32_t handle, char *folder) {
	int i, ret;

	if (handle == PTP_HANDLER_ROOT)
		return GP_OK;

	i = storage_handle_to_n (storage, handle, camera);
	if (i == PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	ret = get_folder_from_handle (camera, storage, camera->pl->params.objectinfo[i].ParentObject, folder);
	if (ret != GP_OK)
		return ret;

	strcat (folder, camera->pl->params.objectinfo[i].Filename);
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
	gp_file_set_type (file, GP_FILE_TYPE_NORMAL);
	gp_file_set_name (file, path->name);
	gp_file_set_mtime (file, time(NULL));
	set_mimetype (camera, file, params->deviceinfo.VendorExtensionID, oi->ObjectFormat);
	CPR (context, ptp_getobject(params, newobject, &ximage));

	gp_log (GP_LOG_DEBUG, "ptp/add_objectid_and_upload", "setting size");
	ret = gp_file_set_data_and_size(file, (char*)ximage, oi->ObjectCompressedSize);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	gp_log (GP_LOG_DEBUG, "ptp/add_objectid_and_upload", "append to fs");
	ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
        if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	gp_log (GP_LOG_DEBUG, "ptp/add_objectid_and_upload", "adding filedata to fs");
	ret = gp_filesystem_set_file_noop(camera->fs, path->folder, file, context);
        if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}

	/* We have now handed over the file, disclaim responsibility by unref. */
	gp_file_unref (file);

	/* we also get the fs info for free, so just set it */
	info.file.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_NAME |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE | GP_FILE_INFO_MTIME;
	strcpy_mime (info.file.type, params->deviceinfo.VendorExtensionID, oi->ObjectFormat);
	strcpy(info.file.name,path->name);
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
	gp_log (GP_LOG_DEBUG, "ptp/add_objectid_and_upload", "setting fileinfo in fs");
	return gp_filesystem_set_info_noop(camera->fs, path->folder, info, context);
}

/**
 * camera_nikon_capture:
 * params:      Camera*			- camera object
 *              CameraCaptureType type	- type of object to capture
 *              CameraFilePath *path    - filled out with filename and folder on return
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
		GPContext *context)
{
	static int capcnt = 0;
	PTPObjectInfo		oi;
	PTPParams		*params = &camera->pl->params;
	PTPDevicePropDesc	propdesc;
	PTPPropertyValue	propval;
	int			inliveview, i, ret, hasc101 = 0, burstnumber = 1;
	uint32_t		newobject;

	if (type != GP_CAPTURE_IMAGE)
		return GP_ERROR_NOT_SUPPORTED;

	if (params->deviceinfo.VendorExtensionID!=PTP_VENDOR_NIKON)
		return GP_ERROR_NOT_SUPPORTED;

	if (!ptp_operation_issupported(params,PTP_OC_NIKON_Capture)) {
		gp_context_error(context,
               	_("Sorry, your camera does not support Nikon capture"));
		return GP_ERROR_NOT_SUPPORTED;
	}
	init_ptp_fs (camera, context);
	if (	ptp_property_issupported(params, PTP_DPC_StillCaptureMode)	&&
		(PTP_RC_OK == ptp_getdevicepropdesc (params, PTP_DPC_StillCaptureMode, &propdesc))) {
		PTPDevicePropDesc       burstdesc;

		if ((propdesc.DataType == PTP_DTC_UINT16)			&&
		    (propdesc.CurrentValue.u16 == 2) /* Burst Mode */		&&
		    ptp_property_issupported(params, PTP_DPC_BurstNumber)	&&
		    (PTP_RC_OK == ptp_getdevicepropdesc (params, PTP_DPC_BurstNumber, &burstdesc))) {
			if (burstdesc.DataType == PTP_DTC_UINT16) {
				burstnumber = burstdesc.CurrentValue.u16;
				gp_log (GP_LOG_DEBUG, "ptp2", "burstnumber %d", burstnumber);
			}
			ptp_free_devicepropdesc (&burstdesc);
		    }
		ptp_free_devicepropdesc (&propdesc);
	}

	/* if in liveview mode, we have to run non-af capture */
	inliveview = 0;
	if (ptp_property_issupported (params, PTP_DPC_NIKON_LiveViewStatus)) {
		ret = ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewStatus, &propval, PTP_DTC_UINT8);
		if (ret == PTP_RC_OK)
			inliveview = propval.u8;
	}

	if (!inliveview && ptp_operation_issupported(params,PTP_OC_NIKON_AfCaptureSDRAM)) {
		do {
			ret = ptp_nikon_capture_sdram(params);
		} while (ret == PTP_RC_DeviceBusy);
	} else {
		do {
			ret = ptp_nikon_capture(params, 0xffffffff);
		} while (ret == PTP_RC_DeviceBusy);
	}
	CPR (context, ret);

	CR (gp_port_set_timeout (camera->port, capture_timeout));

	newobject = 0xffff0001;
	while (!((ptp_nikon_device_ready(params) == PTP_RC_OK) && hasc101)) {
		PTPContainer	event;

		/* Just busy loop until the camera is ready again. */
		/* and wait for the 0xc101 event */
		gp_context_idle (context);
		CPR (context, ptp_check_event (params));
		while (ptp_get_one_event(params, &event)) {
			gp_log (GP_LOG_DEBUG , "ptp/nikon_capture", "event.Code is %x / param %lx", event.Code, (unsigned long)event.Param1);
			if (event.Code == PTP_EC_Nikon_ObjectAddedInSDRAM) {
				hasc101=1;
				newobject = event.Param1;
				if (!newobject) newobject = 0xffff0001;
			}
		}
	}
	if (!newobject) newobject = 0xffff0001;

	for (i=0;i<burstnumber;i++) {
		/* In Burst mode, the image is always 0xffff0001.
		 * The firmware just gives us one after the other for the same ID
		 * Not so for the D700 :/
		 */
		ret = ptp_getobjectinfo (params, newobject, &oi);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_ERROR,"nikon_capture","getobjectinfo(%x) failed: %d", newobject, ret);
			return translate_ptp_result (ret);
		}
		if (oi.ParentObject != 0)
			gp_log (GP_LOG_ERROR,"nikon_capture", "Parentobject is 0x%lx now?", (unsigned long)oi.ParentObject);
		/* Happens on Nikon D70, we get Storage ID 0. So fake one. */
		if (oi.StorageID == 0)
			oi.StorageID = 0x00010001;
		sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx",(unsigned long)oi.StorageID);
		if (oi.ObjectFormat != PTP_OFC_EXIF_JPEG) {
			gp_log (GP_LOG_DEBUG,"nikon_capture", "raw? ofc is 0x%04x, name is %s", oi.ObjectFormat,oi.Filename);
			sprintf (path->name, "capt%04d.nef", capcnt++);
		} else {
			sprintf (path->name, "capt%04d.jpg", capcnt++);
		}
		ret = add_objectid_and_upload (camera, path, context, newobject, &oi);
		if (ret != GP_OK) {
			gp_log (GP_LOG_ERROR, "nikon_capture", "failed to add object\n");
			return ret;
		}
/* this does result in 0x2009 (invalid object id) with the D90 ... curiuos
		ret = ptp_nikon_delete_sdram_image (params, newobject);
 */
		ret = ptp_deleteobject (params, newobject, 0);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_ERROR,"nikon_capture","deleteobject(%x) failed: %x", newobject, ret);
		}
	}
	return GP_OK;
}

/* 60 seconds timeout ... (for long cycles) */
#define EOS_CAPTURE_TIMEOUT 60

/* This is currently the capture method used by the EOS 400D
 * ... in development.
 */
static int
camera_canon_eos_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	int			ret;
	PTPParams		*params = &camera->pl->params;
	uint32_t		newobject = 0x0;
	PTPCanon_changes_entry	entry;
	CameraFile		*file = NULL;
	unsigned char		*ximage = NULL;
	static int		capcnt = 0;
	PTPObjectInfo		oi;
	int			sleepcnt = 1;
	uint32_t		result;
	time_t                  capture_start=time(NULL);

	if (!ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease)) {
		gp_context_error (context,
		_("Sorry, your Canon camera does not support Canon EOS Capture"));
		return GP_ERROR_NOT_SUPPORTED;
	}
	if (!params->eos_captureenabled)
		camera_prepare_capture (camera, context);
	else
		CR( camera_canon_eos_update_capture_target(camera, context, -1));

	/* Get the initial bulk set of event data, otherwise
	 * capture might return busy. */
	ptp_check_eos_events (params);

	ret = ptp_canon_eos_capture (params, &result);
	if (ret != PTP_RC_OK) {
		gp_context_error (context, _("Canon EOS Capture failed: %x"), ret);
		return translate_ptp_result (ret);
	}
	if ((result & 0x7000) == 0x2000) { /* also happened */
		gp_context_error (context, _("Canon EOS Capture failed: %x"), result);
		return translate_ptp_result (result);
	}
	gp_log (GP_LOG_DEBUG, "ptp2/canon_eos_capture", "result is %d", result);
	if (result == 1) {
		gp_context_error (context, _("Canon EOS Capture failed to release: Perhaps no focus?"));
		return GP_ERROR;
	}
	if (result == 7) {
		gp_context_error (context, _("Canon EOS Capture failed to release: Perhaps no more memory on card?"));
		return GP_ERROR_NO_MEMORY;
	}
	if (result) {
		gp_context_error (context, _("Canon EOS Capture failed to release: Unknown error %d, please report."), result);
		return GP_ERROR;
	}

	newobject = 0;
	while ((time(NULL)-capture_start)<=EOS_CAPTURE_TIMEOUT) {
		int i;

		ret = ptp_check_eos_events (params);
		if (ret != PTP_RC_OK) {
			gp_context_error (context, _("Canon EOS Get Changes failed: 0x%04x"), ret);
			return translate_ptp_result (ret);
		}
		while (ptp_get_one_eos_event (params, &entry)) {
			sleepcnt = 1;
			gp_log (GP_LOG_DEBUG, "ptp2/canon_eos_capture", "entry type %04x", entry.type);
			if (entry.type == PTP_CANON_EOS_CHANGES_TYPE_UNKNOWN) {
				free (entry.u.info);
				break;
			}
			if (entry.type == PTP_CANON_EOS_CHANGES_TYPE_OBJECTTRANSFER) {
				gp_log (GP_LOG_DEBUG, "ptp2/canon_eos_capture", "Found new object! OID 0x%x, name %s", (unsigned int)entry.u.object.oid, entry.u.object.oi.Filename);
				newobject = entry.u.object.oid;
				memcpy (&oi, &entry.u.object.oi, sizeof(oi));
				break;
			}
			if (entry.type == PTP_CANON_EOS_CHANGES_TYPE_OBJECTINFO) {
				/* just add it to the filesystem, and return in CameraPath */
				gp_log (GP_LOG_DEBUG, "ptp2/canon_eos_capture", "Found new object! OID 0x%x, name %s", (unsigned int)entry.u.object.oid, entry.u.object.oi.Filename);
				newobject = entry.u.object.oid;
				memcpy (&oi, &entry.u.object.oi, sizeof(oi));
				ret = add_object (camera, newobject, context);
				if (ret != GP_OK)
					continue;
				strcpy  (path->name,  oi.Filename);
				sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)oi.StorageID);
				get_folder_from_handle (camera, oi.StorageID, oi.ParentObject, path->folder);
				/* delete last / or we get confused later. */
				path->folder[ strlen(path->folder)-1 ] = '\0';
				gp_filesystem_append (camera->fs, path->folder, path->name, context);
				break;/* for RAW+JPG mode capture, we just return the first image for now, and
				       * let wait_for_event get the rest. */
			}
			if (newobject)
				break;
		}
		if (newobject)
			break;
		/* Nothing done ... do wait backoff ... if we poll too fast, the camera will spend
		 * all time serving the polling. */
		for (i=sleepcnt;i--;) {
			gp_context_idle (context);
			usleep(20*1000); /* 20 ms */
		}
		sleepcnt++; /* incremental back off */
		if (sleepcnt>10) sleepcnt=10;

		/* not really proven to help keep it on */
		CPR (context, ptp_canon_eos_keepdeviceon (params));
	}
	if (newobject == 0)
		return GP_ERROR;
	gp_log (GP_LOG_DEBUG, "ptp2/canon_eos_capture", "object has OFC 0x%x", oi.ObjectFormat);

	if (oi.StorageID) /* all done above */
		return GP_OK;

	strcpy  (path->folder,"/");
	sprintf (path->name, "capt%04d.", capcnt++);
	if (oi.ObjectFormat == PTP_OFC_CANON_CRW || oi.ObjectFormat == PTP_OFC_CANON_CRW3) {
		strcat(path->name, "cr2");
		gp_file_set_mime_type (file, GP_MIME_CRW);
	} else {
		strcat(path->name, "jpg");
		gp_file_set_mime_type (file, GP_MIME_JPEG);
	}

	ret = gp_file_new(&file);
	if (ret!=GP_OK) return ret;
	gp_file_set_type (file, GP_FILE_TYPE_NORMAL);
	gp_file_set_name (file, path->name);
	gp_file_set_mtime (file, time(NULL));

	gp_log (GP_LOG_DEBUG, "ptp2/canon_eos_capture", "trying to get object size=0x%x", oi.ObjectCompressedSize);
	CPR (context, ptp_canon_eos_getpartialobject (params, newobject, 0, oi.ObjectCompressedSize, &ximage));
	CPR (context, ptp_canon_eos_transfercomplete (params, newobject));
	ret = gp_file_set_data_and_size(file, (char*)ximage, oi.ObjectCompressedSize);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	ret = gp_filesystem_set_file_noop(camera->fs, path->folder, file, context);
	if (ret != GP_OK) {
		gp_file_free (file);
		return ret;
	}
	/* We have now handed over the file, disclaim responsibility by unref. */
	gp_file_unref (file);
	return GP_OK;
}

static int
_timeout_passed(struct timeval *start, int timeout) {
	struct timeval curtime;

	gettimeofday (&curtime, NULL);
	return ((curtime.tv_sec - start->tv_sec)*1000)+((curtime.tv_usec - start->tv_usec)/1000) >= timeout;
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
	static int 		capcnt = 0;
	PTPObjectInfo		oi;
	int			found, ret, isevent, timeout, sawcapturecomplete = 0, viewfinderwason = 0;
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

	if (!ptp_property_issupported(params, PTP_DPC_CANON_FlashMode)) {
		/* did not call --set-config capture=on, do it for user */
		ret = camera_prepare_capture (camera, context);
		if (ret != GP_OK)
			return ret;
		if (!ptp_property_issupported(params, PTP_DPC_CANON_FlashMode)) {
			gp_context_error (context,
			_("Sorry, initializing your camera did not work. Please report this."));
			return GP_ERROR_NOT_SUPPORTED;
		}
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
				int k, stgcnt = 0;
				for (k=0;k<storageids.n;k++) {
					if (!(storageids.Storage[k] & 0xffff)) continue;
					if (storageids.Storage[k] == 0x80000001) continue;
					stgcnt++;
				}
				if (!stgcnt) {
					gp_log (GP_LOG_DEBUG, "ptp", "Assuming no CF card present - switching to MEMORY Transfer.");
					propval.u16 = xmode = CANON_TRANSFER_MEMORY;
				}
				free (storageids.Storage);
			}
		}
		ret = ptp_setdevicepropvalue(params, PTP_DPC_CANON_CaptureTransferMode, &propval, PTP_DTC_UINT16);
		if (ret != PTP_RC_OK)
			gp_log (GP_LOG_DEBUG, "ptp", "setdevicepropvalue CaptureTransferMode failed, %x", ret);
	}

	if (params->canon_viewfinder_on) { /* disable during capture ... reenable later on. */
		ret = ptp_canon_viewfinderoff (params);
		if (ret != PTP_RC_OK) {
			gp_context_error (context, _("Canon disable viewfinder failed: %d"), ret);
			SET_CONTEXT_P(params, NULL);
			return translate_ptp_result (ret);
		}
		viewfinderwason = 1;
		params->canon_viewfinder_on = 0;
	}
	ptp_check_event (params);

#if 0
	/* FIXME: For now, to avoid flash during debug */
	propval.u8 = 0;
	ret = ptp_setdevicepropvalue(params, PTP_DPC_CANON_FlashMode, &propval, PTP_DTC_UINT8);
#endif
	ret = ptp_canon_initiatecaptureinmemory (params);
	if (ret != PTP_RC_OK) {
		gp_context_error (context, _("Canon Capture failed: %x"), ret);
		return translate_ptp_result (ret);
	}
	sawcapturecomplete = 0;
	/* Checking events in stack. */
	gettimeofday (&event_start, NULL);
	found = FALSE;

	gp_port_get_timeout (camera->port, &timeout);
	CR (gp_port_set_timeout (camera->port, capture_timeout));
	while (!_timeout_passed(&event_start, capture_timeout)) {
		gp_context_idle (context);
		/* Make sure we do not poll USB interrupts after the capture complete event.
		 * MacOS libusb 1 has non-timing out interrupts so we must avoid event reads that will not
		 * result in anything.
		 */
		if (!sawcapturecomplete && (PTP_RC_OK == params->event_check (params, &event))) {
			if (event.Code == PTP_EC_CaptureComplete) {
				sawcapturecomplete = 1;
				gp_log (GP_LOG_DEBUG, "ptp", "Event: capture complete.");
			} else
				gp_log (GP_LOG_DEBUG, "ptp", "Unknown event: 0x%X", event.Code);
			isevent = 1;
		} else {
			ret = ptp_canon_checkevent (params,&event,&isevent);
			if (ret!=PTP_RC_OK)
				continue;
		}
		if (!isevent)
			continue;
		gp_log (GP_LOG_DEBUG, "ptp","evdata: nparams=0x%X, code=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", event.Nparam,event.Code,event.Transaction_ID, event.Param1, event.Param2, event.Param3);
		switch (event.Code) {
		case PTP_EC_ObjectAdded: {
			/* add newly created object to internal structures. this hopefully just is a new folder */
			PTPObjectInfo	*obinfo;
			int		j;

			ret = add_object (camera, event.Param1, context);
			if (ret != GP_OK)
				break;
			j = handle_to_n (event.Param1, camera);
			obinfo = &params->objectinfo[j];
			/* this might be just the folder add, ignore that. */
			if (obinfo->ObjectFormat == PTP_OFC_Association) {
				/* new directory ... mark fs as to be refreshed */
				gp_filesystem_reset (camera->fs); /* FIXME: implement more lightweight folder add */
				break;
			} else {
				/* new file */
				newobject = event.Param1;
				/* FALLTHROUGH */
			}
		}
		case PTP_EC_CANON_RequestObjectTransfer: {
			int j;

			newobject = event.Param1;
			gp_log (GP_LOG_DEBUG, "ptp", "PTP_EC_CANON_RequestObjectTransfer, object handle=0x%X.",newobject);
			for (j=0;j<2;j++) {
				isevent = 0;
				ret=ptp_canon_checkevent(params,&event,&isevent);
				if ((ret==PTP_RC_OK) && isevent)
					gp_log (GP_LOG_DEBUG, "ptp", "evdata: L=0x%X, C=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", event.Nparam,event.Code,event.Transaction_ID, event.Param1, event.Param2, event.Param3);
				if (isevent) {
					gp_log (GP_LOG_DEBUG, "ptp", "Unhandled canon event: 0x%04x.", event.Code);
					if (event.Code == PTP_EC_CaptureComplete)
						sawcapturecomplete = 1;
				}
			}
			/* Marcus: Not sure if we really needs this. This refocuses the camera.
			   ret = ptp_canon_reset_aeafawb(params,7);
			 */
			found = TRUE;
			break;
		}
		case PTP_EC_CaptureComplete:
			sawcapturecomplete = 1;
			break;
		}
		if (found == TRUE)
			break;
	}
	CR (gp_port_set_timeout (camera->port, timeout));
	/* Catch event, attempt  2 */
	if (!sawcapturecomplete) {
		if (PTP_RC_OK==params->event_wait (params, &event)) {
			if (event.Code==PTP_EC_CaptureComplete)
				gp_log (GP_LOG_DEBUG, "ptp", "Event: capture complete(2).");
			else
				gp_log (GP_LOG_DEBUG, "ptp", "Event: 0x%X (2)", event.Code);
		} else
			gp_log (GP_LOG_DEBUG, "ptp", "No expected capture complete event");
	}
	if (!found) {
	    gp_log (GP_LOG_DEBUG, "ptp","ERROR: Capture timed out!");
	    return GP_ERROR_TIMEOUT;
	}
	if (viewfinderwason) { /* disable during capture ... reenable later on. */
		viewfinderwason = 0;
		ret = ptp_canon_viewfinderon (params);
		if (ret != PTP_RC_OK) {
			gp_context_error (context, _("Canon enable viewfinder failed: %d"), ret);
			SET_CONTEXT_P(params, NULL);
			return translate_ptp_result (ret);
		}
		params->canon_viewfinder_on = 1;
	}

	/* FIXME: handle multiple images (as in BurstMode) */
	ret = ptp_getobjectinfo (params, newobject, &oi);
	if (ret != PTP_RC_OK) return translate_ptp_result (ret);

	if (oi.ParentObject != 0) {
		if (xmode != CANON_TRANSFER_CARD) {
			fprintf (stderr,"parentobject is 0x%x, but not in card mode?\n", oi.ParentObject);
		}
		ret = add_object (camera, newobject, context);
		if (ret != GP_OK)
			return ret;
		strcpy  (path->name,  oi.Filename);
		sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)oi.StorageID);
		get_folder_from_handle (camera, oi.StorageID, oi.ParentObject, path->folder);
		/* delete last / or we get confused later. */
		path->folder[ strlen(path->folder)-1 ] = '\0';
		return gp_filesystem_append (camera->fs, path->folder, path->name, context);
	} else {
		if (xmode == CANON_TRANSFER_CARD) {
			fprintf (stderr,"parentobject is 0, but not in memory mode?\n");
		}
		sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx",(unsigned long)oi.StorageID);
		sprintf (path->name, "capt%04d.jpg", capcnt++);
		return add_objectid_and_upload (camera, path, context, newobject, &oi);
	}
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	PTPContainer event;
	PTPParams *params = &camera->pl->params;
	uint32_t newobject = 0x0;
	int done;

	/* adjust if we ever do sound or movie capture */
	if (type != GP_CAPTURE_IMAGE)
		return GP_ERROR_NOT_SUPPORTED;

	SET_CONTEXT_P(params, context);
	camera->pl->checkevents = TRUE;
	init_ptp_fs (camera, context);

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		ptp_operation_issupported(params, PTP_OC_NIKON_Capture)
	) {
		char buf[1024];
		if ((GP_OK != gp_setting_get("ptp2","capturetarget",buf)) || !strcmp(buf,"sdram"))
			return camera_nikon_capture (camera, type, path, context);
	}
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_InitiateCaptureInMemory)
	) {
		return camera_canon_capture (camera, type, path, context);
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease)
	) {
		return camera_canon_eos_capture (camera, type, path, context);
	}

	if (!ptp_operation_issupported(params,PTP_OC_InitiateCapture)) {
		gp_context_error(context,
               	_("Sorry, your camera does not support generic capture"));
		return GP_ERROR_NOT_SUPPORTED;
	}

	/* A capture may take longer than the standard 8 seconds.
	 * The G5 for instance does, or in dark rooms ...
	 * Even 16 seconds might not be enough. (Marcus)
	 */
	/* ptp_initiatecapture() returns immediately, only the event
	 * indicating that the capure has been completed may occur after
	 * few seconds. moving down the code. (kil3r)
	 */
	CPR(context,ptp_initiatecapture(params, 0x00000000, 0x00000000));
	CR (gp_port_set_timeout (camera->port, capture_timeout));
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

	/* The Nikon way: Does not send AddObject event ... so try else */
	if ((params->deviceinfo.VendorExtensionID==PTP_VENDOR_NIKON) && NIKON_BROKEN_CAP(camera->pl)) {
		PTPObjectHandles	handles;
		int tries = 5;

            	GP_DEBUG("PTPBUG_NIKON_BROKEN_CAPTURE bug workaround");
		while (tries--) {
			int i;
			uint16_t ret = ptp_getobjecthandles (params, PTP_HANDLER_SPECIAL, 0x000000, 0x000000, &handles);
			if (ret != PTP_RC_OK)
				break;

			/* if (handles.n == params->handles.n)
			 *	continue;
			 * While this is a potential optimization, lets skip it for now.
			 */
			newobject = 0;
			for (i=0;i<handles.n;i++) {
				int j;
				for (j=0;j<params->handles.n;j++) {
					if (params->handles.Handler[j] == handles.Handler[i])
						break;
				}
				if (j==params->handles.n) {
					newobject = handles.Handler[i];
					add_object (camera, newobject, context);
					break;
				}
			}
			free (handles.Handler);
			if (newobject)
				break;
			sleep(1);
		}
		goto out;
	}
	/* the standard defined way ... wait for some capture related events. */
	done = 0;
	while (!done) {
		uint16_t ret = params->event_wait(params,&event);
		int res;

		CR (gp_port_set_timeout (camera->port, normal_timeout));
		if (ret!=PTP_RC_OK) {
			if ((ret == PTP_ERROR_IO) && newobject) {
				gp_log (GP_LOG_ERROR, "ptp/capture", "Did not receive capture complete event. Going on, but please report and try adding PTP_NO_CAPTURE_COMPLETE flag.");
				break;
			}
			gp_context_error (context,_("No event received, error %x."), ret);
			/* we're not setting *path on error! */
			return translate_ptp_result (ret);
		}
		switch (event.Code) {
		case PTP_EC_ObjectRemoved:
			/* Perhaps from previous Canon based capture + delete. Ignore. */
			break;
		case PTP_EC_ObjectAdded: {
			/* add newly created object to internal structures. but just once (think NEF+JPG) */
			if (!newobject) {
				PTPObjectInfo	*obinfo;
				int		j;

				res = add_object (camera, event.Param1, context);
				if (res != GP_OK)
					break;
				j = handle_to_n (event.Param1, camera);
				obinfo = &params->objectinfo[j];
				/* this might be just the folder add, ignore that. */
				if (obinfo->ObjectFormat == PTP_OFC_Association) {
					/* new directory ... mark fs as to be refreshed */
					gp_filesystem_reset (camera->fs); /* FIXME: implement more lightweight folder add */
				} else {
					/* new file */
					newobject = event.Param1;
					if (NO_CAPTURE_COMPLETE(camera->pl))
						done=1;
				}
			}
			break;
		}
		case PTP_EC_CaptureComplete:
			done=1;
			break;
		default:
			gp_log (GP_LOG_DEBUG,"ptp2/capture", "Received event 0x%04x, ignoring (please report).",event.Code);
			/* done = 1; */
			break;
		}
	}
out:
	/* clear path, so we get defined results even without object info */
	path->name[0]='\0';
	path->folder[0]='\0';

	if (newobject != 0) {
		int i;

		for (i = params->handles.n ; i--; ) {
			PTPObjectInfo	*obinfo;

			if (params->handles.Handler[i] != newobject)
				continue;
			obinfo = &params->objectinfo[i];
			strcpy  (path->name,  obinfo->Filename);
			sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)obinfo->StorageID);
			get_folder_from_handle (camera, obinfo->StorageID, obinfo->ParentObject, path->folder);
			/* delete last / or we get confused later. */
			path->folder[ strlen(path->folder)-1 ] = '\0';
			CR (gp_filesystem_append (camera->fs, path->folder, path->name, context));
			break;
		}
	}
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
	static int 	capcnt = 0;
	uint16_t	ret;
	struct timeval	event_start;
	CameraFile	*file;
	char		*ximage;
	int		sleepcnt = 1;
	int		i;

	SET_CONTEXT(camera, context);
	gp_log (GP_LOG_DEBUG, "ptp2/wait_for_event", "waiting for events timeout %d ms", timeout);

	init_ptp_fs (camera, context);

	memset (&event, 0, sizeof(event));
	*eventtype = GP_EVENT_TIMEOUT;
	*eventdata = NULL;

	gettimeofday (&event_start,NULL);
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease)
	) {
		if (!params->eos_captureenabled)
			camera_prepare_capture (camera, context);
		while (1) {
			int i;
			PTPCanon_changes_entry	entry;

			ret = ptp_check_eos_events (params);
			if (ret != PTP_RC_OK) {
				gp_context_error (context, _("Canon EOS Get Changes failed: 0x%04x"), ret);
				return translate_ptp_result (ret);
			}
			while (ptp_get_one_eos_event (params, &entry)) {
				sleepcnt = 1;
				gp_log (GP_LOG_DEBUG, "ptp2/wait_for_eos_event", "entry type %04x", entry.type);
				switch (entry.type) {
				case PTP_CANON_EOS_CHANGES_TYPE_OBJECTTRANSFER:
					gp_log (GP_LOG_DEBUG, "ptp2/wait_for_eos_event", "Found new object! OID 0x%x, name %s", (unsigned int)entry.u.object.oid, entry.u.object.oi.Filename);
					free (entry.u.object.oi.Filename);

					newobject = entry.u.object.oid;

					path = (CameraFilePath *)malloc(sizeof(CameraFilePath));
					if (!path)
						return GP_ERROR_NO_MEMORY;
					path->name[0]='\0';
					strcpy (path->folder,"/");
					ret = gp_file_new(&file);
					if (ret!=GP_OK) return ret;
					gp_file_set_type (file, GP_FILE_TYPE_NORMAL);
					sprintf (path->name, "capt%04d.", capcnt++);
					if ((entry.u.object.oi.ObjectFormat == PTP_OFC_CANON_CRW) || (entry.u.object.oi.ObjectFormat == PTP_OFC_CANON_CRW3)) {
						strcat(path->name, "cr2");
						gp_file_set_mime_type (file, GP_MIME_CRW);
					} else {
						strcat(path->name, "jpg");
						gp_file_set_mime_type (file, GP_MIME_JPEG);
					}
					gp_file_set_name (file, path->name);
					gp_file_set_mtime (file, time(NULL));

					gp_log (GP_LOG_DEBUG, "ptp2/canon_eos_capture", "trying to get object size=0x%x", entry.u.object.oi.ObjectCompressedSize);
					CPR (context, ptp_canon_eos_getpartialobject (params, newobject, 0, entry.u.object.oi.ObjectCompressedSize, (unsigned char**)&ximage));
					CPR (context, ptp_canon_eos_transfercomplete (params, newobject));
					ret = gp_file_set_data_and_size(file, (char*)ximage, entry.u.object.oi.ObjectCompressedSize);
					if (ret != GP_OK) {
						gp_file_free (file);
						return ret;
					}
					ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
					if (ret != GP_OK) {
						gp_file_free (file);
						return ret;
					}
					ret = gp_filesystem_set_file_noop(camera->fs, path->folder, file, context);
					if (ret != GP_OK) {
						gp_file_free (file);
						return ret;
					}
					*eventtype = GP_EVENT_FILE_ADDED;
					*eventdata = path;
					/* We have now handed over the file, disclaim responsibility by unref. */
					gp_file_unref (file);
					return GP_OK;
				case PTP_CANON_EOS_CHANGES_TYPE_OBJECTINFO:
					/* just add it to the filesystem, and return in CameraPath */
					gp_log (GP_LOG_DEBUG, "ptp2/canon_eos_capture", "Found new objectinfo! OID 0x%x, name %s", (unsigned int)entry.u.object.oid, entry.u.object.oi.Filename);
					newobject = entry.u.object.oid;
					add_object (camera, newobject, context);
					path = (CameraFilePath *)malloc(sizeof(CameraFilePath));
					if (!path)
						return GP_ERROR_NO_MEMORY;
					strcpy  (path->name,  entry.u.object.oi.Filename);
					free (entry.u.object.oi.Filename);
					sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)entry.u.object.oi.StorageID);
					get_folder_from_handle (camera, entry.u.object.oi.StorageID, entry.u.object.oi.ParentObject, path->folder);
					/* delete last / or we get confused later. */
					path->folder[ strlen(path->folder)-1 ] = '\0';
					gp_filesystem_append (camera->fs, path->folder, path->name, context);
					*eventtype = GP_EVENT_FILE_ADDED;
					*eventdata = path;
					return GP_OK;
				case PTP_CANON_EOS_CHANGES_TYPE_PROPERTY: {
					char *x;
					x = malloc(strlen("PTP Property 0123 changed")+1);
					if (x) {
						sprintf (x, "PTP Property %04x changed", entry.u.propid);
						*eventtype = GP_EVENT_UNKNOWN;
						*eventdata = x;
						return GP_OK;
					}
					break;
				}
				case PTP_CANON_EOS_CHANGES_TYPE_CAMERASTATUS: {
					char *x;
					x = malloc(strlen("Camera Status 123456789012345")+1);
					if (x) {
						sprintf (x, "Camera Status %d", entry.u.status);
						*eventtype = GP_EVENT_UNKNOWN;
						*eventdata = x;
						return GP_OK;
					}
					break;
				}
				case PTP_CANON_EOS_CHANGES_TYPE_UNKNOWN:
					/* only return if interesting stuff happened */
					if (entry.u.info) {
						*eventtype = GP_EVENT_UNKNOWN;
						*eventdata = entry.u.info; /* take over the allocated string allocation ownership */
						return GP_OK;
					}
					/* continue otherwise */
					break;
				default:
					gp_log (GP_LOG_DEBUG, "ptp2/wait_for_eos_event", "Unhandled EOS event 0x%04x", entry.type);
					break;
				}
			}
			if (_timeout_passed (&event_start, timeout))
				break;
			/* incremental backoff of polling ... but only if we do not pass the wait time */
			for (i=sleepcnt;i--;) {
				int resttime;
				struct timeval curtime;

				gp_context_idle (context);
				gettimeofday (&curtime, 0);
				resttime = ((curtime.tv_sec - event_start.tv_sec)*1000)+((curtime.tv_usec - event_start.tv_usec)/1000);
				if (resttime < 20)
					break;
				usleep(20*1000); /* 20 ms */
			}
			sleepcnt++; /* incremental back off */
			if (sleepcnt>10) sleepcnt=10;
		}
		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_CheckEvent)
	) {
		while (1) {
			CPR (context, ptp_check_event (params));
			if (ptp_get_one_event(params, &event)) {
				gp_log (GP_LOG_DEBUG, "ptp","canon event: nparam=0x%X, C=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", event.Nparam,event.Code,event.Transaction_ID, event.Param1, event.Param2, event.Param3);
				goto handleregular;
			}
			if (_timeout_passed (&event_start, timeout))
				break;
			gp_context_idle (context);
		}
		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		ptp_operation_issupported(params, PTP_OC_NIKON_CheckEvent)
	) {
		uint32_t	newobject;

		do {
			CPR (context, ptp_check_event (params));
			if (!ptp_get_one_event (params, &event)) {
				if (_timeout_passed (&event_start, timeout))
					break;
				/* incremental backoff wait ... including this wait loop */
				for (i=sleepcnt;i--;) {
					int resttime;
					struct timeval curtime;

					gp_context_idle (context);
					gettimeofday (&curtime, 0);
					resttime = ((curtime.tv_sec - event_start.tv_sec)*1000)+((curtime.tv_usec - event_start.tv_usec)/1000);
					if (resttime < 20)
						break;
					usleep(20*1000); /* 20 ms */
				}
				sleepcnt++; /* incremental back off */
				if (sleepcnt>10) sleepcnt=10;
				continue;
			}
			sleepcnt = 1;

			gp_log (GP_LOG_DEBUG , "ptp/nikon_capture", "event.Code is %x / param %lx", event.Code, (unsigned long)event.Param1);
			switch (event.Code) {
			case PTP_EC_ObjectAdded: {
				int j, res;
				PTPObjectInfo	*obinfo;

				newobject = event.Param1;
				j = handle_to_n (newobject, camera);
				if (j != PTP_HANDLER_SPECIAL) {
					gp_log (GP_LOG_DEBUG , "ptp/wait_nikon_event", "ObjectAdded(0x%lx), is already in handle list", (unsigned long)event.Param1);
					continue;
				}
				path = (CameraFilePath *)malloc(sizeof(CameraFilePath));
				if (!path)
					return GP_ERROR_NO_MEMORY;
				res = add_object (camera, newobject, context);
				if (res != GP_OK) { /* might have been deleted previously */
					free (path);
					continue;
				}
				path->name[0]='\0';
				path->folder[0]='\0';

				j = handle_to_n (newobject, camera);
				if (j == PTP_HANDLER_SPECIAL) {
					free (path);
					continue;
				}

				obinfo = &params->objectinfo[j];
				strcpy  (path->name,  obinfo->Filename);
				sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)obinfo->StorageID);
				get_folder_from_handle (camera, obinfo->StorageID, obinfo->ParentObject, path->folder);
				/* delete last / or we get confused later. */
				path->folder[ strlen(path->folder)-1 ] = '\0';
				if (obinfo->ObjectFormat == PTP_OFC_Association) { /* new folder! */
					*eventtype = GP_EVENT_FOLDER_ADDED;
					*eventdata = path;
					gp_filesystem_reset (camera->fs); /* FIXME: implement more lightweight folder add */
					/* if this was the last current event ... stop and return the folder add */
					return GP_OK;
				} else {
					CR (gp_filesystem_append (camera->fs, path->folder,
								  path->name, context));
					*eventtype = GP_EVENT_FILE_ADDED;
					*eventdata = path;
					return GP_OK;
				}
				break;
			}
			case PTP_EC_Nikon_ObjectAddedInSDRAM: {
				PTPObjectInfo		oi;

				newobject = event.Param1;
				if (!newobject) newobject = 0xffff0001;
				ret = ptp_getobjectinfo (params, newobject, &oi);
				if (ret != PTP_RC_OK)
					continue;
				path = (CameraFilePath *)malloc(sizeof(CameraFilePath));
				if (!path)
					return GP_ERROR_NO_MEMORY;
				path->name[0]='\0';
				strcpy (path->folder,"/");
				ret = gp_file_new(&file);
				if (ret!=GP_OK) return ret;
				if (oi.ObjectFormat != PTP_OFC_EXIF_JPEG) {
					gp_log (GP_LOG_DEBUG,"nikon_wait_event", "raw? ofc is 0x%04x, name is %s", oi.ObjectFormat,oi.Filename);
					sprintf (path->name, "capt%04d.nef", capcnt++);
					gp_file_set_mime_type (file, "image/x-nikon-nef"); /* FIXME */
				} else {
					sprintf (path->name, "capt%04d.jpg", capcnt++);
					gp_file_set_mime_type (file, GP_MIME_JPEG);
				}
				gp_file_set_name (file, path->name);
				gp_file_set_type (file, GP_FILE_TYPE_NORMAL);
				gp_file_set_mtime (file, time(NULL));

				gp_log (GP_LOG_DEBUG, "ptp2/wait_nikon_event", "trying to get object size=0x%x", oi.ObjectCompressedSize);
				CPR (context, ptp_getobject (params, newobject, (unsigned char**)&ximage));
				ret = gp_file_set_data_and_size(file, (char*)ximage, oi.ObjectCompressedSize);
				if (ret != GP_OK) {
					gp_file_free (file);
					return ret;
				}
				ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
				if (ret != GP_OK) {
					gp_file_free (file);
					return ret;
				}
				ret = gp_filesystem_set_file_noop(camera->fs, path->folder, file, context);
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
			{
				*eventtype = GP_EVENT_CAPTURE_COMPLETE;
				*eventdata = NULL;
				return GP_OK;
			}
			case PTP_EC_DevicePropChanged:
			{
				char *x;
				x = malloc(strlen("PTP Property 0123 changed")+1);
				if (x) {
					sprintf (x, "PTP Property %04x changed", event.Param1);
					*eventtype = GP_EVENT_UNKNOWN;
					*eventdata = x;
					return GP_OK;
				}
				break;
			}
			/* as we can read multiple events we should retrieve a good one if possible
			 * and not a random one.*/
			default: {
				char *x;

				*eventtype = GP_EVENT_UNKNOWN;
				x = malloc(strlen("PTP Event 0123, Param1 01234567")+1);
				if (x) {
					sprintf (x, "PTP Event %04x, Param1 %08x", event.Code, event.Param1);
					*eventdata = x;
					return GP_OK;
				}
			}
			}
			if (_timeout_passed (&event_start, timeout))
				break;
		} while (1);
		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}
	CPR (context, ptp_check_event(params));
	if (!ptp_get_one_event (params, &event)) {
		/* FIXME: Might be another error, but usually is a timeout */
		gp_log (GP_LOG_DEBUG, "ptp2", "wait_for_event: no events received.");
		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}
	gp_log (GP_LOG_DEBUG, "ptp2", "wait_for_event: code=0x%04x, param1 0x%08x",
		event.Code, event.Param1
	);

handleregular:
	switch (event.Code) {
	case PTP_EC_CaptureComplete:
		*eventtype = GP_EVENT_CAPTURE_COMPLETE;
		*eventdata = NULL;
		break;
	case PTP_EC_ObjectAdded: {
		PTPObjectInfo	*obinfo;

		path = (CameraFilePath *)malloc(sizeof(CameraFilePath));
		if (!path)
			return GP_ERROR_NO_MEMORY;
		newobject = event.Param1;
		add_object (camera, event.Param1, context);
		path->name[0]='\0';
		path->folder[0]='\0';

		i = handle_to_n (newobject, camera);
		if (i == PTP_HANDLER_SPECIAL)
			return GP_ERROR;
		obinfo = &params->objectinfo[i];
		strcpy  (path->name,  obinfo->Filename);
		sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)obinfo->StorageID);
		get_folder_from_handle (camera, obinfo->StorageID, obinfo->ParentObject, path->folder);
		/* delete last / or we get confused later. */
		path->folder[ strlen(path->folder)-1 ] = '\0';
		if (obinfo->ObjectFormat == PTP_OFC_Association) { /* new folder! */
			*eventtype = GP_EVENT_FOLDER_ADDED;
			*eventdata = path;
			gp_filesystem_reset (camera->fs); /* FIXME: implement more lightweight folder add */
		} else {
			CR (gp_filesystem_append (camera->fs, path->folder,
						  path->name, context));
			*eventtype = GP_EVENT_FILE_ADDED;
			*eventdata = path;
		}
		break;
	}
	case PTP_EC_DevicePropChanged:
	{
		char *x;
		x = malloc(strlen("PTP Property 0123 changed")+1);
		if (x) {
			sprintf (x, "PTP Property %04x changed", event.Param1);
			*eventtype = GP_EVENT_UNKNOWN;
			*eventdata = x;
			return GP_OK;
		}
		break;
	}
	default:
	{
		char *x;

		*eventtype = GP_EVENT_UNKNOWN;
		x = malloc(strlen("PTP Event 0123, Param1 01234567")+1);
		if (x) {
			sprintf (x, "PTP Event %04x, Param1 %08x", event.Code, event.Param1);
			*eventdata = x;
		}
		break;
	}
	}
	return GP_OK;
}

static int
_value_to_str(PTPPropertyValue *data, uint16_t dt, char *txt, int spaceleft) {
	int	n;
	char	*origtxt = txt;

	if (dt == PTP_DTC_STR)
		return snprintf (txt, spaceleft, "'%s'", data->str);
	if (dt & PTP_DTC_ARRAY_MASK) {
		int i;

		n = snprintf (txt, spaceleft, "a[%d] ", data->a.count);
		if (n >= spaceleft) return 0; spaceleft -= n; txt += n;
		for ( i=0; i<data->a.count; i++) {
			n = _value_to_str(&data->a.v[i], dt & ~PTP_DTC_ARRAY_MASK, txt, spaceleft);
			if (n >= spaceleft) return 0; spaceleft -= n; txt += n;
			if (i!=data->a.count-1) {
				n = snprintf (txt, spaceleft, ",");
				if (n >= spaceleft) return 0; spaceleft -= n; txt += n;
			}
		}
		return txt - origtxt;
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
	/*
		PTP_DTC_INT64
		PTP_DTC_UINT64
		PTP_DTC_INT128
		PTP_DTC_UINT128
	*/
		default:
			return snprintf (txt, spaceleft, "Unknown %x", dt);
		}
	}
	return 0;
}

static const char *
_get_getset(uint8_t gs) {
	switch (gs) {
	case PTP_DPGS_Get: return N_("read only");
	case PTP_DPGS_GetSet: return N_("readwrite");
	default: return N_("Unknown");
	}
	return N_("Unknown");
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
	uint16_t	res;
	Camera		*camera = (Camera*)data;
	PTPParams	*params = &camera->pl->params;
	unsigned char	*xdata;
	unsigned int	size;
	int i;
	struct canon_theme_entry	*ent;

	SET_CONTEXT(camera, context);

	res = ptp_canon_get_customize_data (params, 1, &xdata, &size);
	if (res != PTP_RC_OK)  {
		report_result(context, res, params->deviceinfo.VendorExtensionID);
		return translate_ptp_result (res);
	}
	if (size < 42+sizeof(struct canon_theme_entry)*5)
		return GP_ERROR_BAD_PARAMETERS;
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
	uint16_t	res;
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

	res = ptp_nikon_curve_download (params, &xdata, &size);
	if (res != PTP_RC_OK)  {
		report_result(context, res, params->deviceinfo.VendorExtensionID);
		return translate_ptp_result (res);
	}
	tonecurve = (PTPNIKONCurveData *) xdata;
	ntcfile = malloc(2000);
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

static int
camera_summary (Camera* camera, CameraText* summary, GPContext *context)
{
	int n, i, j, ret;
	int spaceleft;
	char *txt;
	PTPParams *params = &(camera->pl->params);
	PTPDeviceInfo pdi;
	PTPStorageIDs storageids;

	SET_CONTEXT(camera, context);

	spaceleft = sizeof(summary->text);
	txt = summary->text;

	n = snprintf (txt, spaceleft,_("Manufacturer: %s\n"),params->deviceinfo.Manufacturer);
	if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
	n = snprintf (txt, spaceleft,_("Model: %s\n"),params->deviceinfo.Model);
	if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
	n = snprintf (txt, spaceleft,_("  Version: %s\n"),params->deviceinfo.DeviceVersion);
	if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
	if (params->deviceinfo.SerialNumber) {
		n = snprintf (txt, spaceleft,_("  Serial Number: %s\n"),params->deviceinfo.SerialNumber);
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
	}
	if (params->deviceinfo.VendorExtensionID) {
		n = snprintf (txt, spaceleft,_("Vendor Extension ID: 0x%x (%d.%d)\n"),
			params->deviceinfo.VendorExtensionID,
			params->deviceinfo.VendorExtensionVersion/100,
			params->deviceinfo.VendorExtensionVersion%100
		);
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		if (params->deviceinfo.VendorExtensionDesc) {
			n = snprintf (txt, spaceleft,_("Vendor Extension Description: %s\n"),params->deviceinfo.VendorExtensionDesc);
			if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		}
	}
	if (params->deviceinfo.StandardVersion != 100) {
		n = snprintf (txt, spaceleft,_("PTP Standard Version: %d.%d\n"),
			params->deviceinfo.StandardVersion/100,
			params->deviceinfo.StandardVersion%100
		);
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
	}
	if (params->deviceinfo.FunctionalMode) {
		n = snprintf (txt, spaceleft,_("Functional Mode: 0x%04x\n"),params->deviceinfo.FunctionalMode);
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
	}

/* Dump Formats */
	n = snprintf (txt, spaceleft,_("\nCapture Formats: "));
	if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

	for (i=0;i<params->deviceinfo.CaptureFormats_len;i++) {
		n = ptp_render_ofc (params, params->deviceinfo.CaptureFormats[i], spaceleft, txt);
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		if (i<params->deviceinfo.CaptureFormats_len-1) {
			n = snprintf (txt, spaceleft," ");
			if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		}
	}
	n = snprintf (txt, spaceleft,"\n");
	if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

	n = snprintf (txt, spaceleft,_("Display Formats: "));
	if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
	for (i=0;i<params->deviceinfo.ImageFormats_len;i++) {
		n = ptp_render_ofc (params, params->deviceinfo.ImageFormats[i], spaceleft, txt);
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		if (i<params->deviceinfo.ImageFormats_len-1) {
			n = snprintf (txt, spaceleft,", ");
			if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		}
	}
	n = snprintf (txt, spaceleft,"\n");
	if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

	if (is_mtp_capable (camera) &&
	    ptp_operation_issupported(params,PTP_OC_MTP_GetObjectPropsSupported)
	) {
		n = snprintf (txt, spaceleft,_("Supported MTP Object Properties:\n"));
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		for (i=0;i<params->deviceinfo.ImageFormats_len;i++) {
			uint16_t ret, *props = NULL;
			uint32_t propcnt = 0;
			int j;

			n = snprintf (txt, spaceleft,"\t");
			if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
			n = ptp_render_ofc (params, params->deviceinfo.ImageFormats[i], spaceleft, txt);
			if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
			n = snprintf (txt, spaceleft,"/%04x:", params->deviceinfo.ImageFormats[i]);
			if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

			ret = ptp_mtp_getobjectpropssupported (params, params->deviceinfo.ImageFormats[i], &propcnt, &props);
			if (ret != PTP_RC_OK) {
				n = snprintf (txt, spaceleft,_(" PTP error %04x on query"), ret);
				if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
			} else {
				for (j=0;j<propcnt;j++) {
					n = snprintf (txt, spaceleft," %04x/",props[j]);
					if (n >= spaceleft) { free (props); return GP_OK;}  spaceleft -= n; txt += n;
					n = ptp_render_mtp_propname(props[j],spaceleft,txt);
					if (n >= spaceleft) { free (props); return GP_OK;} spaceleft -= n; txt += n;
				}
				free(props);
			}
			n = snprintf (txt, spaceleft,"\n");
			if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		}
	}

/* Dump out dynamic capabilities */
	n = snprintf (txt, spaceleft,_("\nDevice Capabilities:\n"));
	if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

	/* First line for file operations */
		n = snprintf (txt, spaceleft,_("\tFile Download, "));
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		if (ptp_operation_issupported(params,PTP_OC_DeleteObject))
			n = snprintf (txt, spaceleft,_("File Deletion, "));
		else
			n = snprintf (txt, spaceleft,_("No File Deletion, "));
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

		if (ptp_operation_issupported(params,PTP_OC_SendObject))
			n = snprintf (txt, spaceleft,_("File Upload\n"));
		else
			n = snprintf (txt, spaceleft,_("No File Upload\n"));
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

	/* Second line for capture */
		if (ptp_operation_issupported(params,PTP_OC_InitiateCapture))
			n = snprintf (txt, spaceleft,_("\tGeneric Image Capture, "));
		else
			n = snprintf (txt, spaceleft,_("\tNo Image Capture, "));
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		if (ptp_operation_issupported(params,PTP_OC_InitiateOpenCapture))
			n = snprintf (txt, spaceleft,_("Open Capture, "));
		else
			n = snprintf (txt, spaceleft,_("No Open Capture, "));
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

		n = 0;
		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		    ptp_operation_issupported(params, PTP_OC_CANON_ViewfinderOn)) {
			n = snprintf (txt, spaceleft,_("Canon Capture\n"));
		} else  {
			if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
			    ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease)) {
				n = snprintf (txt, spaceleft,_("Canon EOS Capture\n"));
			} else  {
				if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
				     ptp_operation_issupported(params, PTP_OC_NIKON_Capture)) {
					n = snprintf (txt, spaceleft,_("Nikon Capture\n"));
				} else {
					n = snprintf (txt, spaceleft,_("No vendor specific capture\n"));
				}
			}
		}
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

	/* Third line for Wifi support, but just leave it out if not there. */
		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		     ptp_operation_issupported(params, PTP_OC_NIKON_GetProfileAllData)) {
			n = snprintf (txt, spaceleft,_("\tNikon Wifi support\n"));
			if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		}

		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		     ptp_operation_issupported(params, PTP_OC_CANON_GetMACAddress)) {
			n = snprintf (txt, spaceleft,_("\tCanon Wifi support\n"));
			if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		}

/* Dump storage information */

	if (ptp_operation_issupported(params,PTP_OC_GetStorageIDs) &&
	    ptp_operation_issupported(params,PTP_OC_GetStorageInfo)
	) {
		CPR (context, ptp_getstorageids(params,
			&storageids));
		n = snprintf (txt, spaceleft,_("\nStorage Devices Summary:\n"));
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

		for (i=0; i<storageids.n; i++) {
			char tmpname[20], *s;

			PTPStorageInfo storageinfo;
			/* invalid storage, storageinfo might fail on it (Nikon D300s e.g.) */
			if ((storageids.Storage[i]&0x0000ffff)==0)
				continue;

			n = snprintf (txt, spaceleft,"store_%08x:\n",(unsigned int)storageids.Storage[i]);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;

			CPR (context, ptp_getstorageinfo(params,
				storageids.Storage[i], &storageinfo));
			n = snprintf (txt, spaceleft,_("\tStorageDescription: %s\n"),
				storageinfo.StorageDescription?storageinfo.StorageDescription:_("None")
			);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
			n = snprintf (txt, spaceleft,_("\tVolumeLabel: %s\n"),
				storageinfo.VolumeLabel?storageinfo.VolumeLabel:_("None")
			);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;

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
			n = snprintf (txt, spaceleft,_("\tStorage Type: %s\n"), s);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;

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
			n = snprintf (txt, spaceleft,_("\tFilesystemtype: %s\n"), s);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;

			switch (storageinfo.AccessCapability) {
			case PTP_AC_ReadWrite: s = _("Read-Write"); break;
			case PTP_AC_ReadOnly: s = _("Read-Only"); break;
			case PTP_AC_ReadOnly_with_Object_Deletion: s = _("Read Only with Object deletion"); break;
			default:
				snprintf(tmpname, sizeof(tmpname), _("Unknown: 0x%04x\n"), storageinfo.AccessCapability);
				s = tmpname;
				break;
			}
			n = snprintf (txt, spaceleft,_("\tAccess Capability: %s\n"), s);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
			n = snprintf (txt, spaceleft,_("\tMaximum Capability: %llu (%lu MB)\n"),
				(unsigned long long)storageinfo.MaxCapability,
				(unsigned long)(storageinfo.MaxCapability/1024/1024)
			);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
			n = snprintf (txt, spaceleft,_("\tFree Space (Bytes): %llu (%lu MB)\n"),
				(unsigned long long)storageinfo.FreeSpaceInBytes,
				(unsigned long)(storageinfo.FreeSpaceInBytes/1024/1024)
			);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
			n = snprintf (txt, spaceleft,_("\tFree Space (Images): %d\n"), (unsigned int)storageinfo.FreeSpaceInImages);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
			if (storageinfo.StorageDescription) free (storageinfo.StorageDescription);
			if (storageinfo.VolumeLabel) free (storageinfo.VolumeLabel);
		}
		free (storageids.Storage);
	}

	n = snprintf (txt, spaceleft,_("\nDevice Property Summary:\n"));
	if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
	/* The information is cached. However, the canon firmware changes
	 * the available properties in capture mode.
	 */
	CPR(context, ptp_getdeviceinfo (params, &pdi));
	fixup_cached_deviceinfo (camera, &pdi);
        for (i=0;i<pdi.DevicePropertiesSupported_len;i++) {
		PTPDevicePropDesc dpd;
		unsigned int dpc = pdi.DevicePropertiesSupported[i];
		const char *propname = ptp_get_property_description (params, dpc);

		if (propname) {
			/* string registered for i18n in ptp.c. */
			n = snprintf(txt, spaceleft, "%s(0x%04x):", _(propname), dpc);
		} else {
			n = snprintf(txt, spaceleft, "Property 0x%04x:", dpc);
		}
		if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;


		/* Do not read the 0xd201 property (found on Creative Zen series).
		 * It seems to cause hangs.
		 */
		if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_MICROSOFT) {
			if (dpc == 0xd201) {
				n = snprintf(txt, spaceleft, _(" not read out.\n"));
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				continue;
			}
		}

		memset (&dpd, 0, sizeof (dpd));
		ret = ptp_getdevicepropdesc (params, dpc, &dpd);
		if (ret == PTP_RC_OK) {
			n = snprintf (txt, spaceleft, "(%s) ",_get_getset(dpd.GetSet));
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
			n = snprintf (txt, spaceleft, "(type=0x%x) ",dpd.DataType);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
			switch (dpd.FormFlag) {
			case PTP_DPFF_None:	break;
			case PTP_DPFF_Range: {
				n = snprintf (txt, spaceleft, "Range [");
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				n = _value_to_str (&dpd.FORM.Range.MinimumValue, dpd.DataType, txt, spaceleft);
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				n = snprintf (txt, spaceleft, " - ");
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				n= _value_to_str (&dpd.FORM.Range.MaximumValue, dpd.DataType, txt, spaceleft);
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				n = snprintf (txt, spaceleft, ", step ");
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				n= _value_to_str (&dpd.FORM.Range.StepSize, dpd.DataType, txt, spaceleft);
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				n = snprintf (txt, spaceleft, "] value: ");
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				break;
			}
			case PTP_DPFF_Enumeration:
				n = snprintf (txt, spaceleft, "Enumeration [");
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				if ((dpd.DataType & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)  {
					n = snprintf (txt, spaceleft, "\n\t");
					if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				}
				for (j = 0; j<dpd.FORM.Enum.NumberOfValues; j++) {
					n = _value_to_str(dpd.FORM.Enum.SupportedValue+j,dpd.DataType,txt, spaceleft);
					if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
					if (j != dpd.FORM.Enum.NumberOfValues-1) {
						n = snprintf (txt, spaceleft, ",");
						if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
						if ((dpd.DataType & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)  {
							n = snprintf (txt, spaceleft, "\n\t");
							if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
						}
					}
				}
				if ((dpd.DataType & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)  {
					n = snprintf (txt, spaceleft, "\n\t");
					if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				}
				n = snprintf (txt, spaceleft, "] value: ");
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				break;
			}
			n = ptp_render_property_value(params, dpc, &dpd, sizeof(summary->text) - strlen(summary->text) - 1, txt);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
			if (n) {
				n = snprintf(txt, spaceleft, " (");
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				n = _value_to_str (&dpd.CurrentValue, dpd.DataType, txt, spaceleft);
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
				n = snprintf(txt, spaceleft, ")");
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
			} else {
				n = _value_to_str (&dpd.CurrentValue, dpd.DataType, txt, spaceleft);
				if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
			}
		} else {
			n = snprintf (txt, spaceleft, _(" error %x on query."), ret);
			if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
		}
		n = snprintf(txt, spaceleft, "\n");
		if (n>=spaceleft) return GP_OK;spaceleft-=n;txt+=n;
		ptp_free_devicepropdesc (&dpd);
        }
	ptp_free_DI (&pdi);
	return (GP_OK);
}

static uint32_t
find_child (const char *file, uint32_t storage, uint32_t handle, Camera *camera)
{
	int i;
	PTPObjectInfo *oi = camera->pl->params.objectinfo;

	for (i = 0; i < camera->pl->params.handles.n; i++) {
		if ((oi[i].StorageID==storage) && (oi[i].ParentObject==handle))
			if (!strcmp(oi[i].Filename,file))
				return (camera->pl->params.handles.Handler[i]);
	}
	/* else not found */
	return (PTP_HANDLER_SPECIAL);
}


static uint32_t
folder_to_handle(const char *folder, uint32_t storage, uint32_t parent, Camera *camera)
{
	char *c;
	if (!strlen(folder)) return PTP_HANDLER_ROOT;
	if (!strcmp(folder,"/")) return PTP_HANDLER_ROOT;

	c=strchr(folder,'/');
	if (c!=NULL) {
		*c=0;
		parent=find_child (folder, storage, parent, camera);
		return folder_to_handle(c+1, storage, parent, camera);
	} else  {
		return find_child (folder, storage, parent, camera);
	}
}
	

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
    Camera *camera = (Camera *)data;
    PTPParams *params = &camera->pl->params;
    uint32_t parent, storage=0x0000000;
    int i,hasgetstorageids;
    SET_CONTEXT_P(params, context);

    gp_log (GP_LOG_DEBUG, "ptp2", "file_list_func(%s)", folder);

    /* There should be NO files in root folder */
    if (!strcmp(folder, "/"))
        return (GP_OK);

    if (!strcmp(folder, "/special")) {
	for (i=0; i<nrofspecial_files; i++)
		CR (gp_list_append (list, special_files[i].name, NULL));
	return (GP_OK);
    }

    init_ptp_fs (camera, context);

    /* compute storage ID value from folder patch */
    folder_to_storage(folder,storage);

    /* Get (parent) folder handle omiting storage pseudofolder */
    find_folder_handle(folder,storage,parent,data);

    hasgetstorageids = ptp_operation_issupported(params,PTP_OC_GetStorageIDs);
    for (i = 0; i < params->handles.n; i++) {
	/* not our parent -> next */
	if (params->objectinfo[i].ParentObject!=parent)
		continue;

	/* not on our storage devices -> next */
	if (	(hasgetstorageids &&
		(params->objectinfo[i].StorageID != storage)))
		continue;

	/* Is a directory -> next */
	if (params->objectinfo[i].ObjectFormat == PTP_OFC_Association)
		continue;

	if (!params->objectinfo[i].Filename)
	    continue;

	if (1 || CAN_HAVE_DUPE_FILE(camera->pl)) {
	    /* HP Photosmart 850, the camera tends to duplicate filename in the list.
             * Original patch by clement.rezvoy@gmail.com */
	    /* search backwards, likely gets hits faster. */
	    /* FIXME Marcus: This is also O(n^2) ... bad for large directories. */
	    if (GP_OK == gp_list_find_by_name(list, NULL, params->objectinfo[i].Filename)) {
		gp_log (GP_LOG_ERROR, "ptp2/file_list_func",
			"Duplicate filename '%s' in folder '%s'. Ignoring nth entry.\n",
			params->objectinfo[i].Filename, folder);
		continue;
	    }
	}
	CR(gp_list_append (list, params->objectinfo[i].Filename, NULL));
    }
    return GP_OK;
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	PTPParams *params = &((Camera *)data)->pl->params;
	int i, hasgetstorageids;
	uint32_t handler,storage;

	SET_CONTEXT_P(params, context);
	gp_log (GP_LOG_DEBUG, "ptp2", "folder_list_func(%s)", folder);
	/* add storage pseudofolders in root folder */
	if (!strcmp(folder, "/")) {
		if (ptp_operation_issupported(params,PTP_OC_GetStorageIDs)) {
			PTPStorageIDs storageids;

			CPR (context, ptp_getstorageids(params, &storageids));
			for (i=0; i<storageids.n; i++) {
				char fname[PTP_MAXSTRLEN];

				/* invalid storage, storageinfo might fail on it (Nikon D300s e.g.) */
				if ((storageids.Storage[i]&0x0000ffff)==0) continue;
				snprintf(fname, sizeof(fname),
					STORAGE_FOLDER_PREFIX"%08x",
					storageids.Storage[i]);
				CR (gp_list_append (list, fname, NULL));
			}
			free (storageids.Storage);
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

	init_ptp_fs ((Camera*)data, context);

	/* compute storage ID value from folder path */
	folder_to_storage(folder,storage);

	/* Get folder handle omiting storage pseudofolder */
	find_folder_handle(folder,storage,handler,data);

	/* Look for objects we can present as directories.
	 * Currently we specify *any* PTP association as directory.
	 */
	hasgetstorageids = ptp_operation_issupported(params,PTP_OC_GetStorageIDs);
	for (i = 0; i < params->handles.n; i++) {
		if (	(params->objectinfo[i].ParentObject==handler)	 &&
			((!hasgetstorageids) || (params->objectinfo[i].StorageID == storage)) &&
			(params->objectinfo[i].ObjectFormat==PTP_OFC_Association)
		)
			CR (gp_list_append (list, params->objectinfo[i].Filename, NULL));
	}
	return (GP_OK);
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
	int j;

	/* ... use little helper call to see if we missed anything in the global
	 * retrieval. */
	ret = ptp_mtp_getobjectpropssupported (params, ofc, &propcnt, &props);
	if (ret != PTP_RC_OK) return translate_ptp_result (ret);
	
	if (params->props) { /* use the fast method, without device access since cached.*/
		char			propname[256];
		char			text[256];
		int 			i, j, n;

		for (j=0;j<params->nrofprops;j++) {
			MTPProperties		*xpl = &params->props[j];

			if (xpl->ObjectHandle != object_id)
				continue;
			for (i=sizeof(uninteresting_props)/sizeof(uninteresting_props[0]);i--;)
				if (uninteresting_props[i] == xpl->property)
					break;
			if (i != -1) /* Is uninteresting. */
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
			case PTP_DTC_INT32:
				sprintf (text, "%d", xpl->propval.i32);
				break;
			case PTP_DTC_INT16:
				sprintf (text, "%d", xpl->propval.i16);
				break;
			case PTP_DTC_INT8:
				sprintf (text, "%d", xpl->propval.i8);
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

		ret = ptp_mtp_getobjectpropdesc (params, props[j], ofc, &opd);
		if (ret != PTP_RC_OK) {
			fprintf (stderr," getobjectpropdesc returns 0x%x\n", ret);
		} else {
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
				case PTP_DTC_INT32:
					sprintf (text, "%d", pv.i32);
					break;
				case PTP_DTC_INT16:
					sprintf (text, "%d", pv.i16);
					break;
				case PTP_DTC_INT8:
					sprintf (text, "%d", pv.i8);
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
	int j;

	if (gp_file_get_data_and_size (file, (const char**)&filedata, &filesize) < GP_OK)
		return (GP_ERROR);

	ret = ptp_mtp_getobjectpropssupported (params, ofc, &propcnt, &props);
	if (ret != PTP_RC_OK) return translate_ptp_result (ret);

	for (j=0;j<propcnt;j++) {
		char			propname[256],propname2[256];
		char			*begin, *end, *content;
		PTPObjectPropDesc	opd;
		int 			i, n;
		PTPPropertyValue	pv;

		for (i=sizeof(readonly_props)/sizeof(readonly_props[0]);i--;)
			if (readonly_props[i] == props[j])
				break;
		if (i != -1) /* Is read/only */
			continue;
		n = ptp_render_mtp_propname(props[j], sizeof(propname), propname);
		sprintf (propname2, "<%s>", propname);
		begin= strstr (filedata, propname2);
		if (!begin) continue;
		begin += strlen(propname2);
		sprintf (propname2, "</%s>", propname);
		end = strstr (begin, propname2);
		if (!end) continue;
		*end = '\0';
		content = strdup(begin);
		*end = '<';
		if (!content)
			continue;
		gp_log (GP_LOG_DEBUG, "ptp2", "found tag %s, content %s", propname, content);
		ret = ptp_mtp_getobjectpropdesc (params, props[j], ofc, &opd);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_DEBUG, "ptp2", " getobjectpropdesc returns 0x%x", ret);
			free (content); content = NULL;
			continue;
		}
		if (opd.GetSet == 0) {
			gp_log (GP_LOG_DEBUG, "ptp2", "Tag %s is read only, sorry.", propname);
			free (content); content = NULL;
			continue;
		}	
		switch (opd.DataType) {
		default:gp_log (GP_LOG_ERROR, "ptp2", "mtp parser: Unknown datatype %d, content %s", opd.DataType, content);
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
	uint16_t	ret;
	int		i, contentlen = 0;
	char		*content = NULL;

	ret = ptp_mtp_getobjectreferences (params, object_id, &objects, &numobjects);
	if (ret != PTP_RC_OK)
		return translate_ptp_result (ret);
	
	for (i=0;i<numobjects;i++) {
		char		buf[4096];
		int		len;

		memset(buf, 0, sizeof(buf));
		len = 0;
		object_id = objects[i];
		do {
			int j = handle_to_n(object_id, camera);
			if (j == PTP_HANDLER_SPECIAL)
				break;
			/* make space for new filename */
			memmove (buf+strlen(params->objectinfo[j].Filename)+1, buf, len);
			memcpy (buf+1, params->objectinfo[j].Filename, strlen (params->objectinfo[j].Filename));
			buf[0] = '/';
			object_id = params->objectinfo[j].ParentObject;
			len = strlen(buf);
		} while (object_id != 0);
		memmove (buf+strlen("/store_00010001"), buf, len);
		sprintf (buf,"/store_%08x",(unsigned int)params->objectinfo[handle_to_n(objects[i],camera)].StorageID);
		buf[strlen(buf)]='/';
		len = strlen(buf);

		if (content) {
			content = realloc (content, contentlen+len+1+1);
			strcpy (content+contentlen, buf);
			strcpy (content+contentlen+len, "\n");
			contentlen += len+1;
		} else {
			content = malloc (len+1+1);
			strcpy (content, buf);
			strcpy (content+len, "\n");
			contentlen = len+1;
		}
	}
	if (!content) content = malloc(1);
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
	uint32_t	storage, objectid, playlistid;
	uint32_t	*oids = NULL;
	int		nrofoids = 0;
	uint16_t	ret;

	while (*s) {
		char *t = strchr(s,'\n');
		char *fn, *filename;
		if (t) {
			fn = malloc (t-s+1);
			if (!fn) return GP_ERROR_NO_MEMORY;
			memcpy (fn, s, t-s);
			fn[t-s]='\0';
		} else {
			fn = malloc (strlen(s)+1);
			if (!fn) return GP_ERROR_NO_MEMORY;
			strcpy (fn,s);
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
		/* Get file number omiting storage pseudofolder */
		find_folder_handle(fn, storage, objectid, camera);
		objectid = find_child(filename, storage, objectid, camera);
		if (objectid != PTP_HANDLER_SPECIAL) {
			if (nrofoids) {
				oids = realloc(oids, sizeof(oids[0])*(nrofoids+1));
				if (!oids) return GP_ERROR_NO_MEMORY;
			} else {
				oids = malloc(sizeof(oids[0]));
				if (!oids) return GP_ERROR_NO_MEMORY;
			}
			oids[nrofoids] = objectid;
			nrofoids++;
		} else {
			/*fprintf (stderr,"%s/%s NOT FOUND!\n", fn, filename);*/
			gp_log (GP_LOG_ERROR , "mtp playlist upload", "Object %s/%s not found on device.", fn, filename);
		}
		free (fn);
		if (!t) break;
		s = t+1;
	}
	oi->ObjectCompressedSize = 1;
	oi->ObjectFormat = PTP_OFC_MTP_AbstractAudioVideoPlaylist;
	ret = ptp_sendobjectinfo(&camera->pl->params, &storage, &oi->ParentObject, &playlistid, oi);
	if (ret != PTP_RC_OK) {
		gp_log (GP_LOG_ERROR, "put mtp playlist", "failed sendobjectinfo of playlist.");
		return translate_ptp_result (ret);
	}
	ret = ptp_sendobject(&camera->pl->params, (unsigned char*)data, 1);
	if (ret != PTP_RC_OK) {
		gp_log (GP_LOG_ERROR, "put mtp playlist", "failed dummy sendobject of playlist.");
		return translate_ptp_result (ret);
	}
	ret = ptp_mtp_setobjectreferences (&camera->pl->params, playlistid, oids, nrofoids);
	if (ret != PTP_RC_OK) {
		gp_log (GP_LOG_ERROR, "put mtp playlist", "failed setobjectreferences.");
		return translate_ptp_result (ret);
	}
	/* update internal structures */
	return add_object(camera, playlistid, context);
}

static int
mtp_get_playlist(
	Camera *camera, CameraFile *file, uint32_t object_id, GPContext *context
) {
	char	*content;
	int	ret, contentlen;

	ret = mtp_get_playlist_string( camera, object_id, &content, &contentlen);
	if (ret != GP_OK) return ret;
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
	unsigned long sendlen, unsigned char *bytes,
	unsigned long *written
) {
	PTPCFHandlerPrivate* priv= (PTPCFHandlerPrivate*)xpriv;
	int ret;
	
	ret = gp_file_append (priv->file, (char*)bytes, sendlen);
	if (ret != GP_OK)
		return PTP_ERROR_IO;
	*written = sendlen;
	return PTP_RC_OK;
}

static uint16_t
ptp_init_camerafile_handler (PTPDataHandler *handler, CameraFile *file) {
	PTPCFHandlerPrivate* priv = malloc (sizeof(PTPCFHandlerPrivate));
	if (!priv) return PTP_RC_GeneralError;
	handler->priv = priv;
	handler->getfunc = gpfile_getfunc;
	handler->putfunc = gpfile_putfunc;
	priv->file = file;
	return PTP_RC_OK;
}

static uint16_t
ptp_exit_camerafile_handler (PTPDataHandler *handler) {
	free (handler->priv);
	return PTP_RC_OK;
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
	uint32_t object_id;
	uint32_t size;
	uint32_t storage;
	PTPObjectInfo * oi;
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
		int i;

		for (i=0;i<nrofspecial_files;i++)
			if (!strcmp (special_files[i].name, filename))
				return special_files[i].getfunc (fs, folder, filename, type, file, data, context);
		return (GP_ERROR_BAD_PARAMETERS); /* file not found */
	}

	init_ptp_fs (camera, context);

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(filename, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	oi=&params->objectinfo[object_id];

	if (oi->ModificationDate != 0) {
		gp_file_set_mtime (file, oi->ModificationDate);
	} else {
		gp_file_set_mtime (file, oi->CaptureDate);
	}

	GP_DEBUG ("Getting file.");
	switch (type) {
	case	GP_FILE_TYPE_EXIF: {
		uint32_t offset;
		uint32_t maxbytes;
		unsigned char 	*ximage = NULL;

		/* Check if we have partial downloads. Otherwise we can just hope
		 * upstream downloads the whole image to get EXIF data. */
		if (!ptp_operation_issupported(params, PTP_OC_GetPartialObject))
			return (GP_ERROR_NOT_SUPPORTED);
		/* Device may hang is a partial read is attempted beyond the file */
		if (oi->ObjectCompressedSize < 10)
			return (GP_ERROR_NOT_SUPPORTED);

		/* We only support JPEG / EXIF format ... others might hang. */
		switch (oi->ObjectFormat) {
		/* tiff style tags ... */
		case PTP_OFC_Undefined: /* 3800 ... used by Nikon NEF */
		case PTP_OFC_TIFF_EP:
		case PTP_OFC_DNG:
		case PTP_OFC_CANON_CRW:
		{
			/* MORE MAGIC */
			unsigned int ifd0offset, dircount, nextifdoff, nextoff, i, xlen;
			/* Note: Could also use Canon partial downloads */
			CPR (context, ptp_getpartialobject (params,
				params->handles.Handler[object_id],
				0, 8, &ximage, &xlen));
			/* FIXME: handle other endianness too */
			if ((ximage[0] != 'M') || (ximage[1] != 'M')) {
				free (ximage);
				return (GP_ERROR_NOT_SUPPORTED);
			}
			if ((ximage[2] != 0) || (ximage[3] != 42)) {
				free (ximage);
				return (GP_ERROR_NOT_SUPPORTED);
			}
			ifd0offset =	 ximage[7] +        (ximage[6] <<  8) +
					(ximage[5] << 16) + (ximage[4] << 24);
			free (ximage);
			/* Read dir count of IFD */
			CPR (context, ptp_getpartialobject (params,
				params->handles.Handler[object_id],
				ifd0offset, 2, &ximage, &xlen));
			dircount = ximage[1] + (ximage[0] << 8);
			free (ximage);
			gp_log (GP_LOG_DEBUG, "ptp2/exif-tiff-reader", "dircount is %d", dircount);
			CPR (context, ptp_getpartialobject (params,
				params->handles.Handler[object_id],
				ifd0offset+2, dircount*12+4 , &ximage, &xlen));
			nextoff = 0;
			for (i=0;i<dircount;i++) {
				unsigned int size = 0;
				unsigned int off = ximage[i*12+11]+
						(ximage[i*12+10] <<8)+
						(ximage[i*12+9] <<16)+
						(ximage[i*12+8] <<24);
				unsigned int cnt = ximage[i*12+7]+
						(ximage[i*12+6] <<8)+
						(ximage[i*12+5] <<16)+
						(ximage[i*12+4] <<24);
				unsigned int type = ximage[i*12+3]+
						(ximage[i*12+2] <<8);
				unsigned int id = ximage[i*12+1]+
						(ximage[i*12+0] <<8);
				gp_log (GP_LOG_DEBUG, "ptp2/exif-tiff-reader", "entry %d, id 0x%04x, off 0x%x, cnt %d, type %d", i, id, off, cnt, type);
				switch (type) {
				case 1: case 2: case 6: case 7: size = 1; break;
				case 3: case 8: size = 2; break;
				case 4: case 9: case 11: size = 4; break;
				case 5: case 10: case 12: size = 8; break;
				default: /*FIXME */ break;
				}
				if (size * cnt > 4)
					off += cnt*size;
				else
					off  = 0;
				if (off > nextoff)
					nextoff = off;

			}
			nextifdoff = ximage[dircount*12+3] + (ximage[dircount*12+2] << 8) +
			            (ximage[dircount*12+1] << 16) + (ximage[dircount*12+0] << 24);
			gp_log (GP_LOG_DEBUG, "ptp2/exif-tiff-reader", "TIFF IFD0 is at off %d, next off is %d", ifd0offset, nextoff);
			free (ximage);
			return (GP_ERROR_NOT_SUPPORTED);
			break;
		}
		case PTP_OFC_EXIF_JPEG: {
			uint32_t xlen = 0;
			/* Note: Could also use Canon partial downloads */
			CPR (context, ptp_getpartialobject (params,
				params->handles.Handler[object_id],
				0, 10, &ximage, &xlen));

			if (!((ximage[0] == 0xff) && (ximage[1] == 0xd8))) {	/* SOI */
				free (ximage);
				return (GP_ERROR_NOT_SUPPORTED);
			}
			if (!((ximage[2] == 0xff) && (ximage[3] == 0xe1))) {	/* App0 */
				free (ximage);
				return (GP_ERROR_NOT_SUPPORTED);
			}
			if (0 != memcmp(ximage+6, "Exif", 4)) {
				free (ximage);
				return (GP_ERROR_NOT_SUPPORTED);
			}
			offset = 2;
			maxbytes = (ximage[4] << 8 ) + ximage[5];
			free (ximage);
			ximage = NULL;
			CPR (context, ptp_getpartialobject (params,
				params->handles.Handler[object_id],
				offset, maxbytes, &ximage, &xlen));
			CR (gp_file_set_data_and_size (file, (char*)ximage, xlen));
			break;
		}
		default:
			return (GP_ERROR_NOT_SUPPORTED);
		}
		break;
	}
	case	GP_FILE_TYPE_PREVIEW: {
		unsigned char *ximage = NULL;

		/* If thumb size is 0 then there is no thumbnail at all... */
		if((size=oi->ThumbCompressedSize)==0) return (GP_ERROR_NOT_SUPPORTED);
		CPR (context, ptp_getthumb(params,
			params->handles.Handler[object_id],
			&ximage));
		set_mimetype (camera, file, params->deviceinfo.VendorExtensionID, oi->ThumbFormat);
		CR (gp_file_set_data_and_size (file, (char*)ximage, size));
		break;
	}
	case	GP_FILE_TYPE_METADATA:
		if (is_mtp_capable (camera) &&
		    ptp_operation_issupported(params,PTP_OC_MTP_GetObjectPropsSupported)
		)
			return ptp_mtp_render_metadata (params,params->handles.Handler[object_id],oi->ObjectFormat,file);
		return (GP_ERROR_NOT_SUPPORTED);
	default: {
		/* We do not allow downloading unknown type files as in most
		cases they are special file (like firmware or control) which
		sometimes _cannot_ be downloaded. doing so we avoid errors.*/
		if (oi->ObjectFormat == PTP_OFC_Association ||
			(oi->ObjectFormat == PTP_OFC_Undefined &&
				((oi->ThumbFormat == PTP_OFC_Undefined) ||
				 (oi->ThumbFormat == 0)
			)
			)
		)
			return (GP_ERROR_NOT_SUPPORTED);

		if (is_mtp_capable (camera) &&
		    (oi->ObjectFormat == PTP_OFC_MTP_AbstractAudioVideoPlaylist))
			return mtp_get_playlist (camera, file, params->handles.Handler[object_id], context);

		size=oi->ObjectCompressedSize;
		if (size) {
			uint16_t	ret;
			PTPDataHandler	handler;

			ptp_init_camerafile_handler (&handler, file);
			ret = ptp_getobject_to_handler(params,
				params->handles.Handler[object_id],
				&handler
			);
			ptp_exit_camerafile_handler (&handler);
			if (ret == PTP_ERROR_CANCEL)
				return GP_ERROR_CANCEL;
			CPR(context, ret);
		} else {
			unsigned char *ximage = NULL;
			/* Do not download 0 sized files.
			 * It is not necessary and even breaks for some camera special files.
			 */
			ximage = malloc(1);
			CR (gp_file_set_data_and_size (file, (char*)ximage, size));
		}

		/* clear the "new" flag on Canons */
		if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
			(params->canon_flags) &&
			(params->canon_flags[object_id] & 0x2000) &&
			ptp_operation_issupported(params,PTP_OC_CANON_SetObjectArchive)
		) {
			/* seems just a byte (0x20 - new) */
			ptp_canon_setobjectarchive (params, params->handles.Handler[object_id], (params->canon_flags[object_id] &~0x2000)>>8);
			params->canon_flags[object_id] &= ~0x2000;
		}
		break;
	}
	}
	return set_mimetype (camera, file, params->deviceinfo.VendorExtensionID, oi->ObjectFormat);
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, CameraFile *file,
		void *data, GPContext *context)
{
	Camera *camera = data;
	PTPObjectInfo oi;
	const char *filename;
	uint32_t parent;
	uint32_t storage;
	uint32_t handle;
	unsigned long intsize;
	PTPParams* params = &camera->pl->params;
	CameraFileType	type;

	SET_CONTEXT_P(params, context);

	init_ptp_fs (camera, context);
	camera->pl->checkevents = TRUE;

	gp_file_get_name (file, &filename);
	gp_file_get_type (file, &type);
	gp_log ( GP_LOG_DEBUG, "ptp2/put_file_func", "folder=%s, filename=%s", folder, filename);

	if (!strcmp (folder, "/special")) {
		int i;

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
			int n;
			PTPObjectInfo *poi;

			/* compute storage ID value from folder patch */
			folder_to_storage(folder,storage);

			/* Get file number omiting storage pseudofolder */
			find_folder_handle(folder, storage, object_id, data);
			object_id = find_child(filename, storage, object_id, camera);
			if (object_id ==PTP_HANDLER_SPECIAL) {
				gp_context_error (context, _("File '%s/%s' does not exist."), folder, filename);
				return (GP_ERROR_BAD_PARAMETERS);
			}
			if ((n = handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL) {
				gp_context_error (context, _("File '%s/%s' does not exist."), folder, filename);
				return (GP_ERROR_BAD_PARAMETERS);
			}
			poi=&params->objectinfo[n];
			return ptp_mtp_parse_metadata (params,object_id,poi->ObjectFormat,file);
		}
		gp_context_error (context, _("Metadata only supported for MTP devices."));
		return GP_ERROR_NOT_SUPPORTED;
	}
	if (type != GP_FILE_TYPE_NORMAL)
		return GP_ERROR_BAD_PARAMETERS;
	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* get parent folder id omiting storage pseudofolder */
	find_folder_handle(folder,storage,parent,data);

	/* if you desire to put file to root folder, you have to use
	 * 0xffffffff instead of 0x00000000 (which means responder decide).
	 */
	if (parent==PTP_HANDLER_ROOT) parent=PTP_HANDLER_SPECIAL;

	/* We don't really want a file to exist with the same name twice. */
	handle = folder_to_handle (filename, storage, parent, camera);
	if (handle != PTP_HANDLER_SPECIAL) {
		gp_log ( GP_LOG_DEBUG, "ptp2/put_file_func", "%s/%s exists.", folder, filename);
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
		CPR (context, ptp_ek_sendfileobjectinfo (params, &storage,
			&parent, &handle, &oi));
		ptp_init_camerafile_handler (&handler, file);
		CPR (context, ptp_ek_sendfileobject_from_handler (params, &handler, intsize));
		ptp_exit_camerafile_handler (&handler);
	} else if (ptp_operation_issupported(params, PTP_OC_SendObjectInfo)) {
		uint16_t	ret;
		PTPDataHandler handler;

		CPR (context, ptp_sendobjectinfo (params, &storage,
			&parent, &handle, &oi));
		ptp_init_camerafile_handler (&handler, file);
		ret = ptp_sendobject_from_handler (params, &handler, intsize);
		ptp_exit_camerafile_handler (&handler);
		if (ret == PTP_ERROR_CANCEL)
			return (GP_ERROR_CANCEL);
		CPR (context, ret);
	} else {
		GP_DEBUG ("The device does not support uploading files!");
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
	uint32_t object_id;
	uint32_t storage;
	PTPParams *params = &camera->pl->params;

	SET_CONTEXT_P(params, context);

	if (!ptp_operation_issupported(params, PTP_OC_DeleteObject))
		return GP_ERROR_NOT_SUPPORTED;

	if (!strcmp (folder, "/special"))
		return GP_ERROR_NOT_SUPPORTED;

	init_ptp_fs (camera, context);
	/* virtual file created by Nikon special capture */
	if (	((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) ||
		 (params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON)   ) &&
		!strncmp (filename, "capt", 4)
	)
		return GP_OK;

	camera->pl->checkevents = TRUE;
	ptp_check_event (params);
	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(filename, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	CPR (context, ptp_deleteobject(params,
		params->handles.Handler[object_id],0));

	/* On some Canon firmwares, a DeleteObject causes a ObjectRemoved event
	 * to be sent. At least on Digital IXUS II and PowerShot A85. But
         * not on 350D.
	 */
	if (DELETE_SENDS_EVENT(camera->pl) &&
	    ptp_event_issupported(params, PTP_EC_ObjectRemoved)) {
		PTPContainer event;

		ptp_check_event (params); /* ignore errors */
		while (ptp_get_one_event (params, &event))
			if (event.Code == PTP_EC_ObjectRemoved)
				break;
 	}
	return (GP_OK);
}

static int
remove_dir_func (CameraFilesystem *fs, const char *folder,
			const char *foldername, void *data, GPContext *context)
{
	Camera *camera = data;
	uint32_t object_id;
	uint32_t storage;
	PTPParams *params = &camera->pl->params;

	SET_CONTEXT_P(params, context);

	if (!ptp_operation_issupported(params, PTP_OC_DeleteObject))
		return GP_ERROR_NOT_SUPPORTED;
	camera->pl->checkevents = TRUE;

	init_ptp_fs (camera, context);
	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(foldername, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);
	CPR (context, ptp_deleteobject(params, params->handles.Handler[object_id],0));
	return (GP_OK);
}

static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo info, void *data, GPContext *context)
{
	Camera *camera = data;
	PTPObjectInfo *oi;
	uint32_t object_id, n;
	uint32_t storage;
	PTPParams *params = &camera->pl->params;

	SET_CONTEXT_P(params, context);

	if (!strcmp (folder, "/special"))
		return (GP_ERROR_BAD_PARAMETERS);
	camera->pl->checkevents = TRUE;

	init_ptp_fs (camera, context);
	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(filename, storage, object_id, camera);
	if ((n=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	oi=&params->objectinfo[n];

	if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
		uint16_t	ret, newprot;

		if ((info.file.permissions & GP_FILE_PERM_DELETE) != GP_FILE_PERM_DELETE)
			newprot = PTP_PS_ReadOnly;
		else
			newprot = PTP_PS_NoProtection;
		if (oi->ProtectionStatus != newprot) {
			if (!ptp_operation_issupported(params, PTP_OC_SetObjectProtection)) {
				gp_context_error (context, _("Device does not support setting object protection."));
				return (GP_ERROR_NOT_SUPPORTED);
			}
			ret = ptp_setobjectprotection (params, object_id, newprot);
			if (ret != PTP_RC_OK) {
				gp_context_error (context, _("Device failed to set object protection to %d, error 0x%04x."), newprot, ret);
				return (GP_ERROR_NOT_SUPPORTED);
			}
			oi->ProtectionStatus = newprot; /* should actually reread objectinfo to be sure, but lets not. */
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
	PTPObjectInfo *oi;
	uint32_t object_id;
	uint32_t storage;
	PTPParams *params = &camera->pl->params;

	SET_CONTEXT_P(params, context);

	if (!strcmp (folder, "/special"))
		return (GP_ERROR_BAD_PARAMETERS); /* for now */

	init_ptp_fs (camera, context);
	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(filename, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	oi=&params->objectinfo[object_id];

	info->file.fields = GP_FILE_INFO_SIZE|GP_FILE_INFO_TYPE|GP_FILE_INFO_MTIME;
	info->file.size   = oi->ObjectCompressedSize;

	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) && params->canon_flags) {
		info->file.fields |= GP_FILE_INFO_STATUS;
		if (params->canon_flags[object_id] & 0x2000)
			info->file.status = GP_FILE_STATUS_NOT_DOWNLOADED;
		else
			info->file.status = GP_FILE_STATUS_DOWNLOADED;
	}

	/* MTP playlists have their own size calculation */
	if (is_mtp_capable (camera) &&
	    (oi->ObjectFormat == PTP_OFC_MTP_AbstractAudioVideoPlaylist)) {
		int ret, contentlen;
		ret = mtp_get_playlist_string (camera, params->handles.Handler[object_id], NULL, &contentlen);
		if (ret != GP_OK) return ret;
		info->file.size = contentlen;
	}

	strcpy_mime (info->file.type, params->deviceinfo.VendorExtensionID, oi->ObjectFormat);
	if (oi->ModificationDate != 0) {
		info->file.mtime = oi->ModificationDate;
	} else {
		info->file.mtime = oi->CaptureDate;
	}

	switch (oi->ProtectionStatus) {
	case PTP_PS_NoProtection:
		info->file.fields	|= GP_FILE_INFO_PERMISSIONS;
		info->file.permissions	 = GP_FILE_PERM_READ|GP_FILE_PERM_DELETE;
		break;
	case PTP_PS_ReadOnly:
		info->file.fields	|= GP_FILE_INFO_PERMISSIONS;
		info->file.permissions	 = GP_FILE_PERM_READ;
		break;
	default:
		gp_log (GP_LOG_ERROR, "ptp2/get_info_func", "mapping protection to gp perm failed, prot is %x", oi->ProtectionStatus);
		break;
	}

	/* if object is an image */
	if ((oi->ObjectFormat & 0x0800) != 0) {
		info->preview.fields = 0;
		strcpy_mime(info->preview.type, params->deviceinfo.VendorExtensionID, oi->ThumbFormat);
		if (strlen(info->preview.type)) {
			info->preview.fields |= GP_FILE_INFO_TYPE;
		}
		if (oi->ThumbCompressedSize) {
			info->preview.size   = oi->ThumbCompressedSize;
			info->preview.fields |= GP_FILE_INFO_SIZE;
		}
		if (oi->ThumbPixWidth) {
			info->preview.width  = oi->ThumbPixWidth;
			info->preview.fields |= GP_FILE_INFO_WIDTH;
		}
		if (oi->ThumbPixHeight) {
			info->preview.height  = oi->ThumbPixHeight;
			info->preview.fields |= GP_FILE_INFO_HEIGHT;
		}
		if (oi->ImagePixWidth) {
			info->file.width  = oi->ImagePixWidth;
			info->file.fields |= GP_FILE_INFO_WIDTH;
		}
		if (oi->ImagePixHeight) {
			info->file.height  = oi->ImagePixHeight;
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

	init_ptp_fs (camera, context);
	memset(&oi, 0, sizeof (PTPObjectInfo));

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* get parent folder id omiting storage pseudofolder */
	find_folder_handle(folder,storage,parent,data);

	/* if you desire to make dir in 'root' folder, you have to use
	 * 0xffffffff instead of 0x00000000 (which means responder decide).
	 */
	if (parent==PTP_HANDLER_ROOT) parent=PTP_HANDLER_SPECIAL;

	handle = folder_to_handle (foldername, storage, parent, camera);
	if (handle != PTP_HANDLER_SPECIAL) {
		return GP_ERROR_DIRECTORY_EXISTS;
	}

	oi.Filename=(char *)foldername;

	oi.ObjectFormat=PTP_OFC_Association;
	oi.ProtectionStatus=PTP_PS_NoProtection;
	oi.AssociationType=PTP_AT_GenericFolder;

	if ((params->deviceinfo.VendorExtensionID==
			PTP_VENDOR_EASTMAN_KODAK) &&
		(ptp_operation_issupported(params,
			PTP_OC_EK_SendFileObjectInfo)))
	{
		CPR (context, ptp_ek_sendfileobjectinfo (params, &storage,
			&parent, &handle, &oi));
	} else if (ptp_operation_issupported(params, PTP_OC_SendObjectInfo)) {
		CPR (context, ptp_sendobjectinfo (params, &storage,
			&parent, &handle, &oi));
	} else {
		GP_DEBUG ("The device does not support make folder!");
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
	int			i,n;
	uint16_t		ret;
	CameraStorageInformation*sif;

	if (!ptp_operation_issupported (params, PTP_OC_GetStorageIDs))
		return (GP_ERROR_NOT_SUPPORTED);

	SET_CONTEXT_P(params, context);
	ret = ptp_getstorageids (params, &sids);
	if (ret != PTP_RC_OK)
		return translate_ptp_result (ret);
	n = 0;
	*sinfos = (CameraStorageInformation*)
		calloc (sizeof (CameraStorageInformation),sids.n);
	for (i = 0; i<sids.n; i++) {
		sif = (*sinfos)+n;

		/* Invalid storage, storageinfo might cause hangs on it (Nikon D300s e.g.) */
		if ((sids.Storage[i]&0x0000ffff)==0) continue;

		ret = ptp_getstorageinfo (params, sids.Storage[i], &si);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_ERROR, "ptp2/storage_info_func", "ptp getstorageinfo failed: 0x%x", ret);
			return translate_ptp_result (ret);
		}
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
			gp_log (GP_LOG_DEBUG, "ptp2/storage_info_func", "unknown storagetype 0x%x", si.StorageType);
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
			gp_log (GP_LOG_DEBUG, "ptp2/storage_info_func", "unknown accesstype 0x%x", si.AccessCapability);
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
		sif->fields |= GP_STORAGEINFO_MAXCAPACITY;
		sif->capacitykbytes = si.MaxCapability / 1024;
		sif->fields |= GP_STORAGEINFO_FREESPACEKBYTES;
		sif->freekbytes = si.FreeSpaceInBytes / 1024;
		if (si.FreeSpaceInImages != -1) {
			sif->fields |= GP_STORAGEINFO_FREESPACEIMAGES;
			sif->freeimages = si.FreeSpaceInImages;
		}
		if (si.StorageDescription) free (si.StorageDescription);
		if (si.VolumeLabel) free (si.VolumeLabel);

		n++;
	}
	free (sids.Storage);
	*nrofsinfos = n;
	return (GP_OK);
}

static void
debug_objectinfo(PTPParams *params, uint32_t oid, PTPObjectInfo *oi) {
	GP_DEBUG ("ObjectInfo for '%s':", oi->Filename);
	GP_DEBUG ("  Object ID: 0x%08x", oid);
	GP_DEBUG ("  StorageID: 0x%08x", oi->StorageID);
	GP_DEBUG ("  ObjectFormat: 0x%04x", oi->ObjectFormat);
	GP_DEBUG ("  ProtectionStatus: 0x%04x", oi->ProtectionStatus);
	GP_DEBUG ("  ObjectCompressedSize: %d", oi->ObjectCompressedSize);
	GP_DEBUG ("  ThumbFormat: 0x%04x", oi->ThumbFormat);
	GP_DEBUG ("  ThumbCompressedSize: %d", oi->ThumbCompressedSize);
	GP_DEBUG ("  ThumbPixWidth: %d", oi->ThumbPixWidth);
	GP_DEBUG ("  ThumbPixHeight: %d", oi->ThumbPixHeight);
	GP_DEBUG ("  ImagePixWidth: %d", oi->ImagePixWidth);
	GP_DEBUG ("  ImagePixHeight: %d", oi->ImagePixHeight);
	GP_DEBUG ("  ImageBitDepth: %d", oi->ImageBitDepth);
	GP_DEBUG ("  ParentObject: 0x%08x", oi->ParentObject);
	GP_DEBUG ("  AssociationType: 0x%04x", oi->AssociationType);
	GP_DEBUG ("  AssociationDesc: 0x%08x", oi->AssociationDesc);
	GP_DEBUG ("  SequenceNumber: 0x%08x", oi->SequenceNumber);
	GP_DEBUG ("  ModificationDate: 0x%08x", (unsigned int)oi->ModificationDate);
	GP_DEBUG ("  CaptureDate: 0x%08x", (unsigned int)oi->CaptureDate);
}

int
init_ptp_fs (Camera *camera, GPContext *context)
{
	int i, id, nroot = 0;
	PTPParams *params = &camera->pl->params;
	char buf[1024];
	uint16_t ret;

	SET_CONTEXT_P(params, context);
	if (camera->pl->fs_loaded) return PTP_RC_OK;
	camera->pl->fs_loaded = 1;

	memset (&params->handles, 0, sizeof(PTPObjectHandles));

	/* Nikon supports a fast filesystem retrieval.
	 * Unfortunately this function returns a flat folder structure
	 * which cannot be changed to represent the actual FAT layout.
	 * So if you need to get access to _all_ files on the ptp fs,
	 * you can change the setting to "false" (gphoto2 --config or
	 * edit ~/.gphoto2/settings directly).
 	 * Since the fast fs mode irritates capture, I changed it to default off.
	 */

	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
	    (ptp_operation_issupported(params, PTP_OC_NIKON_GetFileInfoInBlock)) &&
	    (camera->port->type == GP_PORT_USB) &&
	    ((GP_OK == gp_setting_get("ptp2","nikon.fastfilesystem",buf)) && atoi(buf))
        )
	{
		unsigned char	*data,*curptr;
		unsigned int	size;
		int		i,guessedcnt,curhandle;
		uint32_t	generatedoid = 0x42420000;
		uint32_t	rootoid = generatedoid++;
		int		roothandle = -1;
		uint16_t	res;
		PTPStorageIDs	ids;

		/* To get the correct storage id for all the objects */
		res = ptp_getstorageids (params, &ids);
		if (res != PTP_RC_OK) goto fallback;
		if (ids.n != 1) { /* can't cope with this currently */
			gp_log (GP_LOG_DEBUG, "ptp", "more than 1 storage id present");
			free(ids.Storage);
			goto fallback;
		}
		res = ptp_nikon_getfileinfoinblock(params, 1, 0xffffffff, 0xffffffff, &data, &size);
		if (res != PTP_RC_OK) {
			gp_log (GP_LOG_DEBUG, "ptp", "getfileinfoblock failed");
			free(ids.Storage);
			goto fallback;
		}
		curptr = data;
		if (*curptr != 0x01) { /* version of data format */
			gp_log (GP_LOG_DEBUG, "ptp", "version is 0x%02x, expected 0x01", *curptr);
			free(ids.Storage);
			free(data);
			goto fallback;
		}
		guessedcnt = size/8; /* wild guess ... 4 byte type, at least 2 chars name, 2 more bytes */
		params->handles.Handler = malloc(sizeof(params->handles.Handler[0])*guessedcnt);
		memset(params->handles.Handler,0,sizeof(params->handles.Handler[0])*guessedcnt);
		params->objectinfo = (PTPObjectInfo*)malloc(sizeof(PTPObjectInfo)*guessedcnt);
		memset(params->objectinfo,0,sizeof(PTPObjectInfo)*guessedcnt);
		curhandle=0;
		curptr++;

		/* This ptp command does not get a ready made directory structure, it
		 * gets a list of folders (flat) and its image related file contents.
		 * It does not get AUTPRNT.MRK for instance...
		 * It is however very fast since it is just one ptp command roundtrip.
		 */
		while (curptr-data < size) { /* loops over folders */
			int numents, namelen, dirhandle;
			uint32_t	diroid = generatedoid++;

			namelen = curptr[0]+(curptr[1]<<8)+(curptr[2]<<16)+(curptr[3]<<24);
			curptr+=4;
			if (!strcmp((char*)curptr,"DCIM")) {
				/* to generated the /DCIM/NNNABCDEF/ structure, handle /DCIM/
				 * differently */
				diroid = rootoid;
				roothandle = curhandle;
				params->handles.Handler[curhandle] = rootoid;
				params->objectinfo[curhandle].ParentObject = 0;
				nroot = 1;
			} else {
				if (roothandle == -1) { /* We must synthesize /DCIM... */
					roothandle = curhandle;
					params->handles.Handler[curhandle] = rootoid;
					params->objectinfo[curhandle].ParentObject = 0;
					params->objectinfo[curhandle].StorageID = ids.Storage[0];
					params->objectinfo[curhandle].Filename = strdup("DCIM");
					params->objectinfo[curhandle].ObjectFormat = PTP_OFC_Association;
					params->objectinfo[curhandle].AssociationType = PTP_AT_GenericFolder;
					curhandle++;
				}
				params->handles.Handler[curhandle] = diroid;
				params->objectinfo[curhandle].ParentObject = rootoid;
			}
			params->objectinfo[curhandle].ObjectFormat = PTP_OFC_Association;
			params->objectinfo[curhandle].AssociationType = PTP_AT_GenericFolder;
			params->objectinfo[curhandle].StorageID = ids.Storage[0];
			params->objectinfo[curhandle].Filename = strdup((char*)curptr);

			while (*curptr) curptr++; curptr++;
			numents = curptr[0]+(curptr[1]<<8); curptr+=2;
			dirhandle = curhandle;
			curhandle++;
			for (i=0;i<numents;i++) {
				uint32_t oid, size, xtime;

				oid = curptr[0]+(curptr[1]<<8)+(curptr[2]<<16)+(curptr[3]<<24);
				curptr += 4;
				namelen = curptr[0]+(curptr[1]<<8)+(curptr[2]<<16)+(curptr[3]<<24);
				curptr += 4;
				params->handles.Handler[curhandle] = oid;
				params->objectinfo[curhandle].StorageID = ids.Storage[0];
				params->objectinfo[curhandle].Filename = strdup((char*)curptr);
				params->objectinfo[curhandle].ObjectFormat = PTP_OFC_Undefined;
				if (NULL!=strstr((char*)curptr,".JPG"))
					params->objectinfo[curhandle].ObjectFormat = PTP_OFC_EXIF_JPEG;
				if (NULL!=strstr((char*)curptr,".MOV"))
					params->objectinfo[curhandle].ObjectFormat = PTP_OFC_QT;
				if (NULL!=strstr((char*)curptr,".AVI"))
					params->objectinfo[curhandle].ObjectFormat = PTP_OFC_AVI;
				if (NULL!=strstr((char*)curptr,".WAV"))
					params->objectinfo[curhandle].ObjectFormat = PTP_OFC_WAV;
				while (*curptr) curptr++; curptr++;
				size = curptr[0]+(curptr[1]<<8)+(curptr[2]<<16)+(curptr[3]<<24);
				params->objectinfo[curhandle].ObjectCompressedSize = size;
				curptr += 4;
				xtime = curptr[0]+(curptr[1]<<8)+(curptr[2]<<16)+(curptr[3]<<24);
				if (xtime > 0x12cea600) /* Unknown files are 1.1.1980 */
					params->objectinfo[curhandle].CaptureDate = xtime;
				curptr += 4;
				/* Hack ... to find our directory oid, we just getobjectinfo
				 * the first file object.
				 */
				if (0 && !i) {
					ptp_getobjectinfo(params, oid, &params->objectinfo[curhandle]);
					diroid = params->objectinfo[curhandle].ParentObject;
					params->handles.Handler[dirhandle] = diroid;
					if ((params->objectinfo[dirhandle].ParentObject & 0xffff0000) == 0x42420000) {
						if (roothandle >= 0) {
							ptp_getobjectinfo(params, diroid, &params->objectinfo[dirhandle]);
							rootoid = params->objectinfo[dirhandle].ParentObject;
							params->handles.Handler[roothandle] = rootoid;
						}
					}
				}
				params->objectinfo[curhandle].ParentObject = diroid;
				curhandle++;
			}
		}
		free (ids.Storage);
		params->handles.n = curhandle;
		return PTP_RC_OK;
	}

#if 0
	/* CANON also has fast directory retrieval. And it is mostly complete, so we can use it as full replacement */
	/* Unfortunately this fails on the PowerShot A430. 
	 * And I don't want to whitelist everyone, because I just don't own all of them.
	 * *sigh* -Marcus */
	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
	    ptp_operation_issupported(params,PTP_OC_CANON_GetDirectory))

	{
		PTPObjectInfo	*oinfos = NULL;	
		uint32_t	*flags = NULL;	

		ret = ptp_canon_get_directory (params, &params->handles, &oinfos, &flags);
		if ((ret == PTP_RC_OK) && params->handles.n) {
			params->objectinfo = oinfos;
			params->canon_flags = flags;
			return PTP_RC_OK;
		}
		if (oinfos) free (oinfos);
		if (flags) free (flags);
		/* fallthrough */
	}
#endif

	/* Microsoft/MTP also has fast directory retrieval. */
	if (is_mtp_capable (camera) &&
	    ptp_operation_issupported(params,PTP_OC_MTP_GetObjPropList) &&
	    (camera->pl->bugs & PTP_MTP_PROPLIST_WORKS)
	) {
		PTPObjectInfo	*oinfos = NULL;	
		int		cnt = 0, i, j, nrofprops = 0;
		uint32_t	lasthandle = 0xffffffff;
		MTPProperties 	*props = NULL, *xpl;
                int             oldtimeout;

                /* The follow request causes the device to generate
                 * a list of very file on the device and return it
                 * in a single response.
                 *
                 * Some slow device as well as devices with very
                 * large file systems can easily take longer then
                 * the standard timeout value before it is able
                 * to return a response.
                 *
                 * Temporarly set timeout to allow working with
                 * widest range of devices.
                 */
                gp_port_get_timeout (camera->port, &oldtimeout);
                gp_port_set_timeout (camera->port, 60000);

		ret = ptp_mtp_getobjectproplist (params, 0xffffffff, &props, &nrofprops);
                gp_port_set_timeout (camera->port, oldtimeout);

		if (ret != PTP_RC_OK)
			goto fallback;
		params->props = props; /* cache it */
		params->nrofprops = nrofprops; /* cache it */

		/* count the objects */
		for (i=0;i<nrofprops;i++) {
			xpl = &props[i];
			if (lasthandle != xpl->ObjectHandle) {
				cnt++;
				lasthandle = xpl->ObjectHandle;
			}
		}
		lasthandle = 0xffffffff;
		oinfos = params->objectinfo = malloc (sizeof (PTPObjectInfo) * cnt);
		memset (oinfos ,0 ,sizeof (PTPObjectInfo) * cnt);
		params->handles.Handler = malloc (sizeof (uint32_t) * cnt);
		params->handles.n = cnt;

		i = -1;
		for (j=0;j<nrofprops;j++) {
			xpl = &props[j];
			if (lasthandle != xpl->ObjectHandle) {
				if (i >= 0) {
					if (!oinfos[i].Filename) {
						/* i have one such file on my Creative */
						oinfos[i].Filename = strdup("<null>");
					}
				}
				i++;
				lasthandle = xpl->ObjectHandle;
				params->handles.Handler[i] = xpl->ObjectHandle;
				gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "objectid 0x%x", xpl->ObjectHandle);
			}
			switch (xpl->property) {
			case PTP_OPC_ParentObject:
				if (xpl->datatype != PTP_DTC_UINT32) {
					gp_log (GP_LOG_ERROR, "ptp2/mtpfast", "parentobject has type 0x%x???", xpl->datatype);
					break;
				}
				oinfos[i].ParentObject = xpl->propval.u32;
				if (xpl->propval.u32 == 0)
					nroot++;
				gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "parent 0x%x", xpl->propval.u32);
				break;
			case PTP_OPC_ObjectFormat:
				if (xpl->datatype != PTP_DTC_UINT16) {
					gp_log (GP_LOG_ERROR, "ptp2/mtpfast", "objectformat has type 0x%x???", xpl->datatype);
					break;
				}
				oinfos[i].ObjectFormat = xpl->propval.u16;
				gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "ofc 0x%x", xpl->propval.u16);
				break;
			case PTP_OPC_ObjectSize:
				switch (xpl->datatype) {
				case PTP_DTC_UINT32:
					oinfos[i].ObjectCompressedSize = xpl->propval.u32;
					break;
				case PTP_DTC_UINT64:
					oinfos[i].ObjectCompressedSize = xpl->propval.u64;
					break;
				default:
					gp_log (GP_LOG_ERROR, "ptp2/mtpfast", "objectsize has type 0x%x???", xpl->datatype);
					break;
				}
				gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "objectsize %u", xpl->propval.u32);
				break;
			case PTP_OPC_StorageID:
				if (xpl->datatype != PTP_DTC_UINT32) {
					gp_log (GP_LOG_ERROR, "ptp2/mtpfast", "storageid has type 0x%x???", xpl->datatype);
					break;
				}
				oinfos[i].StorageID = xpl->propval.u32;
				gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "storageid 0x%x", xpl->propval.u32);
				break;
			case PTP_OPC_ProtectionStatus:/*UINT16*/
				if (xpl->datatype != PTP_DTC_UINT16) {
					gp_log (GP_LOG_ERROR, "ptp2/mtpfast", "protectionstatus has type 0x%x???", xpl->datatype);
					break;
				}
				oinfos[i].ProtectionStatus = xpl->propval.u16;
				gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "protection 0x%x", xpl->propval.u16);
				break;
			case PTP_OPC_ObjectFileName:
				if (xpl->datatype != PTP_DTC_STR) {
					gp_log (GP_LOG_ERROR, "ptp2/mtpfast", "filename has type 0x%x???", xpl->datatype);
					break;
				}
				if (xpl->propval.str) {
					gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "filename %s", xpl->propval.str);
					oinfos[i].Filename = strdup(xpl->propval.str);
				} else {
					oinfos[i].Filename = NULL;
				}
				break;
			case PTP_OPC_DateCreated:
				if (xpl->datatype != PTP_DTC_STR) {
					gp_log (GP_LOG_ERROR, "ptp2/mtpfast", "datecreated has type 0x%x???", xpl->datatype);
					break;
				}
				gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "capturedate %s", xpl->propval.str);
				oinfos[i].CaptureDate = ptp_unpack_PTPTIME (xpl->propval.str);
				break;
			case PTP_OPC_DateModified:
				if (xpl->datatype != PTP_DTC_STR) {
					gp_log (GP_LOG_ERROR, "ptp2/mtpfast", "datemodified has type 0x%x???", xpl->datatype);
					break;
				}
				gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "moddate %s", xpl->propval.str);
				oinfos[i].ModificationDate = ptp_unpack_PTPTIME (xpl->propval.str);
				break;
			default:
				if ((xpl->property & 0xfff0) == 0xdc00)
					gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "case %x type %x unhandled", xpl->property, xpl->datatype);
				break;
			}
		}
		return PTP_RC_OK;
	}

fallback:
	/* Get file handles array for filesystem */
	id = gp_context_progress_start (context, 100, _("Initializing Camera"));
	/* Get objecthandles of all objects from all stores */
	CPR (context, ptp_getobjecthandles (params, 0xffffffff, 0x000000, 0x000000, &params->handles));
	gp_context_progress_update (context, id, 10);
	/* wee need that for fileststem */
	params->objectinfo = (PTPObjectInfo*)malloc(sizeof(PTPObjectInfo)*
		params->handles.n);
	memset (params->objectinfo,0,sizeof(PTPObjectInfo)*params->handles.n);

	for (i = 0; i < params->handles.n; i++) {
		uint16_t ret;

		ret = ptp_getobjectinfo(params,
			params->handles.Handler[i],
			&params->objectinfo[i]);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_ERROR, "ptp2/std_getobjectinfo", "received error 0x%04x on query of 0x%08x", ret, params->handles.Handler[i]);
			if (i < params->handles.n-1)
				memmove(&params->handles.Handler[i],&params->handles.Handler[i+1],(params->handles.n-i-1)*sizeof(params->handles.Handler[i]));
			params->handles.n--;
			i--; /* so we get the same entry again ... */
			continue;
		}
#if 1
		debug_objectinfo(params, params->handles.Handler[i], &params->objectinfo[i]);
#endif
		if (params->objectinfo[i].ParentObject == 0)
			nroot++;

                if (	!params->objectinfo[i].Filename ||
			!strlen (params->objectinfo[i].Filename)
		) {
			params->objectinfo[i].Filename = malloc(8+1);
			sprintf (params->objectinfo[i].Filename, "%08x", params->handles.Handler[i]);
			gp_log (GP_LOG_ERROR, "ptp2/std_getobjectinfo", "Replaced empty dirname by '%08x'", params->handles.Handler[i]);
		}

		gp_context_progress_update (context, id,
		10+(90*i)/params->handles.n);
	}
	gp_context_progress_stop (context, id);

	/* for older Canons we now retrieve their object flags, to allow
	 * "new" image handling. This is not yet a substitute for regular
	 * OI retrieval.
	 */
	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
	    ptp_operation_issupported(params,PTP_OC_CANON_GetObjectInfoEx)) {
		uint16_t ret;
		int i;

		params->canon_flags = calloc(sizeof(params->canon_flags[0]),params->handles.n);
		/* Look for all directories, since this function apparently only
		 * returns a directory full of entries and does not recurse
		 */
		for (i=0;i<params->handles.n;i++) {
			int j;
			PTPCANONFolderEntry	*ents = NULL;
			uint32_t		numents = 0;

			/* only retrieve for directories */
			if (!params->objectinfo[i].AssociationType)
				continue;

			ret = ptp_canon_getobjectinfo(params,
				params->objectinfo[i].StorageID,0,
				params->handles.Handler[i],0,
				&ents,&numents
			);
			if (ret != PTP_RC_OK) continue;
			for (j=0;j<numents;j++) {
				int k;
				for (k=0;k<params->handles.n;k++)
					if (params->handles.Handler[k] == ents[j].ObjectHandle)
						break;
				if (k == params->handles.n)
					continue;
				params->canon_flags[k] = ents[j].Flags << 8;
			}
		}
	}

	/* If there are no root directory objects, look for "DCIM" directories.
	 * This way, we can handle cameras that report the wrong ParentObject ID for
	 * root.
	 *
	 * FIXME: If DCIM is there, it will not look for other root directories.
         */
	if (nroot == 0 && params->handles.n > 0) {
		uint32_t	badroothandle = 0xffffffff;

		GP_DEBUG("Bug workaround: Found no root directory objects, looking for some.");
		for (i = 0; i < params->handles.n; i++) {
			PTPObjectInfo *oi = &params->objectinfo[i];

			if (strcmp(oi->Filename, "DCIM") == 0) {
				GP_DEBUG("Changing DCIM ParentObject ID from 0x%x to 0",
					 oi->ParentObject);
				badroothandle = oi->ParentObject;
				oi->ParentObject = 0;
				nroot++;
			}
		}
		for (i = 0; i < params->handles.n; i++) {
			PTPObjectInfo *oi = &params->objectinfo[i];
			if (oi->ParentObject == badroothandle) {
				GP_DEBUG("Changing %s ParentObject ID from 0x%x to 0",
					oi->Filename, oi->ParentObject);
				oi->ParentObject = 0;
				nroot++;
			}
		}
		/* Some cameras do not have a directory at all, just files or unattached
		 * directories. In this case associate all unattached to the 0 object.
		 *
		 * O(n^2) search. Be careful.
		 */
		if (nroot == 0) {
			GP_DEBUG("Bug workaround: Found no root dir entries and no DCIM dir, looking for some.");
			/* look for entries with parentobjects that do not exist */
			for (i = 0; i < params->handles.n; i++) {
				int j;
				PTPObjectInfo *oi = &params->objectinfo[i];

				for (j = 0;j < params->handles.n; j++)
					if (oi->ParentObject == params->handles.Handler[j])
						break;
				if (j == params->handles.n)
					oi->ParentObject = 0;
			}
		}
	}
	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func		= file_list_func,
	.folder_list_func	= folder_list_func,
	.get_info_func		= get_info_func,
	.set_info_func		= set_info_func,
	.get_file_func		= get_file_func,
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
	int ret, i, retried = 0;
	PTPParams *params;
	char *curloc, *camloc;
	GPPortSettings	settings;
	uint32_t	sessionid;
	char		buf[20];
	int 		start_timeout = USB_START_TIMEOUT;
	int 		canon_start_timeout = USB_CANON_START_TIMEOUT;

	gp_port_get_settings (camera->port, &settings);
	/* Make sure our port is either USB or PTP/IP. */
	if ((camera->port->type != GP_PORT_USB) && (camera->port->type != GP_PORT_PTPIP)) {
		gp_context_error (context, _("Currently, PTP is only implemented for "
			"USB and PTP/IP cameras currently, port type %x"), camera->port->type);
		return (GP_ERROR_UNKNOWN_PORT);
	}

	camera->functions->about = camera_about;
	camera->functions->exit = camera_exit;
	camera->functions->capture = camera_capture;
	camera->functions->capture_preview = camera_capture_preview;
	camera->functions->summary = camera_summary;
	camera->functions->get_config = camera_get_config;
	camera->functions->set_config = camera_set_config;
	camera->functions->wait_for_event = camera_wait_for_event;

	/* We need some data that we pass around */
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
	params = &camera->pl->params;
	params->debug_func = ptp_debug_func;
	params->error_func = ptp_error_func;
	params->data = malloc (sizeof (PTPData));
	memset (params->data, 0, sizeof (PTPData));
	((PTPData *) params->data)->camera = camera;
	params->byteorder = PTP_DL_LE;
	if (params->byteorder == PTP_DL_LE)
		camloc = "UCS-2LE";
	else
		camloc = "UCS-2BE";


	switch (camera->port->type) {
	case GP_PORT_USB:
		params->sendreq_func		= ptp_usb_sendreq;
		params->senddata_func	= ptp_usb_senddata;
		params->getresp_func		= ptp_usb_getresp;
		params->getdata_func		= ptp_usb_getdata;
		params->event_wait		= ptp_usb_event_wait;
		params->event_check		= ptp_usb_event_check;
		params->cancelreq_func	= ptp_usb_control_cancel_request;
		params->maxpacketsize 	= settings.usb.maxpacketsize;
		gp_log (GP_LOG_DEBUG, "ptp2", "maxpacketsize %d", settings.usb.maxpacketsize);
		break;
	case GP_PORT_PTPIP: {
		GPPortInfo	info;

		ret = gp_port_get_info (camera->port, &info);
		if (ret != GP_OK) {
			gp_log (GP_LOG_ERROR, "ptpip", "Failed to get port info?\n");
			return ret;
		}
		ret = ptp_ptpip_connect (params, info.path);
		if (ret != GP_OK) {
			gp_log (GP_LOG_ERROR, "ptpip", "Failed to connect.\n");
			return ret;
		}
		params->sendreq_func		= ptp_ptpip_sendreq;
		params->senddata_func	= ptp_ptpip_senddata;
		params->getresp_func		= ptp_ptpip_getresp;
		params->getdata_func		= ptp_ptpip_getdata;
		params->event_wait		= ptp_ptpip_event_wait;
		params->event_check		= ptp_ptpip_event_check;
		break;
	}
	default:
		break;
	}
	if (!params->maxpacketsize)
		params->maxpacketsize = 64; /* assume USB 1.0 */
#ifdef HAVE_ICONV
	curloc = nl_langinfo (CODESET);
	if (!curloc) curloc="UTF-8";
	params->cd_ucs2_to_locale = iconv_open(curloc, camloc);
	params->cd_locale_to_ucs2 = iconv_open(camloc, curloc);
	if ((params->cd_ucs2_to_locale == (iconv_t) -1) ||
	    (params->cd_locale_to_ucs2 == (iconv_t) -1)) {
		gp_log (GP_LOG_ERROR, "iconv", "Failed to create iconv converter.");
		return (GP_ERROR_OS_FAILURE);
	}
#endif
        gp_camera_get_abilities(camera, &a);
        for (i = 0; i<sizeof(models)/sizeof(models[0]); i++) {
            if ((a.usb_vendor == models[i].usb_vendor) &&
                (a.usb_product == models[i].usb_product)){
                camera->pl->bugs = models[i].known_bugs;
                break;
            }
        }
	/* map the libmtp flags to ours. Currently its just 1 flag. */
        for (i = 0; i<sizeof(mtp_models)/sizeof(mtp_models[0]); i++) {
            if ((a.usb_vendor == mtp_models[i].usb_vendor) &&
                (a.usb_product == mtp_models[i].usb_product)){
                	camera->pl->bugs = PTP_MTP;

		/* some players really need it */
		if (!(mtp_models[i].flags & DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL))
			camera->pl->bugs |= PTP_MTP_PROPLIST_WORKS;
		if (mtp_models[i].flags & DEVICE_FLAG_IGNORE_HEADER_ERRORS)
			camera->pl->bugs |= PTP_MTP_ZEN_BROKEN_HEADER;
                break;
            }
        }

	/* Read configurable timeouts */
#define XT(val,def) 					\
	if ((GP_OK == gp_setting_get("ptp2",#val,buf)))	\
		sscanf(buf, "%d", &val);		\
	if (!val) val = def;
	XT(normal_timeout,USB_NORMAL_TIMEOUT);
	XT(capture_timeout,USB_TIMEOUT_CAPTURE);

	/* Choose a shorter timeout on inital setup to avoid
	 * having the user wait too long.
	 */
	if (a.usb_vendor == 0x4a9) { /* CANON */
		/* our special canon friends get a shorter timeout, sinc ethey
		 * occasionaly need 2 retries. */
		XT(canon_start_timeout,USB_CANON_START_TIMEOUT);
		CR (gp_port_set_timeout (camera->port, canon_start_timeout));
	} else {
		XT(start_timeout,USB_START_TIMEOUT);
		CR (gp_port_set_timeout (camera->port, start_timeout));
	}
#undef XT
	/* Establish a connection to the camera */
	SET_CONTEXT(camera, context);

	retried = 0;
	sessionid = 1;
	while (1) {
		ret=ptp_opensession (params, sessionid);
		while (ret==PTP_RC_InvalidTransactionID) {
			sessionid++;
			if (retried < 10) {
				retried++;
				continue;
			}
			/* FIXME: deviceinfo is not read yet ... */
			report_result(context, ret, params->deviceinfo.VendorExtensionID);
			return translate_ptp_result (ret);
		}
		if (ret!=PTP_RC_SessionAlreadyOpened && ret!=PTP_RC_OK) {
			gp_log (GP_LOG_ERROR, "ptp2/camera_init", "ptp_opensession returns %x", ret);
			if ((ret == PTP_ERROR_RESP_EXPECTED) || (ret == PTP_ERROR_IO)) {
				/* Try whacking PTP device */
				if (camera->port->type == GP_PORT_USB)
					ptp_usb_control_device_reset_request (params);
			}
			if (retried < 2) { /* try again */
				retried++;
				continue;
			}
			report_result(context, ret, 0);
			return translate_ptp_result (ret);
		}
		break;
	}
	/* We have cameras where a response takes 15 seconds(!), so make
	 * post init timeouts longer */
	CR (gp_port_set_timeout (camera->port, normal_timeout));

	/* Seems HP does not like getdevinfo outside of session
	   although it's legal to do so */
	/* get device info */
	CPR(context, ptp_getdeviceinfo(params, &params->deviceinfo));

	fixup_cached_deviceinfo (camera, &params->deviceinfo);

	GP_DEBUG ("Device info:");
	GP_DEBUG ("Manufacturer: %s",params->deviceinfo.Manufacturer);
	GP_DEBUG ("  Model: %s", params->deviceinfo.Model);
	GP_DEBUG ("  device version: %s", params->deviceinfo.DeviceVersion);
	GP_DEBUG ("  serial number: '%s'",params->deviceinfo.SerialNumber);
	GP_DEBUG ("Vendor extension ID: 0x%08x",params->deviceinfo.VendorExtensionID);
	GP_DEBUG ("Vendor extension version: %d",params->deviceinfo.VendorExtensionVersion);
	GP_DEBUG ("Vendor extension description: %s",params->deviceinfo.VendorExtensionDesc);
	GP_DEBUG ("Functional Mode: 0x%04x",params->deviceinfo.FunctionalMode);
	GP_DEBUG ("PTP Standard Version: %d",params->deviceinfo.StandardVersion);
	GP_DEBUG ("Supported operations:");
	for (i=0; i<params->deviceinfo.OperationsSupported_len; i++)
		GP_DEBUG ("  0x%04x", params->deviceinfo.OperationsSupported[i]);
	GP_DEBUG ("Events Supported:");
	for (i=0; i<params->deviceinfo.EventsSupported_len; i++)
		GP_DEBUG ("  0x%04x", params->deviceinfo.EventsSupported[i]);
	GP_DEBUG ("Device Properties Supported:");
	for (i=0; i<params->deviceinfo.DevicePropertiesSupported_len;
		i++)
		GP_DEBUG ("  0x%04x", params->deviceinfo.DevicePropertiesSupported[i]);

	/* init internal ptp objectfiles (required for fs implementation) */
	/*init_ptp_fs (camera, context);*/

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
#if 0
		{
			unsigned char *x;
			unsigned long l;
			if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_GetDeviceInfoEx))
				ptp_canon_eos_getdeviceinfo (params, &x, &l);
		}
#endif
#if 0
		/* Marcus: this hides all files on the EOS 450D when triggered */
		/* automatically enable capture mode on EOS cameras to populate property list */
		if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease))
			camera_prepare_capture(camera,context);  
#endif
		break;
	case PTP_VENDOR_NIKON:
		if (ptp_operation_issupported(params, PTP_OC_NIKON_CurveDownload))
			add_special_file("curve.ntc", nikon_curve_get, nikon_curve_put);
		break;
	default:
		break;
	}
	CR (gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera));
	SET_CONTEXT(camera, NULL);
	return (GP_OK);
}
