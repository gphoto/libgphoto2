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
#include <gphoto2-debug.h>
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

#define CPR(camera,result) {short r=(result); if (r!=PTP_RC_OK) {report_result ((camera), r); return (translate_ptp_result (r));}}

#define CPR_free(camera,result, freeptr) {\
			short r=(result);\
			if (r!=PTP_RC_OK) {\
				report_result ((camera), r);\
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
report_result (Camera *camera, short result)
{
	unsigned int i;

	for (i = 0; ptp_errors[i].txt; i++)
		if (ptp_errors[i].n == result)
			gp_camera_set_error (camera, ptp_errors[i].txt);
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
//	{"Kodak DC-240 (PTP)",  0x040a, 0x0121}, /* Special firmware */
	{"Kodak DC-4800", 0x040a, 0x0160},
/*
	{"Kodak DC-3215", 0x040a, 0x0525},
	{"Kodak DX-3500", 0x040a, 0x0500},
	{"Kodak DX-3600", 0x040a, 0x0510},
	{"Kodak DX-3700", 0x040a, 0x0530},
	{"Kodak DX-3900", 0x040a, 0x0170},
	{"Kodak MC3", 0, 0},
	{"Sony DSC-P5", 0, 0},
	{"Sony DSC-F707", 0x054c, 0x004e},
	{"HP PhotoSmart 318", 0x03f0, 0x6302},
*/	{NULL, 0, 0}
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
set_mimetype (Camera *camera, CameraFile *file, unsigned short ofc)
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
strcpy_mime(char * dest, unsigned short ofc) {
	int i;

	for (i = 0; object_formats[i].format_code; i++)
		if (object_formats[i].format_code == ofc)
			strcpy(dest, object_formats[i].txt);

}
	
struct _CameraPrivateLibrary {
	PTPParams params;
};

static short
ptp_read_func (unsigned char *bytes, unsigned int size, void *data)
{
	Camera *camera = data;
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
	Camera *camera = data;
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

static void
ptp_debug_func (void *data, const char *format, va_list args)
{
	gp_logv (GP_LOG_DEBUG, "ptp", format, args);
}

static void
ptp_error_func (void *data, const char *format, va_list args)
{
	Camera *camera = data;
	char buf[2048];

	vsnprintf (buf, sizeof (buf), format, args);
	gp_camera_set_error (camera, "%s", buf);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	memset(&a,0, sizeof(a));
	for (i = 0; models[i].model; i++) {
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port   = GP_PORT_USB;
		a.speed[0] = 0;
		a.usb_vendor = models[i].usb_vendor;
		a.usb_product= models[i].usb_product;
		a.operations        = GP_OPERATION_NONE;
		a.file_operations   = GP_FILE_OPERATION_PREVIEW|
					GP_FILE_OPERATION_DELETE;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		CR (gp_abilities_list_append (list, a));
		memset(&a,0, sizeof(a));
	}

	strcpy(a.model, "USB PTP Class Camera");
	a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port   = GP_PORT_USB;
	a.speed[0] = 0;
	a.usb_class = 6;
	a.usb_subclass = -1;
	a.usb_protocol = -1;
	a.operations        = GP_OPERATION_NONE;
	a.file_operations   = GP_FILE_OPERATION_PREVIEW|
				GP_FILE_OPERATION_DELETE;
	a.folder_operations = GP_FOLDER_OPERATION_NONE;
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
camera_exit (Camera *camera)
{
	if (camera->pl) {
		ptp_closesession (&camera->pl->params);
		free (camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *text)
{
	strncpy (text->text,
		 _("Written by Mariusz Woloszyn <emsi@ipartners.pl>. "
		   "Enjoy!"), sizeof (text->text));
	return (GP_OK);
}

static void
add_dir (Camera *camera, uint32_t parent, uint32_t handle, const char *filename)
{
	int n;
	n=camera->pl->params.handles.n++;
	camera->pl->params.objectinfo = (PTPObjectInfo*)
		realloc(camera->pl->params.objectinfo,
		sizeof(PTPObjectInfo)*(n+1));
	camera->pl->params.handles.handler[n]=handle;

	camera->pl->params.objectinfo[n].Filename=malloc(strlen(filename)+1);
	strcpy(camera->pl->params.objectinfo[n].Filename, filename);
	camera->pl->params.objectinfo[n].ObjectFormat=PTP_OFC_Association;
	camera->pl->params.objectinfo[n].AssociationType=PTP_AT_GenericFolder;
	
	camera->pl->params.objectinfo[n].ParentObject=parent;
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
	return 0xffffffff;  // NOT FOUND
}


static uint32_t
folder_to_handle(const char *folder, uint32_t parent, Camera *camera)
{
	char *c;
	if (!strlen(folder)) return 0x00000000;
	if (!strcmp(folder,"/")) return 0x00000000;

	c=strchr(folder+1,'/');
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
		void *data)
{
	Camera *camera = data;
	uint32_t parent;
	char *backfolder=malloc(strlen(folder));
	int i;
	
	memcpy(backfolder,folder+1, strlen(folder));
	parent=folder_to_handle(backfolder,0, camera);
	free(backfolder);
	if (!strcmp(folder,"/"))
	for (i = 0; i < camera->pl->params.handles.n; i++) {
	if (( camera->pl->params.objectinfo[i].ObjectFormat & 0x0800) != 0)
	if (camera->pl->params.objectinfo[i].ParentObject==parent)
		CR (gp_list_append (list, camera->pl->params.objectinfo[i].Filename, NULL));
	}

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data)
{
	Camera *camera = data;
	PTPObjectInfo * oi = ((Camera*)data)->pl->params.objectinfo;
	int i;
	uint32_t parent;
	char *backfolder=malloc(strlen(folder));

	memcpy(backfolder,folder+1, strlen(folder));
	parent=folder_to_handle(backfolder,0, camera);
	free(backfolder);
	gp_filesystem_dump(fs);
	for (i = 0; i < ((Camera*)data)->pl->params.handles.n; i++) {
	if (oi[i].ObjectFormat==PTP_OFC_Association &&
		oi[i].AssociationType==PTP_AT_GenericFolder)
	if (camera->pl->params.objectinfo[i].ParentObject==parent)
			CR (gp_list_append (list, oi[i].Filename, NULL));
	}
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data)
{
	Camera *camera = data;
	PTPReq *fdata = NULL;
	char * image;
	unsigned long image_id;
	uint32_t size;


	// Get file number
	image_id = gp_filesystem_number (fs, folder, filename);

	// don't try to download ancillary objects!
	if (( camera->pl->params.objectinfo[image_id].ObjectFormat & 0x0800) == 0) return (GP_OK);
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		size=camera->pl->params.objectinfo[image_id].ObjectCompressedSize;
		fdata=malloc(size+PTP_REQ_HDR_LEN);
		CPR_free (camera, ptp_getobject(&camera->pl->params,
			camera->pl->params.handles.handler[image_id],
			size, fdata), fdata);
		image=malloc(size);
		memcpy(image, fdata->data, size);
		free(fdata);
		CR (gp_file_set_data_and_size (file, image, size));
		// XXX does gp_file_set_data_and_size free image ptr upon
		// failure??
		break;

	case GP_FILE_TYPE_PREVIEW:
		size=camera->pl->params.objectinfo[image_id].ThumbCompressedSize;
		fdata=malloc(size+PTP_REQ_HDR_LEN);
		CPR_free (camera, ptp_getthumb(&camera->pl->params,
			camera->pl->params.handles.handler[image_id],
			size, fdata), fdata);
		image=malloc(size);
		memcpy(image, fdata->data, size);
		free(fdata);
		CR (gp_file_set_data_and_size (file, image, size));
		// XXX does gp_file_set_data_and_size free image ptr upon
		// failure??
		break;
		

	default:

	}

	CR (set_mimetype (camera, file, camera->pl->params.objectinfo[image_id].ObjectFormat));

	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
			const char *filename, void *data)
{
	Camera *camera = data;

	unsigned long image_id;

	if (strcmp (folder, "/"))
		return (GP_ERROR_DIRECTORY_NOT_FOUND);

	// Get file number
	image_id = gp_filesystem_number (fs, folder, filename);

	CPR (camera, ptp_deleteobject(&camera->pl->params,
		camera->pl->params.handles.handler[image_id],0));

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data)
{
	Camera *camera = data;
	PTPObjectInfo *oi;
	int n;

	CR (n = gp_filesystem_number (fs, folder, filename));
	oi=&camera->pl->params.objectinfo[n];
/*	GP_DEBUG ("ObjectInfo for '%s':");
	GP_DEBUG ("  StorageID: %d", oi.StorageID);
	GP_DEBUG ("  ObjectFormat: %d", oi.ObjectFormat);
	GP_DEBUG ("  ObjectCompressedSize: %d", oi.ObjectCompressedSize);
	GP_DEBUG ("  ThumbFormat: %d", oi.ThumbFormat);
	GP_DEBUG ("  ThumbCompressedSize: %d", oi.ThumbCompressedSize);
	GP_DEBUG ("  ThumbPixWidth: %d", oi.ThumbPixWidth);
	GP_DEBUG ("  ThumbPixHeight: %d", oi.ThumbPixHeight);
	GP_DEBUG ("  ImagePixWidth: %d", oi.ImagePixWidth);
	GP_DEBUG ("  ImagePixHeight: %d", oi.ImagePixHeight);
	GP_DEBUG ("  ImageBitDepth: %d", oi.ImageBitDepth);
	GP_DEBUG ("  ParentObject: %d", oi.ParentObject);
	GP_DEBUG ("  AssociationType: %d", oi.AssociationType);
	GP_DEBUG ("  AssociationDesc: %d", oi.AssociationDesc);
	GP_DEBUG ("  SequenceNumber: %d", oi.SequenceNumber);
*/
	info->file.fields = GP_FILE_INFO_SIZE|GP_FILE_INFO_TYPE;

	info->file.size   = oi->ObjectCompressedSize;
	strcpy_mime (info->file.type, oi->ObjectFormat);

	if ((oi->ObjectFormat & 0x0800) != 0) {
		info->preview.fields = GP_FILE_INFO_SIZE|GP_FILE_INFO_WIDTH
				|GP_FILE_INFO_HEIGHT|GP_FILE_INFO_TIME|GP_FILE_INFO_TYPE;
		strcpy_mime(info->preview.type, oi->ThumbFormat);
		info->preview.size   = oi->ThumbCompressedSize;
		info->preview.width  = oi->ThumbPixWidth;
		info->preview.height = oi->ThumbPixHeight;

		info->file.fields = info->file.fields|GP_FILE_INFO_WIDTH|GP_FILE_INFO_HEIGHT|
				GP_FILE_INFO_TIME;

		info->file.width  = oi->ImagePixWidth;
		info->file.height = oi->ImagePixHeight;
		info->file.time = oi->CaptureDate;
	}

		return (GP_OK);
}

static int
make_dir_func (CameraFilesystem *fs, const char *folder, const char *foldername,
	       void *data)
{
	Camera *camera = data;
	PTPObjectInfo oi;
	uint32_t parent;
	uint32_t store;
	uint32_t handle;

	memset(&oi, 0, sizeof (PTPObjectInfo));

	// blackmagic, obtain ObjectHandle of upper folder
	// XXX 0xffffffff=root filesystem
	parent=0x00000000;

	// any store (responder decides)
	store=0xffffffff;
	
	oi.Filename=foldername;

	oi.StorageID=0xffffffff;
	oi.ObjectFormat=PTP_OFC_Association;
	oi.ProtectionStatus=PTP_PS_NoProtection;
	oi.AssociationType=PTP_AT_GenericFolder;

	CPR (camera, ptp_ek_sendfileobjectinfo (&camera->pl->params,
		&store, &parent, &handle, &oi));
	//CPR (camera, ptp_ek_sendfileobject (&camera->pl->params,
	//	&oi, 18));
	return (GP_OK);
}

static int
init_ptp_fs (Camera *camera)
{
	int i;

	/* Get file handles array for filesystem */
	CPR (camera, ptp_getobjecthandles (&camera->pl->params, &camera->pl->params.handles, 0xffffffff)); // XXX return from all stores

	// wee need that for fileststem :/
	camera->pl->params.objectinfo = (PTPObjectInfo*)malloc(sizeof(PTPObjectInfo)*camera->pl->params.handles.n);
	for (i = 0; i < camera->pl->params.handles.n; i++) {
		CPR (camera, ptp_getobjectinfo(&camera->pl->params,
		camera->pl->params.handles.handler[i], &camera->pl->params.objectinfo[i]));
	}

	add_dir (camera, 0x00000000, 0xff000000, "DIR1");
	add_dir (camera, 0x00000000, 0xff000001, "DIR20");
	add_dir (camera, 0xff000000, 0xff000002, "subDIR1");
	add_dir (camera, 0xff000002, 0xff000003, "subsubDIR1");

	return (GP_OK);
}


int
camera_init (Camera *camera)
{
	GPPortSettings settings;
	short ret;

	/* Make sure our port is a USB port */
	if (camera->port->type != GP_PORT_USB) {
		gp_camera_set_error (camera, _("PTP is implemented for "
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
	camera->pl->params.byteorder = PTP_DL_LE;
	camera->pl->params.data = camera;
	camera->pl->params.transaction_id=0x01;

	/* Configure the port */
	CR (gp_port_set_timeout (camera->port, 3000));
	CR (gp_port_get_settings (camera->port, &settings));

	/* Use the defaults the core parsed */
	CR (gp_port_set_settings (camera->port, settings));

	/* Establish a connection to the camera */
	ret=ptp_opensession (&camera->pl->params, 1);
	while (ret==PTP_RC_InvalidTransactionID) {
		camera->pl->params.transaction_id+=10;
		ret=ptp_opensession (&camera->pl->params, 1);
	}
	if (ret!=PTP_RC_SessionAlreadyOpened && ret!=PTP_RC_OK) {
		report_result(camera, ret);
		return (translate_ptp_result(ret));
	}

	// init internal ptp objectfiles (required for fs implementation)
	init_ptp_fs (camera);


	/* Configure the CameraFilesystem */
	CR (gp_filesystem_set_list_funcs (camera->fs, file_list_func,
					  folder_list_func, camera));
	CR (gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL,
					  camera));
	CR (gp_filesystem_set_file_funcs (camera->fs, get_file_func,
					  delete_file_func, camera));
	CR (gp_filesystem_set_folder_funcs (camera->fs, NULL,
					    NULL, make_dir_func,
					    NULL, camera));

	return (GP_OK);
}
