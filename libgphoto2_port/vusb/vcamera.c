/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* camera.c
 *
 * Copyright (c) 2015 Marcus Meissner <marcus@jet.franken.de>
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

#include "config.h"
#include <gphoto2/gphoto2-port-library.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <string.h>
#include <stdint.h>

#include "vcamera.h"

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-log.h>

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

#define CHECK(result) {int r=(result); if (r<0) return (r);}

static uint32_t get_32bit_le(unsigned char *data) {
	return	data[0]	| (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static uint16_t get_16bit_le(unsigned char *data) {
	return	data[0]	| (data[1] << 8);
}

static int put_32bit_le(unsigned char *data, uint32_t x) {
	data[0] = x & 0xff;
	data[1] = (x>>8) & 0xff;
	data[2] = (x>>16) & 0xff;
	data[3] = (x>>24) & 0xff;
	return 4;
}

static int put_16bit_le(unsigned char *data, uint16_t x) {
	data[0] = x & 0xff;
	data[1] = (x>>8) & 0xff;
	return 2;
}

static int put_string(unsigned char *data, char *str) {
	int i;

	if (strlen(str)>255)
		gp_log (GP_LOG_ERROR, "put_string", "string length is longer than 255 characters");

	data[0] = strlen (str);
	for (i=0;i<data[0];i++)
		put_16bit_le(data+1+2*i,str[i]);

	return 1+strlen(str)*2;
}

static int put_16bit_le_array(unsigned char *data, uint16_t *arr, int cnt) {
	int x = 0, i;

	x += put_32bit_le (data,cnt);
	for (i=0;i<cnt;i++)
		x += put_16bit_le (data+x, arr[i]);
	return x;
}

static void
ptp_senddata(vcamera *cam, uint16_t code, unsigned char *data, int bytes) {
	unsigned char	*offset;
	int size = bytes + 12;

	if (!cam->inbulk) {
		cam->inbulk = malloc(size);
	} else {
		cam->inbulk = realloc(cam->inbulk,cam->nrinbulk+size);
	}
	offset = cam->inbulk + cam->nrinbulk;
	cam->nrinbulk += size;

	put_32bit_le(offset,size);
	put_16bit_le(offset+4,0x2);
	put_16bit_le(offset+6,code);
	put_32bit_le(offset+8,cam->seqnr);
	memcpy(offset+12,data,bytes);
}
static void
ptp_response(vcamera *cam, uint16_t code, int nparams) {
	unsigned char	*offset;
	int 		x = 0;

	if (!cam->inbulk) {
		cam->inbulk = malloc(12 + nparams*4);
	} else {
		cam->inbulk = realloc(cam->inbulk,cam->nrinbulk+12+nparams*4);
	}
	offset = cam->inbulk + cam->nrinbulk;
	cam->nrinbulk += 12+nparams*4;
	x += put_32bit_le(offset+x,12+nparams*4);
	x += put_16bit_le(offset+x,0x3);
	x += put_16bit_le(offset+x,code);
	x += put_32bit_le(offset+x,cam->seqnr);
	/* FIXME: put params */

	cam->seqnr++;
}

#define PTP_RC_OK 			0x2001
#define PTP_RC_GeneralError 		0x2002
#define PTP_RC_InvalidParameter		0x201D
#define PTP_RC_SessionAlreadyOpened     0x201E

#define CHECK_PARAM_COUNT(x)											\
	if (ptp->nparams != x) {										\
		gp_log (GP_LOG_ERROR,"ptp_deviceinfo_write","params should be %d, but is %d", x, ptp->nparams);	\
		ptp_response(cam, PTP_RC_GeneralError, 0);							\
		return 1;											\
	}

#define CHECK_SEQUENCE_NUMBER()											\
	if (ptp->seqnr != cam->seqnr) {										\
		/* not clear if normal cameras react like this */						\
		gp_log (GP_LOG_ERROR,"vcam_process_output", "seqnr %d was sent, expected was %d", ptp.seqnr, cam->seqnr);\
		ptp_response(cam,PTP_RC_GeneralError,0);							\
		return;												\
	}\

static int
ptp_opensession_write(vcamera *cam, ptpcontainer *ptp) {
	CHECK_PARAM_COUNT(1);

	if (ptp->params[0] == 0) {
		gp_log (GP_LOG_ERROR,"ptp_opensession_write","session must not be 0, is %d", ptp->params[0]);
		ptp_response(cam, PTP_RC_InvalidParameter, 0);
		return 1;
	}
	if (cam->session) {
		gp_log (GP_LOG_ERROR,"ptp_opensession_write","session is already open");
		ptp_response(cam, PTP_RC_SessionAlreadyOpened, 0);
		return 1;
	}
	cam->session = ptp->params[0];
	ptp_response(cam,PTP_RC_OK,0);
	return 1;
}

static int
ptp_deviceinfo_write(vcamera *cam, ptpcontainer *ptp) {
	unsigned char	*data;
	int		x = 0;
	uint16_t	opcodes[2];

	CHECK_PARAM_COUNT(0);

	/* Session does not need to be open for GetDeviceInfo */

	/* Getdeviceinfo is special. it can be called with transid 0 outside of the session. */
	if ((ptp->seqnr != 0) && (ptp->seqnr != cam->seqnr)) {
		/* not clear if normal cameras react like this */						\
		gp_log (GP_LOG_ERROR,"vcam_process_output", "seqnr %d was sent, expected was %d", ptp->seqnr, cam->seqnr);\
		ptp_response(cam,PTP_RC_GeneralError,0);							\
		return 1;
	}
	data = malloc(2000);

	x += put_16bit_le(data+x,100);		/* StandardVersion */
	x += put_32bit_le(data+x,0);		/* VendorExtensionID */
	x += put_16bit_le(data+x,0);		/* VendorExtensionVersion */
	x += put_string(data+x,"GPhoto-VirtualCamera: 1.0;");	/* VendorExtensionDesc */
	x += put_16bit_le(data+x,0);		/* FunctionalMode */

	opcodes[0] = 0x1001;
	opcodes[1] = 0x1002;
	x += put_16bit_le_array(data+x,opcodes,2);	/* OperationsSupported */
	x += put_16bit_le_array(data+x,NULL,0);		/* EventsSupported */
	x += put_16bit_le_array(data+x,NULL,0);		/* DevicePropertiesSupported */
	x += put_16bit_le_array(data+x,NULL,0);		/* CaptureFormats */
	x += put_16bit_le_array(data+x,NULL,0);		/* ImageFormats */
	x += put_string(data+x,"GPhoto");		/* Manufacturer */
	x += put_string(data+x,"VirtualCamera");	/* Model */
	x += put_string(data+x,"2.5.9");		/* DeviceVersion */
	x += put_string(data+x,"0.1");			/* DeviceVersion */
	x += put_string(data+x,"1");			/* SerialNumber */

	ptp_senddata(cam,0x1001,data,x);
	ptp_response(cam,0x2001,0);
	return 1;
}

struct ptp_function {
	int	code;
	int	(*write)(vcamera *cam, ptpcontainer *ptp);
} ptp_functions[] = {
	{0x1002,	ptp_opensession_write },
	{0x1001,	ptp_deviceinfo_write },
};

static int vcam_init(vcamera* cam) {
	return GP_OK;
}

static int vcam_exit(vcamera* cam) {
	return GP_OK;
}

static int vcam_open(vcamera* cam) {
	return GP_OK;
}

static int vcam_close(vcamera* cam) {
	return GP_OK;
}

static void
vcam_process_output(vcamera *cam) {
	ptpcontainer	ptp;
	int		i;

	if (cam->nroutbulk < 4)
		return; /* wait for more data */

	ptp.size = get_32bit_le(cam->outbulk);
	if (ptp.size > cam->nroutbulk)
		return; /* wait for more data */

	if (ptp.size < 12) {	/* No ptp command can be less than 12 bytes */
		/* not clear if normal cameras react like this */
		gp_log (GP_LOG_ERROR,"vcam_process_output", "input size was %d, minimum is 12", ptp.size);
		ptp_response(cam,PTP_RC_GeneralError,0);
		memmove(cam->outbulk,cam->outbulk+ptp.size,cam->nroutbulk-ptp.size);
		cam->nroutbulk -= ptp.size;
		return;
	}
	/* ptp:  4 byte size, 2 byte opcode, 2 byte type, 4 byte serial number */
	ptp.type  = get_16bit_le(cam->outbulk+4);
	ptp.code  = get_16bit_le(cam->outbulk+6);
	ptp.seqnr = get_32bit_le(cam->outbulk+8);

	if ((ptp.type != 1) && (ptp.type != 2)) { /* We want either CMD or DATA phase. */
		/* not clear if normal cameras react like this */
		gp_log (GP_LOG_ERROR,"vcam_process_output", "expected CMD or DATA, but type was %d", ptp.type);
		ptp_response(cam,PTP_RC_GeneralError,0);
		memmove(cam->outbulk,cam->outbulk+ptp.size,cam->nroutbulk-ptp.size);
		cam->nroutbulk -= ptp.size;
		return;
	}
	if ((ptp.code & 0x7000) != 0x1000) {
		/* not clear if normal cameras react like this */
		gp_log (GP_LOG_ERROR,"vcam_process_output", "OPCODE 0x%04x does not start with 0x1 or 0x9", ptp.code);
		ptp_response(cam,PTP_RC_GeneralError,0);
		memmove(cam->outbulk,cam->outbulk+ptp.size,cam->nroutbulk-ptp.size);
		cam->nroutbulk -= ptp.size;
		return;
	}
	if ((ptp.size - 12) % 4) {
		/* not clear if normal cameras react like this */
		gp_log (GP_LOG_ERROR,"vcam_process_output", "SIZE-12 is not divisible by 4, but is %d", ptp.size-12);
		ptp_response(cam,PTP_RC_GeneralError,0);
		memmove(cam->outbulk,cam->outbulk+ptp.size,cam->nroutbulk-ptp.size);
		cam->nroutbulk -= ptp.size;
		return;
	}
	if ((ptp.size - 12) / 4 >= 6) {
		/* not clear if normal cameras react like this */
		gp_log (GP_LOG_ERROR,"vcam_process_output", "(SIZE-12)/4 is %d, exceeds maximum arguments", (ptp.size-12)/4);
		ptp_response(cam,PTP_RC_GeneralError,0);
		memmove(cam->outbulk,cam->outbulk+ptp.size,cam->nroutbulk-ptp.size);
		cam->nroutbulk -= ptp.size;
		return;
	}
	ptp.nparams = (ptp.size - 12)/4;
	for (i=0;i<ptp.nparams;i++)
		ptp.params[i] = get_32bit_le(cam->outbulk+12+i*4);

	cam->nroutbulk -= ptp.size;

	/* call the opcode handler */

	for (i=0;i<sizeof(ptp_functions)/sizeof(ptp_functions[0]);i++) {
		if (ptp_functions[i].code == ptp.code) {
			ptp_functions[i].write(cam,&ptp);
			break;
		}
	}
}

static int vcam_read(vcamera*cam, int ep, char *data, int bytes) {
	int	toread = bytes;

	if (toread > cam->nrinbulk)
		toread = cam->nrinbulk;
	memcpy(data, cam->inbulk, toread);
	memmove(cam->inbulk + toread, cam->inbulk, (cam->nrinbulk - toread));
	cam->nrinbulk -= toread;
	return toread;
}

static int vcam_write(vcamera*cam, int ep, const char *data, int bytes) {
	/*gp_log_data("vusb", data, bytes, "data, vcam_write");*/
	if (!cam->outbulk) {
		cam->outbulk = malloc(bytes);
	} else {
		cam->outbulk = realloc(cam->outbulk,cam->nroutbulk + bytes);
	}
	memcpy(cam->outbulk + cam->nroutbulk, data, bytes);
	cam->nroutbulk += bytes;

	vcam_process_output(cam);

	return bytes;
}

vcamera*
vcamera_new(void) {
	vcamera *cam;

	cam = calloc(1,sizeof(vcamera));
	if (!cam) return NULL;

	cam->init = vcam_init;
	cam->exit = vcam_exit;
	cam->open = vcam_open;
	cam->close = vcam_close;

	cam->read = vcam_read;
	cam->write = vcam_write;

	cam->seqnr = 0;

	return cam;
}
