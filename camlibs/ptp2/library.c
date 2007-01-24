/* library.c
 *
 * Copyright (C) 2001-2005 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2006 Marcus Meissner <marcus@jet.franken.de>
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
#define _BSD_SOURCE
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <langinfo.h>

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

/*
 * On MacOS (Darwin) and *BSD we're not using glibc, but libiconv.
 * glibc knows that UCS-2 is to be in the local machine endianness,
 * whereas libiconv does not. So we construct this macro to get
 * things right. Reportedly, glibc 2.1.3 has a bug so that UCS-2
 * is always bigendian though, we would need to work around that
 * too...
 */
#ifndef __GLIBC__
#define UCS_2_INTERNAL "UCS-2-INTERNAL"
#else
#if (__GLIBC__ == 2 && __GLIBC_MINOR__ <= 1 )
#error "Too old glibc. This versions iconv() implementation cannot be trusted."
#endif
#define UCS_2_INTERNAL "UCS-2"
#endif

#include "ptp.h"
#include "ptp-bugs.h"
#include "ptp-private.h"
#include "ptp-pack.c"

#define GP_MODULE "PTP2"

#define USB_START_TIMEOUT 8000
#define USB_NORMAL_TIMEOUT 20000
#define USB_TIMEOUT_CAPTURE 20000

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

	{PTP_ERROR_IO,		  0, N_("PTP I/O error")},
	{PTP_ERROR_BADPARAM,	  0, N_("PTP Error: bad parameter")},
	{PTP_ERROR_DATA_EXPECTED, 0, N_("PTP Protocol error, data expected")},
	{PTP_ERROR_RESP_EXPECTED, 0, N_("PTP Protocol error, response expected")},
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
	case PTP_ERROR_BADPARAM:
		return (GP_ERROR_BAD_PARAMETERS);
	case PTP_RC_OK:
		return (GP_OK);
	default:
		return (GP_ERROR);
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
	{"Kodak:C330",   0x040a, 0x058c, PTPBUG_DCIM_WRONG_PARENT},
	/* c340 Maurizio Daniele <hayabusa@portalis.it> */
	{"Kodak:C340",   0x040a, 0x058d, 0},
	{"Kodak:V530",   0x040a, 0x058e, 0},
	/* v550 Jon Burgess <jburgess@uklinux.net> */
	{"Kodak:V550",   0x040a, 0x058f, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1618878&group_id=8874 */
	{"Kodak:V570",   0x040a, 0x0591, 0},
	{"Kodak:P850",   0x040a, 0x0592, 0},
	/* https://launchpad.net/distros/ubuntu/+source/libgphoto2/+bug/67532 */
	{"Kodak:C530",   0x040a, 0x059a, 0},
	/* Ivan Baldo, http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=387998 */
	{"Kodak:CD33",   0x040a, 0x059c, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=208874&aid=1565144&group_id=8874 */
	{"Kodak:Z612",   0x040a, 0x059d, 0},
	/* David D. Huff Jr. <David.Huff@computer-critters.com> */
	{"Kodak:Z650",   0x040a, 0x059e, 0},
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
        /* via IRC */
	{"Kodak:C633",   0x040a, 0x05ba, 0},

	/* HP PTP cameras */
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
	{"Sony:MVC-CD300 (PTP mode)", 0x054c, 0x004e, 0},
	{"Sony:MVC-CD500 (PTP mode)", 0x054c, 0x004e, 0},
	{"Sony:DSC-U10 (PTP mode)",   0x054c, 0x004e, 0},

	/* Nikon Coolpix 2500: M. Meissner, 05 Oct 2003 */
	{"Nikon:Coolpix 2500 (PTP mode)", 0x04b0, 0x0109, 0},
	/* Nikon Coolpix 5700: A. Tanenbaum, 29 Oct 2002 */
	{"Nikon:Coolpix 5700 (PTP mode)", 0x04b0, 0x010d, PTP_CAP},
	/* Nikon Coolpix 4500: T. Kaproncai, 22 Aug 2003 */
	{"Nikon Coolpix 4500 (PTP mode)", 0x04b0, 0x010b, 0},
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
	/* Nikon Coolpix 4800 */
	{"Nikon:Coolpix 4800 (PTP mode)", 0x04b0, 0x0129, 0},
	/* Nikon Coolpix SQ: M. Holzbauer, 07 Jul 2003 */
	{"Nikon:Coolpix 4100 (PTP mode)", 0x04b0, 0x012d, 0},
	/* Nikon Coolpix 5600: Andy Shevchenko, 11 Aug 2005 */
	{"Nikon:Coolpix 5600 (PTP mode)", 0x04b0, 0x012e, PTP_CAP},
	/* 4600: Martin Klaffenboeck <martin.klaffenboeck@gmx.at> */
	{"Nikon:Coolpix 4600 (PTP mode)", 0x04b0, 0x0130, 0},
	/* 4600: Roberto Costa <roberto.costa@ensta.org>, 22 Oct 2006 */
	{"Nikon:Coolpix 4600a (PTP mode)", 0x04b0, 0x0131, 0},
	{"Nikon:Coolpix 5900 (PTP mode)", 0x04b0, 0x0135, PTP_CAP},
	{"Nikon:Coolpix P1 (PTP mode)",   0x04b0, 0x0140, PTP_CAP},
	/* Marcus Meissner */
	{"Nikon:Coolpix P2 (PTP mode)",   0x04b0, 0x0142, PTP_CAP},
	/* Lowe, John Michael <jomlowe@iupui.edu> */
	{"Nikon:Coolpix S2 (PTP mode)",   0x04b0, 0x014e, 0},
	{"Nikon:Coolpix SQ (PTP mode)",   0x04b0, 0x0202, 0},
	/* lars marowski bree, 16.8.2004 */
	{"Nikon:Coolpix 4200 (PTP mode)", 0x04b0, 0x0204, 0},
	/* Nikon Coolpix 5200: Andy Shevchenko, 18 Jul 2005 */
	{"Nikon:Coolpix 5200 (PTP mode)", 0x04b0, 0x0206, 0},
	/* https://launchpad.net/bugs/63473 */
	{"Nikon:Coolpix L1 (PTP mode)",   0x04b0, 0x0208, 0},
	/* Nikon Coolpix 2000 */
	{"Nikon:Coolpix 2000 (PTP mode)", 0x04b0, 0x0302, 0},

	/* From IRC reporter. */
	{"Nikon:Coolpix L4 (PTP mode)",   0x04b0, 0x0305, 0},

	/* Nikon D100 has a PTP mode: westin 2002.10.16 */
	{"Nikon:DSC D100 (PTP mode)",     0x04b0, 0x0402, 0},
	/* D2H SLR in PTP mode from Steve Drew <stevedrew@gmail.com> */
	{"Nikon:D2H SLR (PTP mode)",      0x04b0, 0x0404, 0},
	{"Nikon:DSC D70 (PTP mode)",      0x04b0, 0x0406, PTP_CAP},

	/* Justin Case <justin_case@gmx.net> */
	{"Nikon:D2X SLR (PTP mode)",      0x04b0, 0x0408, PTP_CAP},

	/* Niclas Gustafsson (nulleman @ sf) */
	{"Nikon:D50 (PTP mode)",          0x04b0, 0x040a, PTP_CAP},

	{"Nikon:DSC D70s (PTP mode)",     0x04b0, 0x040e, PTP_CAP},
	/* Jana Jaeger <jjaeger.suse.de> */
	{"Nikon:DSC D200 (PTP mode)",     0x04b0, 0x0410, PTP_CAP},
	/* Christian Deckelmann @ SUSE */
	{"Nikon:DSC D80 (PTP mode)",      0x04b0, 0x0412, PTP_CAP},

	/* Sridharan Rengaswamy <sridhar@stsci.edu>, Coolpix L3 */
	{"Nikon:Coolpix L3 (PTP mode)",   0x04b0, 0x041a, 0},

	/* Thomas Luzat <thomas.luzat@gmx.net> */
	{"Panasonic:DMC-FZ20 (alternate id)", 0x04da, 0x2372, 0},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1350226&group_id=8874&atid=208874 */
	{"Panasonic:DMC-LZ2",             0x04da, 0x2372, 0},
	/* https://sourceforge.net/tracker/index.php?func=detail&aid=1405541&group_id=8874&atid=358874 */
	{"Panasonic:DMC-LC1",             0x04da, 0x2372, 0},

	/* Søren Krarup Olesen <sko@acoustics.aau.dk> */
	{"Leica:D-LUX 2",                 0x04da, 0x2375, 0},

	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1275100&group_id=8874&atid=358874 */
	{"Panasonic:Lumix FZ5",           0x04da, 0x2372, 0},

	{"Panasonic:DMC-FZ20",            0x04da, 0x2374, 0},
	/* from Tomas Herrdin <tomas.herrdin@swipnet.se> */
	{"Panasonic:DMC-LS3",             0x04da, 0x2374, 0},

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

	/* IRC report */
	{"Casio:EX-Z120",                 0x07cf, 0x1042, 0},
	/* https://launchpad.net/bugs/64146 */
	{"Casio:EX-Z700",                 0x07cf, 0x104c, 0},

	/* (at least some) newer Canon cameras can be switched between
	 * PTP and "normal" (i.e. Canon) mode
	 * Canon PS G3: A. Marinichev, 20 nov 2002
	 */
	{"Canon:PowerShot S45 (PTP mode)",      0x04a9, 0x306d, PTPBUG_DELETE_SENDS_EVENT},
		/* 0x306c is S45 in normal (canon) mode */
	{"Canon:PowerShot G3 (PTP mode)",       0x04a9, 0x306f, PTPBUG_DELETE_SENDS_EVENT},
		/* 0x306e is G3 in normal (canon) mode */
	{"Canon:PowerShot S230 (PTP mode)",     0x04a9, 0x3071, PTPBUG_DELETE_SENDS_EVENT},
		/* 0x3070 is S230 in normal (canon) mode */
	{"Canon:Digital IXUS v3 (PTP mode)",    0x04a9, 0x3071, PTPBUG_DELETE_SENDS_EVENT},
		/* it's the same as S230 */

	{"Canon:Digital IXUS II (PTP mode)",    0x04a9, 0x3072, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot SD100 (PTP mode)",    0x04a9, 0x3072, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},

	{"Canon:PowerShot A70 (PTP)",           0x04a9, 0x3073, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A60 (PTP)",           0x04a9, 0x3074, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
		/* IXUS 400 has the same PID in both modes, Till Kamppeter */
	{"Canon:Digital IXUS 400 (PTP mode)",   0x04a9, 0x3075, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A300 (PTP mode)",     0x04a9, 0x3076, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot S50 (PTP mode)",      0x04a9, 0x3077, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot G5 (PTP mode)",       0x04a9, 0x3085, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Elura 50 (PTP mode)",           0x04a9, 0x3087, PTPBUG_DELETE_SENDS_EVENT},
		/* 0x3084 is the EOS 300D/Digital Rebel in normal (canon) mode */
	{"Canon:EOS 300D (PTP mode)",           0x04a9, 0x3099, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:EOS Digital Rebel (PTP mode)",  0x04a9, 0x3099, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:EOS Kiss Digital (PTP mode)",   0x04a9, 0x3099, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:PowerShot A80 (PTP)",           0x04a9, 0x309a, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS i (PTP mode)",     0x04a9, 0x309b, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot S1 IS (PTP mode)",    0x04a9, 0x309c, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	/* Canon Elura 65, provolone on #gphoto on 2006-12-17 */
	{"Canon:Elura 65 (PTP mode)",           0x04a9, 0x30a5, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Powershot S70 (PTP mode)",      0x04a9, 0x30b1, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Powershot S60 (PTP mode)",      0x04a9, 0x30b2, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Powershot G6 (PTP mode)",       0x04a9, 0x30b3, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS 500 (PTP mode)",   0x04a9, 0x30b4, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot S500 (PTP mode)",     0x04a9, 0x30b4, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A75 (PTP mode)",      0x04a9, 0x30b5, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot SD110 (PTP mode)",    0x04a9, 0x30b6, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS IIs (PTP mode)",   0x04a9, 0x30b6, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A400 (PTP mode)",     0x04a9, 0x30b7, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A310 (PTP mode)",     0x04a9, 0x30b8, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot A85 (PTP mode)",      0x04a9, 0x30b9, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS 430 (PTP mode)",   0x04a9, 0x30ba, PTPBUG_DELETE_SENDS_EVENT},
 	{"Canon:PowerShot S410 (PTP mode)",     0x04a9, 0x30ba, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
 	{"Canon:PowerShot A95 (PTP mode)",      0x04a9, 0x30bb, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS 40 (PTP mode)",    0x04a9, 0x30bf, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
 	{"Canon:PowerShot SD200 (PTP mode)",    0x04a9, 0x30c0, PTPBUG_DELETE_SENDS_EVENT},
 	{"Canon:Digital IXUS 30 (PTP mode)",    0x04a9, 0x30c0, PTPBUG_DELETE_SENDS_EVENT},
 	{"Canon:PowerShot A520 (PTP mode)",     0x04a9, 0x30c1, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A510 (PTP mode)",     0x04a9, 0x30c2, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:EOS 1D Mark II (PTP mode)",     0x04a9, 0x30ea, PTPBUG_DCIM_WRONG_PARENT},
 	{"Canon:EOS 20D (PTP mode)",            0x04a9, 0x30ec, PTPBUG_DCIM_WRONG_PARENT},
	/* 30ef is the ID in explicit PTP mode.
	 * 30ee is the ID with the camera in Canon mode, but the camera reacts to
	 * PTP commands according to:
	 * https://sourceforge.net/tracker/?func=detail&atid=108874&aid=1394326&group_id=8874
	 * They need to have different names.
	 */
	{"Canon:EOS 350D (PTP mode)",           0x04a9, 0x30ee, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:EOS 350D",                      0x04a9, 0x30ef, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:PowerShot S2 IS (PTP mode)",    0x04a9, 0x30f0, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot SD430 (PTP mode)",    0x04a9, 0x30f1, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS Wireless (PTP mode)",0x04a9, 0x30f1, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:Digital IXUS 700 (PTP mode)",   0x04a9, 0x30f2, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot SD500 (PTP mode)",    0x04a9, 0x30f2, PTPBUG_DELETE_SENDS_EVENT},
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
	{"Canon:Digital IXUS 55 (PTP mode)",    0x04a9, 0x30ff, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot SD450 (PTP mode)",    0x04a9, 0x30ff, PTPBUG_DELETE_SENDS_EVENT},
 	{"Canon:Optura 600 (PTP mode)",         0x04a9, 0x3105, 0},
	/* Jeff Mock <jeff@mock.com> */
 	{"Canon:EOS 5D (PTP mode)",             0x04a9, 0x3101, PTPBUG_DCIM_WRONG_PARENT},
	/* Nick Richards <nick@nedrichards.com> */
	{"Canon:Digital IXUS 50 (PTP mode)",    0x04a9, 0x310e, PTPBUG_DELETE_SENDS_EVENT},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1640547&group_id=8874&atid=358874 */
	{"Canon:PowerShot A420 (PTP mode)",     0x04a9, 0x310f, PTPBUG_DELETE_SENDS_EVENT},
	/* Canon 400D does not have the infamous PTP bug */
	{"Canon:EOS 400D (PTP mode)",           0x04a9, 0x3110, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1456391&group_id=8874 */
	{"Canon:EOS 30D (PTP mode)",            0x04a9, 0x3113, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:Digital IXUS 900Ti (PTP mode)", 0x04a9, 0x3115, PTPBUG_DCIM_WRONG_PARENT},
	{"Canon:Digital IXUS 750 (PTP mode)",   0x04a9, 0x3116, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot A700 (PTP mode)",     0x04a9, 0x3117, PTPBUG_DELETE_SENDS_EVENT},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1498577&group_id=8874&atid=358874 */
	{"Canon:PowerShot SD700 (PTP mode)",    0x04a9, 0x3119, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:Digital IXUS 800 (PTP mode)",   0x04a9, 0x3119, PTPBUG_DELETE_SENDS_EVENT},
	/* Gert Vervoort <gert.vervoort@hccnet.nl> */
	{"Canon:PowerShot S3 IS (PTP mode)",    0x04a9, 0x311a, PTPBUG_DELETE_SENDS_EVENT},
	/* David Goodenough <david.goodenough at linkchoose.co.uk> */
	{"Canon:PowerShot A540 (PTP mode)",     0x04a9, 0x311b, PTPBUG_DELETE_SENDS_EVENT},
	/* Irc reporter */
	{"Canon:Digital IXUS 60 (PTP mode)",    0x04a9, 0x311c, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot SD600 (PTP mode)",    0x04a9, 0x311c, PTPBUG_DELETE_SENDS_EVENT},
	/* Harald Dunkel <harald.dunkel@t-online.de> */
	{"Canon:PowerShot G7 (PTP mode)",	0x04a9, 0x3125, PTPBUG_DELETE_SENDS_EVENT},
	/* Ales Kozumplik <kozumplik@gmail.com> */
	{"Canon:PowerShot A530 (PTP mode)",     0x04a9, 0x3126, PTPBUG_DELETE_SENDS_EVENT},
	/* Jerome Vizcaino <vizcaino_jerome@yahoo.fr> */
	{"Canon:Digital IXUS 850 IS (PTP mode)",0x04a9, 0x3136, PTPBUG_DELETE_SENDS_EVENT},
	/* https://launchpad.net/bugs/64146 */
	{"Canon:PowerShot SD40 (PTP mode)",	0x04a9, 0x3137, PTPBUG_DELETE_SENDS_EVENT},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1565043&group_id=8874&atid=358874 */
	{"Canon:PowerShot A710 IS (PTP mode)",  0x04a9, 0x3138, PTPBUG_DELETE_SENDS_EVENT},
	{"Canon:PowerShot A640 (PTP mode)",     0x04a9, 0x3139, PTPBUG_DELETE_SENDS_EVENT|PTP_CAP|PTP_CAP_PREVIEW},
	{"Canon:PowerShot A630 (PTP mode)",     0x04a9, 0x313a, PTPBUG_DELETE_SENDS_EVENT},



	/* Konica-Minolta PTP cameras */
	{"Konica-Minolta:DiMAGE A2 (PTP mode)",        0x132b, 0x0001, 0},
	{"Konica-Minolta:DiMAGE Z2 (PictBridge mode)", 0x132b, 0x0007, 0},
	{"Konica-Minolta:DiMAGE Z3 (PictBridge mode)", 0x132b, 0x0018, 0},
	{"Konica-Minolta:DiMAGE Z5 (PictBridge mode)", 0x132b, 0x0022, 0},

	/* Fuji PTP cameras */
	{"Fuji:FinePix S7000 (PictBridge mode)",0x04cb, 0x0142, 0},
	{"Fuji:FinePix A330 (PictBridge mode)", 0x04cb, 0x014a, 0},
	{"Fuji:FinePix E900 (PictBridge mode)", 0x04cb, 0x0193, 0},
	{"Fuji:FinePix F30 (PictBridge mode)",  0x04cb, 0x019b, 0},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1620750&group_id=8874 */
	{"Fuji:FinePix S6500fd (PictBridge mode)",0x04cb, 0x01bf, 0},
	/* launchpad 67532 */
	{"Fuji:FinePix F31fd (PictBridge mode)",0x04cb, 0x01c1, 0},

	/* Ricoh Caplio GX */
	{"Ricoh:Caplio GX (PTP mode)",          0x05ca, 0x0325, 0},
	/* Ricoh Caplio R3 */
	{"Ricoh:Caplio R3 (PTP mode)",          0x05ca, 0x032f, 0},

	/* Rollei dr5  */
	{"Rollei:dr5 (PTP mode)",               0x05ca, 0x220f, 0},

	/* Ricoh Caplio GX 8 */
	{"Ricoh:Caplio GX 8 (PTP mode)",        0x05ca, 0x032d, 0},

	/* Pentax cameras */
	{"Pentax:Optio 43WR",                   0x0a17, 0x000d, 0},

	{"Sanyo:VPC-C5 (PTP mode)",             0x0474, 0x0230, 0},

	/* This camera speaks _only_ PictBridge, so it is too limited
	 * for us. -Marcus
	{"Motorola:K1 (PTP mode)",              0x22b8, 0x4811, 0},
	*/

	/************ Add MTP devices below this line ***********/
	/* Jay MacDonald <jay@cheakamus.com> */
	{"iRiver:T10 (alternate)",              0x4102, 0x1113, PTP_MTP},
	/* Andreas Thienemann <andreas@bawue.de> */
	{"iRiver:T20 FM",			0x4102, 0x1114, PTP_MTP},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1620359&group_id=8874 */
	{"iRiver:T20 2",			0x4102, 0x1115, PTP_MTP},
	/* Roger Pixley <skreech2@gmail.com> */
	{"iRiver:U10",                          0x4102, 0x1116, PTP_MTP},
	/* Freaky <freaky@bananateam.nl> */
	{"iRiver:T10",                          0x4102, 0x1117, PTP_MTP},
	/* Martin Senst <martin.senst@gmx.de> */
	{"iRiver:T20",                          0x4102, 0x1118, PTP_MTP},
	/* Bruno Parente Lima <brunoparente77@yahoo.com.br> */
	{"iRiver:T30",                          0x4102, 0x1119, PTP_MTP},
	/* reported by by David Wolpoff */
	{"iRiver:T10 2GB",			0x4102, 0x1120, PTP_MTP},
	/* https://sourceforge.net/tracker/?func=detail&atid=358874&aid=1620359&group_id=8874 */
	{"iRiver:N12",				0x4102, 0x1122, PTP_MTP},
	/* Reported by Adam Torgerson */
	{"iRiver:Clix",				0x4102, 0x112a, PTP_MTP},
	/* Scott Call <scott.call@gmail.com> */
	{"iRiver:H10 20GB",                     0x4102, 0x2101, PTP_MTP},
	/* Petr Spatka spatka@luzanky.cz */
	{"iRiver:H10",                          0x4102, 0x2102, PTP_MTP},
	{"iRiver:Portable Media Center",	0x1006, 0x4002, PTP_MTP},
	{"iRiver:Portable Media Center",	0x1006, 0x4003, PTP_MTP},
	/* From: thanosz@softhome.net */
	{"Philips:HDD6320",			0x0471, 0x01eb, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Philips:HDD6320 2",			0x0471, 0x014b, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Philips:HDD6130/17",			0x0471, 0x014c, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* from XNJB forum */
	{"Philips:GoGear SA9200",		0x0471, 0x014f, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* From Gerhard Mekenkamp */
	{"Philips:GoGear Audio",		0x0471, 0x0165, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* from XNJB user (libmtp) */
	{"Philips:PSA235",			0x0471, 0x7e01, PTP_MTP|PTP_MTP_PROPLIST_WORKS},

	{"Creative:Zen Vision",			0x041e, 0x411f, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Creative:Portable Media Center",	0x041e, 0x4123, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Creative:Zen Xtra",			0x041e, 0x4128, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Second generation Dell DJ",		0x041e, 0x412f, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Creative:Zen Micro",			0x041e, 0x4130, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Creative:Zen Touch",			0x041e, 0x4131, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Dell:Pocket DJ",			0x041e, 0x4132, PTP_MTP},
	{"Creative:Zen Sleek",			0x041e, 0x4137, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* Jennifer Scalf <oneferna@gmail.com> */
	{"Creative:Zen MicroPhoto",             0x041e, 0x413c, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Creative:Zen Sleek Photo",		0x041e, 0x413d, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Creative:Zen Vision:M",		0x041e, 0x413e, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* Reported by marazm@o2.pl */
	{"Creative:Zen V",			0x041e, 0x4150, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* Reported by danielw@iinet.net.au */
	{"Creative:Zen Vision M",		0x041e, 0x4151, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* Reported by Darel on the XNJB forums */
	{"Creative:Zen V plus",			0x041e, 0x4152, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* From Richard Low of the libmtp devteam */
	{"Creative:Zen Vision W",		0x041e, 0x4153, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* The 2 below will not work out of the box, since the autoswitch
	 * to USB Mass Storage once probed.
	 * https://sourceforge.net/tracker/?func=detail&atid=108874&aid=1577793&group_id=8874
	 */
	{"Dunlop:MP3 player 1GB",		0x10d6, 0x2200, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"EGOMAN:MD223AFD",			0x10d6, 0x2200, PTP_MTP|PTP_MTP_PROPLIST_WORKS},

	/* IRC reporter */
	{"Dell:DJ Itty",			0x413c, 0x4500, PTP_MTP|PTP_MTP_PROPLIST_WORKS},

	{"Intel:Bandon Portable Media Center",	0x045e, 0x00c9, PTP_MTP|PTP_MTP_PROPLIST_WORKS},

	/* Marcoen Hirschberg <marcoen@users.sourceforge.net> */
	{"Toshiba:Gigabeat MEGF-40",            0x0930, 0x0009, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Toshiba:Gigabeat",                    0x0930, 0x000c, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Toshiba:Gigabeat S",                  0x0930, 0x0010, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* Reported by Rob Brown */
	{"Toshiba:Gigabeat P10",                0x0930, 0x0011, PTP_MTP|PTP_MTP_PROPLIST_WORKS},

	/* Reported by gudul1@users.sourceforge.net */
	{"Archos 104",				0x0e79, 0x120a, PTP_MTP|PTP_MTP_PROPLIST_WORKS},

	/* From Mark Veinot */
	{"JVC:Alneo XA-HD500", 			0x04f1, 0x6105, PTP_MTP|PTP_MTP_PROPLIST_WORKS},

	/* Contributed by aronvanammers on SourceForge (libmtp) */
	{"Samsung:YH-925GS",			0x04e8, 0x5024, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Samsung:YH-820",			0x04e8, 0x502e, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Samsung:YH-925",			0x04e8, 0x502f, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1474056&group_id=8874&atid=108874 */
	{"Samsung:YP-T7J", 			0x04e8, 0x5047, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* Reported by cstrickler@gmail.com */
	{"Samsung:YP-U2J (YP-U2JXB/XAA)",	0x04e8, 0x5054, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* Reported by Andrew Benson */
	{"Samsung:YP-F2J",			0x04e8, 0x5057, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* Reported by Patrick <skibler@gmail.com> */
	{"Samsung:YP-K5",			0x04e8, 0x505a, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* Reported by Matthew Wilcox <matthew@wil.cx> */
	{"Samsung Yepp T9",			0x04e8, 0x507f, PTP_MTP|PTP_MTP_PROPLIST_WORKS},

	{"Samsung:YH-999 Portable Media Center",0x04e8, 0x5a0f, PTP_MTP|PTP_MTP_PROPLIST_WORKS},

	/* Reported by Brian Robison */
	{"SanDisk:Sansa m240",			0x0781, 0x7400, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* Reported by tangent_@users.sourceforge.net */
	{"SanDisk:Sansa c150",			0x0781, 0x7410, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* http://sourceforge.net/tracker/index.php?func=detail&aid=1515815&group_id=8874&atid=358874 */
	{"Sandisk:Sansa e200",			0x0781, 0x7420, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"Sandisk:Sansa e260",			0x0781, 0x7420, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	{"SanDisk:Sansa c250",			0x0781, 0x7450, PTP_MTP|PTP_MTP_PROPLIST_WORKS},

	/* Reported by Farooq Zaman. NOTE: Can just list files currently and might have other problems! */
	{"Microsoft:Zune",			0x045e, 0x0710, PTP_MTP|PTP_MTP_PROPLIST_WORKS},
	/* From Richard Low */
	{"Sirius:Stiletto",			0x18f6, 0x0102, PTP_MTP|PTP_MTP_PROPLIST_WORKS}
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

	{PTP_OFC_MTP_OGG,		PTP_VENDOR_MICROSOFT, "application/ogg"},
	{PTP_OFC_MTP_FLAC,		PTP_VENDOR_MICROSOFT, "audio/x-flac"},
	{PTP_OFC_MTP_MP2,		PTP_VENDOR_MICROSOFT, "video/mpeg"},
	{PTP_OFC_MTP_M4A,		PTP_VENDOR_MICROSOFT, "audio/x-m4a"},
	{PTP_OFC_MTP_MP4,		PTP_VENDOR_MICROSOFT, "video/mp4"},
	{PTP_OFC_MTP_3GP,		PTP_VENDOR_MICROSOFT, "audio/3gpp"},
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

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; i < sizeof(models)/sizeof(models[0]); i++) {
		memset(&a, 0, sizeof(a));
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port   = GP_PORT_USB;
		a.speed[0] = 0;
		a.usb_vendor = models[i].usb_vendor;
		a.usb_product= models[i].usb_product;
		if (models[i].known_bugs & PTP_MTP) {
			a.operations        = GP_OPERATION_NONE;
			a.device_type       = GP_DEVICE_AUDIO_PLAYER;
			a.file_operations   = GP_FILE_OPERATION_DELETE;
		} else {
			a.device_type       = GP_DEVICE_STILL_CAMERA;
			a.operations        = GP_OPERATION_NONE;
			if (models[i].known_bugs & PTP_CAP)
				a.operations |= GP_OPERATION_CAPTURE_IMAGE | GP_OPERATION_CONFIG;
			if (models[i].known_bugs & PTP_CAP_PREVIEW)
				a.operations |= GP_OPERATION_CAPTURE_PREVIEW;
			a.file_operations   = GP_FILE_OPERATION_PREVIEW |
						GP_FILE_OPERATION_DELETE;
		}
		a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE |
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
	a.operations        = GP_CAPTURE_IMAGE | GP_OPERATION_CONFIG;
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
		/* close iconv converters */
		iconv_close(camera->pl->params.cd_ucs2_to_locale);
		iconv_close(camera->pl->params.cd_locale_to_ucs2);
		/* close ptp session */
		ptp_closesession (params);
		ptp_free_params(params);
		free (params->data);
		free (camera->pl); /* also frees params */
		params = NULL;
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
	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *text, GPContext *context)
{
	/* Note that we are not a so called 'Licensed Implementation' of MTP
	 * ... (for a LI you need express approval from Microsoft etc.)
	 */
	strncpy (text->text,
	 _("PTP2 driver\n"
	   "(c)2001-2005 by Mariusz Woloszyn <emsi@ipartners.pl>.\n"
	   "(c)2003-2006 by Marcus Meissner <marcus@jet.franken.de>.\n"
	   "This driver supports cameras that support PTP or PictBridge(tm), and\n"
	   "Media Players that support the Media Transfer Protocol (MTP).\n"
	   "\n"
	   "This driver is not a 'Licensed Implementation' of the Media Transfer Protocol.\n"
	   "\n"
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
	/* clear objectinfo entry for new object and assign new handler */
	memset(&params->objectinfo[n-1],0,sizeof(PTPObjectInfo));
	params->handles.Handler[n-1]=handle;
	/* get new obectinfo */
	CPR (context, ptp_getobjectinfo(params, handle, &params->objectinfo[n-1]));
	return (GP_OK);
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	unsigned char	*data = NULL;
	uint32_t	size = 0;
	int ret;
	PTPParams *params = &camera->pl->params;

	/* Currently disabled, since we must make sure for Canons
	 * that prepare capture was called.
	 * Enable: remote 0 &&, run
	 * 	gphoto2 --set-config capture=on --capture-preview
	 */
	if (camera->pl->params.deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) {
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
		gp_file_set_data_and_size ( file, (char*)data, size );
		gp_file_set_mime_type (file, GP_MIME_JPEG);     /* always */
		/* Add an arbitrary file name so caller won't crash */
		gp_file_set_name (file, "canon_preview.jpg");

		ret = ptp_canon_viewfinderoff (params);
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
add_objectid_to_gphotofs(Camera *camera, CameraFilePath *path, GPContext *context,
	uint32_t newobject, PTPObjectInfo *oi) {
	int			ret;
	PTPParams		*params = &camera->pl->params;
	CameraFile		*file = NULL;
	unsigned char		*ximage = NULL;
	CameraFileInfo		info;

	ret = gp_file_new(&file);
	if (ret!=GP_OK) return ret;
	gp_file_set_type (file, GP_FILE_TYPE_NORMAL);
	gp_file_set_name(file, path->name);
	set_mimetype (camera, file, params->deviceinfo.VendorExtensionID, oi->ObjectFormat);
	CPR (context, ptp_getobject(params, newobject, &ximage));
	ret = gp_file_set_data_and_size(file, (char*)ximage, oi->ObjectCompressedSize);
	if (ret != GP_OK) return ret;
	ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
        if (ret != GP_OK) return ret;
	ret = gp_filesystem_set_file_noop(camera->fs, path->folder, file, context);
        if (ret != GP_OK) return ret;

	/* We have now handed over the file, disclaim responsibility by unref. */
	gp_file_unref (file);

	/* we also get the fs info for free, so just set it */
	info.file.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_NAME |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE;
	strcpy_mime (info.file.type, params->deviceinfo.VendorExtensionID, oi->ObjectFormat);
	strcpy(info.file.name,path->name);
	info.file.width		= oi->ImagePixWidth;
	info.file.height	= oi->ImagePixHeight;
	info.file.size		= oi->ObjectCompressedSize;
	info.preview.fields = GP_FILE_INFO_TYPE |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE;
	strcpy_mime (info.preview.type, params->deviceinfo.VendorExtensionID, oi->ThumbFormat);
	info.preview.width	= oi->ThumbPixWidth;
	info.preview.height	= oi->ThumbPixHeight;
	info.preview.size	= oi->ThumbCompressedSize;
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
	int			i, ret, hasc101 = 0, burstnumber = 1;

	if (type != GP_CAPTURE_IMAGE)
		return GP_ERROR_NOT_SUPPORTED;

	if (params->deviceinfo.VendorExtensionID!=PTP_VENDOR_NIKON)
		return GP_ERROR_NOT_SUPPORTED;

	if (!ptp_operation_issupported(params,PTP_OC_NIKON_Capture)) {
		gp_context_error(context,
               	_("Sorry, your camera does not support Nikon capture"));
		return GP_ERROR_NOT_SUPPORTED;
	}
	if (	ptp_property_issupported(params, PTP_DPC_StillCaptureMode)	&&
		(PTP_RC_OK == ptp_getdevicepropdesc (params, PTP_DPC_StillCaptureMode, &propdesc)) &&
		(propdesc.DataType == PTP_DTC_UINT16)				&&
		(propdesc.CurrentValue.u16 == 2) /* Burst Mode */		&&
		ptp_property_issupported(params, PTP_DPC_BurstNumber)		&&
		(PTP_RC_OK == ptp_getdevicepropdesc (params, PTP_DPC_BurstNumber, &propdesc)) &&
		(propdesc.DataType == PTP_DTC_UINT16)
	) {
		burstnumber = propdesc.CurrentValue.u16;
		gp_log (GP_LOG_DEBUG, "ptp2", "burstnumber %d", burstnumber);
	}

	do {
		ret = ptp_nikon_capture(params, 0xffffffff);
	} while (ret == PTP_RC_DeviceBusy);
	CPR (context, ret);

	CR (gp_port_set_timeout (camera->port, USB_TIMEOUT_CAPTURE));

	while (!((ptp_nikon_device_ready(params) == PTP_RC_OK) && hasc101)) {
		int i, evtcnt;
		PTPUSBEventContainer *nevent = NULL;

		/* Just busy loop until the camera is ready again. */
		/* and wait for the 0xc101 event */
		ret = ptp_nikon_check_event(params, &nevent, &evtcnt);
		if (ret != PTP_RC_OK)
			break;
		for (i=0;i<evtcnt;i++) {
			/*fprintf(stderr,"1:nevent.Code is %x / param %lx\n", nevent[i].code, (unsigned long)nevent[i].param1);*/
			if (nevent[i].code == 0xc101) hasc101=1;
		}
		free (nevent);
	}

	for (i=0;i<burstnumber;i++) {
		/* In Burst mode, the image is always 0xffff0001.
		 * The firmware just gives us one after the other for the same ID
		 */
		ret = ptp_getobjectinfo (params, 0xffff0001, &oi);
		if (ret != PTP_RC_OK) {
			fprintf (stderr,"getobjectinfo(%x) failed: %d\n", 0xffff0001, ret);
			return GP_ERROR_IO;
		}
		if (oi.ParentObject != 0)
			fprintf(stderr,"Parentobject is 0x%lx now?\n", (unsigned long)oi.ParentObject);
		/* Happens on Nikon D70, we get Storage ID 0. So fake one. */
		if (oi.StorageID == 0)
			oi.StorageID = 0x00010001;
		sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx",(unsigned long)oi.StorageID);
		sprintf (path->name, "capt%04d.jpg", capcnt++);
		ret = add_objectid_to_gphotofs(camera, path, context, 0xffff0001, &oi);
		if (ret != GP_OK) {
			fprintf (stderr, "failed to add object\n");
			return ret;
		}
	}
	return GP_OK;
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
	static int capcnt = 0;
	PTPObjectInfo		oi;
	int			i, ret, isevent;
	PTPParams		*params = &camera->pl->params;
	uint32_t		newobject = 0x0;
	PTPPropertyValue	propval;
	uint16_t		val16;
	PTPContainer		event;
	PTPUSBEventContainer	usbevent;
	uint32_t		handle;

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

	propval.u16=3; /* 3 */
	ret = ptp_setdevicepropvalue(params, PTP_DPC_CANON_D029, &propval, PTP_DTC_UINT16);
	if (ret != PTP_RC_OK)
		gp_log (GP_LOG_DEBUG, "ptp", "setdevicepropvalue 0xd029 failed, %d\n", ret);

#if 0
	/* FIXME: For now, to avoid flash during debug */
	propval.u8 = 0;
	ret = ptp_setdevicepropvalue(params, PTP_DPC_CANON_FlashMode, &propval, PTP_DTC_UINT8);
#endif
	ret = ptp_canon_initiatecaptureinmemory (params);
	if (ret != PTP_RC_OK) {
		gp_context_error (context, _("Canon Capture failed: %d"), ret);
		return GP_ERROR;
	}
	/* Catch event */
	if (PTP_RC_OK == (val16 = params->event_wait (params, &event))) {
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
			newobject = usbevent.param1;

			for (j=0;j<2;j++) {
				ret=ptp_canon_checkevent(params,&usbevent,&isevent);
				if ((ret==PTP_RC_OK) && isevent)
					gp_log (GP_LOG_DEBUG, "ptp", "evdata: L=0x%X, T=0x%X, C=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X\n", usbevent.length,usbevent.type,usbevent.code,usbevent.trans_id, usbevent.param1, usbevent.param2, usbevent.param3);
			}


			ret = ptp_canon_aeafawb(params,7);
			break;
		}
	}
	/* Catch event, attempt  2 */
	if (val16!=PTP_RC_OK) {
		if (PTP_RC_OK==params->event_wait (params, &event)) {
			if (event.Code==PTP_EC_CaptureComplete)
				printf("Event: capture complete. \n");
			else
				printf("Event: 0x%X\n", event.Code);
		} else
			printf("No expected capture complete event\n");
	}
	if (i==100) {
	    gp_log (GP_LOG_DEBUG, "ptp","ERROR: Capture timed out!\n");
	    return GP_ERROR_TIMEOUT;
	}

	/* FIXME: handle multiple images (as in BurstMode) */
	ret = ptp_getobjectinfo (params, newobject, &oi);
	if (ret != PTP_RC_OK) return GP_ERROR_IO;
	if (oi.ParentObject != 0) {
		fprintf(stderr,"Parentobject is 0x%lx now?\n", (unsigned long)oi.ParentObject);
	}
	sprintf (path->folder,"/"STORAGE_FOLDER_PREFIX"%08lx",(unsigned long)oi.StorageID);
	sprintf (path->name, "capt%04d.jpg", capcnt++);
	return add_objectid_to_gphotofs(camera, path, context, newobject, &oi);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	PTPContainer event;
	PTPParams *params = &camera->pl->params;
	uint32_t newobject = 0x0;

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		ptp_operation_issupported(params, PTP_OC_NIKON_Capture)
	){
		char buf[1024];
		if ((GP_OK != gp_setting_get("ptp2","capturetarget",buf)) || !strcmp(buf,"sdram"))
			return camera_nikon_capture (camera, type, path, context);
	}
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_InitiateCaptureInMemory)
	) {
		char buf[1024];
		if ((GP_OK != gp_setting_get("ptp2","capturetarget",buf)) || !strcmp(buf,"sdram"))
			return camera_canon_capture (camera, type, path, context);
	}

	if (type != GP_CAPTURE_IMAGE)
		return GP_ERROR_NOT_SUPPORTED;

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
	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_NIKON)
		goto out;
	{
		short ret = params->event_wait(params,&event);
		CR (gp_port_set_timeout (camera->port, USB_NORMAL_TIMEOUT));
		if (ret!=PTP_RC_OK) goto err;
	}
	while (event.Code==PTP_EC_ObjectAdded) {
		/* add newly created object to internal structures */
		add_object (camera, event.Param1, context);
		newobject = event.Param1;

		if (params->event_wait (params, &event)!=PTP_RC_OK)
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

		for (i = params->handles.n ; i--; ) {
			PTPObjectInfo	*obinfo;

			if (params->handles.Handler[i] != newobject)
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

static int
camera_wait_for_event (Camera *camera, int timeout,
		       CameraEventType *eventtype, void **eventdata,
		       GPContext *context) {
	PTPContainer event;
	PTPParams *params = &camera->pl->params;
	uint32_t newobject = 0x0;
	CameraFilePath *path;
        int i, oldtimeout;
	uint16_t ret;

	memset (&event, 0, sizeof(event));
	gp_port_get_timeout (camera->port, &oldtimeout);
	gp_port_set_timeout (camera->port, timeout);
	ret = params->event_wait(params,&event);
	gp_port_set_timeout (camera->port, oldtimeout);

	if (ret!=PTP_RC_OK) {
		/* FIXME: Might be another error, but usually is a timeout */
		gp_log (GP_LOG_DEBUG, "ptp2", "wait_for_event: received error 0x%04x", ret);
		*eventtype = GP_EVENT_TIMEOUT;
		return GP_OK;
	}
	gp_log (GP_LOG_DEBUG, "ptp2", "wait_for_event: code=0x%04x, param1 0x%08x",
		event.Code, event.Param1
	);

	switch (event.Code) {
	case PTP_EC_ObjectAdded:
		path = (CameraFilePath *)malloc(sizeof(CameraFilePath));
		if (!path)
			return GP_ERROR_NO_MEMORY;
		newobject = event.Param1;
		add_object (camera, event.Param1, context);
		path->name[0]='\0';
		path->folder[0]='\0';

		for (i = params->handles.n ; i--; ) {
			PTPObjectInfo	*obinfo;

			if (params->handles.Handler[i] != newobject)
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
		*eventtype = GP_EVENT_FILE_ADDED;
		*eventdata = path;
		break;
	default: {
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

	((PTPData *) camera->pl->params.data)->context = context;

	res = ptp_canon_get_customize_data (params, 1, &xdata, &size);
	if (res != PTP_RC_OK)  {
		report_result(context, res, params->deviceinfo.VendorExtensionID);
		return (translate_ptp_result(res));
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

	res = ptp_nikon_curve_download (params, &xdata, &size);
	if (res != PTP_RC_OK)  {
		report_result(context, res, params->deviceinfo.VendorExtensionID);
		return (translate_ptp_result(res));
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
	int n, i, j;
	int spaceleft;
	char *txt;
	PTPParams *params = &(camera->pl->params);
	PTPDeviceInfo pdi;
	PTPStorageIDs storageids;

	spaceleft = sizeof(summary->text);
	n = snprintf (summary->text, sizeof (summary->text),
		_("Model: %s\n"
		"  device version: %s\n"
		"  serial number:  %s\n"
		"Vendor extension ID: 0x%08x\n"
		"Vendor extension description: %s\n"
	),
		params->deviceinfo.Model,
		params->deviceinfo.DeviceVersion,
		params->deviceinfo.SerialNumber,
		params->deviceinfo.VendorExtensionID,
		params->deviceinfo.VendorExtensionDesc);

	if (n>=sizeof (summary->text))
		return GP_OK;
	spaceleft -= n;
	txt = summary->text + strlen (summary->text);

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

	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
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
					if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
					n = ptp_render_mtp_propname(props[j],spaceleft,txt);
					if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
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
		    ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_ViewfinderOn)) {
			n = snprintf (txt, spaceleft,_("Canon Capture\n"));
		} else  {
			if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
			     ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_Capture)) {
				n = snprintf (txt, spaceleft,_("Nikon Capture\n"));
			} else {
				n = snprintf (txt, spaceleft,_("No vendor specific capture\n"));
			}
		}
		if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;

	/* Third line for Wifi support, but just leave it out if not there. */
		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		     ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_GetProfileAllData)) {
			n = snprintf (txt, spaceleft,_("\tNikon Wifi support\n"));
			if (n >= spaceleft) return GP_OK; spaceleft -= n; txt += n;
		}

		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		     ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_GetMACAddress)) {
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
		ptp_free_devicepropdesc (&dpd);
        }
	ptp_free_DI (&pdi);
	return (GP_OK);
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
    gp_log (GP_LOG_DEBUG, "ptp2", "file_list_func(%s)", folder);

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
	/* not our parent -> next */
	if (params->objectinfo[i].ParentObject!=parent)
		continue;

	/* not on our storage devices -> next */
	if ((ptp_operation_issupported(params,PTP_OC_GetStorageIDs)
            && (params->objectinfo[i].StorageID != storage)))
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
	int i;
	uint32_t handler,storage;

	/*((PTPData *)((Camera *)data)->pl->params.data)->context = context;*/
	gp_log (GP_LOG_DEBUG, "ptp2", "folder_list_func(%s)", folder);

	/* add storage pseudofolders in root folder */
	if (!strcmp(folder, "/")) {
		PTPStorageIDs storageids;

		if (ptp_operation_issupported(params,PTP_OC_GetStorageIDs)) {
			CPR (context, ptp_getstorageids(params,
				&storageids));
			for (i=0; i<storageids.n; i++) {
				char fname[PTP_MAXSTRLEN];

				if ((storageids.Storage[i]&0x0000ffff)==0) continue;
				snprintf(fname, strlen(STORAGE_FOLDER_PREFIX)+9,
					STORAGE_FOLDER_PREFIX"%08x",
					storageids.Storage[i]);
				CR (gp_list_append (list, fname, NULL));
			}
		} else {
			char fname[PTP_MAXSTRLEN];
			snprintf(fname, strlen(STORAGE_FOLDER_PREFIX)+9,
					STORAGE_FOLDER_PREFIX"%08x",
					0xdeadbeef
			);
			gp_list_append (list, fname, NULL);
		}

		if (nrofspecial_files)
			CR (gp_list_append (list, "special", NULL));

		if (storageids.Storage[0] != 0xdeadbeef)
			free (storageids.Storage);
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

	/* Look for objects we can present as directories.
	 * Currently we specify *any* PTP association as directory.
	 */
	for (i = 0; i < params->handles.n; i++) {
		if (	(params->objectinfo[i].ParentObject==handler)		&&
			((!ptp_operation_issupported(params,PTP_OC_GetStorageIDs)) ||
			 (params->objectinfo[i].StorageID == storage)
			)							&&
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

	if (params->proplist) { /* use the fast method, without device access since cached.*/
		char			propname[256];
		char			text[256];
		int 			i, n;
		MTPPropList		*xpl = params->proplist;
	
		while (xpl) {
			if (xpl->ObjectHandle != object_id) {
				xpl = xpl->next;
				continue;
			}
			for (i=sizeof(uninteresting_props)/sizeof(uninteresting_props[0]);i--;)
				if (uninteresting_props[i] == xpl->property)
					break;
			if (i != -1) {/* Is uninteresting. */
				xpl = xpl->next;
				continue;
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
			xpl = xpl->next;
		}
		return (GP_OK);
	}

	ret = ptp_mtp_getobjectpropssupported (params, ofc, &propcnt, &props);
	if (ret != PTP_RC_OK) return (GP_ERROR);

	for (j=0;j<propcnt;j++) {
		char			propname[256];
		char			text[256];
		PTPObjectPropDesc	opd;
		int 			i, n;

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
	if (ret != PTP_RC_OK) return (GP_ERROR);

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
		gp_log (GP_LOG_DEBUG, "ptp2", "found tag %s, content %s", propname, content);
		ret = ptp_mtp_getobjectpropdesc (params, props[j], ofc, &opd);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_DEBUG, "ptp2", " getobjectpropdesc returns 0x%x", ret);
			free (content); content = NULL;
			continue;
		}
		if (opd.GetSet == 0) {
			gp_log (GP_LOG_DEBUG, "ptp2", "Tag %s is read only, sorry.", propname);
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
		return (translate_ptp_result(ret));
	
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
		return GP_ERROR;
	}
	ret = ptp_sendobject(&camera->pl->params, (unsigned char*)data, 1);
	if (ret != PTP_RC_OK) {
		gp_log (GP_LOG_ERROR, "put mtp playlist", "failed dummy sendobject of playlist.");
		return GP_ERROR;
	}
	ret = ptp_mtp_setobjectreferences (&camera->pl->params, playlistid, oids, nrofoids);
	if (ret != PTP_RC_OK) {
		gp_log (GP_LOG_ERROR, "put mtp playlist", "failed setobjectreferences.");
		return GP_ERROR;
	}
	/* update internal structures */
	add_object(camera, playlistid, context);
	return GP_OK;
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
	gp_file_set_data_and_size (file, content, contentlen);
	return (GP_OK);
}

typedef struct {
	CameraFile	*file;
} PTPCFHandlerPrivate;

static uint16_t
gpfile_getfunc (PTPParams *params, void *priv,
	unsigned long wantlen, unsigned char *bytes,
	unsigned long *gotlen
) {
	PTPCFHandlerPrivate* private = (PTPCFHandlerPrivate*)priv;
	int ret;
	size_t	gotlensize;

	ret = gp_file_slurp (private->file, bytes, wantlen, &gotlensize);
	*gotlen = gotlensize;
	if (ret != GP_OK)
		return PTP_ERROR_IO;
	return PTP_RC_OK;
}

static uint16_t
gpfile_putfunc (PTPParams *params, void *priv,
	unsigned long sendlen, unsigned char *bytes,
	unsigned long *written
) {
	PTPCFHandlerPrivate* private = (PTPCFHandlerPrivate*)priv;
	int ret;
	
	ret = gp_file_append (private->file, bytes, sendlen);
	if (ret != GP_OK)
		return PTP_ERROR_IO;
	*written = sendlen;
	return PTP_RC_OK;
}

static uint16_t
ptp_init_camerafile_handler (PTPDataHandler *handler, CameraFile *file) {
	PTPCFHandlerPrivate* private = malloc (sizeof(PTPCFHandlerPrivate));
	if (!private) return PTP_RC_GeneralError;
	handler->private = private;
	handler->getfunc = gpfile_getfunc;
	handler->putfunc = gpfile_putfunc;
	private->file = file;
	return PTP_RC_OK;
}

static uint16_t
ptp_exit_camerafile_handler (PTPDataHandler *handler) {
	free (handler->private);
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
	unsigned char * image=NULL;
	uint32_t object_id;
	uint32_t size;
	uint32_t storage;
	PTPObjectInfo * oi;
	PTPParams *params = &camera->pl->params;

	((PTPData *) params->data)->context = context;

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

	oi=&params->objectinfo[object_id];

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
		if (oi->ObjectFormat != PTP_OFC_EXIF_JPEG)
			return (GP_ERROR_NOT_SUPPORTED);

		/* Note: Could also use Canon partial downloads */
		CPR (context, ptp_getpartialobject (params,
			params->handles.Handler[object_id],
			0, 10, &ximage));

		if (!((ximage[0] == 0xff) && (ximage[1] == 0xd8))) {	/* SOI */
			free (image);
			return (GP_ERROR_NOT_SUPPORTED);
		}
		if (!((ximage[2] == 0xff) && (ximage[3] == 0xe1))) {	/* App0 */
			free (image);
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
			offset, maxbytes, &ximage));
		CR (gp_file_set_data_and_size (file, (char*)ximage, maxbytes));
		break;
	}
	case	GP_FILE_TYPE_PREVIEW: {
		unsigned char *ximage = NULL;

		/* If thumb size is 0 then there is no thumbnail at all... */
		if((size=oi->ThumbCompressedSize)==0) return (GP_ERROR_NOT_SUPPORTED);
		CPR (context, ptp_getthumb(params,
			params->handles.Handler[object_id],
			&ximage));
		CR (gp_file_set_data_and_size (file, (char*)ximage, size));
		/* XXX does gp_file_set_data_and_size free() image ptr upon
		   failure?? */
		break;
	}
	case	GP_FILE_TYPE_METADATA:
		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
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
				oi->ThumbFormat == PTP_OFC_Undefined))
			return (GP_ERROR_NOT_SUPPORTED);

		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
		    (oi->ObjectFormat == PTP_OFC_MTP_AbstractAudioVideoPlaylist))
			return mtp_get_playlist (camera, file, params->handles.Handler[object_id], context);

		size=oi->ObjectCompressedSize;
		if (size) {
			PTPDataHandler handler;
			ptp_init_camerafile_handler (&handler, file);
			CPR (context, ptp_getobject_to_handler(params,
			params->handles.Handler[object_id],
			&handler));
			ptp_exit_camerafile_handler (&handler);
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
	PTPParams* params=&camera->pl->params;
	CameraFileType	type;

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
	gp_file_get_type (file, &type);
	gp_log ( GP_LOG_DEBUG, "ptp2/put_file_func", "folder=%s, filename=%s", folder, filename);

	if (type == GP_FILE_TYPE_METADATA) {
		if ((params->deviceinfo.VendorExtensionID==PTP_VENDOR_MICROSOFT) &&
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
		return GP_ERROR;
	}
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

	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
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
		PTPDataHandler handler;
		CPR (context, ptp_sendobjectinfo (params, &storage,
			&parent, &handle, &oi));
		ptp_init_camerafile_handler (&handler, file);
		CPR (context, ptp_sendobject_from_handler (params, &handler, intsize));
		ptp_exit_camerafile_handler (&handler);
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
	PTPParams *params = &camera->pl->params;

	((PTPData *) params->data)->context = context;

	if (!ptp_operation_issupported(params, PTP_OC_DeleteObject))
		return GP_ERROR_NOT_SUPPORTED;

	if (!strcmp (folder, "/special"))
		return GP_ERROR_NOT_SUPPORTED;

	/* virtual file created by Nikon special capture */
	if (	((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) ||
		 (params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON)   ) &&
		!strncmp (filename, "capt", 4)
	)
		return GP_OK;

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(filename, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	CPR (context, ptp_deleteobject(params,
		params->handles.Handler[object_id],0));

	/* Remove it from the internal structures. */
	memcpy (params->handles.Handler+object_id,
		params->handles.Handler+object_id+1,
		(params->handles.n-object_id-1)*sizeof(params->handles.Handler[0])
	);
	memcpy (params->objectinfo+object_id,
		params->objectinfo+object_id+1,
		(params->handles.n-object_id-1)*sizeof(params->objectinfo[0])
	);
	params->handles.n--;

	/* On some Canon firmwares, a DeleteObject causes a ObjectRemoved event
	 * to be sent. At least on Digital IXUS II and PowerShot A85. But
         * not on 350D.
	 */
	if (DELETE_SENDS_EVENT(camera->pl) &&
	    ptp_event_issupported(params, PTP_EC_ObjectRemoved)) {
		PTPContainer event;
		int ret;

		do {
			ret = params->event_check (params, &event);
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
	PTPParams *params = &camera->pl->params;

	((PTPData *) params->data)->context = context;

	if (!ptp_operation_issupported(params, PTP_OC_DeleteObject))
		return GP_ERROR_NOT_SUPPORTED;

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(foldername, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	CPR (context, ptp_deleteobject(params, params->handles.Handler[object_id],0));

	/* Remove it from the internal structures. */
	memcpy (params->handles.Handler+object_id,
		params->handles.Handler+object_id+1,
		(params->handles.n-object_id-1)*sizeof(params->handles.Handler[0])
	);
	memcpy (params->objectinfo+object_id,
		params->objectinfo+object_id+1,
		(params->handles.n-object_id-1)*sizeof(params->objectinfo[0])
	);
	params->handles.n--;
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

	((PTPData *) params->data)->context = context;

	if (!strcmp (folder, "/special"))
		return (GP_ERROR_BAD_PARAMETERS); /* for now */

	/* compute storage ID value from folder patch */
	folder_to_storage(folder,storage);

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, storage, object_id, data);
	object_id = find_child(filename, storage, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	oi=&params->objectinfo[object_id];

	info->file.fields = GP_FILE_INFO_SIZE|GP_FILE_INFO_TYPE|GP_FILE_INFO_MTIME;

	/* Avoid buffer overflows on long filenames, just don't copy it
	 * if it is too long.
	 */
	if (oi->Filename && (strlen(oi->Filename)+1 < sizeof(info->file.name))) {
		strcpy(info->file.name, oi->Filename);
		info->file.fields |= GP_FILE_INFO_NAME;
	}
	info->file.size   = oi->ObjectCompressedSize;

	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) && params->canon_flags) {
		info->file.fields |= GP_FILE_INFO_STATUS;
		if (params->canon_flags[object_id] & 0x2000)
			info->file.status = GP_FILE_STATUS_NOT_DOWNLOADED;
		else
			info->file.status = GP_FILE_STATUS_DOWNLOADED;
	}

	/* MTP playlists have their own size calculation */
	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
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
	add_object(camera, handle, context);
	return (GP_OK);
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
	int			i;
	uint16_t		ret;
	CameraStorageInformation*sif;

	if (!ptp_operation_issupported (params, PTP_OC_GetStorageIDs))
		return (GP_ERROR_NOT_SUPPORTED);

	ret = ptp_getstorageids (params, &sids);
	if (ret != PTP_RC_OK)
		return translate_ptp_result (ret);
	*nrofsinfos = sids.n;
	*sinfos = (CameraStorageInformation*)
		calloc (sizeof (CameraStorageInformation),sids.n);
	for (i = 0; i<sids.n; i++) {
		sif = (*sinfos)+i;
		ret = ptp_getstorageinfo (params, sids.Storage[i], &si);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_ERROR, "ptp2/storage_info_func", "ptp getstorageinfo failed: 0x%x", ret);
			return GP_ERROR;
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
	}
	free (sids.Storage);
	return (GP_OK);
}

static int
init_ptp_fs (Camera *camera, GPContext *context)
{
	int i,id,nroot;
	PTPParams *params = &camera->pl->params;
	char buf[1024];
	uint16_t ret;

	((PTPData *) params->data)->context = context;
	memset (&params->handles, 0, sizeof(PTPObjectHandles));

	/* Nikon supports a fast filesystem retrieval.
	 * Unfortunately this function returns a flat folder structure
	 * which cannot be changed to represent the actual FAT layout.
	 * So if you need to get access to _all_ files on the ptp fs,
	 * you can change the setting to "false" (gphoto2 --config or
	 * edit ~/.gphoto2/settings directly).
	 * A normal user does only download the images ... so the default
	 * is "fast".
	 */

	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
	    (ptp_operation_issupported(params, PTP_OC_NIKON_GetFileInfoInBlock)) &&
	    ((GP_OK != gp_setting_get("ptp2","nikon.fastfilesystem",buf)) || atoi(buf))
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

	/* CANON also has fast directory retrieval. And it is mostly complete, so we can use it as full replacement */
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
		/* fallthrough */
	}

	/* Microsoft/MTP also has fast directory retrieval. */
	if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_MICROSOFT) &&
	    ptp_operation_issupported(params,PTP_OC_MTP_GetObjPropList) &&
	    (camera->pl->bugs & PTP_MTP_PROPLIST_WORKS)
	) {
		PTPObjectInfo	*oinfos = NULL;	
		int		cnt = 0,i;
		uint32_t	lasthandle = 0xffffffff;
		MTPPropList 	*proplist = NULL, *xpl;

		ret = ptp_mtp_getobjectproplist (&camera->pl->params, 0xffffffff, &proplist);
		if (ret != PTP_RC_OK)
			goto fallback;
		params->proplist = proplist; /* cache it */

		/* count the objects */
		xpl = proplist;
		while (xpl) {
			if (lasthandle != xpl->ObjectHandle) {
				cnt++;
				lasthandle = xpl->ObjectHandle;
			}
			xpl = xpl->next;
		}
		lasthandle = 0xffffffff;
		oinfos = params->objectinfo = malloc (sizeof (PTPObjectInfo) * cnt);
		memset (oinfos ,0 ,sizeof (PTPObjectInfo) * cnt);
		params->handles.Handler = malloc (sizeof (uint32_t) * cnt);
		params->handles.n = cnt;

		xpl = proplist; i = -1;
		while (xpl) {
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
			}
			switch (xpl->property) {
			case PTP_OPC_ParentObject:
				oinfos[i].ParentObject = xpl->propval.u32;
				break;
			case PTP_OPC_ObjectFormat:
				oinfos[i].ObjectFormat = xpl->propval.u16;
				break;
			case PTP_OPC_ObjectSize:
				oinfos[i].ObjectCompressedSize = xpl->propval.u32;
				break;
			case PTP_OPC_StorageID:
				oinfos[i].StorageID = xpl->propval.u32;
				break;
			case PTP_OPC_ObjectFileName:
				oinfos[i].Filename = xpl->propval.str;
				break;
			default:
				if ((xpl->property & 0xfff0) == 0xdc00)
					gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "case %x type %x unhandled.\n", xpl->property, xpl->datatype);
				break;
			}
			xpl = xpl->next;
		}
		return PTP_RC_OK;
	}

fallback:
	/* Get file handles array for filesystem */
	id = gp_context_progress_start (context, 100, _("Initializing Camera"));
	/* get objecthandles of all objects from all stores */
	CPR (context, ptp_getobjecthandles
	(params, 0xffffffff, 0x000000, 0x000000, &params->handles));

	gp_context_progress_update (context, id, 10);
	/* wee need that for fileststem */
	params->objectinfo = (PTPObjectInfo*)malloc(sizeof(PTPObjectInfo)*
		params->handles.n);
	memset (params->objectinfo,0,sizeof(PTPObjectInfo)*params->handles.n);

	for (i = 0; i < params->handles.n; i++) {
		CPR (context, ptp_getobjectinfo(params,
			params->handles.Handler[i],
			&params->objectinfo[i]));
#if 1
		{
		PTPObjectInfo *oi;

		oi=&params->objectinfo[i];
		GP_DEBUG ("ObjectInfo for '%s':", oi->Filename);
		GP_DEBUG ("  Object ID: 0x%08x",
			params->handles.Handler[i]);
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

        if (DCIM_WRONG_PARENT_BUG(camera->pl))
        {
            GP_DEBUG("PTPBUG_DCIM_WRONG_PARENT bug workaround");
            /* Count number of root directory objects */
            nroot = 0;
            for (i = 0; i < params->handles.n; i++)
		if (params->objectinfo[i].ParentObject == 0)
                    nroot++;

            GP_DEBUG("Found %d root directory objects", nroot);

            /* If no root directory objects, look for "DCIM".  This way, we can
             * handle cameras that report the wrong ParentObject ID for root
             */
            if (nroot == 0 && params->handles.n > 0) {
		for (i = 0; i < params->handles.n; i++)
		{
                    PTPObjectInfo *oi = &params->objectinfo[i];

                    if (strcmp(oi->Filename, "DCIM") == 0)
                    {
                        GP_DEBUG("Changing DCIM ParentObject ID from 0x%x to 0",
                                 oi->ParentObject);
                        oi->ParentObject = 0;
			nroot++;
                    }
		}
            }
	    /* Some cameras do not have a directory at all, just files or unattached
	     * directories. In this case associate all unattached to the 0 object.
	     */
            if (nroot == 0) {
		/* look for entries with parentobjects that do not exist */
		for (i = 0; i < params->handles.n; i++)
		{
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

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func		= file_list_func,
	.folder_list_func	= folder_list_func,
	.get_info_func		= get_info_func,
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
	char *curloc;
	GPPortSettings	settings;

	gp_port_get_settings (camera->port, &settings);
	/* Make sure our port is either USB or PTP/IP. */
	if ((camera->port->type != GP_PORT_USB) && (camera->port->type != GP_PORT_PTPIP)) {
		gp_context_error (context, _("PTP is only implemented for "
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
	camera->pl->params.debug_func = ptp_debug_func;
	camera->pl->params.error_func = ptp_error_func;
	camera->pl->params.data = malloc (sizeof (PTPData));
	memset (camera->pl->params.data, 0, sizeof (PTPData));
	((PTPData *) camera->pl->params.data)->camera = camera;
	camera->pl->params.byteorder = PTP_DL_LE;
	camera->pl->params.maxpacketsize = settings.usb.maxpacketsize;
	if (!camera->pl->params.maxpacketsize)
		camera->pl->params.maxpacketsize = 64; /* assume USB 1.0 */

	switch (camera->port->type) {
	case GP_PORT_USB:
		camera->pl->params.sendreq_func		= ptp_usb_sendreq;
		camera->pl->params.senddata_func	= ptp_usb_senddata;
		camera->pl->params.getresp_func		= ptp_usb_getresp;
		camera->pl->params.getdata_func		= ptp_usb_getdata;
		camera->pl->params.event_wait		= ptp_usb_event_wait;
		camera->pl->params.event_check		= ptp_usb_event_check;
		break;
	case GP_PORT_PTPIP: {
		GPPortInfo	pinfo;

		ret = gp_port_get_info (camera->port, &pinfo);
		if (ret != GP_OK) {
			gp_log (GP_LOG_ERROR, "ptpip", "Failed to get port info?\n");
			return ret;
		}
		ret = ptp_ptpip_connect (&camera->pl->params, pinfo.path);
		if (ret != GP_OK) {
			gp_log (GP_LOG_ERROR, "ptpip", "Failed to connect.\n");
			return ret;
		}
		camera->pl->params.sendreq_func		= ptp_ptpip_sendreq;
		camera->pl->params.senddata_func	= ptp_ptpip_senddata;
		camera->pl->params.getresp_func		= ptp_ptpip_getresp;
		camera->pl->params.getdata_func		= ptp_ptpip_getdata;
		camera->pl->params.event_wait		= ptp_ptpip_event_wait;
		camera->pl->params.event_check		= ptp_ptpip_event_check;
		break;
	}
	default:
		break;
	}

	curloc = nl_langinfo (CODESET);
	if (!curloc) curloc="UTF-8";
	camera->pl->params.cd_ucs2_to_locale = iconv_open(curloc, UCS_2_INTERNAL);
	camera->pl->params.cd_locale_to_ucs2 = iconv_open(UCS_2_INTERNAL, curloc);
	if ((camera->pl->params.cd_ucs2_to_locale == (iconv_t) -1) ||
	    (camera->pl->params.cd_locale_to_ucs2 == (iconv_t) -1)) {
		gp_log (GP_LOG_ERROR, "iconv", "Failed to create iconv converter.\n");
		return (GP_ERROR_OS_FAILURE);
	}
	
        gp_camera_get_abilities(camera, &a);
        for (i = 0; sizeof(models)/sizeof(models[0]); i++) {
            if ((a.usb_vendor == models[i].usb_vendor) &&
                (a.usb_product == models[i].usb_product)){
                camera->pl->bugs = models[i].known_bugs;
                break;
            }
        }

	/* Choose a shorter timeout on inital setup to avoid
	 * having the user wait too long.
	 */
	CR (gp_port_set_timeout (camera->port, USB_START_TIMEOUT));

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
			report_result(context, ret, camera->pl->params.deviceinfo.VendorExtensionID);
			return (translate_ptp_result(ret));
		}
		break;
	}
	/* We have cameras where a response takes 15 seconds(!), so make
	 * post init timeouts longer */
	CR (gp_port_set_timeout (camera->port, USB_NORMAL_TIMEOUT));

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
#if 0
		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_ThemeDownload)) {
			add_special_file("startimage.jpg",	canon_theme_get, canon_theme_put);
			add_special_file("startsound.wav",	canon_theme_get, canon_theme_put);
			add_special_file("operation.wav",	canon_theme_get, canon_theme_put);
			add_special_file("shutterrelease.wav",	canon_theme_get, canon_theme_put);
			add_special_file("selftimer.wav",	canon_theme_get, canon_theme_put);
		}
#endif
		break;
	case PTP_VENDOR_NIKON:
		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_CurveDownload))
			add_special_file("curve.ntc", nikon_curve_get, nikon_curve_put);
		break;
	default:
		break;
	}
	CR (gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera));

	return (GP_OK);
}
