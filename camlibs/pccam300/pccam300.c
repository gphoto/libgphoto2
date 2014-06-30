/****************************************************************/
/* pccam300.c - Gphoto2 library for the Creative PC-CAM 300     */
/*                                                              */
/* Authors: Till Adam <till@adam-lilienthal.de>                 */
/*          Miah Gregory <mace@darksilence.net>                 */
/*                                                              */
/* This library is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU Library General  */
/* Public License as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any     */
/* later version.                                               */
/*                                                              */
/* This library is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU Library General Public License for     */
/* more details.                                                */
/*                                                              */
/* You should have received a copy of the GNU Library General   */
/* Public License along with this library; if not, write to the */
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <_stdint.h>

#include "pccam300.h"

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-log.h>

#define GP_MODULE "pccam300"

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
#  define _(String) (String)
#  define N_(String) (String)
#endif

/*
 * waits until the status value is 0 or 8. 
 * if status == 0xb0 or 0x40 we will wait some more
 */
static int
pccam300_wait_for_status (GPPort *port)
{
	int retries = 20;
	unsigned char status = 1;

	while (status != 0x00 && retries--) {
		gp_port_set_timeout (port, 3000);
		CHECK(gp_port_usb_msg_read (port, 0x06, 0x00, 0x00, (char *)&status, 1));
		if (status == 0 || status == 8)
			return GP_OK;
		if (status == 0xb0) {
			gp_port_set_timeout (port, 200000);
			CHECK(gp_port_usb_msg_read (port, 0x06, 0x00, 0x00,
						    (char *)&status, 1));
		}
		if (status == 0x40) {
			gp_port_set_timeout (port, 400000);
			CHECK(gp_port_usb_msg_read (port, 0x06, 0x00, 0x00,
						    (char *)&status, 1));
		}
	}
	return GP_ERROR;
}

/*
 * FIXME
 */
int
pccam300_delete_file (GPPort *port, GPContext *context, int index)
{
	return GP_ERROR_NOT_SUPPORTED;
}

int
pccam300_delete_all (GPPort *port, GPContext *context)
{

	CHECK (gp_port_usb_msg_write (port, 0x4, 0x0, 0x0, NULL, 0));
	CHECK (pccam300_wait_for_status (port));
	CHECK (gp_port_usb_msg_write (port, 0x9, 0x0, 0x1, NULL, 0));
	CHECK (pccam300_wait_for_status (port));
	return GP_OK;
}

int
pccam300_get_filecount (GPPort *port, int *filecount)
{
	uint8_t response;

	gp_port_set_timeout (port, 400000);
	CHECK (gp_port_usb_msg_read (port, 0x08, 0x00, 0x00,
				     (char *)&response, 0x01));
	*filecount = response;
	return GP_OK;
}

int
pccam300_get_filesize (GPPort *port, unsigned int index,
                       unsigned int *filesize)
{
	uint8_t response[3];
	uint16_t i = index;

	gp_port_set_timeout (port, 400000);
	CHECK (gp_port_usb_msg_read (port, 0x08, i, 0x0001,
				     (char *)response, 0x03));
	*filesize = (response[0] & 0xff)
		+ (response[1] & 0xff) * 0x100 + (response[2] & 0xff) * 0x10000;

	return GP_OK;
}

int
pccam300_get_mem_info (GPPort *port, GPContext *context, int *totalmem,
                       int *freemem)
{
	unsigned char response[4];

	gp_port_set_timeout (port, 400000);
	CHECK (gp_port_usb_msg_read (port, 0x60, 0x00, 0x02,
				     (char *)response, 0x04));
	*totalmem = response[2] * 65536 + response[1] * 256 + response[0];
	CHECK (pccam300_wait_for_status (port));
	CHECK (gp_port_usb_msg_read (port, 0x60, 0x00, 0x03,
				     (char *)response, 0x04));
	*freemem = response[2] * 65536 + response[1] * 256 + response[0];
	CHECK (pccam300_wait_for_status (port));
	return GP_OK;
}


int
pccam300_get_file (GPPort *port, GPContext *context, int index,
                   unsigned char **data, unsigned int *size,
                   unsigned int *type)
{
	unsigned int data_size;
	int r;
	uint8_t *buf = NULL;

	/* This is somewhat strange, but works, and the win driver does the
	 * same. Apparently requesting the file size twice triggers the
	 * download of the file. */
	CHECK (pccam300_get_filesize (port, index, &data_size));
	CHECK (pccam300_get_filesize (port, index, &data_size));
	
	/* The jpeg header we will download after the data itself is 623
	 * bytes long. The first 0x200 bytes of the data appear to be
	 * garbage, they are overwritten by the header. */
	*size = data_size + 623 - 0x200;
	if (!(buf = malloc (*size)))
		return GP_ERROR_NO_MEMORY;

	/* Read the data into the buffer overlapping the header area by
	 * 0x200 bytes. */
	r = gp_port_read (port, (char *)buf + 623 - 0x200, data_size);
	if (r < GP_OK) {
		free (buf);
		return r;
	}

	/* FIXME: if we find a RIFF marker in the data stream, assume that
	 *        this is an avi, for now
	 *        NWG: Sat 18th January 2003.
	 */
	if (buf[0x243] == 'R' && buf[0x244] == 'I' &&
	    buf[0x245] == 'F' && buf[0x246] == 'F')
	{
		/* maybe an avi, but could be a wav too */
		*type = PCCAM300_MIME_AVI;
		memmove(buf, buf + 623 - 0x200, data_size);
		*size = data_size;
	} else {
		/* offset 0x8 in the downloaded data contains the identifier
		 * we need to request the correct header.
		 */
		r = gp_port_usb_msg_read(port, 0x0b, buf[623 - 0x200 + 8],
		                         0x3, (char *)buf, 623);
		if (r < GP_OK) {
			free (buf);
			return r;
		}
		*type = PCCAM300_MIME_JPEG;
	}
	*data = buf;
	return GP_OK;
}


int
pccam300_init (GPPort *port, GPContext *context)
{
	CHECK (gp_port_usb_msg_write (port, 0xe0, 0x00, 0x1, NULL, 0x00));
	CHECK (pccam300_wait_for_status (port));

	return GP_OK;
}
