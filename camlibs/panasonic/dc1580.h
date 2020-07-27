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

	This is a Panasonic DC1580 camera gPhoto library header file.
*/

/* DC1580 command codes */

#define	DSC2_CMD_SET_BAUD	0x04
#define	DSC2_CMD_SEND_DATA	0x05
#define	DSC2_CMD_GET_INDEX	0x07
#define	DSC2_CMD_CONNECT	0x10
#define	DSC2_CMD_DELETE		0x11
#define	DSC2_CMD_PREVIEW	0x14
#define	DSC2_CMD_SET_SIZE	0x15
#define	DSC2_CMD_THUMB		0x16
#define	DSC2_CMD_SELECT		0x1a
#define	DSC2_CMD_GET_DATA	0x1e
#define	DSC2_CMD_RESET		0x1f

/* DC1580 response codes */

#define DSC2_RSP_OK		0x01
#define DSC2_RSP_MODEL		0x03
#define DSC2_RSP_DATA		0x05
#define DSC2_RSP_INDEX		0x08
#define DSC2_RSP_IMGSIZE	0x1d

/* DC1580 response buffer offsets, 0 - first byte in buffer */

#define DSC2_BUF_BASE		0
#define DSC2_BUF_SEQ 		1
#define DSC2_BUF_SEQC		2
#define DSC2_BUF_CMD		3
#define DSC2_BUF_DATA		4
#define DSC2_BUF_CSUM		14

/* End of dc1580.h */
