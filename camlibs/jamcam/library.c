/****************************************************************/
/* library.c  - Gphoto2 library for the KBGear JamCam v3.0      */
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

#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "library.h"

struct jamcam_file jamcam_files[1024];
static int jamcam_count = 0;

static int jamcam_set_usb_mem_pointer( Camera *camera, int position ) {
	char reply[4];

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_set_usb_mem_pointer");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** position:  %d (0x%x)",
		position, position);

	gp_port_usb_msg_write( camera->port,
		0xa1,
		( position       ) & 0xffff,
		( position >> 16 ) & 0xffff,
		NULL, 0 );

	gp_port_usb_msg_read( camera->port,
		0xa0,
		0,
		0,
		reply, 4 );

	return( GP_OK );
}

int jamcam_file_count (Camera *camera) {
	char buf[16];
	unsigned char reply[16];
	int position = 0;
	int data_incr;
	int width;
	int height;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_file_count");

	jamcam_count = 0;

	memset( buf, 0, sizeof( buf ));

	switch( camera->port->type ) {
		default:
		case GP_PORT_SERIAL:
			strcpy( buf, "KB00" );
			buf[4] = ( position       ) & 0xff;
			buf[5] = ( position >>  8 ) & 0xff;
			buf[5] = ( position >> 16 ) & 0xff;
			buf[5] = ( position >> 24 ) & 0xff;
			jamcam_write_packet( camera, buf, 8 );

			jamcam_read_packet( camera, reply, 16 );

			while( reply[0] != 0xff ) {
				width  = (reply[5] * 256) + reply[4];
				height = (reply[7] * 256) + reply[6];

				data_incr = 0;
				data_incr += reply[8];
				data_incr += reply[9] * 256;
				data_incr += reply[10] * 256 * 256;
				data_incr += reply[11] * 256 * 256 * 256;

				jamcam_files[jamcam_count].position = position;
				jamcam_files[jamcam_count].width = width;
				jamcam_files[jamcam_count].height = height;
				jamcam_files[jamcam_count].data_incr = data_incr;

				jamcam_count++;

				position += data_incr;

				buf[4] = ( position       ) & 0xff;
				buf[5] = ( position >>  8 ) & 0xff;
				buf[6] = ( position >> 16 ) & 0xff;
				buf[7] = ( position >> 24 ) & 0xff;
				jamcam_write_packet( camera, buf, 8 );
			
				jamcam_read_packet( camera, reply, 16 );
			}
			break;

		case GP_PORT_USB:
			jamcam_set_usb_mem_pointer( camera, position );

			gp_port_read (camera->port, reply, 0x10 );

			width  = (reply[13] * 256) + reply[12];
			height = (reply[15] * 256) + reply[14];

			jamcam_set_usb_mem_pointer( camera, position + 8 );

			gp_port_read (camera->port, reply, 0x10 );

			while((unsigned char)reply[0] != 0xff ) {
				data_incr = 0;
				data_incr += reply[0];
				data_incr += reply[1] * 256;
				data_incr += reply[2] * 256 * 256;
				data_incr += reply[3] * 256 * 256 * 256;

				jamcam_files[jamcam_count].position = position;
				jamcam_files[jamcam_count].width = width;
				jamcam_files[jamcam_count].height = height;
				jamcam_files[jamcam_count].data_incr = data_incr;
				jamcam_count++;

				position += data_incr;

				jamcam_set_usb_mem_pointer( camera, position );

				gp_port_read (camera->port, reply, 0x10 );

				width  = (reply[13] * 256) + reply[12];
				height = (reply[15] * 256) + reply[14];

				jamcam_set_usb_mem_pointer( camera, position + 8 );

				gp_port_read (camera->port, reply, 0x10 );
			}
			break;
	}

	gp_debug_printf (GP_DEBUG_LOW, "jamcam",
		"*** returning jamcam_count = %d", jamcam_count);
	return( jamcam_count );
}

int jamcam_fetch_memory( Camera *camera, char *data, int start, int length ) {
	char packet[16];
	int end = start + length - 1;
	int bytes_read = 0;
	int bytes_to_read;
	int bytes_left = length;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_fetch_memory");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** start:  %d (0x%x)",
		start, start);
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** length: %d (0x%x)",
		length, length);

	switch( camera->port->type ) {
		default:
		case GP_PORT_SERIAL:
			memset( packet, 0, sizeof( packet ));
			strcpy( packet, "KB01" );

			/* start */
			packet[4] = ( start       ) & 0xff;
			packet[5] = ( start >>  8 ) & 0xff;
			packet[6] = ( start >> 16 ) & 0xff;
			packet[7] = ( start >> 24 ) & 0xff;

			/* end (inclusive) */
			packet[8]  = ( end       ) & 0xff;
			packet[9]  = ( end >>  8 ) & 0xff;
			packet[10] = ( end >> 16 ) & 0xff;
			packet[11] = ( end >> 24 ) & 0xff;

			jamcam_write_packet( camera, packet, 12 );

			break;

		case GP_PORT_USB:

			break;
	}

	while( bytes_left ) {
		switch( camera->port->type ) {
			default:
			case GP_PORT_SERIAL:
				bytes_to_read = bytes_left > SER_PKT_SIZE ? SER_PKT_SIZE : bytes_left;
				CHECK (jamcam_read_packet( camera, data + bytes_read, bytes_to_read ));
				break;
			case GP_PORT_USB:
				bytes_to_read = bytes_left > USB_PKT_SIZE ? USB_PKT_SIZE : bytes_left;
				jamcam_set_usb_mem_pointer( camera, start + bytes_read );
				gp_port_read (camera->port, data + bytes_read, bytes_to_read );
				break;
		}
				
		bytes_left -= bytes_to_read;
		bytes_read += bytes_to_read;
	}

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_fetch_memory OK");
	return( GP_OK );
}

int jamcam_request_image( Camera *camera, char *buf, int *len, int number ) {
	int position;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_request_image");

	*len = DATA_SIZE;

	position = jamcam_files[number].position + 0x10;
	*len = jamcam_files[number].width * jamcam_files[number].height;

	if ( camera->port->type == GP_PORT_USB ) {
		jamcam_set_usb_mem_pointer( camera, position );
		gp_port_read (camera->port, buf, 120 );

		position += 8;
	}

	return( jamcam_fetch_memory( camera, buf, position, *len ));
}

struct jamcam_file *jamcam_file_info(Camera *camera, int number)
{
	return( &jamcam_files[number] );
}

int jamcam_request_thumbnail( Camera *camera, char *buf, int *len, int number ) {
	char line[600];
	char packet[16];
	int position;
	int x, y;
	char *ptr;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_request_thumbnail");

	memset( packet, 0, sizeof( packet ));

	position = jamcam_files[number].position + 0x10;

	*len = 4800;

	ptr = buf;

	/* fetch thumbnail lines and build the thumbnail */
	position += 10 * jamcam_files[number].width;
	for( y = 0 ; y < 60 ; y++ ) {
		jamcam_fetch_memory( camera, line, position,
			jamcam_files[number].width );

		if ( jamcam_files[number].width == 600 ) {
			for( x = 22; x < 578 ; x += 7 ) {
				*(ptr++) = line[x];
			}
			position += 7 * 600;
		} else {
			for( x = 0; x < 320 ; ) {
				*(ptr++) = line[x];
				x += 3;
				*(ptr++) = line[x];
				x += 5;
			}

			if ( y % 2 ) {
				position += 5 * 320;
			} else {
				position += 3 * 320;
			}
		}
	}

	return( GP_OK );
}

int jamcam_write_packet (Camera *camera, char *packet, int length) {
	int ret, r;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_write_packet");

	for (r = 0; r < RETRIES; r++) {
		ret = gp_port_write (camera->port, packet, length);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;

		return (ret);
	}

	return (GP_ERROR_IO_TIMEOUT);
}


int jamcam_read_packet (Camera *camera, char *packet, int length) {
	int r = 0;
	int bytes_read;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_read_packet");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** length: %d (0x%x)",
		length, length);

	for (r = 0; r < RETRIES; r++) {
		bytes_read = gp_port_read (camera->port, packet, length);
		if (bytes_read == GP_ERROR_IO_TIMEOUT)
			continue;
		if (bytes_read < 0)
			return (bytes_read);

		if ( bytes_read == length ) {
			return( GP_OK );
		}
	}

	return (GP_ERROR_IO_TIMEOUT);
}


int jamcam_enq (Camera *camera)
{
	int ret, r = 0;
	unsigned char buf[16];

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_enq");

	switch( camera->port->type ) {
		default:
		case GP_PORT_SERIAL:
			strcpy((char *)buf, "KB99" );

			for (r = 0; r < RETRIES; r++) {

				ret = jamcam_write_packet (camera, (char *)buf, 4);
				if (ret == GP_ERROR_IO_TIMEOUT)
					continue;
				if (ret != GP_OK)
					return (ret);

				ret = jamcam_read_packet (camera, (char *)buf, 4);
				if (ret == GP_ERROR_IO_TIMEOUT)
					continue;
				if (ret != GP_OK)
					return (ret);

				if ( !strncmp( (char *)buf, "KIDB", 4 ))
					return (GP_OK);
				else
					return (GP_ERROR_CORRUPTED_DATA);
			
			}
			break;

		case GP_PORT_USB:
			gp_port_usb_msg_write( camera->port,
				0xa5,
				0x0004,
				0x0000,
				NULL, 0 );
			jamcam_set_usb_mem_pointer( camera, 0x0000 );

			gp_port_read( camera->port, (char *)buf, 0x0c );

			if ( !strncmp( (char *)buf, "KB00", 4 )) {
				return (GP_OK);
			} else if (( buf[0] == 0xf0 ) &&
					 ( buf[1] == 0xfd ) &&
					 ( buf[2] == 0x03 )) {
				return( GP_OK );
			} else {
				return (GP_ERROR_CORRUPTED_DATA);
			}

			break;
	}

	return (GP_ERROR_IO_TIMEOUT);
}

int jamcam_who_knows (Camera *camera)
{
	int ret, r = 0;
	char buf[16];

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_who_knows");

	/* usb port doesn't need this packet */
	if ( camera->port->type == GP_PORT_USB ) {
		return( GP_OK );
	}

	strcpy( buf, "KB04" );

	for (r = 0; r < RETRIES; r++) {

		ret = jamcam_write_packet (camera, buf, 4);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return (ret);

		ret = jamcam_read_packet (camera, buf, 4);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return (ret);

		return (GP_OK);
	}
	return (GP_ERROR_IO_TIMEOUT);
}


#if 0

/***********************************************************************/
/* gamma table correction and bayer routines, hopefully will go into a */
/* common Gphoto2 area at some point.                                  */
/***********************************************************************/


int gp_create_gamma_table( unsigned char *table, double g ) {
	int x;

	for( x = 0; x < 256 ; x++ ) {
		table[x] = ( 255 * pow((double)x/255, g ));
	}

	return( GP_OK );
}

int gp_gamma_correct_triple( unsigned char *data, int pixels,
	unsigned char *red, unsigned char *green, unsigned char *blue ) {
	int x;

	for( x = 0; x < ( pixels * 3 ); x += 3 ) {
		data[x+0] = red[data[x+0]];
		data[x+1] = green[data[x+1]];
		data[x+2] = blue[data[x+2]];
	}

	return( GP_OK );
}


int gp_gamma_correct_single( unsigned char *data, int pixels,
	unsigned char *gtable ) {

	return( gp_gamma_correct_triple( data, pixels, gtable, gtable, gtable ));
}

static int tile_colors[4][4] =
	{{ 0, 1, 1, 2},   {1, 0, 2, 1},   {2, 1, 1, 0},   {1, 2, 0, 1}};

#define RED 0
#define GREEN 1
#define BLUE 2

static int gp_bayer_expand(int w, int h, unsigned char *raw,
	unsigned char *output, int tile)
{
	int x, y, i;
	int colour, bayer;
	char *ptr = raw;

	for(y = 0; y < h; ++y) {
		for(x = 0; x < w; ++x) {
			bayer = (x&1?0:1) + (y&1?0:2);

			colour = tile_colors[tile][bayer];

			i = (y * w + x) * 3;

			output[i+RED]    = 0;
			output[i+GREEN]  = 0;
			output[i+BLUE]   = 0;
			output[i+colour] = *(ptr++);
		}
	}

	return( GP_OK );
}


#define AD(x, y, w) ((y)*(w)*3+3*(x))


static int gp_bayer_interpolate(int w, int h, unsigned char *image, int tile)
{
	int x, y, bayer;
	int p0, p1, p2, p3;
	int div, value;

	switch( tile ) {
		default:
		case BAYER_TILE_RGGB: p0 = 0; p1 = 1; p2 = 2; p3 = 3; break;
		case BAYER_TILE_GRBG: p0 = 1; p1 = 0; p2 = 3; p3 = 2; break;
		case BAYER_TILE_BGGR: p0 = 3; p1 = 2; p2 = 1; p3 = 0; break;
		case BAYER_TILE_GBRG: p0 = 2; p1 = 3; p2 = 0; p3 = 1; break;
	}

	for(y = 0; y < h; y++) {
		for(x = 0; x < w; x++) {
			// work out pixel type
			bayer = (x&1?0:1) + (y&1?0:2);

			if ( bayer == p0 ) {
				/* red. green lrtb, blue diagonals */
				div = value = 0;
				if ( y )          { value += image[AD(x,y-1,w)+GREEN]; div++; }
				if ( y < (h - 1)) { value += image[AD(x,y+1,w)+GREEN]; div++; }
				if ( x )          { value += image[AD(x-1,y,w)+GREEN]; div++; }
				if ( x < (w - 1)) { value += image[AD(x+1,y,w)+GREEN]; div++; }
				image[AD(x,y,w)+GREEN] = value / div;

				div = value = 0;
				if (( y < (h - 1)) && ( x < (w - 1)))
					{ value += image[AD(x+1,y+1,w)+BLUE]; div++; }
				if (( y ) && ( x ))
					{ value += image[AD(x-1,y-1,w)+BLUE]; div++; }
				if (( y < (h - 1)) && ( x ))
					{ value += image[AD(x-1,y+1,w)+BLUE]; div++; }
				if (( y ) && (x < (w - 1)))
					{ value += image[AD(x+1,y-1,w)+BLUE]; div++; }
				image[AD(x,y,w)+BLUE] = value / div;
			} else if ( bayer == p1 ) {
				/* green. red lr, blue tb */
				div = value = 0;
				if ( x < (w - 1)) { value += image[AD(x+1,y,w)+RED]; div++; }
				if ( x )          { value += image[AD(x-1,y,w)+RED]; div++; }
				image[AD(x,y,w)+RED] = value / div;

				div = value = 0;
				if ( y < (h - 1)) { value += image[AD(x,y+1,w)+BLUE]; div++; }
				if ( y )          { value += image[AD(x,y-1,w)+BLUE]; div++; }
				image[AD(x,y,w)+BLUE] = value / div;
			} else if ( bayer == p2 ) {
				/* green. blue lr, red tb */
				div = value = 0;
				if ( x < (w - 1)) { value += image[AD(x+1,y,w)+BLUE]; div++; }
				if ( x )          { value += image[AD(x-1,y,w)+BLUE]; div++; }
				image[AD(x,y,w)+BLUE] = value / div;

				div = value = 0;
				if ( y < (h - 1)) { value += image[AD(x,y+1,w)+RED]; div++; }
				if ( y )          { value += image[AD(x,y-1,w)+RED]; div++; }
				image[AD(x,y,w)+RED] = value / div;
			} else {
				/* blue. green lrtb, red diagonals */
				div = value = 0;
				if ( y )          { value += image[AD(x,y-1,w)+GREEN]; div++; }
				if ( y < (h - 1)) { value += image[AD(x,y+1,w)+GREEN]; div++; }
				if ( x )          { value += image[AD(x-1,y,w)+GREEN]; div++; }
				if ( x < (w - 1)) { value += image[AD(x+1,y,w)+GREEN]; div++; }
				image[AD(x,y,w)+GREEN] = value / div;

				div = value = 0;
				if (( y < (h - 1)) && ( x < (w - 1)))
					{ value += image[AD(x+1,y+1,w)+RED]; div++; }
				if (( y ) && ( x ))
					{ value += image[AD(x-1,y-1,w)+RED]; div++; }
				if (( y < (h - 1)) && ( x ))
					{ value += image[AD(x-1,y+1,w)+RED]; div++; }
				if (( y ) && (x < (w - 1)))
					{ value += image[AD(x+1,y-1,w)+RED]; div++; }
				image[AD(x,y,w)+RED] = value / div;
			}
		}
	}

	return( GP_OK );
}

int gp_bayer_decode(int w, int h, unsigned char *input, unsigned char *output,
	int tile) {

	gp_bayer_expand( w, h, input, output, tile );
	gp_bayer_interpolate( w, h, output, tile );

	return( GP_OK );
}

#endif
