/****************************************************************/
/* library.h  - Gphoto2 library for the KBGear JamCam v2 and v3 */
/*                                                              */
/* Copyright (C) 2001 Chris Pinkham                             */
/*                                                              */
/* Author: Chris Pinkham <cpinkham@infi.net>                    */
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
/* Free Software Foundation, Inc., 59 Temple Place - Suite 330, */
/* Boston, MA 02111-1307, USA.                                  */
/****************************************************************/

#ifndef __LIBRARY_H__
#define __LIBRARY_H__

#define	RETRIES			10

#define SER_PKT_SIZE 4096
#define USB_PKT_SIZE 4096

#define CHECK(result) {int res; res = result; if (res < 0) return (res);}

struct jamcam_file {
	int position;
	int width;
	int height;
	int data_incr;
};

int jamcam_enq(Camera *camera);
int jamcam_query_mmc_card(Camera *camera);
int jamcam_write_packet(Camera *camera, char *packet, int length);
int jamcam_read_packet(Camera *camera, char *packet, int length);
int jamcam_file_count(Camera *camera);
struct jamcam_file *jamcam_file_info(Camera *camera, int number);
int jamcam_fetch_memory(Camera *camera, CameraFile *file, char *data, int start, int length );
int jamcam_request_image(Camera *camera, CameraFile *file, char *buf, int *len, int number);
int jamcam_request_thumbnail(Camera *camera, CameraFile *file, char *buf, int *len, int number );

#endif /* __LIBRARY_H__ */
