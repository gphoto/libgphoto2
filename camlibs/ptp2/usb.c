/* usb.c
 *
 * Copyright (C) 2001-2004 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2014 Marcus Meissner <marcus@jet.franken.de>
 * Copyright (C) 2006-2007 Linus Walleij <triad@df.lth.se>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _BSD_SOURCE
#include <config.h>
#include "ptp.h"
#include "ptp-private.h"
#include "ptp-bugs.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

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

#define CONTEXT_BLOCK_SIZE	200000

#define PTP_CNT_INIT(cnt) {memset(&cnt,0,sizeof(cnt));}

/* PTP2_FAST_TIMEOUT: how long (in milliseconds) we should wait for
 * an URB to come back on an interrupt endpoint */
/* 100 is not enough for various cameras types. 150 seems to work better */
#define PTP2_FAST_TIMEOUT       150

/* Pack / unpack functions */

#include "ptp-pack.c"

/* send / receive functions */

uint16_t
ptp_usb_sendreq (PTPParams* params, PTPContainer* req)
{
	int res, towrite, do_retry = TRUE;
	PTPUSBBulkContainer usbreq;
	Camera *camera = ((PTPData *)params->data)->camera;
	char buf[100];

	ptp_render_opcode(params, req->Code, sizeof(buf),buf);
	GP_LOG_D ("Sending PTP_OC 0x%0x / %s request...", req->Code, buf);

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
	towrite = PTP_USB_BULK_REQ_LEN-(sizeof(uint32_t)*(5-req->Nparam));
retry:
	res = gp_port_write (camera->port, (char*)&usbreq, towrite);
	if (res != towrite) {
		if (res < 0) {
			GP_LOG_E ("PTP_OC 0x%04x sending req failed: %s (%d)", req->Code, gp_port_result_as_string(res), res);
			if (res == GP_ERROR_IO_WRITE && do_retry) {
				GP_LOG_D ("Clearing halt on OUT EP and retrying once.");
				gp_port_usb_clear_halt (camera->port, GP_PORT_USB_ENDPOINT_OUT);
				do_retry = FALSE;
				goto retry;
			}
		} else
			GP_LOG_E ("PTP_OC 0x%04x sending req failed: wrote only %d of %d bytes", req->Code, res, towrite);
		return PTP_ERROR_IO;
	}
	return PTP_RC_OK;
}

uint16_t
ptp_usb_senddata (PTPParams* params, PTPContainer* ptp,
		  uint64_t size, PTPDataHandler *handler
) {
	uint16_t ret = PTP_RC_OK;
	int res, wlen, datawlen;
	PTPUSBBulkContainer usbdata;
	unsigned long bytes_left_to_transfer, written;
	Camera *camera = ((PTPData *)params->data)->camera;
	unsigned char *bytes;
	int progressid = 0;
	int usecontext = (size > CONTEXT_BLOCK_SIZE);
	GPContext *context = ((PTPData *)params->data)->context;
	char buf[100];

	ptp_render_opcode(params, ptp->Code, sizeof(buf),buf);
	GP_LOG_D ("Sending PTP_OC 0x%0x / %s data...", ptp->Code, buf);
	/* build appropriate USB container */
	usbdata.length	= htod32(PTP_USB_BULK_HDR_LEN+size);
	usbdata.type	= htod16(PTP_USB_CONTAINER_DATA);
	usbdata.code	= htod16(ptp->Code);
	usbdata.trans_id= htod32(ptp->Transaction_ID);

	if (params->split_header_data) {
		datawlen = 0;
		wlen = PTP_USB_BULK_HDR_LEN;
	} else {
		unsigned long gotlen;
		/* For all camera devices. */
		datawlen = (size<PTP_USB_BULK_PAYLOAD_LEN_WRITE)?size:PTP_USB_BULK_PAYLOAD_LEN_WRITE;
		wlen = PTP_USB_BULK_HDR_LEN + datawlen;
		ret = handler->getfunc(params, handler->priv, datawlen, usbdata.payload.data, &gotlen);
		if (ret != PTP_RC_OK)
			return ret;
		if (gotlen != datawlen)
			return PTP_RC_GeneralError;
	}
	res = gp_port_write (camera->port, (char*)&usbdata, wlen);
	if (res != wlen) {
		if (res < 0)
			GP_LOG_E ("PTP_OC 0x%04x sending data failed: %s (%d)", ptp->Code, gp_port_result_as_string(res), res);
		else
			GP_LOG_E ("PTP_OC 0x%04x sending data failed: wrote only %d of %d bytes", ptp->Code, res, wlen);
		return PTP_ERROR_IO;
	}
	if (size <= datawlen) { /* nothing more to do */
		written = wlen;
		goto finalize;
	}
	if (usecontext)
		progressid = gp_context_progress_start (context, (size/CONTEXT_BLOCK_SIZE), _("Uploading..."));
	bytes = malloc (4096);
	if (!bytes)
		return PTP_RC_GeneralError;
	/* if everything OK send the rest */
	bytes_left_to_transfer = size-datawlen;
	ret = PTP_RC_OK;
	written = 0;
	while(bytes_left_to_transfer > 0) {
		unsigned long readlen, toread, oldwritten = written;

		toread = 4096;
		if (toread > bytes_left_to_transfer)
			toread = bytes_left_to_transfer;
		ret = handler->getfunc (params, handler->priv, toread, bytes, &readlen);
		if (ret != PTP_RC_OK)
			break;
		res = gp_port_write (camera->port, (char*)bytes, readlen);
		if (res < 0) {
			ret = PTP_ERROR_IO;
			break;
		}
		bytes_left_to_transfer -= res;
		written += res;
		if (usecontext && (oldwritten/CONTEXT_BLOCK_SIZE < written/CONTEXT_BLOCK_SIZE))
			gp_context_progress_update (context, progressid, written/CONTEXT_BLOCK_SIZE);
#if 0 /* Does not work this way... Hmm. */
		if (gp_context_cancel(context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			ret = ptp_usb_control_cancel_request (params,ptp->Transaction_ID);
			if (ret == PTP_RC_OK)
				ret = PTP_ERROR_CANCEL;
			break;
		}
#endif
	}
	if (usecontext)
		gp_context_progress_stop (context, progressid);
	free (bytes);
finalize:
	if ((ret == PTP_RC_OK) && ((written % params->maxpacketsize) == 0))
		gp_port_write (camera->port, "x", 0);
	if ((ret!=PTP_RC_OK) && (ret!=PTP_ERROR_CANCEL))
		ret = PTP_ERROR_IO;
	return ret;
}

static uint16_t
ptp_usb_getpacket(PTPParams *params, PTPUSBBulkContainer *packet, uint32_t *rlen)
{
	int		tries = 0, result;
	Camera		*camera = ((PTPData *)params->data)->camera;

	/* read the header and potentially the first data */
	if (params->response_packet_size > 0) {
		GP_LOG_D ("Returning previously buffered response packet.");
		/* If there is a buffered packet, just use it. */
		memcpy(packet, params->response_packet, params->response_packet_size);
		*rlen = params->response_packet_size;
		free(params->response_packet);
		params->response_packet = NULL;
		params->response_packet_size = 0;
		/* Here this signifies a "virtual read" */
		return PTP_RC_OK;
	}
retry:
	/* A packet should come in a single read always. */
	result = gp_port_read (camera->port, (char*)packet, sizeof(*packet));
	/* This might be a left over zero-write of the device at the end of the previous transmission */
	if (result == 0)
		result = gp_port_read (camera->port, (char*)packet, sizeof(*packet));
	if (result > 0) {
		*rlen = result;
		return PTP_RC_OK;
	}
	if (result == GP_ERROR_IO_READ) {
		GP_LOG_D ("Clearing halt on IN EP and retrying once.");
		gp_port_usb_clear_halt (camera->port, GP_PORT_USB_ENDPOINT_IN);
		/* retrying only makes sense if we did not read anything yet */
		if (tries++ < 1)
			goto retry;
	}
	return PTP_ERROR_IO;
}

#define READLEN 64*1024 /* read blob size */

uint16_t
ptp_usb_getdata (PTPParams* params, PTPContainer* ptp, PTPDataHandler *handler)
{
	uint16_t ret;
	PTPUSBBulkContainer usbdata;
	unsigned char	*data = NULL;
	uint32_t	bytes_to_read, bytes_read;
	Camera		*camera = ((PTPData *)params->data)->camera;
	int		report_progress, progress_id = 0, do_retry = TRUE, res = GP_OK;
	GPContext *context = ((PTPData *)params->data)->context;
	char buf[100];

	ptp_render_opcode(params, ptp->Code, sizeof(buf),buf);
	GP_LOG_D ("Reading PTP_OC 0x%0x / %s data...", ptp->Code, buf);
	PTP_CNT_INIT(usbdata);

	ret = ptp_usb_getpacket (params, &usbdata, &bytes_read);
	if (ret != PTP_RC_OK)
		goto exit;
	if (dtoh16(usbdata.type) != PTP_USB_CONTAINER_DATA) {
		/* We might have got a response instead. On error for instance. */
		/* TODO: check if bytes_read == usbdata.length */
		if (dtoh16(usbdata.type) == PTP_USB_CONTAINER_RESPONSE) {
			params->response_packet = malloc(dtoh32(usbdata.length));
			if (!params->response_packet) return PTP_RC_GeneralError;
			memcpy(params->response_packet, (uint8_t *) &usbdata, dtoh32(usbdata.length));
			params->response_packet_size = dtoh32(usbdata.length);
			ret = PTP_RC_OK;
		} else {
			ret = PTP_ERROR_DATA_EXPECTED;
		}
		goto exit;
	}
	if (dtoh16(usbdata.code)!=ptp->Code) {
		/* A creative Zen device breaks down here, by leaving out
			 * Code and Transaction ID */
		if (MTP_ZEN_BROKEN_HEADER(params)) {
			GP_LOG_D ("Read broken PTP header (Code is %04x vs %04x), compensating.",
				  dtoh16(usbdata.code), ptp->Code);
			usbdata.code = dtoh16(ptp->Code);
			usbdata.trans_id = htod32(ptp->Transaction_ID);
		} else {
			GP_LOG_E ("Read broken PTP header (Code is %04x vs %04x).",
				  dtoh16(usbdata.code), ptp->Code );
			ret = PTP_ERROR_IO;
			goto exit;
		}
	}
	if (bytes_read > dtoh32(usbdata.length)) {
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
		uint32_t surplen = bytes_read - dtoh32(usbdata.length);

		if (surplen >= PTP_USB_BULK_HDR_LEN) {
			params->response_packet = malloc(surplen);
			if (!params->response_packet) return PTP_RC_GeneralError;
			memcpy(params->response_packet, (uint8_t *) &usbdata + dtoh32(usbdata.length), surplen);
			params->response_packet_size = surplen;
		} else {
			GP_LOG_D ("Read %ld bytes too much, expect problems!",
				  (long)(bytes_read - dtoh32(usbdata.length)));
		}
		bytes_read = dtoh32(usbdata.length);
	}

	/* For most PTP devices rlen is 512 == sizeof(usbdata)
	 * here. For MTP devices splitting header and data it might
	 * be PTP_USB_BULK_HDR_LEN (12).
	 */
	/* autodetect split header/data MTP devices */
	if (dtoh32(usbdata.length) > PTP_USB_BULK_HDR_LEN && (bytes_read==PTP_USB_BULK_HDR_LEN))
		params->split_header_data = 1;

	/* Copy the payload bytes we already read (with the first usb packet) */
	ret = handler->putfunc (params, handler->priv, bytes_read - PTP_USB_BULK_HDR_LEN, usbdata.payload.data);
	if (ret != PTP_RC_OK)
		goto exit;

	/* Check if we are done already... */
	if (bytes_read >= dtoh32(usbdata.length))
		goto exit;

	/* If not read the rest of it. */
	/* Make bytes_to_read contain the number of remaining payload-bytes to read. */
	bytes_to_read = dtoh32(usbdata.length) - bytes_read;
	/* Make bytes_read contain the number of payload-bytes already read. */
	bytes_read -= PTP_USB_BULK_HDR_LEN;

	data = malloc(READLEN);
	if (!data) return PTP_RC_GeneralError;

	report_progress = (bytes_to_read > 2*CONTEXT_BLOCK_SIZE) && (dtoh32(usbdata.length) != 0xffffffffU);
	if (report_progress)
		progress_id = gp_context_progress_start (context, (bytes_to_read/CONTEXT_BLOCK_SIZE), _("Downloading..."));
	while (bytes_to_read > 0) {
		unsigned long chunk_to_read = bytes_to_read;

		/* if in read-until-short-packet mode, read one packet at a time */
		/* else read in large blobs */
		/* else read all but the last short packet depending on EP packetsize. */
		if (dtoh32(usbdata.length) == 0xffffffffU)
			chunk_to_read = PTP_USB_BULK_HS_MAX_PACKET_LEN_READ;
		else if (chunk_to_read > READLEN)
			chunk_to_read = READLEN;
		else if (chunk_to_read > params->maxpacketsize)
			chunk_to_read = chunk_to_read - (chunk_to_read % params->maxpacketsize);
		res = gp_port_read (camera->port, (char*)data, chunk_to_read);
		if (res == GP_ERROR_IO_READ && do_retry) {
			GP_LOG_D ("Clearing halt on IN EP and retrying once.");
			gp_port_usb_clear_halt (camera->port, GP_PORT_USB_ENDPOINT_IN);
			/* retrying only once and only if we did not read anything yet */
			do_retry = FALSE;
			continue;
		} else if (res <= 0) {
			ret = PTP_ERROR_IO;
			break;
		} else
			do_retry = FALSE; /* once we have succesfully read any data, don't try again */
		ret = handler->putfunc (params, handler->priv, res, data);
		if (ret != PTP_RC_OK)
			break;
		if (dtoh32(usbdata.length) == 0xffffffffU) {
			/* once we have read a short packet, we are done. */
			if (res < PTP_USB_BULK_HS_MAX_PACKET_LEN_READ)
				bytes_to_read = 0;
		} else
			bytes_to_read -= res;
		bytes_read += res;
		if (report_progress && ((bytes_read-res)/CONTEXT_BLOCK_SIZE < bytes_read/CONTEXT_BLOCK_SIZE))
			gp_context_progress_update (context, progress_id, bytes_read/CONTEXT_BLOCK_SIZE);
		/* Only cancel transfers larger than 1MB. as
		 * canceling did not work on eos_get_viewfinder of 200kb */
		if ((bytes_read > 1024*1024) && gp_context_cancel(context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			ret = PTP_ERROR_CANCEL;
			break;
		}
	}
	if (report_progress)
		gp_context_progress_stop (context, progress_id);

exit:
	free (data);

	if ((ret!=PTP_RC_OK) && (ret!=PTP_ERROR_CANCEL)) {
		GP_LOG_E ("PTP_OC 0x%04x receiving data failed: %s (0x%04x)", ptp->Code, ptp_strerror(ret, params->deviceinfo.VendorExtensionID), ret);
		return PTP_ERROR_IO;
	}
	return ret;
}

uint16_t
ptp_usb_getresp (PTPParams* params, PTPContainer* resp)
{
	uint16_t 		ret;
	uint32_t		rlen;
	PTPUSBBulkContainer	usbresp;
	/*GPContext		*context = ((PTPData *)params->data)->context;*/

	GP_LOG_D ("Reading PTP_OC 0x%0x response...", resp->Code);
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
		GP_LOG_E ("PTP_OC 0x%04x receiving resp failed: %s (0x%04x)", resp->Code, ptp_strerror(ret, params->deviceinfo.VendorExtensionID), ret);
		return ret;
	}
	/* build an appropriate PTPContainer */
	resp->Code=dtoh16(usbresp.code);
	resp->SessionID=params->session_id;
	resp->Transaction_ID=dtoh32(usbresp.trans_id);
	if (resp->Transaction_ID != params->transaction_id - 1) {
		if (MTP_ZEN_BROKEN_HEADER(params)) {
			GP_LOG_D ("Read broken PTP header (transid is %08x vs %08x), compensating.",
				  resp->Transaction_ID, params->transaction_id - 1
			);
			resp->Transaction_ID=params->transaction_id-1;
		}
		/* else will be handled by ptp.c as error. */
	}
	resp->Nparam=(rlen-12)/4;
	resp->Param1=dtoh32(usbresp.payload.params.param1);
	resp->Param2=dtoh32(usbresp.payload.params.param2);
	resp->Param3=dtoh32(usbresp.payload.params.param3);
	resp->Param4=dtoh32(usbresp.payload.params.param4);
	resp->Param5=dtoh32(usbresp.payload.params.param5);
	return ret;
}

/* Event handling functions */

/* PTP Events wait for or check mode */
#define PTP_EVENT_CHECK			0x0000	/* waits for */
#define PTP_EVENT_CHECK_FAST		0x0001	/* checks */

static inline uint16_t
ptp_usb_event (PTPParams* params, PTPContainer* event, int wait)
{
	int			result, timeout, fasttimeout;
	unsigned long		rlen;
	PTPUSBEventContainer	usbevent;
	Camera			*camera = ((PTPData *)params->data)->camera;

	if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON)
		fasttimeout = PTP2_FAST_TIMEOUT*2;
	else
		fasttimeout = PTP2_FAST_TIMEOUT;

	PTP_CNT_INIT(usbevent);

	if (event==NULL)
		return PTP_ERROR_BADPARAM;

	switch(wait) {
	case PTP_EVENT_CHECK:
		result = gp_port_check_int (camera->port, (char*)&usbevent, sizeof(usbevent));
		if (result <= 0) result = gp_port_check_int (camera->port, (char*)&usbevent, sizeof(usbevent));
		break;
	case PTP_EVENT_CHECK_FAST:
		gp_port_get_timeout (camera->port, &timeout);
		gp_port_set_timeout (camera->port, fasttimeout);
		result = gp_port_check_int (camera->port, (char*)&usbevent, sizeof(usbevent));
		if (result <= 0) result = gp_port_check_int (camera->port, (char*)&usbevent, sizeof(usbevent));
		gp_port_set_timeout (camera->port, timeout);
		break;
	default:
		return PTP_ERROR_BADPARAM;
	}
	if (result < 0) {
		GP_LOG_E ("Reading PTP event failed: %s (%d)", gp_port_result_as_string(result), result);
		if (result == GP_ERROR_TIMEOUT)
			return PTP_ERROR_TIMEOUT;
		return PTP_ERROR_IO;
	}
	if (result == 0) {
		GP_LOG_E ("Reading PTP event failed: a 0 read occurred, assuming timeout.");
		return PTP_ERROR_TIMEOUT;
	}
	rlen = result;
	if (rlen < 8) {
		GP_LOG_E ("Reading PTP event failed: only %ld bytes read", rlen);
		return PTP_ERROR_IO;
	}

	/* Only do the additional reads for "events". Canon IXUS 2 likes to
	 * send unrelated data.
	 */
	if (	(dtoh16(usbevent.type) == PTP_USB_CONTAINER_EVENT) &&
		(dtoh32(usbevent.length) > rlen)
	) {
		GP_LOG_D ("Canon incremental read (done: %ld, todo: %d)", rlen, dtoh32(usbevent.length));
		gp_port_get_timeout (camera->port, &timeout);
		gp_port_set_timeout (camera->port, PTP2_FAST_TIMEOUT);
		while (dtoh32(usbevent.length) > rlen) {
			result = gp_port_check_int (camera->port, ((char*)&usbevent)+rlen, sizeof(usbevent)-rlen);
			if (result <= 0)
				break;
			rlen += result;
		}
		gp_port_set_timeout (camera->port, timeout);
	}
	/* if we read anything over interrupt endpoint it must be an event */
	/* build an appropriate PTPContainer */
	event->Nparam  = (rlen-12)/4;
	event->Code   = dtoh16(usbevent.code);
	event->SessionID=params->session_id;
	event->Transaction_ID=dtoh32(usbevent.trans_id);
	event->Param1 = dtoh32(usbevent.param1);
	event->Param2 = dtoh32(usbevent.param2);
	event->Param3 = dtoh32(usbevent.param3);
	return PTP_RC_OK;
}

uint16_t
ptp_usb_event_check (PTPParams* params, PTPContainer* event) {

	return ptp_usb_event (params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_usb_event_wait (PTPParams* params, PTPContainer* event) {

	return ptp_usb_event (params, event, PTP_EVENT_CHECK);
}

uint16_t
ptp_usb_control_get_extended_event_data (PTPParams *params, char *buffer, int *size) {
	Camera	*camera = ((PTPData *)params->data)->camera;
	int	ret;

	GP_LOG_D ("Getting extended event data.");
	ret = gp_port_usb_msg_class_read (camera->port, 0x65, 0x0000, 0x0000, buffer, *size);
	if (ret < GP_OK)
		return PTP_ERROR_IO;
	*size = ret;
	return PTP_RC_OK;
}

uint16_t
ptp_usb_control_device_reset_request (PTPParams *params) {
	Camera	*camera = ((PTPData *)params->data)->camera;
	int	ret;

	GP_LOG_D ("Sending usb device reset request.");
	ret = gp_port_usb_msg_class_write (camera->port, 0x66, 0x0000, 0x0000, NULL, 0);
	if (ret < GP_OK)
		return PTP_ERROR_IO;
	return PTP_RC_OK;
}

uint16_t
ptp_usb_control_get_device_status (PTPParams *params, char *buffer, int *size) {
	Camera	*camera = ((PTPData *)params->data)->camera;
	int	ret;

	ret = gp_port_usb_msg_class_read (camera->port, 0x67, 0x0000, 0x0000, buffer, *size);
	if (ret < GP_OK)
		return PTP_ERROR_IO;
	*size = ret;
	return PTP_RC_OK;
}

uint16_t
ptp_usb_control_cancel_request (PTPParams *params, uint32_t transactionid) {
	Camera	*camera = ((PTPData *)params->data)->camera;
	int	ret;
	unsigned char	buffer[6];

	htod16a(&buffer[0],PTP_EC_CancelTransaction);
	htod32a(&buffer[2],transactionid);
	ret = gp_port_usb_msg_class_write (camera->port, 0x64, 0x0000, 0x0000, (char*)buffer, sizeof (buffer));
	if (ret < GP_OK)
		return PTP_ERROR_IO;
	return PTP_RC_OK;
}
