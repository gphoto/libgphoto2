/* ptp.h
 *
 * Copyright (C) 2001 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2009 Marcus Meissner <marcus@jet.franken.de>
 * Copyright (C) 2006-2008 Linus Walleij <triad@df.lth.se>
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
#include <time.h>
#ifdef HAVE_ICONV
#include <iconv.h>
#endif
#include "gphoto2-endian.h"
#include "device-flags.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* PTP datalayer byteorder */

#define PTP_DL_BE			0xF0
#define	PTP_DL_LE			0x0F

/* USB interface class */
#ifndef USB_CLASS_PTP
#define USB_CLASS_PTP			6
#endif

/* PTP request/response/event general PTP container (transport independent) */

struct _PTPContainer {
	uint16_t Code;
	uint32_t SessionID;
	uint32_t Transaction_ID;
	/* params  may be of any type of size less or equal to uint32_t */
	uint32_t Param1;
	uint32_t Param2;
	uint32_t Param3;
	/* events can only have three parameters */
	uint32_t Param4;
	uint32_t Param5;
	/* the number of meaningfull parameters */
	uint8_t	 Nparam;
};
typedef struct _PTPContainer PTPContainer;

/* PTP USB Bulk-Pipe container */
/* USB bulk max packet length for high speed endpoints */
/* The max packet is set to 512 bytes. The spec says
 * "end of data transfers are signaled by short packets or NULL
 * packets". It never says anything about 512, but current
 * implementations seem to have chosen this value, which also
 * happens to be the size of an USB 2.0 HS endpoint, even though
 * this is not necessary.
 *
 * Previously we had this as 4096 for MTP devices. We have found
 * and fixed the bugs that made this necessary and it can be 512 again.
 */
#define PTP_USB_BULK_HS_MAX_PACKET_LEN_WRITE	512
#define PTP_USB_BULK_HS_MAX_PACKET_LEN_READ   512
#define PTP_USB_BULK_HDR_LEN		(2*sizeof(uint32_t)+2*sizeof(uint16_t))
#define PTP_USB_BULK_PAYLOAD_LEN_WRITE	(PTP_USB_BULK_HS_MAX_PACKET_LEN_WRITE-PTP_USB_BULK_HDR_LEN)
#define PTP_USB_BULK_PAYLOAD_LEN_READ	(PTP_USB_BULK_HS_MAX_PACKET_LEN_READ-PTP_USB_BULK_HDR_LEN)
#define PTP_USB_BULK_REQ_LEN	(PTP_USB_BULK_HDR_LEN+5*sizeof(uint32_t))

struct _PTPUSBBulkContainer {
	uint32_t length;
	uint16_t type;
	uint16_t code;
	uint32_t trans_id;
	union {
		struct {
			uint32_t param1;
			uint32_t param2;
			uint32_t param3;
			uint32_t param4;
			uint32_t param5;
		} params;
       /* this must be set to the maximum of PTP_USB_BULK_PAYLOAD_LEN_WRITE 
        * and PTP_USB_BULK_PAYLOAD_LEN_READ */
		unsigned char data[PTP_USB_BULK_PAYLOAD_LEN_READ];
	} payload;
};
typedef struct _PTPUSBBulkContainer PTPUSBBulkContainer;

/* PTP USB Asynchronous Event Interrupt Data Format */
struct _PTPUSBEventContainer {
	uint32_t length;
	uint16_t type;
	uint16_t code;
	uint32_t trans_id;
	uint32_t param1;
	uint32_t param2;
	uint32_t param3;
};
typedef struct _PTPUSBEventContainer PTPUSBEventContainer;

struct _PTPCanon_directtransfer_entry {
	uint32_t	oid;
	char		*str;
};
typedef struct _PTPCanon_directtransfer_entry PTPCanon_directtransfer_entry;

/* USB container types */

#define PTP_USB_CONTAINER_UNDEFINED		0x0000
#define PTP_USB_CONTAINER_COMMAND		0x0001
#define PTP_USB_CONTAINER_DATA			0x0002
#define PTP_USB_CONTAINER_RESPONSE		0x0003
#define PTP_USB_CONTAINER_EVENT			0x0004

/* PTP/IP definitions */
#define PTPIP_INIT_COMMAND_REQUEST	1
#define PTPIP_INIT_COMMAND_ACK		2
#define PTPIP_INIT_EVENT_REQUEST	3
#define PTPIP_INIT_EVENT_ACK		4
#define PTPIP_INIT_FAIL			5
#define PTPIP_CMD_REQUEST		6
#define PTPIP_CMD_RESPONSE		7
#define PTPIP_EVENT			8
#define PTPIP_START_DATA_PACKET		9
#define PTPIP_DATA_PACKET		10
#define PTPIP_CANCEL_TRANSACTION	11
#define PTPIP_END_DATA_PACKET		12
#define PTPIP_PING			13
#define PTPIP_PONG			14

struct _PTPIPHeader {
	uint32_t	length;
	uint32_t	type;
};
typedef struct _PTPIPHeader PTPIPHeader;

/* Vendor IDs */
#define PTP_VENDOR_EASTMAN_KODAK	0x00000001
#define PTP_VENDOR_SEIKO_EPSON		0x00000002
#define PTP_VENDOR_AGILENT		0x00000003
#define PTP_VENDOR_POLAROID		0x00000004
#define PTP_VENDOR_AGFA_GEVAERT		0x00000005
#define PTP_VENDOR_MICROSOFT		0x00000006
#define PTP_VENDOR_EQUINOX		0x00000007
#define PTP_VENDOR_VIEWQUEST		0x00000008
#define PTP_VENDOR_STMICROELECTRONICS	0x00000009
#define PTP_VENDOR_NIKON		0x0000000A
#define PTP_VENDOR_CANON		0x0000000B
  
/* Vendor extension ID used for MTP */
#define PTP_VENDOR_MTP			0xffffffff  

/* Operation Codes */

/* PTP v1.0 operation codes */
#define PTP_OC_Undefined                0x1000
#define PTP_OC_GetDeviceInfo            0x1001
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
/* PTP v1.1 operation codes */
#define PTP_OC_StartEnumHandles		0x101D
#define PTP_OC_EnumHandles		0x101E
#define PTP_OC_StopEnumHandles		0x101F
#define PTP_OC_GetVendorExtensionMaps	0x1020
#define PTP_OC_GetVendorDeviceInfo	0x1021
#define PTP_OC_GetResizedImageObject	0x1022
#define PTP_OC_GetFilesystemManifest	0x1023
#define PTP_OC_GetStreamInfo		0x1024
#define PTP_OC_GetStream		0x1025

/* Eastman Kodak extension Operation Codes */
#define PTP_OC_EK_GetSerial		0x9003
#define PTP_OC_EK_SetSerial		0x9004
#define PTP_OC_EK_SendFileObjectInfo	0x9005
#define PTP_OC_EK_SendFileObject	0x9006
#define PTP_OC_EK_SetText		0x9008

/* Canon extension Operation Codes */
#define PTP_OC_CANON_GetPartialObjectInfo	0x9001
/* 9002 - sends 2 uint32, nothing back  */
#define PTP_OC_CANON_SetObjectArchive		0x9002
#define PTP_OC_CANON_KeepDeviceOn		0x9003
#define PTP_OC_CANON_LockDeviceUI		0x9004
#define PTP_OC_CANON_UnlockDeviceUI		0x9005
#define PTP_OC_CANON_GetObjectHandleByName	0x9006
/* no 9007 observed yet */
#define PTP_OC_CANON_InitiateReleaseControl	0x9008
#define PTP_OC_CANON_TerminateReleaseControl	0x9009
#define PTP_OC_CANON_TerminatePlaybackMode	0x900A
#define PTP_OC_CANON_ViewfinderOn		0x900B
#define PTP_OC_CANON_ViewfinderOff		0x900C
#define PTP_OC_CANON_DoAeAfAwb			0x900D

/* 900e - send nothing, gets 5 uint16t in 32bit entities back in 20byte datablob */
#define PTP_OC_CANON_GetCustomizeSpec		0x900E
#define PTP_OC_CANON_GetCustomizeItemInfo	0x900F
#define PTP_OC_CANON_GetCustomizeData		0x9010
#define PTP_OC_CANON_SetCustomizeData		0x9011
#define PTP_OC_CANON_GetCaptureStatus		0x9012
#define PTP_OC_CANON_CheckEvent			0x9013
#define PTP_OC_CANON_FocusLock			0x9014
#define PTP_OC_CANON_FocusUnlock		0x9015
#define PTP_OC_CANON_GetLocalReleaseParam	0x9016
#define PTP_OC_CANON_SetLocalReleaseParam	0x9017
#define PTP_OC_CANON_AskAboutPcEvf		0x9018
#define PTP_OC_CANON_SendPartialObject		0x9019
#define PTP_OC_CANON_InitiateCaptureInMemory	0x901A
#define PTP_OC_CANON_GetPartialObjectEx		0x901B
#define PTP_OC_CANON_SetObjectTime		0x901C
#define PTP_OC_CANON_GetViewfinderImage		0x901D
#define PTP_OC_CANON_GetObjectAttributes	0x901E
#define PTP_OC_CANON_ChangeUSBProtocol		0x901F
#define PTP_OC_CANON_GetChanges			0x9020
#define PTP_OC_CANON_GetObjectInfoEx		0x9021
#define PTP_OC_CANON_InitiateDirectTransfer	0x9022
#define PTP_OC_CANON_TerminateDirectTransfer 	0x9023
#define PTP_OC_CANON_SendObjectInfoByPath 	0x9024
#define PTP_OC_CANON_SendObjectByPath 		0x9025
#define PTP_OC_CANON_InitiateDirectTansferEx	0x9026
#define PTP_OC_CANON_GetAncillaryObjectHandles	0x9027
#define PTP_OC_CANON_GetTreeInfo 		0x9028
#define PTP_OC_CANON_GetTreeSize 		0x9029
#define PTP_OC_CANON_NotifyProgress 		0x902A
#define PTP_OC_CANON_NotifyCancelAccepted	0x902B
/* 902c: no parms, read 3 uint32 in data, no response parms */
#define PTP_OC_CANON_902C			0x902C
#define PTP_OC_CANON_GetDirectory		0x902D

#define PTP_OC_CANON_SetPairingInfo		0x9030
#define PTP_OC_CANON_GetPairingInfo		0x9031
#define PTP_OC_CANON_DeletePairingInfo		0x9032
#define PTP_OC_CANON_GetMACAddress		0x9033
/* 9034: 1 param, no parms returned */
#define PTP_OC_CANON_SetDisplayMonitor		0x9034
#define PTP_OC_CANON_PairingComplete		0x9035
#define PTP_OC_CANON_GetWirelessMAXChannel	0x9036

/* 9101: no args, 8 byte data (01 00 00 00 00 00 00 00), no resp data. */
#define PTP_OC_CANON_EOS_GetStorageIDs		0x9101
/* 9102: 1 arg (0)
 * 0x28 bytes of data:
    00000000: 34 00 00 00 02 00 02 91 0a 00 00 00 04 00 03 00
    00000010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00000020: 00 00 ff ff ff ff 03 43 00 46 00 00 00 03 41 00
    00000030: 3a 00 00 00
 * no resp args
 */
#define PTP_OC_CANON_EOS_GetStorageInfo		0x9102
#define PTP_OC_CANON_EOS_GetObjectInfo		0x9103
#define PTP_OC_CANON_EOS_GetObject		0x9104
#define PTP_OC_CANON_EOS_DeleteObject		0x9105
#define PTP_OC_CANON_EOS_FormatStore		0x9106
#define PTP_OC_CANON_EOS_GetPartialObject	0x9107
#define PTP_OC_CANON_EOS_GetDeviceInfoEx	0x9108

/* sample1:
 * 3 cmdargs: 1,0xffffffff,00 00 10 00;
 * data:
    00000000: 48 00 00 00 02 00 09 91 12 00 00 00 01 00 00 00
    00000010: 38 00 00 00 00 00 00 30 01 00 00 00 01 30 00 00
    00000020: 01 00 00 00 10 00 00 00 00 00 00 00 00 00 00 20
    00000030: 00 00 00 30 44 43 49 4d 00 00 00 00 00 00 00 00	DCIM
    00000040: 00 00 00 00 cc c3 01 46
 * 2 respargs: 0x0, 0x3c
 * 
 * sample2:
 *
    00000000: 18 00 00 00 01 00 09 91 15 00 00 00 01 00 00 00
    00000010: 00 00 00 30 00 00 10 00

    00000000: 48 00 00 00 02 00 09 91 15 00 00 00 01 00 00 00
    00000010: 38 00 00 00 00 00 9c 33 01 00 00 00 01 30 00 00
    00000020: 01 00 00 00 10 00 00 00 00 00 00 00 00 00 00 30
    00000030: 00 00 9c 33 32 33 31 43 41 4e 4f 4e 00 00 00 00	 231CANON
    00000040: 00 00 00 00 cc c3 01 46

 */
#define PTP_OC_CANON_EOS_GetObjectInfoEx	0x9109
#define PTP_OC_CANON_EOS_GetThumbEx		0x910A
#define PTP_OC_CANON_EOS_SendPartialObject	0x910B
#define PTP_OC_CANON_EOS_SetObjectAttributes	0x910C
#define PTP_OC_CANON_EOS_GetObjectTime		0x910D
#define PTP_OC_CANON_EOS_SetObjectTime		0x910E

/* 910f: no args, no data, 1 response arg (0). */
#define PTP_OC_CANON_EOS_RemoteRelease		0x910F
/* Marcus: looks more like "Set DeviceProperty" in the trace. 
 *
 * no cmd args
 * data phase (0xc, 0xd11c, 0x1)
 * no resp args 
 */
#define PTP_OC_CANON_EOS_SetDevicePropValueEx	0x9110
#define PTP_OC_CANON_EOS_GetRemoteMode		0x9113
/* 9114: 1 arg (0x1), no data, no resp data. */
#define PTP_OC_CANON_EOS_SetRemoteMode		0x9114
/* 9115: 1 arg (0x1), no data, no resp data. */
#define PTP_OC_CANON_EOS_SetEventMode		0x9115
/* 9116: no args, data phase, no resp data. */
#define PTP_OC_CANON_EOS_GetEvent		0x9116
#define PTP_OC_CANON_EOS_TransferComplete	0x9117
#define PTP_OC_CANON_EOS_CancelTransfer		0x9118
#define PTP_OC_CANON_EOS_ResetTransfer		0x9119

/* 911a: 3 args (0xfffffff7, 0x00001000, 0x00000001), no data, no resp data. */
/* 911a: 3 args (0x001dfc60, 0x00001000, 0x00000001), no data, no resp data. */
#define PTP_OC_CANON_EOS_PCHDDCapacity		0x911A

/* 911b: no cmd args, no data, no resp args */
#define PTP_OC_CANON_EOS_SetUILock		0x911B
/* 911c: no cmd args, no data, no resp args */
#define PTP_OC_CANON_EOS_ResetUILock		0x911C
#define PTP_OC_CANON_EOS_KeepDeviceOn		0x911D
#define PTP_OC_CANON_EOS_SetNullPacketMode	0x911E
#define PTP_OC_CANON_EOS_UpdateFirmware		0x911F
#define PTP_OC_CANON_EOS_TransferCompleteDT	0x9120
#define PTP_OC_CANON_EOS_CancelTransferDT	0x9121
#define PTP_OC_CANON_EOS_SetWftProfile		0x9122
#define PTP_OC_CANON_EOS_GetWftProfile		0x9122
#define PTP_OC_CANON_EOS_SetProfileToWft	0x9124
#define PTP_OC_CANON_EOS_BulbStart		0x9125
#define PTP_OC_CANON_EOS_BulbEnd		0x9126
#define PTP_OC_CANON_EOS_RequestDevicePropValue	0x9127

/* 0x9128 args (0x1/0x2, 0x0), no data, no resp args */
#define PTP_OC_CANON_EOS_RemoteReleaseOn	0x9128
/* 0x9129 args (0x1/0x2), no data, no resp args */
#define PTP_OC_CANON_EOS_RemoteReleaseOff	0x9129

#define PTP_OC_CANON_EOS_InitiateViewfinder	0x9151
#define PTP_OC_CANON_EOS_TerminateViewfinder	0x9152
#define PTP_OC_CANON_EOS_GetViewFinderData	0x9153
#define PTP_OC_CANON_EOS_DoAf			0x9154
#define PTP_OC_CANON_EOS_DriveLens		0x9155
#define PTP_OC_CANON_EOS_DepthOfFieldPreview	0x9156
#define PTP_OC_CANON_EOS_ClickWB		0x9157
#define PTP_OC_CANON_EOS_Zoom			0x9158
#define PTP_OC_CANON_EOS_ZoomPosition		0x9159
#define PTP_OC_CANON_EOS_SetLiveAfFrame		0x915a
#define PTP_OC_CANON_EOS_AfCancel		0x9160
#define PTP_OC_CANON_EOS_FAPIMessageTX		0x91FE
#define PTP_OC_CANON_EOS_FAPIMessageRX		0x91FF

/* Nikon extension Operation Codes */
#define PTP_OC_NIKON_GetProfileAllData	0x9006
#define PTP_OC_NIKON_SendProfileData	0x9007
#define PTP_OC_NIKON_DeleteProfile	0x9008
#define PTP_OC_NIKON_SetProfileData	0x9009
#define PTP_OC_NIKON_AdvancedTransfer	0x9010
#define PTP_OC_NIKON_GetFileInfoInBlock	0x9011
#define PTP_OC_NIKON_Capture		0x90C0	/* 1 param,   no data */
#define PTP_OC_NIKON_AfDrive		0x90C1	/* no params, no data */
#define PTP_OC_NIKON_SetControlMode	0x90C2	/* 1 param,   no data */
#define PTP_OC_NIKON_DelImageSDRAM	0x90C3	/* no params, no data */
#define PTP_OC_NIKON_GetLargeThumb	0x90C4
#define PTP_OC_NIKON_CurveDownload	0x90C5	/* 1 param,   data in */
#define PTP_OC_NIKON_CurveUpload	0x90C6	/* 1 param,   data out */
#define PTP_OC_NIKON_CheckEvent		0x90C7	/* no params, data in */
#define PTP_OC_NIKON_DeviceReady	0x90C8	/* no params, no data */
#define PTP_OC_NIKON_SetPreWBData	0x90C9	/* 3 params,  data out */
#define PTP_OC_NIKON_GetVendorPropCodes	0x90CA	/* 0 params, data in */
#define PTP_OC_NIKON_AfCaptureSDRAM	0x90CB	/* no params, no data */
#define PTP_OC_NIKON_GetPictCtrlData	0x90CC
#define PTP_OC_NIKON_SetPictCtrlData	0x90CD
#define PTP_OC_NIKON_DelCstPicCtrl	0x90CE
#define PTP_OC_NIKON_GetPicCtrlCapability	0x90CF

/* Nikon Liveview stuff */
#define PTP_OC_NIKON_GetPreviewImg	0x9200
#define PTP_OC_NIKON_StartLiveView	0x9201
#define PTP_OC_NIKON_EndLiveView	0x9202
#define PTP_OC_NIKON_GetLiveViewImg	0x9203
#define PTP_OC_NIKON_MfDrive		0x9204
#define PTP_OC_NIKON_ChangeAfArea	0x9205
#define PTP_OC_NIKON_AfDriveCancel	0x9206

#define PTP_OC_NIKON_GetDevicePTPIPInfo	0x90E0

/* Microsoft / MTP extension codes */

#define PTP_OC_MTP_GetObjectPropsSupported	0x9801
#define PTP_OC_MTP_GetObjectPropDesc		0x9802
#define PTP_OC_MTP_GetObjectPropValue		0x9803
#define PTP_OC_MTP_SetObjectPropValue		0x9804
#define PTP_OC_MTP_GetObjPropList		0x9805
#define PTP_OC_MTP_SetObjPropList		0x9806
#define PTP_OC_MTP_GetInterdependendPropdesc	0x9807
#define PTP_OC_MTP_SendObjectPropList		0x9808
#define PTP_OC_MTP_GetObjectReferences		0x9810
#define PTP_OC_MTP_SetObjectReferences		0x9811
#define PTP_OC_MTP_UpdateDeviceFirmware		0x9812
#define PTP_OC_MTP_Skip				0x9820

/*
 * Windows Media Digital Rights Management for Portable Devices 
 * Extension Codes (microsoft.com/WMDRMPD: 10.1)
 */
#define PTP_OC_MTP_WMDRMPD_GetSecureTimeChallenge	0x9101
#define PTP_OC_MTP_WMDRMPD_GetSecureTimeResponse	0x9102
#define PTP_OC_MTP_WMDRMPD_SetLicenseResponse	0x9103
#define PTP_OC_MTP_WMDRMPD_GetSyncList		0x9104
#define PTP_OC_MTP_WMDRMPD_SendMeterChallengeQuery	0x9105
#define PTP_OC_MTP_WMDRMPD_GetMeterChallenge	0x9106
#define PTP_OC_MTP_WMDRMPD_SetMeterResponse		0x9107
#define PTP_OC_MTP_WMDRMPD_CleanDataStore		0x9108
#define PTP_OC_MTP_WMDRMPD_GetLicenseState		0x9109
#define PTP_OC_MTP_WMDRMPD_SendWMDRMPDCommand	0x910A
#define PTP_OC_MTP_WMDRMPD_SendWMDRMPDRequest	0x910B

/* 
 * Windows Media Digital Rights Management for Portable Devices 
 * Extension Codes (microsoft.com/WMDRMPD: 10.1)
 * Below are operations that have no public documented identifier 
 * associated with them "Vendor-defined Command Code"
 */
#define PTP_OC_MTP_WMDRMPD_SendWMDRMPDAppRequest	0x9212
#define PTP_OC_MTP_WMDRMPD_GetWMDRMPDAppResponse	0x9213
#define PTP_OC_MTP_WMDRMPD_EnableTrustedFilesOperations	0x9214
#define PTP_OC_MTP_WMDRMPD_DisableTrustedFilesOperations 0x9215
#define PTP_OC_MTP_WMDRMPD_EndTrustedAppSession		0x9216
/* ^^^ guess ^^^ */

/*
 * Microsoft Advanced Audio/Video Transfer 
 * Extensions (microsoft.com/AAVT: 1.0)
 */
#define PTP_OC_MTP_AAVT_OpenMediaSession		0x9170
#define PTP_OC_MTP_AAVT_CloseMediaSession		0x9171
#define PTP_OC_MTP_AAVT_GetNextDataBlock		0x9172
#define PTP_OC_MTP_AAVT_SetCurrentTimePosition		0x9173

/*
 * Windows Media Digital Rights Management for Network Devices 
 * Extensions (microsoft.com/WMDRMND: 1.0) MTP/IP?
 */
#define PTP_OC_MTP_WMDRMND_SendRegistrationRequest	0x9180
#define PTP_OC_MTP_WMDRMND_GetRegistrationResponse	0x9181
#define PTP_OC_MTP_WMDRMND_GetProximityChallenge	0x9182
#define PTP_OC_MTP_WMDRMND_SendProximityResponse	0x9183
#define PTP_OC_MTP_WMDRMND_SendWMDRMNDLicenseRequest	0x9184
#define PTP_OC_MTP_WMDRMND_GetWMDRMNDLicenseResponse	0x9185

/* 
 * Windows Media Player Portiable Devices 
 * Extension Codes (microsoft.com/WMPPD: 11.1)
 */
#define PTP_OC_MTP_WMPPD_ReportAddedDeletedItems	0x9201
#define PTP_OC_MTP_WMPPD_ReportAcquiredItems 	        0x9202
#define PTP_OC_MTP_WMPPD_PlaylistObjectPref		0x9203

/*
 * Undocumented Zune Operation Codes 
 * maybe related to WMPPD extension set?
 */
#define PTP_OC_MTP_ZUNE_GETUNDEFINED001		        0x9204

/* WiFi Provisioning MTP Extension Codes (microsoft.com/WPDWCN: 1.0) */
#define PTP_OC_MTP_WPDWCN_ProcessWFCObject		0x9122

/* Proprietary vendor extension operations mask */
#define PTP_OC_EXTENSION_MASK           0xF000
#define PTP_OC_EXTENSION                0x9000

/* Response Codes */

/* PTP v1.0 response codes */
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
#define PTP_RC_SpecificationByFormatUnsupported         0x2014
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
/* PTP v1.1 response codes */
#define PTP_RC_InvalidEnumHandle	0x2021
#define PTP_RC_NoStreamEnabled		0x2022
#define PTP_RC_InvalidDataSet		0x2023

/* Eastman Kodak extension Response Codes */
#define PTP_RC_EK_FilenameRequired	0xA001
#define PTP_RC_EK_FilenameConflicts	0xA002
#define PTP_RC_EK_FilenameInvalid	0xA003

/* Nikon specific response codes */
#define PTP_RC_NIKON_HardwareError		0xA001
#define PTP_RC_NIKON_OutOfFocus			0xA002
#define PTP_RC_NIKON_ChangeCameraModeFailed	0xA003
#define PTP_RC_NIKON_InvalidStatus		0xA004
#define PTP_RC_NIKON_SetPropertyNotSupported	0xA005
#define PTP_RC_NIKON_WbResetError		0xA006
#define PTP_RC_NIKON_DustReferenceError		0xA007
#define PTP_RC_NIKON_ShutterSpeedBulb		0xA008
#define PTP_RC_NIKON_MirrorUpSequence		0xA009
#define PTP_RC_NIKON_CameraModeNotAdjustFNumber	0xA00A
#define PTP_RC_NIKON_NotLiveView		0xA00B
#define PTP_RC_NIKON_MfDriveStepEnd		0xA00C
#define PTP_RC_NIKON_MfDriveStepInsufficiency	0xA00E
#define PTP_RC_NIKON_AdvancedTransferCancel	0xA022

/* Canon specific response codes */
#define PTP_RC_CANON_A009		0xA009

/* Microsoft/MTP specific codes */
#define PTP_RC_MTP_Undefined			0xA800
#define PTP_RC_MTP_Invalid_ObjectPropCode	0xA801
#define PTP_RC_MTP_Invalid_ObjectProp_Format	0xA802
#define PTP_RC_MTP_Invalid_ObjectProp_Value	0xA803
#define PTP_RC_MTP_Invalid_ObjectReference	0xA804
#define PTP_RC_MTP_Invalid_Dataset		0xA806
#define PTP_RC_MTP_Specification_By_Group_Unsupported		0xA807
#define PTP_RC_MTP_Specification_By_Depth_Unsupported		0xA808
#define PTP_RC_MTP_Object_Too_Large		0xA809
#define PTP_RC_MTP_ObjectProp_Not_Supported	0xA80A

/* Microsoft Advanced Audio/Video Transfer response codes 
(microsoft.com/AAVT 1.0) */
#define PTP_RC_MTP_Invalid_Media_Session_ID	0xA170	
#define PTP_RC_MTP_Media_Session_Limit_Reached	0xA171
#define PTP_RC_MTP_No_More_Data			0xA172

/* WiFi Provisioning MTP Extension Error Codes (microsoft.com/WPDWCN: 1.0) */
#define PTP_RC_MTP_Invalid_WFC_Syntax		0xA121
#define PTP_RC_MTP_WFC_Version_Not_Supported	0xA122

/* libptp2 extended ERROR codes */
#define PTP_ERROR_IO			0x02FF
#define PTP_ERROR_DATA_EXPECTED		0x02FE
#define PTP_ERROR_RESP_EXPECTED		0x02FD
#define PTP_ERROR_BADPARAM		0x02FC
#define PTP_ERROR_CANCEL		0x02FB
#define PTP_ERROR_TIMEOUT		0x02FA

/* PTP Event Codes */

#define PTP_EC_Undefined		0x4000
#define PTP_EC_CancelTransaction	0x4001
#define PTP_EC_ObjectAdded		0x4002
#define PTP_EC_ObjectRemoved		0x4003
#define PTP_EC_StoreAdded		0x4004
#define PTP_EC_StoreRemoved		0x4005
#define PTP_EC_DevicePropChanged	0x4006
#define PTP_EC_ObjectInfoChanged	0x4007
#define PTP_EC_DeviceInfoChanged	0x4008
#define PTP_EC_RequestObjectTransfer	0x4009
#define PTP_EC_StoreFull		0x400A
#define PTP_EC_DeviceReset		0x400B
#define PTP_EC_StorageInfoChanged	0x400C
#define PTP_EC_CaptureComplete		0x400D
#define PTP_EC_UnreportedStatus		0x400E

/* Canon extension Event Codes */
#define PTP_EC_CANON_ExtendedErrorcode		0xC005	/* ? */
#define PTP_EC_CANON_ObjectInfoChanged		0xC008
#define PTP_EC_CANON_RequestObjectTransfer	0xC009
#define PTP_EC_CANON_CameraModeChanged		0xC00C
#define PTP_EC_CANON_ShutterButtonPressed	0xC00E

#define PTP_EC_CANON_StartDirectTransfer	0xC011
#define PTP_EC_CANON_StopDirectTransfer		0xC013

/* Canon EOS events */
#define PTP_EC_CANON_EOS_RequestGetEvent	0xc101
#define PTP_EC_CANON_EOS_ObjectAddedEx		0xc181
#define PTP_EC_CANON_EOS_ObjectRemoved		0xc182
#define PTP_EC_CANON_EOS_RequestGetObjectInfoEx	0xc183
#define PTP_EC_CANON_EOS_StorageStatusChanged	0xc184
#define PTP_EC_CANON_EOS_StorageInfoChanged	0xc185
#define PTP_EC_CANON_EOS_RequestObjectTransfer	0xc186
#define PTP_EC_CANON_EOS_ObjectInfoChangedEx	0xc187
#define PTP_EC_CANON_EOS_ObjectContentChanged	0xc188
#define PTP_EC_CANON_EOS_PropValueChanged	0xc189
#define PTP_EC_CANON_EOS_AvailListChanged	0xc18a
#define PTP_EC_CANON_EOS_CameraStatusChanged	0xc18b
#define PTP_EC_CANON_EOS_WillSoonShutdown	0xc18d
#define PTP_EC_CANON_EOS_ShutdownTimerUpdated	0xc18e
#define PTP_EC_CANON_EOS_RequestCancelTransfer	0xc18f
#define PTP_EC_CANON_EOS_RequestObjectTransferDT	0xc190
#define PTP_EC_CANON_EOS_RequestCancelTransferDT	0xc191
#define PTP_EC_CANON_EOS_StoreAdded		0xc192
#define PTP_EC_CANON_EOS_StoreRemoved		0xc193
#define PTP_EC_CANON_EOS_BulbExposureTime	0xc194
#define PTP_EC_CANON_EOS_RecordingTime		0xc195
#define PTP_EC_CANON_EOS_RequestObjectTransferTS		0xC1a2
#define PTP_EC_CANON_EOS_AfResult		0xc1a3

/* Nikon extension Event Codes */

/* Nikon extension Event Codes */
#define PTP_EC_Nikon_ObjectAddedInSDRAM		0xC101
#define PTP_EC_Nikon_CaptureCompleteRecInSdram	0xC102
/* Gets 1 parameter, objectid pointing to DPOF object */
#define PTP_EC_Nikon_AdvancedTransfer		0xC103
#define PTP_EC_Nikon_PreviewImageAdded		0xC104

/* MTP Event codes */
#define PTP_EC_MTP_ObjectPropChanged		0xC801
#define PTP_EC_MTP_ObjectPropDescChanged	0xC802
#define PTP_EC_MTP_ObjectReferencesChanged	0xC803

/* constants for GetObjectHandles */
#define PTP_GOH_ALL_STORAGE 0xffffffff
#define PTP_GOH_ALL_FORMATS 0x00000000
#define PTP_GOH_ALL_ASSOCS  0x00000000
#define PTP_GOH_ROOT_PARENT 0xffffffff

/* PTP device info structure (returned by GetDevInfo) */

struct _PTPDeviceInfo {
	uint16_t StandardVersion;
	uint32_t VendorExtensionID;
	uint16_t VendorExtensionVersion;
	char	*VendorExtensionDesc;
	uint16_t FunctionalMode;
	uint32_t OperationsSupported_len;
	uint16_t *OperationsSupported;
	uint32_t EventsSupported_len;
	uint16_t *EventsSupported;
	uint32_t DevicePropertiesSupported_len;
	uint16_t *DevicePropertiesSupported;
	uint32_t CaptureFormats_len;
	uint16_t *CaptureFormats;
	uint32_t ImageFormats_len;
	uint16_t *ImageFormats;
	char	*Manufacturer;
	char	*Model;
	char	*DeviceVersion;
	char	*SerialNumber;
};
typedef struct _PTPDeviceInfo PTPDeviceInfo;

/* PTP storageIDs structute (returned by GetStorageIDs) */

struct _PTPStorageIDs {
	uint32_t n;
	uint32_t *Storage;
};
typedef struct _PTPStorageIDs PTPStorageIDs;

/* PTP StorageInfo structure (returned by GetStorageInfo) */
struct _PTPStorageInfo {
	uint16_t StorageType;
	uint16_t FilesystemType;
	uint16_t AccessCapability;
	uint64_t MaxCapability;
	uint64_t FreeSpaceInBytes;
	uint32_t FreeSpaceInImages;
	char 	*StorageDescription;
	char	*VolumeLabel;
};
typedef struct _PTPStorageInfo PTPStorageInfo;

/* PTP objecthandles structure (returned by GetObjectHandles) */

struct _PTPObjectHandles {
	uint32_t n;
	uint32_t *Handler;
};
typedef struct _PTPObjectHandles PTPObjectHandles;

#define PTP_HANDLER_SPECIAL	0xffffffff
#define PTP_HANDLER_ROOT	0x00000000


/* PTP objectinfo structure (returned by GetObjectInfo) */

struct _PTPObjectInfo {
	uint32_t StorageID;
	uint16_t ObjectFormat;
	uint16_t ProtectionStatus;
	uint32_t ObjectCompressedSize;
	uint16_t ThumbFormat;
	uint32_t ThumbCompressedSize;
	uint32_t ThumbPixWidth;
	uint32_t ThumbPixHeight;
	uint32_t ImagePixWidth;
	uint32_t ImagePixHeight;
	uint32_t ImageBitDepth;
	uint32_t ParentObject;
	uint16_t AssociationType;
	uint32_t AssociationDesc;
	uint32_t SequenceNumber;
	char 	*Filename;
	time_t	CaptureDate;
	time_t	ModificationDate;
	char	*Keywords;
};
typedef struct _PTPObjectInfo PTPObjectInfo;

/* max ptp string length INCLUDING terminating null character */

#define PTP_MAXSTRLEN				255

/* PTP Object Format Codes */

/* ancillary formats */
#define PTP_OFC_Undefined			0x3000
#define PTP_OFC_Defined				0x3800
#define PTP_OFC_Association			0x3001
#define PTP_OFC_Script				0x3002
#define PTP_OFC_Executable			0x3003
#define PTP_OFC_Text				0x3004
#define PTP_OFC_HTML				0x3005
#define PTP_OFC_DPOF				0x3006
#define PTP_OFC_AIFF	 			0x3007
#define PTP_OFC_WAV				0x3008
#define PTP_OFC_MP3				0x3009
#define PTP_OFC_AVI				0x300A
#define PTP_OFC_MPEG				0x300B
#define PTP_OFC_ASF				0x300C
#define PTP_OFC_QT				0x300D /* guessing */
/* image formats */
#define PTP_OFC_EXIF_JPEG			0x3801
#define PTP_OFC_TIFF_EP				0x3802
#define PTP_OFC_FlashPix			0x3803
#define PTP_OFC_BMP				0x3804
#define PTP_OFC_CIFF				0x3805
#define PTP_OFC_Undefined_0x3806		0x3806
#define PTP_OFC_GIF				0x3807
#define PTP_OFC_JFIF				0x3808
#define PTP_OFC_PCD				0x3809
#define PTP_OFC_PICT				0x380A
#define PTP_OFC_PNG				0x380B
#define PTP_OFC_Undefined_0x380C		0x380C
#define PTP_OFC_TIFF				0x380D
#define PTP_OFC_TIFF_IT				0x380E
#define PTP_OFC_JP2				0x380F
#define PTP_OFC_JPX				0x3810
/* ptp v1.1 has only DNG new */
#define PTP_OFC_DNG				0x3811
/* Eastman Kodak extension ancillary format */
#define PTP_OFC_EK_M3U				0xb002
/* Canon extension */
#define PTP_OFC_CANON_CRW			0xb101
#define PTP_OFC_CANON_CRW3			0xb103
#define PTP_OFC_CANON_MOV			0xb104
/* MTP extensions */
#define PTP_OFC_MTP_MediaCard			0xb211
#define PTP_OFC_MTP_MediaCardGroup		0xb212
#define PTP_OFC_MTP_Encounter			0xb213
#define PTP_OFC_MTP_EncounterBox		0xb214
#define PTP_OFC_MTP_M4A				0xb215
#define PTP_OFC_MTP_ZUNEUNDEFINED		0xb217 /* Unknown file type */
#define PTP_OFC_MTP_Firmware			0xb802
#define PTP_OFC_MTP_WindowsImageFormat		0xb881
#define PTP_OFC_MTP_UndefinedAudio		0xb900
#define PTP_OFC_MTP_WMA				0xb901
#define PTP_OFC_MTP_OGG				0xb902
#define PTP_OFC_MTP_AAC				0xb903
#define PTP_OFC_MTP_AudibleCodec		0xb904
#define PTP_OFC_MTP_FLAC			0xb906
#define PTP_OFC_MTP_UndefinedVideo		0xb980
#define PTP_OFC_MTP_WMV				0xb981
#define PTP_OFC_MTP_MP4				0xb982
#define PTP_OFC_MTP_MP2				0xb983
#define PTP_OFC_MTP_3GP				0xb984
#define PTP_OFC_MTP_UndefinedCollection		0xba00
#define PTP_OFC_MTP_AbstractMultimediaAlbum	0xba01
#define PTP_OFC_MTP_AbstractImageAlbum		0xba02
#define PTP_OFC_MTP_AbstractAudioAlbum		0xba03
#define PTP_OFC_MTP_AbstractVideoAlbum		0xba04
#define PTP_OFC_MTP_AbstractAudioVideoPlaylist	0xba05
#define PTP_OFC_MTP_AbstractContactGroup	0xba06
#define PTP_OFC_MTP_AbstractMessageFolder	0xba07
#define PTP_OFC_MTP_AbstractChapteredProduction	0xba08
#define PTP_OFC_MTP_AbstractAudioPlaylist	0xba09
#define PTP_OFC_MTP_AbstractVideoPlaylist	0xba0a
#define PTP_OFC_MTP_AbstractMediacast		0xba0b
#define PTP_OFC_MTP_WPLPlaylist			0xba10
#define PTP_OFC_MTP_M3UPlaylist			0xba11
#define PTP_OFC_MTP_MPLPlaylist			0xba12
#define PTP_OFC_MTP_ASXPlaylist			0xba13
#define PTP_OFC_MTP_PLSPlaylist			0xba14
#define PTP_OFC_MTP_UndefinedDocument		0xba80
#define PTP_OFC_MTP_AbstractDocument		0xba81
#define PTP_OFC_MTP_XMLDocument			0xba82
#define PTP_OFC_MTP_MSWordDocument		0xba83
#define PTP_OFC_MTP_MHTCompiledHTMLDocument	0xba84
#define PTP_OFC_MTP_MSExcelSpreadsheetXLS	0xba85
#define PTP_OFC_MTP_MSPowerpointPresentationPPT	0xba86
#define PTP_OFC_MTP_UndefinedMessage		0xbb00
#define PTP_OFC_MTP_AbstractMessage		0xbb01
#define PTP_OFC_MTP_UndefinedContact		0xbb80
#define PTP_OFC_MTP_AbstractContact		0xbb81
#define PTP_OFC_MTP_vCard2			0xbb82
#define PTP_OFC_MTP_vCard3			0xbb83
#define PTP_OFC_MTP_UndefinedCalendarItem	0xbe00
#define PTP_OFC_MTP_AbstractCalendarItem	0xbe01
#define PTP_OFC_MTP_vCalendar1			0xbe02
#define PTP_OFC_MTP_vCalendar2			0xbe03
#define PTP_OFC_MTP_UndefinedWindowsExecutable	0xbe80
#define PTP_OFC_MTP_MediaCast			0xbe81
#define PTP_OFC_MTP_Section			0xbe82

/* PTP Association Types */
#define PTP_AT_Undefined			0x0000
#define PTP_AT_GenericFolder			0x0001
#define PTP_AT_Album				0x0002
#define PTP_AT_TimeSequence			0x0003
#define PTP_AT_HorizontalPanoramic		0x0004
#define PTP_AT_VerticalPanoramic		0x0005
#define PTP_AT_2DPanoramic			0x0006
#define PTP_AT_AncillaryData			0x0007

/* PTP Protection Status */

#define PTP_PS_NoProtection			0x0000
#define PTP_PS_ReadOnly				0x0001
#define PTP_PS_MTP_ReadOnlyData			0x8002
#define PTP_PS_MTP_NonTransferableData		0x8003

/* PTP Storage Types */

#define PTP_ST_Undefined			0x0000
#define PTP_ST_FixedROM				0x0001
#define PTP_ST_RemovableROM			0x0002
#define PTP_ST_FixedRAM				0x0003
#define PTP_ST_RemovableRAM			0x0004

/* PTP FilesystemType Values */

#define PTP_FST_Undefined			0x0000
#define PTP_FST_GenericFlat			0x0001
#define PTP_FST_GenericHierarchical		0x0002
#define PTP_FST_DCF				0x0003

/* PTP StorageInfo AccessCapability Values */

#define PTP_AC_ReadWrite			0x0000
#define PTP_AC_ReadOnly				0x0001
#define PTP_AC_ReadOnly_with_Object_Deletion	0x0002

/* Property Describing Dataset, Range Form */

union _PTPPropertyValue {
	char		*str;	/* common string, malloced */
	uint8_t		u8;
	int8_t		i8;
	uint16_t	u16;
	int16_t		i16;
	uint32_t	u32;
	int32_t		i32;
	uint64_t	u64;
	int64_t		i64;
	/* XXXX: 128 bit signed and unsigned missing */
	struct array {
		uint32_t	count;
		union _PTPPropertyValue	*v;	/* malloced, count elements */
	} a;
};

typedef union _PTPPropertyValue PTPPropertyValue;

/* Metadata lists for MTP operations */
struct _MTPProperties {
	uint16_t 	 	property;
	uint16_t 	 	datatype;
	uint32_t 	 	ObjectHandle;
	PTPPropertyValue 	propval;
};
typedef struct _MTPProperties MTPProperties;

struct _PTPPropDescRangeForm {
	PTPPropertyValue 	MinimumValue;
	PTPPropertyValue 	MaximumValue;
	PTPPropertyValue 	StepSize;
};
typedef struct _PTPPropDescRangeForm PTPPropDescRangeForm;

/* Property Describing Dataset, Enum Form */

struct _PTPPropDescEnumForm {
	uint16_t		NumberOfValues;
	PTPPropertyValue	*SupportedValue;	/* malloced */
};
typedef struct _PTPPropDescEnumForm PTPPropDescEnumForm;

/* Device Property Describing Dataset (DevicePropDesc) */

struct _PTPDevicePropDesc {
	uint16_t		DevicePropertyCode;
	uint16_t		DataType;
	uint8_t			GetSet;
	PTPPropertyValue	FactoryDefaultValue;
	PTPPropertyValue	CurrentValue;
	uint8_t			FormFlag;
	union	{
		PTPPropDescEnumForm	Enum;
		PTPPropDescRangeForm	Range;
	} FORM;
};
typedef struct _PTPDevicePropDesc PTPDevicePropDesc;

/* Object Property Describing Dataset (DevicePropDesc) */

struct _PTPObjectPropDesc {
	uint16_t		ObjectPropertyCode;
	uint16_t		DataType;
	uint8_t			GetSet;
	PTPPropertyValue	FactoryDefaultValue;
	uint32_t		GroupCode;
	uint8_t			FormFlag;
	union	{
		PTPPropDescEnumForm	Enum;
		PTPPropDescRangeForm	Range;
	} FORM;
};
typedef struct _PTPObjectPropDesc PTPObjectPropDesc;

/* Canon filesystem's folder entry Dataset */

#define PTP_CANON_FilenameBufferLen	13
#define PTP_CANON_FolderEntryLen	28

struct _PTPCANONFolderEntry {
	uint32_t	ObjectHandle;
	uint16_t	ObjectFormatCode;
	uint8_t		Flags;
	uint32_t	ObjectSize;
	time_t		Time;
	char		Filename[PTP_CANON_FilenameBufferLen];
};
typedef struct _PTPCANONFolderEntry PTPCANONFolderEntry;

/* Nikon Tone Curve Data */

#define PTP_NIKON_MaxCurvePoints 19

struct _PTPNIKONCoordinatePair {
	uint8_t		X;
	uint8_t		Y;
};

typedef struct _PTPNIKONCoordinatePair PTPNIKONCoordinatePair;

struct _PTPNTCCoordinatePair {
	uint8_t		X;
	uint8_t		Y;
};

typedef struct _PTPNTCCoordinatePair PTPNTCCoordinatePair;

struct _PTPNIKONCurveData {
	char 			static_preamble[6];
	uint8_t			XAxisStartPoint;
	uint8_t			XAxisEndPoint;
	uint8_t			YAxisStartPoint;
	uint8_t			YAxisEndPoint;
	uint8_t			MidPointIntegerPart;
	uint8_t			MidPointDecimalPart;
	uint8_t			NCoordinates;
	PTPNIKONCoordinatePair	CurveCoordinates[PTP_NIKON_MaxCurvePoints];
};

typedef struct _PTPNIKONCurveData PTPNIKONCurveData;

struct _PTPEKTextParams {
	char	*title;
	char	*line[5];
};
typedef struct _PTPEKTextParams PTPEKTextParams;

/* Nikon Wifi profiles */

struct _PTPNIKONWifiProfile {
	/* Values valid both when reading and writing profiles */
	char      profile_name[17];
	uint8_t   device_type;
	uint8_t   icon_type;
	char      essid[33];

	/* Values only valid when reading. Some of these are in the write packet,
	 * but are set automatically, like id, display_order and creation_date. */
	uint8_t   id;
	uint8_t   valid;
	uint8_t   display_order;
	char      creation_date[16];
	char      lastusage_date[16];
	
	/* Values only valid when writing */
	uint32_t  ip_address;
	uint8_t   subnet_mask; /* first zero bit position, e.g. 24 for 255.255.255.0 */
	uint32_t  gateway_address;
	uint8_t   address_mode; /* 0 - Manual, 2-3 -  DHCP ad-hoc/managed*/
	uint8_t   access_mode; /* 0 - Managed, 1 - Adhoc */
	uint8_t   wifi_channel; /* 1-11 */
	uint8_t   authentification; /* 0 - Open, 1 - Shared, 2 - WPA-PSK */
	uint8_t   encryption; /* 0 - None, 1 - WEP 64bit, 2 - WEP 128bit (not supported: 3 - TKIP) */
	uint8_t   key[64];
	uint8_t   key_nr;
//	char      guid[16];
};

typedef struct _PTPNIKONWifiProfile PTPNIKONWifiProfile;

#define PTP_CANON_EOS_CHANGES_TYPE_UNKNOWN		0
#define PTP_CANON_EOS_CHANGES_TYPE_OBJECTINFO		1
#define PTP_CANON_EOS_CHANGES_TYPE_OBJECTTRANSFER	2

struct _PTPCanon_New_Object {
	uint32_t		oid;
	PTPObjectInfo	oi;
};

struct _PTPCanon_changes_entry {
	int	type;
	union {
		struct _PTPCanon_New_Object	object;	/* TYPE_OBJECTINFO */
	} u;
};
typedef struct _PTPCanon_changes_entry PTPCanon_changes_entry;

typedef struct _PTPCanon_Property {
	uint32_t		size;
	uint32_t		type;
	uint32_t		proptype;
	unsigned char		*data;

	/* fill out for queries */
	PTPDevicePropDesc	dpd;
} PTPCanon_Property;

typedef struct _PTPCanonEOSDeviceInfo {
	/* length */
	uint32_t EventsSupported_len;
	uint32_t *EventsSupported;

	uint32_t DevicePropertiesSupported_len;
	uint32_t *DevicePropertiesSupported;

	uint32_t unk_len;
	uint32_t *unk;
} PTPCanonEOSDeviceInfo;

/* DataType Codes */

#define PTP_DTC_UNDEF		0x0000
#define PTP_DTC_INT8		0x0001
#define PTP_DTC_UINT8		0x0002
#define PTP_DTC_INT16		0x0003
#define PTP_DTC_UINT16		0x0004
#define PTP_DTC_INT32		0x0005
#define PTP_DTC_UINT32		0x0006
#define PTP_DTC_INT64		0x0007
#define PTP_DTC_UINT64		0x0008
#define PTP_DTC_INT128		0x0009
#define PTP_DTC_UINT128		0x000A

#define PTP_DTC_ARRAY_MASK	0x4000

#define PTP_DTC_AINT8		(PTP_DTC_ARRAY_MASK | PTP_DTC_INT8)
#define PTP_DTC_AUINT8		(PTP_DTC_ARRAY_MASK | PTP_DTC_UINT8)
#define PTP_DTC_AINT16		(PTP_DTC_ARRAY_MASK | PTP_DTC_INT16)
#define PTP_DTC_AUINT16		(PTP_DTC_ARRAY_MASK | PTP_DTC_UINT16)
#define PTP_DTC_AINT32		(PTP_DTC_ARRAY_MASK | PTP_DTC_INT32)
#define PTP_DTC_AUINT32		(PTP_DTC_ARRAY_MASK | PTP_DTC_UINT32)
#define PTP_DTC_AINT64		(PTP_DTC_ARRAY_MASK | PTP_DTC_INT64)
#define PTP_DTC_AUINT64		(PTP_DTC_ARRAY_MASK | PTP_DTC_UINT64)
#define PTP_DTC_AINT128		(PTP_DTC_ARRAY_MASK | PTP_DTC_INT128)
#define PTP_DTC_AUINT128	(PTP_DTC_ARRAY_MASK | PTP_DTC_UINT128)

#define PTP_DTC_STR		0xFFFF

/* Device Properties Codes */

/* PTP v1.0 property codes */
#define PTP_DPC_Undefined		0x5000
#define PTP_DPC_BatteryLevel		0x5001
#define PTP_DPC_FunctionalMode		0x5002
#define PTP_DPC_ImageSize		0x5003
#define PTP_DPC_CompressionSetting	0x5004
#define PTP_DPC_WhiteBalance		0x5005
#define PTP_DPC_RGBGain			0x5006
#define PTP_DPC_FNumber			0x5007
#define PTP_DPC_FocalLength		0x5008
#define PTP_DPC_FocusDistance		0x5009
#define PTP_DPC_FocusMode		0x500A
#define PTP_DPC_ExposureMeteringMode	0x500B
#define PTP_DPC_FlashMode		0x500C
#define PTP_DPC_ExposureTime		0x500D
#define PTP_DPC_ExposureProgramMode	0x500E
#define PTP_DPC_ExposureIndex		0x500F
#define PTP_DPC_ExposureBiasCompensation	0x5010
#define PTP_DPC_DateTime		0x5011
#define PTP_DPC_CaptureDelay		0x5012
#define PTP_DPC_StillCaptureMode	0x5013
#define PTP_DPC_Contrast		0x5014
#define PTP_DPC_Sharpness		0x5015
#define PTP_DPC_DigitalZoom		0x5016
#define PTP_DPC_EffectMode		0x5017
#define PTP_DPC_BurstNumber		0x5018
#define PTP_DPC_BurstInterval		0x5019
#define PTP_DPC_TimelapseNumber		0x501A
#define PTP_DPC_TimelapseInterval	0x501B
#define PTP_DPC_FocusMeteringMode	0x501C
#define PTP_DPC_UploadURL		0x501D
#define PTP_DPC_Artist			0x501E
#define PTP_DPC_CopyrightInfo		0x501F
/* PTP v1.1 property codes */
#define PTP_DPC_SupportedStreams	0x5020
#define PTP_DPC_EnabledStreams		0x5021
#define PTP_DPC_VideoFormat		0x5022
#define PTP_DPC_VideoResolution		0x5023
#define PTP_DPC_VideoQuality		0x5024
#define PTP_DPC_VideoFrameRate		0x5025
#define PTP_DPC_VideoContrast		0x5026
#define PTP_DPC_VideoBrightness		0x5027
#define PTP_DPC_AudioFormat		0x5028
#define PTP_DPC_AudioBitrate		0x5029
#define PTP_DPC_AudioSamplingRate	0x502A
#define PTP_DPC_AudioBitPerSample	0x502B
#define PTP_DPC_AudioVolume		0x502C

/* Proprietary vendor extension device property mask */
#define PTP_DPC_EXTENSION_MASK		0xF000
#define PTP_DPC_EXTENSION		0xD000

/* Zune extension device property codes */
#define PTP_DPC_MTP_ZUNE_UNKNOWN1	0xD181
#define PTP_DPC_MTP_ZUNE_UNKNOWN2	0xD132
#define PTP_DPC_MTP_ZUNE_UNKNOWN3	0xD215
#define PTP_DPC_MTP_ZUNE_UNKNOWN4	0xD216

/* Eastman Kodak extension device property codes */
#define PTP_DPC_EK_ColorTemperature	0xD001
#define PTP_DPC_EK_DateTimeStampFormat	0xD002
#define PTP_DPC_EK_BeepMode		0xD003
#define PTP_DPC_EK_VideoOut		0xD004
#define PTP_DPC_EK_PowerSaving		0xD005
#define PTP_DPC_EK_UI_Language		0xD006

/* Canon extension device property codes */
#define PTP_DPC_CANON_BeepMode		0xD001
#define PTP_DPC_CANON_BatteryKind	0xD002
#define PTP_DPC_CANON_BatteryStatus	0xD003
#define PTP_DPC_CANON_UILockType	0xD004
#define PTP_DPC_CANON_CameraMode	0xD005
#define PTP_DPC_CANON_ImageQuality	0xD006
#define PTP_DPC_CANON_FullViewFileFormat 0xD007
#define PTP_DPC_CANON_ImageSize		0xD008
#define PTP_DPC_CANON_SelfTime		0xD009
#define PTP_DPC_CANON_FlashMode		0xD00A
#define PTP_DPC_CANON_Beep		0xD00B
#define PTP_DPC_CANON_ShootingMode	0xD00C
#define PTP_DPC_CANON_ImageMode		0xD00D
#define PTP_DPC_CANON_DriveMode		0xD00E
#define PTP_DPC_CANON_EZoom		0xD00F
#define PTP_DPC_CANON_MeteringMode	0xD010
#define PTP_DPC_CANON_AFDistance	0xD011
#define PTP_DPC_CANON_FocusingPoint	0xD012
#define PTP_DPC_CANON_WhiteBalance	0xD013
#define PTP_DPC_CANON_SlowShutterSetting	0xD014
#define PTP_DPC_CANON_AFMode		0xD015
#define PTP_DPC_CANON_ImageStabilization	0xD016
#define PTP_DPC_CANON_Contrast		0xD017
#define PTP_DPC_CANON_ColorGain		0xD018
#define PTP_DPC_CANON_Sharpness		0xD019
#define PTP_DPC_CANON_Sensitivity	0xD01A
#define PTP_DPC_CANON_ParameterSet	0xD01B
#define PTP_DPC_CANON_ISOSpeed		0xD01C
#define PTP_DPC_CANON_Aperture		0xD01D
#define PTP_DPC_CANON_ShutterSpeed	0xD01E
#define PTP_DPC_CANON_ExpCompensation	0xD01F
#define PTP_DPC_CANON_FlashCompensation	0xD020
#define PTP_DPC_CANON_AEBExposureCompensation	0xD021
#define PTP_DPC_CANON_AvOpen		0xD023
#define PTP_DPC_CANON_AvMax		0xD024
#define PTP_DPC_CANON_FocalLength	0xD025
#define PTP_DPC_CANON_FocalLengthTele	0xD026
#define PTP_DPC_CANON_FocalLengthWide	0xD027
#define PTP_DPC_CANON_FocalLengthDenominator	0xD028
#define PTP_DPC_CANON_CaptureTransferMode	0xD029
#define CANON_TRANSFER_ENTIRE_IMAGE_TO_PC	0x0002
#define CANON_TRANSFER_SAVE_THUMBNAIL_TO_DEVICE	0x0004
#define CANON_TRANSFER_SAVE_IMAGE_TO_DEVICE	0x0008
/* we use those values: */
#define CANON_TRANSFER_MEMORY		(2|1)
#define CANON_TRANSFER_CARD		(8|4|1)

#define PTP_DPC_CANON_Zoom		0xD02A
#define PTP_DPC_CANON_NamePrefix	0xD02B
#define PTP_DPC_CANON_SizeQualityMode	0xD02C
#define PTP_DPC_CANON_SupportedThumbSize	0xD02D
#define PTP_DPC_CANON_SizeOfOutputDataFromCamera	0xD02E
#define PTP_DPC_CANON_SizeOfInputDataToCamera		0xD02F
#define PTP_DPC_CANON_RemoteAPIVersion	0xD030
#define PTP_DPC_CANON_FirmwareVersion	0xD031
#define PTP_DPC_CANON_CameraModel	0xD032
#define PTP_DPC_CANON_CameraOwner	0xD033
#define PTP_DPC_CANON_UnixTime		0xD034
#define PTP_DPC_CANON_CameraBodyID	0xD035
#define PTP_DPC_CANON_CameraOutput	0xD036
#define PTP_DPC_CANON_DispAv		0xD037
#define PTP_DPC_CANON_AvOpenApex	0xD038
#define PTP_DPC_CANON_DZoomMagnification	0xD039
#define PTP_DPC_CANON_MlSpotPos		0xD03A
#define PTP_DPC_CANON_DispAvMax		0xD03B
#define PTP_DPC_CANON_AvMaxApex		0xD03C
#define PTP_DPC_CANON_EZoomStartPosition		0xD03D
#define PTP_DPC_CANON_FocalLengthOfTele	0xD03E
#define PTP_DPC_CANON_EZoomSizeOfTele	0xD03F
#define PTP_DPC_CANON_PhotoEffect	0xD040
#define PTP_DPC_CANON_AssistLight	0xD041
#define PTP_DPC_CANON_FlashQuantityCount	0xD042
#define PTP_DPC_CANON_RotationAngle	0xD043
#define PTP_DPC_CANON_RotationScene	0xD044
#define PTP_DPC_CANON_EventEmulateMode	0xD045
#define PTP_DPC_CANON_DPOFVersion	0xD046
#define PTP_DPC_CANON_TypeOfSupportedSlideShow	0xD047
#define PTP_DPC_CANON_AverageFilesizes	0xD048
#define PTP_DPC_CANON_ModelID		0xD049

/* From EOS 400D trace. */
#define PTP_DPC_CANON_EOS_Aperture		0xD101
#define PTP_DPC_CANON_EOS_ShutterSpeed		0xD102
#define PTP_DPC_CANON_EOS_ISOSpeed		0xD103
#define PTP_DPC_CANON_EOS_ExpCompensation	0xD104
#define PTP_DPC_CANON_EOS_AutoExposureMode	0xD105
#define PTP_DPC_CANON_EOS_DriveMode		0xD106
#define PTP_DPC_CANON_EOS_MeteringMode		0xD107 
#define PTP_DPC_CANON_EOS_FocusMode		0xD108
#define PTP_DPC_CANON_EOS_WhiteBalance		0xD109
#define PTP_DPC_CANON_EOS_ColorTemperature	0xD10A
#define PTP_DPC_CANON_EOS_WhiteBalanceAdjustA	0xD10B
#define PTP_DPC_CANON_EOS_WhiteBalanceAdjustB	0xD10C
#define PTP_DPC_CANON_EOS_WhiteBalanceXA	0xD10D
#define PTP_DPC_CANON_EOS_WhiteBalanceXB	0xD10E
#define PTP_DPC_CANON_EOS_ColorSpace		0xD10F
#define PTP_DPC_CANON_EOS_PictureStyle		0xD110
#define PTP_DPC_CANON_EOS_BatteryPower		0xD111
#define PTP_DPC_CANON_EOS_BatterySelect		0xD112
#define PTP_DPC_CANON_EOS_CameraTime		0xD113
#define PTP_DPC_CANON_EOS_Owner			0xD115
#define PTP_DPC_CANON_EOS_ModelID		0xD116
#define PTP_DPC_CANON_EOS_PTPExtensionVersion	0xD119
#define PTP_DPC_CANON_EOS_DPOFVersion		0xD11A
#define PTP_DPC_CANON_EOS_AvailableShots	0xD11B
#define PTP_DPC_CANON_EOS_CaptureDestination	0xD11C
#define PTP_DPC_CANON_EOS_BracketMode		0xD11D
#define PTP_DPC_CANON_EOS_CurrentStorage	0xD11E
#define PTP_DPC_CANON_EOS_CurrentFolder		0xD11F
#define PTP_DPC_CANON_EOS_ImageFormat		0xD120	/* file setting */
#define PTP_DPC_CANON_EOS_ImageFormatCF		0xD121	/* file setting CF */
#define PTP_DPC_CANON_EOS_ImageFormatSD		0xD122	/* file setting SD */
#define PTP_DPC_CANON_EOS_ImageFormatExtHD	0xD123	/* file setting exthd */
#define PTP_DPC_CANON_EOS_CompressionS		0xD130
#define PTP_DPC_CANON_EOS_CompressionM1		0xD131
#define PTP_DPC_CANON_EOS_CompressionM2		0xD132
#define PTP_DPC_CANON_EOS_CompressionL		0xD133
#define PTP_DPC_CANON_EOS_PCWhiteBalance1	0xD140
#define PTP_DPC_CANON_EOS_PCWhiteBalance2	0xD141
#define PTP_DPC_CANON_EOS_PCWhiteBalance3	0xD142
#define PTP_DPC_CANON_EOS_PCWhiteBalance4	0xD143
#define PTP_DPC_CANON_EOS_PCWhiteBalance5	0xD144
#define PTP_DPC_CANON_EOS_MWhiteBalance		0xD145
#define PTP_DPC_CANON_EOS_PictureStyleStandard	0xD150
#define PTP_DPC_CANON_EOS_PictureStylePortrait	0xD151
#define PTP_DPC_CANON_EOS_PictureStyleLandscape	0xD152
#define PTP_DPC_CANON_EOS_PictureStyleNeutral	0xD153
#define PTP_DPC_CANON_EOS_PictureStyleFaithful	0xD154
#define PTP_DPC_CANON_EOS_PictureStyleBlackWhite	0xD155
#define PTP_DPC_CANON_EOS_PictureStyleUserSet1	0xD160
#define PTP_DPC_CANON_EOS_PictureStyleUserSet2	0xD161
#define PTP_DPC_CANON_EOS_PictureStyleUserSet3	0xD162
#define PTP_DPC_CANON_EOS_PictureStyleParam1	0xD170
#define PTP_DPC_CANON_EOS_PictureStyleParam2	0xD171
#define PTP_DPC_CANON_EOS_PictureStyleParam3	0xD172
#define PTP_DPC_CANON_EOS_FlavorLUTParams	0xD17f
#define PTP_DPC_CANON_EOS_CustomFunc1		0xD180
#define PTP_DPC_CANON_EOS_CustomFunc2		0xD181
#define PTP_DPC_CANON_EOS_CustomFunc3		0xD182
#define PTP_DPC_CANON_EOS_CustomFunc4		0xD183
#define PTP_DPC_CANON_EOS_CustomFunc5		0xD184
#define PTP_DPC_CANON_EOS_CustomFunc6		0xD185
#define PTP_DPC_CANON_EOS_CustomFunc7		0xD186
#define PTP_DPC_CANON_EOS_CustomFunc8		0xD187
#define PTP_DPC_CANON_EOS_CustomFunc9		0xD188
#define PTP_DPC_CANON_EOS_CustomFunc10		0xD189
#define PTP_DPC_CANON_EOS_CustomFunc11		0xD18a
#define PTP_DPC_CANON_EOS_CustomFunc12		0xD18b
#define PTP_DPC_CANON_EOS_CustomFunc13		0xD18c
#define PTP_DPC_CANON_EOS_CustomFunc14		0xD18d
#define PTP_DPC_CANON_EOS_CustomFunc15		0xD18e
#define PTP_DPC_CANON_EOS_CustomFunc16		0xD18f
#define PTP_DPC_CANON_EOS_CustomFunc17		0xD190
#define PTP_DPC_CANON_EOS_CustomFunc18		0xD191
#define PTP_DPC_CANON_EOS_CustomFunc19		0xD192
#define PTP_DPC_CANON_EOS_CustomFunc19		0xD192
#define PTP_DPC_CANON_EOS_CustomFuncEx		0xD1a0
#define PTP_DPC_CANON_EOS_MyMenu		0xD1a1
#define PTP_DPC_CANON_EOS_MyMenuList		0xD1a2
#define PTP_DPC_CANON_EOS_WftStatus		0xD1a3
#define PTP_DPC_CANON_EOS_WftInputTransmission	0xD1a4
#define PTP_DPC_CANON_EOS_HDDirectoryStructure	0xD1a5
#define PTP_DPC_CANON_EOS_BatteryInfo		0xD1a6
#define PTP_DPC_CANON_EOS_AdapterInfo		0xD1a7
#define PTP_DPC_CANON_EOS_LensStatus		0xD1a8
#define PTP_DPC_CANON_EOS_QuickReviewTime	0xD1a9
#define PTP_DPC_CANON_EOS_CardExtension		0xD1aa
#define PTP_DPC_CANON_EOS_TempStatus		0xD1ab
#define PTP_DPC_CANON_EOS_ShutterCounter	0xD1ac
#define PTP_DPC_CANON_EOS_SpecialOption		0xD1ad
#define PTP_DPC_CANON_EOS_PhotoStudioMode	0xD1ae
#define PTP_DPC_CANON_EOS_SerialNumber		0xD1af
#define PTP_DPC_CANON_EOS_EVFOutputDevice	0xD1b0
#define PTP_DPC_CANON_EOS_EVFMode		0xD1b1
#define PTP_DPC_CANON_EOS_DepthOfFieldPreview	0xD1b2
#define PTP_DPC_CANON_EOS_EVFSharpness		0xD1b3
#define PTP_DPC_CANON_EOS_EVFWBMode		0xD1b4
#define PTP_DPC_CANON_EOS_EVFClickWBCoeffs	0xD1b5
#define PTP_DPC_CANON_EOS_EVFColorTemp		0xD1b6
#define PTP_DPC_CANON_EOS_ExposureSimMode	0xD1b7
#define PTP_DPC_CANON_EOS_EVFRecordStatus	0xD1b8
#define PTP_DPC_CANON_EOS_LvAfSystem		0xD1ba
#define PTP_DPC_CANON_EOS_MovSize		0xD1bb
#define PTP_DPC_CANON_EOS_LvViewTypeSelect	0xD1bc
#define PTP_DPC_CANON_EOS_Artist		0xD1d0
#define PTP_DPC_CANON_EOS_Copyright		0xD1d1
#define PTP_DPC_CANON_EOS_BracketValue		0xD1d2
#define PTP_DPC_CANON_EOS_FocusInfoEx		0xD1d3
#define PTP_DPC_CANON_EOS_DepthOfField		0xD1d4
#define PTP_DPC_CANON_EOS_Brightness		0xD1d5
#define PTP_DPC_CANON_EOS_LensAdjustParams	0xD1d6
#define PTP_DPC_CANON_EOS_EFComp		0xD1d7
#define PTP_DPC_CANON_EOS_LensName		0xD1d8
#define PTP_DPC_CANON_EOS_AEB			0xD1d9
#define PTP_DPC_CANON_EOS_StroboSetting		0xD1da
#define PTP_DPC_CANON_EOS_StroboWirelessSetting	0xD1db
#define PTP_DPC_CANON_EOS_StroboFiring		0xD1dc
#define PTP_DPC_CANON_EOS_LensID		0xD1dd

/* Nikon extension device property codes */
#define PTP_DPC_NIKON_ShootingBank			0xD010
#define PTP_DPC_NIKON_ShootingBankNameA 		0xD011
#define PTP_DPC_NIKON_ShootingBankNameB			0xD012
#define PTP_DPC_NIKON_ShootingBankNameC			0xD013
#define PTP_DPC_NIKON_ShootingBankNameD			0xD014
#define PTP_DPC_NIKON_ResetBank0			0xD015
#define PTP_DPC_NIKON_RawCompression			0xD016
#define PTP_DPC_NIKON_WhiteBalanceAutoBias		0xD017
#define PTP_DPC_NIKON_WhiteBalanceTungstenBias		0xD018
#define PTP_DPC_NIKON_WhiteBalanceFluorescentBias	0xD019
#define PTP_DPC_NIKON_WhiteBalanceDaylightBias		0xD01A
#define PTP_DPC_NIKON_WhiteBalanceFlashBias		0xD01B
#define PTP_DPC_NIKON_WhiteBalanceCloudyBias		0xD01C
#define PTP_DPC_NIKON_WhiteBalanceShadeBias		0xD01D
#define PTP_DPC_NIKON_WhiteBalanceColorTemperature	0xD01E
#define PTP_DPC_NIKON_WhiteBalancePresetNo		0xD01F
#define PTP_DPC_NIKON_WhiteBalancePresetName0		0xD020
#define PTP_DPC_NIKON_WhiteBalancePresetName1		0xD021
#define PTP_DPC_NIKON_WhiteBalancePresetName2		0xD022
#define PTP_DPC_NIKON_WhiteBalancePresetName3		0xD023
#define PTP_DPC_NIKON_WhiteBalancePresetName4		0xD024
#define PTP_DPC_NIKON_WhiteBalancePresetVal0		0xD025
#define PTP_DPC_NIKON_WhiteBalancePresetVal1		0xD026
#define PTP_DPC_NIKON_WhiteBalancePresetVal2		0xD027
#define PTP_DPC_NIKON_WhiteBalancePresetVal3		0xD028
#define PTP_DPC_NIKON_WhiteBalancePresetVal4		0xD029
#define PTP_DPC_NIKON_ImageSharpening			0xD02A
#define PTP_DPC_NIKON_ToneCompensation			0xD02B
#define PTP_DPC_NIKON_ColorModel			0xD02C
#define PTP_DPC_NIKON_HueAdjustment			0xD02D
#define PTP_DPC_NIKON_NonCPULensDataFocalLength		0xD02E	/* Set FMM Manual */
#define PTP_DPC_NIKON_NonCPULensDataMaximumAperture	0xD02F	/* Set F0 Manual */
#define PTP_DPC_NIKON_ShootingMode			0xD030
#define PTP_DPC_NIKON_JPEG_Compression_Policy		0xD031
#define PTP_DPC_NIKON_ColorSpace			0xD032
#define PTP_DPC_NIKON_AutoDXCrop			0xD033
#define PTP_DPC_NIKON_CSMMenuBankSelect			0xD040
#define PTP_DPC_NIKON_MenuBankNameA			0xD041
#define PTP_DPC_NIKON_MenuBankNameB			0xD042
#define PTP_DPC_NIKON_MenuBankNameC			0xD043
#define PTP_DPC_NIKON_MenuBankNameD			0xD044
#define PTP_DPC_NIKON_ResetBank				0xD045
#define PTP_DPC_NIKON_A1AFCModePriority			0xD048
#define PTP_DPC_NIKON_A2AFSModePriority			0xD049
#define PTP_DPC_NIKON_A3GroupDynamicAF			0xD04A
#define PTP_DPC_NIKON_A4AFActivation			0xD04B
#define PTP_DPC_NIKON_FocusAreaIllumManualFocus		0xD04C
#define PTP_DPC_NIKON_FocusAreaIllumContinuous		0xD04D
#define PTP_DPC_NIKON_FocusAreaIllumWhenSelected 	0xD04E
#define PTP_DPC_NIKON_FocusAreaWrap			0xD04F /* area sel */
#define PTP_DPC_NIKON_VerticalAFON			0xD050
#define PTP_DPC_NIKON_AFLockOn				0xD051
#define PTP_DPC_NIKON_FocusAreaZone			0xD052
#define PTP_DPC_NIKON_EnableCopyright			0xD053
#define PTP_DPC_NIKON_ISOAuto				0xD054
#define PTP_DPC_NIKON_EVISOStep				0xD055
#define PTP_DPC_NIKON_EVStep				0xD056 /* EV Step SS FN */
#define PTP_DPC_NIKON_EVStepExposureComp		0xD057
#define PTP_DPC_NIKON_ExposureCompensation		0xD058
#define PTP_DPC_NIKON_CenterWeightArea			0xD059
#define PTP_DPC_NIKON_ExposureBaseMatrix		0xD05A
#define PTP_DPC_NIKON_ExposureBaseCenter		0xD05B
#define PTP_DPC_NIKON_ExposureBaseSpot			0xD05C
#define PTP_DPC_NIKON_LiveViewAF			0xD05D
#define PTP_DPC_NIKON_AELockMode			0xD05E
#define PTP_DPC_NIKON_AELAFLMode			0xD05F
#define PTP_DPC_NIKON_MeterOff				0xD062
#define PTP_DPC_NIKON_SelfTimer				0xD063
#define PTP_DPC_NIKON_MonitorOff			0xD064
#define PTP_DPC_NIKON_ImgConfTime			0xD065
#define PTP_DPC_NIKON_AngleLevel			0xD067
#define PTP_DPC_NIKON_D1ShootingSpeed			0xD068 /* continous speed low */
#define PTP_DPC_NIKON_D2MaximumShots			0xD069
#define PTP_DPC_NIKON_D3ExpDelayMode			0xD06A
#define PTP_DPC_NIKON_LongExposureNoiseReduction	0xD06B
#define PTP_DPC_NIKON_FileNumberSequence		0xD06C
#define PTP_DPC_NIKON_ControlPanelFinderRearControl	0xD06D
#define PTP_DPC_NIKON_ControlPanelFinderViewfinder	0xD06E
#define PTP_DPC_NIKON_D7Illumination			0xD06F
#define PTP_DPC_NIKON_NrHighISO				0xD070
#define PTP_DPC_NIKON_SHSET_CH_GUID_DISP		0xD071
#define PTP_DPC_NIKON_ArtistName			0xD072
#define PTP_DPC_NIKON_CopyrightInfo			0xD073
#define PTP_DPC_NIKON_FlashSyncSpeed			0xD074
#define PTP_DPC_NIKON_FlashShutterSpeed			0xD075	/* SB Low Limit */
#define PTP_DPC_NIKON_E3AAFlashMode			0xD076
#define PTP_DPC_NIKON_E4ModelingFlash			0xD077
#define PTP_DPC_NIKON_BracketSet			0xD078	/* Bracket Type? */
#define PTP_DPC_NIKON_E6ManualModeBracketing		0xD079	/* Bracket Factor? */
#define PTP_DPC_NIKON_BracketOrder			0xD07A
#define PTP_DPC_NIKON_E8AutoBracketSelection		0xD07B	/* Bracket Method? */
#define PTP_DPC_NIKON_BracketingSet			0xD07C
#define PTP_DPC_NIKON_F1CenterButtonShootingMode	0xD080
#define PTP_DPC_NIKON_CenterButtonPlaybackMode		0xD081
#define PTP_DPC_NIKON_F2Multiselector			0xD082
#define PTP_DPC_NIKON_F3PhotoInfoPlayback		0xD083	/* MultiSelector Dir */
#define PTP_DPC_NIKON_F4AssignFuncButton		0xD084  /* CMD Dial Rotate */
#define PTP_DPC_NIKON_F5CustomizeCommDials		0xD085  /* CMD Dial Change */
#define PTP_DPC_NIKON_ReverseCommandDial		0xD086  /* CMD Dial FN Set */
#define PTP_DPC_NIKON_ApertureSetting			0xD087  /* CMD Dial Active */
#define PTP_DPC_NIKON_MenusAndPlayback			0xD088  /* CMD Dial Active */
#define PTP_DPC_NIKON_F6ButtonsAndDials			0xD089  /* Universal Mode? */
#define PTP_DPC_NIKON_NoCFCard				0xD08A	/* Enable Shutter? */
#define PTP_DPC_NIKON_CenterButtonZoomRatio		0xD08B
#define PTP_DPC_NIKON_FunctionButton2			0xD08C
#define PTP_DPC_NIKON_AFAreaPoint			0xD08D
#define PTP_DPC_NIKON_NormalAFOn			0xD08E
#define PTP_DPC_NIKON_ImageCommentString		0xD090
#define PTP_DPC_NIKON_ImageCommentEnable		0xD091
#define PTP_DPC_NIKON_ImageRotation			0xD092
#define PTP_DPC_NIKON_ManualSetLensNo			0xD093
#define PTP_DPC_NIKON_MovScreenSize			0xD0A0
#define PTP_DPC_NIKON_MovVoice				0xD0A1
#define PTP_DPC_NIKON_Bracketing			0xD0C0
#define PTP_DPC_NIKON_AutoExposureBracketStep		0xD0C1
#define PTP_DPC_NIKON_AutoExposureBracketProgram	0xD0C2
#define PTP_DPC_NIKON_AutoExposureBracketCount		0xD0C3
#define PTP_DPC_NIKON_WhiteBalanceBracketStep		0xD0C4
#define PTP_DPC_NIKON_WhiteBalanceBracketProgram	0xD0C5
#define PTP_DPC_NIKON_LensID				0xD0E0
#define PTP_DPC_NIKON_LensSort				0xD0E1
#define PTP_DPC_NIKON_LensType				0xD0E2
#define PTP_DPC_NIKON_FocalLengthMin			0xD0E3
#define PTP_DPC_NIKON_FocalLengthMax			0xD0E4
#define PTP_DPC_NIKON_MaxApAtMinFocalLength		0xD0E5
#define PTP_DPC_NIKON_MaxApAtMaxFocalLength		0xD0E6
#define PTP_DPC_NIKON_FinderISODisp			0xD0F0
#define PTP_DPC_NIKON_AutoOffPhoto			0xD0F2
#define PTP_DPC_NIKON_AutoOffMenu			0xD0F3
#define PTP_DPC_NIKON_AutoOffInfo			0xD0F4
#define PTP_DPC_NIKON_SelfTimerShootNum			0xD0F5
#define PTP_DPC_NIKON_VignetteCtrl			0xD0F7
#define PTP_DPC_NIKON_ExposureTime			0xD100	/* Shutter Speed */
#define PTP_DPC_NIKON_ACPower				0xD101
#define PTP_DPC_NIKON_WarningStatus			0xD102
#define PTP_DPC_NIKON_MaximumShots			0xD103 /* remain shots (in RAM buffer?) */
#define PTP_DPC_NIKON_AFLockStatus			0xD104
#define PTP_DPC_NIKON_AELockStatus			0xD105
#define PTP_DPC_NIKON_FVLockStatus			0xD106
#define PTP_DPC_NIKON_AutofocusLCDTopMode2		0xD107
#define PTP_DPC_NIKON_AutofocusArea			0xD108
#define PTP_DPC_NIKON_FlexibleProgram			0xD109
#define PTP_DPC_NIKON_LightMeter			0xD10A	/* Exposure Status */
#define PTP_DPC_NIKON_RecordingMedia			0xD10B	/* Card or SDRAM */
#define PTP_DPC_NIKON_USBSpeed				0xD10C
#define PTP_DPC_NIKON_CCDNumber				0xD10D
#define PTP_DPC_NIKON_CameraOrientation			0xD10E
#define PTP_DPC_NIKON_GroupPtnType			0xD10F
#define PTP_DPC_NIKON_FNumberLock			0xD110
#define PTP_DPC_NIKON_ExposureApertureLock		0xD111	/* shutterspeed lock*/
#define PTP_DPC_NIKON_TVLockSetting			0xD112
#define PTP_DPC_NIKON_AVLockSetting			0xD113
#define PTP_DPC_NIKON_IllumSetting			0xD114
#define PTP_DPC_NIKON_FocusPointBright			0xD115
#define PTP_DPC_NIKON_ExternalFlashAttached		0xD120
#define PTP_DPC_NIKON_ExternalFlashStatus		0xD121
#define PTP_DPC_NIKON_ExternalFlashSort			0xD122
#define PTP_DPC_NIKON_ExternalFlashMode			0xD123
#define PTP_DPC_NIKON_ExternalFlashCompensation		0xD124
#define PTP_DPC_NIKON_NewExternalFlashMode		0xD125
#define PTP_DPC_NIKON_FlashExposureCompensation		0xD126
#define PTP_DPC_NIKON_OptimizeImage			0xD140
#define PTP_DPC_NIKON_Saturation			0xD142
#define PTP_DPC_NIKON_BW_FillerEffect			0xD143
#define PTP_DPC_NIKON_BW_Sharpness			0xD144
#define PTP_DPC_NIKON_BW_Contrast			0xD145
#define PTP_DPC_NIKON_BW_Setting_Type			0xD146
#define PTP_DPC_NIKON_Slot2SaveMode			0xD148
#define PTP_DPC_NIKON_RawBitMode			0xD149
#define PTP_DPC_NIKON_ISOAutoTime			0xD14E
#define PTP_DPC_NIKON_FlourescentType			0xD14F
#define PTP_DPC_NIKON_TuneColourTemperature		0xD150
#define PTP_DPC_NIKON_TunePreset0			0xD151
#define PTP_DPC_NIKON_TunePreset1			0xD152
#define PTP_DPC_NIKON_TunePreset2			0xD153
#define PTP_DPC_NIKON_TunePreset3			0xD154
#define PTP_DPC_NIKON_TunePreset4			0xD155
#define PTP_DPC_NIKON_BeepOff				0xD160
#define PTP_DPC_NIKON_AutofocusMode			0xD161
#define PTP_DPC_NIKON_AFAssist				0xD163
#define PTP_DPC_NIKON_PADVPMode				0xD164	/* iso auto time */
#define PTP_DPC_NIKON_ImageReview			0xD165
#define PTP_DPC_NIKON_AFAreaIllumination		0xD166
#define PTP_DPC_NIKON_FlashMode				0xD167
#define PTP_DPC_NIKON_FlashCommanderMode		0xD168
#define PTP_DPC_NIKON_FlashSign				0xD169
#define PTP_DPC_NIKON_ISO_Auto				0xD16A
#define PTP_DPC_NIKON_RemoteTimeout			0xD16B
#define PTP_DPC_NIKON_GridDisplay			0xD16C
#define PTP_DPC_NIKON_FlashModeManualPower		0xD16D
#define PTP_DPC_NIKON_FlashModeCommanderPower		0xD16E
#define PTP_DPC_NIKON_AutoFP				0xD16F
#define PTP_DPC_NIKON_CSMMenu				0xD180
#define PTP_DPC_NIKON_WarningDisplay			0xD181
#define PTP_DPC_NIKON_BatteryCellKind			0xD182
#define PTP_DPC_NIKON_ISOAutoHiLimit			0xD183
#define PTP_DPC_NIKON_DynamicAFArea			0xD184
#define PTP_DPC_NIKON_ContinuousSpeedHigh		0xD186
#define PTP_DPC_NIKON_InfoDispSetting			0xD187
#define PTP_DPC_NIKON_PreviewButton			0xD189
#define PTP_DPC_NIKON_PreviewButton2			0xD18A
#define PTP_DPC_NIKON_AEAFLockButton2			0xD18B
#define PTP_DPC_NIKON_IndicatorDisp			0xD18D
#define PTP_DPC_NIKON_CellKindPriority			0xD18E
#define PTP_DPC_NIKON_BracketingFramesAndSteps		0xD190
#define PTP_DPC_NIKON_LiveViewMode			0xD1A0
#define PTP_DPC_NIKON_LiveViewDriveMode			0xD1A1
#define PTP_DPC_NIKON_LiveViewStatus			0xD1A2
#define PTP_DPC_NIKON_LiveViewImageZoomRatio		0xD1A3
#define PTP_DPC_NIKON_LiveViewProhibitCondition		0xD1A4
#define PTP_DPC_NIKON_ExposureDisplayStatus		0xD1B0
#define PTP_DPC_NIKON_ExposureIndicateStatus		0xD1B1
#define PTP_DPC_NIKON_InfoDispErrStatus			0xD1B2
#define PTP_DPC_NIKON_ExposureIndicateLightup		0xD1B3
#define PTP_DPC_NIKON_FlashOpen				0xD1C0
#define PTP_DPC_NIKON_FlashCharged			0xD1C1
#define PTP_DPC_NIKON_FlashMRepeatValue			0xD1D0
#define PTP_DPC_NIKON_FlashMRepeatCount			0xD1D1
#define PTP_DPC_NIKON_FlashMRepeatInterval		0xD1D2
#define PTP_DPC_NIKON_FlashCommandChannel		0xD1D3
#define PTP_DPC_NIKON_FlashCommandSelfMode		0xD1D4
#define PTP_DPC_NIKON_FlashCommandSelfCompensation	0xD1D5
#define PTP_DPC_NIKON_FlashCommandSelfValue		0xD1D6
#define PTP_DPC_NIKON_FlashCommandAMode			0xD1D7
#define PTP_DPC_NIKON_FlashCommandACompensation		0xD1D8
#define PTP_DPC_NIKON_FlashCommandAValue		0xD1D9
#define PTP_DPC_NIKON_FlashCommandBMode			0xD1DA
#define PTP_DPC_NIKON_FlashCommandBCompensation		0xD1DB
#define PTP_DPC_NIKON_FlashCommandBValue		0xD1DC
#define PTP_DPC_NIKON_ActivePicCtrlItem			0xD200
#define PTP_DPC_NIKON_ChangePicCtrlItem			0xD201

/* Microsoft/MTP specific */
#define PTP_DPC_MTP_SecureTime				0xD101
#define PTP_DPC_MTP_DeviceCertificate			0xD102
#define PTP_DPC_MTP_RevocationInfo			0xD103
#define PTP_DPC_MTP_SynchronizationPartner		0xD401
#define PTP_DPC_MTP_DeviceFriendlyName			0xD402
#define PTP_DPC_MTP_VolumeLevel				0xD403
#define PTP_DPC_MTP_DeviceIcon				0xD405
#define PTP_DPC_MTP_SessionInitiatorInfo		0xD406
#define PTP_DPC_MTP_PerceivedDeviceType			0xD407
#define PTP_DPC_MTP_PlaybackRate                        0xD410
#define PTP_DPC_MTP_PlaybackObject                      0xD411
#define PTP_DPC_MTP_PlaybackContainerIndex              0xD412
#define PTP_DPC_MTP_PlaybackPosition                    0xD413
#define PTP_DPC_MTP_PlaysForSureID                      0xD131

/* Zune specific property codes */
#define PTP_DPC_MTP_Zune_UnknownVersion			0xD181

/* MTP specific Object Properties */
#define PTP_OPC_StorageID				0xDC01
#define PTP_OPC_ObjectFormat				0xDC02
#define PTP_OPC_ProtectionStatus			0xDC03
#define PTP_OPC_ObjectSize				0xDC04
#define PTP_OPC_AssociationType				0xDC05
#define PTP_OPC_AssociationDesc				0xDC06
#define PTP_OPC_ObjectFileName				0xDC07
#define PTP_OPC_DateCreated				0xDC08
#define PTP_OPC_DateModified				0xDC09
#define PTP_OPC_Keywords				0xDC0A
#define PTP_OPC_ParentObject				0xDC0B
#define PTP_OPC_AllowedFolderContents			0xDC0C
#define PTP_OPC_Hidden					0xDC0D
#define PTP_OPC_SystemObject				0xDC0E
#define PTP_OPC_PersistantUniqueObjectIdentifier	0xDC41
#define PTP_OPC_SyncID					0xDC42
#define PTP_OPC_PropertyBag				0xDC43
#define PTP_OPC_Name					0xDC44
#define PTP_OPC_CreatedBy				0xDC45
#define PTP_OPC_Artist					0xDC46
#define PTP_OPC_DateAuthored				0xDC47
#define PTP_OPC_Description				0xDC48
#define PTP_OPC_URLReference				0xDC49
#define PTP_OPC_LanguageLocale				0xDC4A
#define PTP_OPC_CopyrightInformation			0xDC4B
#define PTP_OPC_Source					0xDC4C
#define PTP_OPC_OriginLocation				0xDC4D
#define PTP_OPC_DateAdded				0xDC4E
#define PTP_OPC_NonConsumable				0xDC4F
#define PTP_OPC_CorruptOrUnplayable			0xDC50
#define PTP_OPC_ProducerSerialNumber			0xDC51
#define PTP_OPC_RepresentativeSampleFormat		0xDC81
#define PTP_OPC_RepresentativeSampleSize		0xDC82
#define PTP_OPC_RepresentativeSampleHeight		0xDC83
#define PTP_OPC_RepresentativeSampleWidth		0xDC84
#define PTP_OPC_RepresentativeSampleDuration		0xDC85
#define PTP_OPC_RepresentativeSampleData		0xDC86
#define PTP_OPC_Width					0xDC87
#define PTP_OPC_Height					0xDC88
#define PTP_OPC_Duration				0xDC89
#define PTP_OPC_Rating					0xDC8A
#define PTP_OPC_Track					0xDC8B
#define PTP_OPC_Genre					0xDC8C
#define PTP_OPC_Credits					0xDC8D
#define PTP_OPC_Lyrics					0xDC8E
#define PTP_OPC_SubscriptionContentID			0xDC8F
#define PTP_OPC_ProducedBy				0xDC90
#define PTP_OPC_UseCount				0xDC91
#define PTP_OPC_SkipCount				0xDC92
#define PTP_OPC_LastAccessed				0xDC93
#define PTP_OPC_ParentalRating				0xDC94
#define PTP_OPC_MetaGenre				0xDC95
#define PTP_OPC_Composer				0xDC96
#define PTP_OPC_EffectiveRating				0xDC97
#define PTP_OPC_Subtitle				0xDC98
#define PTP_OPC_OriginalReleaseDate			0xDC99
#define PTP_OPC_AlbumName				0xDC9A
#define PTP_OPC_AlbumArtist				0xDC9B
#define PTP_OPC_Mood					0xDC9C
#define PTP_OPC_DRMStatus				0xDC9D
#define PTP_OPC_SubDescription				0xDC9E
#define PTP_OPC_IsCropped				0xDCD1
#define PTP_OPC_IsColorCorrected			0xDCD2
#define PTP_OPC_ImageBitDepth				0xDCD3
#define PTP_OPC_Fnumber					0xDCD4
#define PTP_OPC_ExposureTime				0xDCD5
#define PTP_OPC_ExposureIndex				0xDCD6
#define PTP_OPC_DisplayName				0xDCE0
#define PTP_OPC_BodyText				0xDCE1
#define PTP_OPC_Subject					0xDCE2
#define PTP_OPC_Priority				0xDCE3
#define PTP_OPC_GivenName				0xDD00
#define PTP_OPC_MiddleNames				0xDD01
#define PTP_OPC_FamilyName				0xDD02
#define PTP_OPC_Prefix					0xDD03
#define PTP_OPC_Suffix					0xDD04
#define PTP_OPC_PhoneticGivenName			0xDD05
#define PTP_OPC_PhoneticFamilyName			0xDD06
#define PTP_OPC_EmailPrimary				0xDD07
#define PTP_OPC_EmailPersonal1				0xDD08
#define PTP_OPC_EmailPersonal2				0xDD09
#define PTP_OPC_EmailBusiness1				0xDD0A
#define PTP_OPC_EmailBusiness2				0xDD0B
#define PTP_OPC_EmailOthers				0xDD0C
#define PTP_OPC_PhoneNumberPrimary			0xDD0D
#define PTP_OPC_PhoneNumberPersonal			0xDD0E
#define PTP_OPC_PhoneNumberPersonal2			0xDD0F
#define PTP_OPC_PhoneNumberBusiness			0xDD10
#define PTP_OPC_PhoneNumberBusiness2			0xDD11
#define PTP_OPC_PhoneNumberMobile			0xDD12
#define PTP_OPC_PhoneNumberMobile2			0xDD13
#define PTP_OPC_FaxNumberPrimary			0xDD14
#define PTP_OPC_FaxNumberPersonal			0xDD15
#define PTP_OPC_FaxNumberBusiness			0xDD16
#define PTP_OPC_PagerNumber				0xDD17
#define PTP_OPC_PhoneNumberOthers			0xDD18
#define PTP_OPC_PrimaryWebAddress			0xDD19
#define PTP_OPC_PersonalWebAddress			0xDD1A
#define PTP_OPC_BusinessWebAddress			0xDD1B
#define PTP_OPC_InstantMessengerAddress			0xDD1C
#define PTP_OPC_InstantMessengerAddress2		0xDD1D
#define PTP_OPC_InstantMessengerAddress3		0xDD1E
#define PTP_OPC_PostalAddressPersonalFull		0xDD1F
#define PTP_OPC_PostalAddressPersonalFullLine1		0xDD20
#define PTP_OPC_PostalAddressPersonalFullLine2		0xDD21
#define PTP_OPC_PostalAddressPersonalFullCity		0xDD22
#define PTP_OPC_PostalAddressPersonalFullRegion		0xDD23
#define PTP_OPC_PostalAddressPersonalFullPostalCode	0xDD24
#define PTP_OPC_PostalAddressPersonalFullCountry	0xDD25
#define PTP_OPC_PostalAddressBusinessFull		0xDD26
#define PTP_OPC_PostalAddressBusinessLine1		0xDD27
#define PTP_OPC_PostalAddressBusinessLine2		0xDD28
#define PTP_OPC_PostalAddressBusinessCity		0xDD29
#define PTP_OPC_PostalAddressBusinessRegion		0xDD2A
#define PTP_OPC_PostalAddressBusinessPostalCode		0xDD2B
#define PTP_OPC_PostalAddressBusinessCountry		0xDD2C
#define PTP_OPC_PostalAddressOtherFull			0xDD2D
#define PTP_OPC_PostalAddressOtherLine1			0xDD2E
#define PTP_OPC_PostalAddressOtherLine2			0xDD2F
#define PTP_OPC_PostalAddressOtherCity			0xDD30
#define PTP_OPC_PostalAddressOtherRegion		0xDD31
#define PTP_OPC_PostalAddressOtherPostalCode		0xDD32
#define PTP_OPC_PostalAddressOtherCountry		0xDD33
#define PTP_OPC_OrganizationName			0xDD34
#define PTP_OPC_PhoneticOrganizationName		0xDD35
#define PTP_OPC_Role					0xDD36
#define PTP_OPC_Birthdate				0xDD37
#define PTP_OPC_MessageTo				0xDD40
#define PTP_OPC_MessageCC				0xDD41
#define PTP_OPC_MessageBCC				0xDD42
#define PTP_OPC_MessageRead				0xDD43
#define PTP_OPC_MessageReceivedTime			0xDD44
#define PTP_OPC_MessageSender				0xDD45
#define PTP_OPC_ActivityBeginTime			0xDD50
#define PTP_OPC_ActivityEndTime				0xDD51
#define PTP_OPC_ActivityLocation			0xDD52
#define PTP_OPC_ActivityRequiredAttendees		0xDD54
#define PTP_OPC_ActivityOptionalAttendees		0xDD55
#define PTP_OPC_ActivityResources			0xDD56
#define PTP_OPC_ActivityAccepted			0xDD57
#define PTP_OPC_Owner					0xDD5D
#define PTP_OPC_Editor					0xDD5E
#define PTP_OPC_Webmaster				0xDD5F
#define PTP_OPC_URLSource				0xDD60
#define PTP_OPC_URLDestination				0xDD61
#define PTP_OPC_TimeBookmark				0xDD62
#define PTP_OPC_ObjectBookmark				0xDD63
#define PTP_OPC_ByteBookmark				0xDD64
#define PTP_OPC_LastBuildDate				0xDD70
#define PTP_OPC_TimetoLive				0xDD71
#define PTP_OPC_MediaGUID				0xDD72
#define PTP_OPC_TotalBitRate				0xDE91
#define PTP_OPC_BitRateType				0xDE92
#define PTP_OPC_SampleRate				0xDE93
#define PTP_OPC_NumberOfChannels			0xDE94
#define PTP_OPC_AudioBitDepth				0xDE95
#define PTP_OPC_ScanDepth				0xDE97
#define PTP_OPC_AudioWAVECodec				0xDE99
#define PTP_OPC_AudioBitRate				0xDE9A
#define PTP_OPC_VideoFourCCCodec			0xDE9B
#define PTP_OPC_VideoBitRate				0xDE9C
#define PTP_OPC_FramesPerThousandSeconds		0xDE9D
#define PTP_OPC_KeyFrameDistance			0xDE9E
#define PTP_OPC_BufferSize				0xDE9F
#define PTP_OPC_EncodingQuality				0xDEA0
#define PTP_OPC_EncodingProfile				0xDEA1
#define PTP_OPC_BuyFlag					0xD901

/* WiFi Provisioning MTP Extension property codes */
#define PTP_OPC_WirelessConfigurationFile		0xB104

/* Device Property Form Flag */

#define PTP_DPFF_None			0x00
#define PTP_DPFF_Range			0x01
#define PTP_DPFF_Enumeration		0x02

/* Object Property Codes used by MTP (first 3 are same as DPFF codes) */
#define PTP_OPFF_None			0x00
#define PTP_OPFF_Range			0x01
#define PTP_OPFF_Enumeration		0x02
#define PTP_OPFF_DateTime		0x03
#define PTP_OPFF_FixedLengthArray	0x04
#define PTP_OPFF_RegularExpression	0x05
#define PTP_OPFF_ByteArray		0x06
#define PTP_OPFF_LongString		0xFF

/* Device Property GetSet type */
#define PTP_DPGS_Get			0x00
#define PTP_DPGS_GetSet			0x01

/* Glue stuff starts here */

typedef struct _PTPParams PTPParams;


typedef uint16_t (* PTPDataGetFunc)	(PTPParams* params, void*priv,
					unsigned long wantlen,
	                                unsigned char *data, unsigned long *gotlen);

typedef uint16_t (* PTPDataPutFunc)	(PTPParams* params, void*priv,
					unsigned long sendlen,
	                                unsigned char *data, unsigned long *putlen);
typedef struct _PTPDataHandler {
	PTPDataGetFunc		getfunc;
	PTPDataPutFunc		putfunc;
	void			*priv;
} PTPDataHandler;

/*
 * This functions take PTP oriented arguments and send them over an
 * appropriate data layer doing byteorder conversion accordingly.
 */
typedef uint16_t (* PTPIOSendReq)	(PTPParams* params, PTPContainer* req);
typedef uint16_t (* PTPIOSendData)	(PTPParams* params, PTPContainer* ptp,
					 unsigned long size, PTPDataHandler*getter);

typedef uint16_t (* PTPIOGetResp)	(PTPParams* params, PTPContainer* resp);
typedef uint16_t (* PTPIOGetData)	(PTPParams* params, PTPContainer* ptp,
	                                 PTPDataHandler *putter);
typedef uint16_t (* PTPIOCancelReq)	(PTPParams* params, uint32_t transaction_id);

/* debug functions */
typedef void (* PTPErrorFunc) (void *data, const char *format, va_list args)
#if (__GNUC__ >= 3)
	__attribute__((__format__(printf,2,0)))
#endif
;
typedef void (* PTPDebugFunc) (void *data, const char *format, va_list args)
#if (__GNUC__ >= 3)
	__attribute__((__format__(printf,2,0)))
#endif
;

struct _PTPObject {
	uint32_t	oid;
	unsigned int	flags;
#define PTPOBJECT_OBJECTINFO_LOADED	(1<<0)
#define PTPOBJECT_CANONFLAGS_LOADED	(1<<1)
#define PTPOBJECT_MTPPROPLIST_LOADED	(1<<2)
#define PTPOBJECT_DIRECTORY_LOADED	(1<<3)
#define PTPOBJECT_PARENTOBJECT_LOADED	(1<<4)
#define PTPOBJECT_STORAGEID_LOADED	(1<<5)

	PTPObjectInfo	oi;
	uint32_t	canon_flags;
	MTPProperties	*mtpprops;
	int		nrofmtpprops;
};
typedef struct _PTPObject PTPObject;

struct _PTPParams {
	/* device flags */
	uint32_t	device_flags;

	/* data layer byteorder */
	uint8_t		byteorder;
	uint16_t	maxpacketsize;

	/* PTP IO: Custom IO functions */
	PTPIOSendReq	sendreq_func;
	PTPIOSendData	senddata_func;
	PTPIOGetResp	getresp_func;
	PTPIOGetData	getdata_func;
	PTPIOGetResp	event_check;
	PTPIOGetResp	event_wait;
	PTPIOCancelReq	cancelreq_func;

	/* Custom error and debug function */
	PTPErrorFunc	error_func;
	PTPDebugFunc	debug_func;

	/* Data passed to above functions */
	void		*data;

	/* ptp transaction ID */
	uint32_t	transaction_id;
	/* ptp session ID */
	uint32_t	session_id;

	/* PTP IO: if we have MTP style split header/data transfers */
	int		split_header_data;

	/* PTP: internal structures used by ptp driver */
	PTPObject	*objects;
	int		nrofobjects;

	PTPDeviceInfo	deviceinfo;

	/* PTP: the current event queue */
	PTPContainer	*events;
	int		nrofevents;

	/* PTP: Canon specific flags list */
	PTPCanon_Property	*canon_props;
	int			nrofcanon_props;
	int			canon_viewfinder_on;

	/* PTP: Canon EOS event queue */
	PTPCanon_changes_entry	*backlogentries;
	int			nrofbacklogentries;
	int			eos_captureenabled;

	/* PTP: Wifi profiles */
	uint8_t 	wifi_profiles_version;
	uint8_t		wifi_profiles_number;
	PTPNIKONWifiProfile *wifi_profiles;

	/* IO: PTP/IP related data */
	int		cmdfd, evtfd;
	uint8_t		cameraguid[16];
	uint32_t	eventpipeid;
	char		*cameraname;

#ifdef HAVE_ICONV
	/* PTP: iconv converters */
	iconv_t	cd_locale_to_ucs2;
	iconv_t cd_ucs2_to_locale;
#endif

	/* IO: Sometimes the response packet get send in the dataphase
	 * too. This only happens for a Samsung player now.
	 */
	uint8_t		*response_packet;
	uint16_t	response_packet_size;
};

/* last, but not least - ptp functions */
uint16_t ptp_usb_sendreq	(PTPParams* params, PTPContainer* req);
uint16_t ptp_usb_senddata	(PTPParams* params, PTPContainer* ptp,
				 unsigned long size, PTPDataHandler *handler);
uint16_t ptp_usb_getresp	(PTPParams* params, PTPContainer* resp);
uint16_t ptp_usb_getdata	(PTPParams* params, PTPContainer* ptp, 
	                         PTPDataHandler *handler);
uint16_t ptp_usb_event_check	(PTPParams* params, PTPContainer* event);
uint16_t ptp_usb_event_wait	(PTPParams* params, PTPContainer* event);

uint16_t ptp_usb_control_get_extended_event_data (PTPParams *params, char *buffer, int *size);
uint16_t ptp_usb_control_device_reset_request (PTPParams *params);
uint16_t ptp_usb_control_get_device_status (PTPParams *params, char *buffer, int *size);
uint16_t ptp_usb_control_cancel_request (PTPParams *params, uint32_t transid);


int      ptp_ptpip_connect	(PTPParams* params, const char *port);
uint16_t ptp_ptpip_sendreq	(PTPParams* params, PTPContainer* req);
uint16_t ptp_ptpip_senddata	(PTPParams* params, PTPContainer* ptp,
				unsigned long size, PTPDataHandler *handler);
uint16_t ptp_ptpip_getresp	(PTPParams* params, PTPContainer* resp);
uint16_t ptp_ptpip_getdata	(PTPParams* params, PTPContainer* ptp, 
	                         PTPDataHandler *handler);
uint16_t ptp_ptpip_event_wait	(PTPParams* params, PTPContainer* event);
uint16_t ptp_ptpip_event_check	(PTPParams* params, PTPContainer* event);

uint16_t ptp_getdeviceinfo	(PTPParams* params, PTPDeviceInfo* deviceinfo);

uint16_t ptp_generic_no_data	(PTPParams* params, uint16_t opcode, unsigned int cnt, ...);

uint16_t ptp_opensession	(PTPParams *params, uint32_t session);

/**
 * ptp_closesession:
 * params:      PTPParams*
 *
 * Closes session.
 *
 * Return values: Some PTP_RC_* code.
 **/
#define ptp_closesession(params) ptp_generic_no_data(params,PTP_OC_CloseSession,0)
/**
 * ptp_resetdevice:
 * params:      PTPParams*
 *              
 * Uses the built-in function to reset the device
 *
 * Return values: Some PTP_RC_* code.
 *
 */
#define ptp_resetdevice(params) ptp_generic_no_data(params,PTP_OC_ResetDevice,0)

uint16_t ptp_getstorageids	(PTPParams* params, PTPStorageIDs* storageids);
uint16_t ptp_getstorageinfo 	(PTPParams* params, uint32_t storageid,
				PTPStorageInfo* storageinfo);
/**
 * ptp_formatstore:
 * params:      PTPParams*
 *              storageid               - StorageID
 *
 * Formats the storage on the device.
 *
 * Return values: Some PTP_RC_* code.
 **/
#define ptp_formatstore(params,storageid) ptp_generic_no_data(params,PTP_OC_FormatStore,1,storageid)

uint16_t ptp_getobjecthandles 	(PTPParams* params, uint32_t storage,
				uint32_t objectformatcode,
				uint32_t associationOH,
				PTPObjectHandles* objecthandles);

uint16_t ptp_getnumobjects 	(PTPParams* params, uint32_t storage,
				uint32_t objectformatcode,
				uint32_t associationOH,
				uint32_t* numobs);

uint16_t ptp_getobjectinfo	(PTPParams *params, uint32_t handle,
				PTPObjectInfo* objectinfo);

uint16_t ptp_getobject		(PTPParams *params, uint32_t handle,
				unsigned char** object);
uint16_t ptp_getobject_tofd     (PTPParams* params, uint32_t handle, int fd);
uint16_t ptp_getobject_to_handler (PTPParams* params, uint32_t handle, PTPDataHandler*);
uint16_t ptp_getpartialobject	(PTPParams* params, uint32_t handle, uint32_t offset,
				uint32_t maxbytes, unsigned char** object);
uint16_t ptp_getthumb		(PTPParams *params, uint32_t handle,
				unsigned char** object);

uint16_t ptp_deleteobject	(PTPParams* params, uint32_t handle,
				uint32_t ofc);

uint16_t ptp_sendobjectinfo	(PTPParams* params, uint32_t* store,
				uint32_t* parenthandle, uint32_t* handle,
				PTPObjectInfo* objectinfo);
/**
 * ptp_setobjectprotection:
 * params:      PTPParams*
 *              uint16_t newprot        - object protection flag
 *              
 * Set protection of object.
 *
 * Return values: Some PTP_RC_* code.
 *
 */
#define ptp_setobjectprotection(params,oid,newprot) ptp_generic_no_data(params,PTP_OC_SetObjectProtection,2,oid,newprot)
uint16_t ptp_sendobject		(PTPParams* params, unsigned char* object,
				 uint32_t size);
uint16_t ptp_sendobject_fromfd  (PTPParams* params, int fd, uint32_t size);
uint16_t ptp_sendobject_from_handler  (PTPParams* params, PTPDataHandler*, uint32_t size);
/**
 * ptp_initiatecapture:
 * params:      PTPParams*
 *              storageid               - destination StorageID on Responder
 *              ofc                     - object format code
 * 
 * Causes device to initiate the capture of one or more new data objects
 * according to its current device properties, storing the data into store
 * indicated by storageid. If storageid is 0x00000000, the object(s) will
 * be stored in a store that is determined by the capturing device.
 * The capturing of new data objects is an asynchronous operation.
 *
 * Return values: Some PTP_RC_* code.
 **/
#define ptp_initiatecapture(params,storageid,ofc) ptp_generic_no_data(params,PTP_OC_InitiateCapture,2,storageid,ofc)

uint16_t ptp_getdevicepropdesc	(PTPParams* params, uint16_t propcode,
				PTPDevicePropDesc *devicepropertydesc);
uint16_t ptp_getdevicepropvalue	(PTPParams* params, uint16_t propcode,
				PTPPropertyValue* value, uint16_t datatype);
uint16_t ptp_setdevicepropvalue (PTPParams* params, uint16_t propcode,
                        	PTPPropertyValue* value, uint16_t datatype);


uint16_t ptp_check_event (PTPParams *params);
int ptp_get_one_event (PTPParams *params, PTPContainer *evt);

/* Microsoft MTP extensions */
uint16_t ptp_mtp_getobjectpropdesc (PTPParams* params, uint16_t opc, uint16_t ofc,
				PTPObjectPropDesc *objectpropertydesc);
uint16_t ptp_mtp_getobjectpropvalue (PTPParams* params, uint32_t oid, uint16_t opc, 
				PTPPropertyValue *value, uint16_t datatype);
uint16_t ptp_mtp_setobjectpropvalue (PTPParams* params, uint32_t oid, uint16_t opc,
				PTPPropertyValue *value, uint16_t datatype);
uint16_t ptp_mtp_getobjectreferences (PTPParams* params, uint32_t handle, uint32_t** ohArray, uint32_t* arraylen);
uint16_t ptp_mtp_setobjectreferences (PTPParams* params, uint32_t handle, uint32_t* ohArray, uint32_t arraylen);
uint16_t ptp_mtp_getobjectproplist (PTPParams* params, uint32_t handle, MTPProperties **props, int *nrofprops);
uint16_t ptp_mtp_sendobjectproplist (PTPParams* params, uint32_t* store, uint32_t* parenthandle, uint32_t* handle,
				     uint16_t objecttype, uint64_t objectsize, MTPProperties *props, int nrofprops);
uint16_t ptp_mtp_setobjectproplist (PTPParams* params, MTPProperties *props, int nrofprops);

/* Eastman Kodak extensions */
uint16_t ptp_ek_9007 (PTPParams* params, unsigned char **serial, unsigned int *size);
uint16_t ptp_ek_9009 (PTPParams* params, uint32_t*, uint32_t*);
uint16_t ptp_ek_900c (PTPParams* params, unsigned char **serial, unsigned int *size);
uint16_t ptp_ek_getserial (PTPParams* params, unsigned char **serial, unsigned int *size);
uint16_t ptp_ek_setserial (PTPParams* params, unsigned char *serial, unsigned int size);
uint16_t ptp_ek_settext (PTPParams* params, PTPEKTextParams *text);
uint16_t ptp_ek_sendfileobjectinfo (PTPParams* params, uint32_t* store,
				uint32_t* parenthandle, uint32_t* handle,
				PTPObjectInfo* objectinfo);
uint16_t ptp_ek_sendfileobject	(PTPParams* params, unsigned char* object,
				uint32_t size);
uint16_t ptp_ek_sendfileobject_from_handler	(PTPParams* params, PTPDataHandler*,
				uint32_t size);

/* Canon PTP extensions */
#define ptp_canon_9012(params) ptp_generic_no_data(params,0x9012,0)
uint16_t ptp_canon_gettreeinfo (PTPParams* params, uint32_t* out);
uint16_t ptp_canon_gettreesize (PTPParams* params, PTPCanon_directtransfer_entry**, unsigned int*cnt);
uint16_t ptp_canon_getpartialobjectinfo (PTPParams* params, uint32_t handle,
				uint32_t p2, uint32_t* size, uint32_t* rp2);

uint16_t ptp_canon_get_mac_address (PTPParams* params, unsigned char **mac);
/**
 * ptp_canon_startshootingmode:
 * params:      PTPParams*
 * 
 * Starts shooting session. It emits a StorageInfoChanged
 * event via the interrupt pipe and pushes the StorageInfoChanged
 * and CANON_CameraModeChange events onto the event stack
 * (see operation PTP_OC_CANON_CheckEvent).
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_startshootingmode(params) ptp_generic_no_data(params,PTP_OC_CANON_InitiateReleaseControl,0)
/**
 * ptp_canon_endshootingmode:
 * params:      PTPParams*
 * 
 * This operation is observed after pressing the Disconnect 
 * button on the Remote Capture app. It emits a StorageInfoChanged 
 * event via the interrupt pipe and pushes the StorageInfoChanged
 * and CANON_CameraModeChange events onto the event stack
 * (see operation PTP_OC_CANON_CheckEvent).
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_endshootingmode(params) ptp_generic_no_data(params,PTP_OC_CANON_TerminateReleaseControl,0)
/**
 * ptp_canon_viewfinderon:
 * params:      PTPParams*
 * 
 * Prior to start reading viewfinder images, one  must call this operation.
 * Supposedly, this operation affects the value of the CANON_ViewfinderMode
 * property.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_viewfinderon(params) ptp_generic_no_data(params,PTP_OC_CANON_ViewfinderOn,0)
/**
 * ptp_canon_viewfinderoff:
 * params:      PTPParams*
 * 
 * Before changing the shooting mode, or when one doesn't need to read
 * viewfinder images any more, one must call this operation.
 * Supposedly, this operation affects the value of the CANON_ViewfinderMode
 * property.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_viewfinderoff(params) ptp_generic_no_data(params,PTP_OC_CANON_ViewfinderOff,0)
/**
 * ptp_canon_reset_aeafawb:
 * params:      PTPParams*
 *              uint32_t flags  - what shall be reset.
 *                      1 - autoexposure
 *                      2 - autofocus
 *                      4 - autowhitebalance
 * 
 * Called "Reset AeAfAwb" (auto exposure, focus, white balance)
 *
 * Return values: Some PTP_RC_* code.
 **/
#define PTP_CANON_RESET_AE	0x1
#define PTP_CANON_RESET_AF	0x2
#define PTP_CANON_RESET_AWB	0x4
#define ptp_canon_reset_aeafawb(params,flags) ptp_generic_no_data(params,PTP_OC_CANON_DoAeAfAwb,1,flags)
uint16_t ptp_canon_checkevent (PTPParams* params, 
				PTPContainer* event, int* isevent);
/**
 * ptp_canon_focuslock:
 *
 * This operation locks the focus. It is followed by the CANON_GetChanges(?)
 * operation in the log. 
 * It affects the CANON_MacroMode property. 
 *
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_focuslock(params) ptp_generic_no_data(params,PTP_OC_CANON_FocusLock,0)
/**
 * ptp_canon_focusunlock:
 *
 * This operation unlocks the focus. It is followed by the CANON_GetChanges(?)
 * operation in the log. 
 * It sets the CANON_MacroMode property value to 1 (where it occurs in the log).
 * 
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_focusunlock(params) ptp_generic_no_data(params,PTP_OC_CANON_FocusUnlock,0)
/**
 * ptp_canon_keepdeviceon:
 *
 * This operation sends a "ping" style message to the camera.
 * 
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_keepdeviceon(params) ptp_generic_no_data(params,PTP_OC_CANON_KeepDeviceOn,0)
/**
 * ptp_canon_eos_keepdeviceon:
 *
 * This operation sends a "ping" style message to the camera.
 * 
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_eos_keepdeviceon(params) ptp_generic_no_data(params,PTP_OC_CANON_EOS_KeepDeviceOn,0)
/**
 * ptp_canon_initiatecaptureinmemory:
 * 
 * This operation starts the image capture according to the current camera
 * settings. When the capture has happened, the camera emits a CaptureComplete
 * event via the interrupt pipe and pushes the CANON_RequestObjectTransfer,
 * CANON_DeviceInfoChanged and CaptureComplete events onto the event stack
 * (see operation CANON_CheckEvent). From the CANON_RequestObjectTransfer
 * event's parameter one can learn the just captured image's ObjectHandle.
 * The image is stored in the camera's own RAM.
 * On the next capture the image will be overwritten!
 *
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_initiatecaptureinmemory(params) ptp_generic_no_data(params,PTP_OC_CANON_InitiateCaptureInMemory,0)
/**
 * ptp_canon_eos_requestdevicepropvalue:
 *
 * This operation sends a "ping" style message to the camera.
 * 
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_eos_requestdevicepropvalue(params,prop) ptp_generic_no_data(params,PTP_OC_CANON_EOS_RequestDevicePropValue,1,prop)
/**
 * ptp_canon_eos_capture:
 * 
 * This starts a EOS400D style capture. You have to use the
 * 0x9116 command to poll for its completion.
 * The image is saved on the CF Card currently.
 *
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_eos_capture(params) ptp_generic_no_data(params,PTP_OC_CANON_EOS_RemoteRelease,0)
uint16_t ptp_canon_eos_getevent (PTPParams* params, PTPCanon_changes_entry **entries, int *nrofentries);
uint16_t ptp_canon_getpartialobject (PTPParams* params, uint32_t handle, 
				uint32_t offset, uint32_t size,
				uint32_t pos, unsigned char** block, 
				uint32_t* readnum);
uint16_t ptp_canon_getviewfinderimage (PTPParams* params, unsigned char** image,
				uint32_t* size);
uint16_t ptp_canon_getchanges (PTPParams* params, uint16_t** props,
				uint32_t* propnum); 
uint16_t ptp_canon_getobjectinfo (PTPParams* params, uint32_t store,
				uint32_t p2, uint32_t parenthandle,
				uint32_t handle, 
				PTPCANONFolderEntry** entries,
				uint32_t* entnum);
uint16_t ptp_canon_eos_getdeviceinfo (PTPParams* params, PTPCanonEOSDeviceInfo*di);
/**
 * ptp_canon_eos_setuilock:
 *
 * This command sets UI lock
 *  
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_eos_setuilock(params) ptp_generic_no_data(params,PTP_OC_CANON_EOS_SetUILock,0)
/**
 * ptp_canon_eos_resetuilock:
 *
 * This command sets UI lock
 *  
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_eos_resetuilock(params) ptp_generic_no_data(params,PTP_OC_CANON_EOS_ResetUILock,0)
/**
 * ptp_canon_eos_start_viewfinder:
 *
 * This command starts Viewfinder mode of newer Canon DSLRs.
 *  
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_eos_start_viewfinder(params) ptp_generic_no_data(params,PTP_OC_CANON_EOS_InitiateViewfinder,0)
/**
 * ptp_canon_eos_end_viewfinder:
 *
 * This command ends Viewfinder mode of newer Canon DSLRs.
 *  
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_eos_end_viewfinder(params) ptp_generic_no_data(params,PTP_OC_CANON_EOS_TerminateViewfinder,0)
uint16_t ptp_canon_eos_get_viewfinder_image (PTPParams* params, unsigned char **data, unsigned int *size);
uint16_t ptp_canon_get_objecthandle_by_name (PTPParams* params, char* name, uint32_t* objectid);
uint16_t ptp_canon_get_directory (PTPParams* params, PTPObjectHandles *handles, PTPObjectInfo **oinfos, uint32_t **flags);
/**
 * ptp_canon_setobjectarchive:
 *
 * params:      PTPParams*
 *              uint32_t        objectid
 *              uint32_t        flags
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_canon_setobjectarchive(params,oid,flags) ptp_generic_no_data(params,PTP_OC_CANON_SetObjectArchive,2,oid,flags)
uint16_t ptp_canon_get_customize_data (PTPParams* params, uint32_t themenr,
				unsigned char **data, unsigned int *size);
uint16_t ptp_canon_getpairinginfo (PTPParams* params, uint32_t nr, unsigned char**, unsigned int*);

uint16_t ptp_canon_eos_getstorageids (PTPParams* params, PTPStorageIDs* storageids);
uint16_t ptp_canon_eos_getstorageinfo (PTPParams* params, uint32_t p1);
uint16_t ptp_canon_eos_getpartialobject (PTPParams* params, uint32_t oid, uint32_t off, uint32_t xsize, unsigned char**data);
uint16_t ptp_canon_eos_setdevicepropvalueex (PTPParams* params, unsigned char* data, unsigned int size);
#define ptp_canon_eos_setremotemode(params,p1) ptp_generic_no_data(params,PTP_OC_CANON_EOS_SetRemoteMode,1,p1)
#define ptp_canon_eos_seteventmode(params,p1) ptp_generic_no_data(params,PTP_OC_CANON_EOS_SetEventMode,1,p1)
/**
 * ptp_canon_eos_transfercomplete:
 * 
 * This ends a direct object transfer from an EOS camera.
 *
 * params:      PTPParams*
 *              oid             Object ID
 *
 * Return values: Some PTP_RC_* code.
 *
 */
#define ptp_canon_eos_transfercomplete(params,oid) ptp_generic_no_data(params,PTP_OC_CANON_EOS_TransferComplete,1,oid)
/* inHDD = %d, inLength =%d, inReset = %d */
#define ptp_canon_eos_pchddcapacity(params,p1,p2,p3) ptp_generic_no_data(params,PTP_OC_CANON_EOS_PCHDDCapacity,3,p1,p2,p3)
#define ptp_canon_eos_bulbstart(params) ptp_generic_no_data(params,PTP_OC_CANON_EOS_BulbStart,1)
#define ptp_canon_eos_bulbend(params) ptp_generic_no_data(params,PTP_OC_CANON_EOS_BulbEnd,1)
uint16_t ptp_canon_eos_getdevicepropdesc (PTPParams* params, uint16_t propcode,
				PTPDevicePropDesc *devicepropertydesc);
uint16_t ptp_canon_eos_setdevicepropvalue (PTPParams* params, uint16_t propcode,
                        	PTPPropertyValue* value, uint16_t datatype);
uint16_t ptp_nikon_get_vendorpropcodes (PTPParams* params, uint16_t **props, unsigned int *size);
uint16_t ptp_nikon_curve_download (PTPParams* params, 
				unsigned char **data, unsigned int *size);
uint16_t ptp_nikon_getptpipinfo (PTPParams* params, unsigned char **data, unsigned int *size);
uint16_t ptp_nikon_getwifiprofilelist (PTPParams* params);
uint16_t ptp_nikon_writewifiprofile (PTPParams* params, PTPNIKONWifiProfile* profile);
/**
 * ptp_nikon_deletewifiprofile:
 *
 * This command deletes a wifi profile.
 *  
 * params:      PTPParams*
 *      unsigned int profilenr  - profile number
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_nikon_deletewifiprofile(params,profilenr) ptp_generic_no_data(params,PTP_OC_NIKON_DeleteProfile,1,profilenr)
/**
 * ptp_nikon_setcontrolmode:
 *
 * This command can switch the camera to full PC control mode.
 *  
 * params:      PTPParams*
 *      uint32_t mode - mode
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_nikon_setcontrolmode(params,mode) ptp_generic_no_data(params,PTP_OC_NIKON_SetControlMode,1,mode)
/**
 * ptp_nikon_afdrive:
 *
 * This command runs (drives) the lens autofocus.
 *  
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_nikon_afdrive(params) ptp_generic_no_data(params,PTP_OC_NIKON_AfDrive,0)
/**
 * ptp_nikon_mfdrive:
 *
 * This command runs (drives) the lens autofocus.
 *  
 * params:      PTPParams*
 * flag:        0x1 for (no limit - closest), 0x2 for (closest - no limit)
 * amount:      amount of steps
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_nikon_mfdrive(params,flag,amount) ptp_generic_no_data(params,PTP_OC_NIKON_MfDrive,2,flag,amount)
/**
 * ptp_nikon_capture:
 *
 * This command captures a picture on the Nikon.
 *  
 * params:      PTPParams*
 *      uint32_t x - unknown parameter. seen to be -1.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_nikon_capture(params,x) ptp_generic_no_data(params,PTP_OC_NIKON_Capture,1,x)
/**
 * ptp_nikon_capture_sdram:
 *
 * This command captures a picture on the Nikon.
 *  
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_nikon_capture_sdram(params) ptp_generic_no_data(params,PTP_OC_NIKON_AfCaptureSDRAM,0)
/**
 * ptp_nikon_start_liveview:
 *
 * This command starts LiveView mode of newer Nikons DSLRs.
 *  
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_nikon_start_liveview(params) ptp_generic_no_data(params,PTP_OC_NIKON_StartLiveView,0)
uint16_t ptp_nikon_get_liveview_image (PTPParams* params, unsigned char**,unsigned int*);
uint16_t ptp_nikon_get_preview_image (PTPParams* params, unsigned char**, unsigned int*, uint32_t*);
/**
 * ptp_nikon_end_liveview:
 *
 * This command ends LiveView mode of newer Nikons DSLRs.
 *  
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_nikon_end_liveview(params) ptp_generic_no_data(params,PTP_OC_NIKON_EndLiveView,0)
uint16_t ptp_nikon_check_event (PTPParams* params, PTPContainer **evt, int *evtcnt);
uint16_t ptp_nikon_getfileinfoinblock (PTPParams* params, uint32_t p1, uint32_t p2, uint32_t p3,
					unsigned char **data, unsigned int *size);
/**
 * ptp_nikon_device_ready:
 *
 * This command checks if the device is ready. Used after
 * a capture.
 *  
 * params:      PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
#define ptp_nikon_device_ready(params) ptp_generic_no_data (params, PTP_OC_NIKON_DeviceReady, 0)
uint16_t ptp_mtp_getobjectpropssupported (PTPParams* params, uint16_t ofc, uint32_t *propnum, uint16_t **props);

/* Non PTP protocol functions */
int ptp_operation_issupported	(PTPParams* params, uint16_t operation);
int ptp_event_issupported	(PTPParams* params, uint16_t event);
int ptp_property_issupported	(PTPParams* params, uint16_t property);

void ptp_free_devicepropdesc	(PTPDevicePropDesc* dpd);
void ptp_free_devicepropvalue	(uint16_t dt, PTPPropertyValue* dpd);
void ptp_free_objectpropdesc	(PTPObjectPropDesc* dpd);
void ptp_free_params		(PTPParams *params);
void ptp_free_objectinfo	(PTPObjectInfo *oi);
void ptp_free_object		(PTPObject *oi);

void ptp_perror			(PTPParams* params, uint16_t error);

const char*
ptp_get_property_description(PTPParams* params, uint16_t dpc);

int
ptp_render_property_value(PTPParams* params, uint16_t dpc,
                          PTPDevicePropDesc *dpd, int length, char *out);
int ptp_render_ofc(PTPParams* params, uint16_t ofc, int spaceleft, char *txt);
int ptp_render_opcode(PTPParams* params, uint16_t opcode, int spaceleft, char *txt);
int ptp_render_mtp_propname(uint16_t propid, int spaceleft, char *txt);
MTPProperties *ptp_get_new_object_prop_entry(MTPProperties **props, int *nrofprops);
void ptp_destroy_object_prop(MTPProperties *prop);
void ptp_destroy_object_prop_list(MTPProperties *props, int nrofprops);
MTPProperties *ptp_find_object_prop_in_cache(PTPParams *params, uint32_t const handle, uint32_t const attribute_id);
void ptp_remove_object_from_cache(PTPParams *params, uint32_t handle);
uint16_t ptp_add_object_to_cache(PTPParams *params, uint32_t handle);
uint16_t ptp_object_want (PTPParams *, uint32_t handle, int want, PTPObject**retob);
void ptp_objects_sort (PTPParams *);
uint16_t ptp_object_find (PTPParams *params, uint32_t handle, PTPObject **retob);
uint16_t ptp_object_find_or_insert (PTPParams *params, uint32_t handle, PTPObject **retob);
/* ptpip.c */
void ptp_nikon_getptpipguid (unsigned char* guid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PTP_H__ */
