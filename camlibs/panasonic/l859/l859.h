/*
	$Id$

	Copyright (c) 2001 Andrew Selkirk <aselkirk@mailandnews.com>

	This file is part of the gPhoto project and may only be used,  modified,
	and distributed under the terms of the gPhoto project license,  COPYING.
	By continuing to use, modify, or distribute  this file you indicate that
	you have read the license, understand and accept it fully.

	THIS  SOFTWARE IS PROVIDED AS IS AND COME WITH NO WARRANTY  OF ANY KIND,
	EITHER  EXPRESSED OR IMPLIED.  IN NO EVENT WILL THE COPYRIGHT  HOLDER BE
	LIABLE FOR ANY DAMAGES RESULTING FROM THE USE OF THIS SOFTWARE.

	Note:

	This is the Panasonic PV-L859 camera gPhoto library source code.

*/

#include <stdio.h>
#include <errno.h>
#include <gphoto2.h>

#define L859_BLOCKSIZE    0x74
#define L859_BUFSIZE      0x74

struct _CameraPrivateLibrary {
	char buf[L859_BUFSIZE];
	int  size;
	int  speed;
};

/* L859 Command Codes */
#define L859_CMD_CONNECT        0x2a
#define L859_CMD_INIT           0x28
#define L859_CMD_SPEED_DEFAULT  0x00
#define L859_CMD_SPEED_19200    0x22
#define L859_CMD_SPEED_57600    0x24
#define L859_CMD_SPEED_115200   0x26
#define	L859_CMD_RESET	        0x20
#define L859_CMD_IMAGE          0xd0
#define L859_CMD_PREVIEW        0xe8
#define L859_CMD_PREVIEW_NEXT   0xe5
#define L859_CMD_ACK            0x06
#define L859_CMD_DELETE_1       0xd1
#define L859_CMD_DELETE_2       0xd2
#define L859_CMD_DELETE_3       0xd3
#define L859_CMD_DELETE_ALL     0xef
#define L859_CMD_DELETE_ACK     0x15

/* End of dc.h */
