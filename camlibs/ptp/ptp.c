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

#define CR(p) {short ret = (p); if (ret != PTP_RC_OK) return (ret);}

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

	ptp_debug (params, "Sending request...");
	ret=params->write_func ((unsigned char *) req, PTP_REQ_LEN,
				 params->data);
	if (databuf==NULL) free (req);
	return ret;
}

#if 0
static short
ptp_senddata(PTPParams* params, PTPReq* databuf, short code)
{
	short ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;
	
	req->len = PTP_REQ_LEN;
	req->type = PTP_TYPE_DATA;
	req->code = code;
	req->trans_id = params->transaction_id;

	ret=params->write_func ((unsigned char *) req, PTP_REQ_LEN,
				 params->data);
	if (databuf==NULL) free (req);
	return ret;
}
#endif

static short
ptp_getdata(PTPParams* params, PTPReq* databuf)
{
	short ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;

	memset(req, 0, PTP_RESP_LEN);
	ret=params->read_func((unsigned char *) req, PTP_RESP_LEN,
				 params->data);
	if (databuf==NULL) free (req);
	return ret;
}

static short
ptp_getresp(PTPParams* params, PTPReq* databuf)
{
	short ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;

	memset(req, 0, PTP_RESP_LEN);
	ret=params->read_func((unsigned char*) req, PTP_RESP_LEN, params->data);
	if (databuf==NULL) free (req);
	return ret;
}

// Iniciates connection with device, opens session

#if 0
short
ptp_getdevinfo(PTPParams* params, PTPDedviceInfo* devinfo)
{
	short ret;
	PTPReq* req=malloc(sizeof(PTPReq));
	
	// Do GetDevInfo (we may use it for some camera_about)
 	if (devinfo==NULL) devinfo=malloc(sizeof(PTPDedviceInfo));
	memset(req, 0, PTP_RESP_LEN);
	ret=ptp_sendreq(params, req,PTP_OC_GetDevInfo);
	if (ret!=PTP_RC_OK) {
		ptp_error (params, "ptp_getdevinfo sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	ret=ptp_getdata(params, req);
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_DATA) || (req->code!=PTP_OC_GetDevInfo)) {
		ptp_error (params, "ptp_getdevinfo getting data");
		ptp_debug (params, "GetDevInfo data returned:\nlen=0x%8.8x "
			"type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
			req->len, req->type, req->code, req->trans_id);
		if (ret!=PTP_RC_OK) ret=PTP_ERROR_IO; else
		ret=(req->type==PTP_TYPE_DATA)?
			req->code:PTP_ERROR_DATA_EXPECTED;
		free(req);
		return ret;
	}
	memcpy(devinfo, req->data,sizeof(PTPDedviceInfo));
	ret=ptp_getresp(params, req);
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_RESP) || (req->code!=PTP_RC_OK)) {
		ptp_error (params, "ptp_getdevinfo getting resp");
		ptp_debug (params, "GetDevInfo resp:\nlen=0x%8.8x type=0x%4.4x "
			"code=0x%4.4x ID=0x%8.8x\n",
			req->len, req->type, req->code, req->trans_id);
		if (ret!=PTP_RC_OK) ret=PTP_ERROR_IO; else
			ret=(req->type==PTP_TYPE_RESP)?
				req->code:PTP_ERROR_RESP_EXPECTED;
		free(req);
		return ret;
	}
	free(req);
	return PTP_RC_OK;
}
#endif

/**
 * ptp_opensession:
 * @params: #PTPParams
 *
 * Establishes a new connection.
 *
 * Return values: Some PTP_RC_* code.
 **/
short
ptp_opensession(PTPParams* params, int session)
{
	short ret;
	PTPReq* req=malloc(sizeof(PTPReq));
	
	memset(req, 0, PTP_RESP_LEN);
	*(int *)(req->data)=session;
	ret=ptp_sendreq(params, req, PTP_OC_OpenSession);
	if (ret!=PTP_RC_OK) {
		ptp_error (params, "ptp_opensession sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	ret=ptp_getresp(params, req);
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_RESP) || (req->code!=PTP_RC_OK)) {
		ptp_error (params, "ptp_opensession getting resp");
#ifdef DEBUG
		ptp_error (params, "OpenSession resp:\nlen=0x%8.8x"
		" type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_RC_OK) ret=PTP_ERROR_IO; else
			ret=(req->type==PTP_TYPE_RESP)?
				req->code:PTP_ERROR_RESP_EXPECTED;
		free(req);
		return ret;
	}
	free(req);
	return PTP_RC_OK;
}

short
ptp_closesession(PTPParams* params)
{
	short ret;
	PTPReq* req=malloc(sizeof(PTPReq));
	
	memset(req, 0, PTP_RESP_LEN);
	ret=ptp_sendreq(params, req, PTP_OC_CloseSession);
	if (ret!=PTP_RC_OK) {
		ptp_error (params, "ptp_closesession sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	ret=ptp_getresp(params, req);
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_RESP) || (req->code!=PTP_RC_OK)) {
		ptp_error (params, "ptp_closesession getting resp");
#ifdef DEBUG
		ptp_error (params, "CloseSession resp:\nlen=0x%8.8x" 
		" type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_RC_OK) ret=PTP_ERROR_IO; else
			ret=(req->type==PTP_TYPE_RESP)?
				req->code:PTP_ERROR_RESP_EXPECTED;
		free(req);
		return ret;
	}
	free(req);
	return PTP_RC_OK;
}

short
ptp_getobjecthandles(PTPParams* params, PTPObjectHandles* objecthandles)
{
	short ret;
	PTPReq* req=malloc(sizeof(PTPReq));
	
	if (objecthandles==NULL) {
		free (req);
		return PTP_ERROR_BADPARAM;
	}
	memset(req, 0, PTP_RESP_LEN);
	*(int *)(req->data)=0xffffffff;   // XXX return from ALL stores
	ret=ptp_sendreq(params, req, PTP_OC_GetObjectHandles);
	if (ret!=PTP_RC_OK) {
		ptp_error (params, "ptp_getobjecthandles sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	ret=ptp_getdata(params, req);
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_DATA) ||
		(req->code!=PTP_OC_GetObjectHandles)) {
		ptp_error (params, "ptp_getobjecthandles getting resp");
#ifdef DEBUG
		ptp_error (params, "GetObjectHandles data returned:\nlen=0x%8.8x"
		" type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_RC_OK) ret=PTP_ERROR_IO; else
			ret=(req->type==PTP_TYPE_DATA)?
			req->code:PTP_ERROR_DATA_EXPECTED;
		free(req);
		return ret;
	}
	memcpy(objecthandles,req->data,sizeof(PTPObjectHandles));
	ret=ptp_getresp(params, req);
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_RESP) || (req->code!=PTP_RC_OK)) {
		 ptp_error (params, "ptp_getobjecthandles getting resp");
#ifdef DEBUG
		 ptp_error (params, "GetObjectHandles resp:\nlen=0x%8.8x"
		 " type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		 req->len, req->type, req->code, req->trans_id);
#endif
		 if (ret!=PTP_RC_OK) ret=PTP_ERROR_IO; else
			 ret=(req->type==PTP_TYPE_RESP)?
			 	req->code:PTP_ERROR_RESP_EXPECTED;
		free(req);
		return ret;
	}
	free(req);
	return PTP_RC_OK;
}

short
ptp_getobjectinfo(PTPParams* params, PTPObjectHandles* objecthandles,
			int n, PTPObjectInfo* objectinfo)
{
	short ret;
	if ((objecthandles==NULL)||(objectinfo==NULL))
		return PTP_ERROR_BADPARAM;

	{
	PTPReq* req=malloc(sizeof(PTPReq));
	
	memset(req, 0, PTP_RESP_LEN);
	*(int *)(req->data)=objecthandles->handler[n];
	ret=ptp_sendreq(params, req, PTP_OC_GetObjectInfo);
	if (ret!=PTP_RC_OK) {
		ptp_error (params, "ptp_getobjectinfo sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	ret=ptp_getdata(params, req);
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_DATA) ||
		(req->code!=PTP_OC_GetObjectInfo)) {
		ptp_error (params, "ptp_getobjectinfo getting data");
#ifdef DEBUG
		ptp_error (params, "GetObjectInfo data returned:\nlen=0x%8.8x"
		"type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_RC_OK) ret=PTP_ERROR_IO; else
			ret=(req->type==PTP_TYPE_DATA)?
			req->code:PTP_ERROR_DATA_EXPECTED;
		free(req);
		return ret;
	}
	memcpy(objectinfo, req->data, sizeof(PTPObjectInfo));

	ret=ptp_getresp(params, req);
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_RESP) ||
		(req->code!=PTP_RC_OK)) {
		ptp_error (params, "ptp_getobjetsinfo getting resp");
#ifdef DEBUG
		ptp_error (params, "PTP_OC_GetObjectInfo resp:\nlen=0x%8.8x"
		" type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_RC_OK) ret=PTP_ERROR_IO; else
			ret=(req->type==PTP_TYPE_RESP)?
				req->code:PTP_ERROR_RESP_EXPECTED;
		free(req);
		return ret;
	}
	free(req);
	}
	return PTP_RC_OK;
}

short
ptp_getobject(PTPParams* params, PTPObjectHandles* objecthandles, 
			PTPObjectInfo* objectinfo, int n,
			char* object)
{
	short ret;
	if ((objecthandles==NULL)||(objectinfo==NULL)||((object==NULL)))
		return PTP_ERROR_BADPARAM;

	{
	PTPReq* req=malloc(sizeof(PTPReq));
	PTPReq* ptr;
	
	memset(req, 0, PTP_RESP_LEN);
	*(int *)(req->data)=objecthandles->handler[n];
	ret=ptp_sendreq(params, req, PTP_OC_GetObject);
	if (ret!=PTP_RC_OK) {
		ptp_error (params, "ptp_getobject sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	{
	char* obj_tmp=malloc(objectinfo->ObjectCompressedSize+PTP_REQ_HDR_LEN);
	ptr=req; req=(PTPReq*)obj_tmp;
	ret=params->read_func((unsigned char *) req,
		objectinfo->ObjectCompressedSize+PTP_REQ_HDR_LEN,
		params->data);	
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_DATA) ||
		(req->code!=PTP_OC_GetObject)) {
		ptp_error (params, "ptp_getobject getting data");
#ifdef DEBUG
		ptp_error (params, "GetObject data returned:\nlen=0x%8.8x"
		"type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_RC_OK) ret=PTP_ERROR_IO; else
			 ret=(req->type==PTP_TYPE_DATA)?
			 req->code:PTP_ERROR_DATA_EXPECTED;
		free(obj_tmp);
		req=ptr;
		free(req);
		return ret;
	}
	memcpy(object, req->data, objectinfo->ObjectCompressedSize);
	free(obj_tmp);
	}
	req=ptr;
	ret=ptp_getresp(params, req);
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_RESP) ||
		(req->code!=PTP_RC_OK)) {
		ptp_error (params, "ptp_getobject getting resp");
#ifdef DEBUG
		ptp_error (params, "GetObject data returned:\nlen=0x%8.8x"
		"type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		free(req);
		return ret;
	}
	free(req);
	}
	return PTP_RC_OK;
}


short
ptp_getthumb(PTPParams* params, PTPObjectHandles* objecthandles, 
			PTPObjectInfo* objectinfo, int n,
			char* object)
{
	short ret;
	if ((objecthandles==NULL)||(objectinfo==NULL)||((object==NULL)))
		return PTP_ERROR_BADPARAM;

	{
	PTPReq* req=malloc(sizeof(PTPReq));
	PTPReq* ptr;
	
	memset(req, 0, PTP_RESP_LEN);
	*(int *)(req->data)=objecthandles->handler[n];
	ret=ptp_sendreq(params, req, PTP_OC_GetThumb);
	if (ret!=PTP_RC_OK) {
		ptp_error (params, "ptp_getobject sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	{
	char* obj_tmp=malloc(objectinfo->ObjectCompressedSize+PTP_REQ_HDR_LEN);
	ptr=req; req=(PTPReq*)obj_tmp;
	ret=params->read_func((unsigned char *) req,
		objectinfo->ObjectCompressedSize+PTP_REQ_HDR_LEN,
		params->data);	
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_DATA) ||
		(req->code!=PTP_OC_GetThumb)) {
		ptp_error (params, "ptp_getobject getting data");
#ifdef DEBUG
		ptp_error (params, "GetObject data returned:\nlen=0x%8.8x"
		"type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_RC_OK) ret=PTP_ERROR_IO; else
			 ret=(req->type==PTP_TYPE_DATA)?
			 req->code:PTP_ERROR_DATA_EXPECTED;
		free(obj_tmp);
		req=ptr;
		free(req);
		return ret;
	}
	memcpy(object, req->data, objectinfo->ObjectCompressedSize);
	free(obj_tmp);
	}
	req=ptr;
	ret=ptp_getresp(params, req);
	if ((ret!=PTP_RC_OK) ||
		(req->type!=PTP_TYPE_RESP) ||
		(req->code!=PTP_RC_OK)) {
		ptp_error (params, "ptp_getobject getting resp");
#ifdef DEBUG
		ptp_error (params, "GetObject data returned:\nlen=0x%8.8x"
		"type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		free(req);
		return ret;
	}
	free(req);
	}
	return PTP_RC_OK;
}
