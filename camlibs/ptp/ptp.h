/* ptp.h
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

#ifndef __PTP_H__
#define __PTP_H__

#include <stdarg.h>
#include <gphoto2-endian.h>

// PTP request/response/data packet structure

#define PTP_REQ_LEN			30
#define PTP_RESP_LEN			sizeof(PTPReq)
#define PTP_REQ_HDR_LEN                 (2*sizeof(uint32_t)+2*sizeof(uint16_t))
#define PTP_REQ_DATALEN			16384

typedef struct _PTPReq PTPReq;
struct _PTPReq {
	uint32_t len;
	uint16_t type;
	uint16_t code;
	uint32_t trans_id;
	char data[PTP_REQ_DATALEN];
};

// Container types

#define PTP_TYPE_REQ                    (htole16(0x0001))
#define PTP_TYPE_DATA                   (htole16(0x0002))
#define PTP_TYPE_RESP                   (htole16(0x0003))

// Operation Codes

#define PTP_OC_Undefined                (htole16(0x1000))
#define PTP_OC_GetDevInfo               (htole16(0x1001))
#define PTP_OC_OpenSession              (htole16(0x1002))
#define PTP_OC_CloseSession             (htole16(0x1003))
#define PTP_OC_GetStorageIDs            (htole16(0x1004))
#define PTP_OC_GetStorageInfo           (htole16(0x1005))
#define PTP_OC_GetNumObjects            (htole16(0x1006))
#define PTP_OC_GetObjectHandles         (htole16(0x1007))
#define PTP_OC_GetObjectInfo            (htole16(0x1008))
#define PTP_OC_GetObject                (htole16(0x1009))
#define PTP_OC_GetThumb                 (htole16(0x100A))
#define PTP_OC_DeleteObject             (htole16(0x100B))
#define PTP_OC_SendObjectInfo           (htole16(0x100C))
#define PTP_OC_SendObject               (htole16(0x100D))
#define PTP_OC_InitiateCapture          (htole16(0x100E))
#define PTP_OC_FormatStore              (htole16(0x100F))
#define PTP_OC_ResetDevice              (htole16(0x1010))
#define PTP_OC_SelfTest                 (htole16(0x1011))
#define PTP_OC_SetObjectProtection      (htole16(0x1012))
#define PTP_OC_PowerDown                (htole16(0x1013))
#define PTP_OC_GetDevicePropDesc        (htole16(0x1014))
#define PTP_OC_GetDevicePropValue       (htole16(0x1015))
#define PTP_OC_SetDevicePropValue       (htole16(0x1016))
#define PTP_OC_ResetDevicePropValue     (htole16(0x1017))
#define PTP_OC_TerminateOpenCapture     (htole16(0x1018))
#define PTP_OC_MoveObject               (htole16(0x1019))
#define PTP_OC_CopyObject               (htole16(0x101A))
#define PTP_OC_GetPartialObject         (htole16(0x101B))
#define PTP_OC_InitiateOpenCapture      (htole16(0x101C))
// Eastman Kodak extension Operation Codes
#define PTP_OC_EK_SendFileObjectInfo	(htole16(0x9005))
#define PTP_OC_EK_SendFileObject	(htole16(0x9006))

// Response Codes

#define PTP_RC_Undefined                (htole16(0x2000))
#define PTP_RC_OK                       (htole16(0x2001))
#define PTP_RC_GeneralError             (htole16(0x2002))
#define PTP_RC_SessionNotOpen           (htole16(0x2003))
#define PTP_RC_InvalidTransactionID	(htole16(0x2004))
#define PTP_RC_OperationNotSupported    (htole16(0x2005))
#define PTP_RC_ParameterNotSupported    (htole16(0x2006))
#define PTP_RC_IncompleteTransfer       (htole16(0x2007))
#define PTP_RC_InvalidStorageId         (htole16(0x2008))
#define PTP_RC_InvalidObjectHandle      (htole16(0x2009))
#define PTP_RC_DevicePropNotSupported   (htole16(0x200A))
#define PTP_RC_InvalidObjectFormatCode  (htole16(0x200B))
#define PTP_RC_StoreFull                (htole16(0x200C))
#define PTP_RC_ObjectWriteProtected     (htole16(0x200D))
#define PTP_RC_StoreReadOnly            (htole16(0x200E))
#define PTP_RC_AccessDenied             (htole16(0x200F))
#define PTP_RC_NoThumbnailPresent       (htole16(0x2010))
#define PTP_RC_SelfTestFailed           (htole16(0x2011))
#define PTP_RC_PartialDeletion          (htole16(0x2012))
#define PTP_RC_StoreNotAvailable        (htole16(0x2013))
#define PTP_RC_SpecyficationByFormatUnsupported         (htole16(0x2014))
#define PTP_RC_NoValidObjectInfo        (htole16(0x2015))
#define PTP_RC_InvalidCodeFormat        (htole16(0x2016))
#define PTP_RC_UnknownVendorCode        (htole16(0x2017))
#define PTP_RC_CaptureAlreadyTerminated (htole16(0x2018))
#define PTP_RC_DeviceBusy               (htole16(0x2019))
#define PTP_RC_InvalidParentObject      (htole16(0x201A))
#define PTP_RC_InvalidDevicePropFormat  (htole16(0x201B))
#define PTP_RC_InvalidDevicePropValue   (htole16(0x201C))
#define PTP_RC_InvalidParameter         (htole16(0x201D))
#define PTP_RC_SessionAlreadyOpened     (htole16(0x201E))
#define PTP_RC_TransactionCanceled      (htole16(0x201F))
#define PTP_RC_SpecificationOfDestinationUnsupported            (htole16(0x2020))
// Eastman Kodak extension Response Codes
#define PTP_RC_EK_FilenameRequired	(htole16(0xA001))
#define PTP_RC_EK_FilenameConflicts	(htole16(0xA002))
#define PTP_RC_EK_FilenameInvalid	(htole16(0xA003))

// PTP extended ERROR codes

#define PTP_ERROR_IO			(htole16(0x2FF))
#define PTP_ERROR_DATA_EXPECTED		(htole16(0x2FE))
#define PTP_ERROR_RESP_EXPECTED		(htole16(0x2FD))
#define PTP_ERROR_BADPARAM		(htole16(0x2fC))

// PTP device info structure (returned by GetDevInfo)

typedef struct _PTPDeviceInfo PTPDedviceInfo;
struct _PTPDeviceInfo {
	uint16_t StaqndardVersion __attribute__ ((packed));
	uint32_t VendorExtensionID __attribute__ ((packed));
	uint16_t VendorExtensionDesc __attribute__ ((packed));
	char data [PTP_REQ_DATALEN-(2*sizeof(uint16_t))-sizeof(uint32_t)]
	__attribute__ ((packed));
};


// PTP objecthandles structure (returned by GetObjectHandles)

typedef struct _PTPObjectHandles PTPObjectHandles;
struct _PTPObjectHandles {
	uint32_t n;
	uint32_t handler[(PTP_REQ_DATALEN-sizeof(uint32_t))/sizeof(uint32_t)];
};


// PTP objectinfo structure (returned by GetObjectInfo)

typedef struct _PTPObjectInfo PTPObjectInfo;
struct _PTPObjectInfo {
	uint32_t StorageID __attribute__ ((packed));
	uint16_t ObjectFormat __attribute__ ((packed));
	uint16_t ProtectionStatus __attribute__ ((packed));
	uint32_t ObjectCompressedSize __attribute__ ((packed));
	uint16_t ThumbFormat __attribute__ ((packed));
	uint32_t ThumbCompressedSize __attribute__ ((packed));
	uint32_t ThumbPixWidth __attribute__ ((packed));
	uint32_t ThumbPixHeight __attribute__ ((packed));
	uint32_t ImagePixWidth __attribute__ ((packed));
	uint32_t ImagePixHeight __attribute__ ((packed));
	uint32_t ImageBitDepth __attribute__ ((packed));
	uint32_t ParentObject __attribute__ ((packed));
	uint16_t AssociationType __attribute__ ((packed));
	uint32_t AssociationDesc __attribute__ ((packed));
	uint32_t SequenceNumber __attribute__ ((packed));
	uint8_t  filenamelen __attribute__ ((packed));	// makes reading easyier
	char data[PTP_REQ_DATALEN-53] __attribute__ ((packed));
};

#define MAXFILENAMELEN			255

// PTP Object Format Codes

// ancillary formats
#define PTP_OFC_Undefined			(htole16(0x3000))
#define PTP_OFC_Association			(htole16(0x3001))
#define PTP_OFC_Script				(htole16(0x3002))
#define PTP_OFC_Executable			(htole16(0x3003))
#define PTP_OFC_Text				(htole16(0x3004))
#define PTP_OFC_HTML				(htole16(0x3005))
#define PTP_OFC_DPOF				(htole16(0x3006))
#define PTP_OFC_AIFF	 			(htole16(0x3007))
#define PTP_OFC_WAV				(htole16(0x3008))
#define PTP_OFC_MP3				(htole16(0x3009))
#define PTP_OFC_AVI				(htole16(0x300A))
#define PTP_OFC_MPEG				(htole16(0x300B))
#define PTP_OFC_ASF				(htole16(0x300D))
// image formats
#define PTP_OFC_EXIF_JPEG			(htole16(0x3801))
#define PTP_OFC_TIFF_EP				(htole16(0x3802))
#define PTP_OFC_FlashPix			(htole16(0x3803))
#define PTP_OFC_BMP				(htole16(0x3804))
#define PTP_OFC_CIFF				(htole16(0x3805))
#define PTP_OFC_Undefined_0x3806		(htole16(0x3806))
#define PTP_OFC_GIF				(htole16(0x3807))
#define PTP_OFC_JFIF				(htole16(0x3808))
#define PTP_OFC_PCD				(htole16(0x3809))
#define PTP_OFC_PICT				(htole16(0x380A))
#define PTP_OFC_PNG				(htole16(0x380B))
#define PTP_OFC_Undefined_0x380C		(htole16(0x380C))
#define PTP_OFC_TIFF				(htole16(0x380D))
#define PTP_OFC_TIFF_IT				(htole16(0x380E))
#define PTP_OFC_JP2				(htole16(0x380F))
#define PTP_OFC_JPX				(htole16(0x3810))
// Eastman Kodak extension ancillary format
#define PTP_OFC_EK_M3U				(htole16(0xb002))


// PTP Association Types

#define PTP_AT_Undefined			(htole16(0x0000))
#define PTP_AT_GenericFolder			(htole16(0x0001))
#define PTP_AT_Album				(htole16(0x0002))
#define PTP_AT_TimeSequence			(htole16(0x0003))
#define PTP_AT_HorizontalPanoramic		(htole16(0x0004))
#define PTP_AT_VerticalPanoramic		(htole16(0x0005))
#define PTP_AT_2DPanoramic			(htole16(0x0006))
#define PTP_AT_AncillaryData			(htole16(0x0007))

// PTP Protection Status

#define PTP_PS_NoProtection			(htole16(0x0000))
#define PTP_PS_ReadOnly				(htole16(0x0001))

// Glue stuff

typedef short (* PTPIOReadFunc)  (unsigned char *bytes, unsigned int size,
				  void *data);
typedef short (* PTPIOWriteFunc) (unsigned char *bytes, unsigned int size,
				  void *data);
typedef void (* PTPErrorFunc) (void *data, const char *format, va_list args);
typedef void (* PTPDebugFunc) (void *data, const char *format, va_list args);

typedef struct _PTPParams PTPParams;
struct _PTPParams {

	/* Custom IO functions */
	PTPIOReadFunc  read_func;
	PTPIOWriteFunc write_func;

	/* Custom error and debug function */
	PTPErrorFunc error_func;
	PTPDebugFunc debug_func;

	/* Data passed to above functions */
	void *data;

	/* Used by libptp */
	uint32_t transaction_id;
	PTPObjectHandles handles;
};


// ptp functions

uint16_t ptp_opensession  (PTPParams *params, uint32_t session);
uint16_t ptp_closesession (PTPParams *params);

uint16_t ptp_getobjecthandles(PTPParams *params,
			PTPObjectHandles* objecthandles, uint32_t store);
uint16_t ptp_getobjectinfo	(PTPParams *params, uint32_t handle,
			PTPObjectInfo* objectinfo);
uint16_t ptp_getobject	(PTPParams *params, uint32_t handle,
			uint32_t size, PTPReq* object);
uint16_t ptp_getthumb	(PTPParams *params, uint32_t handle,
			uint32_t size, PTPReq* object);
uint16_t ptp_deleteobject	(PTPParams* params, uint32_t handle,
			uint32_t ofc);
uint16_t ptp_ek_sendfileobjectinfo (PTPParams* params, uint32_t* store,
			uint32_t* parenthandle, uint32_t* handle,
			PTPObjectInfo* objectinfo);
uint16_t ptp_ek_sendfileobject (PTPParams* params, PTPReq* object,
			uint32_t size);
void ptp_getobjectfilename	(PTPObjectInfo* objectinfo, char* filename);
time_t ptp_getobjectcapturedate	(PTPObjectInfo* objectinfo);

#endif /* __PTP_H__ */
