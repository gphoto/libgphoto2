/* ptp.c
 *
 * Copyright (C) 2001-2004 Mariusz Woloszyn <emsi@ipartners.pl>
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
#include "ptp.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

#define CHECK_PTP_RC(result)	{uint16_t r=(result); if (r!=PTP_RC_OK) return r;}

#define PTP_CNT_INIT(cnt) {memset(&cnt,0,sizeof(cnt));}

static void
ptp_debug (PTPParams *params, const char *format, ...)
{  
        va_list args;

        va_start (args, format);
        if (params->debug_func!=NULL)
                params->debug_func (params->data, format, args);
        else
	{
                vfprintf (stderr, format, args);
		fprintf (stderr,"\n");
		fflush (stderr);
	}
        va_end (args);
}  

static void
ptp_error (PTPParams *params, const char *format, ...)
{  
        va_list args;

        va_start (args, format);
        if (params->error_func!=NULL)
                params->error_func (params->data, format, args);
        else
	{
                vfprintf (stderr, format, args);
		fprintf (stderr,"\n");
		fflush (stderr);
	}
        va_end (args);
}

/* Pack / unpack functions */

#include "ptp-pack.c"

/* send / receive functions */

uint16_t
ptp_usb_sendreq (PTPParams* params, PTPContainer* req)
{
	uint16_t ret;
	PTPUSBBulkContainer usbreq;

	/* build appropriate USB container */
	usbreq.length=htod32(PTP_USB_BULK_REQ_LEN-
		(sizeof(uint32_t)*(5-req->Nparam)));
	usbreq.type=htod16(PTP_USB_CONTAINER_COMMAND);
	usbreq.code=htod16(req->Code);
	usbreq.trans_id=htod32(req->Transaction_ID);
	usbreq.payload.params.param1=htod32(req->Param1);
	usbreq.payload.params.param2=htod32(req->Param2);
	usbreq.payload.params.param3=htod32(req->Param3);
	usbreq.payload.params.param4=htod32(req->Param4);
	usbreq.payload.params.param5=htod32(req->Param5);
	/* send it to responder */
	ret=params->write_func((unsigned char *)&usbreq,
		PTP_USB_BULK_REQ_LEN-(sizeof(uint32_t)*(5-req->Nparam)),
		params->data);
	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
/*		ptp_error (params,
			"PTP: request code 0x%04x sending req error 0x%04x",
			req->Code,ret); */
	}
	return ret;
}

uint16_t
ptp_usb_senddata (PTPParams* params, PTPContainer* ptp,
			unsigned char *data, unsigned int size)
{
	uint16_t ret;
	PTPUSBBulkContainer usbdata;

	/* build appropriate USB container */
	usbdata.length=htod32(PTP_USB_BULK_HDR_LEN+size);
	usbdata.type=htod16(PTP_USB_CONTAINER_DATA);
	usbdata.code=htod16(ptp->Code);
	usbdata.trans_id=htod32(ptp->Transaction_ID);
	memcpy(usbdata.payload.data,data,
		(size<PTP_USB_BULK_PAYLOAD_LEN)?size:PTP_USB_BULK_PAYLOAD_LEN);
	/* send first part of data */
	ret=params->write_func((unsigned char *)&usbdata, PTP_USB_BULK_HDR_LEN+
		((size<PTP_USB_BULK_PAYLOAD_LEN)?size:PTP_USB_BULK_PAYLOAD_LEN),
		params->data);
	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
/*		ptp_error (params,
		"PTP: request code 0x%04x sending data error 0x%04x",
			ptp->Code,ret);*/
		return ret;
	}
	if (size<=PTP_USB_BULK_PAYLOAD_LEN) return ret;
	/* if everything OK send the rest */
	ret=params->write_func (data+PTP_USB_BULK_PAYLOAD_LEN,
				size-PTP_USB_BULK_PAYLOAD_LEN, params->data);
	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
/*		ptp_error (params,
		"PTP: request code 0x%04x sending data error 0x%04x",
			ptp->Code,ret); */
	}
	return ret;
}

uint16_t
ptp_usb_getdata (PTPParams* params, PTPContainer* ptp,
		unsigned char **data, unsigned int *readlen)
{
	uint16_t ret;
	PTPUSBBulkContainer usbdata;

	PTP_CNT_INIT(usbdata);
	if (*data!=NULL) return PTP_ERROR_BADPARAM;
	do {
		unsigned int len, rlen;
		/* read first(?) part of data */
		ret=params->read_func((unsigned char *)&usbdata,
				sizeof(usbdata), params->data, &len);
		if (ret!=PTP_RC_OK) {
			ret = PTP_ERROR_IO;
			break;
		} else
		if (dtoh16(usbdata.type)!=PTP_USB_CONTAINER_DATA) {
			ret = PTP_ERROR_DATA_EXPECTED;
			break;
		} else
		if (dtoh16(usbdata.code)!=ptp->Code) {
			ret = dtoh16(usbdata.code);
			break;
		}
		/* evaluate data length */
		len=dtoh32(usbdata.length)-PTP_USB_BULK_HDR_LEN;
		/* allocate memory for data */
		*data=calloc(len,1);
		/* copy first part of data to 'data' */
		memcpy(*data,usbdata.payload.data,
			PTP_USB_BULK_PAYLOAD_LEN<len?
			PTP_USB_BULK_PAYLOAD_LEN:len);
		/* is that all of data? */
		if (len+PTP_USB_BULK_HDR_LEN<=sizeof(usbdata)) break;
		/* if not finaly read the rest of it */
		ret=params->read_func(((unsigned char *)(*data))+
					PTP_USB_BULK_PAYLOAD_LEN,
					len-PTP_USB_BULK_PAYLOAD_LEN,
					params->data, &rlen);
		if (ret!=PTP_RC_OK) {
			ret = PTP_ERROR_IO;
			break;
		}
	} while (0);
/*
	if (ret!=PTP_RC_OK) {
		ptp_error (params,
		"PTP: request code 0x%04x getting data error 0x%04x",
			ptp->Code, ret);
	}*/
	return ret;
}

uint16_t
ptp_usb_getresp (PTPParams* params, PTPContainer* resp)
{
	uint16_t ret;
	unsigned int rlen;
	PTPUSBBulkContainer usbresp;

	PTP_CNT_INIT(usbresp);
	/* read response, it should never be longer than sizeof(usbresp) */
	ret=params->read_func((unsigned char *)&usbresp,
				sizeof(usbresp), params->data, &rlen);

	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
	} else
	if (dtoh16(usbresp.type)!=PTP_USB_CONTAINER_RESPONSE) {
		ret = PTP_ERROR_RESP_EXPECTED;
	} else
	if (dtoh16(usbresp.code)!=resp->Code) {
		ret = dtoh16(usbresp.code);
	}
	if (ret!=PTP_RC_OK) {
/*		ptp_error (params,
		"PTP: request code 0x%04x getting resp error 0x%04x",
			resp->Code, ret);*/
		return ret;
	}
	/* build an appropriate PTPContainer */
	resp->Code=dtoh16(usbresp.code);
	resp->SessionID=params->session_id;
	resp->Transaction_ID=dtoh32(usbresp.trans_id);
	resp->Param1=dtoh32(usbresp.payload.params.param1);
	resp->Param2=dtoh32(usbresp.payload.params.param2);
	resp->Param3=dtoh32(usbresp.payload.params.param3);
	resp->Param4=dtoh32(usbresp.payload.params.param4);
	resp->Param5=dtoh32(usbresp.payload.params.param5);
	return ret;
}

/* major PTP functions */

/* Transaction data phase description */
#define PTP_DP_NODATA		0x0000	/* no data phase */
#define PTP_DP_SENDDATA		0x0001	/* sending data */
#define PTP_DP_GETDATA		0x0002	/* receiving data */
#define PTP_DP_DATA_MASK	0x00ff	/* data phase mask */

/* Number of PTP Request phase parameters */
#define PTP_RQ_PARAM0		0x0000	/* zero parameters */
#define PTP_RQ_PARAM1		0x0100	/* one parameter */
#define PTP_RQ_PARAM2		0x0200	/* two parameters */
#define PTP_RQ_PARAM3		0x0300	/* three parameters */
#define PTP_RQ_PARAM4		0x0400	/* four parameters */
#define PTP_RQ_PARAM5		0x0500	/* five parameters */

/**
 * ptp_transaction:
 * params:	PTPParams*
 * 		PTPContainer* ptp	- general ptp container
 * 		uint16_t flags		- lower 8 bits - data phase description
 * 		unsigned int sendlen	- senddata phase data length
 * 		char** data		- send or receive data buffer pointer
 * 		int* recvlen		- receive data length
 *
 * Performs PTP transaction. ptp is a PTPContainer with appropriate fields
 * filled in (i.e. operation code and parameters). It's up to caller to do
 * so.
 * The flags decide thether the transaction has a data phase and what is its
 * direction (send or receive). 
 * If transaction is sending data the sendlen should contain its length in
 * bytes, otherwise it's ignored.
 * The data should contain an address of a pointer to data going to be sent
 * or is filled with such a pointer address if data are received depending
 * od dataphase direction (send or received) or is beeing ignored (no
 * dataphase).
 * The memory for a pointer should be preserved by the caller, if data are
 * beeing retreived the appropriate amount of memory is beeing allocated
 * (the caller should handle that!).
 *
 * Return values: Some PTP_RC_* code.
 * Upon success PTPContainer* ptp contains PTP Response Phase container with
 * all fields filled in.
 **/
static uint16_t
ptp_transaction (PTPParams* params, PTPContainer* ptp, 
		uint16_t flags, unsigned int sendlen, char** data,
		unsigned int *recvlen)
{
	if ((params==NULL) || (ptp==NULL)) 
		return PTP_ERROR_BADPARAM;
	
	ptp->Transaction_ID=params->transaction_id++;
	ptp->SessionID=params->session_id;
	/* send request */
	CHECK_PTP_RC(params->sendreq_func (params, ptp));
	/* is there a dataphase? */
	switch (flags&PTP_DP_DATA_MASK) {
		case PTP_DP_SENDDATA:
			CHECK_PTP_RC(params->senddata_func(params, ptp,
				*data, sendlen));
			break;
		case PTP_DP_GETDATA:
			CHECK_PTP_RC(params->getdata_func(params, ptp,
				(unsigned char**)data, recvlen));
			break;
		case PTP_DP_NODATA:
			break;
		default:
		return PTP_ERROR_BADPARAM;
	}
	/* get response */
	CHECK_PTP_RC(params->getresp_func(params, ptp));
	return PTP_RC_OK;
}

/* Enets handling functions */

/* PTP Events wait for or check mode */
#define PTP_EVENT_CHECK			0x0000	/* waits for */
#define PTP_EVENT_CHECK_FAST		0x0001	/* checks */

static inline uint16_t
ptp_usb_event (PTPParams* params, PTPContainer* event, int wait)
{
	uint16_t ret;
	unsigned int rlen;
	PTPUSBEventContainer usbevent;
	PTP_CNT_INIT(usbevent);

	if ((params==NULL) || (event==NULL)) 
		return PTP_ERROR_BADPARAM;
	
	switch(wait) {
		case PTP_EVENT_CHECK:
			ret=params->check_int_func((unsigned char*)&usbevent,
				sizeof(usbevent), params->data, &rlen);
			break;
		case PTP_EVENT_CHECK_FAST:
			ret=params->check_int_fast_func((unsigned char*)
				&usbevent, sizeof(usbevent), params->data, &rlen);
			break;
		default:
			ret=PTP_ERROR_BADPARAM;
	}
	if (ret!=PTP_RC_OK) {
		ptp_error (params,
			"PTP: reading event an error 0x%04x occured", ret);
		ret = PTP_ERROR_IO;
		/* reading event error is nonfatal (for example timeout) */
	} 
	/* if we read anything over interrupt endpoint it must be an event */
	/* build an appropriate PTPContainer */
	event->Code=dtoh16(usbevent.code);
	event->SessionID=params->session_id;
	event->Transaction_ID=dtoh32(usbevent.trans_id);
	event->Param1=dtoh32(usbevent.param1);
	event->Param2=dtoh32(usbevent.param2);
	event->Param3=dtoh32(usbevent.param3);

	return ret;
}

uint16_t
ptp_usb_event_check (PTPParams* params, PTPContainer* event) {

	return ptp_usb_event (params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_usb_event_wait (PTPParams* params, PTPContainer* event) {

	return ptp_usb_event (params, event, PTP_EVENT_CHECK);
}

/**
 * PTP operation functions
 *
 * all ptp_ functions should take integer parameters
 * in host byte order!
 **/


/**
 * ptp_getdeviceinfo:
 * params:	PTPParams*
 *
 * Gets device info dataset and fills deviceinfo structure.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getdeviceinfo (PTPParams* params, PTPDeviceInfo* deviceinfo)
{
	uint16_t ret;
	int len;
	PTPContainer ptp;
	char* di=NULL;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetDeviceInfo;
	ptp.Nparam=0;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &di, &len);
	if (ret == PTP_RC_OK) ptp_unpack_DI(params, di, deviceinfo, len);
	free(di);
	return ret;
}


/**
 * ptp_opensession:
 * params:	PTPParams*
 * 		session			- session number 
 *
 * Establishes a new session.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_opensession (PTPParams* params, uint32_t session)
{
	uint16_t ret;
	PTPContainer ptp;

	ptp_debug(params,"PTP: Opening session");

	/* SessonID field of the operation dataset should always
	   be set to 0 for OpenSession request! */
	params->session_id=0x00000000;
	/* TransactionID should be set to 0 also! */
	params->transaction_id=0x0000000;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_OpenSession;
	ptp.Param1=session;
	ptp.Nparam=1;
	ret=ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
	/* now set the global session id to current session number */
	params->session_id=session;
	return ret;
}

/**
 * ptp_closesession:
 * params:	PTPParams*
 *
 * Closes session.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_closesession (PTPParams* params)
{
	PTPContainer ptp;

	ptp_debug(params,"PTP: Closing session");

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CloseSession;
	ptp.Nparam=0;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_getststorageids:
 * params:	PTPParams*
 *
 * Gets array of StorageIDs and fills the storageids structure.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getstorageids (PTPParams* params, PTPStorageIDs* storageids)
{
	uint16_t ret;
	PTPContainer ptp;
	int len;
	char* sids=NULL;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetStorageIDs;
	ptp.Nparam=0;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &sids, &len);
	if (ret == PTP_RC_OK) ptp_unpack_SIDs(params, sids, storageids, len);
	free(sids);
	return ret;
}

/**
 * ptp_getststorageinfo:
 * params:	PTPParams*
 *		storageid		- StorageID
 *
 * Gets StorageInfo dataset of desired storage and fills storageinfo
 * structure.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getstorageinfo (PTPParams* params, uint32_t storageid,
			PTPStorageInfo* storageinfo)
{
	uint16_t ret;
	PTPContainer ptp;
	char* si=NULL;
	int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetStorageInfo;
	ptp.Param1=storageid;
	ptp.Nparam=1;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &si, &len);
	if (ret == PTP_RC_OK) ptp_unpack_SI(params, si, storageinfo, len);
	free(si);
	return ret;
}

/**
 * ptp_getobjecthandles:
 * params:	PTPParams*
 *		storage			- StorageID
 *		objectformatcode	- ObjectFormatCode (optional)
 *		associationOH		- ObjectHandle of Association for
 *					  wich a list of children is desired
 *					  (optional)
 *		objecthandles		- pointer to structute
 *
 * Fills objecthandles with structure returned by device.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobjecthandles (PTPParams* params, uint32_t storage,
			uint32_t objectformatcode, uint32_t associationOH,
			PTPObjectHandles* objecthandles)
{
	uint16_t ret;
	PTPContainer ptp;
	char* oh=NULL;
	int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObjectHandles;
	ptp.Param1=storage;
	ptp.Param2=objectformatcode;
	ptp.Param3=associationOH;
	ptp.Nparam=3;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &oh, &len);
	if (ret == PTP_RC_OK) ptp_unpack_OH(params, oh, objecthandles, len);
	free(oh);
	return ret;
}

/**
 * ptp_getobjectinfo:
 * params:	PTPParams*
 *		handle			- Object handle
 *		objectinfo		- pointer to objectinfo that is returned
 *
 * Get objectinfo structure for handle from device.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobjectinfo (PTPParams* params, uint32_t handle,
			PTPObjectInfo* objectinfo)
{
	uint16_t ret;
	PTPContainer ptp;
	char* oi=NULL;
	int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObjectInfo;
	ptp.Param1=handle;
	ptp.Nparam=1;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &oi, &len);
	if (ret == PTP_RC_OK) ptp_unpack_OI(params, oi, objectinfo, len);
	free(oi);
	return ret;
}

/**
 * ptp_getobject:
 * params:	PTPParams*
 *		handle			- Object handle
 *		object			- pointer to data area
 *
 * Get object 'handle' from device and store the data in newly
 * allocated 'object'.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobject (PTPParams* params, uint32_t handle, char** object)
{
	PTPContainer ptp;
	int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObject;
	ptp.Param1=handle;
	ptp.Nparam=1;
	len=0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, &len);
}

/**
 * ptp_getpartialobject:
 * params:	PTPParams*
 *		handle			- Object handle
 *		offset			- Offset into object
 *		maxbytes		- Maximum of bytes to read
 *		object			- pointer to data area
 *
 * Get object 'handle' from device and store the data in newly
 * allocated 'object'. Start from offset and read at most maxbytes.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getpartialobject (PTPParams* params, uint32_t handle, uint32_t offset,
			uint32_t maxbytes, char** object)
{
	PTPContainer ptp;
	int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetPartialObject;
	ptp.Param1=handle;
	ptp.Param2=offset;
	ptp.Param3=maxbytes;
	ptp.Nparam=3;
	len=0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, &len);
}

/**
 * ptp_getthumb:
 * params:	PTPParams*
 *		handle			- Object handle
 *		object			- pointer to data area
 *
 * Get thumb for object 'handle' from device and store the data in newly
 * allocated 'object'.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getthumb (PTPParams* params, uint32_t handle,  char** object)
{
	PTPContainer ptp;
	int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetThumb;
	ptp.Param1=handle;
	ptp.Nparam=1;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, &len);
}

/**
 * ptp_deleteobject:
 * params:	PTPParams*
 *		handle			- object handle
 *		ofc			- object format code (optional)
 * 
 * Deletes desired objects.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_deleteobject (PTPParams* params, uint32_t handle, uint32_t ofc)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_DeleteObject;
	ptp.Param1=handle;
	ptp.Param2=ofc;
	ptp.Nparam=2;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_sendobjectinfo:
 * params:	PTPParams*
 *		uint32_t* store		- destination StorageID on Responder
 *		uint32_t* parenthandle 	- Parent ObjectHandle on responder
 * 		uint32_t* handle	- see Return values
 *		PTPObjectInfo* objectinfo- ObjectInfo that is to be sent
 * 
 * Sends ObjectInfo of file that is to be sent via SendFileObject.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : uint32_t* store	- Responder StorageID in which
 *					  object will be stored
 *		  uint32_t* parenthandle- Responder Parent ObjectHandle
 *					  in which the object will be stored
 *		  uint32_t* handle	- Responder's reserved ObjectHandle
 *					  for the incoming object
 **/
uint16_t
ptp_sendobjectinfo (PTPParams* params, uint32_t* store, 
			uint32_t* parenthandle, uint32_t* handle,
			PTPObjectInfo* objectinfo)
{
	uint16_t ret;
	PTPContainer ptp;
	char* oidata=NULL;
	uint32_t size;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SendObjectInfo;
	ptp.Param1=*store;
	ptp.Param2=*parenthandle;
	ptp.Nparam=2;
	
	size=ptp_pack_OI(params, objectinfo, &oidata);
	ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &oidata, NULL); 
	free(oidata);
	*store=ptp.Param1;
	*parenthandle=ptp.Param2;
	*handle=ptp.Param3; 
	return ret;
}

/**
 * ptp_sendobject:
 * params:	PTPParams*
 *		char*	object		- contains the object that is to be sent
 *		uint32_t size		- object size
 *		
 * Sends object to Responder.
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_sendobject (PTPParams* params, char* object, uint32_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SendObject;
	ptp.Nparam=0;

	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &object, NULL);
}


/**
 * ptp_initiatecapture:
 * params:	PTPParams*
 *		storageid		- destination StorageID on Responder
 *		ofc			- object format code
 * 
 * Causes device to initiate the capture of one or more new data objects
 * according to its current device properties, storing the data into store
 * indicated by storageid. If storageid is 0x00000000, the object(s) will
 * be stored in a store that is determined by the capturing device.
 * The capturing of new data objects is an asynchronous operation.
 *
 * Return values: Some PTP_RC_* code.
 **/

uint16_t
ptp_initiatecapture (PTPParams* params, uint32_t storageid,
			uint32_t ofc)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_InitiateCapture;
	ptp.Param1=storageid;
	ptp.Param2=ofc;
	ptp.Nparam=2;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

uint16_t
ptp_getdevicepropdesc (PTPParams* params, uint16_t propcode, 
			PTPDevicePropDesc* devicepropertydesc)
{
	PTPContainer ptp;
	uint16_t ret;
	int len;
	char* dpd=NULL;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetDevicePropDesc;
	ptp.Param1=propcode;
	ptp.Nparam=1;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dpd, &len);
	if (ret == PTP_RC_OK) ptp_unpack_DPD(params, dpd, devicepropertydesc, len);
	free(dpd);
	return ret;
}


uint16_t
ptp_getdevicepropvalue (PTPParams* params, uint16_t propcode,
			PTPPropertyValue* value, uint16_t datatype)
{
	PTPContainer ptp;
	uint16_t ret;
	int len, offset;
	char* dpv=NULL;


	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetDevicePropValue;
	ptp.Param1=propcode;
	ptp.Nparam=1;
	len=offset=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dpv, &len);
	if (ret == PTP_RC_OK) ptp_unpack_DPV(params, dpv, &offset, len, value, datatype);
	free(dpv);
	return ret;
}

uint16_t
ptp_setdevicepropvalue (PTPParams* params, uint16_t propcode,
			PTPPropertyValue *value, uint16_t datatype)
{
	PTPContainer ptp;
	uint16_t ret;
	uint32_t size;
	char* dpv=NULL;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SetDevicePropValue;
	ptp.Param1=propcode;
	ptp.Nparam=1;
	size=ptp_pack_DPV(params, value, &dpv, datatype);
	ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &dpv, NULL);
	free(dpv);
	return ret;
}

/**
 * ptp_ek_sendfileobjectinfo:
 * params:	PTPParams*
 *		uint32_t* store		- destination StorageID on Responder
 *		uint32_t* parenthandle 	- Parent ObjectHandle on responder
 * 		uint32_t* handle	- see Return values
 *		PTPObjectInfo* objectinfo- ObjectInfo that is to be sent
 * 
 * Sends ObjectInfo of file that is to be sent via SendFileObject.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : uint32_t* store	- Responder StorageID in which
 *					  object will be stored
 *		  uint32_t* parenthandle- Responder Parent ObjectHandle
 *					  in which the object will be stored
 *		  uint32_t* handle	- Responder's reserved ObjectHandle
 *					  for the incoming object
 **/
uint16_t
ptp_ek_sendfileobjectinfo (PTPParams* params, uint32_t* store, 
			uint32_t* parenthandle, uint32_t* handle,
			PTPObjectInfo* objectinfo)
{
	uint16_t ret;
	PTPContainer ptp;
	char* oidata=NULL;
	uint32_t size;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_EK_SendFileObjectInfo;
	ptp.Param1=*store;
	ptp.Param2=*parenthandle;
	ptp.Nparam=2;
	
	size=ptp_pack_OI(params, objectinfo, &oidata);
	ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &oidata, NULL); 
	free(oidata);
	*store=ptp.Param1;
	*parenthandle=ptp.Param2;
	*handle=ptp.Param3; 
	return ret;
}

/**
 * ptp_ek_sendfileobject:
 * params:	PTPParams*
 *		char*	object		- contains the object that is to be sent
 *		uint32_t size		- object size
 *		
 * Sends object to Responder.
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_ek_sendfileobject (PTPParams* params, char* object, uint32_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_EK_SendFileObject;
	ptp.Nparam=0;

	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &object, NULL);
}

/*************************************************************************
 *
 * Canon PTP extensions support
 *
 * (C) Nikolai Kopanygin 2003
 *
 *************************************************************************/


/**
 * ptp_canon_getobjectsize:
 * params:	PTPParams*
 *		uint32_t handle		- ObjectHandle
 *		uint32_t p2 		- Yet unknown parameter,
 *					  value 0 works.
 * 
 * Gets form the responder the size of the specified object.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : uint32_t* size	- The object size
 *		  uint32_t  rp2		- Yet unknown parameter
 *
 **/
uint16_t
ptp_canon_getobjectsize (PTPParams* params, uint32_t handle, uint32_t p2, 
			uint32_t* size, uint32_t* rp2) 
{
	uint16_t ret;
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetObjectSize;
	ptp.Param1=handle;
	ptp.Param2=p2;
	ptp.Nparam=2;
	ret=ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
	*size=ptp.Param1;
	*rp2=ptp.Param2;
	return ret;
}

/**
 * ptp_canon_startshootingmode:
 * params:	PTPParams*
 * 
 * Starts shooting session. It emits a StorageInfoChanged
 * event via the interrupt pipe and pushes the StorageInfoChanged
 * and CANON_CameraModeChange events onto the event stack
 * (see operation PTP_OC_CANON_CheckEvent).
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_startshootingmode (PTPParams* params)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_StartShootingMode;
	ptp.Nparam=0;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_canon_endshootingmode:
 * params:	PTPParams*
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
uint16_t
ptp_canon_endshootingmode (PTPParams* params)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_EndShootingMode;
	ptp.Nparam=0;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_canon_viewfinderon:
 * params:	PTPParams*
 * 
 * Prior to start reading viewfinder images, one  must call this operation.
 * Supposedly, this operation affects the value of the CANON_ViewfinderMode
 * property.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_viewfinderon (PTPParams* params)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_ViewfinderOn;
	ptp.Nparam=0;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_canon_viewfinderoff:
 * params:	PTPParams*
 * 
 * Before changing the shooting mode, or when one doesn't need to read
 * viewfinder images any more, one must call this operation.
 * Supposedly, this operation affects the value of the CANON_ViewfinderMode
 * property.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_viewfinderoff (PTPParams* params)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_ViewfinderOff;
	ptp.Nparam=0;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_canon_reflectchanges:
 * params:	PTPParams*
 * 		uint32_t p1 	- Yet unknown parameter,
 * 				  value 7 works
 * 
 * Make viewfinder reflect changes.
 * There is a button for this operation in the Remote Capture app.
 * What it does exactly I don't know. This operation is followed
 * by the CANON_GetChanges(?) operation in the log.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_reflectchanges (PTPParams* params, uint32_t p1)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_ReflectChanges;
	ptp.Param1=p1;
	ptp.Nparam=1;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}


/**
 * ptp_canon_checkevent:
 * params:	PTPParams*
 * 
 * The camera has a FIFO stack, in which it accumulates events.
 * Partially these events are communicated also via the USB interrupt pipe
 * according to the PTP USB specification, partially not.
 * This operation returns from the device a block of data, empty,
 * if the event stack is empty, or filled with an event's data otherwise.
 * The event is removed from the stack in the latter case.
 * The Remote Capture app sends this command to the camera all the time
 * of connection, filling with it the gaps between other operations. 
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : PTPUSBEventContainer* event	- is filled with the event data
 *						  if any
 *                int *isevent			- returns 1 in case of event
 *						  or 0 otherwise
 **/
uint16_t
ptp_canon_checkevent (PTPParams* params, PTPUSBEventContainer* event, int* isevent)
{
	uint16_t ret;
	PTPContainer ptp;
	char *evdata = NULL;
	int len;
	
	*isevent=0;
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_CheckEvent;
	ptp.Nparam=0;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &evdata, &len);
	if (evdata!=NULL) {
		if (ret == PTP_RC_OK) {
        		ptp_unpack_EC(params, evdata, event, len);
    			*isevent=1;
        	}
		free(evdata);
	}
	return ret;
}


/**
 * ptp_canon_focuslock:
 *
 * This operation locks the focus. It is followed by the CANON_GetChanges(?)
 * operation in the log. 
 * It affects the CANON_MacroMode property. 
 *
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_focuslock (PTPParams* params)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_FocusLock;
	ptp.Nparam=0;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_canon_focusunlock:
 *
 * This operation unlocks the focus. It is followed by the CANON_GetChanges(?)
 * operation in the log. 
 * It sets the CANON_MacroMode property value to 1 (where it occurs in the log).
 * 
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_focusunlock (PTPParams* params)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_FocusUnlock;
	ptp.Nparam=0;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

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
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_initiatecaptureinmemory (PTPParams* params)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_InitiateCaptureInMemory;
	ptp.Nparam=0;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_canon_getpartialobject:
 *
 * This operation is used to read from the device a data 
 * block of an object from a specified offset.
 *
 * params:	PTPParams*
 *      uint32_t handle - the handle of the requested object
 *      uint32_t offset - the offset in bytes from the beginning of the object
 *      uint32_t size - the requested size of data block to read
 *      uint32_t pos - 1 for the first block, 2 - for a block in the middle,
 *                  3 - for the last block
 *
 * Return values: Some PTP_RC_* code.
 *      char **block - the pointer to the block of data read
 *      uint32_t* readnum - the number of bytes read
 *
 **/
uint16_t
ptp_canon_getpartialobject (PTPParams* params, uint32_t handle, 
				uint32_t offset, uint32_t size,
				uint32_t pos, char** block, 
				uint32_t* readnum)
{
	uint16_t ret;
	PTPContainer ptp;
	char *data=NULL;
	int len;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetPartialObject;
	ptp.Param1=handle;
	ptp.Param2=offset;
	ptp.Param3=size;
	ptp.Param4=pos;
	ptp.Nparam=4;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &len);
	if (ret==PTP_RC_OK) {
		*block=data;
		*readnum=ptp.Param1;
	}
	return ret;
}

/**
 * ptp_canon_getviewfinderimage:
 *
 * This operation can be used to read the image which is currently
 * in the camera's viewfinder. The image size is 320x240, format is JPEG.
 * Of course, prior to calling this operation, one must turn the viewfinder
 * on with the CANON_ViewfinderOn command.
 * Invoking this operation many times, one can get live video from the camera!
 * 
 * params:	PTPParams*
 * 
 * Return values: Some PTP_RC_* code.
 *      char **image - the pointer to the read image
 *      unit32_t *size - the size of the image in bytes
 *
 **/
uint16_t
ptp_canon_getviewfinderimage (PTPParams* params, char** image, uint32_t* size)
{
	uint16_t ret;
	PTPContainer ptp;
	int len;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetViewfinderImage;
	ptp.Nparam=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, image, &len);
	if (ret==PTP_RC_OK) *size=ptp.Param1;
	return ret;
}

/**
 * ptp_canon_getchanges:
 *
 * This is an interesting operation, about the effect of which I am not sure.
 * This command is called every time when a device property has been changed 
 * with the SetDevicePropValue operation, and after some other operations.
 * This operation reads the array of Device Properties which have been changed
 * by the previous operation.
 * Probably, this operation is even required to make those changes work.
 *
 * params:	PTPParams*
 * 
 * Return values: Some PTP_RC_* code.
 *      uint16_t** props - the pointer to the array of changed properties
 *      uint32_t* propnum - the number of elements in the *props array
 *
 **/
uint16_t
ptp_canon_getchanges (PTPParams* params, uint16_t** props, uint32_t* propnum)
{
	uint16_t ret;
	PTPContainer ptp;
	char* data=NULL;
	int len;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetChanges;
	ptp.Nparam=0;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &len);
	if (ret == PTP_RC_OK)
        	*propnum=ptp_unpack_uint16_t_array(params,data,0,props);
	free(data);
	return ret;
}

/**
 * ptp_canon_getfolderentries:
 *
 * This command reads a specified object's record in a device's filesystem,
 * or the records of all objects belonging to a specified folder (association).
 *  
 * params:	PTPParams*
 *      uint32_t store - StorageID,
 *      uint32_t p2 - Yet unknown (0 value works OK)
 *      uint32_t parent - Parent Object Handle
 *                      # If Parent Object Handle is 0xffffffff, 
 *                      # the Parent Object is the top level folder.
 *      uint32_t handle - Object Handle
 *                      # If Object Handle is 0, the records of all objects 
 *                      # belonging to the Parent Object are read.
 *                      # If Object Handle is not 0, only the record of this 
 *                      # Object is read.
 *
 * Return values: Some PTP_RC_* code.
 *      PTPCANONFolderEntry** entries - the pointer to the folder entry array
 *      uint32_t* entnum - the number of elements of the array
 *
 **/
uint16_t
ptp_canon_getfolderentries (PTPParams* params, uint32_t store, uint32_t p2, 
			    uint32_t parent, uint32_t handle, 
			    PTPCANONFolderEntry** entries, uint32_t* entnum)
{
	uint16_t ret;
	PTPContainer ptp;
	char *data = NULL;
	int len;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetFolderEntries;
	ptp.Param1=store;
	ptp.Param2=p2;
	ptp.Param3=parent;
	ptp.Param4=handle;
	ptp.Nparam=4;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &len);
	if (ret == PTP_RC_OK) {
		int i;
		*entnum=ptp.Param1;
		*entries=calloc(*entnum, sizeof(PTPCANONFolderEntry));
		if (*entries!=NULL) {
			for(i=0; i<(*entnum); i++)
				ptp_unpack_Canon_FE(params,
					data+i*PTP_CANON_FolderEntryLen,
					&((*entries)[i]) );
		} else {
			ret=PTP_ERROR_IO; /* Cannot allocate memory */
		}
	}
	free(data);
	return ret;
}


/* Non PTP protocol functions */
/* devinfo testing functions */

int
ptp_operation_issupported(PTPParams* params, uint16_t operation)
{
	int i=0;

	for (;i<params->deviceinfo.OperationsSupported_len;i++) {
		if (params->deviceinfo.OperationsSupported[i]==operation)
			return 1;
	}
	return 0;
}


int
ptp_event_issupported(PTPParams* params, uint16_t event)
{
	int i=0;

	for (;i<params->deviceinfo.EventsSupported_len;i++) {
		if (params->deviceinfo.EventsSupported[i]==event)
			return 1;
	}
	return 0;
}


int
ptp_property_issupported(PTPParams* params, uint16_t property)
{
	int i=0;

	for (;i<params->deviceinfo.DevicePropertiesSupported_len;i++)
		if (params->deviceinfo.DevicePropertiesSupported[i]==property)
			return 1;
	return 0;
}

/* ptp structures freeing functions */
void
ptp_free_devicepropvalue(uint16_t dt, PTPPropertyValue* dpd) {
	switch (dt) {
	case PTP_DTC_INT8:	case PTP_DTC_UINT8:
	case PTP_DTC_UINT16:	case PTP_DTC_INT16:
	case PTP_DTC_UINT32:	case PTP_DTC_INT32:
	case PTP_DTC_UINT64:	case PTP_DTC_INT64:
	case PTP_DTC_UINT128:	case PTP_DTC_INT128:
		/* Nothing to free */
		break;
	case PTP_DTC_AINT8:	case PTP_DTC_AUINT8:
	case PTP_DTC_AUINT16:	case PTP_DTC_AINT16:
	case PTP_DTC_AUINT32:	case PTP_DTC_AINT32:
	case PTP_DTC_AUINT64:	case PTP_DTC_AINT64:
	case PTP_DTC_AUINT128:	case PTP_DTC_AINT128:
		if (dpd->a.v)
			free(dpd->a.v);
		break;
	case PTP_DTC_STR:
		if (dpd->str)
			free(dpd->str);
		break;
	}
}

void
ptp_free_devicepropdesc(PTPDevicePropDesc* dpd)
{
	uint16_t i;

	ptp_free_devicepropvalue (dpd->DataType, &dpd->FactoryDefaultValue);
	ptp_free_devicepropvalue (dpd->DataType, &dpd->CurrentValue);
	switch (dpd->FormFlag) {
	case PTP_DPFF_Range:
		ptp_free_devicepropvalue (dpd->DataType, &dpd->FORM.Range.MinimumValue);
		ptp_free_devicepropvalue (dpd->DataType, &dpd->FORM.Range.MaximumValue);
		ptp_free_devicepropvalue (dpd->DataType, &dpd->FORM.Range.StepSize);
		break;
	case PTP_DPFF_Enumeration:
		if (dpd->FORM.Enum.SupportedValue) {
			for (i=0;i<dpd->FORM.Enum.NumberOfValues;i++)
				ptp_free_devicepropvalue (dpd->DataType, dpd->FORM.Enum.SupportedValue+i);
			free (dpd->FORM.Enum.SupportedValue);
		}
	}
}

void 
ptp_perror(PTPParams* params, uint16_t error) {

	int i;
	/* PTP error descriptions */
	static struct {
		short n;
		const char *txt;
	} ptp_errors[] = {
	{PTP_RC_Undefined, 		N_("PTP: Undefined Error")},
	{PTP_RC_OK, 			N_("PTP: OK!")},
	{PTP_RC_GeneralError, 		N_("PTP: General Error")},
	{PTP_RC_SessionNotOpen, 	N_("PTP: Session Not Open")},
	{PTP_RC_InvalidTransactionID, 	N_("PTP: Invalid Transaction ID")},
	{PTP_RC_OperationNotSupported, 	N_("PTP: Operation Not Supported")},
	{PTP_RC_ParameterNotSupported, 	N_("PTP: Parameter Not Supported")},
	{PTP_RC_IncompleteTransfer, 	N_("PTP: Incomplete Transfer")},
	{PTP_RC_InvalidStorageId, 	N_("PTP: Invalid Storage ID")},
	{PTP_RC_InvalidObjectHandle, 	N_("PTP: Invalid Object Handle")},
	{PTP_RC_DevicePropNotSupported, N_("PTP: Device Prop Not Supported")},
	{PTP_RC_InvalidObjectFormatCode, N_("PTP: Invalid Object Format Code")},
	{PTP_RC_StoreFull, 		N_("PTP: Store Full")},
	{PTP_RC_ObjectWriteProtected, 	N_("PTP: Object Write Protected")},
	{PTP_RC_StoreReadOnly, 		N_("PTP: Store Read Only")},
	{PTP_RC_AccessDenied,		N_("PTP: Access Denied")},
	{PTP_RC_NoThumbnailPresent, 	N_("PTP: No Thumbnail Present")},
	{PTP_RC_SelfTestFailed, 	N_("PTP: Self Test Failed")},
	{PTP_RC_PartialDeletion, 	N_("PTP: Partial Deletion")},
	{PTP_RC_StoreNotAvailable, 	N_("PTP: Store Not Available")},
	{PTP_RC_SpecificationByFormatUnsupported,
				N_("PTP: Specification By Format Unsupported")},
	{PTP_RC_NoValidObjectInfo, 	N_("PTP: No Valid Object Info")},
	{PTP_RC_InvalidCodeFormat, 	N_("PTP: Invalid Code Format")},
	{PTP_RC_UnknownVendorCode, 	N_("PTP: Unknown Vendor Code")},
	{PTP_RC_CaptureAlreadyTerminated,
					N_("PTP: Capture Already Terminated")},
	{PTP_RC_DeviceBusy, 		N_("PTP: Device Busy")},
	{PTP_RC_InvalidParentObject, 	N_("PTP: Invalid Parent Object")},
	{PTP_RC_InvalidDevicePropFormat, N_("PTP: Invalid Device Prop Format")},
	{PTP_RC_InvalidDevicePropValue, N_("PTP: Invalid Device Prop Value")},
	{PTP_RC_InvalidParameter, 	N_("PTP: Invalid Parameter")},
	{PTP_RC_SessionAlreadyOpened, 	N_("PTP: Session Already Opened")},
	{PTP_RC_TransactionCanceled, 	N_("PTP: Transaction Canceled")},
	{PTP_RC_SpecificationOfDestinationUnsupported,
			N_("PTP: Specification Of Destination Unsupported")},
	{PTP_RC_EK_FilenameRequired,	N_("PTP: EK Filename Required")},
	{PTP_RC_EK_FilenameConflicts,	N_("PTP: EK Filename Conflicts")},
	{PTP_RC_EK_FilenameInvalid,	N_("PTP: EK Filename Invalid")},

	{PTP_ERROR_IO,		  N_("PTP: I/O error")},
	{PTP_ERROR_BADPARAM,	  N_("PTP: Error: bad parameter")},
	{PTP_ERROR_DATA_EXPECTED, N_("PTP: Protocol error, data expected")},
	{PTP_ERROR_RESP_EXPECTED, N_("PTP: Protocol error, response expected")},
	{0, NULL}
};

	for (i=0; ptp_errors[i].txt!=NULL; i++)
		if (ptp_errors[i].n == error)
			ptp_error(params, ptp_errors[i].txt);
}

const char*
ptp_get_property_description(PTPParams* params, uint16_t dpc)
{
	int i;
	// Device Property descriptions
	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties[] = {
		{PTP_DPC_Undefined,		N_("PTP Undefined Property")},
		{PTP_DPC_BatteryLevel,		N_("Battery Level")},
		{PTP_DPC_FunctionalMode,	N_("Functional Mode")},
		{PTP_DPC_ImageSize,		N_("Image Size")},
		{PTP_DPC_CompressionSetting,	N_("Compression Setting")},
		{PTP_DPC_WhiteBalance,		N_("White Balance")},
		{PTP_DPC_RGBGain,		N_("RGB Gain")},
		{PTP_DPC_FNumber,		N_("F-Number")},
		{PTP_DPC_FocalLength,		N_("Focal Length")},
		{PTP_DPC_FocusDistance,		N_("Focus Distance")},
		{PTP_DPC_FocusMode,		N_("Focus Mode")},
		{PTP_DPC_ExposureMeteringMode,	N_("Exposure Metering Mode")},
		{PTP_DPC_FlashMode,		N_("Flash Mode")},
		{PTP_DPC_ExposureTime,		N_("Exposure Time")},
		{PTP_DPC_ExposureProgramMode,	N_("Exposure Program Mode")},
		{PTP_DPC_ExposureIndex,
					N_("Exposure Index (film speed ISO)")},
		{PTP_DPC_ExposureBiasCompensation,
					N_("Exposure Bias Compensation")},
		{PTP_DPC_DateTime,		N_("Date Time")},
		{PTP_DPC_CaptureDelay,		N_("Pre-Capture Delay")},
		{PTP_DPC_StillCaptureMode,	N_("Still Capture Mode")},
		{PTP_DPC_Contrast,		N_("Contrast")},
		{PTP_DPC_Sharpness,		N_("Sharpness")},
		{PTP_DPC_DigitalZoom,		N_("Digital Zoom")},
		{PTP_DPC_EffectMode,		N_("Effect Mode")},
		{PTP_DPC_BurstNumber,		N_("Burst Number")},
		{PTP_DPC_BurstInterval,		N_("Burst Interval")},
		{PTP_DPC_TimelapseNumber,	N_("Timelapse Number")},
		{PTP_DPC_TimelapseInterval,	N_("Timelapse Interval")},
		{PTP_DPC_FocusMeteringMode,	N_("Focus Metering Mode")},
		{PTP_DPC_UploadURL,		N_("Upload URL")},
		{PTP_DPC_Artist,		N_("Artist")},
		{PTP_DPC_CopyrightInfo,		N_("Copyright Info")},
		{0,NULL}
	};
	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties_EK[] = {
		{PTP_DPC_EK_ColorTemperature,	N_("Color Temperature")},
		{PTP_DPC_EK_DateTimeStampFormat,
					N_("Date Time Stamp Format")},
		{PTP_DPC_EK_BeepMode,		N_("Beep Mode")},
		{PTP_DPC_EK_VideoOut,		N_("Video Out")},
		{PTP_DPC_EK_PowerSaving,	N_("Power Saving")},
		{PTP_DPC_EK_UI_Language,	N_("UI Language")},
		{0,NULL}
	};

	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties_Canon[] = {
		{PTP_DPC_CANON_BeepMode,	N_("Beep Mode")},
		{PTP_DPC_CANON_ViewfinderMode,	N_("Viewfinder Mode")},
		{PTP_DPC_CANON_ImageQuality,	N_("Image Quality")},
		{PTP_DPC_CANON_ImageSize,	N_("Image Size")},
		{PTP_DPC_CANON_FlashMode,	N_("Flash Mode")},
		/* a/v = audio/video */
		{PTP_DPC_CANON_TvAvSetting,	N_("TV A/V Setting")},
		{PTP_DPC_CANON_MeteringMode,	N_("Metering Mode")},
		{PTP_DPC_CANON_MacroMode,	N_("Macro Mode")},
		{PTP_DPC_CANON_FocusingPoint,	N_("Focusing Point")},
		{PTP_DPC_CANON_WhiteBalance,	N_("White Balance")},
		{PTP_DPC_CANON_ISOSpeed,	N_("ISO Speed")},
		{PTP_DPC_CANON_Aperture,	N_("Aperture")},
		{PTP_DPC_CANON_ShutterSpeed,	N_("ShutterSpeed")},
		{PTP_DPC_CANON_ExpCompensation,	N_("Exposure Compensation")},
		{PTP_DPC_CANON_Zoom,		N_("Zoom")},
		{PTP_DPC_CANON_SizeQualityMode,	N_("Size Quality Mode")},
		{PTP_DPC_CANON_FlashMemory,	N_("Flash Memory")},
		{PTP_DPC_CANON_CameraModel,	N_("Camera Model")},
		{PTP_DPC_CANON_CameraOwner,	N_("Camera Owner")},
		{PTP_DPC_CANON_UnixTime,	N_("UNIX Time")},
		{PTP_DPC_CANON_RealImageWidth,	N_("Real Image Width")},
		{PTP_DPC_CANON_PhotoEffect,	N_("Photo Effect")},
		{PTP_DPC_CANON_AssistLight,	N_("Assist Light")},
		{0,NULL}
	};

	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties_Nikon[] = {
		{PTP_DPC_NIKON_WhiteBalanceAutoBias,		/* 0xD017 */
		 N_("Auto White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceTungstenBias,	/* 0xD018 */
		 N_("Tungsten White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceFlourescentBias,	/* 0xD019 */
		 N_("Flourescent White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceDaylightBias,	/* 0xD01a */
		 N_("Daylight White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceFlashBias,		/* 0xD01b */
		 N_("Flash White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceCloudyBias,		/* 0xD01c */
		 N_("Cloudy White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceShadeBias,		/* 0xD01d */
		 N_("Shady White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceColourTemperature,	/* 0xD01e */
		 N_("White Balance Colour Temperature")},
		{PTP_DPC_NIKON_ImageSharpening,			/* 0xD02a */
		 N_("Sharpening")},
		{PTP_DPC_NIKON_ToneCompensation,		/* 0xD02b */
		 N_("Tone Compensation")},
		{PTP_DPC_NIKON_ColourMode,			/* 0xD02c */
		 N_("Colour Mode")},
		{PTP_DPC_NIKON_HueAdjustment,			/* 0xD02d */
		 N_("Hue Adjustment")},
		{PTP_DPC_NIKON_NonCPULensDataFocalLength,	/* 0xD02e */
		 N_("Lens Focal Length (Non CPU)")},
		{PTP_DPC_NIKON_NonCPULensDataMaximumAperature,	/* 0xD02f */
		 N_("Lens Max. Aperture (Non CPU)")},
		{PTP_DPC_NIKON_CSMMenuBankSelect,		/* 0xD040 */
		 N_("PTP_DPC_NIKON_CSMMenuBankSelect")},
		{PTP_DPC_NIKON_MenuBankNameA,			/* 0xD041 */
		 N_("PTP_DPC_NIKON_MenuBankNameA")},
		{PTP_DPC_NIKON_MenuBankNameB,			/* 0xD042 */
		 N_("PTP_DPC_NIKON_MenuBankNameB")},
		{PTP_DPC_NIKON_MenuBankNameC,			/* 0xD043 */
		 N_("PTP_DPC_NIKON_MenuBankNameC")},
		{PTP_DPC_NIKON_MenuBankNameD,			/* 0xD044 */
		 N_("PTP_DPC_NIKON_MenuBankNameD")},
		{PTP_DPC_NIKON_A1AFCModePriority,		/* 0xD048 */
		 N_("PTP_DPC_NIKON_A1AFCModePriority")},
		{PTP_DPC_NIKON_A2AFSModePriority,		/* 0xD049 */
		 N_("PTP_DPC_NIKON_A2AFSModePriority")},
		{PTP_DPC_NIKON_A3GroupDynamicAF,		/* 0xD04a */
		 N_("PTP_DPC_NIKON_A3GroupDynamicAF")},
		{PTP_DPC_NIKON_A4AFActivation,			/* 0xD04b */
		 N_("PTP_DPC_NIKON_A4AFActivation")},
		{PTP_DPC_NIKON_A5FocusAreaIllumManualFocus,	/* 0xD04c */
		 N_("PTP_DPC_NIKON_A5FocusAreaIllumManualFocus")},
		{PTP_DPC_NIKON_FocusAreaIllumContinuous,	/* 0xD04d */
		 N_("PTP_DPC_NIKON_FocusAreaIllumContinuous")},
		{PTP_DPC_NIKON_FocusAreaIllumWhenSelected,	/* 0xD04e */
		 N_("PTP_DPC_NIKON_FocusAreaIllumWhenSelected")},
		{PTP_DPC_NIKON_A6FocusArea,			/* 0xD04f */
		 N_("Focus Area")},
		{PTP_DPC_NIKON_A7VerticalAFON,			/* 0xD050 */
		 N_("Vertical AF On")},
		{PTP_DPC_NIKON_B1ISOAuto,			/* 0xD054 */
		 N_("Auto ISO")},
		{PTP_DPC_NIKON_B2ISOStep,			/* 0xD055 */
		 N_("ISO Step")},
		{PTP_DPC_NIKON_B3EVStep,			/* 0xD056 */
		 N_("Exposure Step")},
		{PTP_DPC_NIKON_B4ExposureCompEv,		/* 0xD057 */
		 N_("Exposure Compensation (EV)")},
		{PTP_DPC_NIKON_B5ExposureComp,			/* 0xD058 */
		 N_("Exposure Compensation")},
		{PTP_DPC_NIKON_B6CenterWeightArea,		/* 0xD059 */
		 N_("Centre Weight Area")},
		{PTP_DPC_NIKON_C1AELock,			/* 0xD05e */
		 N_("Exposure Lock")},
		{PTP_DPC_NIKON_C2AELAFL,			/* 0xD05f */
		 N_("Focus Lock")},
		{PTP_DPC_NIKON_C3AutoMeterOff,			/* 0xD062 */
		 N_("Auto Meter Off Time")},
		{PTP_DPC_NIKON_C4SelfTimer,			/* 0xD063 */
		 N_("Self Timer Delay")},
		{PTP_DPC_NIKON_C5MonitorOff,			/* 0xD064 */
		 N_("LCD Off Time")},
		{PTP_DPC_NIKON_D1ShootingSpeed,			/* 0xD068 */
		 N_("Shooting Speed")},
		{PTP_DPC_NIKON_D2MaximumShots,			/* 0xD069 */
		 N_("Max. Shots")},
		{PTP_DPC_NIKON_D3ExpDelayMode,			/* 0xD06a */
		 N_("PTP_DPC_NIKON_D3ExpDelayMode")},
		{PTP_DPC_NIKON_D4LongExposureNoiseReduction,	/* 0xD06b */
		 N_("Long Exposure Noise Reduction")},
		{PTP_DPC_NIKON_D5FileNumberSequence,		/* 0xD06c */
		 N_("File Number Sequencing")},
		{PTP_DPC_NIKON_D6ControlPanelFinderRearControl,	/* 0xD06d */
		 N_("PTP_DPC_NIKON_D6ControlPanelFinderRearControl")},
		{PTP_DPC_NIKON_ControlPanelFinderViewfinder,	/* 0xD06e */
		 N_("PTP_DPC_NIKON_ControlPanelFinderViewfinder")},
		{PTP_DPC_NIKON_D7Illumination,			/* 0xD06f */
		 N_("PTP_DPC_NIKON_D7Illumination")},
		{PTP_DPC_NIKON_E1FlashSyncSpeed,		/* 0xD074 */
		 N_("Flash Sync. Speed")},
		{PTP_DPC_NIKON_E2FlashShutterSpeed,		/* 0xD075 */
		 N_("Flash Shutter Speed")},
		{PTP_DPC_NIKON_E3AAFlashMode,			/* 0xD076 */
		 N_("Flash Mode")},
		{PTP_DPC_NIKON_E4ModelingFlash,			/* 0xD077 */
		 N_("Modeling Flash")},
		{PTP_DPC_NIKON_E5AutoBracketySet,		/* 0xD078 */
		 N_("Auto Bracket Set")},
		{PTP_DPC_NIKON_E6ManualModeBracketing,		/* 0xD079 */
		 N_("Manual Mode Bracketing")},
		{PTP_DPC_NIKON_E7AutoBracketOrder,		/* 0xD07a */
		 N_("Auto Bracket Order")},
		{PTP_DPC_NIKON_E8AutoBracketSelection,		/* 0xD07b */
		 N_("Auto Bracket Selection")},
		{PTP_DPC_NIKON_F1CenterButtonShootingMode,	/* 0xD080 */
		 N_("Center Button Shooting Mode")},
		{PTP_DPC_NIKON_CenterButtonPlaybackMode,	/* 0xD081 */
		 N_("Center Button Playback Mode")},
		{PTP_DPC_NIKON_F2Multiselector,			/* 0xD082 */
		 N_("Multiselector")},
		{PTP_DPC_NIKON_F3PhotoInfoPlayback,		/* 0xD083 */
		 N_("Photo Info. Playback")},
		{PTP_DPC_NIKON_F4AssignFuncButton,		/* 0xD084 */
		 N_("Assign Func. Button")},
		{PTP_DPC_NIKON_F5CustomizeCommDials,		/* 0xD085 */
		 N_("Customise Command Dials")},
		{PTP_DPC_NIKON_ChangeMainSub,			/* 0xD086 */
		 N_("Reverse Command Dials")},
		{PTP_DPC_NIKON_AperatureSetting,		/* 0xD087 */
		 N_("Aperture Setting")},
		{PTP_DPC_NIKON_MenusAndPlayback,		/* 0xD088 */
		 N_("Menus and Playback")},
		{PTP_DPC_NIKON_F6ButtonsAndDials,		/* 0xD089 */
		 N_("Buttons and Dials")},
		{PTP_DPC_NIKON_F7NoCFCard,			/* 0xD08a */
		 N_("No CF Card Release")},
		{PTP_DPC_NIKON_AutoImageRotation,		/* 0xD092 */
		 N_("Auto Image Rotation")},
		{PTP_DPC_NIKON_ExposureBracketingOnOff,		/* 0xD0c0 */
		 N_("Exposure Bracketing")},
		{PTP_DPC_NIKON_ExposureBracketingIntervalDist,	/* 0xD0c1 */
		 N_("Exposure Bracketing Distance")},
		{PTP_DPC_NIKON_ExposureBracketingNumBracketPlace,/* 0xD0c2 */
		 N_("Exposure Bracketing Number")},
		{PTP_DPC_NIKON_AutofocusLCDTopMode2,		/* 0xD107 */
		 N_("AF LCD Top Mode 2")},
		{PTP_DPC_NIKON_AutofocusLCDTopMode3AndMode4,	/* 0xD108 */
		 N_("Active AF Sensor")},
		{PTP_DPC_NIKON_LightMeter,			/* 0xD10a */
		 N_("Exposure Meter")},
		{PTP_DPC_NIKON_ExposureAperatureLock(ReadOnly),	/* 0xD111 */
		 N_("Exposure Aperture Lock")},
		{PTP_DPC_NIKON_MaximumShots,			/* 0xD103 */
		 N_("Maximum Shots")},
		{PTP_DPC_NIKON_Beep,
		 N_("AF Beep Mode")},
		{PTP_DPC_NIKON_AFC,
		 N_("??? AF Related")},
		{PTP_DPC_NIKON_AFLampOff,
		 N_("AF Lamp")},
		{PTP_DPC_NIKON_PADVPMode,
		 N_("Auto ISO P/A/DVP Setting")},
		{PTP_DPC_NIKON_ReviewOff,
		 N_("Image Review")},
		{PTP_DPC_NIKON_GridDisplay,
		 N_("Viewfinder Grid Display")},
		{PTP_DPC_NIKON_AFAreaIllumination,
		 N_("AF Area Illumination")},
		{PTP_DPC_NIKON_FlashMode,
		 N_("Flash Mode")},
		{PTP_DPC_NIKON_FlashPower,
		 N_("Flash Power")},
		{PTP_DPC_NIKON_FlashSignOff,
		 N_("Flash Sign")},
		{PTP_DPC_NIKON_FlashExposureCompensation,
		 N_("Flash Exposure Compensation")},
		{PTP_DPC_NIKON_RemoteTimeout,
		 N_("Remote Timeout")},
		{PTP_DPC_NIKON_ImageCommentString,
		 N_("Image Comment String")},
		{PTP_DPC_NIKON_FlashOpen,
		 N_("Flash Open")},
		{PTP_DPC_NIKON_FlashCharged,
		 N_("Flash Charged")},
		{PTP_DPC_NIKON_LensID,
		 N_("Lens ID")},
		{PTP_DPC_NIKON_FocalLengthMin,
		 N_("Min. Focal Length")},
		{PTP_DPC_NIKON_FocalLengthMax,
		 N_("Max. Focal Length")},
		{PTP_DPC_NIKON_MaxApAtMinFocalLength,
		 N_("Max. Aperture at Min. Focal Length")},
		{PTP_DPC_NIKON_MaxApAtMaxFocalLength,
		 N_("Max. Aperture at Max. Focal Length")},
		{PTP_DPC_NIKON_LowLight,
		 N_("Low Light")},
		{0,NULL}
	};

	for (i=0; ptp_device_properties[i].txt!=NULL; i++)
		if (ptp_device_properties[i].dpc==dpc)
			return (ptp_device_properties[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_EASTMAN_KODAK)
		for (i=0; ptp_device_properties_EK[i].txt!=NULL; i++)
			if (ptp_device_properties_EK[i].dpc==dpc)
				return (ptp_device_properties_EK[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_CANON)
		for (i=0; ptp_device_properties_Canon[i].txt!=NULL; i++)
			if (ptp_device_properties_Canon[i].dpc==dpc)
				return (ptp_device_properties_Canon[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_NIKON)
		for (i=0; ptp_device_properties_Nikon[i].txt!=NULL; i++)
			if (ptp_device_properties_Nikon[i].dpc==dpc)
				return (ptp_device_properties_Nikon[i].txt);

	return NULL;
}

static int64_t
_value_to_num(PTPPropertyValue *data, uint16_t dt) {
	if (dt == PTP_DTC_STR) {
		return atol(data->str);
	}
	if (dt & PTP_DTC_ARRAY_MASK) {
		return 0;
	} else {
		switch (dt) {
		case PTP_DTC_UNDEF: 
			return 0;
		case PTP_DTC_INT8:
			return data->i8;
		case PTP_DTC_UINT8:
			return data->u8;
		case PTP_DTC_INT16:
			return data->i16;
		case PTP_DTC_UINT16:
			return data->u16;
		case PTP_DTC_INT32:
			return data->i32;
		case PTP_DTC_UINT32:
			return data->u32;
	/*
		PTP_DTC_INT64           
		PTP_DTC_UINT64         
		PTP_DTC_INT128        
		PTP_DTC_UINT128      
	*/
		default:
			return 0;
		}
	}

	return 0;
}

#define PTP_VAL_BOOL(dpc) {dpc, 0, N_("Off")}, {dpc, 1, N_("On")}
#define PTP_VAL_RBOOL(dpc) {dpc, 0, N_("On")}, {dpc, 1, N_("Off")}
#define PTP_VAL_YN(dpc) {dpc, 0, N_("No")}, {dpc, 1, N_("Yes")}

int
ptp_render_property_value(PTPParams* params, uint16_t dpc,
			  PTPDevicePropDesc *dpd, int length, char *out)
{
	int i;

	struct {
		uint16_t dpc;
		double coef;
		double bias;
		const char *format;
	} ptp_value_trans[] = {
		{PTP_DPC_ExposureIndex, 1.0, 0.0, "ISO %.0f"},
		{0, 0.0, 0.0, NULL}
	};

	struct {
		uint16_t dpc;
		double coef;
		double bias;
		const char *format;
	} ptp_value_trans_Nikon[] = {
		{PTP_DPC_BatteryLevel, 1.0, 0.0, "%.0f%%"},
		{PTP_DPC_FNumber, 0.01, 0.0, "f/%.2g"},
		{PTP_DPC_FocalLength, 0.01, 0.0, "%.0f mm"},
		{PTP_DPC_ExposureTime, 0.00001, 0.0, "%.2g sec"},
		{PTP_DPC_ExposureBiasCompensation, 0.001, 0.0, N_("%.1f stops")},
		{PTP_DPC_NIKON_LightMeter, 0.08333, 0.0, N_("%.1f stops")},
		{PTP_DPC_NIKON_FlashExposureCompensation, 0.16666, 0.0, N_("%.1f stops")},
		{PTP_DPC_NIKON_B6CenterWeightArea, 2.0, 6.0, N_("%.0f mm")},
		{PTP_DPC_NIKON_FocalLengthMin, 0.01, 0.0, "%.0f mm"},
		{PTP_DPC_NIKON_FocalLengthMax, 0.01, 0.0, "%.0f mm"},
		{PTP_DPC_NIKON_MaxApAtMinFocalLength, 0.01, 0.0, "f/%.2g"},
		{PTP_DPC_NIKON_MaxApAtMaxFocalLength, 0.01, 0.0, "f/%.2g"},
		{0, 0.0, 0.0, NULL}
	};

	struct {
		uint16_t dpc;
		int64_t key;
		char *value;
	} ptp_value_list_Nikon[] = {
		{PTP_DPC_CompressionSetting, 0, N_("JPEG Basic")},
		{PTP_DPC_CompressionSetting, 1, N_("JPEG Norm")},
		{PTP_DPC_CompressionSetting, 2, N_("JPEG Fine")},
		{PTP_DPC_CompressionSetting, 4, N_("RAW")},
		{PTP_DPC_CompressionSetting, 5, N_("RAW + JPEG Basic")},
		{PTP_DPC_WhiteBalance, 2, N_("Auto")},
		{PTP_DPC_WhiteBalance, 6, N_("Incadesent")},
		{PTP_DPC_WhiteBalance, 5, N_("Fluorescent")},
		{PTP_DPC_WhiteBalance, 4, N_("Daylight")},
		{PTP_DPC_WhiteBalance, 7, N_("Flash")},
		{PTP_DPC_WhiteBalance, 32784, N_("Cloudy")},
		{PTP_DPC_WhiteBalance, 32785, N_("Shade")},
		{PTP_DPC_WhiteBalance, 32787, N_("Preset")},
		{PTP_DPC_FlashMode, 32784, N_("Default")},
		{PTP_DPC_FlashMode, 4, N_("Red-eye Reduction")},
		{PTP_DPC_FlashMode, 32787, N_("Red-eye Reduction + Slow Sync")},
		{PTP_DPC_FlashMode, 32785, N_("Slow Sync")},
		{PTP_DPC_FlashMode, 32785, N_("Rear Curtain Sync + Slow Sync")},
		{PTP_DPC_FocusMeteringMode, 2, N_("Dynamic Area")},
		{PTP_DPC_FocusMeteringMode, 32784, N_("Single Area")},
		{PTP_DPC_FocusMeteringMode, 32785, N_("Closest Subject")},
		{PTP_DPC_FocusMode, 1, N_("Manual Focus")},
		{PTP_DPC_FocusMode, 32784, "AF-S"},
		{PTP_DPC_FocusMode, 32785, "AF-C"},
		PTP_VAL_BOOL(PTP_DPC_NIKON_B1ISOAuto),
		PTP_VAL_BOOL(PTP_DPC_NIKON_B5ExposureComp),
		PTP_VAL_BOOL(PTP_DPC_NIKON_C1AELock),
		{PTP_DPC_ExposureMeteringMode, 2, N_("Center Weighted")},
		{PTP_DPC_ExposureMeteringMode, 3, N_("Matrix")},
		{PTP_DPC_ExposureMeteringMode, 4, N_("Spot")},
		{PTP_DPC_ExposureProgramMode, 1, "M"},
		{PTP_DPC_ExposureProgramMode, 3, "A"},
		{PTP_DPC_ExposureProgramMode, 4, "S"},
		{PTP_DPC_ExposureProgramMode, 2, "P"},
		{PTP_DPC_ExposureProgramMode, 32784, N_("Auto")},
		{PTP_DPC_ExposureProgramMode, 32785, N_("Portrait")},
		{PTP_DPC_ExposureProgramMode, 32786, N_("Landscape")},
		{PTP_DPC_ExposureProgramMode, 32787, N_("Macro")},
		{PTP_DPC_ExposureProgramMode, 32788, N_("Sports")},
		{PTP_DPC_ExposureProgramMode, 32790, N_("Night Landscape")},
		{PTP_DPC_ExposureProgramMode, 32789, N_("Night Portrait")},
		{PTP_DPC_StillCaptureMode, 1, N_("Single Shot")},
		{PTP_DPC_StillCaptureMode, 2, N_("Power Wind")},
		{PTP_DPC_StillCaptureMode, 32785, N_("Timer")},
		{PTP_DPC_StillCaptureMode, 32787, N_("Remote")},
		{PTP_DPC_StillCaptureMode, 32788, N_("Timer + Remote")},
		PTP_VAL_BOOL(PTP_DPC_NIKON_AFC),
		PTP_VAL_RBOOL(PTP_DPC_NIKON_AFLampOff),
		PTP_VAL_RBOOL(PTP_DPC_NIKON_ReviewOff),
		PTP_VAL_BOOL(PTP_DPC_NIKON_GridDisplay),
		{PTP_DPC_NIKON_AFAreaIllumination, 0, N_("Auto")},
		{PTP_DPC_NIKON_AFAreaIllumination, 1, N_("Off")},
		{PTP_DPC_NIKON_AFAreaIllumination, 2, N_("On")},
		{PTP_DPC_NIKON_ColourMode, 0, "sRGB"},
		{PTP_DPC_NIKON_ColourMode, 1, ""},
		{PTP_DPC_NIKON_FlashMode, 0, "iTTL"},
		{PTP_DPC_NIKON_FlashMode, 1, N_("Manual")},
		{PTP_DPC_NIKON_FlashMode, 2, N_("Commander")},
		{PTP_DPC_NIKON_FlashPower, 0, N_("Full")},
		{PTP_DPC_NIKON_FlashPower, 1, "1/2"},
		{PTP_DPC_NIKON_FlashPower, 2, "1/4"},
		{PTP_DPC_NIKON_FlashPower, 3, "1/8"},
		{PTP_DPC_NIKON_FlashPower, 4, "1/16"},
		PTP_VAL_RBOOL(PTP_DPC_NIKON_FlashSignOff),
		{PTP_DPC_NIKON_RemoteTimeout, 0, N_("1 min")},
		{PTP_DPC_NIKON_RemoteTimeout, 1, N_("5 mins")},
		{PTP_DPC_NIKON_RemoteTimeout, 2, N_("10 mins")},
		{PTP_DPC_NIKON_RemoteTimeout, 3, N_("15 mins")},
		PTP_VAL_YN(PTP_DPC_NIKON_FlashOpen),
		PTP_VAL_YN(PTP_DPC_NIKON_FlashCharged),
		PTP_VAL_BOOL(PTP_DPC_NIKON_D4LongExposureNoiseReduction),
		PTP_VAL_BOOL(PTP_DPC_NIKON_D5FileNumberSequence),
		PTP_VAL_BOOL(PTP_DPC_NIKON_ChangeMainSub),
		PTP_VAL_RBOOL(PTP_DPC_NIKON_F7NoCFCard),
		PTP_VAL_BOOL(PTP_DPC_NIKON_AutoImageRotation),
		PTP_VAL_BOOL(PTP_DPC_NIKON_ExposureBracketingOnOff),
		{PTP_DPC_NIKON_AutofocusLCDTopMode3AndMode4, 0, N_("Centre")},
		{PTP_DPC_NIKON_AutofocusLCDTopMode3AndMode4, 1, N_("Top")},
		{PTP_DPC_NIKON_AutofocusLCDTopMode3AndMode4, 2, N_("Bottom")},
		{PTP_DPC_NIKON_AutofocusLCDTopMode3AndMode4, 3, N_("Left")},
		{PTP_DPC_NIKON_AutofocusLCDTopMode3AndMode4, 4, N_("Right")},
		{PTP_DPC_NIKON_LensID, 0, N_("Unknown")},
		{PTP_DPC_NIKON_LensID, 38, "Sigma 70-300mm 1:4-5.6 D APO Macro"},
		{PTP_DPC_NIKON_LensID, 83, "AF Nikkor 80-200mm 1:2.8 D ED"},
		{PTP_DPC_NIKON_LensID, 118, "AF Nikkor 50mm 1:1.8 D"},
		{PTP_DPC_NIKON_LensID, 127, "AF-S Nikkor 18-70mm 1:3.5-4.5G ED DX"},
		PTP_VAL_YN(PTP_DPC_NIKON_LowLight),
		PTP_VAL_RBOOL(PTP_DPC_NIKON_Beep),
		{0, 0, NULL}
	};

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_NIKON) {
		int64_t kval;

		for (i=0; ptp_value_trans[i].dpc!=0; i++)
			if (ptp_value_trans[i].dpc==dpc) {
				double value = _value_to_num(&(dpd->CurrentValue), dpd->DataType);

				return snprintf(out, length, 
					_(ptp_value_trans[i].format),
					value * ptp_value_trans[i].coef +
					ptp_value_trans[i].bias);
			}

		for (i=0; ptp_value_trans_Nikon[i].dpc!=0; i++)
			if (ptp_value_trans_Nikon[i].dpc==dpc) {
				double value = _value_to_num(&(dpd->CurrentValue), dpd->DataType);

				return snprintf(out, length, 
					_(ptp_value_trans_Nikon[i].format),
					value * ptp_value_trans_Nikon[i].coef +
					ptp_value_trans_Nikon[i].bias);
			}

		kval = _value_to_num(&(dpd->CurrentValue), dpd->DataType);

		for (i=0; ptp_value_list_Nikon[i].dpc!=0; i++)
			if (ptp_value_list_Nikon[i].dpc==dpc &&
			    ptp_value_list_Nikon[i].key==kval)
				return snprintf(out, length, "%s",
					_(ptp_value_list_Nikon[i].value));
	}

	return 0;
}
