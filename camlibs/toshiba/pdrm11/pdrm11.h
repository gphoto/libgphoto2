/* pdrm11.h -- interfaces directly with the camera
 *
 * Copyright 2003 David Hogue <david@jawa.gotdns.org>
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

#ifndef __PDRM11_H__
#define __PDRM11_H__

#include "config.h"
#include <gphoto2/gphoto2.h>
#include <_stdint.h>

/* once in a while a gp_port command will fail and then work on a
 * second try this tries to run the command twice and returns the
 * error code if it fails the second time */
#define CHECK(result) { \
	int res; \
	res = result; \
	if (res < 0) { \
		res = result; \
		if (res < 0) { \
			GP_DEBUG("%s--%d: %s returned 0x%x", __FILE__, __LINE__, #result, res); \
			return res; \
		} \
	} \
}

#define CHECK_AND_FREE(result, buf) { \
	int res; \
	res = result; \
	if (res < 0) { \
		res = result; \
		if (res < 0) { \
			GP_DEBUG("%s--%d: %s returned 0x%x", __FILE__, __LINE__, #result, res); \
			free (buf); \
			return res; \
		} \
	} \
}


#define PDRM11_CMD_INIT1	htole16( 0x1f40 )
#define PDRM11_CMD_INIT2	htole16( 0x1f30 )
#define PDRM11_CMD_GET_PIC	htole16( 0x9300 )
#define PDRM11_CMD_GET_THUMB	htole16( 0x9b00 )
#define PDRM11_CMD_GET_INFO	htole16( 0xad00 )
#define PDRM11_CMD_SELECT_PIC2	htole16( 0xae00 )
#define PDRM11_CMD_SELECT_PIC1	htole16( 0xb200 )
#define PDRM11_CMD_GET_NUMPICS	htole16( 0xb600 )
#define PDRM11_CMD_GET_FILENAME	htole16( 0xb900 )
#define PDRM11_CMD_GET_FILESIZE	htole16( 0xb900 )
#define PDRM11_CMD_DELETE	htole16( 0xba40 )
#define PDRM11_CMD_ZERO		htole16( 0xbf01 )	/* not sure what this is, but it almost always returns 00 00 */
#define PDRM11_CMD_READY	htole16( 0xd000 )
#define PDRM11_CMD_GET_THUMBSIZE htole16( 0xe600 )
#define PDRM11_CMD_PING1	htole16( 0xd700 )
#define PDRM11_CMD_PING2	htole16( 0xd800 )
#define PDRM11_CMD_PING3	htole16( 0xd701 )
#define PDRM11_TYPE_JPEG	1
#define PDRM11_TYPE_TIFF	2


int pdrm11_init(GPPort *port);
int pdrm11_get_file(CameraFilesystem *fs, const char *filename, CameraFileType type,
			CameraFile *file, GPPort *port, uint16_t picNum);
int pdrm11_get_filenames(GPPort *port, CameraList *list);
int pdrm11_delete_file(GPPort *port, int picNum);



#endif /* __PDRM11_H__ */
