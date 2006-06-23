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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
 * This is working, but unfinished!
 * - Event handling not finished.
 * - Some configure checking magic missing for the special header files 
 *   and functions.
 * - Not everything implementation correctly cross checked.
 */
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/select.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <gphoto2-library.h>
#include <gphoto2-port-log.h>
#include <gphoto2-setting.h>


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

static uint16_t ptp_ptpip_check_event (PTPParams* params);

/* send / receive functions */
uint16_t
ptp_ptpip_sendreq (PTPParams* params, PTPContainer* req)
{
	unsigned int 		ret;
	uint32_t		len = 18+req->Nparam*4;
	unsigned char 		*request = malloc(len);

	ptp_ptpip_check_event (params);

	htod32a(&request[ptpip_type],PTPIP_CMD_REQUEST);
	htod32a(&request[ptpip_len],len);
	htod32a(&request[ptpip_cmd_dataphase],1);	/* FIXME: dataphase handling */
	htod16a(&request[ptpip_cmd_code],req->Code);
	htod32a(&request[ptpip_cmd_transid],req->Transaction_ID);

	switch (req->Nparam) {
	case 5: htod32a(&request[ptpip_cmd_param5],req->Param5);
	case 4: htod32a(&request[ptpip_cmd_param4],req->Param4);
	case 3: htod32a(&request[ptpip_cmd_param3],req->Param3);
	case 2: htod32a(&request[ptpip_cmd_param2],req->Param2);
	case 1: htod32a(&request[ptpip_cmd_param1],req->Param1);
	case 0:
	default:
		break;
	}
	gp_log_data ( "ptpip/oprequest", (char*)request, len);
	ret = write (params->cmdfd, request, len);
	if (ret == -1)
		perror ("sendreq/write to cmdfd");
	if (ret != len) {
		gp_log (GP_LOG_ERROR, "ptpip","ptp_ptpip_sendreq() len =%d but ret=%d", len, ret);
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
		ret = read (fd, xhdr + curread, len - curread);
		if (ret == -1) {
			perror ("read PTPIPHeader");
			return PTP_RC_GeneralError;
		}
		gp_log_data ( "ptpip/generic_read", (char*)xhdr+curread, ret);
		curread += len;
		if (ret == 0) {
			gp_log (GP_LOG_ERROR, "ptpip", "End of stream after reading %d bytes of ptpipheader", ret);
			return PTP_RC_GeneralError;
		}
	}
	len = dtoh32 (hdr->length) - sizeof (PTPIPHeader);
	if (len < 0) {
		gp_log (GP_LOG_ERROR, "ptpip/generic_read", "len < 0, %d?", len);
		return PTP_RC_GeneralError;
	}
	*data = malloc (len);
	if (!*data) {
		gp_log (GP_LOG_ERROR, "ptpip/generic_read", "malloc failed.");
		return PTP_RC_GeneralError;
	}
	curread = 0;
	while (curread < len) {
		ret = read (fd, (*data)+curread, len-curread);
		if (ret == -1) {
			gp_log (GP_LOG_ERROR, "ptpip/generic_read", "error %d in reading PTPIP data", errno);
			free (*data);*data = NULL;
			return PTP_RC_GeneralError;
		} else {
			gp_log_data ( "ptpip/generic_read", (char*)((*data)+curread), ret);
		}
		if (ret == 0)
			break;
		curread += ret;
	}
	if (curread != len) {
		gp_log (GP_LOG_ERROR, "ptpip/generic_read", "read PTPIP data, ret %d vs len %d", ret, len);
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
	fd_set		infds;
	struct timeval	timeout;
	int ret;
	unsigned char*	data = NULL;
	PTPIPHeader	hdr;

	FD_ZERO(&infds);
	FD_SET(params->evtfd, &infds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 1;
	if (1 != select (params->evtfd+1, &infds, NULL, NULL, &timeout))
		return PTP_RC_OK;
	ret = ptp_ptpip_evt_read (params, &hdr, &data);
	if (ret != PTP_RC_OK)
		return ret;
	gp_log (GP_LOG_DEBUG,"ptpip/check_event", "hdr type %d, length %d", hdr.type, hdr.length);
	return PTP_RC_OK;
}

#define ptpip_startdata_transid		0
#define ptpip_startdata_totallen	4
#define ptpip_data_transid		0
#define ptpip_data_payload		4

#define WRITE_BLOCKSIZE 65536
uint16_t
ptp_ptpip_senddata (PTPParams* params, PTPContainer* ptp,
			unsigned char *data, unsigned int size)
{
	unsigned char	request[16];
	int		ret, curwrite, towrite;
	unsigned char*	xdata;

	htod32a(&request[ptpip_type],PTPIP_START_DATA_PACKET);
	htod32a(&request[ptpip_len],sizeof(request));
	htod32a(&request[ptpip_startdata_transid  + 8],ptp->Transaction_ID);
	htod32a(&request[ptpip_startdata_totallen + 8],size);
	gp_log_data ( "ptpip/senddata", (char*)request, sizeof(request));
	ret = write (params->cmdfd, request, sizeof(request));
	if (ret == -1)
		perror ("sendreq/write to cmdfd");
	if (ret != sizeof(request)) {
		gp_log (GP_LOG_ERROR, "ptpip/senddata", "ptp_ptpip_senddata() len=%d but ret=%d", (int)sizeof(request), ret);
		return PTP_RC_GeneralError;
	}
	xdata = malloc(WRITE_BLOCKSIZE+8+4);
	if (!xdata) return PTP_RC_GeneralError;
	curwrite = 0;
	while (curwrite < size) {
		int type, written, towrite2;

		ptp_ptpip_check_event (params);

		towrite = size - curwrite;
		if (towrite > WRITE_BLOCKSIZE) {
			towrite	= WRITE_BLOCKSIZE;
			type	= PTPIP_DATA_PACKET;
		} else {
			type	= PTPIP_END_DATA_PACKET;
		}
		towrite2 = towrite+12;
		htod32a(&xdata[ptpip_type], type);
		htod32a(&xdata[ptpip_len], towrite2);
		htod32a(&xdata[ptpip_data_transid+8], ptp->Transaction_ID);
		memcpy (&xdata[ptpip_data_payload+8], data+curwrite, towrite);
		gp_log_data("ptpip/senddata", (char*)xdata, towrite2);
		written = 0;
		while (written > towrite2) {
			ret = write (params->cmdfd, xdata+written, towrite2-written);
			if (ret == -1) {
				perror ("write in senddata failed");
				return PTP_RC_GeneralError;
			}
			written += ret;
		}
		curwrite += towrite;
	}
	return PTP_RC_OK;
}

uint16_t
ptp_ptpip_getdata (PTPParams* params, PTPContainer* ptp,
		unsigned char **data, unsigned int *readlen)
{
	PTPIPHeader		hdr;
	unsigned char		*xdata = NULL;
	uint16_t 		ret;
	int			toread, curread;

	ret = ptp_ptpip_cmd_read (params, &hdr, &xdata);
	if (ret != PTP_RC_OK)
		return ret;

	if (dtoh32(hdr.type) == PTPIP_CMD_RESPONSE) { /* might happen if we have no data transfer due to error? */
		gp_log (GP_LOG_ERROR, "ptpip/getdata", "Unexpected ptp response, code %x", dtoh32a(&xdata[8]));
		return PTP_RC_GeneralError;
	}
	if (dtoh32(hdr.type) != PTPIP_START_DATA_PACKET) {
		gp_log (GP_LOG_ERROR, "ptpip/getdata", "got reply type %d\n", dtoh32(hdr.type));
		free (data);
		return PTP_RC_GeneralError;
	}
	toread = dtoh32a(&xdata[ptpip_data_payload]);
	free (xdata); xdata = NULL;

	*readlen = toread;
	*data = malloc(toread);
	curread = 0;
	while (curread < toread) {
		ret = ptp_ptpip_cmd_read (params, &hdr, &xdata);
		if (ret != PTP_RC_OK)
			return ret;
		if (dtoh32(hdr.type) == PTPIP_END_DATA_PACKET) {
			int datalen = dtoh32(hdr.length)-8-ptpip_data_payload;
			if (datalen > (toread-curread)) {
				gp_log (GP_LOG_ERROR, "ptpip/getdata",
					"returned data is too much, expected %d, got %d",
					(toread-curread),datalen
				);
				break;
			}
			memcpy ((*data)+curread, xdata+ptpip_data_payload, datalen);
			curread += datalen;
			free (xdata); xdata = NULL;
			continue;
		}
		if (dtoh32(hdr.type) == PTPIP_DATA_PACKET) {
			int datalen = dtoh32(hdr.length)-8-ptpip_data_payload;
			if (datalen > (toread-curread)) {
				gp_log (GP_LOG_ERROR, "ptpip/getdata",
					"returned data is too much, expected %d, got %d",
					(toread-curread),datalen
				);
				break;
			}
			memcpy ((*data)+curread, xdata+ptpip_data_payload, datalen);
			curread += datalen;
			free (xdata); xdata = NULL;
			continue;
		}
		gp_log (GP_LOG_ERROR, "ptpip/getdata", "ret type %d", hdr.type);
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

	ret = ptp_ptpip_cmd_read (params, &hdr, &data);
	if (ret != PTP_RC_OK)
		return ret;

	resp->Code		= dtoh16a(&data[ptpip_resp_code]);
	resp->Transaction_ID	= dtoh32a(&data[ptpip_resp_transid]);
	n = (dtoh32(hdr.length) - ptpip_resp_param1)/sizeof(uint32_t);
	switch (n) {
	case 5: resp->Param5 = dtoh32a(&data[ptpip_resp_param5]);
	case 4: resp->Param4 = dtoh32a(&data[ptpip_resp_param4]);
	case 3: resp->Param3 = dtoh32a(&data[ptpip_resp_param3]);
	case 2: resp->Param2 = dtoh32a(&data[ptpip_resp_param2]);
	case 1: resp->Param1 = dtoh32a(&data[ptpip_resp_param1]);
	case 0: break;
	default:
		gp_log( GP_LOG_ERROR, "ptpip/getresp", "response got %d parameters?", n);
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
	int 		len,i,ret;
	unsigned char	guid[] = {
		0x61,0x10,0x85,0x7e, 0xfa,0xae,0x98,0x49,
		0x8c,0xa1,0x5e,0x7d, 0x8f,0x9d,0x81,0x29
	};

	if (gethostname (hostname, sizeof(hostname)))
		return PTP_RC_GeneralError;
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

	gp_log_data ( "ptpip/init_cmd", (char*)cmdrequest, len);
	ret = write (params->cmdfd, cmdrequest, len);
	if (ret == -1) {
		perror("write init cmd request");
		return PTP_RC_GeneralError;
	}
	gp_log (GP_LOG_ERROR,"ptpip/init_cmd", "return %d / len %d", ret, len);
	if (ret != len) {
		gp_log (GP_LOG_ERROR, "ptpip", "return %d vs len %d", ret, len);
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

	ret = ptp_ptpip_cmd_read (params, &hdr, &data);
	if (ret != PTP_RC_OK) {
		return ret;
	}
	if (hdr.type != dtoh32(PTPIP_INIT_COMMAND_ACK)) {
		gp_log (GP_LOG_ERROR, "ptpip/init_cmd_ack", "bad type returned %d", htod32(hdr.type));
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

	gp_log_data ( "ptpip/init_event", (char*)evtrequest, ptpip_eventinit_size);
	ret = write (params->evtfd, evtrequest, ptpip_eventinit_size);
	if (ret == -1) {
		perror("write init evt request");
		return PTP_RC_GeneralError;
	}
	if (ret != ptpip_eventinit_size) {
		gp_log (GP_LOG_ERROR, "ptpip", "unexpected retsize %d, expected %d", ret, ptpip_eventinit_size);
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
	if (hdr.type != dtoh32(PTPIP_INIT_EVENT_ACK)) {
		gp_log (GP_LOG_ERROR, "ptpip", "bad type returned %d\n", htod32(hdr.type));
		return PTP_RC_GeneralError;
	}
	free (data);
	return PTP_RC_OK;
}


/* Event handling functions */

/* PTP Events wait for or check mode */
#define PTP_EVENT_CHECK			0x0000	/* waits for */
#define PTP_EVENT_CHECK_FAST		0x0001	/* checks */

static inline uint16_t
ptp_ptpip_event (PTPParams* params, PTPContainer* event, int wait)
{
	fprintf(stderr,"event()\n");
	return PTP_RC_OK;
}

uint16_t
ptp_ptpip_event_check (PTPParams* params, PTPContainer* event) {
	return ptp_ptpip_event (params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_ptpip_event_wait (PTPParams* params, PTPContainer* event) {
	return ptp_ptpip_event (params, event, PTP_EVENT_CHECK);
}


int
ptp_ptpip_connect (PTPParams* params, const char *address) {
	char 		*addr, *s, *p;
	int		port;
	struct sockaddr_in	saddr;
	uint16_t	ret;

	gp_log (GP_LOG_DEBUG,"ptpip/connect", "connecting to %s.", address);
	if (NULL == strchr (address,':'))
		return GP_ERROR_BAD_PARAMETERS;
	addr = strdup (address);
	s = strchr (addr,':'); *s = '\0';
	p = strchr (s+1,':');
	port = 15740;
	if (p) {
		*p = '\0';
		if (!sscanf (p+1,"%d",&port)) {
			fprintf(stderr,"failed to scan for port in %s\n", p+1);
			free (addr);
			return GP_ERROR_BAD_PARAMETERS;
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
	ret = ptp_ptpip_init_command_request (params);
	if (ret != PTP_RC_OK)
		return GP_ERROR_IO;
	ret = ptp_ptpip_init_command_ack (params);
	if (ret != PTP_RC_OK)
		return GP_ERROR_IO;
	if (-1 == connect (params->evtfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in))) {
		perror ("connect evt");
		close (params->cmdfd);
		close (params->evtfd);
		return GP_ERROR_IO;
	}
	ret = ptp_ptpip_init_event_request (params);
	if (ret != PTP_RC_OK)
		return GP_ERROR_IO;
	ret = ptp_ptpip_init_event_ack (params);
	if (ret != PTP_RC_OK)
		return GP_ERROR_IO;
	gp_log (GP_LOG_DEBUG, "ptpip/connect", "ptpip connected!");
	return GP_OK;
}
