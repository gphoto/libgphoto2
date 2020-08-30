/* fujiptpip.c
 *
 * Copyright (C) 2020 Marcus Meissner <marcus@jet.franken.de>
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
/*
 * This is a copy of ptpip.c...
 *
 * This is working, but unfinished!
 * - Event handling not finished.
 * - Some configure checking magic missing for the special header files
 *   and functions.
 * - Not everything implementation correctly cross checked.
 * - Coolpix P3 does not give transfer status (image 000x/000y), and reports an
 *   error when transfers finish correctly.
 *
 * Nikon WU-1* adapters might use 0011223344556677 as GUID always...
 */
#define _DEFAULT_SOURCE
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef WIN32
# include <winsock.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

#include "ptp.h"
#include "ptp-private.h"

#define PTPIP_VERSION_MAJOR 0x0001
#define PTPIP_VERSION_MINOR 0x0000

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

#include "ptp.h"
#include "ptp-bugs.h"

#include "ptp-pack.c"

#define fujiptpip_len		0
#define fujiptpip_type		4	/* not always present */

#define fujiptpip_cmd_dataphase	4
#define fujiptpip_cmd_code	6
#define fujiptpip_cmd_transid	8
#define fujiptpip_cmd_param1	12
#define fujiptpip_cmd_param2	16
#define fujiptpip_cmd_param3	20
#define fujiptpip_cmd_param4	24
#define fujiptpip_cmd_param5	28

#define PTP_EVENT_CHECK			0x0000	/* waits for */
#define PTP_EVENT_CHECK_FAST		0x0001	/* checks */
static uint16_t ptp_fujiptpip_check_event (PTPParams* params);
static uint16_t ptp_fujiptpip_event (PTPParams* params, PTPContainer* event, int wait);

/* send / receive functions */
uint16_t
ptp_fujiptpip_sendreq (PTPParams* params, PTPContainer* req, int dataphase)
{
	int		ret;
	int		len = fujiptpip_cmd_param1 +req->Nparam*4;
	unsigned char 	*request = malloc(len);

	switch (req->Nparam) {
	default:
	case 0: GP_LOG_D ("Sending PTP_OC 0x%0x (%s) request...", req->Code, ptp_get_opcode_name(params, req->Code));
		break;
	case 1: GP_LOG_D ("Sending PTP_OC 0x%0x (%s) (0x%x) request...", req->Code, ptp_get_opcode_name(params, req->Code), req->Param1);
		break;
	case 2: GP_LOG_D ("Sending PTP_OC 0x%0x (%s) (0x%x,0x%x) request...", req->Code, ptp_get_opcode_name(params, req->Code), req->Param1, req->Param2);
		break;
	case 3: GP_LOG_D ("Sending PTP_OC 0x%0x (%s) (0x%x,0x%x,0x%x) request...", req->Code, ptp_get_opcode_name(params, req->Code), req->Param1, req->Param2, req->Param3);
		break;
	}

	ptp_fujiptpip_check_event (params);

	/*htod32a(&request[ptpip_type],PTPIP_CMD_REQUEST);*/
	htod32a(&request[fujiptpip_len],len);
	/* sending data = 2, receiving data or no data = 1 */
	/*if ((dataphase&PTP_DP_DATA_MASK) == PTP_DP_SENDDATA)
		htod16a(&request[fujiptpip_cmd_dataphase],2);
	else*/
		htod16a(&request[fujiptpip_cmd_dataphase],1);
	htod16a(&request[fujiptpip_cmd_code],req->Code);
	htod32a(&request[fujiptpip_cmd_transid],req->Transaction_ID);

	switch (req->Nparam) {
	case 5: htod32a(&request[fujiptpip_cmd_param5],req->Param5);/* fallthrough */
	case 4: htod32a(&request[fujiptpip_cmd_param4],req->Param4);/* fallthrough */
	case 3: htod32a(&request[fujiptpip_cmd_param3],req->Param3);/* fallthrough */
	case 2: htod32a(&request[fujiptpip_cmd_param2],req->Param2);/* fallthrough */
	case 1: htod32a(&request[fujiptpip_cmd_param1],req->Param1);/* fallthrough */
	case 0:
	default:
		break;
	}
	GP_LOG_DATA ( (char*)request, len, "ptpip/oprequest data:");
	ret = write (params->cmdfd, request, len);
	free (request);
	if (ret == -1)
		perror ("sendreq/write to cmdfd");
	if (ret != len) {
		GP_LOG_E ("ptp_fujiptpip_sendreq() len =%d but ret=%d", len, ret);
		return PTP_RC_OK;
	}
	return PTP_RC_OK;
}

static uint16_t
ptp_fujiptpip_generic_read (PTPParams *params, int fd, PTPIPHeader *hdr, unsigned char**data, int withtype) {
	int	ret, len, curread;
	unsigned char *xhdr;
	unsigned int hdrlen;

	xhdr = (unsigned char*)hdr; curread = 0;
	hdrlen = len = sizeof (PTPIPHeader);
	if (!withtype)
		hdrlen = len = sizeof(uint32_t);

	while (curread < len) {
		ret = read (fd, xhdr + curread, len - curread);
		if (ret == -1) {
			perror ("read PTPIPHeader");
			return PTP_RC_GeneralError;
		}
		GP_LOG_DATA ((char*)xhdr+curread, ret, "ptpip/generic_read header:");
		curread += ret;
		if (ret == 0) {
			GP_LOG_E ("End of stream after reading %d bytes of ptpipheader", ret);
			return PTP_RC_GeneralError;
		}
	}
	len = dtoh32 (hdr->length) - hdrlen;
	if (len < 0) {
		GP_LOG_E ("len < 0, %d?", len);
		return PTP_RC_GeneralError;
	}
	*data = malloc (len);
	if (!*data) {
		GP_LOG_E ("malloc failed.");
		return PTP_RC_GeneralError;
	}
	curread = 0;
	while (curread < len) {
		ret = read (fd, (*data)+curread, len-curread);
		if (ret == -1) {
			GP_LOG_E ("error %d in reading PTPIP data", errno);
			free (*data);*data = NULL;
			return PTP_RC_GeneralError;
		} else {
			GP_LOG_DATA ((char*)((*data)+curread), ret, "ptpip/generic_read data:");
		}
		if (ret == 0)
			break;
		curread += ret;
	}
	if (curread != len) {
		GP_LOG_E ("read PTPIP data, ret %d vs len %d", ret, len);
		free (*data);*data = NULL;
		return PTP_RC_GeneralError;
	}
	return PTP_RC_OK;
}

static uint16_t
ptp_fujiptpip_cmd_read (PTPParams* params, PTPIPHeader *hdr, unsigned char** data) {
	ptp_fujiptpip_check_event (params);
	return ptp_fujiptpip_generic_read (params, params->cmdfd, hdr, data, 0);
}

static uint16_t
ptp_fujiptpip_evt_read (PTPParams* params, PTPIPHeader *hdr, unsigned char** data) {
	return ptp_fujiptpip_generic_read (params, params->evtfd, hdr, data, 0);
}

static uint16_t
ptp_fujiptpip_check_event (PTPParams* params) {
	PTPContainer	event;
	uint16_t	ret;

	event.Code = 0;
	ret = ptp_fujiptpip_event (params, &event, PTP_EVENT_CHECK_FAST);
	if (ret != PTP_RC_OK)
		return ret;
	if (event.Code == 0)
		return ret;
	return ptp_add_event (params, &event);
}

#define fujiptpip_data_datatype		4
#define fujiptpip_data_code		6
#define fujiptpip_data_transid		8
#define fujiptpip_data_payload		12

#define WRITE_BLOCKSIZE 65536
uint16_t
ptp_fujiptpip_senddata (PTPParams* params, PTPContainer* ptp,
		uint64_t size, PTPDataHandler *handler
) {
	unsigned char	request[fujiptpip_data_payload];
	unsigned int	curwrite, towrite;
	int		ret;
	unsigned char*	xdata;

	GP_LOG_D ("Sending PTP_OC 0x%0x (%s) data...", ptp->Code, ptp_get_opcode_name(params, ptp->Code));
	//htod32a(&request[ptpip_type],PTPIP_START_DATA_PACKET);
	htod32a(&request[fujiptpip_len],sizeof(request)+size);
	htod16a(&request[fujiptpip_data_datatype],2);
	htod16a(&request[fujiptpip_data_code],ptp->Code);
	htod32a(&request[fujiptpip_data_transid],ptp->Transaction_ID);
	GP_LOG_DATA ((char*)request, sizeof(request), "ptpip/senddata header:");
	ret = write (params->cmdfd, request, sizeof(request));
	if (ret == -1)
		perror ("sendreq/write to cmdfd");
	if (ret != sizeof(request)) {
		GP_LOG_E ("ptp_fujiptpip_senddata() len=%d but ret=%d", (int)sizeof(request), ret);
		return PTP_RC_GeneralError;
	}
	xdata = malloc(WRITE_BLOCKSIZE);
	if (!xdata) return PTP_RC_GeneralError;
	curwrite = 0;
	while (curwrite < size) {
		unsigned long written, towrite2, xtowrite;

		ptp_fujiptpip_check_event (params);

		towrite = size - curwrite;
		if (towrite > WRITE_BLOCKSIZE) {
			towrite	= WRITE_BLOCKSIZE;
		}
		ret = handler->getfunc (params, handler->priv, towrite, xdata, &xtowrite);
		if (ret == -1) {
			perror ("getfunc in senddata failed");
			free (xdata);
			return PTP_RC_GeneralError;
		}
		towrite2 = xtowrite;
		GP_LOG_DATA ((char*)xdata, towrite2, "ptpip/senddata data:");
		written = 0;
		while (written < towrite2) {
			ret = write (params->cmdfd, xdata+written, towrite2-written);
			if (ret == -1) {
				perror ("write in senddata failed");
				free (xdata);
				return PTP_RC_GeneralError;
			}
			written += ret;
		}
		curwrite += towrite;
	}
	free (xdata);
	return PTP_RC_OK;
}

#define fujiptpip_getdata_payload		8

static unsigned char hardcoded_deviceinfo[] = {
100, 0,			/* standard version */
PTP_VENDOR_FUJI, 0,	/* vendor extension id */
0, 0, 100, 0,		/* vendor ext version */

0x16,
0x66, 0x00,
0x75, 0x00,
0x6a, 0x00,
0x69, 0x00,
0x66, 0x00,
0x69, 0x00,
0x6c, 0x00,
0x6d, 0x00,
0x2e, 0x00,
0x63, 0x00,
0x6f, 0x00,
0x2e, 0x00,
0x6a, 0x00,
0x70, 0x00,
0x3a, 0x00,
0x20, 0x00,
0x31, 0x00,
0x2e, 0x00,
0x30, 0x00,
0x3b, 0x00,
0x20, 0x00,
0x00, 0x00,

0x00, 0x00,	/* functional mode */

0x1b, 0x00, 0x00, 0x00,	/* opcodes */
0x01, 0x10,
0x02, 0x10,
0x03, 0x10,
0x04, 0x10,
0x05, 0x10,
0x06, 0x10,
0x07, 0x10,
0x08, 0x10,
0x09, 0x10,
0x0a, 0x10,
0x0b, 0x10,
0x0e, 0x10,
0x0f, 0x10,
0x14, 0x10,
0x15, 0x10,
0x16, 0x10,
0x17, 0x10,
0x18, 0x10,
0x1b, 0x10,
0x1c, 0x10,
0x0c, 0x90,
0x0d, 0x90,
0x1d, 0x90,
0x01, 0x98,
0x02, 0x98,
0x03, 0x98,
0x05, 0x98,

0x06, 0x00, 0x00, 0x00, /* events */
0x02, 0x40,
0x03, 0x40,
0x04, 0x40,
0x05, 0x40,
0x0d, 0x40,
0x06, 0xc0,

0xa8, 0x00, 0x00, 0x00, /* device prop codes */
0x01, 0x50, 0x03, 0x50, 0x05, 0x50, 0x0a, 0x50, 0x0b, 0x50, 0x0c, 0x50, 0x0e, 0x50, 0x0f,
0x50, 0x11, 0x50, 0x12, 0x50, 0x15, 0x50, 0x18, 0x50, 0x19, 0x50, 0x1c, 0x50, 0x01, 0xd0, 0x02,
0xd0, 0x03, 0xd0, 0x04, 0xd0, 0x05, 0xd0, 0x07, 0xd0, 0x08, 0xd0, 0x09, 0xd0, 0x0a, 0xd0, 0x0b,
0xd0, 0x0c, 0xd0, 0x0d, 0xd0, 0x0e, 0xd0, 0x0f, 0xd0, 0x10, 0xd0, 0x11, 0xd0, 0x12, 0xd0, 0x13,
0xd0, 0x14, 0xd0, 0x15, 0xd0, 0x16, 0xd0, 0x17, 0xd0, 0x18, 0xd0, 0x19, 0xd0, 0x1a, 0xd0, 0x1b,
0xd0, 0x1c, 0xd0, 0x00, 0xd1, 0x01, 0xd1, 0x02, 0xd1, 0x03, 0xd1, 0x04, 0xd1, 0x05, 0xd1, 0x06,
0xd1, 0x07, 0xd1, 0x08, 0xd1, 0x09, 0xd1, 0x0a, 0xd1, 0x0b, 0xd1, 0x0c, 0xd1, 0x0d, 0xd1, 0x0e,
0xd1, 0x0f, 0xd1, 0x10, 0xd1, 0x11, 0xd1, 0x12, 0xd1, 0x13, 0xd1, 0x14, 0xd1, 0x15, 0xd1, 0x16,
0xd1, 0x17, 0xd1, 0x18, 0xd1, 0x19, 0xd1, 0x1a, 0xd1, 0x1b, 0xd1, 0x1c, 0xd1, 0x1d, 0xd1, 0x1e,
0xd1, 0x1f, 0xd1, 0x20, 0xd1, 0x21, 0xd1, 0x22, 0xd1, 0x23, 0xd1, 0x24, 0xd1, 0x25, 0xd1, 0x26,
0xd1, 0x27, 0xd1, 0x28, 0xd1, 0x29, 0xd1, 0x2a, 0xd1, 0x2b, 0xd1, 0x2c, 0xd1, 0x2d, 0xd1, 0x2e,
0xd1, 0x2f, 0xd1, 0x30, 0xd1, 0x31, 0xd1, 0x32, 0xd1, 0x33, 0xd1, 0x34, 0xd1, 0x35, 0xd1, 0x36,
0xd1, 0x37, 0xd1, 0x38, 0xd1, 0x39, 0xd1, 0x3a, 0xd1, 0x3b, 0xd1, 0x3c, 0xd1, 0x3d, 0xd1, 0x3e,
0xd1, 0x3f, 0xd1, 0x40, 0xd1, 0x41, 0xd1, 0x42, 0xd1, 0x43, 0xd1, 0x44, 0xd1, 0x45, 0xd1, 0x46,
0xd1, 0x47, 0xd1, 0x48, 0xd1, 0x49, 0xd1, 0x4a, 0xd1, 0x4b, 0xd1, 0x4c, 0xd1, 0x4d, 0xd1, 0x4e,
0xd1, 0x4f, 0xd1, 0x50, 0xd1, 0x51, 0xd1, 0x52, 0xd1, 0x53, 0xd1, 0x54, 0xd1, 0x55, 0xd1, 0x57,
0xd1, 0x58, 0xd1, 0x59, 0xd1, 0x5a, 0xd1, 0x5b, 0xd1, 0x5c, 0xd1, 0x5d, 0xd1, 0x5e, 0xd1, 0x5f,
0xd1, 0x60, 0xd1, 0x61, 0xd1, 0x00, 0xd2, 0x01, 0xd2, 0x02, 0xd2, 0x03, 0xd2, 0x04, 0xd2, 0x05,
0xd2, 0x06, 0xd2, 0x07, 0xd2, 0x08, 0xd2, 0x09, 0xd2, 0x0a, 0xd2, 0x0b, 0xd2, 0x0c, 0xd2, 0x0d,
0xd2, 0x0e, 0xd2, 0x0f, 0xd2, 0x10, 0xd2, 0x11, 0xd2, 0x12, 0xd2, 0x13, 0xd2, 0x14, 0xd2, 0x15,
0xd2, 0x16, 0xd2, 0x17, 0xd2, 0x18, 0xd2, 0x19, 0xd2, 0x1a, 0xd2, 0x1b, 0xd2, 0x06, 0xd4, 0x07,
0xd4,

0x03, 0x00, 0x00, 0x00,	/* capture formats */
0x00, 0x38,
0x01, 0x38,
0x03, 0xb1,

0x04, 0x00, 0x00, 0x00,	/* image formats */
0x00, 0x38,
0x01, 0x38,
0x03, 0xb1,
0x0d, 0x38,

0x09,		/* vendor "FUJIFILM" */
0x46, 0x00,
0x55, 0x00,
0x4a, 0x00,
0x49, 0x00,
0x46, 0x00,
0x49, 0x00,
0x4c, 0x00,
0x4d, 0x00,
0x00, 0x00,

0x06,		/* device "X-T42" */
0x58, 0x00,
0x2d, 0x00,
0x54, 0x00,
0x34, 0x00,
0x32, 0x00,
0x00, 0x00,

0x05,		/* device version ... "1.01" */
0x31, 0x00,
0x2e, 0x00,
0x30, 0x00,
0x31, 0x00,
0x00, 0x00,

0x1f,		/* serial number */
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x30, 0x00,
0x00, 0x00,
};

uint16_t
ptp_fujiptpip_getdata (PTPParams* params, PTPContainer* ptp, PTPDataHandler *handler) {
	PTPIPHeader		hdr;
	unsigned char		*xdata = NULL;
	uint16_t 		ret;
	int			xret;

	GP_LOG_D ("Reading PTP_OC 0x%0x (%s) data...", ptp->Code, ptp_get_opcode_name(params, ptp->Code));
	ret = ptp_fujiptpip_cmd_read (params, &hdr, &xdata);
	if (ret != PTP_RC_OK)
		return ret;

	/* the Fuji X-T4 does only return an empty deviceinfo ... so we synthesize one */
	if ((ptp->Code == PTP_OC_GetDeviceInfo) && (dtoh32(hdr.length)-fujiptpip_getdata_payload-4 == 0)) {
		GP_LOG_D("synthesizing Fuji DeviceInfo");
		xret = handler->putfunc (params, handler->priv, sizeof(hardcoded_deviceinfo),hardcoded_deviceinfo);
	} else {
		GP_LOG_DATA ((char*)(xdata+fujiptpip_getdata_payload), dtoh32(hdr.length)-fujiptpip_getdata_payload-4, "fujiptpip/getdatda data:");
		xret = handler->putfunc (params, handler->priv,
			dtoh32(hdr.length)-fujiptpip_getdata_payload-4, xdata+fujiptpip_getdata_payload
		);
	}
	if (xret != PTP_RC_OK) {
		GP_LOG_E ("failed to putfunc of returned data");
		return GP_ERROR;
	}
	free (xdata);
	return PTP_RC_OK;
}

#define fujiptpip_dataphase	0
#define fujiptpip_resp_code	2
#define fujiptpip_resp_transid	4
#define fujiptpip_resp_param1	8
#define fujiptpip_resp_param2	12
#define fujiptpip_resp_param3	16
#define fujiptpip_resp_param4	20
#define fujiptpip_resp_param5	24

uint16_t
ptp_fujiptpip_getresp (PTPParams* params, PTPContainer* resp)
{
	PTPIPHeader	hdr;
	unsigned char	*data = NULL;
	uint16_t 	ret;
	int		n;

	GP_LOG_D ("Reading PTP_OC 0x%0x (%s) response...", resp->Code, ptp_get_opcode_name(params, resp->Code));
	ret = ptp_fujiptpip_cmd_read (params, &hdr, &data);
	if (ret != PTP_RC_OK)
		return GP_ERROR;

	switch (dtoh16a(data)) {
	case 3:
		GP_LOG_D("PTPIP_CMD_RESPONSE");
		resp->Code		= dtoh16a(&data[fujiptpip_resp_code]);
		resp->Transaction_ID	= dtoh32a(&data[fujiptpip_resp_transid]);
		n = (dtoh32(hdr.length) - sizeof(uint32_t) - fujiptpip_resp_param1)/sizeof(uint32_t);
		switch (n) {
		case 5: resp->Param5 = dtoh32a(&data[fujiptpip_resp_param5]);/* fallthrough */
		case 4: resp->Param4 = dtoh32a(&data[fujiptpip_resp_param4]);/* fallthrough */
		case 3: resp->Param3 = dtoh32a(&data[fujiptpip_resp_param3]);/* fallthrough */
		case 2: resp->Param2 = dtoh32a(&data[fujiptpip_resp_param2]);/* fallthrough */
		case 1: resp->Param1 = dtoh32a(&data[fujiptpip_resp_param1]);/* fallthrough */
		case 0: break;
		default:
			GP_LOG_E ("response got %d parameters?", n);
			break;
		}
		break;
	default:
		GP_LOG_E ("response type %d packet?", dtoh16a(data));
		break;
	}
	free (data);
	return PTP_RC_OK;
}

#define fujiptpip_initcmd_protocolversion	8
#define fujiptpip_initcmd_guid			12
#define fujiptpip_initcmd_name			28

static uint16_t
ptp_fujiptpip_init_command_request (PTPParams* params)
{
	char		hostname[100];
	unsigned char*	cmdrequest;
	unsigned int	i;
	int 		len, ret;
	unsigned char	guid[16];

	ptp_nikon_getptpipguid(guid);
#if !defined (WIN32)
	if (gethostname (hostname, sizeof(hostname)))
		return PTP_RC_GeneralError;
#else
	strcpy (hostname, "gpwindows");
#endif
	len = fujiptpip_initcmd_name + (strlen(hostname)+1)*2;

	cmdrequest = malloc(len);
	htod32a(&cmdrequest[fujiptpip_type],PTPIP_INIT_COMMAND_REQUEST);
	htod32a(&cmdrequest[fujiptpip_len],len);

	htod32a(&cmdrequest[fujiptpip_initcmd_protocolversion], 0x8f53e4f2);	/* magic number */

	memcpy(&cmdrequest[fujiptpip_initcmd_guid], guid, 16);
	for (i=0;i<strlen(hostname)+1;i++) {
		/* -> ucs-2 in little endian */
		cmdrequest[fujiptpip_initcmd_name+i*2] = hostname[i];
		cmdrequest[fujiptpip_initcmd_name+i*2+1] = 0;
	}

	/*
	htod16a(&cmdrequest[fujiptpip_initcmd_name+(strlen(hostname)+1)*2],PTPIP_VERSION_MINOR);
	htod16a(&cmdrequest[fujiptpip_initcmd_name+(strlen(hostname)+1)*2+2],PTPIP_VERSION_MAJOR);
	*/


	GP_LOG_DATA ((char*)cmdrequest, len, "ptpip/init_cmd data:");
	ret = write (params->cmdfd, cmdrequest, len);
	free (cmdrequest);
	if (ret == -1) {
		perror("write init cmd request");
		return PTP_RC_GeneralError;
	}
	GP_LOG_E ("return %d / len %d", ret, len);
	if (ret != len) {
		GP_LOG_E ("return %d vs len %d", ret, len);
		return PTP_RC_GeneralError;
	}
	return PTP_RC_OK;
}

#define ptpip_cmdack_idx	0
#define ptpip_cmdack_guid	4
#define ptpip_cmdack_name	20

static uint16_t
ptp_fujiptpip_init_command_ack (PTPParams* params)
{
	PTPIPHeader	hdr;
	unsigned char	*data = NULL;
	uint16_t 	ret;
	int		i;
	unsigned short	*name;

	ret = ptp_fujiptpip_generic_read (params, params->cmdfd, &hdr, &data, 1);
	if (ret != PTP_RC_OK)
		return ret;
	if (hdr.type != dtoh32(PTPIP_INIT_COMMAND_ACK)) {
		GP_LOG_E ("bad type returned %d", htod32(hdr.type));
		free (data);
		if (hdr.type == PTPIP_INIT_FAIL) /* likely reason is permission denied */
			return PTP_RC_AccessDenied;
		return PTP_RC_GeneralError;
	}
	params->eventpipeid = dtoh32a(&data[ptpip_cmdack_idx]);
	memcpy (params->cameraguid, &data[ptpip_cmdack_guid], 16);
	name = (unsigned short*)&data[ptpip_cmdack_name];
	for (i=0;name[i];i++) /* EMPTY */;
	params->cameraname = malloc((i+1)*sizeof(uint16_t));
	for (i=0;name[i];i++)
		params->cameraname[i] = name[i];
	free (data);
	return PTP_RC_OK;
}

#define fujiptpip_eventinit_idx	8
#define fujiptpip_eventinit_size	12

int
ptp_fujiptpip_init_event (PTPParams* params, const char *address)
{
	char 		*addr, *s, *p;
	int		port, eventport, tries;
	struct sockaddr_in	saddr;

	GP_LOG_D ("connecting to %s.", address);
	if (NULL == strchr (address,':'))
		return GP_ERROR_BAD_PARAMETERS;

#ifdef HAVE_INET_ATON
	addr = strdup (address);
	if (!addr)
		return GP_ERROR_NO_MEMORY;
	s = strchr (addr,':');
	if (!s) {
		GP_LOG_E ("addr %s should contain a :", address);
		free (addr);
		return GP_ERROR_BAD_PARAMETERS;
	}
	*s = '\0';
	p = strchr (s+1,':');
	port = 55740;
	eventport = port+1;
	if (p) {
		*p = '\0';
		if (!sscanf (p+1,"%d",&port)) {
			fprintf(stderr,"failed to scan for port in %s\n", p+1);
			free (addr);
			return GP_ERROR_BAD_PARAMETERS;
		}
		/* different event port ? */
		p = strchr (p+1,':');
		if (p) {
			if (!sscanf (p+1,"%d",&eventport)) {
				fprintf(stderr,"failed to scan for eventport in %s\n", p+1);
				free (addr);
				return GP_ERROR_BAD_PARAMETERS;
			}
		}
	}
	if (!inet_aton (s+1,  &saddr.sin_addr)) {
		fprintf(stderr,"failed to scan for addr in %s\n", s+1);
		free (addr);
		return GP_ERROR_BAD_PARAMETERS;
	}
	free (addr);

	tries = 2;
	saddr.sin_family	= AF_INET;
	saddr.sin_port		= htons(eventport);
	do {
		if (-1 != connect (params->evtfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in)))
			break;
		if ((errno == ECONNREFUSED) && (tries--)) {
			GP_LOG_D ("event connect failed, retrying after short wait");
			usleep(100*1000);
			continue;
		}
		GP_LOG_E ("could not connect event");
		close (params->evtfd);
		return GP_ERROR_IO;
	} while (1);
	GP_LOG_D ("fujiptpip event connected!");
	return GP_OK;
#else
	GP_LOG_E ("Windows currently not supported, neeeds a winsock port.");
	return GP_ERROR_NOT_SUPPORTED;
#endif
}


/* Event handling functions */

/* PTP Events wait for or check mode */

#define ptpip_event_type    	0
#define ptpip_event_code    	2
#define ptpip_event_transid	4
#define ptpip_event_param1	8
#define ptpip_event_param2	12
#define ptpip_event_param3	16

static uint16_t
ptp_fujiptpip_event (PTPParams* params, PTPContainer* event, int wait)
{
#ifndef WIN32
	fd_set		infds;
	struct timeval	timeout;
	int ret;
	unsigned char*	data = NULL;
	PTPIPHeader	hdr;
	int n;

	while (1) {
		FD_ZERO(&infds);
		FD_SET(params->evtfd, &infds);
		timeout.tv_sec = 0;
		if (wait == PTP_EVENT_CHECK_FAST)
			timeout.tv_usec = 1;
		else
			timeout.tv_usec = 1000; /* 1/1000 second  .. perhaps wait longer? */

		ret = select (params->evtfd+1, &infds, NULL, NULL, &timeout);
		if (1 != ret) {
			if (-1 == ret) {
				GP_LOG_D ("select returned error, errno is %d", errno);
				return PTP_ERROR_IO;
			}
			return PTP_ERROR_TIMEOUT;
		}

		ret = ptp_fujiptpip_evt_read (params, &hdr, &data);
		if (ret != PTP_RC_OK)
			return ret;
		GP_LOG_D ("length %d", hdr.length);
		break;
	}

	event->Code		= dtoh16a(&data[ptpip_event_code]);
	event->Transaction_ID	= dtoh32a(&data[ptpip_event_transid]);
	n = (dtoh32(hdr.length) - sizeof(uint32_t) - ptpip_event_param1)/sizeof(uint32_t);
	switch (n) {
	case 3: event->Param3 = dtoh32a(&data[ptpip_event_param3]);/* fallthrough */
	case 2: event->Param2 = dtoh32a(&data[ptpip_event_param2]);/* fallthrough */
	case 1: event->Param1 = dtoh32a(&data[ptpip_event_param1]);/* fallthrough */
	case 0: break;
	default:
		GP_LOG_E ("response got %d parameters?", n);
		break;
	}
	free (data);
	return PTP_RC_OK;
#else
	GP_LOG_E ("not supported currently on Windows");
	return PTP_RC_OK;
#endif
}

uint16_t
ptp_fujiptpip_event_check_queue (PTPParams* params, PTPContainer* event) {
	/* the fast check just takes 1ms, so lets keep it */
	return ptp_fujiptpip_event (params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_fujiptpip_event_check (PTPParams* params, PTPContainer* event) {
	return ptp_fujiptpip_event (params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_fujiptpip_event_wait (PTPParams* params, PTPContainer* event) {
	return ptp_fujiptpip_event (params, event, PTP_EVENT_CHECK);
}

int
ptp_fujiptpip_connect (PTPParams* params, const char *address) {
	char 		*addr, *s, *p;
	int		port, eventport;
	struct sockaddr_in	saddr;
	uint16_t	ret;

	GP_LOG_D ("connecting to %s.", address);
	if (NULL == strchr (address,':'))
		return GP_ERROR_BAD_PARAMETERS;

#ifdef HAVE_INET_ATON
	addr = strdup (address);
	if (!addr)
		return GP_ERROR_NO_MEMORY;
	s = strchr (addr,':');
	if (!s) {
		GP_LOG_E ("addr %s should contain a :", address);
		free (addr);
		return GP_ERROR_BAD_PARAMETERS;
	}
	*s = '\0';
	p = strchr (s+1,':');
	port = 55740;
	eventport = port+1;
	if (p) {
		*p = '\0';
		if (!sscanf (p+1,"%d",&port)) {
			fprintf(stderr,"failed to scan for port in %s\n", p+1);
			free (addr);
			return GP_ERROR_BAD_PARAMETERS;
		}
		/* different event port ? */
		p = strchr (p+1,':');
		if (p) {
			if (!sscanf (p+1,"%d",&eventport)) {
				fprintf(stderr,"failed to scan for eventport in %s\n", p+1);
				free (addr);
				return GP_ERROR_BAD_PARAMETERS;
			}
		}
	}
	if (!inet_aton (s+1,  &saddr.sin_addr)) {
		fprintf(stderr,"failed to scan for addr in %s\n", s+1);
		free (addr);
		return GP_ERROR_BAD_PARAMETERS;
	}
	saddr.sin_port		= htons(port);
	saddr.sin_family	= AF_INET;
	free (addr);
	params->cmdfd = socket (PF_INET, SOCK_STREAM, 0);
	if (params->cmdfd == -1) {
		perror ("socket cmd");
		return GP_ERROR_BAD_PARAMETERS;
	}
	params->evtfd = socket (PF_INET, SOCK_STREAM, 0);
	if (params->evtfd == -1) {
		perror ("socket evt");
		close (params->cmdfd);
		return GP_ERROR_BAD_PARAMETERS;
	}
	if (-1 == connect (params->cmdfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in))) {
		perror ("connect cmd");
		close (params->cmdfd);
		close (params->evtfd);
		return GP_ERROR_IO;
	}
	ret = ptp_fujiptpip_init_command_request (params);
	if (ret != PTP_RC_OK) {
		close (params->cmdfd);
		close (params->evtfd);
		return translate_ptp_result (ret);
	}
	ret = ptp_fujiptpip_init_command_ack (params);
	if (ret != PTP_RC_OK) {
		close (params->cmdfd);
		close (params->evtfd);
		return translate_ptp_result (ret);
	}
	GP_LOG_D ("fujiptpip connected!");
	return GP_OK;
#else
	GP_LOG_E ("Windows currently not supported, neeeds a winsock port.");
	return GP_ERROR_NOT_SUPPORTED;
#endif
}
