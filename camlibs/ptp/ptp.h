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

// Container types

#define PTP_TYPE_REQ                    0x0001
#define PTP_TYPE_DATA                   0x0002
#define PTP_TYPE_RESP                   0x0003

// Operation Codes

#define PTP_OC_Undefined                0x1000
#define PTP_OC_GetDevInfo               0x1001
#define PTP_OC_OpenSession              0x1002
#define PTP_OC_CloseSession             0x1003
#define PTP_OC_GetStorageIDs            0x1004
#define PTP_OC_GetStorageInfo           0x1005
#define PTP_OC_GetNumObjects            0x1006
#define PTP_OC_GetObjectHandles         0x1007
#define PTP_OC_GetObjectInfo            0x1008
#define PTP_OC_GetObject                0x1009
#define PTP_OC_GetThumb                 0x100A
#define PTP_OC_DeleteObject             0x100B
#define PTP_OC_SendObjectInfo           0x100C
#define PTP_OC_SendObject               0x100D
#define PTP_OC_InitiateCapture          0x100E
#define PTP_OC_FormatStore              0x100F
#define PTP_OC_ResetDevice              0x1010
#define PTP_OC_SelfTest                 0x1011
#define PTP_OC_SetObjectProtection      0x1012
#define PTP_OC_PowerDown                0x1013
#define PTP_OC_GetDevicePropDesc        0x1014
#define PTP_OC_GetDevicePropValue       0x1015
#define PTP_OC_SetDevicePropValue       0x1016
#define PTP_OC_ResetDevicePropValue     0x1017
#define PTP_OC_TerminateOpenCapture     0x1018
#define PTP_OC_MoveObject               0x1019
#define PTP_OC_CopyObject               0x101A
#define PTP_OC_GetPartialObject         0x101B
#define PTP_OC_InitiateOpenCapture      0x101C

// Response Codes

#define PTP_RC_Undefined                0x2000
#define PTP_RC_OK                       0x2001
#define PTP_RC_GeneralError             0x2002
#define PTP_RC_SessionNotOpen           0x2003
#define PTP_RC_InvalidTransactionID	0x2004
#define PTP_RC_OperationNotSupported    0x2005
#define PTP_RC_ParameterNotSupported    0x2006
#define PTP_RC_IncompleteTransfer       0x2007
#define PTP_RC_InvalidStorageId         0x2008
#define PTP_RC_InvalidObjectHandle      0x2009
#define PTP_RC_DevicePropNotSupported   0x200A
#define PTP_RC_InvalidObjectFormatCode  0x200B
#define PTP_RC_StoreFull                0x200C
#define PTP_RC_ObjectWriteProtected     0x200D
#define PTP_RC_StoreReadOnly            0x200E
#define PTP_RC_AccessDenied             0x200F
#define PTP_RC_NoThumbnailPresent       0x2010
#define PTP_RC_SelfTestFailed           0x2011
#define PTP_RC_PartialDeletion          0x2012
#define PTP_RC_StoreNotAvailable        0x2013
#define PTP_RC_SpecyficationByFormatUnsupported         0x2014
#define PTP_RC_NoValidObjectInfo        0x2015
#define PTP_RC_InvalidCodeFormat        0x2016
#define PTP_RC_UnknownVendorCode        0x2017
#define PTP_RC_CaptureAlreadyTerminated 0x2018
#define PTP_RC_DeviceBusy               0x2019
#define PTP_RC_InvalidParentObject      0x201A
#define PTP_RC_InvalidDevicePropFormat  0x201B
#define PTP_RC_InvalidDevicePropValue   0x201C
#define PTP_RC_InvalidParameter         0x201D
#define PTP_RC_SessionAlreadyOpened     0x201E
#define PTP_RC_TransactionCanceled      0x201F
#define PTP_RC_SpecificationOfDestinationUnsupported            0x2020

// PTP extended ERROR codes

#define PTP_ERROR_IO			0x2FF
#define PTP_ERROR_DATA_EXPECTED		0x2FE
#define PTP_ERROR_RESP_EXPECTED		0x2FD
#define PTP_ERROR_BADPARAM		0x2fC

#define PTP_REQ_HDR_LEN                 (2*sizeof(int)+2*sizeof(short))
#define PTP_REQ_DATALEN			16384

// PTP device info structure (returned by GetDevInfo)

typedef struct _PTPDeviceInfo PTPDedviceInfo;
struct _PTPDeviceInfo {
	short StaqndardVersion;
	int VendorExtensionID;
	short VendorExtensionDesc;
	char data [PTP_REQ_DATALEN-(2*sizeof(short))-sizeof(int)];
};


// PTP objecthandles structure (returned by GetObjectHandles)

typedef struct _PTPObjectHandles PTPObjectHandles;
struct _PTPObjectHandles {
	int n;
	int handler[(PTP_REQ_DATALEN-sizeof(int))/sizeof(int)];
};


// PTP objectinfo structure (returned by GetObjectInfo)

typedef struct _PTPObjectInfo PTPObjectInfo;
struct _PTPObjectInfo {
	int StorageID;
	short ObjectFormat;
	short ProtectionStatus;
	int ObjectCompressedSize;
	short ThumbFormat;
	int ThumbCompressedSize;
	int ThumbPixWidth;
	int ThumbPixHeight;
	int ImagePixWidth;
	int ImagePixHeight;
	int ImageBitDepth;
	int ParentObject;
	short AssociationType;
	int AssociationDesc;
	int SequenceNumber;
	char filenamelen;		// makes reading easyier
	char data[PTP_REQ_DATALEN-11*sizeof(int)-2*sizeof(short)-sizeof(char)];
};

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
	unsigned int transaction_id;
	PTPObjectHandles handles;
};


// ptp functions

short ptp_opensession  (PTPParams *params, int session);
short ptp_closesession (PTPParams *params);

short ptp_getobjecthandles (PTPParams *params, PTPObjectHandles* objecthandles);
short ptp_getobjectinfo   (PTPParams *params, PTPObjectHandles* objecthandles,
			    int n, PTPObjectInfo* objectinfoarray);
short ptp_getobject        (PTPParams *params, PTPObjectHandles* objecthandles,
			    PTPObjectInfo* objectinfoarray, int n,
			    char* object);
short ptp_getthumb         (PTPParams *params, PTPObjectHandles* objecthandles,
			    PTPObjectInfo* objectinfoarray, int n,
			    char* object);

#endif /* __PTP_H__ */
