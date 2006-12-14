/* ptp.c
 *
 * Copyright (C) 2001-2004 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2006 Marcus Meissner <marcus@jet.franken.de>
 * Copyright (C) 2006 Linus Walleij <triad@df.lth.se>
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
#include <unistd.h>

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

/* Used for file transactions */
#define FILE_BUFFER_SIZE 0x10000

uint16_t
ptp_usb_senddata (PTPParams* params, PTPContainer* ptp,
		  unsigned char *data, unsigned int size,
		  int from_fd)
{
	uint16_t ret;
	int wlen, datawlen;
	size_t written;
	PTPUSBBulkContainer usbdata;

	/* build appropriate USB container */
	usbdata.length	= htod32(PTP_USB_BULK_HDR_LEN+size);
	usbdata.type	= htod16(PTP_USB_CONTAINER_DATA);
	usbdata.code	= htod16(ptp->Code);
	usbdata.trans_id= htod32(ptp->Transaction_ID);

	if (params->split_header_data) {
		datawlen = 0;
		wlen = PTP_USB_BULK_HDR_LEN;
	} else {
		/* For all camera devices. */
		datawlen = (size<PTP_USB_BULK_PAYLOAD_LEN)?size:PTP_USB_BULK_PAYLOAD_LEN;
		wlen = PTP_USB_BULK_HDR_LEN + datawlen;
		if (from_fd == -1) {
			memcpy(usbdata.payload.data, data, datawlen);
		} else {
			written = read(from_fd, usbdata.payload.data, datawlen);
			if (written != datawlen)
				return PTP_ERROR_IO;
		}
	}
	/* send first part of data */
	ret = params->write_func((unsigned char *)&usbdata, wlen, params->data);
	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
/*		ptp_error (params,
		"PTP: request code 0x%04x sending data error 0x%04x",
			ptp->Code,ret);*/
		return ret;
	}
	if (size <= datawlen) return ret;
	/* if everything OK send the rest */
	if (from_fd == -1) {
		ret=params->write_func (data + datawlen, size - datawlen, params->data);
	} else {
		uint32_t bytes_to_transfer;
		uint32_t bytes_left_to_transfer;
		void *temp_buf;

		written = 0;
		bytes_left_to_transfer = size-datawlen;

		temp_buf = malloc(FILE_BUFFER_SIZE);
		if (temp_buf == NULL)
			return PTP_ERROR_IO;

		ret = PTP_RC_OK;
		while(bytes_left_to_transfer > 0) {
			if (bytes_left_to_transfer > FILE_BUFFER_SIZE) {
				bytes_to_transfer = FILE_BUFFER_SIZE;
			} else {
				bytes_to_transfer = bytes_left_to_transfer;
			}
			written = read(from_fd, temp_buf, bytes_to_transfer);
			if (written != bytes_to_transfer) {
				ret = PTP_ERROR_IO;
				break;
			}
			ret=params->write_func (temp_buf, bytes_to_transfer, params->data);
			bytes_left_to_transfer -= bytes_to_transfer;
		}
		free(temp_buf);
		
	}
	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
/*		ptp_error (params,
		"PTP: request code 0x%04x sending data error 0x%04x",
			ptp->Code,ret); */
	}
	return ret;
}

static uint16_t ptp_usb_getpacket(PTPParams *params,
		PTPUSBBulkContainer *packet, unsigned int *rlen)
{
	/* read the header and potentially the first data */
	if (params->response_packet_size > 0) {
		/* If there is a buffered packet, just use it. */
		memcpy(packet, params->response_packet, params->response_packet_size);
		*rlen = params->response_packet_size;
		free(params->response_packet);
		params->response_packet = NULL;
		params->response_packet_size = 0;
		/* Here this signifies a "virtual read" */
		return PTP_RC_OK;
	} else {
		return params->read_func((unsigned char *)packet,
					 sizeof(*packet), params->data, rlen);
	}
}

uint16_t
ptp_usb_getdata (PTPParams* params, PTPContainer* ptp,
                 unsigned char **data, unsigned int *readlen,
                 int to_fd)
{
	uint16_t ret;
	PTPUSBBulkContainer usbdata;

	PTP_CNT_INIT(usbdata);

	if (to_fd == -1 &&  *data != NULL)
		return PTP_ERROR_BADPARAM;

	do {
		unsigned int len, rlen;

		ret = ptp_usb_getpacket(params, &usbdata, &rlen);
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
		if (usbdata.length == 0xffffffffU) {
			/* This only happens for MTP_GetObjPropList */
			uint32_t	tsize = PTP_USB_BULK_HS_MAX_PACKET_LEN;
			unsigned char	*tdata = malloc(tsize);
			uint32_t	curoff = 0;

			while (1) {
				ret=params->read_func(tdata+curoff,
						PTP_USB_BULK_HS_MAX_PACKET_LEN, params->data, &rlen);
				if (ret!=PTP_RC_OK) {
					ret = PTP_ERROR_IO;
					break;
				}
				if (rlen < PTP_USB_BULK_HS_MAX_PACKET_LEN) {
					tsize += rlen;
					break;
				}
				tsize += PTP_USB_BULK_HS_MAX_PACKET_LEN;
				curoff+= PTP_USB_BULK_HS_MAX_PACKET_LEN;
				tdata	= realloc(tdata, tsize);
			}
			if (to_fd == -1) {
				*data = tdata;
			} else {
				write (to_fd, tdata, tsize);
				free (tdata);
			}
			return PTP_RC_OK;
		}
		if (rlen > dtoh32(usbdata.length)) {
			/*
			 * Buffer the surplus response packet if it is >=
			 * PTP_USB_BULK_HDR_LEN
			 * (i.e. it is probably an entire package)
			 * else discard it as erroneous surplus data.
			 * This will even work if more than 2 packets appear
			 * in the same transaction, they will just be handled
			 * iteratively.
			 *
			 * Marcus observed stray bytes on iRiver devices;
			 * these are still discarded.
			 */
			unsigned int packlen = dtoh32(usbdata.length);
			unsigned int surplen = rlen - packlen;

			if (surplen >= PTP_USB_BULK_HDR_LEN) {
				params->response_packet = malloc(surplen);
				memcpy(params->response_packet,
				       (uint8_t *) &usbdata + packlen, surplen);
				params->response_packet_size = surplen;
			} else {
				ptp_debug (params, "ptp2/ptp_usb_getdata: read %d bytes too much, expect problems!", rlen - dtoh32(usbdata.length));
			}
			rlen = packlen;
		}

		/* For most PTP devices rlen is 512 == sizeof(usbdata)
		 * here. For MTP devices splitting header and data it might
		 * be 12.
		 */
		/* Evaluate full data length. */
		len=dtoh32(usbdata.length)-PTP_USB_BULK_HDR_LEN;

		/* autodetect split header/data MTP devices */
		if (dtoh32(usbdata.length) > 12 && (rlen==12))
			params->split_header_data = 1;

		if (to_fd == -1) {
			/* Allocate memory for data. */
			*data=calloc(len,1);
			if (!*data) {
                                ptp_error (params, "PTP: Out of memory on allocating %d bytes.", len);
                                return PTP_ERROR_IO;
			}
			if (readlen)
				*readlen = len;

			/* Copy first part of data to 'data' */
			memcpy(*data,usbdata.payload.data,rlen - PTP_USB_BULK_HDR_LEN);

			/* Is that all of data? */
			if (len+PTP_USB_BULK_HDR_LEN<=rlen) break;

			/* If not read the rest of it. */
			ret=params->read_func(((unsigned char *)(*data))+
					      rlen - PTP_USB_BULK_HDR_LEN,
					      len-(rlen - PTP_USB_BULK_HDR_LEN),
					      params->data, &rlen);
			if (ret!=PTP_RC_OK) {
				ret = PTP_ERROR_IO;
				break;
			}
		} else {
			uint32_t bytes_to_write, written;
			uint32_t bytes_left_to_transfer;
			void *temp_buf;
						
			if (readlen)
				*readlen = len;

			bytes_to_write = rlen - PTP_USB_BULK_HDR_LEN;

			ret = write(to_fd, usbdata.payload.data, bytes_to_write);
			if (ret != bytes_to_write) {
				ret = PTP_ERROR_IO;
				break;
			}

			if (len + PTP_USB_BULK_HDR_LEN <= rlen)
				break;
			
			temp_buf = malloc(FILE_BUFFER_SIZE);
			if (temp_buf == NULL) {
				ret = PTP_ERROR_IO;
				break;
			}

			ret = PTP_RC_OK;				
			bytes_left_to_transfer = len - (rlen - PTP_USB_BULK_HDR_LEN);

			while (bytes_left_to_transfer > 0) {
				bytes_to_write = ((bytes_left_to_transfer > FILE_BUFFER_SIZE) ?
						  FILE_BUFFER_SIZE : bytes_left_to_transfer);
				
				ret = params->read_func(temp_buf,
							bytes_to_write,
							params->data, &rlen);

				if (ret != PTP_RC_OK) {
					ret = PTP_ERROR_IO;
					break;
				}

				written = write(to_fd, temp_buf, bytes_to_write);
				if (written != bytes_to_write) {
					ret = PTP_ERROR_IO;
					break;
				} else {
					ret = PTP_RC_OK;
				}

				bytes_left_to_transfer -= bytes_to_write;
			}

			free(temp_buf);

			if (ret != PTP_RC_OK)
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
	ret = ptp_usb_getpacket(params, &usbresp, &rlen);

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
_ptp_transaction (PTPParams* params, PTPContainer* ptp, 
		  uint16_t flags, unsigned int sendlen, unsigned char** data,
		  int fd, unsigned int *recvlen)
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
			*data, sendlen, fd));
		break;
	case PTP_DP_GETDATA:
		CHECK_PTP_RC(params->getdata_func(params, ptp,
			(unsigned char**)data, recvlen, fd));
		break;
	case PTP_DP_NODATA:
		break;
	default:
		return PTP_ERROR_BADPARAM;
	}
	/* get response */
	CHECK_PTP_RC(params->getresp_func(params, ptp));
	if (ptp->Transaction_ID != params->transaction_id-1) {
		ptp_error (params,
			"PTP: Sequence number mismatch %d vs expected %d.",
			ptp->Transaction_ID, params->transaction_id-1
		);
		return PTP_ERROR_BADPARAM;
	}
	return ptp->Code;
}

static uint16_t
ptp_transaction (PTPParams* params, PTPContainer* ptp, 
		 uint16_t flags, unsigned int sendlen, unsigned char** data,
		 unsigned int *recvlen)
{
	return _ptp_transaction(params, ptp, flags, sendlen, data, -1, recvlen);
}

static uint16_t
ptp_transaction_fd (PTPParams* params, PTPContainer* ptp, 
		      uint16_t flags, unsigned int sendlen, 
		      int fd, unsigned int *recvlen)
{
	/*
	 * This dummy data needed since _ptp_transaction()
	 * will dereference the data argument
	 */
	unsigned char *dummydata = NULL;
	
	return _ptp_transaction(params, ptp, flags, sendlen, &dummydata, fd, recvlen);
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
			"PTP: reading event an error 0x%04x occurred", ret);
		ret = PTP_ERROR_IO;
		/* reading event error is nonfatal (for example timeout) */
	}
	/* Only do the additional reads for "events". Canon IXUS 2 likes to
	 * send unrelated data.
	 */
	if (dtoh16(usbevent.type) == PTP_USB_CONTAINER_EVENT) {
		while (dtoh32(usbevent.length) > rlen) {
			unsigned int newrlen = 0;

			ret=params->check_int_fast_func(((unsigned char*)&usbevent)+rlen,
				dtoh32(usbevent.length)-rlen,params->data,&newrlen
			);
			if (ret != PTP_RC_OK)
				break;
			rlen+=newrlen;
		}
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
	unsigned int len;
	PTPContainer ptp;
	unsigned char* di=NULL;

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
	/* zero out response packet buffer */
	params->response_packet = NULL;
	params->response_packet_size = 0;
	
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

	/* free any dangling response packet */
	if (params->response_packet_size > 0) {
		free(params->response_packet);
		params->response_packet = NULL;
		params->response_packet_size = 0;
	}
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
	unsigned int len;
	unsigned char* sids=NULL;

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
	unsigned char* si=NULL;
	unsigned int len;

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
 * ptp_formatstore:
 * params:	PTPParams*
 *              storageid		- StorageID
 *
 * Formats the storage on the device.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_formatstore (PTPParams* params, uint32_t storageid)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_FormatStore;
	ptp.Param1=storageid;
	ptp.Param2=PTP_FST_Undefined;
	ptp.Nparam=2;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
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
	unsigned char* oh=NULL;
	unsigned int len;

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
 * ptp_getnumobjects:
 * params:	PTPParams*
 *		storage			- StorageID
 *		objectformatcode	- ObjectFormatCode (optional)
 *		associationOH		- ObjectHandle of Association for
 *					  wich a list of children is desired
 *					  (optional)
 *		numobs			- pointer to uint32_t that takes number of objects
 *
 * Fills numobs with number of objects on device.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getnumobjects (PTPParams* params, uint32_t storage,
			uint32_t objectformatcode, uint32_t associationOH,
			uint32_t* numobs)
{
	uint16_t ret;
	PTPContainer ptp;
	int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObjectHandles;
	ptp.Param1=storage;
	ptp.Param2=objectformatcode;
	ptp.Param3=associationOH;
	ptp.Nparam=3;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
	if (ret == PTP_RC_OK) {
		if (ptp.Nparam >= 1)
			*numobs = ptp.Param1;
		else
			ret = PTP_RC_GeneralError;
	}
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
	unsigned char* oi=NULL;
	unsigned int len;

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
ptp_getobject (PTPParams* params, uint32_t handle, unsigned char** object)
{
	PTPContainer ptp;
	unsigned int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObject;
	ptp.Param1=handle;
	ptp.Nparam=1;
	len=0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, &len);
}

/**
 * ptp_getobject_tofd:
 * params:	PTPParams*
 *		handle			- Object handle
 *		fd                      - File descriptor to write() to
 *
 * Get object 'handle' from device and write the data to the 
 * given file descriptor.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobject_tofd (PTPParams* params, uint32_t handle, int fd)
{
	PTPContainer ptp;
	unsigned int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObject;
	ptp.Param1=handle;
	ptp.Nparam=1;
	len=0;
	return ptp_transaction_fd(params, &ptp, PTP_DP_GETDATA, 0, fd, &len);
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
			uint32_t maxbytes, unsigned char** object)
{
	PTPContainer ptp;
	unsigned int len;

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
ptp_getthumb (PTPParams* params, uint32_t handle, unsigned char** object)
{
	PTPContainer ptp;
	unsigned int len;

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
	unsigned char* oidata=NULL;
	uint32_t size;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SendObjectInfo;
	ptp.Param1=*store;
	ptp.Param2=*parenthandle;
	ptp.Nparam=2;
	
	size=ptp_pack_OI(params, objectinfo, &oidata);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &oidata, NULL); 
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
ptp_sendobject (PTPParams* params, unsigned char* object, uint32_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SendObject;
	ptp.Nparam=0;

	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &object, NULL);
}

/**
 * ptp_sendobject_fromfd:
 * params:	PTPParams*
 *		fd                      - File descriptor to read() object from
 *              uint32_t size           - File/object size
 *
 * Sends object from file descriptor by consecutive reads from this
 * descriptor.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_sendobject_fromfd (PTPParams* params, int fd, uint32_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SendObject;
	ptp.Nparam=0;

	return ptp_transaction_fd(params, &ptp, PTP_DP_SENDDATA, size, fd, NULL);
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
	unsigned int len;
	unsigned char* dpd=NULL;

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
	unsigned int len;
	int offset;
	unsigned char* dpv=NULL;


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
	unsigned char* dpv=NULL;

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
	unsigned char* oidata=NULL;
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
 * ptp_ek_getserial:
 * params:	PTPParams*
 *		char**	serial		- contains the serial number of the camera
 *		uint32_t* size		- contains the string length
 *		
 * Gets the serial number from the device. (ptp serial)
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_ek_getserial (PTPParams* params, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_EK_GetSerial;
	ptp.Nparam = 0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

/**
 * ptp_ek_setserial:
 * params:	PTPParams*
 *		char*	serial		- contains the new serial number
 *		uint32_t size		- string length
 *		
 * Sets the serial number of the device. (ptp serial)
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_ek_setserial (PTPParams* params, unsigned char *data, unsigned int size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_EK_SetSerial;
	ptp.Nparam = 0;
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL); 
}

/* unclear what it does yet */
uint16_t
ptp_ek_9007 (PTPParams* params, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code   = 0x9007;
	ptp.Nparam = 0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

/* unclear what it does yet */
uint16_t
ptp_ek_9009 (PTPParams* params, uint32_t *p1, uint32_t *p2)
{
	PTPContainer	ptp;
	uint16_t	ret;

	PTP_CNT_INIT(ptp);
	ptp.Code   = 0x9009;
	ptp.Nparam = 0;
	ret = ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL); 
	*p1 = ptp.Param1;
	*p2 = ptp.Param2;
	return ret;
}

/* unclear yet, but I guess it returns the info from 9008 */
uint16_t
ptp_ek_900c (PTPParams* params, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code   = 0x900c;
	ptp.Nparam = 0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
	/* returned data is 16bit,16bit,32bit,32bit */
}

/**
 * ptp_ek_settext:
 * params:	PTPParams*
 *		PTPEKTextParams*	- contains the texts to display.
 *		
 * Displays the specified texts on the TFT of the camera.
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_ek_settext (PTPParams* params, PTPEKTextParams *text)
{
	PTPContainer ptp;
	uint16_t ret;
	unsigned int size;
	unsigned char *data;

	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_EK_SetText;
	ptp.Nparam = 0;
	if (0 == (size = ptp_pack_EK_text(params, text, &data)))
		return PTP_ERROR_BADPARAM;
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL); 
	free(data);
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
ptp_ek_sendfileobject (PTPParams* params, unsigned char* object, uint32_t size)
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
 * ptp_canon_getpartialobjectinfo:
 * params:	PTPParams*
 *		uint32_t handle		- ObjectHandle
 *		uint32_t p2 		- Not fully understood parameter
 *					  0 - returns full size
 *					  1 - returns thumbnail size (or EXIF?)
 * 
 * Gets form the responder the size of the specified object.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : uint32_t* size	- The object size
 *		  uint32_t* rp2		- Still unknown return parameter
 *                                        (perhaps upper 32bit of size)
 *
 *
 **/
uint16_t
ptp_canon_getpartialobjectinfo (PTPParams* params, uint32_t handle, uint32_t p2, 
			uint32_t* size, uint32_t* rp2) 
{
	uint16_t ret;
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetPartialObjectInfo;
	ptp.Param1=handle;
	ptp.Param2=p2;
	ptp.Nparam=2;
	ret=ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
	*size=ptp.Param1;
	*rp2=ptp.Param2;
	return ret;
}

/**
 * ptp_canon_get_mac_address:
 * params:	PTPParams*
 *					  value 0 works.
 * Gets the MAC address of the wireless transmitter.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : unsigned char* mac	- The MAC address
 *
 **/
uint16_t
ptp_canon_get_mac_address (PTPParams* params, unsigned char **mac)
{
	PTPContainer ptp;
	unsigned int size = 0;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetMACAddress;
	ptp.Nparam=0;
	*mac = NULL;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, mac, &size);
}

/**
 * ptp_canon_get_directory:
 * params:	PTPParams*

 * Gets the full directory of the camera.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : PTPObjectHandles        *handles	- filled out with handles
 * 		  PTPObjectInfo           **oinfos	- allocated array of PTP Object Infos
 * 		  uint32_t                **flags	- allocated array of CANON Flags
 *
 **/
uint16_t
ptp_canon_get_directory (PTPParams* params,
	PTPObjectHandles	*handles,
	PTPObjectInfo		**oinfos,	/* size(handles->n) */
	uint32_t		**flags		/* size(handles->n) */
) {
	PTPContainer	ptp;
	unsigned char	*dir = NULL;
	unsigned int	size = 0;
	uint16_t	ret;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetDirectory;
	ptp.Nparam=0;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dir, &size);
	if (ret != PTP_RC_OK)
		return ret;
	ret = ptp_unpack_canon_directory(params, dir, ptp.Param1, handles, oinfos, flags);
	free (dir);
	return ret;
}

/**
 * ptp_canon_setobjectarchive:
 *
 * params:	PTPParams*
 *		uint32_t	objectid
 *		uint32_t	flags
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_setobjectarchive (PTPParams* params, uint32_t oid, uint32_t flags)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_SetObjectArchive;
	ptp.Nparam=2;
	ptp.Param1=oid;
	ptp.Param2=flags;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
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
 * ptp_canon_initiate_direct_transfer:
 * params:	PTPParams*
 *              uint32_t *out
 * 
 * Switches the camera display to on and lets the user
 * select what to transfer. Sends a 0xc011 event when started 
 * and 0xc013 if direct transfer aborted.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_initiate_direct_transfer (PTPParams* params, uint32_t *out)
{
	PTPContainer ptp;
	uint16_t ret;

	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_CANON_InitiateDirectTransferEx2;
	ptp.Nparam = 1;
	ptp.Param1 = 0xf;
	ret = ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
	if ((ret == PTP_RC_OK) && (ptp.Nparam>0))
		*out = ptp.Param1;
	return ret;
}

/**
 * ptp_canon_get_target_handles:
 * params:	PTPParams*
 *              PTPCanon_directtransfer_entry **out
 *              unsigned int *outsize
 * 
 * Retrieves direct transfer entries specifying the images to transfer
 * from the camera (to be retrieved after 0xc011 event).
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_get_target_handles (PTPParams* params,
	PTPCanon_directtransfer_entry **entries, unsigned int *cnt)
{
	PTPContainer ptp;
	uint16_t ret;
	unsigned char *out = NULL, *cur;
	int i;
	unsigned int size;
	
	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_CANON_GetTargetHandles;
	ptp.Nparam = 0;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &out, &size);
	if (ret != PTP_RC_OK)
		return ret;
	*cnt = dtoh32a(out);
	*entries = malloc(sizeof(PTPCanon_directtransfer_entry)*(*cnt));
	cur = out+4;
	for (i=0;i<*cnt;i++) {
		unsigned char len;
		(*entries)[i].oid = dtoh32a(cur);
		(*entries)[i].str = ptp_unpack_string(params, cur, 4, &len);
		cur += 4+(cur[4]*2+1);
	}
	free (out);
	return PTP_RC_OK;
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
 * ptp_canon_aeafawb:
 * params:	PTPParams*
 * 		uint32_t p1 	- Yet unknown parameter,
 * 				  value 7 works
 * 
 * Called AeAfAwb (auto exposure, focus, white balance)
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_aeafawb (PTPParams* params, uint32_t p1)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_DoAeAfAwb;
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
	unsigned char *evdata = NULL;
	unsigned int len;
	
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

uint16_t
ptp_canon_9012 (PTPParams* params)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=0x9012;
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
				uint32_t pos, unsigned char** block, 
				uint32_t* readnum)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char *data=NULL;
	unsigned int len;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetPartialObjectEx;
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
ptp_canon_getviewfinderimage (PTPParams* params, unsigned char** image, uint32_t* size)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned int len;
	
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
	unsigned char* data=NULL;
	unsigned int len;
	
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
 * ptp_canon_getobjectinfo:
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
ptp_canon_getobjectinfo (PTPParams* params, uint32_t store, uint32_t p2, 
			    uint32_t parent, uint32_t handle, 
			    PTPCANONFolderEntry** entries, uint32_t* entnum)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char *data = NULL;
	unsigned int len;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetObjectInfoEx;
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

/**
 * ptp_canon_get_objecthandle_by_name:
 *
 * This command looks up the specified object on the camera.
 *
 * Format is "A:\\PATH".
 *
 * The 'A' is the VolumeLabel from GetStorageInfo,
 * my IXUS has "A" for the card and "V" for internal memory.
 *  
 * params:	PTPParams*
 *      char* name - path name
 *
 * Return values: Some PTP_RC_* code.
 *      uint32_t *oid - PTP object id.
 *
 **/
uint16_t
ptp_canon_get_objecthandle_by_name (PTPParams* params, char* name, uint32_t* objectid)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char *data = NULL;
	uint8_t len;

	PTP_CNT_INIT (ptp);
	ptp.Code=PTP_OC_CANON_GetObjectHandleByName;
	ptp.Nparam=0;
	len=0;
	data = malloc (2*(strlen(name)+1)+2);
	memset (data, 0, 2*(strlen(name)+1)+2);
	ptp_pack_string (params, name, data, 0, &len);
	ret=ptp_transaction (params, &ptp, PTP_DP_SENDDATA, (len+1)*2+1, &data, NULL);
	free (data);
	*objectid = ptp.Param1;
	return ret;
}

/**
 * ptp_canon_get_customize_data:
 *
 * This command downloads the specified theme slot, including jpegs
 * and wav files.
 *  
 * params:	PTPParams*
 *      uint32_t themenr - nr of theme
 *
 * Return values: Some PTP_RC_* code.
 *      unsigned char **data - pointer to data pointer
 *      unsigned int  *size - size of data returned
 *
 **/
uint16_t
ptp_canon_get_customize_data (PTPParams* params, uint32_t themenr,
		unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	*data = NULL;
	*size = 0;
	PTP_CNT_INIT(ptp);
	ptp.Code	= PTP_OC_CANON_GetCustomizeData;
	ptp.Param1	= themenr;
	ptp.Nparam	= 1;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}


uint16_t
ptp_nikon_curve_download (PTPParams* params, unsigned char **data, unsigned int *size) {
	PTPContainer ptp;
	*data = NULL;
	*size = 0;
	PTP_CNT_INIT(ptp);
	ptp.Code	= PTP_OC_NIKON_CurveDownload;
	ptp.Nparam	= 0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

uint16_t
ptp_nikon_getfileinfoinblock ( PTPParams* params,
	uint32_t p1, uint32_t p2, uint32_t p3,
	unsigned char **data, unsigned int *size
) {
	PTPContainer ptp;
	*data = NULL;
	*size = 0;
	PTP_CNT_INIT(ptp);
	ptp.Code	= PTP_OC_NIKON_GetFileInfoInBlock;
	ptp.Nparam	= 3;
	ptp.Param1	= p1;
	ptp.Param2	= p2;
	ptp.Param3	= p3;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

/**
 * ptp_nikon_setcontrolmode:
 *
 * This command can switch the camera to full PC control mode.
 *  
 * params:	PTPParams*
 *      uint32_t mode - mode
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_setcontrolmode (PTPParams* params, uint32_t mode)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_NIKON_SetControlMode;
        ptp.Param1=mode;
        ptp.Nparam=1;
        return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_nikon_capture:
 *
 * This command captures a picture on the Nikon.
 *  
 * params:	PTPParams*
 *      uint32_t x - unknown parameter. seen to be -1.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_capture (PTPParams* params, uint32_t x)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_NIKON_Capture;
        ptp.Param1=x;
        ptp.Nparam=1;
        return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_nikon_check_event:
 *
 * This command checks the event queue on the Nikon.
 *  
 * params:	PTPParams*
 *      PTPUSBEventContainer **event - list of usb events.
 *	int *evtcnt - number of usb events in event structure.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_check_event (PTPParams* params, PTPUSBEventContainer** event, int* evtcnt)
{
        PTPContainer ptp;
	uint16_t ret;
	unsigned char *data = NULL;
	unsigned int size = 0;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_NIKON_CheckEvent;
	ptp.Nparam=0;
	*evtcnt = 0;
	ret = ptp_transaction (params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	if (ret == PTP_RC_OK) {
		ptp_unpack_Nikon_EC (params, data, size, event, evtcnt);
		free (data);
	}
	return ret;
}

/**
 * ptp_nikon_device_ready:
 *
 * This command checks if the device is ready. Used after
 * a capture.
 *  
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_device_ready (PTPParams* params)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_NIKON_DeviceReady;
        ptp.Nparam=0;
        return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_nikon_getptpipinfo:
 *
 * This command gets the ptpip info data.
 *  
 * params:	PTPParams*
 *	unsigned char *data	- data
 *	unsigned int size	- size of returned data
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_getptpipinfo (PTPParams* params, unsigned char **data, unsigned int *size)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_NIKON_GetDevicePTPIPInfo;
        ptp.Nparam=0;
        return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size);
}

/**
 * ptp_nikon_getwifiprofilelist:
 *
 * This command gets the wifi profile list.
 *  
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_getwifiprofilelist (PTPParams* params)
{
        PTPContainer ptp;
	unsigned char* data;
	unsigned int size;
	unsigned int pos;
	unsigned int profn;
	unsigned int n;
	char* buffer;
	uint8_t len;
	
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_NIKON_GetProfileAllData;
        ptp.Nparam=0;
	size = 0;
	data = NULL;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));

	if (size < 2) return PTP_RC_Undefined; /* FIXME: Add more precise error code */

	params->wifi_profiles_version = data[0];
	params->wifi_profiles_number = data[1];
	if (params->wifi_profiles)
		free(params->wifi_profiles);
	
	params->wifi_profiles = malloc(params->wifi_profiles_number*sizeof(PTPNIKONWifiProfile));
	memset(params->wifi_profiles, 0, params->wifi_profiles_number*sizeof(PTPNIKONWifiProfile));

	pos = 2;
	profn = 0;
	while (profn < params->wifi_profiles_number && pos < size) {
		if (pos+6 >= size) return PTP_RC_Undefined;
		params->wifi_profiles[profn].id = data[pos++];
		params->wifi_profiles[profn].valid = data[pos++];

		n = dtoh32a(&data[pos]);
		pos += 4;
		if (pos+n+4 >= size) return PTP_RC_Undefined;
		strncpy(params->wifi_profiles[profn].profile_name, (char*)&data[pos], n);
		params->wifi_profiles[profn].profile_name[16] = '\0';
		pos += n;

		params->wifi_profiles[profn].display_order = data[pos++];
		params->wifi_profiles[profn].device_type = data[pos++];
		params->wifi_profiles[profn].icon_type = data[pos++];

		buffer = ptp_unpack_string(params, data, pos, &len);
		strncpy(params->wifi_profiles[profn].creation_date, buffer, sizeof(params->wifi_profiles[profn].creation_date));
		pos += (len*2+1);
		if (pos+1 >= size) return PTP_RC_Undefined;
		/* FIXME: check if it is really last usage date */
		buffer = ptp_unpack_string(params, data, pos, &len);
		strncpy(params->wifi_profiles[profn].lastusage_date, buffer, sizeof(params->wifi_profiles[profn].lastusage_date));
		pos += (len*2+1);
		if (pos+5 >= size) return PTP_RC_Undefined;
		
		n = dtoh32a(&data[pos]);
		pos += 4;
		if (pos+n >= size) return PTP_RC_Undefined;
		strncpy(params->wifi_profiles[profn].essid, (char*)&data[pos], n);
		params->wifi_profiles[profn].essid[32] = '\0';
		pos += n;
		pos += 1;
		profn++;
	}

#if 0
	PTPNIKONWifiProfile test;
	memset(&test, 0, sizeof(PTPNIKONWifiProfile));
	strcpy(test.profile_name, "MyTest");
	test.icon_type = 1;
	strcpy(test.essid, "nikon");
	test.ip_address = 10 + 11 << 16 + 11 << 24;
	test.subnet_mask = 24;
	test.access_mode = 1;
	test.wifi_channel = 1;
	test.key_nr = 1;

	ptp_nikon_writewifiprofile(params, &test);
#endif

	return PTP_RC_OK;
}

/**
 * ptp_nikon_deletewifiprofile:
 *
 * This command deletes a wifi profile.
 *  
 * params:	PTPParams*
 *	unsigned int profilenr	- profile number
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_deletewifiprofile (PTPParams* params, uint32_t profilenr)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_NIKON_DeleteProfile;
	ptp.Nparam=1;
	ptp.Param1=profilenr;
	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_nikon_writewifiprofile:
 *
 * This command gets the ptpip info data.
 *  
 * params:	PTPParams*
 *	unsigned int profilenr	- profile number
 *	unsigned char *data	- data
 *	unsigned int size	- size of returned data
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_writewifiprofile (PTPParams* params, PTPNIKONWifiProfile* profile)
{
	unsigned char guid[16];
	
	PTPContainer ptp;
	unsigned char buffer[1024];
	unsigned char* data = buffer;
	int size = 0;
	int i;
	uint8_t len;
	int profilenr = -1;
	
	ptp_nikon_getptpipguid(guid);

	if (!params->wifi_profiles)
		CHECK_PTP_RC(ptp_nikon_getwifiprofilelist(params));
	
	for (i = 0; i < params->wifi_profiles_number; i++) {
		if (!params->wifi_profiles[i].valid) {
			profilenr = params->wifi_profiles[i].id;
			break;
		}
	}
	
	if (profilenr == -1) {
		/* No free profile! */
		return PTP_RC_StoreFull;
	}
	
	memset(buffer, 0, 1024);
	
	buffer[0x00] = 0x64; /* Version */
	
	/* Profile name */
	htod32a(&buffer[0x01], 17);
	/* 16 as third parameter, so there will always be a null-byte in the end */
	strncpy((char*)&buffer[0x05], profile->profile_name, 16);
	
	buffer[0x16] = 0x00; /* Display order */
	buffer[0x17] = profile->device_type;
	buffer[0x18] = profile->icon_type;
	
	/* FIXME: Creation date: put a real date here */
	ptp_pack_string(params, "19990909T090909", data, 0x19, &len);
	
	/* IP parameters */
	*((unsigned int*)&buffer[0x3A]) = profile->ip_address; /* Do not reverse bytes */
	buffer[0x3E] = profile->subnet_mask;
	*((unsigned int*)&buffer[0x3F]) = profile->gateway_address; /* Do not reverse bytes */
	buffer[0x43] = profile->address_mode;
	
	/* Wifi parameters */
	buffer[0x44] = profile->access_mode;
	buffer[0x45] = profile->wifi_channel;
	
	htod32a(&buffer[0x46], 33); /* essid */
	 /* 32 as third parameter, so there will always be a null-byte in the end */
	strncpy((char*)&buffer[0x4A], profile->essid, 32);
	
	buffer[0x6B] = profile->authentification;
	buffer[0x6C] = profile->encryption;
	htod32a(&buffer[0x6D], 64);
	for (i = 0; i < 64; i++) {
		buffer[0x71+i] = profile->key[i];
	}
	buffer[0xB1] = profile->key_nr;
	memcpy(&buffer[0xB2], guid, 16);
	
	switch(profile->encryption) {
	case 1: /* WEP 64bit */
		htod16a(&buffer[0xC2], 5); /* (64-24)/8 = 5 */
		break;
	case 2: /* WEP 128bit */
		htod16a(&buffer[0xC2], 13); /* (128-24)/8 = 13 */
		break;
	default:
		htod16a(&buffer[0xC2], 0);
	}
	size = 0xC4;
	       
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_NIKON_SendProfileData;
	ptp.Nparam=1;
	ptp.Param1=profilenr;
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
}

/**
 * ptp_mtp_getobjectpropssupported:
 *
 * This command gets the object properties possible from the device.
 *  
 * params:	PTPParams*
 *	uint ofc		- object format code
 *	unsigned int *propnum	- number of elements in returned array
 *	uint16_t *props		- array of supported properties
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_mtp_getobjectpropssupported (PTPParams* params, uint16_t ofc,
		 uint32_t *propnum, uint16_t **props
) {
        PTPContainer ptp;
	uint16_t ret;
	unsigned char *data = NULL;
	unsigned int size = 0;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_MTP_GetObjectPropsSupported;
        ptp.Nparam = 1;
        ptp.Param1 = ofc;
        ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	if (ret == PTP_RC_OK)
        	*propnum=ptp_unpack_uint16_t_array(params,data,0,props);
	free(data);
	return ret;
}

/**
 * ptp_mtp_getobjectpropdesc:
 *
 * This command gets the object property description.
 *  
 * params:	PTPParams*
 *	uint16_t opc	- object property code
 *	uint16_t ofc	- object format code
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_mtp_getobjectpropdesc (
	PTPParams* params, uint16_t opc, uint16_t ofc, PTPObjectPropDesc *opd
) {
        PTPContainer ptp;
	uint16_t ret;
	unsigned char *data = NULL;
	unsigned int size = 0;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_MTP_GetObjectPropDesc;
        ptp.Nparam = 2;
        ptp.Param1 = opc;
        ptp.Param2 = ofc;
        ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	if (ret == PTP_RC_OK)
		ptp_unpack_OPD (params, data, opd, size);
	free(data);
	return ret;
}

/**
 * ptp_mtp_getobjectpropvalue:
 *
 * This command gets the object properties of an object handle.
 *  
 * params:	PTPParams*
 *	uint32_t objectid	- object format code
 *	uint16_t opc		- object prop code
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_mtp_getobjectpropvalue (
	PTPParams* params, uint32_t oid, uint16_t opc,
	PTPPropertyValue *value, uint16_t datatype
) {
        PTPContainer ptp;
	uint16_t ret;
	unsigned char *data = NULL;
	unsigned int size = 0;
	int offset = 0;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_MTP_GetObjectPropValue;
        ptp.Nparam = 2;
        ptp.Param1 = oid;
        ptp.Param2 = opc;
        ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	if (ret == PTP_RC_OK)
		ptp_unpack_DPV(params, data, &offset, size, value, datatype);
	free(data);
	return ret;
}

/**
 * ptp_mtp_setobjectpropvalue:
 *
 * This command gets the object properties of an object handle.
 *  
 * params:	PTPParams*
 *	uint32_t objectid	- object format code
 *	uint16_t opc		- object prop code
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_mtp_setobjectpropvalue (
	PTPParams* params, uint32_t oid, uint16_t opc,
	PTPPropertyValue *value, uint16_t datatype
) {
        PTPContainer ptp;
	uint16_t ret;
	unsigned char *data = NULL;
	unsigned int size ;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_MTP_SetObjectPropValue;
        ptp.Nparam = 2;
        ptp.Param1 = oid;
        ptp.Param2 = opc;
	size = ptp_pack_DPV(params, value, &data, datatype);
        ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	return ret;
}

uint16_t
ptp_mtp_getobjectreferences (PTPParams* params, uint32_t handle, uint32_t** ohArray, uint32_t* arraylen)
{
	PTPContainer ptp;
	uint16_t ret;
	unsigned char* dpv=NULL;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_MTP_GetObjectReferences;
	ptp.Param1=handle;
	ptp.Nparam=1;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dpv, NULL);
	if (ret == PTP_RC_OK) *arraylen = ptp_unpack_uint32_t_array(params, dpv, 0, ohArray);
	free(dpv);
	return ret;
}

uint16_t
ptp_mtp_setobjectreferences (PTPParams* params, uint32_t handle, uint32_t* ohArray, uint32_t arraylen)
{
	PTPContainer ptp;
	uint16_t ret;
	uint32_t size;
	unsigned char* dpv=NULL;

	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_MTP_SetObjectReferences;
	ptp.Param1 = handle;
	ptp.Nparam = 1;
	size = ptp_pack_uint32_t_array(params, ohArray, arraylen, &dpv);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, (unsigned char **)&dpv, NULL);
	free(dpv);
	return ret;
}

uint16_t
ptp_mtp_getobjectproplist (PTPParams* params, uint32_t handle, MTPPropList **proplist)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* opldata = NULL;
	unsigned int oplsize;

	PTP_CNT_INIT(ptp);
	ptp.Code = PTP_OC_MTP_GetObjPropList;
	ptp.Param1 = handle;
	ptp.Param2 = 0x00000000U;  /* 0x00000000U should be "all formats" */
	ptp.Param3 = 0xFFFFFFFFU;  /* 0xFFFFFFFFU should be "all properties" */
	ptp.Param4 = 0x00000000U;
	ptp.Param5 = 0x00000000U;
	ptp.Nparam = 5;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &opldata, &oplsize);  
	if (ret == PTP_RC_OK) ptp_unpack_OPL(params, opldata, proplist, oplsize);
	if (opldata != NULL)
		free(opldata);
	return ret;
}

uint16_t
ptp_mtp_sendobjectproplist (PTPParams* params, uint32_t* store, uint32_t* parenthandle, uint32_t* handle,
			    uint16_t objecttype, uint64_t objectsize, MTPPropList *proplist)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* opldata=NULL;
	uint32_t oplsize;

	PTP_CNT_INIT(ptp);
	ptp.Code = PTP_OC_MTP_SendObjectPropList;
	ptp.Param1 = *store;
	ptp.Param2 = *parenthandle;
	ptp.Param3 = (uint32_t) objecttype;
	ptp.Param4 = (uint32_t) (objectsize >> 32);
	ptp.Param5 = (uint32_t) (objectsize & 0xffffffffU);
	ptp.Nparam = 5;

	/* Set object handle to 0 for a new object */
	oplsize = ptp_pack_OPL(params,proplist,&opldata);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, oplsize, &opldata, NULL); 
	free(opldata);
	*store = ptp.Param1;
	*parenthandle = ptp.Param2;
	*handle = ptp.Param3; 

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
ptp_free_objectpropdesc(PTPObjectPropDesc* opd)
{
	uint16_t i;

	ptp_free_devicepropvalue (opd->DataType, &opd->FactoryDefaultValue);
	switch (opd->FormFlag) {
	case PTP_OPFF_None:
		break;
	case PTP_OPFF_Range:
		ptp_free_devicepropvalue (opd->DataType, &opd->FORM.Range.MinimumValue);
		ptp_free_devicepropvalue (opd->DataType, &opd->FORM.Range.MaximumValue);
		ptp_free_devicepropvalue (opd->DataType, &opd->FORM.Range.StepSize);
		break;
	case PTP_OPFF_Enumeration:
		if (opd->FORM.Enum.SupportedValue) {
			for (i=0;i<opd->FORM.Enum.NumberOfValues;i++)
				ptp_free_devicepropvalue (opd->DataType, opd->FORM.Enum.SupportedValue+i);
			free (opd->FORM.Enum.SupportedValue);
		}
	default:
		fprintf (stderr, "Unknown OPFF type %d\n", opd->FormFlag);
		break;
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
	/* Device Property descriptions */
	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties[] = {
		{PTP_DPC_Undefined,		N_("Undefined PTP Property")},
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
		{PTP_DPC_CANON_ShootingMode,	N_("Shooting Mode")},
		{PTP_DPC_CANON_MeteringMode,	N_("Metering Mode")},
		{PTP_DPC_CANON_AFDistance,	N_("AF Distance")},
		{PTP_DPC_CANON_FocusingPoint,	N_("Focusing Point")},
		{PTP_DPC_CANON_WhiteBalance,	N_("White Balance")},
		{PTP_DPC_CANON_ISOSpeed,	N_("ISO Speed")},
		{PTP_DPC_CANON_Aperture,	N_("Aperture")},
		{PTP_DPC_CANON_ShutterSpeed,	N_("ShutterSpeed")},
		{PTP_DPC_CANON_ExpCompensation,	N_("Exposure Compensation")},
		{PTP_DPC_CANON_Zoom,		N_("Zoom")},
		{PTP_DPC_CANON_SizeQualityMode,	N_("Size Quality Mode")},
		{PTP_DPC_CANON_FirmwareVersion,	N_("Firmware Version")},
		{PTP_DPC_CANON_CameraModel,	N_("Camera Model")},
		{PTP_DPC_CANON_CameraOwner,	N_("Camera Owner")},
		{PTP_DPC_CANON_UnixTime,	N_("UNIX Time")},
		{PTP_DPC_CANON_DZoomMagnification,	N_("Digital Zoom Magnification")},
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
		{PTP_DPC_NIKON_WhiteBalanceFluorescentBias,	/* 0xD019 */
		 N_("Fluorescent White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceDaylightBias,	/* 0xD01a */
		 N_("Daylight White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceFlashBias,		/* 0xD01b */
		 N_("Flash White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceCloudyBias,		/* 0xD01c */
		 N_("Cloudy White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceShadeBias,		/* 0xD01d */
		 N_("Shady White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceColorTemperature,	/* 0xD01e */
		 N_("White Balance Colour Temperature")},
		{PTP_DPC_NIKON_ImageSharpening,			/* 0xD02a */
		 N_("Sharpening")},
		{PTP_DPC_NIKON_ToneCompensation,		/* 0xD02b */
		 N_("Tone Compensation")},
		{PTP_DPC_NIKON_ColorModel,			/* 0xD02c */
		 N_("Color Model")},
		{PTP_DPC_NIKON_HueAdjustment,			/* 0xD02d */
		 N_("Hue Adjustment")},
		{PTP_DPC_NIKON_NonCPULensDataFocalLength,	/* 0xD02e */
		 N_("Lens Focal Length (Non CPU)")},
		{PTP_DPC_NIKON_NonCPULensDataMaximumAperture,	/* 0xD02f */
		 N_("Lens Max. Aperture (Non CPU)")},
		{PTP_DPC_NIKON_CSMMenuBankSelect,		/* 0xD040 */
		 "PTP_DPC_NIKON_CSMMenuBankSelect"},
		{PTP_DPC_NIKON_MenuBankNameA,			/* 0xD041 */
		 "PTP_DPC_NIKON_MenuBankNameA"},
		{PTP_DPC_NIKON_MenuBankNameB,			/* 0xD042 */
		 "PTP_DPC_NIKON_MenuBankNameB"},
		{PTP_DPC_NIKON_MenuBankNameC,			/* 0xD043 */
		 "PTP_DPC_NIKON_MenuBankNameC"},
		{PTP_DPC_NIKON_MenuBankNameD,			/* 0xD044 */
		 "PTP_DPC_NIKON_MenuBankNameD"},
		{PTP_DPC_NIKON_A1AFCModePriority,		/* 0xD048 */
		 "PTP_DPC_NIKON_A1AFCModePriority"},
		{PTP_DPC_NIKON_A2AFSModePriority,		/* 0xD049 */
		 "PTP_DPC_NIKON_A2AFSModePriority"},
		{PTP_DPC_NIKON_A3GroupDynamicAF,		/* 0xD04a */
		 "PTP_DPC_NIKON_A3GroupDynamicAF"},
		{PTP_DPC_NIKON_A4AFActivation,			/* 0xD04b */
		 "PTP_DPC_NIKON_A4AFActivation"},
		{PTP_DPC_NIKON_A5FocusAreaIllumManualFocus,	/* 0xD04c */
		 "PTP_DPC_NIKON_A5FocusAreaIllumManualFocus"},
		{PTP_DPC_NIKON_FocusAreaIllumContinuous,	/* 0xD04d */
		 "PTP_DPC_NIKON_FocusAreaIllumContinuous"},
		{PTP_DPC_NIKON_FocusAreaIllumWhenSelected,	/* 0xD04e */
		 "PTP_DPC_NIKON_FocusAreaIllumWhenSelected"},
		{PTP_DPC_NIKON_FocusAreaWrap,			/* 0xD04f */
		 N_("Focus Area Wrap")},
		{PTP_DPC_NIKON_A7VerticalAFON,			/* 0xD050 */
		 N_("Vertical AF On")},
		{PTP_DPC_NIKON_ISOAuto,				/* 0xD054 */
		 N_("Auto ISO")},
		{PTP_DPC_NIKON_B2ISOStep,			/* 0xD055 */
		 N_("ISO Step")},
		{PTP_DPC_NIKON_EVStep,				/* 0xD056 */
		 N_("Exposure Step")},
		{PTP_DPC_NIKON_B4ExposureCompEv,		/* 0xD057 */
		 N_("Exposure Compensation (EV)")},
		{PTP_DPC_NIKON_ExposureCompensation,		/* 0xD058 */
		 N_("Exposure Compensation")},
		{PTP_DPC_NIKON_CenterWeightArea,		/* 0xD059 */
		 N_("Centre Weight Area")},
		{PTP_DPC_NIKON_AELockMode,			/* 0xD05e */
		 N_("Exposure Lock")},
		{PTP_DPC_NIKON_AELAFLMode,			/* 0xD05f */
		 N_("Focus Lock")},
		{PTP_DPC_NIKON_MeterOff,			/* 0xD062 */
		 N_("Auto Meter Off Time")},
		{PTP_DPC_NIKON_SelfTimer,			/* 0xD063 */
		 N_("Self Timer Delay")},
		{PTP_DPC_NIKON_MonitorOff,			/* 0xD064 */
		 N_("LCD Off Time")},
		{PTP_DPC_NIKON_D1ShootingSpeed,			/* 0xD068 */
		 N_("Shooting Speed")},
		{PTP_DPC_NIKON_D2MaximumShots,			/* 0xD069 */
		 N_("Max. Shots")},
		{PTP_DPC_NIKON_D3ExpDelayMode,			/* 0xD06a */
		 "PTP_DPC_NIKON_D3ExpDelayMode"},
		{PTP_DPC_NIKON_LongExposureNoiseReduction,	/* 0xD06b */
		 N_("Long Exposure Noise Reduction")},
		{PTP_DPC_NIKON_FileNumberSequence,		/* 0xD06c */
		 N_("File Number Sequencing")},
		{PTP_DPC_NIKON_D6ControlPanelFinderRearControl,	/* 0xD06d */
		 "PTP_DPC_NIKON_D6ControlPanelFinderRearControl"},
		{PTP_DPC_NIKON_ControlPanelFinderViewfinder,	/* 0xD06e */
		 "PTP_DPC_NIKON_ControlPanelFinderViewfinder"},
		{PTP_DPC_NIKON_D7Illumination,			/* 0xD06f */
		 "PTP_DPC_NIKON_D7Illumination"},
		{PTP_DPC_NIKON_E1FlashSyncSpeed,		/* 0xD074 */
		 N_("Flash Sync. Speed")},
		{PTP_DPC_NIKON_FlashShutterSpeed,		/* 0xD075 */
		 N_("Flash Shutter Speed")},
		{PTP_DPC_NIKON_E3AAFlashMode,			/* 0xD076 */
		 N_("Flash Mode")},
		{PTP_DPC_NIKON_E4ModelingFlash,			/* 0xD077 */
		 N_("Modeling Flash")},
		{PTP_DPC_NIKON_BracketSet,			/* 0xD078 */
		 N_("Bracket Set")},
		{PTP_DPC_NIKON_E6ManualModeBracketing,		/* 0xD079 */
		 N_("Manual Mode Bracketing")},
		{PTP_DPC_NIKON_BracketOrder,			/* 0xD07a */
		 N_("Bracket Order")},
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
		{PTP_DPC_NIKON_ReverseCommandDial,		/* 0xD086 */
		 N_("Reverse Command Dial")},
		{PTP_DPC_NIKON_ApertureSetting,			/* 0xD087 */
		 N_("Aperture Setting")},
		{PTP_DPC_NIKON_MenusAndPlayback,		/* 0xD088 */
		 N_("Menus and Playback")},
		{PTP_DPC_NIKON_F6ButtonsAndDials,		/* 0xD089 */
		 N_("Buttons and Dials")},
		{PTP_DPC_NIKON_NoCFCard,			/* 0xD08a */
		 N_("No CF Card Release")},
		{PTP_DPC_NIKON_ImageRotation,			/* 0xD092 */
		 N_("Image Rotation")},
		{PTP_DPC_NIKON_Bracketing,			/* 0xD0c0 */
		 N_("Exposure Bracketing")},
		{PTP_DPC_NIKON_ExposureBracketingIntervalDist,	/* 0xD0c1 */
		 N_("Exposure Bracketing Distance")},
		{PTP_DPC_NIKON_BracketingProgram,		/* 0xD0c2 */
		 N_("Exposure Bracketing Number")},
		{PTP_DPC_NIKON_AutofocusLCDTopMode2,		/* 0xD107 */
		 N_("AF LCD Top Mode 2")},
		{PTP_DPC_NIKON_AutofocusArea,			/* 0xD108 */
		 N_("Active AF Sensor")},
		{PTP_DPC_NIKON_LightMeter,			/* 0xD10a */
		 N_("Exposure Meter")},
		{PTP_DPC_NIKON_ExposureApertureLock,		/* 0xD111 */
		 N_("Exposure Aperture Lock")},
		{PTP_DPC_NIKON_MaximumShots,			/* 0xD103 */
		 N_("Maximum Shots")},
		{PTP_DPC_NIKON_OptimizeImage,			/* 0xD140 */
		 N_("Optimize Image")},
		{PTP_DPC_NIKON_Saturation,			/* 0xD142 */
		 N_("Saturation")},
		{PTP_DPC_NIKON_CSMMenu,				/* 0xD180 */
		 N_("CSM Menu")},
		{PTP_DPC_NIKON_BeepOff,
		 N_("AF Beep Mode")},
		{PTP_DPC_NIKON_AutofocusMode,
		 N_("Autofocus Mode")},
		{PTP_DPC_NIKON_AFAssist,
		 N_("AF Assist Lamp")},
		{PTP_DPC_NIKON_PADVPMode,
		 N_("Auto ISO P/A/DVP Setting")},
		{PTP_DPC_NIKON_ImageReview,
		 N_("Image Review")},
		{PTP_DPC_NIKON_GridDisplay,
		 N_("Viewfinder Grid Display")},
		{PTP_DPC_NIKON_AFAreaIllumination,
		 N_("AF Area Illumination")},
		{PTP_DPC_NIKON_FlashMode,
		 N_("Flash Mode")},
		{PTP_DPC_NIKON_FlashModeManualPower,
		 N_("Flash Mode Manual Power")},
		{PTP_DPC_NIKON_FlashSign,
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
		{PTP_DPC_NIKON_ACPower, N_("AC Power")},
		{PTP_DPC_NIKON_BracketingSet, N_("NIKON Auto Bracketing Set")},
		{PTP_DPC_NIKON_WhiteBalanceBracketStep, N_("NIKON White Balance Bracket Step")},
		{PTP_DPC_NIKON_AFLLock, N_("NIKON AF-L Locked")},
		{0,NULL}
	};
        struct {
		uint16_t dpc;
		const char *txt;
        } ptp_device_properties_MTP[] = {
		{PTP_DPC_MTP_SecureTime,        N_("Secure Time")},
		{PTP_DPC_MTP_DeviceCertificate, N_("Device Certificate")},
		{PTP_DPC_MTP_SynchronizationPartner,
		 N_("Synchronization Partner")},
		{PTP_DPC_MTP_DeviceFriendlyName,
		 N_("Device Friendly Name")},
		{PTP_DPC_MTP_VolumeLevel,       N_("Volume Level")},
		{PTP_DPC_MTP_DeviceIcon,        N_("Device Icon")},
		{PTP_DPC_MTP_PlaybackRate,      N_("Playback Rate")},
		{PTP_DPC_MTP_PlaybackObject,    N_("Playback Object")},
		{PTP_DPC_MTP_PlaybackContainerIndex,
		 N_("Playback Container Index")},
		{PTP_DPC_MTP_PlaybackPosition,  N_("Playback Position")},
		{0,NULL}
        };

	for (i=0; ptp_device_properties[i].txt!=NULL; i++)
		if (ptp_device_properties[i].dpc==dpc)
			return (ptp_device_properties[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_MICROSOFT)
		for (i=0; ptp_device_properties_MTP[i].txt!=NULL; i++)
			if (ptp_device_properties_MTP[i].dpc==dpc)
				return (ptp_device_properties_MTP[i].txt);

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
		if (!data->str)
			return 0;
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
		{PTP_DPC_NIKON_CenterWeightArea, 2.0, 6.0, N_("%.0f mm")},
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
		{PTP_DPC_WhiteBalance, 6, N_("Incandescent")},
		{PTP_DPC_WhiteBalance, 5, N_("Fluorescent")},
		{PTP_DPC_WhiteBalance, 4, N_("Daylight")},
		{PTP_DPC_WhiteBalance, 7, N_("Flash")},
		{PTP_DPC_WhiteBalance, 32784, N_("Cloudy")},
		{PTP_DPC_WhiteBalance, 32785, N_("Shade")},
		{PTP_DPC_WhiteBalance, 32786, N_("Color Temperature")},
		{PTP_DPC_WhiteBalance, 32787, N_("Preset")},
		{PTP_DPC_FlashMode, 32784, N_("Default")},
		{PTP_DPC_FlashMode, 4, N_("Red-eye Reduction")},
		{PTP_DPC_FlashMode, 32787, N_("Red-eye Reduction + Slow Sync")},
		{PTP_DPC_FlashMode, 32785, N_("Slow Sync")},
		{PTP_DPC_FlashMode, 32785, N_("Rear Curtain Sync + Slow Sync")},
		{PTP_DPC_FocusMeteringMode, 2, N_("Dynamic Area")},
		{PTP_DPC_FocusMeteringMode, 32784, N_("Single Area")},
		{PTP_DPC_FocusMeteringMode, 32785, N_("Closest Subject")},
		{PTP_DPC_FocusMeteringMode, 32786, N_("Group Dynamic")},
		{PTP_DPC_FocusMode, 1, N_("Manual Focus")},
		{PTP_DPC_FocusMode, 32784, "AF-S"},
		{PTP_DPC_FocusMode, 32785, "AF-C"},
		PTP_VAL_BOOL(PTP_DPC_NIKON_ISOAuto),
		PTP_VAL_BOOL(PTP_DPC_NIKON_ExposureCompensation),
		PTP_VAL_BOOL(PTP_DPC_NIKON_AELockMode),
		{PTP_DPC_NIKON_AELAFLMode, 0, N_("AE/AF Lock")},
		{PTP_DPC_NIKON_AELAFLMode, 1, N_("AF Lock only")},
		{PTP_DPC_NIKON_AELAFLMode, 2, N_("AE Lock only")},
		{PTP_DPC_NIKON_AELAFLMode, 3, N_("AF Lock Hold")},
		{PTP_DPC_NIKON_AELAFLMode, 4, N_("AF On")},
		{PTP_DPC_NIKON_AELAFLMode, 5, N_("Flash Lock")},
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
		{PTP_DPC_StillCaptureMode, 32784, N_("Continuous Low Speed")},
		{PTP_DPC_StillCaptureMode, 32785, N_("Timer")},
		{PTP_DPC_StillCaptureMode, 32787, N_("Remote")},
		{PTP_DPC_StillCaptureMode, 32787, N_("Mirror Up")},
		{PTP_DPC_StillCaptureMode, 32788, N_("Timer + Remote")},
		PTP_VAL_BOOL(PTP_DPC_NIKON_AutofocusMode),
		PTP_VAL_RBOOL(PTP_DPC_NIKON_AFAssist),
		PTP_VAL_RBOOL(PTP_DPC_NIKON_ImageReview),
		PTP_VAL_BOOL(PTP_DPC_NIKON_GridDisplay),
		{PTP_DPC_NIKON_AFAreaIllumination, 0, N_("Auto")},
		{PTP_DPC_NIKON_AFAreaIllumination, 1, N_("Off")},
		{PTP_DPC_NIKON_AFAreaIllumination, 2, N_("On")},
		{PTP_DPC_NIKON_ColorModel, 0, "sRGB"},
		{PTP_DPC_NIKON_ColorModel, 1, "AdobeRGB"},
		{PTP_DPC_NIKON_ColorModel, 2, "sRGB"},
		{PTP_DPC_NIKON_FlashMode, 0, "iTTL"},
		{PTP_DPC_NIKON_FlashMode, 1, N_("Manual")},
		{PTP_DPC_NIKON_FlashMode, 2, N_("Commander")},
		{PTP_DPC_NIKON_FlashModeManualPower, 0, N_("Full")},
		{PTP_DPC_NIKON_FlashModeManualPower, 1, "1/2"},
		{PTP_DPC_NIKON_FlashModeManualPower, 2, "1/4"},
		{PTP_DPC_NIKON_FlashModeManualPower, 3, "1/8"},
		{PTP_DPC_NIKON_FlashModeManualPower, 4, "1/16"},
		PTP_VAL_RBOOL(PTP_DPC_NIKON_FlashSign),
		{PTP_DPC_NIKON_RemoteTimeout, 0, N_("1 min")},
		{PTP_DPC_NIKON_RemoteTimeout, 1, N_("5 mins")},
		{PTP_DPC_NIKON_RemoteTimeout, 2, N_("10 mins")},
		{PTP_DPC_NIKON_RemoteTimeout, 3, N_("15 mins")},
		PTP_VAL_YN(PTP_DPC_NIKON_FlashOpen),
		PTP_VAL_YN(PTP_DPC_NIKON_FlashCharged),
		PTP_VAL_BOOL(PTP_DPC_NIKON_LongExposureNoiseReduction),
		PTP_VAL_BOOL(PTP_DPC_NIKON_FileNumberSequence),
		PTP_VAL_BOOL(PTP_DPC_NIKON_ReverseCommandDial),
		PTP_VAL_RBOOL(PTP_DPC_NIKON_NoCFCard),
		PTP_VAL_RBOOL(PTP_DPC_NIKON_ImageRotation),
		PTP_VAL_BOOL(PTP_DPC_NIKON_Bracketing),
		{PTP_DPC_NIKON_AutofocusArea, 0, N_("Centre")},
		{PTP_DPC_NIKON_AutofocusArea, 1, N_("Top")},
		{PTP_DPC_NIKON_AutofocusArea, 2, N_("Bottom")},
		{PTP_DPC_NIKON_AutofocusArea, 3, N_("Left")},
		{PTP_DPC_NIKON_AutofocusArea, 4, N_("Right")},
		{PTP_DPC_NIKON_OptimizeImage, 0, N_("Normal")},
		{PTP_DPC_NIKON_OptimizeImage, 1, N_("Vivid")},
		{PTP_DPC_NIKON_OptimizeImage, 2, N_("Sharper")},
		{PTP_DPC_NIKON_OptimizeImage, 3, N_("Softer")},
		{PTP_DPC_NIKON_OptimizeImage, 4, N_("Direct Print")},
		{PTP_DPC_NIKON_OptimizeImage, 5, N_("Portrait")},
		{PTP_DPC_NIKON_OptimizeImage, 6, N_("Landscape")},
		{PTP_DPC_NIKON_OptimizeImage, 7, N_("Custom")},

		{PTP_DPC_NIKON_ImageSharpening, 0, N_("Auto")},
		{PTP_DPC_NIKON_ImageSharpening, 1, N_("Normal")},
		{PTP_DPC_NIKON_ImageSharpening, 2, N_("Low")},
		{PTP_DPC_NIKON_ImageSharpening, 3, N_("Medium Low")},
		{PTP_DPC_NIKON_ImageSharpening, 4, N_("Medium high")},
		{PTP_DPC_NIKON_ImageSharpening, 5, N_("High")},
		{PTP_DPC_NIKON_ImageSharpening, 6, N_("None")},

		{PTP_DPC_NIKON_ToneCompensation, 0, N_("Auto")},
		{PTP_DPC_NIKON_ToneCompensation, 1, N_("Normal")},
		{PTP_DPC_NIKON_ToneCompensation, 2, N_("Low contrast")},
		{PTP_DPC_NIKON_ToneCompensation, 3, N_("Medium low")},
		{PTP_DPC_NIKON_ToneCompensation, 4, N_("Medium high")},
		{PTP_DPC_NIKON_ToneCompensation, 5, N_("High control")},
		{PTP_DPC_NIKON_ToneCompensation, 6, N_("Custom")},

		{PTP_DPC_NIKON_Saturation, 0, N_("Normal")},
		{PTP_DPC_NIKON_Saturation, 1, N_("Moderate")},
		{PTP_DPC_NIKON_Saturation, 2, N_("Enhanced")},

		{PTP_DPC_NIKON_LensID, 0, N_("Unknown")},
		{PTP_DPC_NIKON_LensID, 38, "Sigma 70-300mm 1:4-5.6 D APO Macro"},
		{PTP_DPC_NIKON_LensID, 83, "AF Nikkor 80-200mm 1:2.8 D ED"},
		{PTP_DPC_NIKON_LensID, 118, "AF Nikkor 50mm 1:1.8 D"},
		{PTP_DPC_NIKON_LensID, 127, "AF-S Nikkor 18-70mm 1:3.5-4.5G ED DX"},
		PTP_VAL_YN(PTP_DPC_NIKON_LowLight),
		PTP_VAL_YN(PTP_DPC_NIKON_CSMMenu),
		PTP_VAL_RBOOL(PTP_DPC_NIKON_BeepOff),
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
	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_MICROSOFT) {
		switch (dpc) {
		case PTP_DPC_MTP_SynchronizationPartner:
		case PTP_DPC_MTP_DeviceFriendlyName:
			return snprintf(out, length, "%s", dpd->CurrentValue.str);
		case PTP_DPC_MTP_SecureTime:
		case PTP_DPC_MTP_DeviceCertificate: {
			/* FIXME: Convert to use unicode demux functions */
			for (i=0;(i<dpd->CurrentValue.a.count) && (i<length);i++)
				out[i] = dpd->CurrentValue.a.v[i].u16;
			if (	dpd->CurrentValue.a.count &&
				(dpd->CurrentValue.a.count < length)) {
				out[dpd->CurrentValue.a.count-1] = 0;
				return dpd->CurrentValue.a.count-1;
			} else {
				out[length-1] = 0;
				return length;
			}
			break;
		}
		default:
			break;
		}
	}

	return 0;
}

struct {
	uint16_t ofc;
	const char *format;
} ptp_ofc_trans[] = {
	{PTP_OFC_Undefined,"Undefined Type"},
	{PTP_OFC_Association,"Association/Directory"},
	{PTP_OFC_Script,"Script"},
	{PTP_OFC_Executable,"Executable"},
	{PTP_OFC_Text,"Text"},
	{PTP_OFC_HTML,"HTML"},
	{PTP_OFC_DPOF,"DPOF"},
	{PTP_OFC_AIFF,"AIFF"},
	{PTP_OFC_WAV,"MS Wave"},
	{PTP_OFC_MP3,"MP3"},
	{PTP_OFC_AVI,"MS AVI"},
	{PTP_OFC_MPEG,"MPEG"},
	{PTP_OFC_ASF,"ASF"},
	{PTP_OFC_QT,"Apple Quicktime"},
	{PTP_OFC_EXIF_JPEG,"JPEG"},
	{PTP_OFC_TIFF_EP,"TIFF EP"},
	{PTP_OFC_FlashPix,"FlashPix"},
	{PTP_OFC_BMP,"BMP"},
	{PTP_OFC_CIFF,"CIFF"},
	{PTP_OFC_GIF,"GIF"},
	{PTP_OFC_JFIF,"JFIF"},
	{PTP_OFC_PCD,"PCD"},
	{PTP_OFC_PICT,"PICT"},
	{PTP_OFC_PNG,"PNG"},
	{PTP_OFC_TIFF,"TIFF"},
	{PTP_OFC_TIFF_IT,"TIFF_IT"},
	{PTP_OFC_JP2,"JP2"},
	{PTP_OFC_JPX,"JPX"},
};

struct {
	uint16_t ofc;
	const char *format;
} ptp_ofc_mtp_trans[] = {
	{PTP_OFC_MTP_Firmware,N_("Firmware")},
	{PTP_OFC_MTP_WindowsImageFormat,N_("WindowsImageFormat")},
	{PTP_OFC_MTP_UndefinedAudio,N_("Undefined Audio")},
	{PTP_OFC_MTP_WMA,"WMA"},
	{PTP_OFC_MTP_OGG,"OGG"},
	{PTP_OFC_MTP_AudibleCodec,N_("Audible.com Codec")},
	{PTP_OFC_MTP_UndefinedVideo,N_("Undefined Video")},
	{PTP_OFC_MTP_WMV,"WMV"},
	{PTP_OFC_MTP_MP4,"MP4"},
	{PTP_OFC_MTP_UndefinedCollection,N_("Undefined Collection")},
	{PTP_OFC_MTP_AbstractMultimediaAlbum,N_("Abstract Multimedia Album")},
	{PTP_OFC_MTP_AbstractImageAlbum,N_("Abstract Image Album")},
	{PTP_OFC_MTP_AbstractAudioAlbum,N_("Abstract Audio Album")},
	{PTP_OFC_MTP_AbstractVideoAlbum,N_("Abstract Video Album")},
	{PTP_OFC_MTP_AbstractAudioVideoPlaylist,N_("Abstract Audio Video Playlist")},
	{PTP_OFC_MTP_AbstractContactGroup,N_("Abstract Contact Group")},
	{PTP_OFC_MTP_AbstractMessageFolder,N_("Abstract Message Folder")},
	{PTP_OFC_MTP_AbstractChapteredProduction,N_("Abstract Chaptered Production")},
	{PTP_OFC_MTP_WPLPlaylist,N_("WPL Playlist")},
	{PTP_OFC_MTP_M3UPlaylist,N_("M3U Playlist")},
	{PTP_OFC_MTP_MPLPlaylist,N_("MPL Playlist")},
	{PTP_OFC_MTP_ASXPlaylist,N_("ASX Playlist")},
	{PTP_OFC_MTP_PLSPlaylist,N_("PLS Playlist")},
	{PTP_OFC_MTP_UndefinedDocument,N_("UndefinedDocument")},
	{PTP_OFC_MTP_AbstractDocument,N_("AbstractDocument")},
	{PTP_OFC_MTP_UndefinedMessage,N_("UndefinedMessage")},
	{PTP_OFC_MTP_AbstractMessage,N_("AbstractMessage")},
	{PTP_OFC_MTP_UndefinedContact,N_("UndefinedContact")},
	{PTP_OFC_MTP_AbstractContact,N_("AbstractContact")},
	{PTP_OFC_MTP_vCard2,N_("vCard2")},
	{PTP_OFC_MTP_vCard3,N_("vCard3")},
	{PTP_OFC_MTP_UndefinedCalendarItem,N_("UndefinedCalendarItem")},
	{PTP_OFC_MTP_AbstractCalendarItem,N_("AbstractCalendarItem")},
	{PTP_OFC_MTP_vCalendar1,N_("vCalendar1")},
	{PTP_OFC_MTP_vCalendar2,N_("vCalendar2")},
	{PTP_OFC_MTP_UndefinedWindowsExecutable,N_("Undefined Windows Executable")},
};

int
ptp_render_ofc(PTPParams* params, uint16_t ofc, int spaceleft, char *txt)
{
	int i;
	
	if (!(ofc & 0x8000)) {
		for (i=0;i<sizeof(ptp_ofc_trans)/sizeof(ptp_ofc_trans[0]);i++)
			if (ofc == ptp_ofc_trans[i].ofc)
				return snprintf(txt, spaceleft,_(ptp_ofc_trans[i].format));
	} else {
		switch (params->deviceinfo.VendorExtensionID) {
		case PTP_VENDOR_EASTMAN_KODAK:
			switch (ofc) {
			case PTP_OFC_EK_M3U:
				return snprintf (txt, spaceleft,_("M3U"));
			default:
				break;
			}
			break;
		case PTP_VENDOR_MICROSOFT:
			for (i=0;i<sizeof(ptp_ofc_mtp_trans)/sizeof(ptp_ofc_mtp_trans[0]);i++)
				if (ofc == ptp_ofc_mtp_trans[i].ofc)
					return snprintf(txt, spaceleft,_(ptp_ofc_mtp_trans[i].format));
			break;
		default:break;
		}
	}
	return snprintf (txt, spaceleft,_("Unknown(%04x)"), ofc);
}

struct {
	uint16_t opcode;
	const char *name;
} ptp_opcode_trans[] = {
	{PTP_OC_Undefined,N_("Undefined")},
	{PTP_OC_GetDeviceInfo,N_("get device info")},
	{PTP_OC_OpenSession,N_("Open session")},
	{PTP_OC_CloseSession,N_("Close session")},
	{PTP_OC_GetStorageIDs,N_("Get storage IDs")},
	{PTP_OC_GetStorageInfo,N_("Get storage info")},
	{PTP_OC_GetNumObjects,N_("Get number of objects")},
	{PTP_OC_GetObjectHandles,N_("Get object handles")},
	{PTP_OC_GetObjectInfo,N_("Get object info")},
	{PTP_OC_GetObject,N_("Get object")},
	{PTP_OC_GetThumb,N_("Get thumbnail")},
	{PTP_OC_DeleteObject,N_("Delete object")},
	{PTP_OC_SendObjectInfo,N_("Send object info")},
	{PTP_OC_SendObject,N_("Send object")},
	{PTP_OC_InitiateCapture,N_("Initiate capture")},
	{PTP_OC_FormatStore,N_("Format storage")},
	{PTP_OC_ResetDevice,N_("Reset device")},
	{PTP_OC_SelfTest,N_("Self test device")},
	{PTP_OC_SetObjectProtection,N_("Set object protection")},
	{PTP_OC_PowerDown,N_("Power down device")},
	{PTP_OC_GetDevicePropDesc,N_("Get device property description")},
	{PTP_OC_GetDevicePropValue,N_("Get device property value")},
	{PTP_OC_SetDevicePropValue,N_("Set device property value")},
	{PTP_OC_ResetDevicePropValue,N_("Reset device property value")},
	{PTP_OC_TerminateOpenCapture,N_("Terminate open capture")},
	{PTP_OC_MoveObject,N_("Move object")},
	{PTP_OC_CopyObject,N_("Copy object")},
	{PTP_OC_GetPartialObject,N_("Get partial object")},
	{PTP_OC_InitiateOpenCapture,N_("Initiate open capture")}
};

struct {
	uint16_t opcode;
	const char *name;
} ptp_opcode_mtp_trans[] = {
	{PTP_OC_MTP_GetSecureTimeChallenge,N_("Get secure time challenge")},
	{PTP_OC_MTP_GetSecureTimeResponse,N_("Get secure time response")},
	{PTP_OC_MTP_SetLicenseResponse,N_("Set license response")},
	{PTP_OC_MTP_GetSyncList,N_("Get sync list")},
	{PTP_OC_MTP_SendMeterChallengeQuery,N_("Send meter challenge query")},
	{PTP_OC_MTP_GetMeterChallenge,N_("Get meter challenge")},
	{PTP_OC_MTP_SetMeterResponse,N_("Get meter response")},
	{PTP_OC_MTP_CleanDataStore,N_("Clean data store")},
	{PTP_OC_MTP_GetLicenseState,N_("Get license state")},
	{PTP_OC_MTP_GetObjectPropsSupported,N_("Get object properties supported")},
	{PTP_OC_MTP_GetObjectPropDesc,N_("Get object property description")},
	{PTP_OC_MTP_GetObjectPropValue,N_("Get object property value")},
	{PTP_OC_MTP_SetObjectPropValue,N_("Set object property value")},
	{PTP_OC_MTP_GetObjPropList,N_("Get object property list")},
	{PTP_OC_MTP_SetObjPropList,N_("Set object property list")},
	{PTP_OC_MTP_GetInterdependendPropdesc,N_("Get interdependent property description")},
	{PTP_OC_MTP_SendObjectPropList,N_("Send object property list")},
	{PTP_OC_MTP_GetObjectReferences,N_("Get object references")},
	{PTP_OC_MTP_SetObjectReferences,N_("Set object references")},
	{PTP_OC_MTP_UpdateDeviceFirmware,N_("Update device firmware")},
	{PTP_OC_MTP_Skip,N_("Skip to next position in playlist")}
};

int
ptp_render_opcode(PTPParams* params, uint16_t opcode, int spaceleft, char *txt)
{
	int i;

	if (!(opcode & 0x8000)) {
		for (i=0;i<sizeof(ptp_opcode_trans)/sizeof(ptp_opcode_trans[0]);i++)
			if (opcode == ptp_opcode_trans[i].opcode)
				return snprintf(txt, spaceleft,_(ptp_opcode_trans[i].name));
	} else {
		switch (params->deviceinfo.VendorExtensionID) {
		case PTP_VENDOR_MICROSOFT:
			for (i=0;i<sizeof(ptp_opcode_mtp_trans)/sizeof(ptp_opcode_mtp_trans[0]);i++)
				if (opcode == ptp_opcode_mtp_trans[i].opcode)
					return snprintf(txt, spaceleft,_(ptp_opcode_mtp_trans[i].name));
			break;
		default:break;
		}
	}
	return snprintf (txt, spaceleft,_("Unknown(%04x)"), opcode);
}


struct {
	uint16_t id;
	const char *name;
} ptp_opc_trans[] = {
	{PTP_OPC_StorageID,"StorageID"},
	{PTP_OPC_ObjectFormat,"ObjectFormat"},
	{PTP_OPC_ProtectionStatus,"ProtectionStatus"},
	{PTP_OPC_ObjectSize,"ObjectSize"},
	{PTP_OPC_AssociationType,"AssociationType"},
	{PTP_OPC_AssociationDesc,"AssociationDesc"},
	{PTP_OPC_ObjectFileName,"ObjectFileName"},
	{PTP_OPC_DateCreated,"DateCreated"},
	{PTP_OPC_DateModified,"DateModified"},
	{PTP_OPC_Keywords,"Keywords"},
	{PTP_OPC_ParentObject,"ParentObject"},
	{PTP_OPC_PersistantUniqueObjectIdentifier,"PersistantUniqueObjectIdentifier"},
	{PTP_OPC_SyncID,"SyncID"},
	{PTP_OPC_PropertyBag,"PropertyBag"},
	{PTP_OPC_Name,"Name"},
	{PTP_OPC_CreatedBy,"CreatedBy"},
	{PTP_OPC_Artist,"Artist"},
	{PTP_OPC_DateAuthored,"DateAuthored"},
	{PTP_OPC_Description,"Description"},
	{PTP_OPC_URLReference,"URLReference"},
	{PTP_OPC_LanguageLocale,"LanguageLocale"},
	{PTP_OPC_CopyrightInformation,"CopyrightInformation"},
	{PTP_OPC_Source,"Source"},
	{PTP_OPC_OriginLocation,"OriginLocation"},
	{PTP_OPC_DateAdded,"DateAdded"},
	{PTP_OPC_NonConsumable,"NonConsumable"},
	{PTP_OPC_CorruptOrUnplayable,"CorruptOrUnplayable"},
	{PTP_OPC_RepresentativeSampleFormat,"RepresentativeSampleFormat"},
	{PTP_OPC_RepresentativeSampleSize,"RepresentativeSampleSize"},
	{PTP_OPC_RepresentativeSampleHeight,"RepresentativeSampleHeight"},
	{PTP_OPC_RepresentativeSampleWidth,"RepresentativeSampleWidth"},
	{PTP_OPC_RepresentativeSampleDuration,"RepresentativeSampleDuration"},
	{PTP_OPC_RepresentativeSampleData,"RepresentativeSampleData"},
	{PTP_OPC_Width,"Width"},
	{PTP_OPC_Height,"Height"},
	{PTP_OPC_Duration,"Duration"},
	{PTP_OPC_Rating,"Rating"},
	{PTP_OPC_Track,"Track"},
	{PTP_OPC_Genre,"Genre"},
	{PTP_OPC_Credits,"Credits"},
	{PTP_OPC_Lyrics,"Lyrics"},
	{PTP_OPC_SubscriptionContentID,"SubscriptionContentID"},
	{PTP_OPC_ProducedBy,"ProducedBy"},
	{PTP_OPC_UseCount,"UseCount"},
	{PTP_OPC_SkipCount,"SkipCount"},
	{PTP_OPC_LastAccessed,"LastAccessed"},
	{PTP_OPC_ParentalRating,"ParentalRating"},
	{PTP_OPC_MetaGenre,"MetaGenre"},
	{PTP_OPC_Composer,"Composer"},
	{PTP_OPC_EffectiveRating,"EffectiveRating"},
	{PTP_OPC_Subtitle,"Subtitle"},
	{PTP_OPC_OriginalReleaseDate,"OriginalReleaseDate"},
	{PTP_OPC_AlbumName,"AlbumName"},
	{PTP_OPC_AlbumArtist,"AlbumArtist"},
	{PTP_OPC_Mood,"Mood"},
	{PTP_OPC_DRMStatus,"DRMStatus"},
	{PTP_OPC_SubDescription,"SubDescription"},
	{PTP_OPC_IsCropped,"IsCropped"},
	{PTP_OPC_IsColorCorrected,"IsColorCorrected"},
	{PTP_OPC_TotalBitRate,"TotalBitRate"},
	{PTP_OPC_BitRateType,"BitRateType"},
	{PTP_OPC_SampleRate,"SampleRate"},
	{PTP_OPC_NumberOfChannels,"NumberOfChannels"},
	{PTP_OPC_AudioBitDepth,"AudioBitDepth"},
	{PTP_OPC_ScanDepth,"ScanDepth"},
	{PTP_OPC_AudioWAVECodec,"AudioWAVECodec"},
	{PTP_OPC_AudioBitRate,"AudioBitRate"},
	{PTP_OPC_VideoFourCCCodec,"VideoFourCCCodec"},
	{PTP_OPC_VideoBitRate,"VideoBitRate"},
	{PTP_OPC_FramesPerThousandSeconds,"FramesPerThousandSeconds"},
	{PTP_OPC_KeyFrameDistance,"KeyFrameDistance"},
	{PTP_OPC_BufferSize,"BufferSize"},
	{PTP_OPC_EncodingQuality,"EncodingQuality"},
	{PTP_OPC_BuyFlag,"BuyFlag"},
};

int
ptp_render_mtp_propname(uint16_t propid, int spaceleft, char *txt) {
	int i;
	for (i=0;i<sizeof(ptp_opc_trans)/sizeof(ptp_opc_trans[0]);i++)
		if (propid == ptp_opc_trans[i].id)
			return snprintf(txt, spaceleft,ptp_opc_trans[i].name);
	return snprintf (txt, spaceleft,"unknown(%04x)", propid);
}
