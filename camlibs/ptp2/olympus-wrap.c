/* olympus_wrap.c
 *
 * Copyright (c) 2012-2013 Marcus Meissner  <marcus@jet.franken.de>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Notes:
 * XDISCVRY.X3C file sent on start, empty.
 */

#define _DEFAULT_SOURCE

#include "config.h"

#ifdef HAVE_LIBXML2

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <_stdint.h>

#include <libxml/parser.h>

#include "ptp.h"
#include "ptp-private.h"
#include "ptp-pack.c"
#include "olympus-wrap.h"

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-setting.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>

/*
 * The following things are the way the are just to ensure that USB
 * wrapper packets have the correct byte-order on all types of machines.
 * Intel byte order is used instead of "network" byte order :-(
 */
typedef struct
{ unsigned char c1, c2, c3, c4; } uw32_t; /* A type for 32-bit integers */
typedef struct
{ unsigned char c1, c2, c3, c4; } uw4c_t; /* A type for 4-byte things */

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
 * USB storage wrapper packets start with ASCII bytes "USBC".
 * The responses back from the camera start with "USBS".
 */
#define UW_MAGIC_OUT ((uw4c_t){ 'U','S','B','C' })
#define UW_MAGIC_IN  ((uw4c_t){ 'U','S','B','S' })

#pragma pack(1)
/*
 * The rest of the USB wrapper packet looks like this:
 */

/* This is linux/include/linux/usb/storage.h, struct bulk_cb_wrap
 * 31 byte length */
typedef struct
{
      uw4c_t magic;		/* The letters U S B C for packets sent to camera */
      uw32_t tag;		/* The SCSI command tag */
      uw32_t rw_length;		/* Length of data to be read or written next */
      unsigned char flags;	/* in / out flag mostly */
      unsigned char lun;	/* 0 here */
      unsigned char length;	/* of the CDB... but 0x0c is used here in the traces */
      unsigned char cdb[16];	
} uw_header_t;

/*
 * This is the end response block from the camera looks like this:
 *
 * This is the generic bulk style USB Storage response block.
 *
 * linux/include/linux/usb/storage.h, struct bulk_cs_wrap */
typedef struct
{
      uw4c_t magic;	/* The letters U S B S for packets from camera */
      uw32_t tag;	/* A copy of whatever value the host made up */
      uw32_t residue;	/* residual read? */
      char   status;	/* status byte */
} uw_response_t;


/* In the SCSI API, the CDB[16] block. */
typedef struct
{
      unsigned char cmd;
      char   zero1[8];
      uw32_t length;
      char   zero2[3];
} uw_scsicmd_t;

#pragma pack()

/* This is for the unique tag id for the UMS command / response
 * It gets incremented by one for every command.
 */
static int ums_tag = 0x42424242;
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

	GP_LOG_D ("usb_wrap_OK");
	if ((ret = gp_port_read(dev, (char*)&rsp, sizeof(rsp))) != sizeof(rsp)) {
		GP_LOG_D ("gp_port_read *** FAILED (%d vs %d bytes)", (int)sizeof(rsp), ret);
		if (ret < GP_OK)
			return ret;
		return GP_ERROR;
	}
	if (	!UW_EQUAL(rsp.magic, UW_MAGIC_IN) ||
		!UW_EQUAL(rsp.tag, hdr->tag))
	{
		GP_LOG_E ("usb_wrap_OK wrong session *** FAILED");
		return GP_ERROR;
	}
	/*
	 * 32bit residual length (0) and 8 bit status (0) are good.
	 */
	if (	rsp.residue.c1 != 0 ||
		rsp.residue.c2 != 0 ||
		rsp.residue.c3 != 0 ||
		rsp.residue.c4 != 0 ||
		rsp.status != 0) {
		GP_LOG_E ("Error: usb_wrap_OK failed - residual non-0 or status %x", rsp.status);
		return GP_ERROR;
	}
	return GP_OK;
}

/* so it mirrors:
	ret = gp_port_send_scsi_cmd (camera->port, 1, (char*)&cmd, sizeof(cmd),
		sense_buffer, sizeof(sense_buffer), (char*)&usbreq, usbreq.length);
 */
static int
scsi_wrap_cmd(
	GPPort *port,
	int todev,
	char *cmd,   unsigned int cmdlen,
	char *sense, unsigned int senselen,
	char *data,  unsigned int size
) {
	uw_header_t		hdr;
	int			ret;

	memset(&hdr, 0, sizeof(hdr));
	hdr.magic	= UW_MAGIC_OUT;
	hdr.tag		= uw_value(ums_tag);
	ums_tag++;
	hdr.rw_length	= uw_value(size);
	hdr.length	= 12; /* seems to be always 12, even as we send 16 byte CDBs */
	hdr.flags	= todev?0:(1<<7);
	hdr.lun		= 0;

	memcpy(hdr.cdb, cmd, cmdlen);

	if ((ret=gp_port_write(port, (char*)&hdr, sizeof(hdr))) < GP_OK) {
		GP_LOG_E ("scsi_wrap_cmd *** FAILED to write scsi cmd");
		return GP_ERROR_IO;
	}
	if (todev) {
		if ((ret=gp_port_write(port, (char*)data, size)) < GP_OK) {
			GP_LOG_E ("scsi_wrap_cmd *** FAILED to write scsi data");
			return GP_ERROR_IO;
		}
	} else {
		if ((ret=gp_port_read(port, (char*)data, size)) < GP_OK) {
			GP_LOG_E ("scsi_wrap_cmd *** FAILED to read scsi data");
			return GP_ERROR_IO;
		}
	}
	if ((ret=usb_wrap_OK(port, &hdr)) != GP_OK) {
		GP_LOG_E ("scsi_wrap_cmd *** FAILED to get scsi reply");
		return GP_ERROR_IO;
	}
	return GP_OK;
}

#define gp_port_send_scsi_cmd scsi_wrap_cmd

/* Transaction data phase description */
#define PTP_DP_NODATA           0x0000  /* no data phase */
#define PTP_DP_SENDDATA         0x0001  /* sending data */
#define PTP_DP_GETDATA          0x0002  /* receiving data */
#define PTP_DP_DATA_MASK        0x00ff  /* data phase mask */

static uint16_t
ums_wrap_sendreq (PTPParams* params, PTPContainer* req, int dataphase) {
	Camera			*camera = ((PTPData *)params->data)->camera;
	PTPUSBBulkContainer	usbreq;
	char			buf[64];
	int			ret;
	uw_scsicmd_t		cmd;
	char			sense_buffer[32];

	GP_LOG_D ("ums_wrap_sendreq");
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

	memset (&cmd, 0, sizeof(cmd));
	cmd.cmd    = cmdbyte(0);
	cmd.length = uw_value(usbreq.length);

	ret = gp_port_send_scsi_cmd (camera->port, 1, (char*)&cmd, sizeof(cmd),
		sense_buffer, sizeof(sense_buffer), (char*)&usbreq, usbreq.length);
	GP_LOG_D ("send_scsi_cmd ret %d", ret);
	return PTP_RC_OK;
}

static uint16_t
ums_wrap_senddata (
	PTPParams* params, PTPContainer* ptp, uint64_t sendlen, PTPDataHandler*getter
) {
	Camera			*camera = ((PTPData *)params->data)->camera;
	PTPUSBBulkContainer	usbreq;
	int			ret;
	unsigned long		gotlen;
	unsigned char		*xdata;
	uw_scsicmd_t		cmd;
	char			sense_buffer[32];

	GP_LOG_D ("ums_wrap_senddata");

	memset (&cmd, 0, sizeof(cmd));
	cmd.cmd    = cmdbyte(1);
	cmd.length = uw_value(sendlen+12);

	xdata = malloc(sendlen + 12);
	usbreq.length = htod32(sendlen + 12);
	usbreq.type   = htod16(PTP_USB_CONTAINER_DATA);
	usbreq.code   = htod16(ptp->Code);
	usbreq.trans_id = htod32(ptp->Transaction_ID);
	memcpy (xdata, &usbreq, 12);
	ret = getter->getfunc(params, getter->priv, sendlen, xdata+12, &gotlen);
	if (ret != PTP_RC_OK) {
		GP_LOG_E ("ums_wrap_senddata *** data get from handler FAILED, ret %d", ret);
		return ret;
	}
	if (gotlen != sendlen) {
		GP_LOG_E ("ums_wrap_senddata *** data get from handler got %ld instead of %ld", gotlen, sendlen);
		return PTP_ERROR_IO;
	}

	ret = gp_port_send_scsi_cmd (camera->port, 1, (char*)&cmd, sizeof(cmd),
		sense_buffer, sizeof(sense_buffer), (char*)xdata, sendlen+12);

	GP_LOG_D ("send_scsi_cmd ret %d", ret);

	free (xdata);

	return PTP_RC_OK;
}

static uint16_t
ums_wrap_getresp (PTPParams* params, PTPContainer* resp)
{
	Camera			*camera  = ((PTPData *)params->data)->camera;
	PTPUSBBulkContainer	usbresp;
	char			buf[64];
	int			ret;
	uw_scsicmd_t		cmd;
	char			sense_buffer[32];

	GP_LOG_D ("ums_wrap_getresp");
	memset (&cmd, 0, sizeof(cmd));
	cmd.cmd    = cmdbyte(3);
	cmd.length = uw_value(sizeof(buf));

	ret = gp_port_send_scsi_cmd (camera->port, 0, (char*)&cmd, sizeof(cmd),
		sense_buffer, sizeof(sense_buffer), (char*)buf, sizeof(buf));

	GP_LOG_D ("send_scsi_cmd ret %d", ret);

	memcpy (&usbresp, buf, sizeof(buf));
	resp->Code = dtoh16(usbresp.code);
	resp->Nparam = (dtoh32(usbresp.length)-PTP_USB_BULK_REQ_LEN)/sizeof(uint32_t);
	resp->Param1 = dtoh32(usbresp.payload.params.param1);
	resp->Param2 = dtoh32(usbresp.payload.params.param2);
	resp->Param3 = dtoh32(usbresp.payload.params.param3);
	resp->Param4 = dtoh32(usbresp.payload.params.param4);
	resp->Param5 = dtoh32(usbresp.payload.params.param5);
	return PTP_RC_OK;
}

static uint16_t
ums_wrap_getdata (PTPParams* params, PTPContainer* ptp, PTPDataHandler *putter)
{
	Camera			*camera = ((PTPData *)params->data)->camera;
	PTPUSBBulkContainer	usbresp;
	char			buf[64];
	int			ret;
	unsigned long		recvlen;
	char			*data;
	uw_scsicmd_t		cmd;
	char			sense_buffer[32];

	GP_LOG_D ("ums_wrap_getdata");

	memset(&cmd,0,sizeof(cmd));
	cmd.cmd	   = cmdbyte(4);
	cmd.length = uw_value(sizeof(buf));

	ret = gp_port_send_scsi_cmd (camera->port, 0, (char*)&cmd, sizeof(cmd),
		sense_buffer, sizeof(sense_buffer), (char*)buf, sizeof(buf));

	GP_LOG_D ("send_scsi_cmd ret %d", ret);

	memcpy (&usbresp, buf, sizeof(buf));
	if ((dtoh16(usbresp.code) != ptp->Code) && (dtoh16(usbresp.code) != PTP_RC_OK)) {
		GP_LOG_D ("ums_wrap_getdata *** PTP code %04x during PTP data in size read", dtoh16(usbresp.code));
		/* break; */
	}
	if (dtoh16(usbresp.length) < 16) {
		recvlen = 0;
		GP_LOG_D ("ums_wrap_getdata *** PTP size %d during PTP data in size read, expected 16", dtoh16(usbresp.length));
	} else {
		recvlen = dtoh32(usbresp.payload.params.param1);
	}
	data = malloc (recvlen);
	if (!data)
		return PTP_ERROR_IO;

	memset(&cmd,0,sizeof(cmd));
	cmd.cmd    = cmdbyte(2);
	cmd.length = uw_value(recvlen);

	ret = gp_port_send_scsi_cmd (camera->port, 0, (char*)&cmd, sizeof(cmd),
		sense_buffer, sizeof(sense_buffer), (char*)data, recvlen);

	GP_LOG_D ("send_scsi_cmd 2 ret  %d", ret);
	/* skip away the 12 byte header */
	if (recvlen >= 16)
		GP_LOG_DATA (data + PTP_USB_BULK_HDR_LEN, recvlen - PTP_USB_BULK_HDR_LEN, "ptp2/olympus/getdata");
	ret = putter->putfunc ( params, putter->priv, recvlen - PTP_USB_BULK_HDR_LEN, (unsigned char*)data + PTP_USB_BULK_HDR_LEN);
	free (data);
	if (ret != PTP_RC_OK) {
		GP_LOG_E ("ums_wrap_getdata FAILED to push data into put handle, ret %x", ret);
		return PTP_ERROR_IO;
	}
	return PTP_RC_OK;
}

static char* generate_event_OK_xml(PTPParams *params, PTPContainer *ptp);
static int parse_event_xml(PTPParams *params, const char *txt, PTPContainer *resp);

static int
olympus_xml_transfer (PTPParams *params,
	char *cmdxml, char **inxml
) {
	PTPContainer	ptp2;
	int		res;
	PTPObjectInfo	oi;
        unsigned char	*resxml, *oidata = NULL;
        uint32_t	size, newhandle;
	uint16_t	ret;
	PTPParams	*outerparams = params->outer_params;

	GP_LOG_D ("olympus_xml_transfer");
	while (1) {
		GP_LOG_D ("... checking camera for events ...");
		ret = outerparams->event_check(outerparams, &ptp2);
		if (ret == PTP_RC_OK) {
			char *evxml;

			GP_LOG_D ("event: code %04x, p %08x", ptp2.Code, ptp2.Param1);

			if (ptp2.Code != PTP_EC_RequestObjectTransfer) {
				ptp_add_event (params, &ptp2);
				goto skip;
			}
			newhandle = ptp2.Param1;
			if ((newhandle & 0xff000000) != 0x1e000000) {
				GP_LOG_D ("event 0x%04x, handle 0x%08x received, no XML event, just passing on", ptp2.Code, ptp2.Param1);
				ptp_add_event (params, &ptp2);
				continue;
			}

			ret = ptp_getobjectinfo (outerparams, newhandle, &oi);
			if (ret != PTP_RC_OK)
				return ret;
	eventhandler:
			GP_LOG_D ("event xml transfer: got new file: %s", oi.Filename);
			ret = ptp_getobject (outerparams, newhandle, (unsigned char**)&resxml);
			if (ret != PTP_RC_OK)
				return ret;
			evxml = malloc (oi.ObjectCompressedSize + 1);
			memcpy (evxml, resxml, oi.ObjectCompressedSize);
			evxml[oi.ObjectCompressedSize] = 0x00;

			GP_LOG_D ("file content: %s", evxml);

			parse_event_xml (params, evxml, &ptp2);
			/* parse it */

			evxml = generate_event_OK_xml(params, &ptp2);

			GP_LOG_D ("... sending XML event reply to camera ... ");
			memset (&ptp2, 0 , sizeof (ptp2));
			ptp2.Code = PTP_OC_SendObjectInfo;
			ptp2.Nparam = 1;
			ptp2.Param1 = 0x80000001;

			memset (&oi, 0, sizeof (oi));
			oi.ObjectFormat		= PTP_OFC_Script;
			oi.StorageID 		= 0x80000001;
			oi.Filename 		= "HRSPONSE.X3C";
			oi.ObjectCompressedSize	= strlen(evxml);
			size = ptp_pack_OI(params, &oi, &oidata);
			res = ptp_transaction (outerparams, &ptp2, PTP_DP_SENDDATA, size, &oidata, NULL); 
			if (res != PTP_RC_OK)
				return res;
			free(oidata);
			/*handle = ptp2.Param3; ... we do not use the returned handle and leave the file on camera. */

			ptp2.Code = PTP_OC_SendObject;
			ptp2.Nparam = 0;
			res = ptp_transaction(outerparams, &ptp2, PTP_DP_SENDDATA, strlen(evxml), (unsigned char**)&evxml, NULL);
			if (res != PTP_RC_OK)
				return res;
			continue;
		}
	skip:

		GP_LOG_D ("... sending XML request to camera ... ");
		memset (&ptp2, 0 , sizeof (ptp2));
		ptp2.Code = PTP_OC_SendObjectInfo;
		ptp2.Nparam = 1;
		ptp2.Param1 = 0x80000001;

		memset (&oi, 0, sizeof (oi));
		oi.ObjectFormat		= PTP_OFC_Script;
		oi.StorageID 		= 0x80000001;
		oi.Filename 		= "HREQUEST.X3C";
		oi.ObjectCompressedSize	= strlen(cmdxml);

/* 
"HRSPONSE.X3C" ... sent back to camera after receiving an event. 
<output><result>2001</result><ec102/></output
 */

		size = ptp_pack_OI(params, &oi, &oidata);
		res = ptp_transaction (outerparams, &ptp2, PTP_DP_SENDDATA, size, &oidata, NULL); 
		if (res != PTP_RC_OK)
			return res;
		free(oidata);
		/*handle = ptp2.Param3; ... we do not use the returned handle and leave the file on camera. */

		ptp2.Code = PTP_OC_SendObject;
		ptp2.Nparam = 0;
		res = ptp_transaction(outerparams, &ptp2, PTP_DP_SENDDATA, strlen(cmdxml), (unsigned char**)&cmdxml, NULL);
		if (res != PTP_RC_OK)
			return res;

		GP_LOG_D ("... waiting for camera ...");
redo:
		ret = outerparams->event_wait(outerparams, &ptp2);
		if (ret != PTP_RC_OK)
			return ret;
		GP_LOG_D ("event: code %04x, p %08x", ptp2.Code, ptp2.Param1);
		if (ptp2.Code != PTP_EC_RequestObjectTransfer) {
			ptp_add_event (params, &ptp2);
			goto redo;
		}
		/* FIXME: check for 0x1e0000* ? */
		newhandle = ptp2.Param1;
		ret = ptp_getobjectinfo (outerparams, newhandle, &oi);
		if (ret != PTP_RC_OK)
			return ret;
		GP_LOG_D ("regular xml transfer: got new file: %s", oi.Filename);
		if (strcmp(oi.Filename,"DRSPONSE.X3C")) {
			GP_LOG_E ("FIXME: regular xml transfer: got new file: %s", oi.Filename);
			goto eventhandler;
		}
		ret = ptp_getobject (outerparams, newhandle, (unsigned char**)&resxml);
		if (ret != PTP_RC_OK)
			return ret;
		*inxml = malloc (oi.ObjectCompressedSize + 1);
		memcpy (*inxml, resxml, oi.ObjectCompressedSize);
		(*inxml)[oi.ObjectCompressedSize] = 0x00;

		GP_LOG_D ("file content: %s", *inxml);
		/* parse it */
		break;
	}
	return PTP_RC_OK;
}

static int
traverse_tree (PTPParams *params, int depth, xmlNodePtr node) {
	xmlNodePtr	next;
	xmlChar		*xchar;
	int n;
	char 		*xx;


	if (!node) return FALSE;
	xx = malloc(depth * 4 + 1);
	memset (xx, ' ', depth*4);
	xx[depth*4] = 0;

	n = xmlChildElementCount (node);

	next = node;
	do {
		ptp_debug(params,"%snode %s", xx, next->name);
		ptp_debug(params,"%selements %d", xx, n);
		xchar = xmlNodeGetContent (next);
		ptp_debug(params,"%scontent %s", xx, xchar);
		traverse_tree (params, depth+1,xmlFirstElementChild (next));
	} while ((next = xmlNextElementSibling (next)));
	free (xx);
	return TRUE;
}

static int
parse_9581_tree (xmlNodePtr node) {
	xmlNodePtr next;

	next = xmlFirstElementChild (node);
	while (next) {
		if (!strcmp ((char*)next->name, "data")) {
			char *decoded, *x;
			char *xchars = (char*)xmlNodeGetContent (next);

			x = decoded = malloc(strlen(xchars)+1);
			while (xchars[0] && xchars[1]) {
				int y;
				sscanf(xchars,"%02x", &y);
				*x++ = y;
				xchars+=2;
			}
			*x = '\0';
			GP_LOG_D ("9581: %s", decoded);


			next = xmlNextElementSibling (next);
			free (decoded);
			continue;
		}
		GP_LOG_E ("9581: unhandled node type %s", next->name);
		next = xmlNextElementSibling (next);
	}
	/*traverse_tree (0, node);*/
	return TRUE;
}

static int
parse_910a_tree (xmlNodePtr node) {
	xmlNodePtr next;

	next = xmlFirstElementChild (node);
	do {
		if (!strcmp ((char*)next->name, "param")) {
			unsigned int x;
			xmlChar *xchar = xmlNodeGetContent (next);
			if (!sscanf((char*)xchar,"%08x", &x)) {
				fprintf(stderr,"could not parse param content %s\n", xchar);
			}
			fprintf(stderr,"param content is 0x%08x\n", x);
			continue;
		}
		fprintf (stderr,"910a: unhandled type %s\n", next->name);
	} while ((next = xmlNextElementSibling (next)));
	/*traverse_tree (0, node);*/
	return TRUE;
}

static int
parse_9302_tree (xmlNodePtr node) {
	xmlNodePtr	next;
	xmlChar		*xchar;

	next = xmlFirstElementChild (node);
	while (next) {
		if (!strcmp((char*)next->name, "x3cVersion")) {
			int x3cver;
			xchar = xmlNodeGetContent (next);
			sscanf((char*)xchar, "%04x", &x3cver);
			GP_LOG_D ("x3cVersion %d.%d", (x3cver>>8)&0xff, x3cver&0xff);
			goto xnext;
		}
		if (!strcmp((char*)next->name, "productIDs")) {
			char *x, *nextspace;
			int len;
			x = (char*)(xchar = xmlNodeGetContent (next));
			GP_LOG_D ("productIDs:");

			do {
				nextspace=strchr(x,' ');
				if (nextspace) nextspace++;

				/* ascii ptp string, 1 byte length, little endian 16 bit chars */
				if (sscanf(x,"%02x", &len)) {
					int i;
					char *str = malloc(len+1);
					for (i=0;i<len;i++) {
						int xc;
						if (sscanf(x+2+i*4,"%04x", &xc)) {
							int cx;

							cx = ((xc>>8) & 0xff) | ((xc & 0xff) << 8);
							str[i] = cx;
						}
						str[len] = 0;
					}
					GP_LOG_D ("\t%s", str);
					free (str);
				}
				x = nextspace;
			} while (x);

			goto xnext;
		}
		GP_LOG_E ("unknown node in 9301: %s", next->name);
xnext:
		next = xmlNextElementSibling (next);
	}
	return TRUE;
}

#if 0
static int
parse_9301_value (PTPParams *params, const char *str, uint16_t type, PTPPropertyValue *propval) {
	switch (type) {
	case 6: { /*UINT32*/
		unsigned int x;
		if (!sscanf(str,"%08x", &x)) {
			ptp_debug( params, "could not parse uint32 %s", str);
			return PTP_RC_GeneralError;
		}
		ptp_debug( params, "\t%d", x);
		propval->u32 = x;
		break;
	}
	case 5: { /*INT32*/
		int x;
		if (!sscanf(str,"%08x", &x)) {
			ptp_debug( params, "could not parse int32 %s", str);
			return PTP_RC_GeneralError;
		}
		ptp_debug( params, "\t%d", x);
		propval->i32 = x;
		break;
	}
	case 4: { /*UINT16*/
		unsigned int x;
		if (!sscanf(str,"%04x", &x)) {
			ptp_debug( params, "could not parse uint16 %s", str);
			return PTP_RC_GeneralError;
		}
		ptp_debug( params, "\t%d", x);
		propval->u16 = x;
		break;
	}
	case 3: { /*INT16*/
		int x;
		if (!sscanf(str,"%04x", &x)) {
			ptp_debug( params, "could not parse int16 %s", str);
			return PTP_RC_GeneralError;
		}
		ptp_debug( params, "\t%d", x);
		propval->i16 = x;
		break;
	}
	case 2: { /*UINT8*/
		unsigned int x;
		if (!sscanf(str,"%02x", &x)) {
			ptp_debug( params, "could not parse uint8 %s", str);
			return PTP_RC_GeneralError;
		}
		ptp_debug( params, "\t%d", x);
		propval->u8 = x;
		break;
	}
	case 1: { /*INT8*/
		int x;
		if (!sscanf(str,"%02x", &x)) {
			ptp_debug( params, "could not parse int8 %s", str);
			return PTP_RC_GeneralError;
		} 
		ptp_debug( params, "\t%d", x);
		propval->i8 = x;
		break;
	}
	case 65535: { /* string */
		int len;

		/* ascii ptp string, 1 byte length, little endian 16 bit chars */
		if (sscanf(str,"%02x", &len)) {
			int i;
			char *xstr = malloc(len+1);
			for (i=0;i<len;i++) {
				int xc;
				if (sscanf(str+2+i*4,"%04x", &xc)) {
					int cx;

					cx = ((xc>>8) & 0xff) | ((xc & 0xff) << 8);
					xstr[i] = cx;
				}
				xstr[len] = 0;
			}
			ptp_debug( params, "\t%s", xstr);
			propval->str = xstr;
			break;
		}
		ptp_debug( params, "string %s not parseable!", str);
		return PTP_RC_GeneralError;
	}
	case 7: /*INT64*/
	case 8: /*UINT64*/
	case 9: /*INT128*/
	case 10: /*UINT128*/
	default:
		ptp_debug( params, "unhandled data type %d!", type);
		return PTP_RC_GeneralError;
	}
	return PTP_RC_OK;
}


static int
parse_9301_propdesc (PTPParams *params, xmlNodePtr node, PTPDevicePropDesc *dpd) {
	xmlNodePtr next;
	int type = -1;

	dpd->FormFlag	= PTP_DPFF_None;
	dpd->GetSet	= PTP_DPGS_Get;
	next = xmlFirstElementChild (node);
	do {
		if (!strcmp((char*)next->name,"type")) {	/* propdesc.DataType */
			if (!sscanf((char*)xmlNodeGetContent (next), "%04x", &type)) {
				ptp_debug( params, "\ttype %s not parseable?",xmlNodeGetContent (next));
				return 0;
			}
			ptp_debug( params, "type 0x%x", type);
			dpd->DataType = type;
			continue;
		}
		if (!strcmp((char*)next->name,"attribute")) {	/* propdesc.GetSet */
			int attr;

			if (!sscanf((char*)xmlNodeGetContent (next), "%02x", &attr)) {
				ptp_debug( params, "\tattr %s not parseable",xmlNodeGetContent (next));
				return 0;
			}
			ptp_debug( params, "attribute 0x%x", attr);
			dpd->GetSet = attr;
			continue;
		}
		if (!strcmp((char*)next->name,"default")) {	/* propdesc.FactoryDefaultValue */
			ptp_debug( params, "default value");
			parse_9301_value (params, (char*)xmlNodeGetContent (next), type, &dpd->FactoryDefaultValue);
			continue;
		}
		if (!strcmp((char*)next->name,"value")) {	/* propdesc.CurrentValue */
			ptp_debug( params, "current value");
			parse_9301_value (params, (char*)xmlNodeGetContent (next), type, &dpd->CurrentValue);
			continue;
		}
		if (!strcmp((char*)next->name,"enum")) {	/* propdesc.FORM.Enum */
			int n,i;
			char *s;

			ptp_debug( params, "enum");
			dpd->FormFlag = PTP_DPFF_Enumeration;
			s = (char*)xmlNodeGetContent (next);
			n = 0;
			do {
				s = strchr(s,' ');
				if (s) s++;
				n++;
			} while (s);
			dpd->FORM.Enum.NumberOfValues = n;
			dpd->FORM.Enum.SupportedValue = malloc (n * sizeof(PTPPropertyValue));
			s = (char*)xmlNodeGetContent (next);
			i = 0;
			do {
				parse_9301_value (params, s, type, &dpd->FORM.Enum.SupportedValue[i]); /* should turn ' ' into \0? */
				i++;
				s = strchr(s,' ');
				if (s) s++;
			} while (s && (i<n));
			continue;
		}
		if (!strcmp((char*)next->name,"range")) {	/* propdesc.FORM.Enum */
			char *s = (char*)xmlNodeGetContent (next);
			dpd->FormFlag = PTP_DPFF_Range;
			ptp_debug( params, "range");
			parse_9301_value (params, s, type, &dpd->FORM.Range.MinimumValue); /* should turn ' ' into \0? */
			s = strchr(s,' ');
			if (!s) continue;
			s++;
			parse_9301_value (params, s, type, &dpd->FORM.Range.MaximumValue); /* should turn ' ' into \0? */
			s = strchr(s,' ');
			if (!s) continue;
			s++;
			parse_9301_value (params, s, type, &dpd->FORM.Range.StepSize); /* should turn ' ' into \0? */

			continue;
		}
		ptp_debug (params, "\tpropdescvar: %s", next->name);
		ptp_debug (params, "\tcontent: %s", (char*)xmlNodeGetContent(next));
		traverse_tree (params, 3, xmlFirstElementChild(next));
	} while ((next = xmlNextElementSibling (next)));
	return PTP_RC_OK;
}

static int
parse_1015_tree (xmlNodePtr node, uint16_t type) {
	PTPPropertyValue	propval;
	xmlNodePtr		next;

	next = xmlFirstElementChild (node);
	return parse_value ((char*)xmlNodeGetContent (next), type, &propval);
}
#endif

static int
traverse_output_tree (PTPParams *params, xmlNodePtr node, PTPContainer *resp) {
	xmlNodePtr next;
	int cmd;

	if (strcmp((char*)node->name,"output")) {
		GP_LOG_E ("node is not output, but %s.", node->name);
		return FALSE;
	}
	if (xmlChildElementCount(node) != 2) {
		GP_LOG_E ("output: expected 2 children, got %ld.", xmlChildElementCount(node));
		return FALSE;
	}
	next = xmlFirstElementChild (node);
	if (!strcmp((char*)next->name,"result")) {
		int result;
		xmlChar *xchar;
		xchar = xmlNodeGetContent (next);
		if (!sscanf((char*)xchar,"%04x",&result))
			GP_LOG_E ("failed scanning result from %s", xchar);
		resp->Code = result;
		GP_LOG_D ("ptp result is 0x%04x", result);
	
	}
	next = xmlNextElementSibling (next);
	if (!sscanf ((char*)next->name, "c%04x", &cmd)) {
		GP_LOG_E ("expected c<HEX>, have %s", next->name);
		return FALSE;
	}
	GP_LOG_D ("cmd is 0x%04x", cmd);
	switch (cmd) {
	/* reviewed OK. */
	case PTP_OC_OLYMPUS_Capture:	return TRUE;
	case PTP_OC_GetDevicePropDesc:	return TRUE;

	/* TODO */
#if 0
	case PTP_OC_OLYMPUS_GetDeviceInfo: return parse_9301_tree (next); /* 9301 */
#endif
	case PTP_OC_OLYMPUS_OpenSession: return parse_9302_tree (next);
	case PTP_OC_OLYMPUS_GetCameraControlMode: return parse_910a_tree (next);
	case PTP_OC_OLYMPUS_GetCameraID: return parse_9581_tree (next);

	case PTP_OC_SetDevicePropValue: /* <output>\n<result>2001</result>\n<c1016>\n<pD135/>\n</c1016>\n</output> */
		/* we could cross check the parameter, but its not strictly necessary */
		return TRUE;
#if 0
	case PTP_OC_GetDevicePropValue: return parse_1015_tree ( next , PTP_DTC_UINT32);
#endif
	default:
		return traverse_tree (params, 0, next);
	}
	return FALSE;
}

static int
traverse_input_tree (PTPParams *params, xmlNodePtr node, PTPContainer *resp) {
	unsigned int	curpar = 0;
	int		evt;
	xmlNodePtr	next = xmlFirstElementChild (node);
	uint32_t	pars[5];


	if (!next) {
		GP_LOG_E ("no nodes below input.");
		return FALSE;
	}

	resp->Code = 0;
	while (next) {
		if (sscanf((char*)next->name,"e%x",&evt)) {
			resp->Code = evt;

			switch (evt) {
			case PTP_EC_Olympus_PropertyChanged: {
				xmlNodePtr propidnode = xmlFirstElementChild(next);

				/* Gets a list of property that changed ... stuff into
				 * event queue. */
				while (propidnode) {
					int propid;

					if (sscanf((char*)propidnode->name,"p%x", &propid)) {
						PTPContainer ptp;

						memset(&ptp, 0, sizeof(ptp));
						ptp.Code = PTP_EC_DevicePropChanged;
						ptp.Nparam = 1;
						ptp.Param1 = propid;
						ptp_add_event (params, &ptp);
					}
					propidnode = xmlNextElementSibling (propidnode);
				}
				break;
			}
			default:
				if (xmlChildElementCount(node) != 0) {
					GP_LOG_E ("event %s hat tree below?", (char*)next->name);
					traverse_tree (params, 0, xmlFirstElementChild(next));
				}
			}
			next = xmlNextElementSibling (next);
			continue;
		}
		if (!strcmp((char*)next->name,"param")) {
			int x;
			if (sscanf((char*)xmlNodeGetContent(next),"%x", &x)) {
				if (curpar < sizeof(pars)/sizeof(pars[0]))
					pars[curpar++] = x;
				else
					GP_LOG_E ("ignore superfluous argument %s/%x", (char*)xmlNodeGetContent(next), x);
			}
			next = xmlNextElementSibling (next);
			continue;
		}
		GP_LOG_E ("parsing event input node, unknown node %s", (char*)next->name);
		next = xmlNextElementSibling (next);
	}
	resp->Nparam = curpar;
	switch (curpar) {
	case 5: resp->Param5 = pars[4];
	case 4: resp->Param4 = pars[3];
	case 3: resp->Param3 = pars[2];
	case 2: resp->Param2 = pars[1];
	case 1: resp->Param1 = pars[0];
	case 0: break;
	}
	/* FIXME: decode content and inject into PTP event queue. */
	return TRUE;
}

static int
traverse_x3c_tree (PTPParams *params, xmlNodePtr node, PTPContainer *resp) {
	xmlNodePtr	next;

	if (!node)
		return FALSE;
	if (strcmp((char*)node->name,"x3c")) {
		GP_LOG_E ("node is not x3c, but %s.", node->name);
		return FALSE;
	}
	if (xmlChildElementCount(node) != 1) {
		GP_LOG_E ("x3c: expected 1 child, got %ld.", xmlChildElementCount(node));
		return FALSE;
	}
	next = xmlFirstElementChild (node);
	if (!strcmp((char*)next->name, "output"))
		return traverse_output_tree (params, next, resp);
	if (!strcmp((char*)next->name, "input"))
		return traverse_input_tree (params, next, resp); /* event */
	GP_LOG_E ("unknown name %s below x3c.", next->name);
	return FALSE;
}

static int
traverse_x3c_event_tree (PTPParams *params, xmlNodePtr node, PTPContainer *resp) {
	xmlNodePtr	next;

	if (!node)
		return FALSE;
	if (strcmp((char*)node->name,"x3c")) {
		GP_LOG_E ("node is not x3c, but %s.", node->name);
		return FALSE;
	}
	if (xmlChildElementCount(node) != 1) {
		GP_LOG_E ("x3c: expected 1 child, got %ld.", xmlChildElementCount(node));
		return FALSE;
	}
	next = xmlFirstElementChild (node);
	if (!strcmp((char*)next->name, "input"))
		return traverse_input_tree (params, next, resp); /* event */
	GP_LOG_E ("unknown name %s below x3c.", next->name);
	return FALSE;
}

static int
parse_xml(PTPParams *params, const char *txt, PTPContainer *resp) {
	xmlDocPtr	docin;
	xmlNodePtr	docroot;

	docin = xmlReadMemory (txt, strlen(txt), "http://gphoto.org/", "utf-8", 0);
	if (!docin) return FALSE;
	docroot = xmlDocGetRootElement (docin);
	if (!docroot) return FALSE;
	return traverse_x3c_tree (params, docroot, resp);
}

static int
parse_event_xml(PTPParams *params, const char *txt, PTPContainer *resp) {
	xmlDocPtr	docin;
	xmlNodePtr	docroot;

	docin = xmlReadMemory (txt, strlen(txt), "http://gphoto.org/", "utf-8", 0);
	if (!docin) return FALSE;
	docroot = xmlDocGetRootElement (docin);
	if (!docroot) return FALSE;
	return traverse_x3c_event_tree (params, docroot, resp);
}

static void
encode_command (xmlNodePtr inputnode, PTPContainer *ptp, unsigned char *data, int len)
{
	xmlNodePtr	cmdnode;
	char 		code[20];

	sprintf(code,"c%04X", ptp->Code);
	cmdnode 	= xmlNewChild (inputnode, NULL, (xmlChar*)code, NULL);

	switch (ptp->Code) {
	case 0x1014: { /* OK */
		sprintf (code, "p%04X", ptp->Param1);
		xmlNewChild (cmdnode, NULL, (xmlChar*)code, NULL);
		break;
	}
	case 0x1016: {
		char buf[20];
		xmlNodePtr	pnode;
		/* zb <c1016><pD10D><value>000A000D</value></pD10D></c1016> */
		/* FIXME: might still be wrong. */
		/* We can directly byte encode the data we get from the PTP stack */
		/* ... BUT the byte order is bigendian (printed) vs encoded */
		int i;
		char *x = malloc (len*2+1);

		if (len <= 4) { /* just dump the bytes in big endian byteorder */
			for (i=0;i<len;i++)
				sprintf(x+2*i,"%02X",data[len-i-1]);
		} else {
			for (i=0;i<len;i++)
				sprintf(x+2*i,"%02X",data[i]);
		}
		sprintf(buf,"p%04X", ptp->Param1);
		pnode = xmlNewChild (cmdnode, NULL, (xmlChar*)buf, NULL);
		xmlNewChild (pnode, NULL, (xmlChar*)"value", (xmlChar*)x);
		free (x);
		break;
	}
	default:
		if (ptp->Nparam) {
			switch (ptp->Nparam) {
			case 1:
				sprintf (code, "%08X", ptp->Param1);
				xmlNewChild (cmdnode, NULL, (xmlChar*)"param", (xmlChar*)code);
				break;
			case 2:
				sprintf (code, "%08X", ptp->Param1);
				xmlNewChild (cmdnode, NULL, (xmlChar*)"param", (xmlChar*)code);
				sprintf (code, "%08X", ptp->Param2);
				xmlNewChild (cmdnode, NULL, (xmlChar*)"param", (xmlChar*)code);
				break;
			}
		}
		break;
	}
}

static char*
generate_event_OK_xml(PTPParams *params, PTPContainer *ptp) {
	xmlDocPtr	docout;
	xmlChar		*output;
	int		len;
	xmlNodePtr	x3cnode, inputnode;
	char		buf[10];

	/* 
	"HRSPONSE.X3C" ... sent back to camera after receiving an event. 
	<output><result>2001</result><ec102/></output
	 */

	docout 		= xmlNewDoc ((xmlChar*)"1.0");
	x3cnode		= xmlNewDocNode (docout, NULL, (xmlChar*)"x3c", NULL);
	                  xmlNewNs (x3cnode,(xmlChar*)"http://www1.olympus-imaging.com/ww/x3c",NULL);
	inputnode 	= xmlNewChild (x3cnode, NULL, (xmlChar*)"output", NULL);

	sprintf (buf,"e%04X", ptp->Code);

	xmlNewChild (inputnode,  NULL, (xmlChar*)"result", (xmlChar*)"2001");
	xmlNewChild (inputnode,  NULL, (xmlChar*)buf, NULL);

	xmlDocSetRootElement (docout, x3cnode);
	xmlDocDumpMemory (docout, &output, &len);

	GP_LOG_D ("generated xml is:");
	GP_LOG_D ("%s", output);

	/* NOTE: Windows driver generates XML with CRLF, Unix just creates XML with LF.
	 * Olympus E-410 does not seem to care. */
	return (char*)output;
}
static char*
generate_xml(PTPParams *params, PTPContainer *ptp, unsigned char *data, int len) {
	xmlDocPtr	docout;
	xmlChar		*output;
	xmlNodePtr	x3cnode;
	xmlNodePtr	inputnode;

	docout 		= xmlNewDoc ((xmlChar*)"1.0");
	x3cnode		= xmlNewDocNode (docout, NULL, (xmlChar*)"x3c", NULL);
	                  xmlNewNs (x3cnode,(xmlChar*)"http://www1.olympus-imaging.com/ww/x3c",NULL);
	inputnode 	= xmlNewChild (x3cnode, NULL, (xmlChar*)"input", NULL);

	/* The fun starts in here: */
	encode_command (inputnode, ptp, data, len);

	xmlDocSetRootElement (docout, x3cnode);
	xmlDocDumpMemory (docout, &output, &len);

	GP_LOG_D ("generated xml is:");
	GP_LOG_D ("%s", output);

	/* NOTE: Windows driver generates XML with CRLF, Unix just creates XML with LF.
	 * Olympus E-410 does not seem to care. */
	return (char*)output;
}

static int
is_outer_operation (PTPParams* params, uint16_t opcode) {
	unsigned int i;

	GP_LOG_D ("is_outer_operation %04x", opcode);
	/* the ones we need before we can do getdeviceinfo */
	if (opcode == PTP_OC_OpenSession)	return 1;
	if (opcode == PTP_OC_SendObjectInfo)	return 1;
	if (opcode == PTP_OC_SendObject)	return 1;
	if (opcode == PTP_OC_GetDeviceInfo)	return 1;
	if (opcode == PTP_OC_GetStorageIDs)	return 1;

	/* all vendor ones are XML driven. */
	if ((opcode & 0x8000) == 0x8000) return 0;

	/* Do nothing here, either do stuff in senddata, getdata or getresp,
	 * which will get the PTPContainer req too. */
        for (i=0;i<params->outer_deviceinfo.OperationsSupported_len;i++)
                if (params->outer_deviceinfo.OperationsSupported[i]==opcode)
                        return TRUE;
	GP_LOG_D ("is_outer_operation %04x - is WRAPPED", opcode);
	return FALSE;
}

static uint16_t
ums_wrap2_event_check (PTPParams* params, PTPContainer* req)
{
	PTPContainer	ptp2;
	int		res;
	PTPObjectInfo	oi;
        unsigned char	*resxml, *oidata = NULL;
        uint32_t	size, newhandle;
	uint16_t	ret;
	PTPParams	*outerparams = params->outer_params;
	char		*evxml;

	GP_LOG_D ("ums_wrap2_event_check");

	while (1) {
		ret = outerparams->event_check(outerparams, &ptp2);
		if (ret != PTP_RC_OK)
			return ret;

		GP_LOG_D ("event: code %04x, p %08x", ptp2.Code, ptp2.Param1);

		if (ptp2.Code != PTP_EC_RequestObjectTransfer) {
			GP_LOG_D ("event 0x%04x received, just passing on", ptp2.Code);
			memcpy (req, &ptp2, sizeof(ptp2));
			return PTP_RC_OK;
		}

		newhandle = ptp2.Param1;

		if ((newhandle & 0xff000000) != 0x1e000000) {
			GP_LOG_D ("event 0x%04x, handle 0x%08x received, no XML event, just passing on", ptp2.Code, ptp2.Param1);
			ptp_add_event (params, &ptp2);
			continue;
		}

		ret = ptp_getobjectinfo (outerparams, newhandle, &oi);
		if (ret != PTP_RC_OK)
			return ret;
		GP_LOG_D ("event xml: got new file: %s", oi.Filename);
		if (!strstr(oi.Filename,".X3C")) {
			GP_LOG_D ("PTP_EC_RequestObjectTransfer with non XML filename %s", oi.Filename);
			memcpy (req, &ptp2, sizeof(ptp2));
			return PTP_RC_OK;
		}
		ret = ptp_getobject (outerparams, newhandle, (unsigned char**)&resxml);
		if (ret != PTP_RC_OK)
			return ret;
		evxml = malloc (oi.ObjectCompressedSize + 1);
		memcpy (evxml, resxml, oi.ObjectCompressedSize);
		evxml[oi.ObjectCompressedSize] = 0x00;

		GP_LOG_D ("file content: %s", evxml);

		/* FIXME: handle the case where we get a non X3C file, like during capture */

		/* parse it  ... into req */
		parse_event_xml (params, evxml, req);

		/* generate reply */
		evxml = generate_event_OK_xml(params, req);

		GP_LOG_D ("... sending XML event reply to camera ... ");
		memset (&ptp2, 0 , sizeof (ptp2));
		ptp2.Code = PTP_OC_SendObjectInfo;
		ptp2.Nparam = 1;
		ptp2.Param1 = 0x80000001;

		memset (&oi, 0, sizeof (oi));
		oi.ObjectFormat		= PTP_OFC_Script;
		oi.StorageID 		= 0x80000001;
		oi.Filename 		= "HRSPONSE.X3C";
		oi.ObjectCompressedSize	= strlen(evxml);
		size = ptp_pack_OI(params, &oi, &oidata);
		res = ptp_transaction (outerparams, &ptp2, PTP_DP_SENDDATA, size, &oidata, NULL); 
		if (res != PTP_RC_OK)
			return res;
		free(oidata);
		/*handle = ptp2.Param3; ... we do not use the returned handle and leave the file on camera. */

		ptp2.Code = PTP_OC_SendObject;
		ptp2.Nparam = 0;
		res = ptp_transaction(outerparams, &ptp2, PTP_DP_SENDDATA, strlen(evxml), (unsigned char**)&evxml, NULL);
		if (res != PTP_RC_OK)
			return res;
		return PTP_RC_OK;
	}
}

static uint16_t
ums_wrap2_sendreq (PTPParams* params, PTPContainer* req, int dataphase)
{
	GP_LOG_D ("ums_wrap2_sendreq");
	if (is_outer_operation (params,req->Code))
		return ums_wrap_sendreq (params,req,dataphase);
	/* We do stuff in either senddata, getdata or getresp, not here. */
	params->olympus_cmd   = NULL;
	params->olympus_reply = NULL;
	return PTP_RC_OK;
}

static uint16_t
ums_wrap2_senddata (
	PTPParams* params, PTPContainer* ptp, uint64_t sendlen, PTPDataHandler*getter
) {
	unsigned char	*data;
	uint16_t	ret;
	unsigned long	gotlen;

	if (is_outer_operation (params, ptp->Code))
		return ums_wrap_senddata (params, ptp, sendlen, getter);

	GP_LOG_D ("ums_wrap2_senddata");
	data = malloc (sendlen);
	ret = getter->getfunc(params, getter->priv, sendlen, data, &gotlen);
	if (ret != PTP_RC_OK) {
		GP_LOG_D ("ums_wrap2_senddata *** data get from handler FAILED, ret %d", ret);
		return ret;
	}
	params->olympus_cmd = generate_xml (params, ptp, data, sendlen);
	free (data);
	/* Do not do stuff yet, do it in getresp */
	return PTP_RC_OK;
}

static uint16_t
ums_wrap2_getdata (PTPParams* params, PTPContainer* ptp, PTPDataHandler *putter) {
	char		*resxml = NULL;
	uint16_t	ret;

	if (is_outer_operation (params, ptp->Code))
		return ums_wrap_getdata (params, ptp, putter);

	GP_LOG_D ("ums_wrap2_getdata");

	/* Either send or get data, not both. olympus_cmd is NULL now */
	params->olympus_cmd = generate_xml (params, ptp, NULL, 0);

	/* Do the fun stuff. */
	ret = olympus_xml_transfer (params, params->olympus_cmd, &resxml);
	if (ret != PTP_RC_OK)
		return ret;

	/* Remember the returned XML for getresp() for the PTP return code. */
	params->olympus_reply = resxml;
	switch (ptp->Code) {
#if 0
	case PTP_OC_GetDevicePropDesc: {
		PTPPropertyDesc	dpd;
		parse_9301_propdesc (xmlFirstElementChild (next), &dpd);
		/* decode the XML ... reencode the binary presentation of the propdesc */
		break;
	}
#endif
	default:
		/* Just put the XML blob as-is as data... It will be processed in ptp.c */
		return putter->putfunc(params,putter->priv,strlen(resxml)+1,(unsigned char*)resxml);
	}
}

static uint16_t
ums_wrap2_getresp (PTPParams* params, PTPContainer* resp) {
	int ret;

	if (is_outer_operation(params, resp->Code))
		return ums_wrap_getresp (params, resp);

	GP_LOG_D ("ums_wrap2_getresp");
	if (!params->olympus_cmd) /* no data phase at all */
		params->olympus_cmd = generate_xml (params, resp, NULL, 0);
	if (!params->olympus_reply) {
		/* Do the actual handshake here. */
		ret = olympus_xml_transfer (params, params->olympus_cmd, &params->olympus_reply);
		if (ret != PTP_RC_OK) {
			GP_LOG_E ("ums_wrap2_getresp: error %x from transfer", ret);
			return ret;
		}
	}
	parse_xml (params, params->olympus_reply, resp);
	return PTP_RC_OK;
}

uint16_t
olympus_setup (PTPParams *params) {
	PTPParams	*outerparams;

	params->getresp_func	= ums_wrap2_getresp;
	params->senddata_func	= ums_wrap2_senddata;
	params->getdata_func	= ums_wrap2_getdata;
	params->sendreq_func	= ums_wrap2_sendreq;

	params->event_check	= ums_wrap2_event_check;
	params->event_wait	= ums_wrap2_event_check;

	params->outer_params = outerparams = malloc (sizeof(PTPParams));
	memcpy(outerparams, params, sizeof(PTPParams));
	outerparams->sendreq_func	= ums_wrap_sendreq;
	outerparams->getresp_func	= ums_wrap_getresp;
	outerparams->senddata_func	= ums_wrap_senddata;
	outerparams->getdata_func	= ums_wrap_getdata;
	outerparams->getdata_func	= ums_wrap_getdata;

	/* events come just as PTP events */
	outerparams->event_check	= ptp_usb_event_check;
	outerparams->event_wait		= ptp_usb_event_wait;

	return PTP_RC_OK;
}
#endif /* HAVE_LIBXML2 */
