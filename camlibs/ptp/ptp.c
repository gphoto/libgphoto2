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

#include <config.h>
#include "ptp.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
	
	req->len = htod32(PTP_REQ_LEN);
	req->type = htod16(PTP_TYPE_REQ);
	req->code = htod16(code);
	req->trans_id = htod32(params->transaction_id);
	params->transaction_id++;

	ret=params->write_func ((unsigned char *) req, PTP_REQ_LEN,
				 params->data);
	if (databuf==NULL) free (req);
	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
		ptp_error (params,
		"request code 0x%4x sending req error", code);
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
		"request code 0x%4x sending data error", code);
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
		ret = req->code;
	}
	if (ret!=PTP_RC_OK) 
		ptp_error (params,
		"request code 0x%4.4x getting data error 0x%4.4x", code, ret);
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
		ret = req->code;
	}
	if (ret!=PTP_RC_OK)
		ptp_error (params,
		"request code 0x%4x getting resp error 0x%4x", code, ret);
	if (databuf==NULL) free (req);
	return ret;
}

// major PTP functions

// Transaction data phase description
#define PTP_DP_NODATA		0x00	// No Data Phase
#define PTP_DP_SENDDATA		0x01	// sending data
#define PTP_DP_GETDATA		0x02	// geting data

/**
 * ptp_transaction:
 * params:	PTPParams*
 * 		PTPReq* req		- request phase PTPReq
 * 		uint16_t code		- PTP operation code
 * 		unsigned short dataphase- data phase description
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
			unsigned short dataphase, unsigned int datalen,
			PTPReq* dataphasebuf)
{
	if ((params==NULL) || (req==NULL)) 
		return PTP_ERROR_BADPARAM;
	CHECK_PTP_RC(ptp_sendreq(params, req, code));
	switch (dataphase) {
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

#if 0
// Do GetDevInfo (we may use it for some camera_about)
uint16_t
ptp_getdevinfo (PTPParams* params, PTPDedviceInfo* devinfo)
{
}
#endif

/**
 * all ptp_ functions should take integer parameters
 * in host byte order!
 **/

/**
 * ptp_opensession:
 * params:	PTPParams*
 * 		uint32_t session		- session number 
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
		PTP_DP_NODATA, 0, NULL);
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
		PTP_DP_NODATA, 0, NULL);
}

/**
 * ptp_getobjecthandles:
 * params:	PTPParams*
 *		PTPObjectHandles*	- pointer to structute
 *		uint32_t store		- StoreID
 *
 * Fills objecthandles with structure returned by device.
 *
 * Return values: Some PTP_RC_* code.
 **/
//     XXX !!! this function prototype gonna change soon!
// XXX still ObjectFormatCode and ObjectHandle of Assiciation NOT
//     IMPLEMENTED (parameter 2 and 3)
uint16_t
ptp_getobjecthandles (PTPParams* params, PTPObjectHandles* objecthandles,
			uint32_t store)
{
	uint16_t ret;
	PTPReq req;
	PTPReq* oh=malloc(sizeof(PTPObjectHandles)+PTP_REQ_HDR_LEN);
	
	*(int *)(req.data)=htod32(store);

	ret=ptp_transaction(params, &req, PTP_OC_GetObjectHandles,
		PTP_DP_GETDATA, sizeof(PTPObjectHandles), oh);
	memcpy(objecthandles, oh->data, sizeof(PTPObjectHandles));
	free(oh);
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
		PTP_DP_GETDATA, PTP_REQ_DATALEN, &oi);
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
		PTP_DP_GETDATA, size, object);
}


uint16_t
ptp_getthumb (PTPParams* params, uint32_t handle, 
			uint32_t size, PTPReq* object)
{
	PTPReq req;

	*(int *)(req.data)=htod32(handle);
	return ptp_transaction(params, &req, PTP_OC_GetThumb,
		PTP_DP_GETDATA, size, object);
}

/**
 * ptp_deleteobject:
 * params:	PTPParams*
 *		uint32_t handle		- object handle
 *		uint32_t ofc	- object format code
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
	PTP_DP_NODATA, 0, NULL);
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

	*(uint32_t *)(req.data)=htod32(*store);
	*(uint32_t *)(req.data+4)=htod32(*parenthandle);
	
	memcpy(req_oi.data, objectinfo, sizeof (PTPObjectInfo));
	
	ret= ptp_transaction(params, &req, PTP_OC_EK_SendFileObjectInfo,
		PTP_DP_SENDDATA, sizeof(PTPObjectInfo), &req_oi); 
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
		PTP_DP_SENDDATA, size, object);
}

