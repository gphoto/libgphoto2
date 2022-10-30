/* ptpip.c
 *
 * Copyright (C) 2006 Marcus Meissner <marcus@jet.franken.de>
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

#include "ptpip-private.h"

#ifdef WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <fcntl.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

#include "libgphoto2/i18n.h"

#include "ptp.h"
#include "ptp-private.h"

#define PTPIP_VERSION_MAJOR 0x0001
#define PTPIP_VERSION_MINOR 0x0000

#include "ptp.h"
#include "ptp-bugs.h"

#include "ptp-pack.c"

#define ptpip_len		0
#define ptpip_type		4

#define ptpip_cmd_dataphase	8
#define ptpip_cmd_code		12
#define ptpip_cmd_transid	14
#define ptpip_cmd_param1	18
#define ptpip_cmd_param2	22
#define ptpip_cmd_param3	26
#define ptpip_cmd_param4	30
#define ptpip_cmd_param5	34

#define PTP_EVENT_CHECK			0x0000	/* waits for */
#define PTP_EVENT_CHECK_FAST		0x0001	/* checks */
static uint16_t ptp_ptpip_check_event (PTPParams* params);
static uint16_t ptp_ptpip_event (PTPParams* params, PTPContainer* event, int wait);

/* send / receive functions */
uint16_t
ptp_ptpip_sendreq (PTPParams* params, PTPContainer* req, int dataphase)
{
	int		ret;
	int		len = 18+req->Nparam*4;
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

	ptp_ptpip_check_event (params);

	htod32a(&request[ptpip_type],PTPIP_CMD_REQUEST);
	htod32a(&request[ptpip_len],len);
	/* sending data = 2, receiving data or no data = 1 */
	if ((dataphase&PTP_DP_DATA_MASK) == PTP_DP_SENDDATA)
		htod32a(&request[ptpip_cmd_dataphase],2);
	else
		htod32a(&request[ptpip_cmd_dataphase],1);
	htod16a(&request[ptpip_cmd_code],req->Code);
	htod32a(&request[ptpip_cmd_transid],req->Transaction_ID);

	switch (req->Nparam) {
	case 5: htod32a(&request[ptpip_cmd_param5],req->Param5);/* fallthrough */
	case 4: htod32a(&request[ptpip_cmd_param4],req->Param4);/* fallthrough */
	case 3: htod32a(&request[ptpip_cmd_param3],req->Param3);/* fallthrough */
	case 2: htod32a(&request[ptpip_cmd_param2],req->Param2);/* fallthrough */
	case 1: htod32a(&request[ptpip_cmd_param1],req->Param1);/* fallthrough */
	case 0:
	default:
		break;
	}
	GP_LOG_DATA ( (char*)request, len, "ptpip/oprequest data:");
	ret = ptpip_write_with_timeout(params->cmdfd, request, len, PTPIP_DEFAULT_TIMEOUT_S, PTPIP_DEFAULT_TIMEOUT_MS);
	free (request);
	if (ret == PTPSOCK_ERR) {
		ptpip_perror ("sendreq/write to cmdfd");
		if (ptpip_get_socket_error() == ETIMEDOUT)
			return PTP_ERROR_TIMEOUT;
		return PTP_ERROR_IO;
	}
	if (ret != len) {
		GP_LOG_E ("ptp_ptpip_sendreq() len =%d but ret=%d", len, ret);
		return PTP_RC_OK;
	}
	return PTP_RC_OK;
}

static uint16_t
ptp_ptpip_generic_read (PTPParams *params, int fd, PTPIPHeader *hdr, unsigned char**data) {
	int	ret, len, curread;
	unsigned char *xhdr;

	xhdr = (unsigned char*)hdr; curread = 0; len = sizeof (PTPIPHeader);
	while (curread < len) {
		ret = ptpip_read_with_timeout (fd, xhdr + curread, len - curread, PTPIP_DEFAULT_TIMEOUT_S, PTPIP_DEFAULT_TIMEOUT_MS);
		if (ret == PTPSOCK_ERR) {
			ptpip_perror ("read PTPIPHeader");
			if (ptpip_get_socket_error() == ETIMEDOUT)
				return PTP_ERROR_TIMEOUT;
			return PTP_ERROR_IO;
		}
		GP_LOG_DATA ((char*)xhdr+curread, ret, "ptpip/generic_read header:");
		curread += ret;
		if (ret == 0) {
			GP_LOG_E ("End of stream after reading %d bytes of ptpipheader", ret);
			return PTP_RC_GeneralError;
		}
	}
	len = dtoh32 (hdr->length) - sizeof (PTPIPHeader);
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
		ret = ptpip_read_with_timeout (fd, (*data)+curread, len-curread, PTPIP_DEFAULT_TIMEOUT_S, PTPIP_DEFAULT_TIMEOUT_MS);
		if (ret == PTPSOCK_ERR) {
			GP_LOG_E ("error %d in reading PTPIP data", ptpip_get_socket_error());
			free (*data);*data = NULL;
			if (ptpip_get_socket_error() == ETIMEDOUT)
				return PTP_ERROR_TIMEOUT;
			return PTP_ERROR_IO;
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
ptp_ptpip_cmd_read (PTPParams* params, PTPIPHeader *hdr, unsigned char** data) {
	ptp_ptpip_check_event (params);
	return ptp_ptpip_generic_read (params, params->cmdfd, hdr, data);
}

static uint16_t
ptp_ptpip_evt_read (PTPParams* params, PTPIPHeader *hdr, unsigned char** data) {
	return ptp_ptpip_generic_read (params, params->evtfd, hdr, data);
}

static uint16_t
ptp_ptpip_check_event (PTPParams* params) {
	PTPContainer	event;
	uint16_t	ret;

	event.Code = 0;
	ret = ptp_ptpip_event (params, &event, PTP_EVENT_CHECK_FAST);
	if (ret != PTP_RC_OK)
		return ret;
	if (event.Code == 0)
		return ret;
	return ptp_add_event (params, &event);
}

#define ptpip_startdata_transid		0
#define ptpip_startdata_totallen	4
#define ptpip_startdata_unknown		8
#define ptpip_data_transid		0
#define ptpip_data_payload		4

#define WRITE_BLOCKSIZE 65536
uint16_t
ptp_ptpip_senddata (PTPParams* params, PTPContainer* ptp,
		uint64_t size, PTPDataHandler *handler
) {
	unsigned char	request[0x14];
	unsigned int	curwrite, towrite;
	int		ret;
	unsigned char*	xdata;

	GP_LOG_D ("Sending PTP_OC 0x%0x (%s) data...", ptp->Code, ptp_get_opcode_name(params, ptp->Code));
	htod32a(&request[ptpip_type],PTPIP_START_DATA_PACKET);
	htod32a(&request[ptpip_len],sizeof(request));
	htod32a(&request[ptpip_startdata_transid  + 8],ptp->Transaction_ID);
	htod32a(&request[ptpip_startdata_totallen + 8],size);
	htod32a(&request[ptpip_startdata_unknown  + 8],0);
	GP_LOG_DATA ((char*)request, sizeof(request), "ptpip/senddata header:");
	ret = ptpip_write_with_timeout (params->cmdfd, request, sizeof(request), PTPIP_DEFAULT_TIMEOUT_S, PTPIP_DEFAULT_TIMEOUT_MS);
	if (ret == PTPSOCK_ERR) {
		ptpip_perror ("sendreq/write to cmdfd");
		if (ptpip_get_socket_error() == ETIMEDOUT)
			return PTP_ERROR_TIMEOUT;
		return PTP_ERROR_IO;
	}
	if (ret != sizeof(request)) {
		GP_LOG_E ("ptp_ptpip_senddata() len=%d but ret=%d", (int)sizeof(request), ret);
		return PTP_RC_GeneralError;
	}
	xdata = malloc(WRITE_BLOCKSIZE+8+4);
	if (!xdata) return PTP_RC_GeneralError;
	curwrite = 0;
	while (curwrite < size) {
		unsigned long type, written, towrite2, xtowrite;

		ptp_ptpip_check_event (params);

		towrite = size - curwrite;
		if (towrite > WRITE_BLOCKSIZE) {
			towrite	= WRITE_BLOCKSIZE;
			type	= PTPIP_DATA_PACKET;
		} else {
			type	= PTPIP_END_DATA_PACKET;
		}
		ret = handler->getfunc (params, handler->priv, towrite, &xdata[ptpip_data_payload+8], &xtowrite);
		if (ret == -1) {
			ptpip_perror ("getfunc in senddata failed");
			free (xdata);
			return PTP_RC_GeneralError;
		}
		towrite2 = xtowrite + 12;
		htod32a(&xdata[ptpip_type], type);
		htod32a(&xdata[ptpip_len], towrite2);
		htod32a(&xdata[ptpip_data_transid+8], ptp->Transaction_ID);
		GP_LOG_DATA ((char*)xdata, towrite2, "ptpip/senddata data:");
		written = 0;
		while (written < towrite2) {
			ret = ptpip_write_with_timeout (params->cmdfd, xdata+written, towrite2-written, PTPIP_DEFAULT_TIMEOUT_S, PTPIP_DEFAULT_TIMEOUT_MS);
			if (ret == PTPSOCK_ERR) {
				ptpip_perror ("write in senddata failed");
				free (xdata);
				if (ptpip_get_socket_error() == ETIMEDOUT)
					return PTP_ERROR_TIMEOUT;
				return PTP_ERROR_IO;
			}
			written += ret;
		}
		curwrite += towrite;
	}
	free (xdata);
	return PTP_RC_OK;
}

uint16_t
ptp_ptpip_getdata (PTPParams* params, PTPContainer* ptp, PTPDataHandler *handler) {
	PTPIPHeader		hdr;
	unsigned char		*xdata = NULL;
	uint16_t 		ret;
	unsigned long		toread, curread;
	int			xret;

	GP_LOG_D ("Reading PTP_OC 0x%0x (%s) data...", ptp->Code, ptp_get_opcode_name(params, ptp->Code));
	ret = ptp_ptpip_cmd_read (params, &hdr, &xdata);
	if (ret != PTP_RC_OK)
		return ret;

	if (dtoh32(hdr.type) == PTPIP_CMD_RESPONSE) { /* might happen if we have no data transfer due to error? */
		GP_LOG_E ("Unexpected ptp response, ptp code %x", dtoh16a(&xdata[0]));
		return dtoh16a(&xdata[0]);
	}
	if (dtoh32(hdr.type) != PTPIP_START_DATA_PACKET) {
		GP_LOG_E ("got reply type %d\n", dtoh32(hdr.type));
		return PTP_RC_GeneralError;
	}
	toread = dtoh32a(&xdata[ptpip_data_payload]);
	free (xdata); xdata = NULL;
	curread = 0;
	while (curread < toread) {
		ret = ptp_ptpip_cmd_read (params, &hdr, &xdata);
		if (ret != PTP_RC_OK)
			return ret;
		if (dtoh32(hdr.type) == PTPIP_END_DATA_PACKET) {
			unsigned long datalen = dtoh32(hdr.length)-8-ptpip_data_payload;
			if (datalen > (toread-curread)) {
				GP_LOG_E ("returned data is too much, expected %ld, got %ld",
					  (toread-curread), datalen
				);
				break;
			}
			xret = handler->putfunc (params, handler->priv,
				datalen, xdata+ptpip_data_payload
			);
			if (xret != PTP_RC_OK) {
				GP_LOG_E ("failed to putfunc of returned data");
				break;
			}
			curread += datalen;
			free (xdata); xdata = NULL;
			continue;
		}
		if (dtoh32(hdr.type) == PTPIP_DATA_PACKET) {
			unsigned long datalen = dtoh32(hdr.length)-8-ptpip_data_payload;
			if (datalen > (toread-curread)) {
				GP_LOG_E ("returned data is too much, expected %ld, got %ld",
					  (toread-curread), datalen
				);
				break;
			}
			xret = handler->putfunc (params, handler->priv,
				datalen, xdata+ptpip_data_payload
			);
			if (xret != PTP_RC_OK) {
				GP_LOG_E ("failed to putfunc of returned data");
				break;
			}
			curread += datalen;
			free (xdata); xdata = NULL;
			continue;
		}
		GP_LOG_E ("ret type %d", hdr.type);
	}
	if (curread < toread)
		return PTP_RC_GeneralError;
	return PTP_RC_OK;
}

#define ptpip_resp_code		0
#define ptpip_resp_transid	2
#define ptpip_resp_param1	6
#define ptpip_resp_param2	10
#define ptpip_resp_param3	14
#define ptpip_resp_param4	18
#define ptpip_resp_param5	22

uint16_t
ptp_ptpip_getresp (PTPParams* params, PTPContainer* resp)
{
	PTPIPHeader	hdr;
	unsigned char	*data = NULL;
	uint16_t 	ret;
	int		n;

	GP_LOG_D ("Reading PTP_OC 0x%0x (%s) response...", resp->Code, ptp_get_opcode_name(params, resp->Code));
retry:
	ret = ptp_ptpip_cmd_read (params, &hdr, &data);
	if (ret != PTP_RC_OK)
		return ret;

	switch (dtoh32(hdr.type)) {
	case PTPIP_END_DATA_PACKET:
		resp->Transaction_ID	= dtoh32a(&data[0]);
		GP_LOG_D("PTPIP_END_DATA_PACKET (tid = 0x%08x)", resp->Transaction_ID);
		free (data);
		data = NULL;
		goto retry;
	case PTPIP_CMD_RESPONSE:
		resp->Code		= dtoh16a(&data[ptpip_resp_code]);
		resp->Transaction_ID	= dtoh32a(&data[ptpip_resp_transid]);
		GP_LOG_D("PTPIP_CMD_RESPONSE (result=0x%04x, tid=0x%08x)", resp->Code, resp->Transaction_ID);
		n = (dtoh32(hdr.length) - sizeof(hdr) - ptpip_resp_param1)/sizeof(uint32_t);
		switch (n) {
		case 5: resp->Param5 = dtoh32a(&data[ptpip_resp_param5]);/* fallthrough */
		case 4: resp->Param4 = dtoh32a(&data[ptpip_resp_param4]);/* fallthrough */
		case 3: resp->Param3 = dtoh32a(&data[ptpip_resp_param3]);/* fallthrough */
		case 2: resp->Param2 = dtoh32a(&data[ptpip_resp_param2]);/* fallthrough */
		case 1: resp->Param1 = dtoh32a(&data[ptpip_resp_param1]);/* fallthrough */
		case 0: break;
		default:
			GP_LOG_E ("response got %d parameters?", n);
			break;
		}
		break;
	default:
		GP_LOG_E ("response type %d packet?", dtoh32(hdr.type));
		break;
	}
	free (data);
	return PTP_RC_OK;
}

#define ptpip_initcmd_guid	8
#define ptpip_initcmd_name	24

static uint16_t
ptp_ptpip_init_command_request (PTPParams* params)
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
	DWORD hostname_size = (DWORD)sizeof(hostname);
	if (!GetComputerNameA(hostname, &hostname_size))
		return PTP_RC_GeneralError;
#endif
	len = ptpip_initcmd_name + (strlen(hostname)+1)*2 + 4;

	cmdrequest = malloc(len);
	htod32a(&cmdrequest[ptpip_type],PTPIP_INIT_COMMAND_REQUEST);
	htod32a(&cmdrequest[ptpip_len],len);

	memcpy(&cmdrequest[ptpip_initcmd_guid], guid, 16);
	for (i=0;i<strlen(hostname)+1;i++) {
		/* -> ucs-2 in little endian */
		cmdrequest[ptpip_initcmd_name+i*2] = hostname[i];
		cmdrequest[ptpip_initcmd_name+i*2+1] = 0;
	}
	htod16a(&cmdrequest[ptpip_initcmd_name+(strlen(hostname)+1)*2],PTPIP_VERSION_MINOR);
	htod16a(&cmdrequest[ptpip_initcmd_name+(strlen(hostname)+1)*2+2],PTPIP_VERSION_MAJOR);

	GP_LOG_DATA ((char*)cmdrequest, len, "ptpip/init_cmd data:");
	ret = ptpip_write_with_timeout (params->cmdfd, cmdrequest, len, PTPIP_DEFAULT_TIMEOUT_S, PTPIP_DEFAULT_TIMEOUT_MS);
	free (cmdrequest);
	if (ret == PTPSOCK_ERR) {
		ptpip_perror("write init cmd request");
		if (ptpip_get_socket_error() == ETIMEDOUT)
			return PTP_ERROR_TIMEOUT;
		return PTP_ERROR_IO;
	}
	GP_LOG_D ("return %d / len %d", ret, len);
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
ptp_ptpip_init_command_ack (PTPParams* params)
{
	PTPIPHeader	hdr;
	unsigned char	*data = NULL;
	uint16_t 	ret;
	int		i;
	unsigned short	*name;

	ret = ptp_ptpip_generic_read (params, params->cmdfd, &hdr, &data);
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
	params->cameraname = calloc((i+1),sizeof(uint16_t));
	for (i=0;name[i];i++)
		params->cameraname[i] = name[i];
	free (data);
	return PTP_RC_OK;
}

#define ptpip_eventinit_idx	8
#define ptpip_eventinit_size	12
static uint16_t
ptp_ptpip_init_event_request (PTPParams* params)
{
	unsigned char	evtrequest[ptpip_eventinit_size];
	int 		ret;

	htod32a(&evtrequest[ptpip_type],PTPIP_INIT_EVENT_REQUEST);
	htod32a(&evtrequest[ptpip_len],ptpip_eventinit_size);
	htod32a(&evtrequest[ptpip_eventinit_idx],params->eventpipeid);

	GP_LOG_DATA ((char*)evtrequest, ptpip_eventinit_size, "ptpip/init_event data:");
	ret = ptpip_write_with_timeout (params->evtfd, evtrequest, ptpip_eventinit_size, PTPIP_DEFAULT_TIMEOUT_S, PTPIP_DEFAULT_TIMEOUT_MS);
	if (ret == PTPSOCK_ERR) {
		ptpip_perror("write init evt request");
		if (ptpip_get_socket_error() == ETIMEDOUT)
			return PTP_ERROR_TIMEOUT;
		return PTP_ERROR_IO;
	}
	if (ret != ptpip_eventinit_size) {
		GP_LOG_E ("unexpected retsize %d, expected %d", ret, ptpip_eventinit_size);
		return PTP_RC_GeneralError;
	}
	return PTP_RC_OK;
}

static uint16_t
ptp_ptpip_init_event_ack (PTPParams* params)
{
	PTPIPHeader	hdr;
	unsigned char	*data = NULL;
	uint16_t	ret;

	ret = ptp_ptpip_evt_read (params, &hdr, &data);
	if (ret != PTP_RC_OK)
		return ret;
	free (data);
	if (hdr.type != dtoh32(PTPIP_INIT_EVENT_ACK)) {
		GP_LOG_E ("bad type returned %d\n", htod32(hdr.type));
		return PTP_RC_GeneralError;
	}
	return PTP_RC_OK;
}


/* Event handling functions */

/* PTP Events wait for or check mode */

#define ptpip_event_code    0
#define ptpip_event_transid	2
#define ptpip_event_param1	6
#define ptpip_event_param2	10
#define ptpip_event_param3	14

static uint16_t
ptp_ptpip_event (PTPParams* params, PTPContainer* event, int wait)
{
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
				GP_LOG_D ("select returned error, errno is %d", ptpip_get_socket_error());
				if (ptpip_get_socket_error() == ETIMEDOUT)
					return PTP_ERROR_TIMEOUT;
				return PTP_ERROR_IO;
			}
			return PTP_ERROR_TIMEOUT;
		}

		ret = ptp_ptpip_evt_read (params, &hdr, &data);
		if (ret != PTP_RC_OK)
			return ret;
		GP_LOG_D ("hdr type %d, length %d", hdr.type, hdr.length);

		if (dtoh32(hdr.type) == PTPIP_EVENT) {
			break;
		}

		/* TODO: Handle cancel transaction and ping/pong
		 * If not PTPIP_EVENT, process it and wait for next PTPIP_EVENT
		 */
		GP_LOG_E ("unknown/unhandled event type %d", dtoh32(hdr.type));
	}

	event->Code		= dtoh16a(&data[ptpip_event_code]);
	event->Transaction_ID	= dtoh32a(&data[ptpip_event_transid]);
	n = (dtoh32(hdr.length) - sizeof(hdr) - ptpip_event_param1)/sizeof(uint32_t);
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
}

uint16_t
ptp_ptpip_event_check_queue (PTPParams* params, PTPContainer* event) {
	/* the fast check just takes 1ms, so lets keep it */
	return ptp_ptpip_event (params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_ptpip_event_check (PTPParams* params, PTPContainer* event) {
	return ptp_ptpip_event (params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_ptpip_event_wait (PTPParams* params, PTPContainer* event) {
	return ptp_ptpip_event (params, event, PTP_EVENT_CHECK);
}

/**
 * ptp_nikon_getwifiguid:
 *
 * This command gets the GUID of this machine. If it does not exists, it creates
 * one.
 *
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
void
ptp_nikon_getptpipguid (unsigned char* guid) {
	char buffer[1024];
	int i;
	long val;
	int valid;
	char* endptr;
	char* pos;

	gp_setting_get("ptp2_ip","guid",buffer);

	if (strlen(buffer) == 47) { /* 47 = 16*2 (numbers) + 15 (semi-colons) */
		pos = buffer;
		valid = 1;
		for (i = 0; i < 16; i++) {
			val = strtol(pos, &endptr, 16);
			if (((*endptr != ':') && (*endptr != 0)) || (endptr != pos+2)) {
				valid = 0;
				break;
			}
			guid[i] = (unsigned char)val;
			pos += 3;
		}
		/*printf("GUID ");
		for (i = 0; i < 16; i++) {
			printf("%02x:", guid[i]);
		}
		printf("\n");*/
		if (valid)
			return;
	}

	/*fprintf(stderr, "Invalid GUID\n");*/

	/* Generate an ID */
	srand(time(NULL));
	buffer[0] = 0;
	pos = buffer;
	for (i = 0; i < 16; i++) {
		guid[i] = (unsigned char) ((256.0 * rand()) / RAND_MAX);
		pos += sprintf(pos, "%02x:", guid[i]);
	}
	buffer[47] = 0;

	/*printf("New GUID: %s\n", buffer);*/

	gp_setting_set("ptp2_ip","guid",buffer);
}


int
ptp_ptpip_connect (PTPParams* params, const char *address) {
	char 		*addr, *s, *p;
	int		port, eventport, tries;
	struct sockaddr_in	saddr;
	uint16_t	ret;

	GP_LOG_D ("connecting to %s.", address);
	if (NULL == strchr (address,':'))
		return GP_ERROR_BAD_PARAMETERS;

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
	eventport = port = 15740;
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
#ifdef HAVE_INET_ATON
	if (!inet_aton (s+1,  &saddr.sin_addr)) {
#else
	if (inet_pton(AF_INET, s+1, &saddr.sin_addr) != 1) {
#endif
		fprintf(stderr,"failed to scan for addr in %s\n", s+1);
		free (addr);
		return GP_ERROR_BAD_PARAMETERS;
	}
	saddr.sin_port		= htons(port);
	saddr.sin_family	= AF_INET;
	free (addr);
	PTPSOCK_SOCKTYPE cmdfd = params->cmdfd = socket (PF_INET, SOCK_STREAM, PTPSOCK_PROTO);
	if (cmdfd == PTPSOCK_INVALID) {
		ptpip_perror ("socket cmd");
		return GP_ERROR_BAD_PARAMETERS;
	}
	if (ptpip_set_nonblock(cmdfd) == -1) {
		ptpip_perror("cmdfd set nonblocking");
		PTPSOCK_CLOSE (params->cmdfd);
		return GP_ERROR_IO;
	}
	PTPSOCK_SOCKTYPE evtfd = params->evtfd = socket (PF_INET, SOCK_STREAM, PTPSOCK_PROTO);
	if (evtfd == PTPSOCK_INVALID) {
		ptpip_perror ("socket evt");
		PTPSOCK_CLOSE (params->cmdfd);
		return GP_ERROR_BAD_PARAMETERS;
	}
	if (ptpip_set_nonblock(evtfd) == -1) {
		ptpip_perror("evtfd set nonblocking");
		PTPSOCK_CLOSE (params->cmdfd);
		PTPSOCK_CLOSE (params->evtfd);
		return GP_ERROR_IO;
	}
	if (ptpip_connect_with_timeout (params->cmdfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in), PTPIP_DEFAULT_TIMEOUT_S, PTPIP_DEFAULT_TIMEOUT_MS) == -1) {
		ptpip_perror("connect cmd");
		PTPSOCK_CLOSE (params->cmdfd);
		PTPSOCK_CLOSE (params->evtfd);
		return GP_ERROR_IO;
	}
	ret = ptp_ptpip_init_command_request (params);
	if (ret != PTP_RC_OK) {
		PTPSOCK_CLOSE (params->cmdfd);
		PTPSOCK_CLOSE (params->evtfd);
		return translate_ptp_result (ret);
	}
	ret = ptp_ptpip_init_command_ack (params);
	if (ret != PTP_RC_OK) {
		PTPSOCK_CLOSE (params->cmdfd);
		PTPSOCK_CLOSE (params->evtfd);
		return translate_ptp_result (ret);
	}
	/* seen on Ricoh Theta, camera is not immediately ready. try again two times. */
	tries = 2;
	saddr.sin_port		= htons(eventport);
	do {
		if (PTPSOCK_ERR != ptpip_connect_with_timeout (params->evtfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in), PTPIP_DEFAULT_TIMEOUT_S, PTPIP_DEFAULT_TIMEOUT_MS))
			break;
		if ((ptpip_get_socket_error() == ECONNREFUSED) && (tries--)) {
			GP_LOG_D ("event connect failed, retrying after short wait");
			int sleep_ms = 100;
#ifdef WIN32
			Sleep(sleep_ms);
#else
			usleep(sleep_ms*1000);
#endif
			continue;
		}
		GP_LOG_E ("could not connect event");
		PTPSOCK_CLOSE (params->cmdfd);
		PTPSOCK_CLOSE (params->evtfd);
		return GP_ERROR_IO;
	} while (1);
	ret = ptp_ptpip_init_event_request (params);
	if (ret != PTP_RC_OK)
		return translate_ptp_result (ret);
	ret = ptp_ptpip_init_event_ack (params);
	if (ret != PTP_RC_OK)
		return translate_ptp_result (ret);
	GP_LOG_D ("ptpip connected!");
	return GP_OK;
}

int
ptp_ptpip_disconnect (PTPParams* params) {
	if (params->cmdfd != PTPSOCK_INVALID) {
		PTPSOCK_CLOSE (params->cmdfd);
		params->cmdfd = PTPSOCK_INVALID;
	}
	if (params->evtfd != PTPSOCK_INVALID) {
		PTPSOCK_CLOSE (params->evtfd);
		params->evtfd = PTPSOCK_INVALID;
	}
	if (params->jpgfd != PTPSOCK_INVALID) {
		PTPSOCK_CLOSE (params->jpgfd);
		params->jpgfd = PTPSOCK_INVALID;
	}
	GP_LOG_D ("ptpip disconnected!");
	return GP_OK;
}

int
ptpip_connect_with_timeout (int fd, const struct sockaddr *address, socklen_t address_len, int seconds, int milliseconds) {
	int res = 0;
	struct timeval tv;
	socklen_t errsz = sizeof(res);
	fd_set fds;

	if ((res = connect(fd, address, address_len)) == -1) {
		if (ptpip_get_socket_error() == EINPROGRESS) { // connection is not ready yet
			tv.tv_sec = seconds;
			tv.tv_usec = milliseconds * 1000;
			FD_ZERO(&fds);
			FD_SET(fd, &fds);
			res = select(fd+1, NULL, &fds, NULL, &tv);
			if (res == -1) { // Select failed
				ptpip_perror ("select");
				return -1;
			} else if (res == 0) { // Timeout
				ptpip_set_socket_error (ETIMEDOUT);
				return -1;
			} else { // Socket is marked as writable, connect is okay, query actual error status
				if(getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&res, &errsz) == -1) {
					ptpip_perror("getsockopt");
					return -1;
				} else if(res != 0) { // Socket has an error
					ptpip_set_socket_error (res);
					return -1;
				} else { // Connect successful
					return res;
				}
			}
		} else { // connect failed with error.
			return -1;
		}
	}
	return res;
}

ssize_t
ptpip_read_with_timeout (int fd, void *buf, size_t nbytes, int seconds, int milliseconds) {
	ssize_t res;
	struct timeval tv;
	fd_set fds;

	res = PTPSOCK_READ(fd, buf, nbytes);
	if (res == -1 && (ptpip_get_socket_error() == EAGAIN || ptpip_get_socket_error() == EWOULDBLOCK)) {
		tv.tv_sec = seconds;
		tv.tv_usec = milliseconds * 1000;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		res = select(fd+1, &fds, NULL, NULL, &tv);
		if (res == -1) { // Select failed
			ptpip_perror ("select");
			return -1;
		} else if (res == 0) { // Timeout
			ptpip_set_socket_error (ETIMEDOUT);
			return -1;
		} else { // Socket is marked as readable, do the read now
			return PTPSOCK_READ(fd, buf, nbytes);
		}
	} else {
		return res;
	}
}

ssize_t
ptpip_write_with_timeout (int fd, void *buf, size_t nbytes, int seconds, int milliseconds) {
	ssize_t res;
	struct timeval tv;
	fd_set fds;

	res = PTPSOCK_WRITE(fd, buf, nbytes);
	if (res == -1 && (ptpip_get_socket_error() == EAGAIN || ptpip_get_socket_error() == EWOULDBLOCK)) {
		tv.tv_sec = seconds;
		tv.tv_usec = milliseconds * 1000;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		res = select(fd+1, NULL, &fds, NULL, &tv);
		if (res == -1) { // Select failed
			ptpip_perror ("select");
			return -1;
		} else if (res == 0) { // Timeout
			ptpip_set_socket_error (ETIMEDOUT);
			return -1;
		} else { // Socket is marked as writable, do the write now
			return PTPSOCK_WRITE(fd, buf, nbytes);
		}
	} else {
		return res;
	}
}

int
ptpip_set_nonblock (int fd) {
#ifdef WIN32
	u_long mode = 0;
	if (ioctlsocket (fd, FIONBIO, &mode) == SOCKET_ERROR) {
		return -1;
	}
#else
	int flags = fcntl(fd, F_GETFL);
	if (flags < 0) {
		return -1;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		return -1;
	}
#endif
	return 0;
}

void
ptpip_perror (const char *what) {
#ifdef WIN32
	wchar_t *s = NULL;
	FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
		(LPWSTR)&s, 0, NULL);
	fprintf(stderr, "%s: %S\n", what, s);
	fflush(stderr);
	LocalFree(s);
#else
	perror(what);
#endif
}

int
ptpip_get_socket_error (void) {
#ifdef WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

void
ptpip_set_socket_error (int err) {
#ifdef WIN32
	WSASetLastError(err);
#endif
	errno = err;
}
