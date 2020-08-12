/**********************************************************************
*       Minolta Dimage V digital camera communication library         *
*               Copyright 2000,2001 Gus Hartmann                      *
*                                                                     *
*    This program is free software; you can redistribute it and/or    *
*    modify it under the terms of the GNU General Public License as   *
*    published by the Free Software Foundation; either version 2 of   *
*    the License, or (at your option) any later version.              *
*                                                                     *
*    This program is distributed in the hope that it will be useful,  *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of   *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
*    GNU General Public License for more details.                     *
*                                                                     *
*    You should have received a copy of the GNU General Public        *
*    License along with this program; if not, write to the *
*    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
*    Boston, MA  02110-1301  USA
*                                                                     *
**********************************************************************/

/* $Id$ */

#include "config.h"

#include <stdio.h>

#include "dimagev.h"

#define GP_MODULE "dimagev"

/* This function takes an array of unsigned chars, as well as the length, and
   creates a dimagev_packet ready for sending to the camera. Packets must be
   of the form below:

                                    bytes
     --------------------------------------------------------------------
	 |  0  |  1  |  2  |  3  |   4 through ( n-4)     | n-3 | n-2 | n-1 |
     --------------------------------------------------------------------
     | STX | Seq |  Length   |      Payload           | Checksum  | ETX |
     --------------------------------------------------------------------

	 STX and ETX are defined in dimage.h; their values are 0x02 and 0x03.
	 Seq is the sequence number; start at zero.
	 Payload is the good stuff.
	 Checksum is the sum of all bytes 0 through ( n-4 ) mod 65536.

	 A packet must be at least eight bytes, with at least one byte of payload.
*/
dimagev_packet *dimagev_make_packet(const unsigned char *const buffer, unsigned int length, unsigned int seq) {
	int i=0;
	unsigned int checksum=0;
	dimagev_packet *p;

	if ( ( p = calloc(1, sizeof(dimagev_packet) ) ) == NULL ) {
		GP_DEBUG("dimagev_make_packet::unable to allocate packet");
		return NULL;
	}

	p->length = length + 7;

	p->buffer[0] = DIMAGEV_STX;
	p->buffer[1] = seq & 0x000000ff;
	p->buffer[2] = ( p->length & 0x0000ff00) >> 8;
	p->buffer[3] = p->length & 0x000000ff;

	memcpy(&(p->buffer[4]), buffer, (unsigned int) length);

	/* Now the footer. */
	for (i=0 ; i < p->length - 3 ; i++ )
	{
		checksum += (unsigned int) p->buffer[i];
	}
	p->buffer[(p->length - 3)] = (unsigned char) ((checksum & 0x0000ff00) >> 8 );
	p->buffer[(p->length - 2)] = (unsigned char) ( checksum & 0x000000ff );
	p->buffer[(p->length - 1)] = (unsigned char) DIMAGEV_ETX;

	return p;
}

/* dimagev_verify_packet(): return GP_OK if valid packet, GP_ERROR otherwise. */
int dimagev_verify_packet(dimagev_packet *p) {
	int i=0;
	unsigned short correct_checksum=0, current_checksum=0;

	/* All packets must start with DIMAGEV_STX and end with DIMAGEV_ETX. It's an easy check. */
	if ( ( p->buffer[0] != (unsigned char) DIMAGEV_STX ) || ( p->buffer[(p->length - 1)] != (unsigned char) DIMAGEV_ETX ) ) {
		GP_DEBUG( "dimagev_verify_packet::packet missing STX and/or ETX");
		return GP_ERROR_CORRUPTED_DATA;
	}

	correct_checksum = (p->buffer[(p->length - 3)] * 256) + p->buffer[(p->length - 2)];

	for ( i = 0 ; i < ( p->length - 3 ) ; i++ ) {
		current_checksum += p->buffer[i];
	}

	if ( current_checksum != correct_checksum ) {
		GP_DEBUG( "dimagev_verify_packet::checksum bytes were %02x%02x, checksum was %d, should be %d", p->buffer[( p->length - 3) ], p->buffer[ ( p->length -2 ) ], current_checksum, correct_checksum);
		return GP_ERROR_CORRUPTED_DATA;
	} else {
		return GP_OK;
	}
}

dimagev_packet *dimagev_read_packet(dimagev_t *dimagev) {
	dimagev_packet *p;
	unsigned char char_buffer;

	if ( ( p = malloc(sizeof(dimagev_packet)) ) == NULL ) {
		GP_DEBUG("dimagev_read_packet::unable to allocate packet");
		return NULL;
	}

	if ( gp_port_read(dimagev->dev, (char *)p->buffer, 4) < GP_OK ) {
		GP_DEBUG( "dimagev_read_packet::unable to read packet header - will try to send NAK");
		free(p);

		/* Send a NAK */
		char_buffer = DIMAGEV_NAK;
		if ( gp_port_write(dimagev->dev, (char *)&char_buffer, 1) < GP_OK ) {
			GP_DEBUG( "dimagev_read_packet::unable to send NAK");
			return NULL;
		}

		/* Who likes recursion? */
		return dimagev_read_packet(dimagev);

	}

	p->length = ( p->buffer[2] * 256 ) + ( p->buffer[3] );

	if ( gp_port_read(dimagev->dev, (char *)&(p->buffer[4]), ( p->length - 4)) < GP_OK ) {
		GP_DEBUG( "dimagev_read_packet::unable to read packet body - will try to send NAK");
		free(p);

		/* Send a NAK */
		char_buffer = DIMAGEV_NAK;
		if ( gp_port_write(dimagev->dev, (char *)&char_buffer, 1) < GP_OK ) {
			GP_DEBUG( "dimagev_read_packet::unable to send NAK");
			return NULL;
		}

		/* Who likes recursion? */
		return dimagev_read_packet(dimagev);

	}

	/* Now we *should* have a packet. Let's do a sanity check. */
	if ( dimagev_verify_packet(p) < GP_OK ) {
		GP_DEBUG( "dimagev_read_packet::got an invalid packet - will try to send NAK");
		free(p);

		/* Send a NAK */
		char_buffer = DIMAGEV_NAK;
		if ( gp_port_write(dimagev->dev, (char *)&char_buffer, 1) < GP_OK ) {
			GP_DEBUG( "dimagev_read_packet::unable to send NAK");
			return NULL;
		}

		/* Who likes recursion? */
		return dimagev_read_packet(dimagev);

	}

	return p;
}

void dimagev_dump_packet(dimagev_packet *p) {
	int i=0;

	printf("Packet length is %d\n", p->length);

	for ( i = 0 ; i < p->length ; i++ ) {
		printf("%02x ",(unsigned int) p->buffer[i]);
	}
	printf("\n");
	return;
}

dimagev_packet *dimagev_strip_packet(dimagev_packet *p) {
	dimagev_packet *stripped;

	/* All camera packets must start with DIMAGEV_STX and end with DIMAGEV_ETX. */
	/* Packets used as strings shouldn't have these. It's an easy check. */
	if ( ( p->buffer[0] != (unsigned char) DIMAGEV_STX ) || ( p->buffer[(p->length - 1)] != (unsigned char) DIMAGEV_ETX ) ) {
		return NULL;
	}

	if ( ( stripped = malloc(sizeof(dimagev_packet)) ) == NULL ) {
		GP_DEBUG("dimagev_strip_packet::unable to allocate destination packet");
		return NULL;
	}

	stripped->length = ( p->length - 7 );

	memcpy(stripped->buffer, &(p->buffer[4]), stripped->length);

	return stripped;
}

unsigned char dimagev_decimal_to_bcd(unsigned char decimal) {
	unsigned char bcd = (unsigned char) 0;
	int tensdigit = 0;

	if ( decimal > (unsigned char) 99 ) {
		/* No good way to handle this. */
		return (unsigned char) 0;
	} else {
		tensdigit = decimal / 10;
		bcd = tensdigit * 16;
		bcd += decimal % 10;
		return bcd;
	}
}

unsigned char dimagev_bcd_to_decimal(unsigned char bcd) {
	if ( bcd > 99 ) {
		/* The highest value that we can handle in BCD */
		return (unsigned char) 99;
	} else {
		return ((bcd/16)*10 + (bcd%16));
	}
}

int dimagev_packet_sequence(dimagev_packet *p) {
	return (int) p->buffer[1];
}
