/* library.c
 *
 * Copyright (C) 2001 Mariusz Woloszyn <emsi@ipartners.pl>
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
#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <gphoto2-library.h>
#include <gphoto2-port-log.h>

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
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "ptp.h"

#define GP_MODULE "PTP"

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
#define find_folder_handle(fn,p,d)	{			\
		{						\
		char *backfolder=malloc(strlen(fn));		\
		char *tmpfolder;				\
		memcpy(backfolder,fn+1, strlen(fn));		\
		if ((tmpfolder=strchr(backfolder+1,'/'))==NULL) tmpfolder="/";\
		p=folder_to_handle(tmpfolder+1,0,(Camera *)d);\
		free(backfolder);				\
		}						\
}

#define STORAGE_FOLDER_PREFIX		"store_"

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
	{PTP_RC_DeviceBusy, 		N_("PTP Device Bus")},
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
			gp_context_error (context, dgettext(PACKAGE, ptp_errors[i].txt));
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
		return (PTP_RC_GeneralError);
	default:
		return (PTP_RC_GeneralError);
	}
}

static struct {
	const char *model;
	unsigned short usb_vendor;
	unsigned short usb_product;
} models[] = {
	/*
	 * The very first PTP camera (with special firmware only), also
	 * called "PTP Prototype", may report non PTP interface class
	 */
	{"Kodak DC-240 (PTP)",  0x040a, 0x0121},
	/*
	 * Old DC-4800 firmware reported cutom interface class, so we have
	 * to detect it by product/vendor IDs
	 */
	{"Kodak DC-4800", 0x040a, 0x0160},
	/* Below other camers known to be detected by interface class */

	{"Kodak DX-3215", 0x040a, 0x0525},
	{"Kodak DX-3500", 0x040a, 0x0500},
	{"Kodak DX-3600", 0x040a, 0x0510},
	{"Kodak DX-3700", 0x040a, 0x0530},
	{"Kodak DX-3900", 0x040a, 0x0170},
	{"Kodak DX-4900", 0x040a, 0x0550},
	{"Kodak MC3", 0x040a, 0x0400},
	{"HP PhotoSmart 318", 0x03f0, 0x6302},
	{"HP PhotoSmart 612", 0x03f0, 0x6302},
	{"HP PhotoSmart 715",  0x03f0, 0x6402},
	/* I got information that all SONY PTP cameras use the same
	   product/vendor IDs */
	{"Sony DSC-P5", 0x054c, 0x004e},
	{"Sony DSC-F707V", 0x054c, 0x004e},
	{"Sony DSC-P30", 0x054c, 0x004e},
	{"Sony DSC-P50",  0x054c, 0x004e},
	{"Sony DSC-S75",  0x054c, 0x004e},
	{"Sony DSC-S85",  0x054c, 0x004e},
	{"Sony MVC-CD300",  0x054c, 0x004e},

	/* more comming soon :) */
	{NULL, 0, 0}
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
	{PTP_OFC_ASF,		"vide/x-asf"},
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
};

struct _PTPData {
	Camera *camera;
	GPContext *context;
};
typedef struct _PTPData PTPData;

static short
ptp_read_func (unsigned char *bytes, unsigned int size, void *data)
{
	Camera *camera = ((PTPData *)data)->camera;
	int result;

	/*
	 * gp_port_read returns (in case of success) the number of bytes
	 * read. libptp doesn't need that.
	 */
	result = gp_port_read (camera->port, bytes, size);
	if (result==0) result = gp_port_read (camera->port, bytes, size);
	if (result >= 0)
		return (PTP_RC_OK);
	else
		return (translate_gp_result (result));
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
		return (translate_gp_result (result));
}

static short
ptp_check_int (unsigned char *bytes, unsigned int size, void *data)
{
	Camera *camera = ((PTPData *)data)->camera;
	int result;

	/*
	 * gp_port_check_int returns (in case of success) the number of bytes
	 * read. libptp doesn't need that.
	 */

	result = gp_port_check_int (camera->port, bytes, size);
	if (result==0) result = gp_port_check_int (camera->port, bytes, size);
	if (result >= 0)
		return (PTP_RC_OK);
	else
		return (translate_gp_result (result));
}


static void
ptp_debug_func (void *data, const char *format, va_list args)
{
	gp_logv (GP_LOG_DEBUG, "ptp", format, args);
}

static void
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
		a.status = GP_DRIVER_STATUS_TESTING;
		a.port   = GP_PORT_USB;
		a.speed[0] = 0;
		a.usb_vendor = models[i].usb_vendor;
		a.usb_product= models[i].usb_product;
		a.operations        = GP_OPERATION_NONE;
		a.file_operations   = GP_FILE_OPERATION_PREVIEW|
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
	a.operations        = GP_OPERATION_NONE;
	a.file_operations   = GP_FILE_OPERATION_PREVIEW|
				GP_FILE_OPERATION_DELETE;
	a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE
		| GP_FOLDER_OPERATION_MAKE_DIR;
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
	if (camera->pl) {
		ptp_closesession (&camera->pl->params);
		free (camera->pl);
		camera->pl = NULL;
	}
	/* FIXME: free all camera->pl->params.objectinfo[] and
	   other malloced data */

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *text, GPContext *context)
{
	strncpy (text->text,
		 _("Written by Mariusz Woloszyn <emsi@ipartners.pl>. "
		   "Enjoy!"), sizeof (text->text));
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

static inline int
handle_to_n (uint32_t handle, Camera *camera)
{
	int i;
	for (i = 0; i < camera->pl->params.handles.n; i++)
		if (camera->pl->params.handles.handler[i]==handle) return i;
	/* else not found */
	return (PTP_HANDLER_SPECIAL);
}


static uint32_t
find_child (const char *file, uint32_t handle, Camera *camera)
{
	int i;
	PTPObjectInfo *oi = camera->pl->params.objectinfo;

	for (i = 0; i < camera->pl->params.handles.n; i++) {
		if (oi[i].ParentObject==handle)
			if (!strcmp(oi[i].Filename,file))
				return (camera->pl->params.handles.handler[i]);
	}
	/* else not found */
	return (PTP_HANDLER_SPECIAL);
}


static uint32_t
folder_to_handle(const char *folder, uint32_t parent, Camera *camera)
{
	char *c;
	if (!strlen(folder)) return PTP_HANDLER_ROOT;
	if (!strcmp(folder,"/")) return PTP_HANDLER_ROOT;

	c=strchr(folder,'/');
	if (c!=NULL) {
		*c=0;
		parent=find_child (folder, parent, camera);
		return folder_to_handle(c+1,parent, camera);
	} else  {
		return find_child (folder, parent, camera);
	}
}
	

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	PTPParams *params = &((Camera *)data)->pl->params;
	uint32_t parent, storage=0x0000000;
	int i;

	/*((PTPData *)((Camera *)data)->pl->params.data)->context = context;*/

	/* There should be NO files in root folder */
	if (!strcmp(folder, "/")) {
		return (GP_OK);
	}
	/* be paranoid, there can be ONLY storage pseudofolders in /, but
	   check for that! */
	if (!strncmp(folder,"/"STORAGE_FOLDER_PREFIX,strlen(STORAGE_FOLDER_PREFIX)+1)) {
		/* compute the storageID */
		char storagetxt[MAXFILENAMELEN];
		/* be paranoid, allways!!! */
		if (strlen(folder)<strlen(STORAGE_FOLDER_PREFIX)+1) return (GP_ERROR);
		sprintf(storagetxt,"0x%s",&folder[strlen(STORAGE_FOLDER_PREFIX)+1]);
		storage=strtol(storagetxt,NULL,16);
	}


	/* Get (parent) folder handle omiting storage pseudofolder */
	find_folder_handle(folder,parent,data);
	for (i = 0; i < params->handles.n; i++) {
	if (params->objectinfo[i].ParentObject==parent)
	if (params->objectinfo[i].ObjectFormat != PTP_OFC_Association)
	if (params->objectinfo[i].StorageID == storage)
		CR (gp_list_append (list, params->objectinfo[i].Filename, NULL));
	}

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	PTPParams *params = &((Camera *)data)->pl->params;
	int i;

	/*((PTPData *)((Camera *)data)->pl->params.data)->context = context;*/

	/* add storage pseudofolders in root folder */
	if (!strcmp(folder, "/")) {
		PTPStorageIDs storageids;
		CPR (context, ptp_getstorageids(params,
		&storageids));
		for (i=0; i<storageids.n; i++) {
			char fname[MAXFILENAMELEN];
			PTPStorageInfo storageinfo;
			if ((storageids.storage[i]&0x0000ffff)==0) continue;
			CPR (context, ptp_getstorageinfo(params,
			storageids.storage[i], &storageinfo));
			snprintf(fname, MAXFILENAMELEN, STORAGE_FOLDER_PREFIX"%8.8x",
			storageids.storage[i]);
			CR (gp_list_append (list, fname, NULL));
		}
		return (GP_OK);
	}
	{
	uint32_t parent,storage=0x0000000;

	/* be paranoid, there can be ONLY storage pseudofolders in /, but
	   check for that! */
	if (!strncmp(folder,"/"STORAGE_FOLDER_PREFIX,strlen(STORAGE_FOLDER_PREFIX)+1)) {
		/* compute the storageID */
		char storagetxt[MAXFILENAMELEN];
		/* be paranoid, allways!!! */
		if (strlen(folder)<strlen(STORAGE_FOLDER_PREFIX)+1) return (GP_ERROR);
		sprintf(storagetxt,"0x%s",&folder[strlen(STORAGE_FOLDER_PREFIX)+1]);
		storage=strtol(storagetxt,NULL,16);
	}
	/* Get (parent) folder handle omiting storage pseudofolder */
	find_folder_handle(folder,parent,data);
	for (i = 0; i < params->handles.n; i++) {
	if (params->objectinfo[i].ParentObject==parent)
	if (params->objectinfo[i].StorageID == storage)
	if (params->objectinfo[i].ObjectFormat==PTP_OFC_Association &&
		params->objectinfo[i].AssociationType==PTP_AT_GenericFolder)
		CR (gp_list_append (list, params->objectinfo[i].Filename, NULL));
	}
	}
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	PTPReq *fdata = NULL;
	char * image;
	uint32_t object_id;
	uint32_t size;
	PTPObjectInfo * oi;

	((PTPData *) camera->pl->params.data)->context = context;

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, object_id, data);
	object_id = find_child(filename, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	oi=&camera->pl->params.objectinfo[object_id];
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		if ((oi->ObjectFormat == PTP_OFC_Undefined) ||
			(oi->ObjectFormat == PTP_OFC_Association))
			return (GP_ERROR_NOT_SUPPORTED);
		size=oi->ObjectCompressedSize;
		fdata=malloc(size+PTP_REQ_HDR_LEN);
		CPR_free (context, ptp_getobject(&camera->pl->params,
			camera->pl->params.handles.handler[object_id],
			size, fdata), fdata);
		image=malloc(size);
		memcpy(image, fdata->data, size);
		free(fdata);
		CR (gp_file_set_data_and_size (file, image, size));
		/* XXX does gp_file_set_data_and_size free() image ptr upon
		   failure?? */
		break;

	case GP_FILE_TYPE_PREVIEW:
		/* Don't allow to get thumb of nonimage objects! */
		if ((oi->ObjectFormat & 0x0800) == 0) return (GP_ERROR_NOT_SUPPORTED);
		/* if no thumb, for some reason */
		if((size=oi->ThumbCompressedSize)==0) return (GP_ERROR_NOT_SUPPORTED);
		fdata=malloc(size+PTP_REQ_HDR_LEN);
		CPR_free (context, ptp_getthumb(&camera->pl->params,
			camera->pl->params.handles.handler[object_id],
			size, fdata), fdata);
		image=malloc(size);
		memcpy(image, fdata->data, size);
		free(fdata);
		CR (gp_file_set_data_and_size (file, image, size));
		/* XXX does gp_file_set_data_and_size free() image ptr upon
		   failure?? */
		break;
		
	default:
		return (GP_ERROR_NOT_SUPPORTED);
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
	const char *filename, *object;
	uint32_t parent;
	uint32_t store;
	uint32_t handle;
	long int intsize;
	uint32_t size;
	PTPReq *fdata;

	((PTPData *) camera->pl->params.data)->context = context;
	memset(&oi, 0, sizeof (PTPObjectInfo));
	gp_file_get_name (file, &filename); 
	gp_file_get_data_and_size (file, &object, &intsize);
	size=(uint32_t)intsize;

	/* get parent folder id omiting storage pseudofolder */
	find_folder_handle(folder,parent,data);
	/* in Kodak DX3500 (probably others),
	 * you _have_ to specify store, so we use
	 * same as parent folder, or 0x00020001 if root.
	 * FIXME!
	 */
	if ( parent != PTP_HANDLER_ROOT ) {
		int n = handle_to_n(parent,camera);
		store = camera->pl->params.objectinfo[n].StorageID;
	} else
		store=0x00020001; // FIXME

	/* if you desire to put file to root folder, you have to use 
	 * 0xffffffff instead of 0x00000000 (which means responder decide).
	 */
	if (parent==PTP_HANDLER_ROOT) parent=PTP_HANDLER_SPECIAL;

	oi.Filename=(char *)filename;
	oi.ObjectFormat=get_mimetype(camera, file);
	oi.ObjectCompressedSize=size;
	gp_file_get_mtime(file, &oi.ModificationDate);
	CPR (context, ptp_ek_sendfileobjectinfo (&camera->pl->params,
		&store, &parent, &handle, &oi));
	fdata=malloc(size+PTP_REQ_HDR_LEN);
	memcpy(fdata->data, object, size);
	CPR (context, ptp_ek_sendfileobject (&camera->pl->params,
		fdata, size));
	free(fdata);

	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
			const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned long object_id;

	((PTPData *) camera->pl->params.data)->context = context;

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, object_id, data);
	object_id = find_child(filename, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	CPR (context, ptp_deleteobject(&camera->pl->params,
		camera->pl->params.handles.handler[object_id],0));

	return (GP_OK);
}

static int
remove_dir_func (CameraFilesystem *fs, const char *folder,
			const char *foldername, void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned long object_id;

	((PTPData *) camera->pl->params.data)->context = context;

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, object_id, data);
	object_id = find_child(foldername, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	CPR (context, ptp_deleteobject(&camera->pl->params,
		camera->pl->params.handles.handler[object_id],0));

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	PTPObjectInfo *oi;
	uint32_t object_id;

	((PTPData *) camera->pl->params.data)->context = context;

	/* Get file number omiting storage pseudofolder */
	find_folder_handle(folder, object_id, data);
	object_id = find_child(filename, object_id, camera);
	if ((object_id=handle_to_n(object_id, camera))==PTP_HANDLER_SPECIAL)
		return (GP_ERROR_BAD_PARAMETERS);

	oi=&camera->pl->params.objectinfo[object_id];
/*
	GP_DEBUG ("ObjectInfo for '%s':");
	GP_DEBUG ("  StorageID: %d", oi->StorageID);
	GP_DEBUG ("  ObjectFormat: %d", oi->ObjectFormat);
	GP_DEBUG ("  ObjectCompressedSize: %d", oi->ObjectCompressedSize);
	GP_DEBUG ("  ThumbFormat: %d", oi->ThumbFormat);
	GP_DEBUG ("  ThumbCompressedSize: %d", oi->ThumbCompressedSize);
	GP_DEBUG ("  ThumbPixWidth: %d", oi->ThumbPixWidth);
	GP_DEBUG ("  ThumbPixHeight: %d", oi->ThumbPixHeight);
	GP_DEBUG ("  ImagePixWidth: %d", oi->ImagePixWidth);
	GP_DEBUG ("  ImagePixHeight: %d", oi->ImagePixHeight);
	GP_DEBUG ("  ImageBitDepth: %d", oi->ImageBitDepth);
	GP_DEBUG ("  ParentObject: %d", oi->ParentObject);
	GP_DEBUG ("  AssociationType: %d", oi->AssociationType);
	GP_DEBUG ("  AssociationDesc: %d", oi->AssociationDesc);
	GP_DEBUG ("  SequenceNumber: %d", oi->SequenceNumber);
*/
	info->file.fields = GP_FILE_INFO_SIZE|GP_FILE_INFO_TYPE|GP_FILE_INFO_MTIME;

	info->file.size   = oi->ObjectCompressedSize;
	strcpy_mime (info->file.type, oi->ObjectFormat);
	info->file.mtime = oi->ModificationDate;

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
	uint32_t store;
	uint32_t handle;

	((PTPData *) camera->pl->params.data)->context = context;
	memset(&oi, 0, sizeof (PTPObjectInfo));

	/* get parent folder id omiting storage pseudofolder */
	find_folder_handle(folder,parent,data);

	/* try to obtain StorageID from parent object */
	if ( parent != PTP_HANDLER_ROOT ) {
		int n = handle_to_n(parent,camera);
		store = camera->pl->params.objectinfo[n].StorageID;
	} else
		store=0x00020001;  // FIXME

	/* if you desire to put file to root folder, you have to use
	 * 0xffffffff instead of 0x00000000 (which means responder decide).
	 */
	if (parent==PTP_HANDLER_ROOT) parent=PTP_HANDLER_SPECIAL;

	oi.Filename=(char *)foldername;

	oi.ObjectFormat=PTP_OFC_Association;
	oi.ProtectionStatus=PTP_PS_NoProtection;
	oi.AssociationType=PTP_AT_GenericFolder;

	CPR (context, ptp_ek_sendfileobjectinfo (&camera->pl->params,
		&store, &parent, &handle, &oi));
	/* to create folder on kodak camera you don't have to sendfileobject */
#if 0
	CPR (context, ptp_ek_sendfileobject (&camera->pl->params,
		&oi, 18));
#endif
	return (GP_OK);
}

static int
init_ptp_fs (Camera *camera, GPContext *context)
{
	int i,id;

	((PTPData *) camera->pl->params.data)->context = context;

	/* Get file handles array for filesystem */
	id = gp_context_progress_start (context, 0, "Initializing Camera");
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
	for (i = 0; i < camera->pl->params.handles.n; i++) {
		CPR (context, ptp_getobjectinfo(&camera->pl->params,
			camera->pl->params.handles.handler[i],
			&camera->pl->params.objectinfo[i]));
		gp_context_progress_update (context, id,
		(90*i)/camera->pl->params.handles.n);
	}
	gp_context_progress_stop (context, id);

/*
	add_dir (camera, 0x00000000, 0xff000000, "DIR1");
	add_dir (camera, 0x00000000, 0xff000001, "DIR20");
	add_dir (camera, 0xff000000, 0xff000002, "subDIR1");
	add_dir (camera, 0xff000002, 0xff000003, "subsubDIR1");
	move_object_by_number (camera, 0xff000002, 2);
	move_object_by_number (camera, 0xff000001, 3);
	move_object_by_number (camera, 0xff000002, 4);
	// Used for testing with my camera, which does not support subdirs
*/
	return (GP_OK);
}


int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	short ret;

	/* Make sure our port is a USB port */
	if (camera->port->type != GP_PORT_USB) {
		gp_context_error (context, _("PTP is implemented for "
			"USB cameras only."));
		return (GP_ERROR_UNKNOWN_PORT);
	}

	camera->functions->about = camera_about;
	camera->functions->exit = camera_exit;

	/* We need some data that we pass around */
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	camera->pl->params.write_func = ptp_write_func;
	camera->pl->params.read_func  = ptp_read_func;
	camera->pl->params.debug_func = ptp_debug_func;
	camera->pl->params.error_func = ptp_error_func;
	camera->pl->params.data = malloc (sizeof (PTPData));
	memset (camera->pl->params.data, 0, sizeof (PTPData));
	((PTPData *) camera->pl->params.data)->camera = camera;
	camera->pl->params.byteorder = PTP_DL_LE;
	/* the transaction_id is incemente before sending, thus it means 0x0 */
	camera->pl->params.transaction_id=0xffffffff;

	/* On large fiels (over 50M) deletion takes over 3 seconds */
	CR (gp_port_set_timeout (camera->port, 6000));
	/* Configure the port */
	CR (gp_port_get_settings (camera->port, &settings));

	/* Use the defaults the core parsed */
	CR (gp_port_set_settings (camera->port, settings));

	/* Establish a connection to the camera */
	((PTPData *) camera->pl->params.data)->context = context;
	ret=ptp_opensession (&camera->pl->params, 1);
	while (ret==PTP_RC_InvalidTransactionID) {
		camera->pl->params.transaction_id+=10;
		ret=ptp_opensession (&camera->pl->params, 1);
	}
	if (ret!=PTP_RC_SessionAlreadyOpened && ret!=PTP_RC_OK) {
		report_result(context, ret);
		return (translate_ptp_result(ret));
	}

	/* init internal ptp objectfiles (required for fs implementation) */
	init_ptp_fs (camera, context);


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
