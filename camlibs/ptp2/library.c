/* library.c
 *
 * Copyright (C) 2001-2005 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2005 Marcus Meissner <marcus@jet.franken.de>
 * Copyright (C) 2005 Hubert Figuiere <hfiguiere@teaser.fr>
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
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <gphoto2-library.h>
#include <gphoto2-port-log.h>

#define EXPERIMENTAL_CANON_CAPTURE 0


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

#define GP_MODULE "PTP2"

#define USB_TIMEOUT 8000
#define USB_TIMEOUT_CAPTURE 20000

#define CPR(context,result) {short r=(result); if (r!=PTP_RC_OK) {report_result ((context), r); return (translate_ptp_result (r));}}

#define CPR_free(context,result, freeptr) {\
			short r=(result);\
			if (r!=PTP_RC_OK) {\
				report_result ((context), r);\
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
		{						\
		if (!strncmp(fn,"/"STORAGE_FOLDER_PREFIX,strlen(STORAGE_FOLDER_PREFIX)+1))							\
		{						\
			if (strlen(fn)<strlen(STORAGE_FOLDER_PREFIX)+8+1) \
				return (GP_ERROR);		\
			s = strtoul(fn + strlen(STORAGE_FOLDER_PREFIX)+1, NULL, 16);								\
		} else return (GP_ERROR);			\
		}						\
}

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
	special_files[nrofspecial_files].putfunc = putfunc;
	special_files[nrofspecial_files].getfunc = getfunc;
	nrofspecial_files++;
	return (GP_OK);
}

#define STORAGE_FOLDER_PREFIX		"store_"

/* PTP error descriptions */
static struct {
	short n;
	const char *txt;
} ptp_errors[] = {
	{PTP_RC_Undefined, 		N_("PTP Undefined Error")},
	{PTP_RC_OK, 			N_("PTP OK!")},
	{PTP_RC_GeneralError, 		N_("PTP General Error")},
	{PTP_RC_SessionNotOpen, 	N_("PTP Session Not Open")},
	{PTP_RC_InvalidTransactionID, 	N_("PTP Invalid Transaction ID")},
	{PTP_RC_OperationNotSupported, 	N_("PTP Operation Not Supported")},
	{PTP_RC_ParameterNotSupported, 	N_("PTP Parameter Not Supported")},
	{PTP_RC_IncompleteTransfer, 	N_("PTP Incomplete Transfer")},
	{PTP_RC_InvalidStorageId, 	N_("PTP Invalid Storage ID")},
	{PTP_RC_InvalidObjectHandle, 	N_("PTP Invalid Object Handle")},
	{PTP_RC_DevicePropNotSupported, N_("PTP Device Prop Not Supported")},
	{PTP_RC_InvalidObjectFormatCode, N_("PTP Invalid Object Format Code")},
	{PTP_RC_StoreFull, 		N_("PTP Store Full")},
	{PTP_RC_ObjectWriteProtected, 	N_("PTP Object Write Protected")},
	{PTP_RC_StoreReadOnly, 		N_("PTP Store Read Only")},
	{PTP_RC_AccessDenied,		N_("PTP Access Denied")},
	{PTP_RC_NoThumbnailPresent, 	N_("PTP No Thumbnail Present")},
	{PTP_RC_SelfTestFailed, 	N_("PTP Self Test Failed")},
	{PTP_RC_PartialDeletion, 	N_("PTP Partial Deletion")},
	{PTP_RC_StoreNotAvailable, 	N_("PTP Store Not Available")},
	{PTP_RC_SpecificationByFormatUnsupported,
				N_("PTP Specification By Format Unsupported")},
	{PTP_RC_NoValidObjectInfo, 	N_("PTP No Valid Object Info")},
	{PTP_RC_InvalidCodeFormat, 	N_("PTP Invalid Code Format")},
	{PTP_RC_UnknownVendorCode, 	N_("PTP Unknown Vendor Code")},
	{PTP_RC_CaptureAlreadyTerminated,
					N_("PTP Capture Already Terminated")},
	{PTP_RC_DeviceBusy, 		N_("PTP Device Busy")},
	{PTP_RC_InvalidParentObject, 	N_("PTP Invalid Parent Object")},
	{PTP_RC_InvalidDevicePropFormat, N_("PTP Invalid Device Prop Format")},
	{PTP_RC_InvalidDevicePropValue, N_("PTP Invalid Device Prop Value")},
	{PTP_RC_InvalidParameter, 	N_("PTP Invalid Parameter")},
	{PTP_RC_SessionAlreadyOpened, 	N_("PTP Session Already Opened")},
	{PTP_RC_TransactionCanceled, 	N_("PTP Transaction Canceled")},
	{PTP_RC_SpecificationOfDestinationUnsupported,
			N_("PTP Specification Of Destination Unsupported")},
	{PTP_RC_EK_FilenameRequired,	N_("PTP EK Filename Required")},
	{PTP_RC_EK_FilenameConflicts,	N_("PTP EK Filename Conflicts")},
	{PTP_RC_EK_FilenameInvalid,	N_("PTP EK Filename Invalid")},

	{PTP_ERROR_IO,		  N_("PTP I/O error")},
	{PTP_ERROR_BADPARAM,	  N_("PTP Error: bad parameter")},
	{PTP_ERROR_DATA_EXPECTED, N_("PTP Protocol error, data expected")},
	{PTP_ERROR_RESP_EXPECTED, N_("PTP Protocol error, response expected")},
	{0, NULL}
};

static void
report_result (GPContext *context, short result)
{
	unsigned int i;

	for (i = 0; ptp_errors[i].txt; i++)
		if (ptp_errors[i].n == result)
			gp_context_error (context, "%s", dgettext(GETTEXT_PACKAGE, ptp_errors[i].txt));
}

static int
translate_ptp_result (short result)
{
	switch (result) {
	case PTP_RC_ParameterNotSupported:
		return (GP_ERROR_BAD_PARAMETERS);
	case PTP_ERROR_BADPARAM:
		return (GP_ERROR_BAD_PARAMETERS);
	case PTP_RC_OK:
		return (GP_OK);
	default:
		return (GP_ERROR);
	}
}

static short
translate_gp_result (int result)
{
	switch (result) {
	case GP_OK:
		return (PTP_RC_OK);
	case GP_ERROR:
	default:
		GP_DEBUG ("PTP: gp_port_* function returned 0x%04x \t %i",result,result);
		return (PTP_RC_GeneralError);
	}
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
	{"Kodak:DX7590", 0x040a, 0x057f, 0},
	{"Kodak:CX6445", 0x040a, 0x0584, 0},
	{"Kodak:CX7525", 0x040a, 0x0586, 0},

	/* HP PTP cameras */
	{"HP:PhotoSmart 812 (PTP mode)", 0x03f0, 0x4202, 0},
	{"HP:PhotoSmart 850 (PTP mode)", 0x03f0, 0x4302, 0},
	/* HP PhotoSmart 935: T. Kaproncai, 25 Jul 2003*/
	{"HP:PhotoSmart 935 (PTP mode)", 0x03f0, 0x4402, 0},
	/* HP:PhotoSmart 945: T. Jelbert, 2004/03/29	*/
	{"HP:PhotoSmart 945 (PTP mode)", 0x03f0, 0x4502, 0},
	{"HP:PhotoSmart 318 (PTP mode)", 0x03f0, 0x6302, 0},
	{"HP:PhotoSmart 612 (PTP mode)", 0x03f0, 0x6302, 0},
	{"HP:PhotoSmart 715 (PTP mode)", 0x03f0, 0x6402, 0},
	{"HP:PhotoSmart 120 (PTP mode)", 0x03f0, 0x6502, 0},
	{"HP:PhotoSmart 320 (PTP mode)", 0x03f0, 0x6602, 0},
	{"HP:PhotoSmart 720 (PTP mode)", 0x03f0, 0x6702, 0},
	{"HP:PhotoSmart 620 (PTP mode)", 0x03f0, 0x6802, 0},
	{"HP:PhotoSmart 735 (PTP mode)", 0x03f0, 0x6a02, 0},	
	{"HP:PhotoSmart R707 (PTP mode)", 0x03f0, 0x6b02, 0},
        {"HP:PhotoSmart 635 (PTP mode)", 0x03f0, 0x7102, 0},
	/* report from Federico Prat Villar <fprat@lsi.uji.es> */
	{"HP:PhotoSmart 43x (PTP mode)", 0x03f0, 0x7202, 0},
	{"HP:PhotoSmart M307 (PTP mode)", 0x03f0, 0x7302, 0},

	/* Most Sony PTP cameras use the same product/vendor IDs. */
	{"Sony:PTP",                  0x054c, 0x004e, 0},
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
	{"Sony:DSC-S75 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-S85 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-T1 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-T3 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-U20 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:DSC-V1 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-W1 (PTP mode)",    0x054c, 0x004e, 0},
	{"Sony:DSC-W12 (PTP mode)",   0x054c, 0x004e, 0},
	{"Sony:MVC-CD300 (PTP mode)", 0x054c, 0x004e, 0},
	{"Sony:MVC-CD500 (PTP mode)", 0x054c, 0x004e, 0},

	/* Nikon Coolpix 2500: M. Meissner, 05 Oct 2003 */
	{"Nikon:Coolpix 2500 (PTP mode)", 0x04b0, 0x0109, 0},
	/* Nikon Coolpix 5700: A. Tanenbaum, 29 Oct 2002 */
	{"Nikon:Coolpix 5700 (PTP mode)", 0x04b0, 0x010d, 0},
	/* Nikon CoolPix 4500: T. Kaproncai, 22 Aug 2003 */
	{"Nikon CoolPix 4500 (PTP mode)", 0x04b0, 0x010b, 0},
	/* Nikon Coolpix 4300: Marco Rodriguez, 10 dic 2002 */
	{"Nikon:Coolpix 4300 (PTP mode)", 0x04b0, 0x010f, 0},
	/* Nikon Coolpix 3500: M. Meissner, 07 May 2003 */
	{"Nikon:Coolpix 3500 (PTP mode)", 0x04b0, 0x0111, 0},
	/* Nikon Coolpix 885: S. Anderson, 19 nov 2002 */
	{"Nikon:Coolpix 885 (PTP mode)",  0x04b0, 0x0112, 0},
	/* Nikon Coolpix 5000, Firmware v1.7 or later */
	{"Nikon:Coolpix 5000 (PTP mode)", 0x04b0, 0x0113, 0},
	/* Nikon Coolpix 3100 */
	{"Nikon:Coolpix 3100 (PTP mode)", 0x04b0, 0x0115, 0},
	/* Nikon Coolpix 2100 */
	{"Nikon:Coolpix 2100 (PTP mode)", 0x04b0, 0x0117, 0},
	/* Nikon Coolpix 5400: T. Kaproncai, 25 Jul 2003 */
	{"Nikon:Coolpix 5400 (PTP mode)", 0x04b0, 0x0119, 0},
	/* Nikon Coolpix 3700: T. Ehlers, 18 Jan 2004 */
	{"Nikon:Coolpix 3700 (PTP mode)", 0x04b0, 0x011d, 0},
	/* Nikon Coolpix 3200 */
	{"Nikon:Coolpix 3200 (PTP mode)", 0x04b0, 0x0121, 0},
	/* Nikon Coolpix 2200 */
	{"Nikon:Coolpix 2200 (PTP mode)", 0x04b0, 0x0122, 0},
	/* Nikon Coolpix SQ: M. Holzbauer, 07 Jul 2003 */
	{"Nikon:Coolpix 4100 (PTP mode)", 0x04b0, 0x012d, 0},
	{"Nikon:Coolpix SQ (PTP mode)",   0x04b0, 0x0202, 0},
	/* lars marowski bree, 16.8.2004 */
	{"Nikon:Coolpix 4200 (PTP mode)", 0x04b0, 0x0204, 0},
	/* Nikon Coolpix 2000 */
	{"Nikon:Coolpix 2000 (PTP mode)", 0x04b0, 0x0302, 0},
	/* Nikon D100 has a PTP mode: westin 2002.10.16 */
	{"Nikon:DSC D100 (PTP mode)",     0x04b0, 0x0402, 0},
	/* D2H SLR in PTP mode from Steve Drew <stevedrew@gmail.com> */
	{"Nikon:D2H SLR (PTP mode)",      0x04b0, 0x0404, 0},
	{"Nikon:DSC D70 (PTP mode)",      0x04b0, 0x0406, 0},

	/* Justin Case <justin_case@gmx.net> */
	{"Nikon:D2X SLR (PTP mode)",      0x04b0, 0x0408, 0},

	/* Thomas Luzat <thomas.luzat@gmx.net> */
	{"Panasonic:DMC-FZ20 (alternate id)", 0x04da, 0x2372, 0},

	{"Panasonic:DMC-FZ20",            0x04da, 0x2374, 0},

	/* http://callendor.zongo.be/wiki/OlympusMju500 */
	{"Olympus:mju 500",               0x07b4, 0x0113, 0},
	/* From VICTOR <viaaurea@yahoo.es> */
	{"Olympus:C-350Z",                0x07b4, 0x0114, 0},
	{"Olympus:D-560Z",                0x07b4, 0x0114, 0},
	{"Olympus:X-250",                 0x07b4, 0x0114, 0},

	/* (at least some) newer Canon cameras can be switched between
	 * PTP and "normal" (i.e. Canon) mode
	 * Canon PS G3: A. Marinichev, 20 nov 2002
	 */
	{"Canon:PowerShot S45 (PTP mode)",      0x04a9, 0x306d, 0},
		/* 0x306c is S45 in normal (canon) mode */
	{"Canon:PowerShot G3 (PTP mode)",       0x04a9, 0x306f, 0},
		/* 0x306e is G3 in normal (canon) mode */
	{"Canon:PowerShot S230 (PTP mode)",     0x04a9, 0x3071, 0},
		/* 0x3070 is S230 in normal (canon) mode */
	{"Canon:Digital IXUS v3 (PTP mode)",    0x04a9, 0x3071, 0},
		/* it's the same as S230 */

	{"Canon:Digital IXUS II (PTP mode)",    0x04a9, 0x3072, 0},
	{"Canon:PowerShot SD100 (PTP mode)",    0x04a9, 0x3072, 0},

	{"Canon:PowerShot A70 (PTP)",           0x04a9, 0x3073, 0},
	{"Canon:PowerShot A60 (PTP)",           0x04a9, 0x3074, 0},
		/* IXUS 400 has the same PID in both modes, Till Kamppeter */
	{"Canon:Digital IXUS 400 (PTP mode)",   0x04a9, 0x3075, 0},
	{"Canon:PowerShot S50 (PTP mode)",      0x04a9, 0x3077, 0},
	{"Canon:PowerShot G5 (PTP mode)",       0x04a9, 0x3085, 0},
	{"Canon:Elura 50 (PTP mode)",           0x04a9, 0x3087, 0},
		/* 0x3084 is the EOS 300D/Digital Rebel in normal (canon) mode */
	{"Canon:EOS 300D (PTP mode)",           0x04a9, 0x3099, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:EOS Digital Rebel (PTP mode)",  0x04a9, 0x3099, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:EOS Kiss Digital (PTP mode)",   0x04a9, 0x3099, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:PowerShot A80 (PTP)",           0x04a9, 0x309a, 0},
	{"Canon:Digital IXUS i (PTP mode)",     0x04a9, 0x309b, 0},
	{"Canon:PowerShot S1 IS (PTP mode)",    0x04a9, 0x309c, 0},
	{"Canon:Powershot S70 (PTP mode)",      0x04a9, 0x30b1, 0},
	{"Canon:Powershot S60 (PTP mode)",      0x04a9, 0x30b2, 0},
	{"Canon:Powershot G6 (PTP mode)",       0x04a9, 0x30b3, 0},
	{"Canon:Digital IXUS 500 (PTP mode)",   0x04a9, 0x30b4, 0},
	{"Canon:PowerShot S500 (PTP mode)",     0x04a9, 0x30b4, 0},
	{"Canon:PowerShot A75 (PTP mode)",      0x04a9, 0x30b5, 0},
	{"Canon:PowerShot SD110 (PTP mode)",    0x04a9, 0x30b6, 0},
	{"Canon:Digital IXUS IIs (PTP mode)",   0x04a9, 0x30b6, 0},
	{"Canon:PowerShot A400 (PTP mode)",     0x04a9, 0x30b7, 0},
	{"Canon:PowerShot A310 (PTP mode)",     0x04a9, 0x30b8, 0},
	{"Canon:PowerShot A85 (PTP mode)",      0x04a9, 0x30b9, 0},
	{"Canon:Digital IXUS 430 (PTP mode)",   0x04a9, 0x30ba, 0},
 	{"Canon:PowerShot S410 (PTP mode)",     0x04a9, 0x30ba, 0},
 	{"Canon:PowerShot A95 (PTP mode)",      0x04a9, 0x30bb, 0},
	{"Canon:Digital IXUS 40 (PTP mode)",    0x04a9, 0x30bf, 0},
 	{"Canon:PowerShot SD200 (PTP mode)",    0x04a9, 0x30c0, 0},
 	{"Canon:Digital IXUS 30 (PTP mode)",    0x04a9, 0x30c0, 0},
 	{"Canon:PowerShot A520 (PTP mode)",     0x04a9, 0x30c1, 0},
	{"Canon:PowerShot A510 (PTP mode)",     0x04a9, 0x30c2, 0},
	{"Canon:EOS 1D Mark II (PTP mode)",     0x04a9, 0x30ea, PTPBUG_DCIM_WRONG_PARENT},
 	{"Canon:EOS 20D (PTP mode)",            0x04a9, 0x30ec, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:EOS 350D (PTP mode)",           0x04a9, 0x30ef, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:PowerShot S2 IS (PTP mode)",    0x04a9, 0x30f0, 0},
	{"Canon:Digital IXUS 700 (PTP mode)",   0x04a9, 0x30f2, 0},
	{"Canon:PowerShot SD500 (PTP mode)",    0x04a9, 0x30f2, 0},
	/* Nick Richards <nick@nedrichards.com> */
	{"Canon:Digital IXUS 50 (PTP mode)",    0x04a9, 0x310e, 0},

	/* Konica-Minolta PTP cameras */
	{"Konica-Minolta:DiMAGE A2 (PTP mode)",        0x132b, 0x0001, 0},
	{"Konica-Minolta:DiMAGE Z2 (PictBridge mode)", 0x132b, 0x0007, 0},
	{"Konica-Minolta:DiMAGE Z3 (PictBridge mode)", 0x132b, 0x0018, 0},
	{"Konica-Minolta:DiMAGE Z5 (PictBridge mode)", 0x132b, 0x0022, 0},

	/* Fuji PTP cameras */
	{"Fuji:FinePix S7000 (PictBridge mode)",0x04cb, 0x0142, 0},
	{"Fuji:FinePix A330 (PictBridge mode)",0x04cb, 0x014a, 0},

	/* Ricoh Caplio GX */
	{"Ricoh:Caplio GX (PTP mode)",          0x05ca, 0x0325, 0},

	/* Rollei dr5  */
	{"Rollei:dr5 (PTP mode)",               0x05ca, 0x220f, 0},

	/* Pentax cameras */
	{"Pentax:Optio 43WR",                   0x0a17, 0x000d, 0},

	/* more coming soon :) */
	{NULL, 0, 0, 0}
};

static struct {
	unsigned short format_code;
	const char *txt;
} object_formats[] = {
	{PTP_OFC_Undefined,	"application/x-unknown"},
	{PTP_OFC_Association,	"application/x-association"},
	{PTP_OFC_Script,	"application/x-script"},
	{PTP_OFC_Executable,	"application/octet-stream"},
	{PTP_OFC_Text,		"text/plain"},
	{PTP_OFC_HTML,		"text/html"},
	{PTP_OFC_DPOF,		"text/plain"},
	{PTP_OFC_AIFF,		"audio/x-aiff"},
	{PTP_OFC_WAV,		GP_MIME_WAV},
	{PTP_OFC_MP3,		"audio/basic"},
	{PTP_OFC_AVI,		GP_MIME_AVI},
	{PTP_OFC_MPEG,		"video/mpeg"},
	{PTP_OFC_ASF,		"video/x-asf"},
	{PTP_OFC_QT,		"video/quicktime"},
	{PTP_OFC_EXIF_JPEG,	GP_MIME_JPEG},
	{PTP_OFC_TIFF_EP,	"image/x-tiffep"},
	{PTP_OFC_FlashPix,	"image/x-flashpix"},
	{PTP_OFC_BMP,		GP_MIME_BMP},
	{PTP_OFC_CIFF,		"image/x-ciff"},
	{PTP_OFC_Undefined_0x3806, "application/x-unknown"},
	{PTP_OFC_GIF,		"image/gif"},
	{PTP_OFC_JFIF,		GP_MIME_JPEG},
	{PTP_OFC_PCD,		"image/x-pcd"},
	{PTP_OFC_PICT,		"image/x-pict"},
	{PTP_OFC_PNG,		GP_MIME_PNG},
	{PTP_OFC_Undefined_0x380C, "application/x-unknown"},
	{PTP_OFC_TIFF,		GP_MIME_TIFF},
	{PTP_OFC_TIFF_IT,	"image/x-tiffit"},
	{PTP_OFC_JP2,		"image/x-jpeg2000bff"},
	{PTP_OFC_JPX,		"image/x-jpeg2000eff"},
	{0,			NULL}
};

static int
set_mimetype (Camera *camera, CameraFile *file, uint16_t ofc)
{
	int i;

	for (i = 0; object_formats[i].format_code; i++)
		if (object_formats[i].format_code == ofc)
		{
			CR (gp_file_set_mime_type (file, object_formats[i].txt));
			return (GP_OK);
		}

	CR (gp_file_set_mime_type (file, "application/x-unknown"));
	return (GP_OK);
}



static void
strcpy_mime(char * dest, uint16_t ofc) {
	int i;

	for (i = 0; object_formats[i].format_code; i++)
		if (object_formats[i].format_code == ofc) {
			strcpy(dest, object_formats[i].txt);
			return;
		}
	strcpy(dest, "application/x-unknown");
}

static uint32_t
get_mimetype (Camera *camera, CameraFile *file)
{
	int i;
	const char *mimetype;

	gp_file_get_mime_type (file, &mimetype);
	for (i = 0; object_formats[i].format_code; i++)
		if (!strcmp(mimetype,object_formats[i].txt))
			return (object_formats[i].format_code);
	return (PTP_OFC_Undefined);
}
	
struct _CameraPrivateLibrary {
	PTPParams params;
	unsigned long bugs;
};

struct _PTPData {
	Camera *camera;
	GPContext *context;
};
typedef struct _PTPData PTPData;

static short
ptp_read_func (unsigned char *bytes, unsigned int size, void *data, unsigned int *readbytes)
{
	Camera *camera = ((PTPData *)data)->camera;
	int toread, result = GP_ERROR, curread = 0;

	/* Split into small blocks. Too large blocks (>1x MB) would
	 * timeout.
	 */
	while (curread < size) {
		toread =size - curread;
		if (toread > 4096)
			toread = 4096;
		result = gp_port_read (camera->port, bytes + curread, toread);
		if (result == 0) {
			result = gp_port_read (camera->port, bytes + curread, toread);
		}
		if (result < 0)
			break;
		curread += result;
		if (result < toread) /* short reads are common */
			break;
	}
	if (result > 0) {
		*readbytes = curread;
		return (PTP_RC_OK);
	} else {
		return (translate_gp_result (result));
	}
}

static short
ptp_write_func (unsigned char *bytes, unsigned int size, void *data)
{
	Camera *camera = ((PTPData *)data)->camera;
	int result;

	/*
	 * gp_port_write returns (in case of success) the number of bytes
	 * write. libptp doesn't need that.
	 */
	result = gp_port_write (camera->port, bytes, size);
	if (result >= 0)
		return (PTP_RC_OK);
	else
	{
		perror("gp_port_write");
		return (translate_gp_result (result));
	}
}

static short
ptp_check_int (unsigned char *bytes, unsigned int size, void *data, unsigned int *rlen)
{
	Camera *camera = ((PTPData *)data)->camera;
	int result;

	/*
	 * gp_port_check_int returns (in case of success) the number of bytes
	 * read.
	 */

	result = gp_port_check_int (camera->port, bytes, size);
	if (result==0) result = gp_port_check_int (camera->port, bytes, size);
	if (result >= 0) {
		*rlen = result;
		return (PTP_RC_OK);
	} else {
		return (translate_gp_result (result));
	}
}

static short
ptp_check_int_fast (unsigned char *bytes, unsigned int size, void *data, unsigned int *rlen)
{
	Camera *camera = ((PTPData *)data)->camera;
	int result;

	/*
	 * gp_port_check_int returns (in case of success) the number of bytes
	 * read. libptp doesn't need that.
	 */

	result = gp_port_check_int_fast (camera->port, bytes, size);
	if (result==0) result = gp_port_check_int_fast (camera->port, bytes, size);
	if (result >= 0) {
		*rlen = result;
		return (PTP_RC_OK);
	} else {
		return (translate_gp_result (result));
	}
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

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	memset(&a,0, sizeof(a));
	for (i = 0; models[i].model; i++) {
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port   = GP_PORT_USB;
		a.speed[0] = 0;
		a.usb_vendor = models[i].usb_vendor;
		a.usb_product= models[i].usb_product;
		a.operations        = GP_CAPTURE_IMAGE | GP_OPERATION_CONFIG;
		a.file_operations   = GP_FILE_OPERATION_PREVIEW |
					GP_FILE_OPERATION_DELETE;
		a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE
			| GP_FOLDER_OPERATION_MAKE_DIR |
			GP_FOLDER_OPERATION_REMOVE_DIR;
		CR (gp_abilities_list_append (list, a));
		memset(&a,0, sizeof(a));
	}

	strcpy(a.model, "USB PTP Class Camera");
	a.status = GP_DRIVER_STATUS_TESTING;
	a.port   = GP_PORT_USB;
	a.speed[0] = 0;
	a.usb_class = 6;
	a.usb_subclass = -1;
	a.usb_protocol = -1;
	a.operations        = GP_CAPTURE_IMAGE | GP_OPERATION_CONFIG;
	a.file_operations   = GP_FILE_OPERATION_PREVIEW|
				GP_FILE_OPERATION_DELETE;
	a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE
		| GP_FOLDER_OPERATION_MAKE_DIR |
		GP_FOLDER_OPERATION_REMOVE_DIR;
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
		/* close ptp session */
		ptp_closesession (&camera->pl->params);
		free (camera->pl);
		camera->pl = NULL;
	}
	if (camera->port!=NULL)  {
		/* clear halt */
		gp_port_usb_clear_halt
				(camera->port, GP_PORT_USB_ENDPOINT_IN);
		gp_port_usb_clear_halt
				(camera->port, GP_PORT_USB_ENDPOINT_OUT);
		gp_port_usb_clear_halt
				(camera->port, GP_PORT_USB_ENDPOINT_INT);
	}

	/* FIXME: free all camera->pl->params.objectinfo[] and
	   other malloced data */

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *text, GPContext *context)
{
	strncpy (text->text,
		 _("PTP2 driver\n"
		   "(c)2001-2003 by Mariusz Woloszyn <emsi@ipartners.pl>.\n"
		   "Enjoy!"), sizeof (text->text));
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

/* add new object to internal driver structures. issued when creating
folder, uploading objects, or captured images. */
static int
add_object (Camera *camera, uint32_t handle, GPContext *context)
{
	int n;

	/* increase number of objects */
	n=++camera->pl->params.handles.n;
	/* realloc more space for camera->pl->params.objectinfo */
	camera->pl->params.objectinfo = (PTPObjectInfo*)
		realloc(camera->pl->params.objectinfo,
			sizeof(PTPObjectInfo)*n);
	/* realloc more space for amera->pl->params.handles.Handler */
	camera->pl->params.handles.Handler= (uint32_t *)
		realloc(camera->pl->params.handles.Handler,
			sizeof(uint32_t)*n);
	/* clear objectinfo entry for new object and assign new handler */
	memset(&camera->pl->params.objectinfo[n-1],0,sizeof(PTPObjectInfo));
	camera->pl->params.handles.Handler[n-1]=handle;
	/* get new obectinfo */
	CPR (context, ptp_getobjectinfo(&camera->pl->params,
		handle,
		&camera->pl->params.objectinfo[n-1]));
	return (GP_OK);
}

static int
camera_prepare_capture (Camera *camera, GPContext *context)
{
	PTPUSBEventContainer	event;
	PTPContainer		evc;
	PTPPropertyValue	propval;
	uint16_t		val16;
	int 			i, ret, isevent;
	PTPParams		*params = &camera->pl->params;

	gp_log (GP_LOG_DEBUG, "ptp", "prepare_capture\n");
	switch (params->deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_CANON:
		if (!ptp_operation_issupported(params, PTP_OC_CANON_StartShootingMode)) {
			gp_context_error(context,
			_("Sorry, your Canon camera does not support Canon capture"));
			return GP_ERROR_NOT_SUPPORTED;
		}
		
		propval.u16 = 0;
    		ret = ptp_getdevicepropvalue(params, 0xd045, &propval, PTP_DTC_UINT16);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_DEBUG, "ptp", "failed get 0xd045\n");
			return GP_ERROR;
		}
		gp_log (GP_LOG_DEBUG, "ptp","prop 0xd045 value is 0x%4X\n",propval.u16);

    		propval.u16=1;
		ret = ptp_setdevicepropvalue(params, 0xd045, &propval, PTP_DTC_UINT16);
		ret = ptp_getdevicepropvalue(params, 0xd02e, &propval, PTP_DTC_UINT32);
		gp_log (GP_LOG_DEBUG, "ptp", "prop 0xd02e value is 0x%8X, ret %d\n",propval.u32, ret);
		ret = ptp_getdevicepropvalue(params, 0xd02f, &propval, PTP_DTC_UINT32);
		gp_log (GP_LOG_DEBUG, "ptp", "prop 0xd02f value is 0x%8X, ret %d\n",propval.u32, ret);

		ret = ptp_getdeviceinfo (params, &params->deviceinfo);
		ret = ptp_getdeviceinfo (params, &params->deviceinfo);

		ret = ptp_getdevicepropvalue(params, 0xd02e, &propval, PTP_DTC_UINT32);
		gp_log (GP_LOG_DEBUG, "ptp", "prop 0xd02e value is 0x%8x, ret %d\n",propval.u32, ret);
		ret = ptp_getdevicepropvalue(params, 0xd02f, &propval, PTP_DTC_UINT32);
		gp_log (GP_LOG_DEBUG, "ptp", "prop 0xd02f value is 0x%8x, ret %d\n",propval.u32,ret);
		ret = ptp_getdeviceinfo (params, &params->deviceinfo);
		ret = ptp_getdevicepropvalue(params, 0xd045, &propval, PTP_DTC_UINT16);
		gp_log (GP_LOG_DEBUG, "ptp","prop 0xd045 value is 0x%4x, ret %d\n",propval.u16,ret);

		gp_log (GP_LOG_DEBUG, "ptp","Magic code ends.\n");
        
		gp_log (GP_LOG_DEBUG, "ptp","Setting prop. 0xd045 to 4\n");
		propval.u16=4;
		ret = ptp_setdevicepropvalue(params, PTP_DPC_CANON_D045, &propval, PTP_DTC_UINT16);

		CPR (context, ptp_canon_startshootingmode (params));

		/* Catch event */
		if (PTP_RC_OK==(val16=ptp_usb_event_wait (params, &evc))) {
			if (evc.Code==PTP_EC_StorageInfoChanged)
				gp_log (GP_LOG_DEBUG, "ptp", "Event: entering  shooting mode. \n");
			else 
				gp_log (GP_LOG_DEBUG, "ptp", "Event: 0x%X\n", evc.Code);
		} else {
			printf("No event yet, we'll try later.\n");
		}
    
		/* Emptying event stack */
		for (i=0;i<2;i++) {
			ret = ptp_canon_checkevent (params,&event,&isevent);
			if (ret != PTP_RC_OK) {
				gp_log (GP_LOG_DEBUG, "ptp", "error during check event: %d\n", ret);
			}
			if (isevent)
				gp_log (GP_LOG_DEBUG, "ptp", "evdata: L=0x%X, T=0x%X, C=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X\n",
					event.length,event.type,event.code,event.trans_id,
					event.param1, event.param2, event.param3);
		}
		/* Catch event, attempt  2 */
		if (val16!=PTP_RC_OK) {
			if (PTP_RC_OK==ptp_usb_event_wait (params, &evc)) {
				if (evc.Code == PTP_EC_StorageInfoChanged)
					gp_log (GP_LOG_DEBUG, "ptp","Event: entering shooting mode.\n");
				else
					gp_log (GP_LOG_DEBUG, "ptp","Event: 0x%X\n", evc.Code);
			} else
				gp_log (GP_LOG_DEBUG, "ptp", "No expected mode change event.\n");
		}
		/* Reget device info, they change on the Canons. */
		ptp_getdeviceinfo(&camera->pl->params, &camera->pl->params.deviceinfo);
		return GP_OK;
	default:
		/* generic capture does not need preparation */
		return GP_OK;
	}
	return GP_OK;
}

static int
camera_unprepare_capture (Camera *camera, GPContext *context)
{
	int ret;

	gp_log (GP_LOG_DEBUG, "ptp", "Unprepare_capture\n");
	switch (camera->pl->params.deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_CANON:
		if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EndShootingMode)) {
			gp_context_error(context,
			_("Sorry, your Canon camera does not support Canon capture"));
			return GP_ERROR_NOT_SUPPORTED;
		}
		ret = ptp_canon_endshootingmode (&camera->pl->params);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_DEBUG, "ptp", "end shooting mode error %d\n", ret);
			return GP_ERROR;
		}
		/* Reget device info, they change on the Canons. */
		ptp_getdeviceinfo(&camera->pl->params, &camera->pl->params.deviceinfo);
		return GP_OK;
	default:
		/* generic capture does not need unpreparation */
		return GP_OK;
	}
	return GP_OK;
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	char		*data;
	uint32_t	size;
	int ret;
	PTPParams *params = &camera->pl->params;

	/* Currently disabled, since we must make sure for Canons
	 * that prepare capture was called.
	 * Enable: remote 0 &&, run 
	 * 	gphoto2 --set-config capture=on --capture-preview
	 */
	if (	EXPERIMENTAL_CANON_CAPTURE &&
		camera->pl->params.deviceinfo.VendorExtensionID == PTP_VENDOR_CANON
	) {
		if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_ViewfinderOn)) {
			gp_context_error (context,
			_("Sorry, your Canon camera does not support Canon Viewfinder mode"));
			return GP_ERROR_NOT_SUPPORTED;
		}
		ret = ptp_canon_viewfinderon (params);
		if (ret != PTP_RC_OK) {
			gp_context_error (context, _("Canon enable viewfinder failed: %d"), ret);
			return GP_ERROR;
		}
		ret = ptp_canon_getviewfinderimage (params, &data, &size);
		if (ret != PTP_RC_OK) {
			gp_context_error (context, _("Canon get viewfinder image failed: %d"), ret);
			return GP_ERROR;
		}
		gp_file_set_data_and_size ( file, data, size );
		gp_file_set_mime_type (file, GP_MIME_JPEG);     /* always */
		/* Add an arbitrary file name so caller won't crash */
		gp_file_set_name (file, "canon_preview.jpg");

		ret = ptp_canon_viewfinderon (params);
		if (ret != PTP_RC_OK) {
			gp_context_error (context, _("Canon disable viewfinder failed: %d"), ret);
			return GP_ERROR;
		}
		return GP_OK;
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
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	PTPContainer event;
	PTPUSBEventContainer usbevent;
	int ret, i, isevent;
	PTPParams *params = &camera->pl->params;
	uint16_t val16;
	uint32_t handle;
	PTPPropertyValue propval;
	uint32_t	newobject = 0x0;

	if (type != GP_CAPTURE_IMAGE)
		return GP_ERROR_NOT_SUPPORTED;

	/* Canon mode disabled until functioning fully */
	/* to enable, remove 0&&, then:
	 *	gphoto2 --set-config capture=on --config --capture-image
	 *	gphoto2  -f /store_80000001 -p 1
	 *		will download a file called "VirtualObject"
	 */
	if (	EXPERIMENTAL_CANON_CAPTURE &&
		camera->pl->params.deviceinfo.VendorExtensionID == PTP_VENDOR_CANON
	) {
		if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_InitiateCaptureInMemory)) {
			gp_context_error (context,
			_("Sorry, your Canon camera does not support Canon Captureinitiation"));
			return GP_ERROR_NOT_SUPPORTED;
		}
		propval.u16=3; /* 3 */
		ret = ptp_setdevicepropvalue(params, PTP_DPC_CANON_D029, &propval, PTP_DTC_UINT16);
		if (ret != PTP_RC_OK)
			gp_log (GP_LOG_DEBUG, "ptp", "setdevicepropvalue 0xd029 failed, %d\n", ret);

		/* FIXME: For now, to avoid flash during debug */
		propval.u8 = 0;
		ret = ptp_setdevicepropvalue(params, PTP_DPC_CANON_FlashMode, &propval, PTP_DTC_UINT8);

		ret = ptp_canon_initiatecaptureinmemory (params);
		if (ret != PTP_RC_OK) {
			gp_context_error (context, _("Canon Capture failed: %d"), ret);
			return GP_ERROR;
		}
		/* Catch event */
		if (PTP_RC_OK == (val16 = ptp_usb_event_wait (params, &event))) {
			if (event.Code == PTP_EC_CaptureComplete)
				gp_log (GP_LOG_DEBUG, "ptp", "Event: capture complete. \n");
			else
				gp_log (GP_LOG_DEBUG, "ptp", "Unknown event: 0x%X\n", event.Code);
		} /* else no event yet ... try later. */

	        /* checking events in stack. */
		for (i=0;i<100;i++) {
			ret = ptp_canon_checkevent (params,&usbevent,&isevent);
			if (ret!=PTP_RC_OK)
				continue;
			if (isevent)
				gp_log (GP_LOG_DEBUG, "ptp","evdata: L=0x%X, T=0x%X, C=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X\n", usbevent.length,usbevent.type,usbevent.code,usbevent.trans_id, usbevent.param1, usbevent.param2, usbevent.param3);
			if (	isevent  &&
				(usbevent.type==PTP_USB_CONTAINER_EVENT) &&
				(usbevent.code==PTP_EC_CANON_RequestObjectTransfer)
			) {
				int j;

				handle=usbevent.param1;
				gp_log (GP_LOG_DEBUG, "ptp", "PTP_EC_CANON_RequestObjectTransfer, object handle=0x%X. \n",usbevent.param1);
				gp_log (GP_LOG_DEBUG, "ptp","evdata: L=0x%X, T=0x%X, C=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X\n", usbevent.length,usbevent.type,usbevent.code,usbevent.trans_id, usbevent.param1, usbevent.param2, usbevent.param3);

				add_object (camera, usbevent.param1, context);

				for (j=0;j<2;j++) {
					ret=ptp_canon_checkevent(params,&usbevent,&isevent);
					if ((ret==PTP_RC_OK) && isevent)
						gp_log (GP_LOG_DEBUG, "ptp", "evdata: L=0x%X, T=0x%X, C=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X\n", usbevent.length,usbevent.type,usbevent.code,usbevent.trans_id, usbevent.param1, usbevent.param2, usbevent.param3);
				}


				ret = ptp_canon_reflectchanges(params,7);
				break;
			}
		}
		/* Catch event, attempt  2 */
		if (val16!=PTP_RC_OK) {
			if (PTP_RC_OK==ptp_usb_event_wait (params, &event)) {
				if (event.Code==PTP_EC_CaptureComplete)
					printf("Event: capture complete. \n");
				else
					printf("Event: 0x%X\n", event.Code);
			} else
				printf("No expected capture complete event\n");
		}
		if (i==100)
		    gp_log (GP_LOG_DEBUG, "ptp","ERROR: Capture timed out!\n");        
		goto out;
	}

	if (!ptp_operation_issupported(&camera->pl->params,
		PTP_OC_InitiateCapture)) {
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
	CPR(context,ptp_initiatecapture(&camera->pl->params, 0x00000000, 0x00000000));
	CR (gp_port_set_timeout (camera->port, USB_TIMEOUT_CAPTURE));
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
	/* I hate workarounds! Nikon is not 100% PTP compatible here! */
	if (camera->pl->params.deviceinfo.VendorExtensionID==PTP_VENDOR_NIKON) 
		goto out;
	{
		short ret;
		ret=ptp_usb_event_wait(&camera->pl->params,&event);
		CR (gp_port_set_timeout (camera->port, USB_TIMEOUT));
		if (ret!=PTP_RC_OK) goto err;
	}
	while (event.Code==PTP_EC_ObjectAdded) {
		/* add newly created object to internal structures */
		add_object (camera, event.Param1, context);
		newobject = event.Param1;

		if (ptp_usb_event_wait(&camera->pl->params, &event)!=PTP_RC_OK)
		{
			gp_context_error (context,
			_("Capture command completed, but no confirmation received"));
			goto err;
		}
	} 
	if (event.Code==PTP_EC_CaptureComplete) 
		goto out;

	gp_context_error (context,_("Received event 0x%04x"),event.Code);

err:
	/* we're not setting *path on error! */
	return GP_ERROR;

out:
	/* clear path, so we get defined results even without object info */
	path->name[0]='\0';
	path->folder[0]='\0';

	if (newobject != 0) {
		int i;

		for (i = camera->pl->params.handles.n ; i--; ) {
			PTPObjectInfo	*obinfo;

			if (camera->pl->params.handles.Handler[i] != newobject)
				continue;
			obinfo = &camera->pl->params.objectinfo[i];
			strcpy  (path->name,  obinfo->Filename);
			sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx/",(unsigned long)obinfo->StorageID);
			get_folder_from_handle (camera, obinfo->StorageID, obinfo->ParentObject, path->folder);
			/* delete last / or we get confused later. */
			path->folder[ strlen(path->folder)-1 ] = '\0';
			CR (gp_filesystem_append (camera->fs, path->folder,
				path->name, context));
			break;
		}
	}
	return GP_OK;
}

static void
_value_to_str(PTPPropertyValue *data, uint16_t dt, char *txt) {
	if (dt == PTP_DTC_STR) {
		sprintf (txt, "'%s'", data->str);
		return;
	}
	if (dt & PTP_DTC_ARRAY_MASK) {
		int i;

		sprintf (txt, "a[%d] ", data->a.count);
		txt += strlen(txt);
		for ( i=0; i<data->a.count; i++) {
			_value_to_str(&data->a.v[i], dt & ~PTP_DTC_ARRAY_MASK, txt);
			txt += strlen(txt);
			if (i!=data->a.count-1) {
				sprintf (txt, ",");
				txt++;
			}
		}
	} else {
		switch (dt) {
		case PTP_DTC_UNDEF: 
			sprintf (txt, "Undefined");
			break;
		case PTP_DTC_INT8:
			sprintf (txt, "%d", data->i8);
			break;
		case PTP_DTC_UINT8:
			sprintf (txt, "%u", data->u8);
			break; 
		case PTP_DTC_INT16:
			sprintf (txt, "%d", data->i16);
			break;
		case PTP_DTC_UINT16:
			sprintf (txt, "%u", data->u16);
			break;
		case PTP_DTC_INT32:
			sprintf (txt, "%d", data->i32);
			break;
		case PTP_DTC_UINT32:
			sprintf (txt, "%u", data->u32);
			break;
	/*
		PTP_DTC_INT64           
		PTP_DTC_UINT64         
		PTP_DTC_INT128        
		PTP_DTC_UINT128      
	*/
		default:
			sprintf (txt, "Unknown %x", dt);
			break;
		}
	}
	return;
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

static int
canon_theme_get (CameraFilesystem *fs, const char *folder, const char *filename,
		 CameraFileType type, CameraFile *file, void *data,
		 GPContext *context)
{
	uint16_t	res;
	Camera		*camera = (Camera*)data;
	PTPParams	*params = &camera->pl->params;
	char		*xdata;
	unsigned int	size;

	((PTPData *) camera->pl->params.data)->context = context;

	res = ptp_canon_theme_download (params, 1, &xdata, &size);
	if (res != PTP_RC_OK)  {
		report_result(context, res);
		return (translate_ptp_result(res));
	}
	CR (gp_file_set_data_and_size (file, xdata, size));
	return (GP_OK);
}

static int
canon_theme_put (CameraFilesystem *fs, const char *folder, CameraFile *file,
		void *data, GPContext *context)
{
	/* not yet */
	return (GP_OK);
}

static int
camera_summary (Camera* camera, CameraText* summary, GPContext *context)
{
	int n, i, j;
	char *txt;
	PTPParams *params = &(camera->pl->params);
	PTPDeviceInfo pdi;

	n = snprintf (summary->text, sizeof (summary->text),
		_("Model: %s\n"
		"  device version: %s\n"
		"  serial number:  %s\n"
		"Vendor extension ID: 0x%08x\n"
		"Vendor extension description: %s\n"
	),
		camera->pl->params.deviceinfo.Model,
		camera->pl->params.deviceinfo.DeviceVersion,
		camera->pl->params.deviceinfo.SerialNumber,
		camera->pl->params.deviceinfo.VendorExtensionID,
		camera->pl->params.deviceinfo.VendorExtensionDesc);

	if (n>=sizeof (summary->text))
		return GP_OK;

	txt = summary->text + strlen (summary->text);

	/* The information is cached. However, the canon firmware changes
	 * the available properties in capture mode.
	 */
	CPR(context, ptp_getdeviceinfo(&camera->pl->params, &pdi));

        for (i=0;i<pdi.DevicePropertiesSupported_len;i++) {
		PTPDevicePropDesc dpd;
		unsigned int dpc = pdi.DevicePropertiesSupported[i];
		const char *propname = ptp_get_property_description (params, dpc);

		if (propname) {
			/* string registered for i18n in ptp.c. */
			sprintf(txt, "%s(0x%04x):", _(propname), dpc);
		} else {
			sprintf(txt, "Property 0x%04x:", dpc);
		}
		txt = txt+strlen(txt);
		
		memset (&dpd, 0, sizeof (dpd));
		ptp_getdevicepropdesc (params, dpc, &dpd);

		sprintf (txt, "(%s) ",_get_getset(dpd.GetSet));txt += strlen (txt);
		sprintf (txt, "(type=0x%x) ",dpd.DataType);txt += strlen (txt);
		switch (dpd.FormFlag) {
		case PTP_DPFF_None:	break;
		case PTP_DPFF_Range: {
			sprintf (txt, "Range [");txt += strlen(txt);
			_value_to_str (&dpd.FORM.Range.MinimumValue, dpd.DataType, txt);
			txt += strlen(txt);
			sprintf (txt, " - ");txt += strlen(txt);
			_value_to_str (&dpd.FORM.Range.MaximumValue, dpd.DataType, txt);
			txt += strlen(txt);
			sprintf (txt, ", step ");txt += strlen(txt);
			_value_to_str (&dpd.FORM.Range.StepSize, dpd.DataType, txt);
			txt += strlen(txt);
			sprintf (txt, "] value: ");txt += strlen(txt);
			txt += strlen(txt);
			break;
		}
		case PTP_DPFF_Enumeration:
			sprintf (txt, "Enumeration [");txt += strlen(txt);
			if ((dpd.DataType & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)  {
				sprintf (txt, "\n\t");txt += strlen(txt);
			}
			for (j = 0; j<dpd.FORM.Enum.NumberOfValues; j++) {
				_value_to_str(dpd.FORM.Enum.SupportedValue+j,dpd.DataType,txt);
				txt += strlen(txt);
				if (j != dpd.FORM.Enum.NumberOfValues-1) {
					sprintf (txt, ",");txt += strlen(txt);
					if ((dpd.DataType & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)  {
						sprintf (txt, "\n\t");txt += strlen(txt);
					}
				}
			}
			if ((dpd.DataType & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)  {
				sprintf (txt, "\n\t");txt += strlen(txt);
			}
			sprintf (txt, "] value: ");txt += strlen(txt);
			break;
		}
		txt += strlen (txt);
		ptp_render_property_value(params, dpc, &dpd, sizeof(summary->text) - strlen(summary->text) - 1, txt);
		if (strlen(txt)) {
			txt += strlen (txt);
			sprintf(txt, " (");
			txt+=2;
			_value_to_str (&dpd.CurrentValue, dpd.DataType, txt);
			txt += strlen (txt);
			sprintf(txt, ")");
			txt++;
		} else {
			_value_to_str (&dpd.CurrentValue, dpd.DataType, txt);
		}
		txt += strlen(txt);
		sprintf(txt,"\n");
		txt += strlen(txt);
        }
	return (GP_OK);
}

static int
have_prop(Camera *camera, uint16_t vendor, uint16_t prop) {
	int i;

	/* prop 0 matches */
	if (!prop && (camera->pl->params.deviceinfo.VendorExtensionID==vendor))
		return 1;

	for (i=0; i<camera->pl->params.deviceinfo.DevicePropertiesSupported_len; i++) {
		if (prop != camera->pl->params.deviceinfo.DevicePropertiesSupported[i])
			continue;
		if ((prop & 0xf000) == 0x5000) /* generic property */
			return 1;
		if (camera->pl->params.deviceinfo.VendorExtensionID==vendor)
			return 1;
	}
	return 0;
}

struct submenu;
typedef int (*get_func)(Camera *camera, CameraWidget **widget, struct submenu* menu, PTPDevicePropDesc *dpd);
typedef int (*put_func)(Camera *camera, CameraWidget *widget, PTPPropertyValue *propval);

struct submenu {
	char 		*label;
	char		*name;
	uint16_t	propid;
	uint16_t	vendorid;
	uint16_t	type;
	get_func	getfunc;
	put_func	putfunc;
};

struct menu {
	char		*label;
	char		*name;
	struct	submenu	*submenus;
};

struct deviceproptableu8 {
	char		*label;
	uint8_t		value;
	uint16_t	vendor_id;
};

struct deviceproptableu16 {
	char		*label;
	uint16_t	value;
	uint16_t	vendor_id;
};


static int
_get_AUINT8_as_CHAR_ARRAY(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int	j;
	char 	value[128];

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->DataType != PTP_DTC_AUINT8) {
		sprintf (value,_("unexpected datatype %i"),dpd->DataType);
	} else {
		memset(value,0,sizeof(value));
		for (j=0;j<dpd->CurrentValue.a.count;j++)
			value[j] = dpd->CurrentValue.a.v[j].u8;
	}
	gp_widget_set_value (*widget,value);
	return (GP_OK);
}

static int
_get_STR(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	char value[64];

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->DataType != PTP_DTC_STR) {
		sprintf (value,_("unexpected datatype %i"),dpd->DataType);
		gp_widget_set_value (*widget,value);
	} else {
		gp_widget_set_value (*widget,dpd->CurrentValue.str);
	}
	return (GP_OK);
}

static int
_put_STR(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
        const char *string;
        int ret;
        ret = gp_widget_get_value (widget,&string);
        if (ret != GP_OK)
                return ret;
        propval->str = strdup (string);
        return (GP_OK);
}

static int
_put_AUINT8_as_CHAR_ARRAY(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char	*value;
	int	i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	memset(propval,0,sizeof(PTPPropertyValue));
	/* add \0 ? */
	propval->a.v = malloc((strlen(value)+1)*sizeof(PTPPropertyValue));
	propval->a.count = strlen(value)+1;
	for (i=0;i<strlen(value)+1;i++)
		propval->a.v[i].u8 = value[i];
	return (GP_OK);
}

#if 0
static int
_get_Range_INT8(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	float CurrentValue;
	
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name ( *widget, menu->name);
	if (dpd->FormFlag != PTP_DPFF_Range)
		return (GP_ERROR_NOT_SUPPORTED);
	if (dpd->DataType != PTP_DTC_INT8)
		return (GP_ERROR_NOT_SUPPORTED);
	CurrentValue = (float) dpd->CurrentValue.i8;
	gp_widget_set_range ( *widget, (float) dpd->FORM.Range.MinimumValue.i8, (float) dpd->FORM.Range.MaximumValue.i8, (float) dpd->FORM.Range.StepSize.i8);
	gp_widget_set_value ( *widget, &CurrentValue);
	return (GP_OK);
}


static int
_put_Range_INT8(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	int ret;
	float f;
	ret = gp_widget_get_value (widget, &f);
	if (ret != GP_OK) 
		return ret;
	propval->i8 = (int) f;
	return (GP_OK);
}
#endif

static int
_get_Nikon_OnOff_UINT8(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name ( *widget, menu->name);
	if (dpd->FormFlag != PTP_DPFF_Range)
                return (GP_ERROR_NOT_SUPPORTED);
        if (dpd->DataType != PTP_DTC_UINT8)
                return (GP_ERROR_NOT_SUPPORTED);
	gp_widget_add_choice (*widget,_("On"));
	gp_widget_add_choice (*widget,_("Off"));
	gp_widget_set_value ( *widget, (dpd->CurrentValue.u8?_("On"):_("Off")));
	return (GP_OK);
}

static int
_put_Nikon_OnOff_UINT8(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	int ret;
	char *value;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK) 
		return ret;
	if(!strcmp(value,_("On"))) {
		propval->u8 = 1;
		return (GP_OK);
	}
	if(!strcmp(value,_("Off"))) {
		propval->u8 = 0;
		return (GP_OK);
	}
	return (GP_ERROR);
}

static int
_get_UINT32_as_MB(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	char value[64];

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->DataType != PTP_DTC_UINT32) {
		sprintf (value,_("unexpected datatype %i"),dpd->DataType);
	} else {
		sprintf (value,"%i",dpd->CurrentValue.u32/1024/1024);
	}
	gp_widget_set_value (*widget,value);
	return (GP_OK);
}

static struct deviceproptableu16 whitebalance[] = {
	{ N_("Manual"), 0x0001, 0 },
	{ N_("Automatic"), 0x0002, 0 },
	{ N_("One-push Automatic"), 0x0003, 0 },
	{ N_("Daylight"), 0x0004, 0 },
	{ N_("Fluorescent"), 0x0005, 0 },
	{ N_("Tungsten"), 0x0006, 0 },
	{ N_("Flash"), 0x0007, 0 },
	{ N_("Cloudy"), 0x8010, PTP_VENDOR_NIKON },
	{ N_("Shade"), 0x8011, PTP_VENDOR_NIKON },
	{ N_("Preset"), 0x8013, PTP_VENDOR_NIKON },
	{ NULL },
};


static int
_get_WhiteBalance (Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i, j, value=-1;
	gp_widget_new (GP_WIDGET_MENU, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return(GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return(GP_ERROR);
	/* Run through all the supported values and all the values known to
	   the camera, and see which values that matches */
	for (j=0;j<dpd->FORM.Enum.NumberOfValues; j++) {
		for (i=0;whitebalance[i].label;i++) {
			if ((dpd->FORM.Enum.SupportedValue[j].u16 == whitebalance[i].value) && ((whitebalance[i].vendor_id == 0) || (whitebalance[i].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))) {
				gp_widget_add_choice (*widget, _(whitebalance[i].label));
				/* Might as well take note here if this value
				is the one currently set. */
				if (dpd->CurrentValue.u16 == whitebalance[i].value)
					value = i;
				/* Make sure at least a known value is set
				   as chosen initially */
				if (value==-1)
					value = i;
			}
		}
	}
	if (value>=0)
		gp_widget_set_value ( *widget, _(whitebalance[value].label));
	return GP_OK;
}

static int
_put_WhiteBalance(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	int i;
	char *value;
	int ret;
	int retval = -1;

	ret = gp_widget_get_value (widget, &value);
	if(ret != GP_OK)
		return ret;
	for (i=0; (whitebalance[i].label && (retval<0));i++) {
		if (!strcmp ( _(whitebalance[i].label),value))
			retval = i;
	}
	if (retval>=0) 
		propval->u16 = whitebalance[retval].value;
	else
		return GP_ERROR;
	return GP_OK;
}
	
/* Everything is camera specific. */
static struct deviceproptableu8 compression[] = {
	{ N_("JPEG Basic"), 0x00, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal"), 0x01, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine"), 0x02, PTP_VENDOR_NIKON },
	{ N_("NEF (Raw)"), 0x04, PTP_VENDOR_NIKON },
	{ N_("NEF+BASIC"), 0x05, PTP_VENDOR_NIKON },
	{ NULL },
};

static int
_get_Compression (Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i, j, value=-1;
	gp_widget_new (GP_WIDGET_MENU, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return(GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return(GP_ERROR);
	/* Run through all the supported values and all the values known to
	   the camera, and see which values that matches */
	for (j=0;j<dpd->FORM.Enum.NumberOfValues; j++) {
		for (i=0;compression[i].label;i++) {
			if ((dpd->FORM.Enum.SupportedValue[j].u8 == compression[i].value) && ((compression[i].vendor_id == 0) || (compression[i].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))) {
				gp_widget_add_choice (*widget, _(compression[i].label));
				/* Might as well take note here if this value
				is the one currently set. */
				if (dpd->CurrentValue.u8 == compression[i].value)
					value = i;
				/* Make sure at least a known value is set
				   as chosen initially */
				if (value==-1)
					value = i;
			}
		}
	}
	if (value>=0)
		gp_widget_set_value ( *widget, _(compression[value].label));
	return GP_OK;
}

static int
_put_Compression(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	int i;
	char *value;
	int ret;
	int retval = -1;

	ret = gp_widget_get_value (widget, &value);
	if(ret != GP_OK)
		return ret;
	for (i=0; (compression[i].label && (retval<0));i++) {
		if (!strcmp ( _(compression[i].label),value))
			retval = i;
	}
	if (retval>=0) 
		propval->u8 = compression[retval].value;
	else
		return GP_ERROR;
	return GP_OK;
}

static int
_get_ImageSize(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
        int j;

        gp_widget_new (GP_WIDGET_MENU, _(menu->label), widget);
        gp_widget_set_name (*widget, menu->name);
        if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
                return(GP_ERROR);
        if (dpd->DataType != PTP_DTC_STR)
                return(GP_ERROR);
        for (j=0;j<dpd->FORM.Enum.NumberOfValues; j++) {
                gp_widget_add_choice (*widget,dpd->FORM.Enum.SupportedValue[j].str);
        }
        gp_widget_set_value (*widget,dpd->CurrentValue.str);
        return GP_OK;
}

static int
_put_ImageSize(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
        char *value;
        int ret;

        ret = gp_widget_get_value (widget,&value);
        if(ret != GP_OK)
                return ret;
        propval->str = strdup (value);
        return(GP_OK);
}


static int
_get_Canon_AssistLight(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	gp_widget_add_choice (*widget,_("On"));
	gp_widget_add_choice (*widget,_("Off"));
	gp_widget_set_value (*widget,(dpd->CurrentValue.u16?_("On"):_("Off")));
	return (GP_OK);
}

static int
_put_Canon_AssistLight(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	if (!strcmp (value, _("On"))) {
		propval->u16 = 1;
		return (GP_OK);
	}
	if (!strcmp (value, _("Off"))) {
		propval->u16 = 0;
		return (GP_OK);
	}
	return (GP_ERROR);
}

static int
_get_Canon_BeepMode(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	gp_widget_add_choice (*widget,_("On"));
	gp_widget_add_choice (*widget,_("Off"));
	gp_widget_set_value (*widget,(dpd->CurrentValue.u8?_("On"):_("Off")));
	return (GP_OK);
}

static int
_put_Canon_BeepMode(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	if (!strcmp (value, _("On"))) {
		propval->u8 = 1;
		return (GP_OK);
	}
	if (!strcmp (value, _("Off"))) {
		propval->u8 = 0;
		return (GP_OK);
	}
	return (GP_ERROR);
}

static int
_get_Canon_ZoomRange(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	float	f, t, b, s;

	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	f = (float)dpd->CurrentValue.u16;
	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	b = (float)dpd->FORM.Range.MinimumValue.u16;
	t = (float)dpd->FORM.Range.MaximumValue.u16;
	s = (float)dpd->FORM.Range.StepSize.u16;
	gp_widget_set_range (*widget, b, t, s);
	gp_widget_set_value (*widget, &f);
	return (GP_OK);
}

static int
_put_Canon_ZoomRange(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval)
{
	float	f;
	int	ret;

	f = 0.0;
	ret = gp_widget_get_value (widget,&f);
	if (ret != GP_OK) return ret;
	propval->u16 = (unsigned short)f;
	return (GP_OK);
}

static struct deviceproptableu8 canon_quality[] = {
	{ N_("normal"),		0x02, 0 },
	{ N_("fine"),		0x03, 0 },
	{ N_("super fine"),	0x05, 0 },
	{ NULL },
};

static int
_get_Canon_Quality(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_quality)/sizeof (canon_quality[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_quality[i].label));
		if (canon_quality[i].value == dpd->CurrentValue.u8)
			gp_widget_set_value (*widget, _(canon_quality[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_Quality(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_quality)/sizeof (canon_quality[0]);i++) {
		if (!strcmp (value, _(canon_quality[i].label))) {
			propval->u8 = canon_quality[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu8 canon_flash[] = {
	{ N_("off"),	0, 0 },
	{ N_("auto"),	1, 0 },
	{ N_("on"),	2, 0 },
	{ N_("auto red eye"), 5, 0 },
	{ N_("on red eye"), 6, 0 },
};

static int
_get_Canon_FlashMode(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_flash)/sizeof (canon_flash[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_flash[i].label));
		if (canon_flash[i].value == dpd->CurrentValue.u8)
			gp_widget_set_value (*widget, _(canon_flash[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_FlashMode(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_flash)/sizeof (canon_flash[0]);i++) {
		if (!strcmp (value, _(canon_flash[i].label))) {
			propval->u8 = canon_flash[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu8 canon_meteringmode[] = {
	{ "center weighted(?)",	0, 0 },
	{ N_("spot"),		1, 0 },
	{ "integral(?)",	3, 0 },
};

static int
_get_Canon_MeteringMode(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_meteringmode)/sizeof (canon_meteringmode[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_meteringmode[i].label));
		if (canon_meteringmode[i].value == dpd->CurrentValue.u8)
			gp_widget_set_value (*widget, _(canon_meteringmode[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_MeteringMode(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_meteringmode)/sizeof (canon_meteringmode[0]);i++) {
		if (!strcmp (value, _(canon_meteringmode[i].label))) {
			propval->u8 = canon_meteringmode[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu16 canon_shutterspeed[] = {
      {  "auto",0x0000,0 },
      {  "15\"",0x0018,0 },
      {  "13\"",0x001b,0 },
      {  "10\"",0x001d,0 },
      {  "8\"",0x0020,0 },
      {  "6\"",0x0023,0 },
      {  "5\"",0x0025,0 },
      {  "4\"",0x0028,0 },
      {  "3\"2",0x002b,0 },
      {  "2\"5",0x002d,0 },
      {  "2\"",0x0030,0 },
      {  "1\"6",0x0033,0 },
      {  "1\"3",0x0035,0 },
      {  "1\"",0x0038,0 },
      {  "0\"8",0x003b,0 },
      {  "0\"6",0x003d,0 },
      {  "0\"5",0x0040,0 },
      {  "0\"4",0x0043,0 },
      {  "0\"3",0x0045,0 },
      {  "1/4",0x0048,0 },
      {  "1/5",0x004b,0 },
      {  "1/6",0x004d,0 },
      {  "1/8",0x0050,0 },
      {  "1/10",0x0053,0 },
      {  "1/13",0x0055,0 },
      {  "1/15",0x0058,0 },
      {  "1/20",0x005b,0 },
      {  "1/25",0x005d,0 },
      {  "1/30",0x0060,0 },
      {  "1/40",0x0063,0 },
      {  "1/50",0x0065,0 },
      {  "1/60",0x0068,0 },
      {  "1/80",0x006b,0 },
      {  "1/100",0x006d,0 },
      {  "1/125",0x0070,0 },
      {  "1/160",0x0073,0 },
      {  "1/200",0x0075,0 },
      {  "1/250",0x0078,0 },
      {  "1/320",0x007b,0 },
      {  "1/400",0x007d,0 },
      {  "1/500",0x0080,0 },
      {  "1/640",0x0083,0 },
      {  "1/800",0x0085,0 },
      {  "1/1000",0x0088,0 },
      {  "1/1250",0x008b,0 },
      {  "1/1600",0x008d,0 },
      {  "1/2000",0x0090,0 },
};

static int
_get_Canon_ShutterSpeed(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_shutterspeed)/sizeof (canon_shutterspeed[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_shutterspeed[i].label));
		if (canon_shutterspeed[i].value == dpd->CurrentValue.u16)
			gp_widget_set_value (*widget, _(canon_shutterspeed[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_ShutterSpeed(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_shutterspeed)/sizeof (canon_shutterspeed[0]);i++) {
		if (!strcmp (value, _(canon_shutterspeed[i].label))) {
			propval->u16 = canon_shutterspeed[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu16 canon_focuspoints[] = {
	{ N_("center"),	0x3003, 0 },
	{ N_("auto"),	0x3001, 0 },
};

static int
_get_Canon_FocusingPoint(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_focuspoints)/sizeof (canon_focuspoints[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_focuspoints[i].label));
		if (canon_focuspoints[i].value == dpd->CurrentValue.u16)
			gp_widget_set_value (*widget, _(canon_focuspoints[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_FocusingPoint(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_focuspoints)/sizeof (canon_focuspoints[0]);i++) {
		if (!strcmp (value, _(canon_focuspoints[i].label))) {
			propval->u16 = canon_focuspoints[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu8 canon_size[] = {
	{ N_("large"),		0x00, 0 },
	{ N_("medium 1"),	0x01, 0 },
	{ N_("medium 2"),	0x03, 0 },
	{ N_("small"),		0x02, 0 },
	{ NULL },
};

static int
_get_Canon_Size(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_size)/sizeof (canon_size[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_size[i].label));
		if (canon_size[i].value == dpd->CurrentValue.u8)
			gp_widget_set_value (*widget, _(canon_size[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_Size(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_size)/sizeof (canon_size[0]);i++) {
		if (!strcmp (value, _(canon_size[i].label))) {
			propval->u8 = canon_size[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu16 canon_isospeed[] = {
	{ N_("Factory Default"),0xffff, 0 },
	{ "50",			0x0040, 0 },
	{ "100",		0x0048, 0 },
	{ "200",		0x0050, 0 },
	{ "400",		0x0058, 0 },
	{ N_("Auto"),		0x0000, 0 },
};

static int
_get_Canon_ISO(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_isospeed)/sizeof (canon_isospeed[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_isospeed[i].label));
		if (canon_isospeed[i].value == dpd->CurrentValue.u16)
			gp_widget_set_value (*widget, _(canon_isospeed[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_ISO(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_isospeed)/sizeof (canon_isospeed[0]);i++) {
		if (!strcmp (value, _(canon_isospeed[i].label))) {
			propval->u16 = canon_isospeed[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu8 canon_macromode[] = {
	{ N_("Off"),		0x01, 0 },
	{ N_("Macro"),		0x03, 0 },
	{ N_("Long distance"),	0x07, 0 }, /* Unchecked. */
};

static int
_get_Canon_Macro(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_macromode)/sizeof (canon_macromode[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_macromode[i].label));
		if (canon_macromode[i].value == dpd->CurrentValue.u8)
			gp_widget_set_value (*widget, _(canon_macromode[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_Macro(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_macromode)/sizeof (canon_macromode[0]);i++) {
		if (!strcmp (value, _(canon_macromode[i].label))) {
			propval->u8 = canon_macromode[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu8 canon_whitebalance[] = {
      { N_("Auto"),		0, 0 },
      { N_("Daylight"),		1, 0 },
      { N_("Cloudy"),		2, 0 },
      { N_("Tungsten"),		3, 0 },
      { N_("Fluorescent"),	4, 0 },
      { N_("Fluorescent H"),	7, 0 },
      { N_("Custom"),		6, 0 },
};

static int
_get_Canon_WhiteBalance(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_whitebalance)/sizeof (canon_whitebalance[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_whitebalance[i].label));
		if (canon_whitebalance[i].value == dpd->CurrentValue.u8)
			gp_widget_set_value (*widget, _(canon_whitebalance[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_WhiteBalance(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_whitebalance)/sizeof (canon_whitebalance[0]);i++) {
		if (!strcmp (value, _(canon_whitebalance[i].label))) {
			propval->u8 = canon_whitebalance[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu8 canon_expcompensation[] = {
      { N_("Factory Default"),	0xff, 0 },
      { "+2",			0x08, 0 },
      { "+1 2/3",		0x0b, 0 },
      { "+1 1/3",		0x0d, 0 },
      { "+1",			0x10, 0 },
      { "+2/3",			0x13, 0 },
      { "+1/3",			0x15, 0 },
      { "0",			0x18, 0 },
      { "-1/3",			0x1b, 0 },
      { "-2/3",			0x1d, 0 },
      { "-1",			0x20, 0 },
      { "-1 1/3",		0x23, 0 },
      { "-1 2/3",		0x25, 0 },
      { "-2",			0x28, 0 },
};

static int
_get_Canon_ExpCompensation(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_expcompensation)/sizeof (canon_expcompensation[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_expcompensation[i].label));
		if (canon_expcompensation[i].value == dpd->CurrentValue.u8)
			gp_widget_set_value (*widget, _(canon_expcompensation[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_ExpCompensation(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_expcompensation)/sizeof (canon_expcompensation[0]);i++) {
		if (!strcmp (value, _(canon_expcompensation[i].label))) {
			propval->u8 = canon_expcompensation[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu16 canon_photoeffect[] = {
      { N_("Off"),		0, 0 },
      { N_("Vivid"),		1, 0 },
      { N_("Neutral"),		2, 0 },
      { N_("Low sharpening"),	3, 0 },
      { N_("Sepia"),		4, 0 },
      { N_("Black & white"),	5, 0 },
};

static int
_get_Canon_PhotoEffect(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_photoeffect)/sizeof (canon_photoeffect[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_photoeffect[i].label));
		if (canon_photoeffect[i].value == dpd->CurrentValue.u16)
			gp_widget_set_value (*widget, _(canon_photoeffect[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_PhotoEffect(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_photoeffect)/sizeof (canon_photoeffect[0]);i++) {
		if (!strcmp (value, _(canon_photoeffect[i].label))) {
			propval->u16 = canon_photoeffect[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static struct deviceproptableu16 canon_aperture[] = {
      { N_("auto"),	0xffff, 0 },
      { "2.8",		0x0020, 0 },
      { "3.2",		0x0023, 0 },
      { "3.5",		0x0025, 0 },
      { "4.0",		0x0028, 0 },
      { "4.5",		0x002b, 0 },
      { "5",		0x002d, 0 },
      { "5.6",		0x0030, 0 },
      { "6.3",		0x0033, 0 },
      { "7.1",		0x0035, 0 },
      { "8.0",		0x0038, 0 },
};

static int
_get_Canon_Aperture(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	for (i=0;i<sizeof (canon_aperture)/sizeof (canon_aperture[0]);i++) {
		gp_widget_add_choice (*widget, _(canon_aperture[i].label));
		if (canon_aperture[i].value == dpd->CurrentValue.u16)
			gp_widget_set_value (*widget, _(canon_aperture[i].label));
	}
	return (GP_OK);
}

static int
_put_Canon_Aperture(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	char *value;
	int i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof (canon_aperture)/sizeof (canon_aperture[0]);i++) {
		if (!strcmp (value, _(canon_aperture[i].label))) {
			propval->u16 = canon_aperture[i].value;
			return (GP_OK);
		}
	}
	return (GP_ERROR);
}

static int
_get_UINT32_as_time(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	time_t	camtime;

	gp_widget_new (GP_WIDGET_DATE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	camtime = dpd->CurrentValue.u32;
	gp_widget_set_value (*widget,&camtime);
	return (GP_OK);
}

static int
_put_UINT32_as_time(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	time_t	camtime;
	int	ret;

	camtime = 0;
	ret = gp_widget_get_value (widget,&camtime);
	if (ret != GP_OK)
		return ret;
	propval->u32 = camtime;
	return (GP_OK);
}

static int
_get_STR_as_time(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	time_t		camtime;
	struct tm	tm;
	char		capture_date[64],tmp[5];

	/* strptime() is not widely accepted enough to use yet */
	memset(&tm,0,sizeof(tm));
	gp_widget_new (GP_WIDGET_DATE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	strncpy(capture_date,dpd->CurrentValue.str,sizeof(capture_date));
	strncpy (tmp, capture_date, 4);
	tmp[4] = 0;
	tm.tm_year=atoi (tmp) - 1900;
	strncpy (tmp, capture_date + 4, 2);
	tmp[2] = 0;
	tm.tm_mon = atoi (tmp) - 1;
	strncpy (tmp, capture_date + 6, 2);
	tmp[2] = 0;
	tm.tm_mday = atoi (tmp);
	strncpy (tmp, capture_date + 9, 2);
	tmp[2] = 0;
	tm.tm_hour = atoi (tmp);
	strncpy (tmp, capture_date + 11, 2);
	tmp[2] = 0;
	tm.tm_min = atoi (tmp);
	strncpy (tmp, capture_date + 13, 2);
	tmp[2] = 0;
	tm.tm_sec = atoi (tmp);
	camtime = mktime(&tm);
	gp_widget_set_value (*widget,&camtime);
	return (GP_OK);
}

static int
_put_STR_as_time(Camera* camera, CameraWidget *widget, PTPPropertyValue *propval) {
	time_t		camtime;
#ifdef HAVE_GMTIME_R
	struct tm	xtm;
#endif
	struct tm	*pxtm;
	int		ret;
	char		asctime[64];

	camtime = 0;
	ret = gp_widget_get_value (widget,&camtime);
	if (ret != GP_OK)
		return ret;
#ifdef HAVE_GMTIME_R
	pxtm = gmtime_r (&camtime, &xtm);
#else
	pxtm = gmtime (&camtime);
#endif
	/* 20020101T123400.0 is returned by the HP Photosmart */
	sprintf(asctime,"%04d%02d%02dT%02d%02d%02d.0",pxtm->tm_year+1900,pxtm->tm_mon+1,pxtm->tm_mday,pxtm->tm_hour,pxtm->tm_min,pxtm->tm_sec);
	propval->str = strdup(asctime);
	return (GP_OK);
}

static int
_put_None(Camera* camera, CameraWidget *widget, PTPPropertyValue *dpd) {
	return (GP_ERROR_NOT_SUPPORTED);
}

static int
_get_Canon_CaptureMode(Camera* camera, CameraWidget **widget, struct submenu *menu, PTPDevicePropDesc *dpd) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	val = 0;
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Canon_CaptureMode(Camera* camera, CameraWidget *widget, PTPPropertyValue *dpd) {
	int val, ret;

	ret = gp_widget_get_value (widget, &val);
	if (ret != GP_OK)
		return ret;

	if (!EXPERIMENTAL_CANON_CAPTURE)
		return GP_ERROR;

	if (val)
		return camera_prepare_capture (camera, NULL);
	else
		return camera_unprepare_capture (camera, NULL);
}

static struct submenu camera_settings_menu[] = {
	{ N_("Camera Owner"), "owner", PTP_DPC_CANON_CameraOwner, PTP_VENDOR_CANON, PTP_DTC_AUINT8, _get_AUINT8_as_CHAR_ARRAY, _put_AUINT8_as_CHAR_ARRAY },
	{ N_("Camera Model"), "model", PTP_DPC_CANON_CameraModel, PTP_VENDOR_CANON, PTP_DTC_STR, _get_STR, _put_None },
	{ N_("Flash Memory"), "internalmemory", PTP_DPC_CANON_FlashMemory, PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_UINT32_as_MB, _put_None },
	{ N_("Camera Time"),  "time", PTP_DPC_CANON_UnixTime,     PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_UINT32_as_time, _put_UINT32_as_time },
	{ N_("Camera Time"),  "time", PTP_DPC_DateTime,           0,                PTP_DTC_STR, _get_STR_as_time, _put_STR_as_time },
	{ N_("Beep Mode"),  "beep",   PTP_DPC_CANON_BeepMode,     PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_BeepMode, _put_Canon_BeepMode },
        { N_("Image Comment"), "imgcomment", PTP_DPC_NIKON_ImageCommentString, PTP_VENDOR_NIKON, PTP_DTC_STR, _get_STR, _put_STR },

/* virtual */
	{ N_("Capture"), "capture", 0, PTP_VENDOR_CANON, 0, _get_Canon_CaptureMode, _put_Canon_CaptureMode},
	{ NULL },
};

static struct submenu image_settings_menu[] = {
        { N_("Long Exp Noise Reduction"), "longexpnr", PTP_DPC_NIKON_D4LongExposureNoiseReduction, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
        { N_("Image Quality"), "imgquality", PTP_DPC_CompressionSetting, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Compression, _put_Compression},
        { N_("Image Quality"), "imgquality", PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_Quality, _put_Canon_Quality},
        { N_("Image Size"), "imgsize", PTP_DPC_ImageSize, 0, PTP_DTC_STR, _get_ImageSize, _put_ImageSize},
        { N_("Image Size"), "imgsize", PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_Size, _put_Canon_Size},
        { N_("ISO Speed"), "iso", PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_ISO, _put_Canon_ISO},
        { N_("Macro Mode"), "macromode", PTP_DPC_CANON_MacroMode, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_Macro, _put_Canon_Macro},
	{ N_("WhiteBalance"), "whitebalance", PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_WhiteBalance, _put_Canon_WhiteBalance},
	{ N_("Metering Mode"), "meteringmode", PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_MeteringMode, _put_Canon_MeteringMode},
	{ N_("WhiteBalance"), "whitebalance", PTP_DPC_WhiteBalance, 0, PTP_DTC_UINT16, _get_WhiteBalance, _put_WhiteBalance},
	{ N_("Photo Effect"), "photoeffect", PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_PhotoEffect, _put_Canon_PhotoEffect},
	{ N_("Aperture"), "aperture", PTP_DPC_CANON_Aperture, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_Aperture, _put_Canon_Aperture},
	{ N_("Focusing Point"), "focusingpoint", PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_FocusingPoint, _put_Canon_FocusingPoint},
	{ N_("Shutter Speed"), "shutterspeed", PTP_DPC_CANON_ShutterSpeed, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_ShutterSpeed, _put_Canon_ShutterSpeed},
        { NULL },
};

static struct submenu capture_settings_menu[] = {
	{ N_("Zoom"), "zoom", PTP_DPC_CANON_Zoom, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_ZoomRange, _put_Canon_ZoomRange},
	{ N_("Assist Light"), "assistlight", PTP_DPC_CANON_AssistLight, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_AssistLight, _put_Canon_AssistLight},
	{ N_("Exposure Compensation"), "exposurecompensation", PTP_DPC_CANON_ExpCompensation, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_ExpCompensation, _put_Canon_ExpCompensation},
	{ N_("Flash"), "flash", PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_FlashMode, _put_Canon_FlashMode},
	/* { N_("Viewfinder Mode"), "viewfinder", PTP_DPC_CANON_ViewFinderMode, PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_Canon_ViewFinderMode, _put_Canon_ViewFinderMode}, */
	{ NULL },
};

static struct menu menus[] = {
	{ N_("Camera Settings"), "settings", camera_settings_menu },
	{ N_("Image Settings"), "imgsettings", image_settings_menu },
	{ N_("Capture Settings"), "capturesettings", capture_settings_menu },
};

static int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *section, *widget;
	PTPDevicePropDesc dpd;
	int menuno, submenuno;

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera and Driver Configuration"), window);
	gp_widget_set_name (*window, "main");
	for (menuno = 0; menuno < sizeof(menus)/sizeof(menus[0]) ; menuno++ ) {
		gp_widget_new (GP_WIDGET_SECTION, _(menus[menuno].label), &section);
		gp_widget_set_name (section, menus[menuno].name);
		gp_widget_append (*window, section);

		for (submenuno = 0; menus[menuno].submenus[submenuno].name ; submenuno++ ) {
			struct submenu *cursub = menus[menuno].submenus+submenuno;

			if (!have_prop(camera,cursub->vendorid,cursub->propid))
				continue;
			if (cursub->propid) {
				memset(&dpd,0,sizeof(dpd));
				ptp_getdevicepropdesc(&camera->pl->params,cursub->propid,&dpd);
				cursub->getfunc (camera, &widget, cursub, &dpd);
				ptp_free_devicepropdesc(&dpd);
			} else {
				cursub->getfunc (camera, &widget, cursub, NULL);
			}
			gp_widget_append (section, widget);
		}
	}
#if 0
	char value[255];
	if (have_prop(camera,0,PTP_DPC_BatteryLevel)) {
		memset(&dpd,0,sizeof(dpd));
		ptp_getdevicepropdesc(&camera->pl->params,PTP_DPC_BatteryLevel,&dpd);
		GP_DEBUG ("Data Type = 0x%04x",dpd.DataType);
		GP_DEBUG ("Get/Set = 0x%02x",dpd.GetSet);
		GP_DEBUG ("Form Flag = 0x%02x",dpd.FormFlag);
		if (dpd.DataType!=PTP_DTC_UINT8) {
			ptp_free_devicepropdesc(&dpd);
			return GP_ERROR_NOT_SUPPORTED;
		}
		GP_DEBUG ("Factory Default Value = %0.2x",dpd.FactoryDefaultValue.u8);
		GP_DEBUG ("Current Value = %0.2x",dpd.CurrentValue.u8);

		gp_widget_new (GP_WIDGET_SECTION, _("Power (readonly)"), &section);
		gp_widget_append (*window, section);
		switch (dpd.FormFlag) {
		case PTP_DPFF_Enumeration: {
			uint16_t i;
			char tmp[64];

			GP_DEBUG ("Number of values %i",
				dpd.FORM.Enum.NumberOfValues);
			gp_widget_new (GP_WIDGET_TEXT, _("Number of values"),&widget);
			snprintf (value,255,"%i",dpd.FORM.Enum.NumberOfValues);
			gp_widget_set_value (widget,value);
			gp_widget_append (section,widget);
			gp_widget_new (GP_WIDGET_TEXT, _("Supported values"),&widget);
			value[0]='\0';
			for (i=0;i<dpd.FORM.Enum.NumberOfValues;i++){
				snprintf (tmp,6,"|%.3i|",dpd.FORM.Enum.SupportedValue[i].u8);
				strncat(value,tmp,6);
			}
			gp_widget_set_value (widget,value);
			gp_widget_append (section,widget);
			gp_widget_new (GP_WIDGET_TEXT, _("Current value"),&widget);
			snprintf (value,255,"%i",dpd.CurrentValue.u8);
			gp_widget_set_value (widget,value);
			gp_widget_append (section,widget);
			break;
		}
		case PTP_DPFF_Range: {
			float value_float;
			gp_widget_new (GP_WIDGET_RANGE, _("Power (readonly)"), &widget);
			gp_widget_append (section,widget);
			gp_widget_set_range (widget, dpd.FORM.Range.MinimumValue.u8, dpd.FORM.Range.MaximumValue.u8, dpd.FORM.Range.StepSize.u8);
			/* this is a write only capability */
			value_float = dpd.CurrentValue.u8;
			gp_widget_set_value (widget, &value_float);
			gp_widget_changed(widget);
			break;
		}
		case PTP_DPFF_None:
			break;
		default: break;
		}
	}
#endif
	return GP_OK;
}

static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *section, *widget, *subwindow;
	int menuno, submenuno, ret;

	ret = gp_widget_get_child_by_label (window, _("Camera and Driver Configuration"), &subwindow);
	if (ret != GP_OK)
		return ret;
	for (menuno = 0; menuno < sizeof(menus)/sizeof(menus[0]) ; menuno++ ) {
		ret = gp_widget_get_child_by_label (subwindow, _(menus[menuno].label), &section);
		if (ret != GP_OK)
			continue;
		for (submenuno = 0; menus[menuno].submenus[submenuno].label ; submenuno++ ) {
			PTPPropertyValue	propval;

			struct submenu *cursub = menus[menuno].submenus+submenuno;
			if (!have_prop(camera,cursub->vendorid,cursub->propid))
				continue;

			ret = gp_widget_get_child_by_label (section, _(cursub->label), &widget);
			if (ret != GP_OK)
				continue;

			if (!gp_widget_changed (widget))
				continue;

			if (cursub->propid) {
				ret = cursub->putfunc (camera, widget, &propval);
				if (ret == GP_OK)
					ptp_setdevicepropvalue (&camera->pl->params, cursub->propid, &propval, cursub->type);
				ptp_free_devicepropvalue (cursub->type, &propval);
			} else {
				ret = cursub->putfunc (camera, widget, NULL);
			}
		}
	}
	return GP_OK;
}

/* following functions are used for fs testing only */
#if 0
static void
add_dir (Camera *camera, uint32_t parent, uint32_t handle, const char *foldername)
{
	int n;
	n=camera->pl->params.handles.n++;
	camera->pl->params.objectinfo = (PTPObjectInfo*)
		realloc(camera->pl->params.objectinfo,
		sizeof(PTPObjectInfo)*(n+1));
	camera->pl->params.handles.handler[n]=handle;

	camera->pl->params.objectinfo[n].Filename=malloc(strlen(foldername)+1);
	strcpy(camera->pl->params.objectinfo[n].Filename, foldername);
	camera->pl->params.objectinfo[n].ObjectFormat=PTP_OFC_Association;
	camera->pl->params.objectinfo[n].AssociationType=PTP_AT_GenericFolder;
	
	camera->pl->params.objectinfo[n].ParentObject=parent;
}
#endif 

#if 0
static void
move_object_by_handle (Camera *camera, uint32_t parent, uint32_t handle)
{
	int n;

	for (n=0; n<camera->pl->params.handles.n; n++)
		if (camera->pl->params.handles.handler[n]==handle) break;
	if (n==camera->pl->params.handles.n) return;
	camera->pl->params.objectinfo[n].ParentObject=parent;
}
#endif

#if 0
static void
move_object_by_number (Camera *camera, uint32_t parent, int n)
{
	if (n>=camera->pl->params.handles.n) return;
	camera->pl->params.objectinfo[n].ParentObject=parent;
}
#endif

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
    int i;
    
    /*((PTPData *)((Camera *)data)->pl->params.data)->context = context;*/
    
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
    
    /* Get (parent) folder handle omiting storage pseudofolder */
    find_folder_handle(folder,storage,parent,data);
    
    for (i = 0; i < params->handles.n; i++) {
	if (params->objectinfo[i].ParentObject==parent) {
            if (params->objectinfo[i].ObjectFormat != PTP_OFC_Association) {
                if (!ptp_operation_issupported(params,PTP_OC_GetStorageIDs)
                    || (params->objectinfo[i].StorageID == storage)) {
                    CR (gp_list_append (list, params->objectinfo[i].Filename, NULL));
                }
            }
        }
    }
    
    return GP_OK;
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	PTPParams *params = &((Camera *)data)->pl->params;
	int i;
	uint32_t handler,storage;

	/*((PTPData *)((Camera *)data)->pl->params.data)->context = context;*/

	/* add storage pseudofolders in root folder */
	if (!strcmp(folder, "/")) {
		PTPStorageIDs storageids;

		if (ptp_operation_issupported(params,PTP_OC_GetStorageIDs)) {
			CPR (context, ptp_getstorageids(params,
				&storageids));
		} else {
			storageids.n = 1;
			storageids.Storage[0]=0xdeadbeef;
		}
		for (i=0; i<storageids.n; i++) {
			char fname[PTP_MAXSTRLEN];
			PTPStorageInfo storageinfo;
			if ((storageids.Storage[i]&0x0000ffff)==0) continue;
			if (ptp_operation_issupported(params,PTP_OC_GetStorageInfo)) {
				CPR (context, ptp_getstorageinfo(params,
					storageids.Storage[i], &storageinfo));
			}
			snprintf(fname, strlen(STORAGE_FOLDER_PREFIX)+9,
				STORAGE_FOLDER_PREFIX"%08x",
				storageids.Storage[i]);
			CR (gp_list_append (list, fname, NULL));
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
	folder_to_storage(folder,storage);

	/* Get folder handle omiting storage pseudofolder */
	find_folder_handle(folder,storage,handler,data);

	for (i = 0; i < params->handles.n; i++) {
	if (params->objectinfo[i].ParentObject==handler)
	if ((!ptp_operation_issupported(params,PTP_OC_GetStorageIDs)) || 
		(params->objectinfo[i].StorageID == storage))
	if (params->objectinfo[i].ObjectFormat==PTP_OFC_Association &&
		params->objectinfo[i].AssociationType!=PTP_AT_Undefined)
		CR (gp_list_append (list, params->objectinfo[i].Filename, NULL));
	}
	return (GP_OK);
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
	unsigned char * image=NULL;
	uint32_t object_id;
	uint32_t size;
	uint32_t storage;
	PTPObjectInfo * oi;

	((PTPData *) camera->pl->params.data)->context = context;

	if (!strcmp (folder, "/special")) {
		int i;

		for (i=0;i<nrofspecial_files;i++)
			if (!strcmp (special_files[i].name, filename))
				return special_files[i].getfunc (fs, folder, filename, type, file, data, context);
		return (GP_ERROR_BAD_PARAMETERS); /* file not found */
	}

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(filename, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	oi=&camera->pl->params.objectinfo[object_id];

	GP_DEBUG ("Getting file.");
	switch (type) {

	case	GP_FILE_TYPE_EXIF: {
		uint32_t offset;
		uint32_t maxbytes;
		char 	*ximage = NULL;

		/* Check if we have partial downloads. Otherwise we can just hope
		 * upstream downloads the whole image to get EXIF data. */
		if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_GetPartialObject))
			return (GP_ERROR_NOT_SUPPORTED);
		/* could also use Canon partial downloads */
		CPR (context, ptp_getpartialobject (&camera->pl->params,
			camera->pl->params.handles.Handler[object_id],
			0, 10, &ximage));
		image = (unsigned char*)ximage;

		if (!((image[0] == 0xff) && (image[1] == 0xd8))) {	/* SOI */
			free (image);
			return (GP_ERROR_NOT_SUPPORTED);
		}
		if (!((image[2] == 0xff) && (image[3] == 0xe1))) {	/* App0 */
			free (image);
			return (GP_ERROR_NOT_SUPPORTED);
		}
		if (0 != memcmp(image+6, "Exif", 4)) {
			free (image);
			return (GP_ERROR_NOT_SUPPORTED);
		}
		offset = 2;
		maxbytes = (image[4] << 8 ) + image[5];
		free (image);
		ximage = NULL;
		CPR (context, ptp_getpartialobject (&camera->pl->params,
			camera->pl->params.handles.Handler[object_id],
			offset, maxbytes, &ximage));
		CR (gp_file_set_data_and_size (file, ximage, maxbytes));
		break;
	}
	case	GP_FILE_TYPE_PREVIEW: {
		char *ximage = NULL;

		/* If thumb size is 0 then there is no thumbnail at all... */
		if((size=oi->ThumbCompressedSize)==0) return (GP_ERROR_NOT_SUPPORTED);
		CPR (context, ptp_getthumb(&camera->pl->params,
			camera->pl->params.handles.Handler[object_id],
			&ximage));
		CR (gp_file_set_data_and_size (file, ximage, size));
		/* XXX does gp_file_set_data_and_size free() image ptr upon
		   failure?? */
		break;
	}
	default: {
		char *ximage = NULL;

		/* we do not allow downloading unknown type files as in most
		cases they are special file (like firmware or control) which
		sometimes _cannot_ be downloaded. doing so we avoid errors.*/
		if (oi->ObjectFormat == PTP_OFC_Association ||
			(oi->ObjectFormat == PTP_OFC_Undefined &&
				oi->ThumbFormat == PTP_OFC_Undefined))
			return (GP_ERROR_NOT_SUPPORTED);

		size=oi->ObjectCompressedSize;
		CPR (context, ptp_getobject(&camera->pl->params,
			camera->pl->params.handles.Handler[object_id],
			&ximage));
		CR (gp_file_set_data_and_size (file, ximage, size));
		/* XXX does gp_file_set_data_and_size free() image ptr upon
		   failure?? */
		break;
	}
	}
	CR (set_mimetype (camera, file, oi->ObjectFormat));

	return (GP_OK);
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, CameraFile *file,
		void *data, GPContext *context)
{
	Camera *camera = data;
	PTPObjectInfo oi;
	const char *filename;
	char *object;
	uint32_t parent;
	uint32_t storage;
	uint32_t handle;
	long int intsize;
	uint32_t size;
	PTPParams* params=&camera->pl->params;

	((PTPData *) camera->pl->params.data)->context = context;

	if (!strcmp (folder, "/special")) {
		int i;

		for (i=0;i<nrofspecial_files;i++)
			if (!strcmp (special_files[i].name, filename))
				return special_files[i].putfunc (fs, folder, file, data, context);
		return (GP_ERROR_BAD_PARAMETERS); /* file not found */
	}

	memset(&oi, 0, sizeof (PTPObjectInfo));
	gp_file_get_name (file, &filename); 
	gp_file_get_data_and_size (file, (const char **)&object, &intsize);
	size=(uint32_t)intsize;

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* get parent folder id omiting storage pseudofolder */
	find_folder_handle(folder,storage,parent,data);

	/* if you desire to put file to root folder, you have to use 
	 * 0xffffffff instead of 0x00000000 (which means responder decide).
	 */
	if (parent==PTP_HANDLER_ROOT) parent=PTP_HANDLER_SPECIAL;

	oi.Filename=(char *)filename;
	oi.ObjectFormat=get_mimetype(camera, file);
	oi.ObjectCompressedSize=size;
	gp_file_get_mtime(file, &oi.ModificationDate);
	/* if the device is usign PTP_VENDOR_EASTMAN_KODAK extension try
	 * PTP_OC_EK_SendFileObject
	 */
	if ((params->deviceinfo.VendorExtensionID==PTP_VENDOR_EASTMAN_KODAK) &&
		(ptp_operation_issupported(params, PTP_OC_EK_SendFileObject)))
	{
		CPR (context, ptp_ek_sendfileobjectinfo (params, &storage,
			&parent, &handle, &oi));
		CPR (context, ptp_ek_sendfileobject (params, object, size));
	} else if (ptp_operation_issupported(params, PTP_OC_SendObjectInfo)) {
		CPR (context, ptp_sendobjectinfo (params, &storage,
			&parent, &handle, &oi));
		CPR (context, ptp_sendobject (params, object, size));
	} else {
		GP_DEBUG ("The device does not support uploading files!");
		return GP_ERROR_NOT_SUPPORTED;
	}
	/* update internal structures */
	add_object(camera, handle, context);
	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
			const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned long object_id;
	uint32_t storage;

	((PTPData *) camera->pl->params.data)->context = context;

	if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_DeleteObject))
		return GP_ERROR_NOT_SUPPORTED;

	if (!strcmp (folder, "/special"))
		return GP_ERROR_NOT_SUPPORTED;

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(filename, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	CPR (context, ptp_deleteobject(&camera->pl->params,
		camera->pl->params.handles.Handler[object_id],0));

	/* On Canon firmwares, a DeleteObject causes a ObjectRemoved event
	 * to be sent. At least on Digital IXUS II and PowerShot A85.
	 */
	if ((camera->pl->params.deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
	    ptp_event_issupported(&camera->pl->params, PTP_EC_ObjectRemoved)) {
		PTPContainer event;
		int ret;

		do {
			ret = ptp_usb_event_wait (&camera->pl->params, &event);
			if (	(ret == PTP_RC_OK) &&
				(event.Code == PTP_EC_ObjectRemoved)
			)
				break;
		} while (ret == PTP_RC_OK);
 	}
	return (GP_OK);
}

static int
remove_dir_func (CameraFilesystem *fs, const char *folder,
			const char *foldername, void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned long object_id;
	uint32_t storage;

	((PTPData *) camera->pl->params.data)->context = context;

	if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_DeleteObject))
		return GP_ERROR_NOT_SUPPORTED;

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(foldername, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	CPR (context, ptp_deleteobject(&camera->pl->params,
		camera->pl->params.handles.Handler[object_id],0));

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

	((PTPData *) camera->pl->params.data)->context = context;
	
	if (!strcmp (folder, "/special"))
		return (GP_ERROR_BAD_PARAMETERS); /* for now */

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(filename, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	oi=&camera->pl->params.objectinfo[object_id];

	info->file.fields = GP_FILE_INFO_SIZE|GP_FILE_INFO_TYPE|GP_FILE_INFO_MTIME;
	if (oi->Filename) {
		strcpy(info->file.name, oi->Filename);
		info->file.fields |= GP_FILE_INFO_NAME;
	}
	info->file.size   = oi->ObjectCompressedSize;
	strcpy_mime (info->file.type, oi->ObjectFormat);
	if (oi->ModificationDate != 0) {
		info->file.mtime = oi->ModificationDate;
	} else {
		info->file.mtime = oi->CaptureDate;
	}

	/* if object is an image */
	if ((oi->ObjectFormat & 0x0800) != 0) {
		info->preview.fields = GP_FILE_INFO_SIZE|GP_FILE_INFO_WIDTH
				|GP_FILE_INFO_HEIGHT|GP_FILE_INFO_TYPE;
		strcpy_mime(info->preview.type, oi->ThumbFormat);
		info->preview.size   = oi->ThumbCompressedSize;
		info->preview.width  = oi->ThumbPixWidth;
		info->preview.height = oi->ThumbPixHeight;

		info->file.fields = info->file.fields |
				GP_FILE_INFO_WIDTH  |
				GP_FILE_INFO_HEIGHT;

		info->file.width  = oi->ImagePixWidth;
		info->file.height = oi->ImagePixHeight;
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
	PTPParams* params=&camera->pl->params;

	if (!strcmp (folder, "/special"))
		return GP_ERROR_NOT_SUPPORTED;

	((PTPData *) camera->pl->params.data)->context = context;
	memset(&oi, 0, sizeof (PTPObjectInfo));

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* get parent folder id omiting storage pseudofolder */
	find_folder_handle(folder,storage,parent,data);

	/* if you desire to make dir in 'root' folder, you have to use
	 * 0xffffffff instead of 0x00000000 (which means responder decide).
	 */
	if (parent==PTP_HANDLER_ROOT) parent=PTP_HANDLER_SPECIAL;

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
	add_object(camera, handle, context);
	return (GP_OK);
}

static int
init_ptp_fs (Camera *camera, GPContext *context)
{
	int i,id,nroot;

	((PTPData *) camera->pl->params.data)->context = context;

	/* Get file handles array for filesystem */
	id = gp_context_progress_start (context, 100, _("Initializing Camera"));
	/* be paranoid!!! */
	memset (&camera->pl->params.handles, 0, sizeof(PTPObjectHandles));
	/* get objecthandles of all objects from all stores */
	CPR (context, ptp_getobjecthandles
	(&camera->pl->params, 0xffffffff, 0x000000, 0x000000,
	&camera->pl->params.handles));

	gp_context_progress_update (context, id, 10);
	/* wee need that for fileststem */
	camera->pl->params.objectinfo =
		(PTPObjectInfo*)malloc(sizeof(PTPObjectInfo)*
		camera->pl->params.handles.n);
	memset (camera->pl->params.objectinfo,0,sizeof(PTPObjectInfo)
		*camera->pl->params.handles.n);
	for (i = 0; i < camera->pl->params.handles.n; i++) {
		CPR (context, ptp_getobjectinfo(&camera->pl->params,
			camera->pl->params.handles.Handler[i],
			&camera->pl->params.objectinfo[i]));
#if 1
		{
		PTPObjectInfo *oi;

		oi=&camera->pl->params.objectinfo[i];
		GP_DEBUG ("ObjectInfo for '%s':", oi->Filename);
		GP_DEBUG ("  Object ID: 0x%08x",
			camera->pl->params.handles.Handler[i]);
		GP_DEBUG ("  StorageID: 0x%08x", oi->StorageID);
		GP_DEBUG ("  ObjectFormat: 0x%04x", oi->ObjectFormat);
		GP_DEBUG ("  ProtectionStatus: 0x%04x", oi->ProtectionStatus);
		GP_DEBUG ("  ObjectCompressedSize: %d",
			oi->ObjectCompressedSize);
		GP_DEBUG ("  ThumbFormat: 0x%04x", oi->ThumbFormat);
		GP_DEBUG ("  ThumbCompressedSize: %d",
			oi->ThumbCompressedSize);
		GP_DEBUG ("  ThumbPixWidth: %d", oi->ThumbPixWidth);
		GP_DEBUG ("  ThumbPixHeight: %d", oi->ThumbPixHeight);
		GP_DEBUG ("  ImagePixWidth: %d", oi->ImagePixWidth);
		GP_DEBUG ("  ImagePixHeight: %d", oi->ImagePixHeight);
		GP_DEBUG ("  ImageBitDepth: %d", oi->ImageBitDepth);
		GP_DEBUG ("  ParentObject: 0x%08x", oi->ParentObject);
		GP_DEBUG ("  AssociationType: 0x%04x", oi->AssociationType);
		GP_DEBUG ("  AssociationDesc: 0x%08x", oi->AssociationDesc);
		GP_DEBUG ("  SequenceNumber: 0x%08x", oi->SequenceNumber);
		}
#endif
		gp_context_progress_update (context, id,
		10+(90*i)/camera->pl->params.handles.n);
	}
	gp_context_progress_stop (context, id);

        if (DCIM_WRONG_PARENT_BUG(camera->pl))
        {
            GP_DEBUG("PTPBUG_DCIM_WRONG_PARENT bug workaround");
            /* Count number of root directory objects */
            nroot = 0;
            for (i = 0; i < camera->pl->params.handles.n; i++) {
		if (camera->pl->params.objectinfo[i].ParentObject == 0) {
                    nroot++;
                }
            }
            
            GP_DEBUG("Found %d root directory objects", nroot);
            
            /* If no root directory objects, look for "DCIM".  This way, we can
             * handle cameras that report the wrong ParentObject ID for root
             */
            if (nroot == 0 && camera->pl->params.handles.n > 0) {
		for (i = 0; i < camera->pl->params.handles.n; i++)
		{
                    PTPObjectInfo *oi = &camera->pl->params.objectinfo[i];
                    
                    if (strcmp(oi->Filename, "DCIM") == 0)
                    {
                        GP_DEBUG("Changing DCIM ParentObject ID from 0x%x to 0",
                                 oi->ParentObject);
                        oi->ParentObject = 0;
                    }
		}
            }
        }
#if 0
	add_dir (camera, 0x00000000, 0xff000000, "DIR1");
	add_dir (camera, 0x00000000, 0xff000001, "DIR20");
	add_dir (camera, 0xff000000, 0xff000002, "subDIR1");
	add_dir (camera, 0xff000002, 0xff000003, "subsubDIR1");
	move_object_by_number (camera, 0xff000002, 2);
	move_object_by_number (camera, 0xff000001, 3);
	move_object_by_number (camera, 0xff000002, 4);
	/* Used for testing with my camera, which does not support subdirs */
#endif
	return (GP_OK);
}


int
camera_init (Camera *camera, GPContext *context)
{
    	CameraAbilities a;
	int ret, i, retried = 0;

	/* Make sure our port is a USB port */
	if (camera->port->type != GP_PORT_USB) {
		gp_context_error (context, _("PTP is implemented for "
			"USB cameras only."));
		return (GP_ERROR_UNKNOWN_PORT);
	}

	camera->functions->about = camera_about;
	camera->functions->exit = camera_exit;
	camera->functions->capture = camera_capture;
	camera->functions->capture_preview = camera_capture_preview;
	camera->functions->summary = camera_summary;
	camera->functions->get_config = camera_get_config;
	camera->functions->set_config = camera_set_config;

	/* We need some data that we pass around */
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	camera->pl->params.sendreq_func=ptp_usb_sendreq;
	camera->pl->params.senddata_func=ptp_usb_senddata;
	camera->pl->params.getresp_func=ptp_usb_getresp;
	camera->pl->params.getdata_func=ptp_usb_getdata;
	camera->pl->params.write_func = ptp_write_func;
	camera->pl->params.read_func  = ptp_read_func;
	camera->pl->params.check_int_func = ptp_check_int;
	camera->pl->params.check_int_fast_func = ptp_check_int_fast;
	camera->pl->params.debug_func = ptp_debug_func;
	camera->pl->params.error_func = ptp_error_func;
	camera->pl->params.data = malloc (sizeof (PTPData));
	memset (camera->pl->params.data, 0, sizeof (PTPData));
	((PTPData *) camera->pl->params.data)->camera = camera;
	camera->pl->params.byteorder = PTP_DL_LE;

        gp_camera_get_abilities(camera, &a);
        for (i = 0; models[i].model; i++) {
            if ((a.usb_vendor == models[i].usb_vendor) &&
                (a.usb_product == models[i].usb_product)){
                camera->pl->bugs = models[i].known_bugs;
                break;
            }
        }


	/* On large fiels (over 50M) deletion takes over 3 seconds,
	 * waiting for event after capture may take some time also
	 */
	CR (gp_port_set_timeout (camera->port, USB_TIMEOUT));
	/* do we configure port ???*/

	/* Establish a connection to the camera */
	((PTPData *) camera->pl->params.data)->context = context;

	retried = 0;
	while (1) {
		ret=ptp_opensession (&camera->pl->params, 1);
		while (ret==PTP_RC_InvalidTransactionID) {
			camera->pl->params.transaction_id+=10;
			ret=ptp_opensession (&camera->pl->params, 1);
		}
		if (ret!=PTP_RC_SessionAlreadyOpened && ret!=PTP_RC_OK) {
			if (retried < 2) { /* try again */
				retried++;
				continue;
			}
			report_result(context, ret);
			return (translate_ptp_result(ret));
		}
		break;
	}

	/* Seems HP does not like getdevinfo outside of session 
	   although it's legal to do so */
	/* get device info */
	CPR(context, ptp_getdeviceinfo(&camera->pl->params,
	&camera->pl->params.deviceinfo));

	GP_DEBUG ("Device info:");
	GP_DEBUG ("Manufacturer: %s",camera->pl->params.deviceinfo.Manufacturer);
	GP_DEBUG ("  model: %s", camera->pl->params.deviceinfo.Model);
	GP_DEBUG ("  device version: %s", camera->pl->params.deviceinfo.DeviceVersion);
	GP_DEBUG ("  serial number: '%s'",camera->pl->params.deviceinfo.SerialNumber);
	GP_DEBUG ("Vendor extension ID: 0x%08x",camera->pl->params.deviceinfo.VendorExtensionID);
	GP_DEBUG ("Vendor extension description: %s",camera->pl->params.deviceinfo.VendorExtensionDesc);
	GP_DEBUG ("Supported operations:");
	for (i=0; i<camera->pl->params.deviceinfo.OperationsSupported_len; i++)
		GP_DEBUG ("  0x%04x",
			camera->pl->params.deviceinfo.OperationsSupported[i]);
	GP_DEBUG ("Events Supported:");
	for (i=0; i<camera->pl->params.deviceinfo.EventsSupported_len; i++)
		GP_DEBUG ("  0x%04x",
			camera->pl->params.deviceinfo.EventsSupported[i]);
	GP_DEBUG ("Device Properties Supported:");
	for (i=0; i<camera->pl->params.deviceinfo.DevicePropertiesSupported_len;
		i++)
		GP_DEBUG ("  0x%04x",
			camera->pl->params.deviceinfo.DevicePropertiesSupported[i]);

	/* init internal ptp objectfiles (required for fs implementation) */
	init_ptp_fs (camera, context);

	switch (camera->pl->params.deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_CANON:
		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_ThemeDownload))
			add_special_file("theme.dat", canon_theme_get, canon_theme_put);
		break;
	case PTP_VENDOR_NIKON:
	/* Veggen: add curve magic command here: 
		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_ThemeDownload))
			add_special_file("curve.ntc", nikon_curve_get, nikon_curve_put);
		break;e
	*/
	default:
		break;
	}

	/* Configure the CameraFilesystem */
	CR (gp_filesystem_set_list_funcs (camera->fs, file_list_func,
					  folder_list_func, camera));
	CR (gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL,
					  camera));
	CR (gp_filesystem_set_file_funcs (camera->fs, get_file_func,
					  delete_file_func, camera));
	CR (gp_filesystem_set_folder_funcs (camera->fs, put_file_func,
					    NULL, make_dir_func,
					    remove_dir_func, camera));
	return (GP_OK);
}
