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

PTPResult
ptp_getdevinfo(PTPParams* params, PTPReq* databuf)
{
	return ptp_sendreq(params, databuf, PTP_OC_GetDevInfo);
}

PTPResult
ptp_opensession(PTPParams* params, PTPReq* databuf)
{
	return ptp_sendreq(params, databuf, PTP_OC_OpenSession);
}

PTPResult
ptp_closesession(PTPParams* params, PTPReq* databuf)
{
	return ptp_sendreq(params, databuf, PTP_OC_CloseSession);
}

PTPResult
ptp_getobjectinfo(PTPParams* params, PTPReq* databuf)
{
	return ptp_sendreq(params, databuf, PTP_OC_GetObjectInfo);
}

PTPResult
ptp_getobjecthandles(PTPParams* params, PTPReq* databuf)
{
	return ptp_sendreq(params, databuf, PTP_OC_GetObjectHandles);
}

PTPResult
ptp_getobject(PTPParams* params, PTPReq* databuf)
{
	return ptp_sendreq(params, databuf, PTP_OC_GetObject);
}

PTPResult
ptp_resetdevice(PTPParams* params, PTPReq* databuf)
{
	return ptp_sendreq(params, databuf, PTP_OC_ResetDevice);
}

PTPResult
ptp_selftest(PTPParams* params, PTPReq* databuf)
{
	return ptp_sendreq(params, databuf, PTP_OC_SelfTest);
}

PTPResult
ptp_powerdown(PTPParams* params, PTPReq* databuf)
{
	return ptp_sendreq(params, databuf, PTP_OC_PowerDown);
}

// Iniciates connection with device, opens session

PTPResult
ptp_GetDevInfo(PTPParams* params, PTPReq* databuf)
{
	PTPResult ret;
	PTPReq* req=(databuf==NULL)?
		malloc(sizeof(PTPReq)):databuf;

	// Do GetDevInfo (we may use it for some camera_about)
	memset(req, 0, PTP_RESP_LEN);
	ret=ptp_getdevinfo(params, req);
	if (ret!=PTP_OK) {
		params->ptp_error("ptp_getdevinfo sending req");
		if (databuf==NULL) free(req);
		return ret;
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
		if (databuf==NULL) free(req);
		return ret;
	}
	{
	PTPReq* resp=malloc(sizeof(PTPReq));
	ret=ptp_getresp(params, resp);
	if ((ret!=PTP_OK) ||
		(resp->type!=PTP_TYPE_RESP) || (resp->code!=PTP_RC_OK)) {
		params->ptp_error("ptp_getdevinfo getting resp:");
#ifdef DEBUG
		params->ptp_error("GetDevInfo resp:\nlen=0x%8.8x type=0x%4.4x "
			"code=0x%4.4x ID=0x%8.8x\n",
			resp->len, resp->type, resp->code, resp->trans_id);
#endif
		free(resp);
		if (databuf==NULL)  free(req);
		return ret;
	}
	free(resp);
	}
	if (databuf==NULL) free(req);
	return ret;
}
