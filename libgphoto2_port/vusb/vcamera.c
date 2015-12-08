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

static void put_32bit_le(unsigned char *data, uint32_t x) {
	data[0] = x & 0xff;
	data[1] = (x>>8) & 0xff;
	data[2] = (x>>16) & 0xff;
	data[3] = (x>>24) & 0xff;
}

static void put_16bit_le(unsigned char *data, uint16_t x) {
	data[0] = x & 0xff;
	data[1] = (x>>8) & 0xff;
}

static void
ptp_response(vcamera *cam, uint16_t code, int nparams) {
	unsigned char	*offset;

	if (!cam->inbulk) {
		cam->inbulk = malloc(12 + nparams*4);
	} else {
		cam->inbulk = realloc(cam->inbulk,cam->nrinbulk+12+nparams*4);
	}
	offset = cam->inbulk + cam->nrinbulk;
	cam->nrinbulk += 12+nparams*4;
	put_32bit_le(offset,12+nparams*4);
	put_16bit_le(offset+4,0x3);
	put_16bit_le(offset+6,code);
	put_32bit_le(offset+8,cam->seqnr);
	/* FIXME: put params */
}

#define PTP_RC_OK 		0x2001
#define PTP_RC_GeneralError 	0x2002
#define PTP_RC_InvalidParameter	0x201d

static int
ptp_opensession_write(vcamera *cam, ptpcontainer *ptp) {
	if (ptp->nparams != 1) {
		gp_log (GP_LOG_ERROR,"ptp_opensession_write","params should be 1, is %d", ptp->nparams);
		ptp_response(cam, PTP_RC_GeneralError, 0);
		return 0;
	}
	if (ptp->params[0] == 0) {
		gp_log (GP_LOG_ERROR,"ptp_opensession_write","session must not be 0, is %d", ptp->params[0]);
		ptp_response(cam, PTP_RC_InvalidParameter, 0);
		return 0;
	}
	cam->session = ptp->params[0];
	ptp_response(cam,PTP_RC_OK,0);
	cam->seqnr++;
	return 1;
}

struct ptp_function {
	int	code;
	int	(*write)(vcamera *cam, ptpcontainer *ptp);
} ptp_functions[] = {
	{0x1002,	ptp_opensession_write },
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
		ptp.params[i] = ptp.seqnr = get_32bit_le(cam->outbulk+12+i*4);

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

	return cam;
}
