/* ptp.c
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

#include "config.h"
#include "ptp.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

#define CHECK_PTP_RC(result)	{uint16_t r=(result); if (r!=PTP_RC_OK) return r;}
//#define CHECK_PTP_RC_free(result, free_ptr) {uint16_t r=(result); if (r!=PTP_RC_OK) {return r; free(free_ptr);}}

static void
ptp_debug (PTPParams *params, const char *format, ...)
{  
        va_list args;

        va_start (args, format);
        if (params->debug_func)
                params->debug_func (params->data, format, args);
        else
                vfprintf (stderr, format, args);
        va_end (args);
}  

static void
ptp_error (PTPParams *params, const char *format, ...)
{  
        va_list args;

        va_start (args, format);
        if (params->error_func)
                params->error_func (params->data, format, args);
        else
                vfprintf (stderr, format, args);
        va_end (args);
}

// Pack / unpack functions

#include "ptp-pack.c"

// send / receive functions

static uint16_t
ptp_sendreq (PTPParams* params, PTPReq* databuf, uint16_t code)
{
	uint16_t ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;
	
	//req->len = htod32(PTP_REQ_LEN);
	req->type = htod16(PTP_TYPE_REQ);
	req->code = htod16(code);
	req->trans_id = htod32(params->transaction_id);

	ret=params->write_func ((unsigned char *) req, dtoh32(req->len),
				 params->data);
	if (databuf==NULL) free (req);
	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
		ptp_error (params,
		"PTP: request code 0x%4x sending req error", code);
	}
	return ret;
}

static uint16_t
ptp_senddata (PTPParams* params, PTPReq* req, uint16_t code,
		uint32_t writelen)
{
	uint16_t ret;
	if (req==NULL) return PTP_ERROR_BADPARAM;
	
	req->len = htod32(writelen);
	req->type = htod16(PTP_TYPE_DATA);
	req->code = htod16(code);
	req->trans_id = htod32(params->transaction_id);

	ret=params->write_func ((unsigned char *) req, writelen,
				params->data);
	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
		ptp_error (params,
		"PTP: request code 0x%4x sending data error", code);
	}
	return ret;
}

static uint16_t
ptp_getdata (PTPParams* params, PTPReq* req, uint16_t code,
		unsigned int readlen)
{
	uint16_t ret;
	if (req==NULL) return PTP_ERROR_BADPARAM; 

	memset(req, 0, readlen);
	ret=params->read_func((unsigned char *) req, readlen, params->data);

	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
	} else
	if (dtoh16(req->type)!=PTP_TYPE_DATA) {
		ret = PTP_ERROR_DATA_EXPECTED;
	} else
	if (dtoh16(req->code)!=code) {
		ret = dtoh16(req->code);
	}
	if (ret!=PTP_RC_OK) 
		ptp_error (params,
		"PTP: request code 0x%4.4x getting data error 0x%4.4x", code, ret);
	return ret;
}

static uint16_t
ptp_getresp (PTPParams* params, PTPReq* databuf, uint16_t code)
{
	uint16_t ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;

	memset(req, 0, PTP_RESP_LEN);
	ret=params->read_func((unsigned char*) req, PTP_RESP_LEN, params->data);

	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
	} else
	if (dtoh16(req->type)!=PTP_TYPE_RESP) {
		ret = PTP_ERROR_RESP_EXPECTED;
	} else
	if (dtoh16(req->code)!=code) {
		ret = dtoh16(req->code);
	}
	if (ret!=PTP_RC_OK)
		ptp_error (params,
		"PTP: request code 0x%4x getting resp error 0x%4x", code, ret);
	if (databuf==NULL) free (req);
	return ret;
}

// major PTP functions

// Transaction data phase description
#define PTP_DP_NODATA		0x0000	// No Data Phase
#define PTP_DP_SENDDATA		0x0001	// sending data
#define PTP_DP_GETDATA		0x0002	// geting data
#define PTP_DP_DATA_MASK	0x00ff	// data phase mask

// Number of PTP Request phase parameters
#define PTP_RQ_PARAM0		0x0000	// zero parameters
#define PTP_RQ_PARAM1		0x0100	// one parameter
#define PTP_RQ_PARAM2		0x0200	// two parameters
#define PTP_RQ_PARAM3		0x0300	// three parameters
#define PTP_RQ_PARAM4		0x0400	// four parameters
#define PTP_RQ_PARAM5		0x0500	// five parameters

/**
 * ptp_transaction:
 * params:	PTPParams*
 * 		PTPReq* req		- request phase PTPReq
 * 		uint16_t code		- PTP operation code
 * 		uint16_t flags		- lower 8 bits - data phase description
 *					  upper 8 bits - number of params
 *					  of request phase
 * 		unsigned int datalen	- data phase data length
 * 		PTPReq* dataphasebuf	- data phase req bufor
 *
 * Performs PTP transaction. Uses PTPReq* req for Operation Request Phase,
 * sending it to responder and returns Response Phase response there.
 * PTPReq* dataphasebuf buffor is used for PTP Data Phase, depending on 
 * unsigned short dataphase is used for sending or receiving data.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success PTPReq* req contains PTP Response Phase response packet.
 **/
static uint16_t
ptp_transaction (PTPParams* params, PTPReq* req, uint16_t code,
			uint16_t flags, unsigned int datalen,
			PTPReq* dataphasebuf)
{
	if ((params==NULL) || (req==NULL)) 
		return PTP_ERROR_BADPARAM;
	params->transaction_id++;
	// calculate the request phase container length
	req->len=htod32(PTP_REQ_HDR_LEN+((flags>>8)*sizeof(uint32_t)));
	// send request
	CHECK_PTP_RC(ptp_sendreq(params, req, code));
	switch (flags&PTP_DP_DATA_MASK) {
		case PTP_DP_SENDDATA:
			datalen+=PTP_REQ_HDR_LEN;
			CHECK_PTP_RC(ptp_senddata(params, dataphasebuf,
				code, datalen));
			break;
		case PTP_DP_GETDATA:
			datalen+=PTP_REQ_HDR_LEN;
			CHECK_PTP_RC(ptp_getdata(params, dataphasebuf,
				code, datalen));
			break;
		case PTP_DP_NODATA:
			break;
		default:
		return PTP_ERROR_BADPARAM;
	}
	CHECK_PTP_RC(ptp_getresp(params, req, code));
	return PTP_RC_OK;
}

// Enets handling functions

// PTP Events wait for or check mode
#define PTP_EVENT_CHECK			0x0000	// waits for
#define PTP_EVENT_CHECK_FAST		0x0001	// checks

static inline uint16_t
ptp_event (PTPParams* params, PTPEvent* event, int wait)
{
	uint16_t ret;

	if ((params==NULL) || (event==NULL)) 
		return PTP_ERROR_BADPARAM;
	
	switch(wait) {
		case PTP_EVENT_CHECK:
			ret=params->check_int_func((unsigned char*) event, PTP_EVENT_LEN, params->data);
			break;
		case PTP_EVENT_CHECK_FAST:
			ret=params->check_int_fast_func((unsigned char*) event, PTP_EVENT_LEN, params->data);
			break;
		default:
			ret=PTP_ERROR_BADPARAM;
	}

	return ret;
}

uint16_t
ptp_event_check (PTPParams* params, PTPEvent* event) {

	return ptp_event (params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_event_wait (PTPParams* params, PTPEvent* event) {

	return ptp_event (params, event, PTP_EVENT_CHECK);
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
	PTPReq di;
	PTPReq req;

	ret=ptp_transaction(params, &req, PTP_OC_GetDeviceInfo,
		PTP_DP_GETDATA | PTP_RQ_PARAM0, PTP_REQ_DATALEN, &di);
	ptp_unpack_DI(params, &di, deviceinfo);
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
	PTPReq req;
	
	*(int *)(req.data)=htod32(session);

	return ptp_transaction(params, &req, PTP_OC_OpenSession,
		PTP_DP_NODATA | PTP_RQ_PARAM1, 0, NULL);
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
	PTPReq req;

	return ptp_transaction(params, &req, PTP_OC_CloseSession,
		PTP_DP_NODATA | PTP_RQ_PARAM0, 0, NULL);
}

/**
 * ptp_getststorageids:
 * params:	PTPParams*
 *
 * Gets array of StorageiDs and fills the storageids structure.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getstorageids (PTPParams* params, PTPStorageIDs* storageids)
{
	uint16_t ret;
	PTPReq si;
	PTPReq req;

	ret=ptp_transaction(params, &req, PTP_OC_GetStorageIDs,
		PTP_DP_GETDATA | PTP_RQ_PARAM0, PTP_REQ_DATALEN, &si);
	ptp_unpack_SIDs(params, &si, storageids);
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
	PTPReq si;
	PTPReq req;

	*(int *)(req.data)=htod32(storageid);
	ret=ptp_transaction(params, &req, PTP_OC_GetStorageInfo,
		PTP_DP_GETDATA | PTP_RQ_PARAM1, PTP_REQ_DATALEN, &si);
	ptp_unpack_SI(params, &si, storageinfo);
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
	PTPReq req;
	PTPReq oh;
	
	*(int *)(req.data)=htod32(storage);
	*(int *)(req.data+4)=htod32(objectformatcode);
	*(int *)(req.data+8)=htod32(associationOH);

	ret=ptp_transaction(params, &req, PTP_OC_GetObjectHandles,
		PTP_DP_GETDATA | PTP_RQ_PARAM3, PTP_REQ_DATALEN, &oh);
	ptp_unpack_OH(params, &oh, objecthandles);
	return ret;
}

uint16_t
ptp_getobjectinfo (PTPParams* params, uint32_t handle,
			PTPObjectInfo* objectinfo)
{
	uint16_t ret;
	PTPReq req;
	PTPReq oi;

	*(int *)(req.data)=htod32(handle);
	ret=ptp_transaction(params, &req, PTP_OC_GetObjectInfo,
		PTP_DP_GETDATA | PTP_RQ_PARAM1, PTP_REQ_DATALEN, &oi);
	ptp_unpack_OI(params, &oi, objectinfo);
	return ret;
}

uint16_t
ptp_getobject (PTPParams* params, uint32_t handle, 
			uint32_t size, PTPReq* object)
{
	PTPReq req;

	*(int *)(req.data)=htod32(handle);
	return ptp_transaction(params, &req, PTP_OC_GetObject,
		PTP_DP_GETDATA | PTP_RQ_PARAM1, size, object);
}


uint16_t
ptp_getthumb (PTPParams* params, uint32_t handle, 
			uint32_t size, PTPReq* object)
{
	PTPReq req;

	*(int *)(req.data)=htod32(handle);
	return ptp_transaction(params, &req, PTP_OC_GetThumb,
		PTP_DP_GETDATA | PTP_RQ_PARAM1, size, object);
}

/**
 * ptp_deleteobject:
 * params:	PTPParams*
 *		handle			- object handle
 *		ofc			- object format code
 * 
 * Deletes desired objects.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_deleteobject (PTPParams* params, uint32_t handle,
			uint32_t ofc)
{
	PTPReq req;
	*(uint32_t *)(req.data)=htod32(handle);
	*(uint32_t *)(req.data+4)=htod32(ofc);

	return ptp_transaction(params, &req, PTP_OC_DeleteObject,
	PTP_DP_NODATA | PTP_RQ_PARAM2, 0, NULL);
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
	PTPReq req;
	*(uint32_t *)(req.data)=htod32(storageid);
	*(uint32_t *)(req.data+4)=htod32(ofc);
	
	return ptp_transaction(params, &req, PTP_OC_InitiateCapture,
	PTP_DP_NODATA | PTP_RQ_PARAM2, 0, NULL);
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
	PTPReq req;
	PTPReq req_oi;
	uint32_t size;

	*(uint32_t *)(req.data)=htod32(*store);
	*(uint32_t *)(req.data+4)=htod32(*parenthandle);
	
	size=ptp_pack_OI(params, objectinfo, &req_oi);
	ret= ptp_transaction(params, &req, PTP_OC_EK_SendFileObjectInfo,
		PTP_DP_SENDDATA | PTP_RQ_PARAM2, size, &req_oi); 
	*store=dtoh32a(req.data);
	*parenthandle=dtoh32a(req.data+4);
	*handle=dtoh32a(req.data+8); 
	return ret;
}

/**
 * ptp_ek_sendfileobject:
 * params:	PTPParams*
 *		PTPReq*	object		- object->data contain object
 *					  that is to be sent
 *		uint32_t size		- object size
 *		
 * Sends object to Responder.
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_ek_sendfileobject (PTPParams* params, PTPReq* object, uint32_t size)
{
	PTPReq req;
	return ptp_transaction(params, &req, PTP_OC_EK_SendFileObject,
		PTP_DP_SENDDATA | PTP_RQ_PARAM0, size, object);
}


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
ptp_property_issupported(PTPParams* params, uint16_t property)
{
	int i=0;

	for (;i<params->deviceinfo.DevicePropertiesSupported_len;i++) {
		if (params->deviceinfo.DevicePropertiesSupported[i]==property)
			return 1;
	}
	return 0;
}
