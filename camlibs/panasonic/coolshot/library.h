/****************************************************************/
/* library.h  - Gphoto2 library for accessing the Panasonic     */
/*              Coolshot KXL-600A & KXL-601A digital cameras.   */
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

#define	RETRIES			10

#define COOLSHOT_DONE	0x00
#define COOLSHOT_PKT	0x01
#define COOLSHOT_ENQ	0x05
#define COOLSHOT_ACK	0x06
#define COOLSHOT_NAK	0x15

#define CHECK(result) {int res; res = result; if (res < 0) return (res);}

int coolshot_sm           (Camera *camera);
int coolshot_sb           (Camera *camera, int speed);
int coolshot_fs           (Camera *camera, int number);
int coolshot_sp			(Camera *camera);
int coolshot_ack			(Camera *camera);
int coolshot_nak			(Camera *camera);
int coolshot_enq			(Camera *camera);
int coolshot_write_packet (Camera *camera, char *packet);
int coolshot_read_packet  (Camera *camera, char *packet);
int coolshot_file_count		(Camera *camera);
int coolshot_check_checksum( char *packet, int length );
int coolshot_request_image (Camera *camera, CameraFile *file, char *buf, int *len, int number);
int coolshot_request_thumbnail (Camera *camera, CameraFile *file, char *buf, int *len, int number);
int coolshot_download_image (Camera *camera, CameraFile *file, char *buf, int *len, int thumbnail);
int coolshot_build_thumbnail (char *data, int *size);
