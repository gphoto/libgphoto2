/****************************************************************/
/* library.c  - Gphoto2 library for accessing the Panasonic     */
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

#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <time.h>
#include "library.h"
#include <unistd.h>

#define	COOL_SLEEP	10000

int packet_size = 500;

/* ??set mode?? */
int coolshot_sm( Camera *camera ) {
	char buf[16];

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_sm");

	memset( buf, 0, sizeof( buf ));

	buf[0] = COOLSHOT_PKT;
	buf[2] = 'S';
	buf[3] = 'M';
	buf[4] = 0x01;
	buf[15] = 0x02;

	coolshot_write_packet( camera, buf );

	/* read ACK */
	coolshot_read_packet( camera, buf );

	/* read data */
	coolshot_read_packet( camera, buf );

	/* send ACK */
	coolshot_ack( camera );

	packet_size = 128;

	return( GP_OK );
}

/* ?set baud? */
int coolshot_sb( Camera *camera, int speed ) {
	char buf[16];
	gp_port_settings settings;

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_sb");
	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "*** speed: %i", speed);

	memset( buf, 0, sizeof( buf ));

	buf[0] = COOLSHOT_PKT;
	buf[2] = 'S';
	buf[3] = 'B';
	buf[4] = 0x01;
	buf[15] = 0x02;

	gp_port_get_settings (camera->port, &settings);

	switch (speed) {
		case 9600:
			buf[4] = '1';
			settings.serial.speed = 9600;
			break;
		case -1:
		case 19200:
			buf[4] = '2';
			settings.serial.speed = 19200;
			break;
		case 28800:
			buf[4] = '3';
			settings.serial.speed = 28800;
			break;
		case 38400:
			buf[4] = '4';
			settings.serial.speed = 38400;
			break;
		case 57600:
			buf[4] = '5';
			settings.serial.speed = 57600;
			break;
		case 0:		/* Default speed */
		case 115200:
			buf[4] = '6';
			settings.serial.speed = 115200;
			break;
		default:
			return (GP_ERROR_IO_SERIAL_SPEED);
	}

	coolshot_enq( camera );

	/* set speed */
	coolshot_write_packet( camera, buf );

	/* read ack */
	coolshot_read_packet( camera, buf );

	/* read OK */
	coolshot_read_packet( camera, buf );

	/* ack the OK */
	coolshot_ack( camera );

	CHECK (gp_port_set_settings (camera->port, settings));

	GP_SYSTEM_SLEEP(10);
	return (GP_OK);
}

int coolshot_fs( Camera *camera, int number ) {
	char buf[16];

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_fs");

	memset( buf, 0, sizeof( buf ));

	buf[0] = COOLSHOT_PKT;
	buf[2] = 'F';
	buf[3] = 'S';
	buf[7] = number;
	buf[15] = 0x02;

	coolshot_enq( camera );

	coolshot_write_packet( camera, buf );

	/* read ACK */
	coolshot_read_packet( camera, buf );

	/* read data */
	coolshot_read_packet( camera, buf );

	/* send ACK */
	coolshot_ack( camera );
	return( GP_OK );
}

int coolshot_sp( Camera *camera ) {
	char buf[16];

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_sp");

	memset( buf, 0, sizeof( buf ));

	buf[0] = COOLSHOT_PKT;
	buf[2] = 'S';
	buf[3] = 'P';
	buf[4] = 0x02;
	buf[15] = 0x02;

	coolshot_enq( camera );

	coolshot_write_packet( camera, buf );

	/* read ACK */
	coolshot_read_packet( camera, buf );

	packet_size = 500;

	return( GP_OK );
}

int coolshot_file_count (Camera *camera) {
	char buf[16];
	int count = 0;

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_file_count");

	memset( buf, 0, sizeof( buf ));

	buf[0] = COOLSHOT_PKT;
	buf[2] = 'R';
	buf[3] = 'N';
	buf[5] = 0x01;
	buf[15] = 0x02;

	coolshot_enq( camera );

	/* request count */
	coolshot_write_packet( camera, buf );

	/* read ack */
	coolshot_read_packet( camera, buf );

	/* read data packet */
	coolshot_read_packet( camera, buf );

	count = buf[7];

	usleep( COOL_SLEEP );
	coolshot_ack( camera );

	return( count );
}

int coolshot_request_image( Camera *camera, char *buf, int *len, int number ) {
	char packet[16];

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_request_image");

	memset( packet, 0, sizeof( packet ));

	packet[0] = COOLSHOT_PKT;
	packet[2] = 'R';
	packet[3] = 'D';
	packet[7] = number;
	packet[15] = 0x02;

	/* fixme */
	coolshot_fs( camera, number );
	coolshot_sp( camera );

	coolshot_enq( camera );

	/* request image */
	coolshot_write_packet( camera, packet );

	/* read ack */
	coolshot_read_packet( camera, packet );

	/* read OK */
	coolshot_read_packet( camera, packet );

	/* read data */
	coolshot_download_image( camera, buf, len, 0 );

	return( GP_OK );
}

int coolshot_request_thumbnail( Camera *camera, char *buf, int *len, int number ) {
	char packet[16];

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_request_thumbnail");

	memset( packet, 0, sizeof( packet ));

	packet[0] = COOLSHOT_PKT;
	packet[2] = 'R';
	packet[3] = 'M';
	packet[7] = number;
	packet[15] = 0x02;

	/* fixme */
	coolshot_fs( camera, number );
	/*
	coolshot_sp( camera );
	*/

	coolshot_enq( camera );

	/* request image */
	coolshot_write_packet( camera, packet );

	/* read ack */
	coolshot_read_packet( camera, packet );

	/* read OK */
	coolshot_read_packet( camera, packet );

	/* read data */
	coolshot_download_image( camera, buf, len, 0 );

	return( GP_OK );
}

int coolshot_check_checksum( char *packet, int length ) {
	int checksum = 0;
	int p_csum = 0;
	int x;
	unsigned char *ptr;

	ptr = (unsigned char *)packet + 2;
	for( x = 2; x < (length - 4 ); x++ ) {
		checksum += *(ptr++);
		/* checksum += (unsigned char)packet[x]; */
	}
	checksum &= 0xffff; /* 16 bit checksum */

	p_csum = (unsigned char)packet[length - 4];
	p_csum = p_csum << 8;
	p_csum += (unsigned char)packet[length - 3];

	if ( checksum == p_csum ) {
		return( GP_OK );
	} else {
		return( GP_ERROR );
	}
}

int coolshot_download_image( Camera *camera, char *buf, int *len, int thumbnail ) {
	char packet[1024];
	int data_len;
	int bytes_read = 0;
	int last_good = 0;
	double percentage;

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_download_image");

	memset( packet, 0, sizeof( packet ));

	packet[2] = '0';
	packet[3] = '0';

	coolshot_ack( camera );

	coolshot_read_packet( camera, packet );

	data_len = (unsigned char)packet[6] * 256;
	data_len += (unsigned char)packet[7];

	/* fixme, get rid of hardcoded length */
	if ( coolshot_check_checksum( packet, 8 + packet_size + 4 ) == GP_OK ) {
		coolshot_ack( camera );
		last_good = 1;
	} else {
	/*
		coolshot_nak( camera );
	*/
		last_good = 0;
	}

	while( strncmp( packet + 2, "DT", 2 ) == 0 ) {
		/* process packet */
		if ( last_good ) {
			data_len = (unsigned char)packet[6] * 256;
			data_len += (unsigned char)packet[7];

			memcpy( buf + bytes_read, packet + 8, data_len );

			bytes_read += data_len;
		}

		if ( thumbnail ) {
			percentage = bytes_read > 1800 ? 1.0 : ( 1.0 * bytes_read ) / 1800;
		} else {
			percentage = bytes_read > 80000 ? 1.0 : ( 1.0 * bytes_read ) / 80000;
		}
		gp_camera_progress( camera, percentage );

		coolshot_read_packet( camera, packet );

		data_len = (unsigned char)packet[6] * 256;
		data_len += (unsigned char)packet[7];

		/* fixme, get rid of hardcoded length */
		if ( coolshot_check_checksum( packet, 8 + packet_size + 4 ) == GP_OK) {
			coolshot_ack( camera );
			last_good = 1;
		} else {
		/*
			coolshot_nak( camera );
		*/
			last_good = 0;
		}
	}

	coolshot_ack( camera );

	*len = bytes_read;

	return( GP_OK );
}

int coolshot_write_packet (Camera *camera, char *packet) {
	int x, ret, r, checksum=0, length;

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_write_packet");

	if ( packet[0] == COOLSHOT_PKT ) {
		/* fixme */
		length = 16;

		for ( x = 2 ; x < 12; x++ ) {
			checksum += (unsigned char)packet[x];
		}

		packet[length - 4] = (checksum >> 8 ) & 0xff;
		packet[length - 3] = checksum & 0xff;

	} else if (( packet[0] == COOLSHOT_ENQ ) ||
	           ( packet[0] == COOLSHOT_ACK ) ||
	           ( packet[0] == COOLSHOT_NAK )) {
		length = 1;
	} else {
		length = 0;
		/* fixme */
		return( -1 );
	}

	for (r = 0; r < RETRIES; r++) {
		ret = gp_port_write (camera->port, packet, length);
		if (ret == GP_ERROR_TIMEOUT)
			continue;

		return (ret);
	}

	return (GP_ERROR_TIMEOUT);
}


int coolshot_read_packet (Camera *camera, char *packet) {
	int r = 0, x = 0, ret, done, length=0;
	int blocksize, bytes_read;
	char buf[4096];

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_read_packet");

read_packet_again:
	buf[0] = 0;
	packet[0] = 0;

	if (r > 0)
		gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* reading again...");

	done = 0;

	blocksize = 1;

	for (r = 0; r < RETRIES; r++) {

		bytes_read = gp_port_read (camera->port, packet, blocksize);
		if (bytes_read == GP_ERROR_TIMEOUT)
			continue;
		if (bytes_read < 0)
			return (bytes_read);

		if ( packet[0] == COOLSHOT_ENQ ) {
			usleep( COOL_SLEEP );
			coolshot_ack( camera );
			coolshot_read_packet( camera, packet );
			return( GP_OK );
		}

		if (( packet[0] == COOLSHOT_ACK ) ||
			( packet[0] == COOLSHOT_DONE )) {
			return( GP_OK );
		}

		if ( packet[0] != COOLSHOT_PKT ) {
			return( GP_ERROR );
		}

		bytes_read = gp_port_read (camera->port, packet + 1, 3 );
		if (bytes_read == GP_ERROR_TIMEOUT)
			continue;
		if (bytes_read < 0)
			return (bytes_read);

		/* Determine the packet type */
		if (( strncmp( packet + 2, "OK", 2 ) == 0 ) ||
			( strncmp( packet + 2, "DE", 2 ) == 0 ) ||
			( strncmp( packet + 2, "SB", 2 ) == 0 )) {
			/* normal 16-byte packet, so read the other 12 bytes */
			ret = gp_port_read (camera->port, packet + 4, 12 );
			if (ret == GP_ERROR_TIMEOUT) {
				/* fixme */
				/*
				coolshot_nak (camera);
				*/
				goto read_packet_again;
			}

			if (ret < 0)
				return (ret);

			return( GP_OK );
		} else if ( strncmp( packet + 2, "DT", 2 ) == 0 ) {
			/* read in packet number and length */
			ret = gp_port_read (camera->port, packet + 4, 4 );
			/* fixme, error detection */

			length = (unsigned char)packet[6] * 256;
			length += (unsigned char)packet[7];

			if (( packet_size == 128 ) ||
				( length == 128 )) {
				length = 128;
			} else {
				length = 500;
			}
			length += 4;

			ret = gp_port_read (camera->port, packet + 8, length );
			/* fixme, error detection */

			x = 0;
			while(( ret == GP_ERROR_TIMEOUT ) &&
			      ( x < RETRIES )) {
				x++;
				ret = gp_port_read (camera->port, packet + 8, length );
			}

			return( GP_OK );
		}

		if (done) break;
	}

	return (GP_ERROR_TIMEOUT);
}


int coolshot_ack (Camera *camera)
{
	int ret, r = 0;
	char buf[16];

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_ack");

	buf[0] = COOLSHOT_ACK;

	for (r = 0; r < RETRIES; r++) {

		ret = coolshot_write_packet (camera, buf);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		if (ret == GP_OK)
			return (ret);
	}
	return (GP_ERROR_TIMEOUT);
}

int coolshot_nak (Camera *camera)
{
	int ret, r = 0;
	char buf[16];

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_nak");

	buf[0] = COOLSHOT_NAK;

	for (r = 0; r < RETRIES; r++) {

		ret = coolshot_write_packet (camera, buf);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		if (ret == GP_OK)
			return (ret);
	}
	return (GP_ERROR_TIMEOUT);
}

int coolshot_enq (Camera *camera)
{
	int ret, r = 0;
	char buf[16];

	gp_debug_printf (GP_DEBUG_LOW, "coolshot", "* coolshot_enq");

	buf[0] = COOLSHOT_ENQ;

	for (r = 0; r < RETRIES; r++) {

		ret = coolshot_write_packet (camera, buf);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return (ret);

		ret = coolshot_read_packet (camera, buf);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return (ret);

		if (buf[0] == COOLSHOT_ACK)
			return (GP_OK);
		else
			return (GP_ERROR_CORRUPTED_DATA);

	}
	return (GP_ERROR_TIMEOUT);
}


#define WIDTH 40
#define HEIGHT 30

#define Y_ADJ 25

#define RIND(x, y, w) (((w)*(HEIGHT))+((y/2)*(WIDTH/2))+(x/2))
#define BIND(x, y, w) (((w)*(HEIGHT))+((WIDTH/4)*(HEIGHT))+((y/2)*(WIDTH/2))+(x/2))

int coolshot_build_thumbnail (char *data, int *size)
{
	char thumbnail[32768];
	char *ptr;
	unsigned char *udata = (unsigned char *)data;
	char *src;
	int length;
	int x, y;
	int loop;
	int Y, U, V;


	ptr = thumbnail;

	src = data;
	x = y = 0;

	for( loop = 0; loop < *size; loop++ ) {
		if ( x == WIDTH ) {
			x = 0;
			y++;
		}

		if ( y < HEIGHT ) {
			/*
				from imagemagick
				Y =  0.299000*R+0.587000*G+0.114000*B
				Cb= -0.168736*R-0.331264*G+0.500000*B
				Cr=  0.500000*R-0.418688*G-0.081316*B

				R = Y            +1.402000*Cr
				G = Y-0.344136*Cb-0.714136*Cr
				B = Y+1.772000*Cb
			*/

			Y = *src + Y_ADJ;
			U = udata[RIND(x,y,WIDTH)] - 128;
			V = udata[BIND(x,y,WIDTH)] - 128;

			ptr[0] = Y + ( 1.402000 * V );
			ptr[1] = Y - ( 0.344136 * U ) - ( 0.714136 * V );
			ptr[2] = Y + ( 1.772000 * U );
			ptr += 3;

			x++;
			src++;
		}
	}

	/* copy the header */
	sprintf( data,
		"P6\n"
		"# CREATOR: gphoto2, panasonic coolshot library\n"
		"%d %d\n"
		"255\n", 80, 60 );

	length = strlen( data );

	/* copy the image doubling both height and width */
	ptr = data + length;
	for( y = 0; y < HEIGHT; y++ ) {
		src = thumbnail + (y * WIDTH * 3);
		for( x = 0; x < WIDTH ; x++, src += 3 ) {
			ptr[0] = src[0];   ptr[1] = src[1];   ptr[2] = src[2];   ptr += 3;
			ptr[0] = src[0];   ptr[1] = src[1];   ptr[2] = src[2];   ptr += 3;
		}
		src = thumbnail + (y * WIDTH * 3);
		for( x = 0; x < WIDTH ; x++, src += 3 ) {
			ptr[0] = src[0];   ptr[1] = src[1];   ptr[2] = src[2];   ptr += 3;
			ptr[0] = src[0];   ptr[1] = src[1];   ptr[2] = src[2];   ptr += 3;
		}
	}

	length += WIDTH * HEIGHT * 3 * 4;

	*size = length;

	return( GP_OK );
}


