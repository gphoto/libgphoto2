/* olympus_wrap.c
 *
 * Copyright © 2011 Marcus Meissner  <marcus@jet.franken.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
 * 
 *
 * Olympus C-3040Z (and possibly also the C-2040Z and others) have
 * a USB PC Control mode in which "Sierra" protocol packets are tunneled
 * inside another protocol.  This file implements the wrapper protocol.
 * The (ab)use of USB clear halt is not needed for this protocol.
 *
 * IMPORTANT: In order to use this mode, the camera must be switched
 * _out_ of "USB Mass Storage" mode and into "USB PC control mode".
 * The images will not be accessable as a mass-storage/disk device in
 * this mode, but you can control the camera, tell it to take pictures
 * and download images using the protocol implemented in sierra.c.
 *
 * To get to the menu for switching modes, open the memory card
 * access door (the camera senses this) and then press and hold
 * both of the menu buttons until the camera control menu appears.
 * Set it to ON.  This disables the USB mass-storage support.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <_stdint.h>

#include "ptp.h"
#include "ptp-private.h"
#include "ptp-pack.c"
#include "olympus-wrap.h"

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-setting.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>

#define GP_MODULE "ptp2-olympus"

#define CR(result) {int r = (result); if (r < 0) return (r);}

/*
 * The following things are the way the are just to ensure that USB
 * wrapper packets have the correct byte-order on all types of machines.
 * Intel byte order is used instead of "network" byte order :-(
 */
typedef struct
{ unsigned char c1, c2, c3, c4; } uw32_t; /* A type for 32-bit integers */
typedef struct
{ unsigned char c1, c2, c3, c4; } uw4c_t; /* A type for 4-byte things */
typedef struct
{ unsigned char c1, c2; } uw2c_t; /* A type for 2-byte things? */
static uw32_t uw_value(unsigned int value) /* Convert from host-integer to uw32_t */
{
   uw32_t ret;
   ret.c1 = (value       & 0x000000ffUL);
   ret.c2 = (value >> 8  & 0x000000ffUL);
   ret.c3 = (value >> 16 & 0x000000ffUL);
   ret.c4 = (value >> 24 & 0x000000ffUL);
   return ret;
}

static unsigned char
cmdbyte (unsigned char nr) {
	return nr | 0xc0;
}

/* Test for equality between two uw32_t's or two uw4c_t's. */
#define UW_EQUAL(a, b) \
((a).c1==(b).c1 && (a).c2==(b).c2 && (a).c3==(b).c3 && (a).c4==(b).c4)

/*
 * USB wrapper packets start with ASCII bytes "USBC".
 * The responses back from the camera start with "USBS".
 */
#define UW_MAGIC_OUT ((uw4c_t){ 'U','S','B','C' })
#define UW_MAGIC_IN  ((uw4c_t){ 'U','S','B','S' })

static void
make_uw_request(uw4c_t *req,
	unsigned char inout, unsigned char a,
	unsigned char len, unsigned char cmd
) {
	req->c1 = inout;
	req->c2 = a;
	req->c3 = len;
	req->c4 = cmd;
}

#define MAKE_UW_REQUEST_OUT(req, a, b) make_uw_request (req, 0x00, 0x00, a, b) /* write to camera */
#define MAKE_UW_REQUEST_IN(req, a, b) make_uw_request (req, 0x80, 0x00, a, b)  /* read from camera */
/* Move REQUEST_STAT calls to write_packet %%% */

/*
 * Each of the following request types begins with USBC+SessionID and
 * ends with a response from the camera of USBS+SessionID (OK).
 */
#define MAKE_UW_REQUEST_COMMAND(req)		MAKE_UW_REQUEST_OUT(req,0x0c,cmdbyte(0))
#define MAKE_UW_REQUEST_DATAOUT(req)		MAKE_UW_REQUEST_OUT(req,0x0c,cmdbyte(1))
#define MAKE_UW_REQUEST_DATAIN(req)		MAKE_UW_REQUEST_IN (req,0x0c,cmdbyte(2))
#define MAKE_UW_REQUEST_RESPONSE(req)		MAKE_UW_REQUEST_IN (req,0x0c,cmdbyte(3))
#define MAKE_UW_REQUEST_DATAIN_SIZE(req)	MAKE_UW_REQUEST_IN (req,0x0c,cmdbyte(4))

#pragma pack(1)
/*
 * The rest of the USB wrapper packet looks like this:
 */
typedef struct
{
      uw4c_t magic;    /* The letters U S B C for packets sent to camera */
      uw32_t sessionid;        /* Any 32-bit handle the host wants (maybe not 0?) */
      uw32_t rw_length;        /* Length of data to be read or written next */
      uw4c_t request_type;     /* One of the UW_REQUEST_* defines */
      char   zero[3];          /* 00 00 00 ? */
      char   req_camid_len;    /* 00 (or 0x22 if camera id request) */
      char   zero2[4];         /* 00 00 00 ? */
      uw32_t length;           /* Length of transaction repeated? */
      char      zero3[3];      /* 00 00 00 */
} uw_header_t;

/*
 * Data packet to send along with a UW_REQUEST_DATA:
 */
typedef struct
{
      uw32_t length;   /* sizeof(uw_data_t) *plus* sierra pckt after it */
      uw4c_t packet_type;      /* Set this to UW_PACKET_DATA */
      char   zero[56];         /* ? */
} uw_pkout_sierra_hdr_t;

/*
 * Expected response to a UW_REQUEST_STAT:
 */
typedef struct
{
      uw32_t length;   /* Length of this structure, sizeof(uw_stat_t) */
      uw4c_t packet_type;      /* Compare with UW_PACKET_STAT here. */
      char   zero[6];          /* 00 00 00 00 00 00 ? */
} uw_stat_t;

/*
 * Expected response to a get size request, UW_REQUEST_SIZE:
 */
typedef struct
{
      uw32_t length;   /* Length of this structure, sizeof(uw_size_t) */
      uw4c_t packet_type;      /* Compare with UW_PACKET_DATA here. */
      char   zero[4];          /* 00 00 00 00 ? */
      uw32_t size;             /* The size of data waiting to be sent by camera */
} uw_size_t;

/*
 * The end of the response from the camera looks like this:
 */
typedef struct
{
      uw4c_t magic;    /* The letters U S B S for packets from camera */
      uw32_t sessionid;        /* A copy of whatever value the host made up */
      char   zero[5];  /* 00 00 00 00 00 */
} uw_response_t;
#pragma pack()

/*
 * This routine is called after every UW_REQUEST_XXX to get an OK
 * with a matching session ID.
 */
static int
usb_wrap_OK (GPPort *dev, uw_header_t *hdr)
{
   uw_response_t rsp;
   int ret;
   memset(&rsp, 0, sizeof(rsp));

   GP_DEBUG( "usb_wrap_OK" );
   if ((ret = gp_port_read(dev, (char*)&rsp, sizeof(rsp))) != sizeof(rsp))
   {
      gp_log (GP_LOG_DEBUG, GP_MODULE, "gp_port_read *** FAILED (%d vs %d bytes)", (int)sizeof(rsp), ret );
      if (ret < GP_OK)
	return ret;
      return GP_ERROR;
   }
   if (!UW_EQUAL(rsp.magic, UW_MAGIC_IN) ||
       !UW_EQUAL(rsp.sessionid, hdr->sessionid))
   {
      GP_DEBUG( "usb_wrap_OK wrong session *** FAILED" );
      return GP_ERROR;
   }
   /*
    * No idea what these bytes really mean.  Normally they are always 0's
    * when things are in a happy state.
    */
   if (rsp.zero[0] != 0 ||
       rsp.zero[1] != 0 ||
       rsp.zero[2] != 0 ||
       rsp.zero[3] != 0 ||
       rsp.zero[4] != 0)
      {
        GP_DEBUG( "error: ****  usb_wrap_OK failed - not all expected zero bytes are 0" );
        return GP_ERROR;
      }
   return GP_OK;
}

/* This is for the unique session id for the UMS command / response
 * It gets incremented by one for every command.
 */
static int ums_sessionid = 0x42424242;

/* Transaction data phase description */
#define PTP_DP_NODATA           0x0000  /* no data phase */
#define PTP_DP_SENDDATA         0x0001  /* sending data */
#define PTP_DP_GETDATA          0x0002  /* receiving data */
#define PTP_DP_DATA_MASK        0x00ff  /* data phase mask */

uint16_t
ums_wrap_sendreq (PTPParams* params, PTPContainer* req) {
	uw_header_t hdr;
	PTPUSBBulkContainer usbreq;
	char buf[64];
	int ret;
	Camera *camera = ((PTPData *)params->data)->camera;

	GP_DEBUG( "ums_wrap_sendreq" );
	/* Build appropriate USB container */
	usbreq.length= htod32(PTP_USB_BULK_REQ_LEN-
		(sizeof(uint32_t)*(5-req->Nparam)));
	usbreq.type = htod16(PTP_USB_CONTAINER_COMMAND);
	usbreq.code = htod16(req->Code);
	usbreq.trans_id = htod32(req->Transaction_ID);
	usbreq.payload.params.param1 = htod32(req->Param1);
	usbreq.payload.params.param2 = htod32(req->Param2);
	usbreq.payload.params.param3 = htod32(req->Param3);
	usbreq.payload.params.param4 = htod32(req->Param4);
	usbreq.payload.params.param5 = htod32(req->Param5);

	memset(buf,0,sizeof(buf));
	memcpy(buf,&usbreq,usbreq.length);

	memset(&hdr, 0, sizeof(hdr));
	hdr.magic     = UW_MAGIC_OUT;
	hdr.sessionid = uw_value(ums_sessionid);
	ums_sessionid++;
	hdr.rw_length = uw_value(sizeof(buf));
	hdr.length    = uw_value(sizeof(buf));
	MAKE_UW_REQUEST_COMMAND (&hdr.request_type);

	/* First step: send PTP command, first UMS header packet, then data blob  */
	if (	(ret=gp_port_write(camera->port, (char*)&hdr, sizeof(hdr))) < GP_OK ||
		(ret=gp_port_write(camera->port, buf, sizeof(buf))) < GP_OK)
	{
		GP_DEBUG( "ums_wrap_sendreq *** FAILED, ret %d", ret );
		return PTP_ERROR_IO;
	}
	if ((ret=usb_wrap_OK(camera->port, &hdr)) != GP_OK) {
		GP_DEBUG( "ums_wrap_sendreq FAILED to send PTP command" );
		return PTP_ERROR_IO;
	}
	return PTP_RC_OK;
}

uint16_t
ums_wrap_senddata (
	PTPParams* params, PTPContainer* ptp, unsigned long sendlen, PTPDataHandler*getter
) {
	uw_header_t hdr;
	PTPUSBBulkContainer usbreq;
	int ret;
	unsigned long gotlen;
	unsigned char *xdata;
	Camera *camera = ((PTPData *)params->data)->camera;

	GP_DEBUG( "ums_wrap_senddata" );
	memset(&hdr, 0, sizeof(hdr));
	hdr.magic     = UW_MAGIC_OUT;
	hdr.sessionid = uw_value(ums_sessionid);
	ums_sessionid++;
	hdr.rw_length = uw_value(sendlen+12);
	hdr.length    = uw_value(sendlen+12);
	MAKE_UW_REQUEST_DATAOUT (&hdr.request_type);

	xdata = malloc(sendlen + 12);
	usbreq.length = htod32(sendlen + 12);
	usbreq.type   = htod16(PTP_USB_CONTAINER_DATA);
	usbreq.code   = htod16(ptp->Code);
	usbreq.trans_id = htod32(ptp->Transaction_ID);
	memcpy (xdata, &usbreq, 12);
	ret = getter->getfunc(params, getter->priv, sendlen, xdata+12, &gotlen);
	if (ret != PTP_RC_OK) {
		GP_DEBUG( "ums_wrap_senddata *** data get from handler FAILED, ret %d", ret );
		return ret;
	}
	if (gotlen != sendlen) {
		GP_DEBUG( "ums_wrap_senddata *** data get from handler got %ld instead of %ld", gotlen, sendlen );
		return PTP_ERROR_IO;
	}

	if ((ret=gp_port_write(camera->port, (char*)&hdr, sizeof(hdr))) < GP_OK ||
	    (ret=gp_port_write(camera->port, (char*)xdata, sendlen+12)) < GP_OK)
	{
		free (xdata);
		GP_DEBUG( "ums_wrap_senddata *** data send FAILED, ret %d", ret );
		return PTP_ERROR_IO;
	}
	free (xdata);
	if ((ret=usb_wrap_OK(camera->port, &hdr)) != GP_OK) {
		GP_DEBUG( "ums_wrap_senddata FAILED to send PTP data, ret %d", ret );
		return PTP_ERROR_IO;
	}
	return PTP_RC_OK;
}

uint16_t
ums_wrap_getresp(PTPParams* params, PTPContainer* resp)
{
	uw_header_t hdr;
	PTPUSBBulkContainer usbresp;
	char buf[64];
	int ret;
	Camera *camera = ((PTPData *)params->data)->camera;

	GP_DEBUG( "ums_wrap_getresp" );
	memset(&hdr, 0, sizeof(hdr));
	hdr.magic     = UW_MAGIC_OUT;
	hdr.sessionid = uw_value(ums_sessionid);
	ums_sessionid++;
	hdr.rw_length = uw_value(sizeof(buf));
	hdr.length    = uw_value(sizeof(buf));
	MAKE_UW_REQUEST_RESPONSE (&hdr.request_type);
	if ((ret=gp_port_write(camera->port, (char*)&hdr, sizeof(hdr))) < GP_OK ||
	    (ret=gp_port_read(camera->port, buf, sizeof(buf))) != sizeof(buf))
	{
		GP_DEBUG( "ums_wrap_getresp *** FAILED to read PTP response, ret %d", ret );
		return PTP_ERROR_IO;
	}
	if ((ret=usb_wrap_OK(camera->port, &hdr)) != GP_OK) {
		GP_DEBUG( "ums_wrap_getresp FAILED to read UMS reply, ret %d", ret );
		return PTP_ERROR_IO;
	}
	memcpy (&usbresp, buf, sizeof(usbresp));
	resp->Code = dtoh16(usbresp.code);
	resp->Nparam = (dtoh32(usbresp.length)-PTP_USB_BULK_REQ_LEN)/sizeof(uint32_t);
	resp->Param1 = dtoh32(usbresp.payload.params.param1);
	resp->Param2 = dtoh32(usbresp.payload.params.param2);
	resp->Param3 = dtoh32(usbresp.payload.params.param3);
	resp->Param4 = dtoh32(usbresp.payload.params.param4);
	resp->Param5 = dtoh32(usbresp.payload.params.param5);
	return PTP_RC_OK;
}

uint16_t
ums_wrap_getdata (PTPParams* params, PTPContainer* ptp, PTPDataHandler *putter)
{
	uw_header_t hdr;
	PTPUSBBulkContainer usbresp;
	char buf[64];
	int ret;
	Camera *camera = ((PTPData *)params->data)->camera;
	unsigned long recvlen, written;
	char *data;

	GP_DEBUG( "ums_wrap_getdata" );
	memset(&hdr, 0, sizeof(hdr));
	hdr.magic     = UW_MAGIC_OUT;
	hdr.sessionid = uw_value(ums_sessionid);
	ums_sessionid++;
	hdr.rw_length = uw_value(sizeof(buf));
	hdr.length    = uw_value(sizeof(buf));
	MAKE_UW_REQUEST_DATAIN_SIZE (&hdr.request_type);
	if (	(ret=gp_port_write(camera->port, (char*)&hdr, sizeof(hdr))) < GP_OK ||
		(ret=gp_port_read(camera->port, buf, sizeof(buf))) != sizeof(buf))
	{
		GP_DEBUG( "ums_wrap_getdata *** FAILED to read PTP data in size" );
		return PTP_ERROR_IO;
	}
	if ((ret=usb_wrap_OK(camera->port, &hdr)) != GP_OK) {
		GP_DEBUG( "ums_wrap_getdata FAILED to read PTP data in size" );
		return PTP_ERROR_IO;
	}
	memcpy (&usbresp, buf, sizeof(usbresp));
	if ((dtoh16(usbresp.code) != ptp->Code) && (dtoh16(usbresp.code) != PTP_RC_OK)) {
		GP_DEBUG( "ums_wrap_getdata *** PTP code %04x during PTP data in size read", dtoh16(usbresp.code));
		/* break; */
	}
	if (dtoh16(usbresp.length) != 16) {
		GP_DEBUG( "ums_wrap_getdata *** PTP size %d during PTP data in size read, expected 16", dtoh16(usbresp.length));
		return PTP_ERROR_IO;
	}
	recvlen = dtoh32(usbresp.payload.params.param1);
	data = malloc (recvlen);
	if (!data) {
		return PTP_ERROR_IO;
	}
	memset(&hdr, 0, sizeof(hdr));
	hdr.magic     = UW_MAGIC_OUT;
	hdr.sessionid = uw_value(ums_sessionid);
	ums_sessionid++;
	hdr.rw_length = uw_value(recvlen);
	hdr.length    = uw_value(recvlen);
	MAKE_UW_REQUEST_DATAIN (&hdr.request_type);
	if (	(ret=gp_port_write(camera->port, (char*)&hdr, sizeof(hdr))) < GP_OK ||
		(ret=gp_port_read(camera->port, (char*)data, recvlen)) != recvlen)
	{
		GP_DEBUG( "ums_wrap_getdata *** FAILED to read PTP response" );
		free (data);
		return PTP_ERROR_IO;
	}
	if ((ret=usb_wrap_OK(camera->port, &hdr)) != GP_OK) {
		GP_DEBUG( "ums_wrap_getdata FAILED to read PTP data in" );
		free (data);
		return PTP_ERROR_IO;
	}
	/* skip away the 12 byte header */
	ret = putter->putfunc ( params, putter->priv, recvlen - PTP_USB_BULK_HDR_LEN, (unsigned char*)data + PTP_USB_BULK_HDR_LEN, &written);
	free (data);
	if (ret != PTP_RC_OK) {
		GP_DEBUG( "ums_wrap_getdata FAILED to push data into put handle, ret %x", ret );
		return PTP_ERROR_IO;
	}
	if (written != recvlen - PTP_USB_BULK_HDR_LEN) {
		GP_DEBUG( "ums_wrap_getdata FAILED to push data into put handle, len %ld vs %ld", written, recvlen );
		return PTP_ERROR_IO;
	}
	return PTP_RC_OK;
}

#if 0
/*
 * -------------------------------------------------------------------------
 * Here are the two public functions of this C file:
 * -------------------------------------------------------------------------
 */
int
usb_wrap_write_packet (GPPort *dev, unsigned int type, char *sierra_msg, int sierra_len)
{
	GP_DEBUG ("usb_wrap_write_packet");

	CR (usb_wrap_RDY (dev, type));
	CR (usb_wrap_CMND (dev, type, sierra_msg, sierra_len));
	CR (usb_wrap_STAT (dev, type));
	
	return GP_OK;
}

int
usb_wrap_read_packet (GPPort *dev, unsigned int type, char *sierra_response, int sierra_len)
{
	uw32_t uw_size;

	GP_DEBUG ("usb_wrap_read_packet");

	CR (usb_wrap_RDY (dev, type));
	CR (usb_wrap_SIZE (dev, type, &uw_size));
	CR (usb_wrap_DATA (dev, type, sierra_response, &sierra_len, uw_size));
	CR (usb_wrap_STAT (dev, type));
	
	return sierra_len;
}

#endif

/* Decodes an XML blob */
static int
unwrap_xml (char *str, PTPContainer *ptp, unsigned char **data, unsigned long *len) {
	char *x; 
	unsigned int ptpcode, opcode;
	unsigned long param;

#define REMOVESTRING(s)					\
	x = strstr (str, s);					\
	if (x)							\
		memmove (x,x+strlen(s), strlen(x)-strlen(s)+1);
	/* first round of killing wrapping tags */
	GP_DEBUG ("step 0: unwrapping '%s'", str);
	if (!strstr (str, "<x3c xmlns=\"http://www1.olympus-imaging.com/ww/x3c\">")) {
		GP_DEBUG ("ERROR: This is not a wrapped ptp blob?");
		return GP_ERROR;
	}
	REMOVESTRING ("<?xml version=\"1.0\"?>");
	REMOVESTRING ("<x3c xmlns=\"http://www1.olympus-imaging.com/ww/x3c\">");
	REMOVESTRING ("</x3c>");
	GP_DEBUG ("step 1: parsing left over '%s'", str);

	/* kill all \n and \r */
	while (*x) {
		if ((*x == '\n') || (*x == '\r'))
			memmove (x,x+1,strlen(x));
		else
			x++;
	}
	if (strstr (str, "<input>")) { /* This can only be an event */
		REMOVESTRING ("<input>");
		REMOVESTRING ("</input>");
		GP_DEBUG ("step 2: parsing left over '%s' as event input", str);

		if (!strncmp (str,"<e", 2)) {
			if (sscanf (str, "<e%04x", &ptpcode)) {
				ptp->Code = ptpcode;
				ptp->Nparam = 0;
				x = strstr (str, "<param");
				if (x && (sscanf (x, "<param>%08lx</param>", &param))) {
					ptp->Nparam = 1;
					ptp->Param1 = param;
				}
			}
			return GP_OK;
		}
		GP_DEBUG ("step 2: parsing left over input '%s' - type strange?", str);
		return GP_ERROR;
	}
	/* This should now be just command reply + data */
	if (!strstr (str, "<output>")) {
		GP_DEBUG ("step 1: ERROR type not detected!");
		return GP_ERROR;
	}
	REMOVESTRING ("<output>");
	REMOVESTRING ("</output>");
	GP_DEBUG ("step 2: parsing left over '%s' as command output", str);
	x = strstr (str, "<result>");
	if (!x || !sscanf (x, "<result>%x</result>", &ptpcode)) {
		GP_DEBUG ("step 2: ERROR result not detected!");
		return GP_ERROR;
	}
	GP_DEBUG ("result 0x%04x", ptpcode);
	x = strstr (str, "</result><c");
	if (x && sscanf (x,"</result><c%04x", &opcode)) {
		GP_DEBUG ("opcode 0x%04x", opcode);
	}
	return GP_OK;
}

int olympus_wrap_ptp_transaction (PTPParams *params,
	PTPContainer* ptp, uint16_t flags,
	unsigned int sendlen, unsigned char **data, unsigned int *recvlen
) {
	PTPContainer ptp2;
	int res;
	PTPObjectInfo oi;
	char *req, *resp;
        unsigned char* oidata=NULL;
        uint32_t size, handle, newhandle;
	uint16_t ret;

	memset (&ptp2, 0 , sizeof (ptp2));
	ptp2.Code = PTP_OC_SendObjectInfo;
	ptp2.Nparam = 1;
	ptp2.Param1 = 0x80000001;

	req = malloc(2000);
	sprintf (req, "<?xml version=\"1.0\"?>\r\n<x3c xmlns=\"http://www1.olympus-imaging.com/ww/x3c\"><input>");
	sprintf (req+strlen(req), "<c%04x>", PTP_OC_GetDeviceInfo);
#if 0

	case 5: sprintf (req+strlen(req), "<p%04x>", ptp.Param5);
	case 4: sprintf (req+strlen(req), "<p%04x>", ptp.Param4);
	case 3: sprintf (req+strlen(req), "<p%04x>", ptp.Param3);
	case 2: sprintf (req+strlen(req), "<p%04x>", ptp.Param2);
	case 1: sprintf (req+strlen(req), "<p%04x>", ptp.Param1);
	case 0: break;
	}
	<value>0002</value>

	switch (ptp.Nparam) {
	case 5: sprintf (req+strlen(req), "</p%04x>", ptp.Param5);
	case 4: sprintf (req+strlen(req), "</p%04x>", ptp.Param4);
	case 3: sprintf (req+strlen(req), "</p%04x>", ptp.Param3);
	case 2: sprintf (req+strlen(req), "</p%04x>", ptp.Param2);
	case 1: sprintf (req+strlen(req), "</p%04x>", ptp.Param1);
	case 0: break;
	}
#endif
	sprintf (req+strlen(req), "</c%04x>", PTP_OC_GetDeviceInfo);

	sprintf (req+strlen(req), "</input></x3c>\r\n");
	memset (&oi, 0, sizeof (oi));
	oi.ObjectFormat		= PTP_OFC_Script;
	oi.StorageID 		= 0x80000001;
	oi.Filename 		= "HREQUEST.X3C";
	oi.ObjectCompressedSize	= strlen(req);

        size = ptp_pack_OI(params, &oi, &oidata);
        res = ptp_transaction (params, &ptp2, PTP_DP_SENDDATA, size, &oidata, NULL); 
	if (res != PTP_RC_OK)
		return res;
        free(oidata);
	handle = ptp2.Param3;

	ptp2.Code = PTP_OC_SendObject;
	ptp2.Nparam = 0;
	res = ptp_transaction(params, &ptp2, PTP_DP_SENDDATA, strlen(req), (unsigned char**)&req, NULL);
	if (res != PTP_RC_OK)
		return res;
	ret = params->event_wait(params, &ptp2);
	if (ret != PTP_RC_OK)
		return ret;
	GP_DEBUG("event: code %04x, p %08x\n", ptp2.Code, ptp2.Param1);
	if (ptp2.Code != PTP_EC_ObjectAdded) 
		return PTP_RC_OK;
	newhandle = ptp2.Code;
	ret = ptp_getobjectinfo (params, newhandle, &oi);
	if (ret != PTP_RC_OK)
		return ret;
	GP_DEBUG("got new file: %s", oi.Filename);
	ret = ptp_getobject (params, newhandle, (unsigned char**)&resp);
	if (ret != PTP_RC_OK)
		return ret;
	GP_DEBUG("file content: %s", resp);
	unwrap_xml (resp, &ptp2, NULL, NULL);
#if 0
	sprintf (req, "<?xml version=\"1.0\"?>\n<x3c xmlns=\"http://www1.olympus-imaging.com/ww/x3c\">\n<output>\n<result>2001</result>\n<c1016>\n<pD135/>\n</c1016>\n</output>\n</x3c>\n");
#endif
	return PTP_RC_OK;
}
