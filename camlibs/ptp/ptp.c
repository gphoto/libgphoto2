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

#include <stdlib.h>
#include "ptp.h"

PTPResult
ptp_sendreq(PTPParams* params, PTPReq* databuf, short code)
{
	PTPResult ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;
	
	req->len = PTP_REQ_LEN;
	req->type = PTP_TYPE_REQ;
	req->code = code;
	req->trans_id = params->id;
	params->id++;

	ret=params->io_write (req, PTP_REQ_LEN, params->io_data);
	if (databuf==NULL) free (req);
	return ret;
}

PTPResult
ptp_senddata(PTPParams* params, PTPReq* databuf, short code)
{
	PTPResult ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;
	
	req->len = PTP_REQ_LEN;
	req->type = PTP_TYPE_DATA;
	req->code = code;
	req->trans_id = params->id;

	ret=params->io_write (req, PTP_REQ_LEN, params->io_data);
	if (databuf==NULL) free (req);
	return ret;
}

PTPResult
ptp_getdata(PTPParams* params, PTPReq* databuf)
{
	PTPResult ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;

	memset(req, 0, PTP_RESP_LEN);
	ret=params->io_read(req, PTP_RESP_LEN, params->io_data);
	if (databuf==NULL) free (req);
	return ret;
}

PTPResult
ptp_getresp(PTPParams* params, PTPReq* databuf)
{
	PTPResult ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;

	memset(req, 0, PTP_RESP_LEN);
	ret=params->io_read(req, PTP_RESP_LEN, params->io_data);
	if (databuf==NULL) free (req);
	return ret;
}

// Iniciates connection with device, opens session

short
ptp_getdevinfo(PTPParams* params, PTPDedviceInfo* devinfo)
{
	short ret;
	PTPReq* req=malloc(sizeof(PTPReq));
	
	// Do GetDevInfo (we may use it for some camera_about)
 	if (devinfo==NULL) devinfo=malloc(sizeof(PTPDedviceInfo));
	memset(req, 0, PTP_RESP_LEN);
	ret=ptp_sendreq(params, req,PTP_OC_GetDevInfo);
	if (ret!=PTP_OK) {
		params->ptp_error("ptp_getdevinfo sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	ret=ptp_getdata(params, req);
	if ((ret!=PTP_OK) ||
		(req->type!=PTP_TYPE_DATA) || (req->code!=PTP_OC_GetDevInfo)) {
		params->ptp_error("ptp_getdevinfo getting data");
#ifdef DEBUG
		params->ptp_error("GetDevInfo data returned:\nlen=0x%8.8x "
			"type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
			req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_OK) ret=PTP_ERROR_IO; else
		ret=(req->type==PTP_TYPE_DATA)?
			req->code:PTP_ERROR_DATA_EXPECTED;
		free(req);
		return ret;
	}
	memcpy(devinfo, req->data,sizeof(PTPDedviceInfo));
	ret=ptp_getresp(params, req);
	if ((ret!=PTP_OK) ||
		(req->type!=PTP_TYPE_RESP) || (req->code!=PTP_RC_OK)) {
		params->ptp_error("ptp_getdevinfo getting resp");
#ifdef DEBUG
		params->ptp_error("GetDevInfo resp:\nlen=0x%8.8x type=0x%4.4x "
			"code=0x%4.4x ID=0x%8.8x\n",
			req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_OK) ret=PTP_ERROR_IO; else
			ret=(req->type==PTP_TYPE_RESP)?
				req->code:PTP_ERROR_RESP_EXPECTED;
		free(req);
		return ret;
	}
	free(req);
	return PTP_RC_OK;
}


short
ptp_opensession(PTPParams* params, int session)
{
	short ret;
	PTPReq* req=malloc(sizeof(PTPReq));
	
	memset(req, 0, PTP_RESP_LEN);
	*(int *)(req->data)=session;
	ret=ptp_sendreq(params, req, PTP_OC_OpenSession);
	if (ret!=PTP_OK) {
		params->ptp_error("ptp_opensession sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	ret=ptp_getresp(params, req);
	if ((ret!=PTP_OK) ||
		(req->type!=PTP_TYPE_RESP) || (req->code!=PTP_RC_OK)) {
		params->ptp_error("ptp_opensession getting resp");
#ifdef DEBUG
		params->ptp_error("OpenSession resp:\nlen=0x%8.8x"
		" type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_OK) ret=PTP_ERROR_IO; else
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
	if (ret!=PTP_OK) {
		params->ptp_error("ptp_closesession sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	ret=ptp_getresp(params, req);
	if ((ret!=PTP_OK) ||
		(req->type!=PTP_TYPE_RESP) || (req->code!=PTP_RC_OK)) {
		params->ptp_error("ptp_closesession getting resp");
#ifdef DEBUG
		params->ptp_error("CloseSession resp:\nlen=0x%8.8x" 
		" type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_OK) ret=PTP_ERROR_IO; else
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
	
	if (objecthandles==NULL)
		objecthandles=malloc(sizeof(PTPObjectHandles));
	memset(req, 0, PTP_RESP_LEN);
	*(int *)(req->data)=0xffffffff;   // XXX return from ALL stores
	ret=ptp_sendreq(params, req, PTP_OC_GetObjectHandles);
	if (ret!=PTP_OK) {
		params->ptp_error("ptp_getobjecthandles sending req");
		free(req);
		return PTP_ERROR_IO;
	}
	ret=ptp_getdata(params, req);
	if ((ret!=PTP_OK) ||
		(req->type!=PTP_TYPE_DATA) ||
		(req->code!=PTP_OC_GetObjectHandles)) {
		params->ptp_error("ptp_getobjecthandles getting resp");
#ifdef DEBUG
		params->ptp_error("GetObjectHandles data returned:\nlen=0x%8.8x"
		" type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		req->len, req->type, req->code, req->trans_id);
#endif
		if (ret!=PTP_OK) ret=PTP_ERROR_IO; else
			ret=(req->type==PTP_TYPE_DATA)?
			req->code:PTP_ERROR_DATA_EXPECTED;
		free(req);
		return ret;
	}
	memcpy(objecthandles,req->data,sizeof(PTPObjectHandles));
	ret=ptp_getresp(params, req);
	if ((ret!=PTP_OK) ||
		(req->type!=PTP_TYPE_RESP) || (req->code!=PTP_RC_OK)) {
		 params->ptp_error("ptp_getobjecthandles getting resp");
#ifdef DEBUG
		 params->ptp_error("GetObjectHandles resp:\nlen=0x%8.8x"
		 " type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
		 req->len, req->type, req->code, req->trans_id);
#endif
		 if (ret!=PTP_OK) ret=PTP_ERROR_IO; else
			 ret=(req->type==PTP_TYPE_RESP)?
			 	req->code:PTP_ERROR_RESP_EXPECTED;
		free(req);
		return ret;
	}
	free(req);
	return PTP_RC_OK;
}

short
ptp_getobjectsinfo(PTPParams* params, PTPObjectHandles* objecthandles,
			PTPObjectInfo** objectinfoarray)
{
	short ret,i;
	if (objecthandles==NULL)
		return PTP_ERROR_BADPARAM;

	{
	PTPReq* req=malloc(sizeof(PTPReq));
	
	memset(req, 0, PTP_RESP_LEN);
	objectinfoarray=malloc(sizeof(PTPObjectHandles *)*objecthandles->n);
	for (i=0;i<objecthandles->n;i++) {
		objectinfoarray[i]=malloc(sizeof(PTPObjectInfo));
		*(int *)(req->data)=objecthandles->handler[i];
		ret=ptp_sendreq(params, req, PTP_OC_GetObjectInfo);
		if (ret!=PTP_OK) {
			params->ptp_error("ptp_getobjectsinfo sending req");
			free(req);
			return PTP_ERROR_IO;
		}
		ret=ptp_getdata(params, req);
		if ((ret!=PTP_OK) ||
			(req->type!=PTP_TYPE_DATA) ||
			(req->code!=PTP_OC_GetObjectInfo)) {
			params->ptp_error("ptp_getobjectsinfo getting data");
#ifdef DEBUG
			params->ptp_error("GetObjectInfo data returned:\nlen=0x%8.8x"
			"type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
			req->len, req->type, req->code, req->trans_id);
#endif
			if (ret!=PTP_OK) ret=PTP_ERROR_IO; else
				ret=(req->type==PTP_TYPE_DATA)?
				req->code:PTP_ERROR_DATA_EXPECTED;
			free(req);
			// XXX no objectinfoarray cleanup!!!
			return ret;
		}
		memcpy(objectinfoarray[i], req->data, sizeof(PTPObjectInfo));

		ret=ptp_getresp(params, req);
		if ((ret!=PTP_OK) ||
			(req->type!=PTP_TYPE_RESP) ||
			(req->code!=PTP_RC_OK)) {
			params->ptp_error("ptp_getobjectsinfo getting resp");
#ifdef DEBUG
			params->ptp_error("PTP_OC_GetObjectInfo resp:\nlen=0x%8.8x"
			" type=0x%4.4x code=0x%4.4x ID=0x%8.8x\n",
			req->len, req->type, req->code, req->trans_id);
#endif
			if (ret!=PTP_OK) ret=PTP_ERROR_IO; else
				ret=(req->type==PTP_TYPE_RESP)?
					req->code:PTP_ERROR_RESP_EXPECTED;
			free(req);
			// XXX no objectinfoarray cleanup!!!
			return ret;
		}
		// Get next ObjectInfo now
	}
	free(req);
	}
	return PTP_RC_OK;
}

