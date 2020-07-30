/*
	$Id$

	Copyright 2000 Mariusz Zynel <mariusz@mizar.org> (gPhoto port)
	Copyright 2000 Fredrik Roubert <roubert@df.lth.se> (idea)
	Copyright 1999 Galen Brooks <galen@nine.com> (DC1580 code)

	This file is part of the gPhoto project and may only be used,  modified,
	and distributed under the terms of the gPhoto project license,  COPYING.
	By continuing to use, modify, or distribute  this file you indicate that
	you have read the license, understand and accept it fully.

	THIS  SOFTWARE IS PROVIDED AS IS AND COME WITH NO WARRANTY  OF ANY KIND,
	EITHER  EXPRESSED OR IMPLIED.  IN NO EVENT WILL THE COPYRIGHT  HOLDER BE
	LIABLE FOR ANY DAMAGES RESULTING FROM THE USE OF THIS SOFTWARE.

	Note:

	This is a Panasonic DC1000 camera gPhoto library header file.
*/

/* DC1000 command codes */

#define	DSC1_CMD_SEND_DATA	0x00
#define	DSC1_CMD_GET_MODEL	0x02
#define	DSC1_CMD_SET_BAUD	0x04
#define	DSC1_CMD_GET_INDEX	0x07
#define	DSC1_CMD_CONNECT	0x10
#define	DSC1_CMD_DELETE		0x11
#define	DSC1_CMD_PREVIEW	0x14
#define	DSC1_CMD_SET_RES	0x15
#define	DSC1_CMD_SELECT		0x1a
#define	DSC1_CMD_GET_DATA	0x1e
#define	DSC1_CMD_RESET		0x1f

/* DC1000 response codes */

#define DSC1_RSP_DATA		0x00
#define DSC1_RSP_OK		0x01
#define DSC1_RSP_MODEL		0x03
#define DSC1_RSP_INDEX		0x08
#define DSC1_RSP_IMGSIZE	0x1d

/* DC1000 response buffer offsets, 0 - first byte in buffer */

#define DSC1_BUF_SIZE		12
#define DSC1_BUF_CMD		16
#define DSC1_BUF_DATA		17

/* End of dc1000.h */
