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

#define CHECK_PTP_RC(result)	{short r=(result); if (r!=PTP_RC_OK) return r;}
#define CHECK_PTP_RC_free(result, free_ptr) {short r=(result); if (r!=PTP_RC_OK) {return r; free(free_ptr);}}

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

#define PTP_REQ_LEN                     30
#define PTP_REQ_HDR_LEN                 (2*sizeof(int)+2*sizeof(short))
#define PTP_RESP_LEN                    sizeof(PTPReq)

typedef struct _PTPReq PTPReq;
struct _PTPReq {
	int len;
	short type;
	short code;
	int trans_id;
	char data[PTP_REQ_DATALEN];
};

static short
ptp_sendreq(PTPParams* params, PTPReq* databuf, short code)
{
	short ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;
	
	req->len = PTP_REQ_LEN;
	req->type = PTP_TYPE_REQ;
	req->code = code;
	req->trans_id = params->transaction_id;
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

#if 0
static short
ptp_senddata(PTPParams* params, PTPReq* databuf, short code)
{
}
#endif

static short
ptp_getdata(PTPParams* params, PTPReq* req, short code,
		unsigned int readlen)
{
	short ret;
	if (req==NULL) return PTP_ERROR_BADPARAM; 

	memset(req, 0, readlen);
	ret=params->read_func((unsigned char *) req, readlen, params->data);

	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
	} else
	if (req->type!=PTP_TYPE_DATA) {
		ret = PTP_ERROR_DATA_EXPECTED;
	} else
	if (req->code!=code) {
		ret = req->code;
	}
	if (ret!=PTP_RC_OK) 
		ptp_error (params,
		"request code 0x%4.4x getting data error 0x%4.4x", code, ret);
	return ret;
}

static short
ptp_getresp(PTPParams* params, PTPReq* databuf, short code)
{
	short ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;

	memset(req, 0, PTP_RESP_LEN);
	ret=params->read_func((unsigned char*) req, PTP_RESP_LEN, params->data);

	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
	} else
	if (req->type!=PTP_TYPE_RESP) {
		ret = PTP_ERROR_RESP_EXPECTED;
	} else
	if (req->code!=code) {
		ret = req->code;
	}
	if (ret!=PTP_RC_OK)
		ptp_error (params,
		"request code 0x%4x getting resp error 0x%4x", code, ret);
	if (databuf==NULL) free (req);
	return ret;
}

// Transaction sequence description
#define PTP_DP_NODATA		0x00	// No Data Phase
#define PTP_DP_SENDDATA		0x01	// sending data
#define PTP_DP_GETDATA		0x02	// geting data

short
ptp_transaction(PTPParams* params, PTPReq** req, unsigned short code,
			unsigned short dataphase, unsigned int datalen)
{
	if ((params==NULL) || (*req==NULL)) 
		return PTP_ERROR_BADPARAM;
	CHECK_PTP_RC(ptp_sendreq(params, *req, code));
	switch (dataphase) {
		case PTP_DP_SENDDATA:
			// XXX unimplemented
			return PTP_ERROR_BADPARAM;
			break;
		case PTP_DP_GETDATA:
			if (datalen>PTP_REQ_DATALEN) {
				*req=realloc(*req, datalen+PTP_REQ_HDR_LEN);
				if (*req==NULL) {
					ptp_error(params, "realloc while 0x%4x transaction error", code);
					return PTP_RC_GeneralError;
				}
			}
			datalen=datalen+PTP_REQ_HDR_LEN;
			CHECK_PTP_RC(ptp_getdata(params, *req, code, datalen));
			break;
		case PTP_DP_NODATA:
			break;
		default:
		return PTP_ERROR_BADPARAM;
	}
	{
		PTPReq* resp=malloc(sizeof(PTPReq));
		CHECK_PTP_RC_free(ptp_getresp(params, resp, code), resp);
		free (resp);
	}
	return PTP_RC_OK;
}

#if 0
// Do GetDevInfo (we may use it for some camera_about)
short
ptp_getdevinfo(PTPParams* params, PTPDedviceInfo* devinfo)
{
}
#endif

/**
 * ptp_opensession:
 * params:	PTPParams*
 * 		int session		- session number
 *
 * Establishes a new session.
 *
 * Return values: Some PTP_RC_* code.
 **/
short
ptp_opensession(PTPParams* params, int session)
{
	PTPReq req;
	PTPReq* ptr=&req;
	
	*(int *)(req.data)=session;

	return ptp_transaction(params, &ptr, PTP_OC_OpenSession,
		PTP_DP_NODATA, 0);
}

/**
 * ptp_closesession:
 * params:	PTPParams*
 *
 * Closes session.
 *
 * Return values: Some PTP_RC_* code.
 **/
short
ptp_closesession(PTPParams* params)
{
	PTPReq req;
	PTPReq* ptr=&req;

	return ptp_transaction(params, &ptr, PTP_OC_CloseSession,
		PTP_DP_NODATA, 0);
}

/**
 * ptp_getobjecthandles:
 * params:	PTPParams*
 *		PTPObjectHandles*	- pointer to structute
 *		unsigned int store	- StoreID
 *
 * Fills objecthandles with structure returned by device.
 *
 * Return values: Some PTP_RC_* code.
 **/
// XXX still ObjectFormatCode and ObjectHandle of Assiciation NOT
//     IMPLEMENTED (parameter 2 and 3)
short
ptp_getobjecthandles(PTPParams* params, PTPObjectHandles* objecthandles,
			unsigned int store)
{
	short ret;
	PTPReq req;
	PTPReq* ptr=&req;
	
	if (objecthandles==NULL)
		return PTP_ERROR_BADPARAM;

	*(int *)(req.data)=store;

	ret=ptp_transaction(params, &ptr, PTP_OC_GetObjectHandles,
		PTP_DP_GETDATA, sizeof(PTPObjectHandles));
	memcpy(objecthandles, req.data, sizeof(PTPObjectHandles));
	return ret;
}

short
ptp_getobjectinfo(PTPParams* params, unsigned int handler,
			PTPObjectInfo* objectinfo)
{
	short ret;
	PTPReq req;
	PTPReq* ptr=&req;

	if (objectinfo==NULL)
		return PTP_ERROR_BADPARAM;

	*(int *)(req.data)=handler;
	ret=ptp_transaction(params, &ptr, PTP_OC_GetObjectInfo,
		PTP_DP_GETDATA, sizeof(PTPObjectInfo));
	memcpy(objectinfo, req.data, sizeof(PTPObjectInfo));
	return ret;
}

short
ptp_getobject(PTPParams* params, unsigned int handler, 
			unsigned int size, char* object)
{
	short ret;
	PTPReq* req=malloc(sizeof(PTPReq));

	if (object==NULL)
		return PTP_ERROR_BADPARAM;

	memset(req, 0, sizeof(PTPReq));
	*(int *)(req->data)=handler;
	ret=ptp_transaction(params, &req, PTP_OC_GetObject,
		PTP_DP_GETDATA, size);
	memcpy(object, req->data, size);
	free(req);
	return ret;
}


short
ptp_getthumb(PTPParams* params, unsigned int handler, 
			unsigned int size, char* object)
{
	short ret;
	PTPReq* req=malloc(sizeof(PTPReq));
	if (object==NULL)
		return PTP_ERROR_BADPARAM;

	memset(req, 0, sizeof(PTPReq));
	*(int *)(req->data)=handler;
	ret=ptp_transaction(params, &req, PTP_OC_GetThumb,
		PTP_DP_GETDATA, size);
	memcpy(object, req->data, size);
	free(req);
	return ret;
}

/**
 * ptp_deleteobject:
 * params:	PTPParams*
 *
 *		unsigned int handler	- object handler
 *		unsigned int ofc	- object format code
 * 
 * Deletes desired objects.
 *
 * Return values: Some PTP_RC_* code.
 **/
short
ptp_deleteobject(PTPParams* params, unsigned int handler,
			unsigned int ofc)
{
	PTPReq req;
	PTPReq* ptr=&req;
	*(int *)(req.data)=handler;
	*(int *)(req.data+4)=ofc;

	return ptp_transaction(params, &ptr, PTP_OC_DeleteObject,
	PTP_DP_NODATA, 0);
}

void
ptp_getobjectfilename (PTPObjectInfo* objectinfo, char* filename) {
	int i;

	memset (filename, 0, MAXFILELEN);

	for (i=0;i<objectinfo->filenamelen&&i<MAXFILELEN;i++) {
		filename[i]=objectinfo->data[i*2];
	}
}

void
ptp_getobjectcapturedate (PTPObjectInfo* objectinfo, char* date) {
	int i;
	int p=objectinfo->filenamelen;

	memset (date, 0, MAXFILELEN);

	for (i=0;i<objectinfo->data[p*2]&&i<MAXFILELEN;i++) {
		date[i]=objectinfo->data[(i+p)*2+1];
	}
}
