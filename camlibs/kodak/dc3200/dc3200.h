/* dc3200.h
 *
 * Copyright (C) 2000,2001,2002 donn morrison - dmorriso@gulf.uvic.ca
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

/****************************************************
 * kodak dc3200 digital camera driver library       *
 * for gphoto2                                      *
 *                                                  *
 * author: donn morrison - dmorriso@gulf.uvic.ca    *
 * date: dec 2000 - jan 2002                        *
 * license: gpl                                     *
 * version: 1.6                                     *
 *                                                  *
 ****************************************************/

#ifndef __DC3200_H__
#define __DC3200_H__

#include <gphoto2/gphoto2-camera.h>

#define CMD_LIST_FILES		0
#define CMD_GET_PREVIEW		1
#define CMD_GET_FILE		2

#define TIMEOUT	        	750

struct _CameraPrivateLibrary {
	int			pkt_seqnum;	/* sequence number */
	int			cmd_seqnum;	/* command seqnum */
	int			rec_seqnum;	/* last received seqnum */
	int			debug;
	time_t			last;		/* remember last recv time */
	GPContext		*context;	/* for progress updates */
};

int check_last_use(Camera *camera);

#endif
