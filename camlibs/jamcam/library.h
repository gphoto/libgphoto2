/****************************************************************/
/* library.h  - Gphoto2 library for the KBGear JamCam v3.0      */
/*                                                              */
/* Copyright (C) 2001 Chris Pinkham                             */
/*                                                              */
/* Author: Chris Pinkham <cpinkham@infi.net>                    */
/*                                                              */
/* This program is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU General Public   */
/* License as published by the Free Software Foundation; either */
/* version 2 of the License, or (at your option) any later      */
/* version.                                                     */
/*                                                              */
/* This program is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU General Public License for more        */
/* details.                                                     */
/*                                                              */
/* You should have received a copy of the GNU General Public    */
/* License along with this program; if not, write to the Free   */
/* Software Foundation, Inc., 59 Temple Place, Suite 330,       */
/* Boston, MA 02111-1307, USA.                                  */
/****************************************************************/

#ifndef __LIBRARY_H__
#define __LIBRARY_H__

#define	RETRIES			10

#define SER_PKT_SIZE 2048
#define USB_PKT_SIZE 4096
#define DATA_SIZE  261600
#define DATA_INCR  261616
#define DATA_OFFSET 0x10

#define CHECK(result) {int res; res = result; if (res < 0) return (res);}

struct jamcam_file {
	int position;
	int width;
	int height;
	int data_incr;
};

int jamcam_enq(Camera *camera);
int jamcam_who_knows(Camera *camera);
int jamcam_write_packet(Camera *camera, char *packet, int length);
int jamcam_read_packet(Camera *camera, char *packet, int length);
int jamcam_file_count(Camera *camera);
struct jamcam_file *jamcam_file_info(Camera *camera, int number);
int jamcam_fetch_memory(Camera *camera, char *data, int start, int length );
int jamcam_request_image(Camera *camera, char *buf, int *len, int number);
int jamcam_request_thumbnail(Camera *camera, char *buf, int *len, int number );

#if 0
/***********************************************************************/
/* gamma table correction and bayer routines, hopefully will go into a */
/* common Gphoto2 area at some point.                                  */
/***********************************************************************/

int gp_create_gamma_table( unsigned char *table, double g );
int gp_gamma_correct_triple( unsigned char *data, int pixels,
		unsigned char *red, unsigned char *green, unsigned char *blue );
int gp_gamma_correct_single( unsigned char *data, int pixels,
		unsigned char *gtable );

#define BAYER_TILE_RGGB 0
#define BAYER_TILE_GRBG 1
#define BAYER_TILE_BGGR 2
#define BAYER_TILE_GBRG 3

int gp_bayer_decode(int w, int h, unsigned char *input, unsigned char *output,
	int tile);

#endif

#endif __LIBRARY_H__
