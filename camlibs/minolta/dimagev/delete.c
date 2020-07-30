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

#include "dimagev.h"

#define GP_MODULE "dimagev"

int dimagev_delete_picture(dimagev_t *dimagev, int file_number) {
	dimagev_packet *p, *raw;
	unsigned char command_buffer[3];
	char char_buffer = 0;

	if ( dimagev == NULL ) {
		GP_DEBUG( "dimagev_delete_picture::unable to use NULL dimagev_t");
		return GP_ERROR_BAD_PARAMETERS;
	}

	dimagev_dump_camera_status(dimagev->status);
	/* An image can only be deleted if the card is normal or full. */
	if ( dimagev->status->card_status > (unsigned char) 1 ) {
		GP_DEBUG( "dimagev_delete_picture::memory card does not permit deletion");
		return GP_ERROR;
	}

	if ( dimagev->data->host_mode != (unsigned char) 1 ) {

		dimagev->data->host_mode = (unsigned char) 1;

		if ( dimagev_send_data(dimagev) < GP_OK ) {
			GP_DEBUG( "dimagev_delete_picture::unable to set host mode");
			return GP_ERROR_IO;
		}
	}


	/* First make the command packet. */
	command_buffer[0] = 0x05;
	command_buffer[1] = (unsigned char)( file_number / 256 );
	command_buffer[2] = (unsigned char)( file_number % 256 );
	if ( ( p = dimagev_make_packet(command_buffer, 3, 0) ) == NULL ) {
		GP_DEBUG( "dimagev_delete_picture::unable to allocate command packet");
		return GP_ERROR_NO_MEMORY;
	}

	if ( gp_port_write(dimagev->dev, (char *)p->buffer, p->length) < GP_OK ) {
		GP_DEBUG( "dimagev_delete_picture::unable to send set_data packet");
		free(p);
		return GP_ERROR_IO;
	} else if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_delete_picture::no response from camera");
		free(p);
		return GP_ERROR_IO;
	}

	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			GP_DEBUG( "dimagev_delete_picture::camera did not acknowledge transmission");
			return GP_ERROR_IO;
		case DIMAGEV_CAN:
			GP_DEBUG( "dimagev_delete_picture::camera cancels transmission");
			return GP_ERROR_IO;
		default:
			GP_DEBUG( "dimagev_delete_picture::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
	}


	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		GP_DEBUG( "dimagev_delete_picture::unable to read packet");
		return GP_ERROR_IO;
	}

	if ( ( raw = dimagev_strip_packet(p) ) == NULL ) {
		GP_DEBUG( "dimagev_delete_picture::unable to strip packet");
		free(p);
		return GP_ERROR;
	}

	free(p);

	if ( raw->buffer[0] != (unsigned char) 0 ) {
		GP_DEBUG( "dimagev_delete_picture::delete returned error code");
		free(raw);
		return GP_ERROR_NO_MEMORY;
	}
	free(raw);

	char_buffer=DIMAGEV_EOT;
	if ( gp_port_write(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_delete_picture::unable to send ACK");
		return GP_ERROR_IO;
	}

	if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_delete_picture::no response from camera");
		return GP_ERROR_IO;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			GP_DEBUG( "dimagev_delete_picture::camera did not acknowledge transmission");
			return GP_ERROR_IO;
		case DIMAGEV_CAN:
			GP_DEBUG( "dimagev_delete_picture::camera cancels transmission");
			return GP_ERROR_IO;
		default:
			GP_DEBUG( "dimagev_delete_picture::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
	}

	return GP_OK;
}

int dimagev_delete_all(dimagev_t *dimagev) {
	dimagev_packet *p, *raw;
	unsigned char command_buffer[3];
	char char_buffer;

	if ( dimagev == NULL ) {
		GP_DEBUG( "dimagev_delete_all::unable to use NULL dimagev_t");
		return GP_ERROR_BAD_PARAMETERS;
	}

	dimagev_dump_camera_status(dimagev->status);
	/* An image can only be deleted if the card is normal or full. */
	if ( dimagev->status->card_status > (unsigned char) 1 ) {
		GP_DEBUG( "dimagev_delete_all::memory card does not permit deletion");
		return GP_ERROR;
	}

	if ( dimagev->data->host_mode != (unsigned char) 1 ) {

		dimagev->data->host_mode = (unsigned char) 1;

		if ( dimagev_send_data(dimagev) < GP_OK ) {
			GP_DEBUG( "dimagev_delete_all::unable to set host mode");
			return GP_ERROR_IO;
		}
	}


	/* First make the command packet. */
	command_buffer[0] = 0x06;
	if ( ( p = dimagev_make_packet(command_buffer, 1, 0) ) == NULL ) {
		GP_DEBUG( "dimagev_delete_all::unable to allocate command packet");
		return GP_ERROR_IO;
	}

	if ( gp_port_write(dimagev->dev, (char *)p->buffer, p->length) < GP_OK ) {
		GP_DEBUG( "dimagev_delete_all::unable to send set_data packet");
		free(p);
		return GP_ERROR_IO;
	} else if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_delete_all::no response from camera");
		free(p);
		return GP_ERROR_IO;
	}

	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			GP_DEBUG( "dimagev_delete_all::camera did not acknowledge transmission");
			return GP_ERROR_IO;
		case DIMAGEV_CAN:
			GP_DEBUG( "dimagev_delete_all::camera cancels transmission");
			return GP_ERROR_IO;
		default:
			GP_DEBUG( "dimagev_delete_all::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
	}


	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		GP_DEBUG( "dimagev_delete_all::unable to read packet");
		return GP_ERROR_IO;
	}

	if ( ( raw = dimagev_strip_packet(p) ) == NULL ) {
		GP_DEBUG( "dimagev_delete_all::unable to strip packet");
		free(p);
		return GP_ERROR_NO_MEMORY;
	}

	free(p);

	if ( raw->buffer[0] != (unsigned char) 0 ) {
		GP_DEBUG( "dimagev_delete_all::delete returned error code");
		free(raw);
		return GP_ERROR;
	}
	free(raw);

	char_buffer=DIMAGEV_EOT;
	if ( gp_port_write(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_delete_all::unable to send ACK");
		return GP_ERROR_IO;
	}

	if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_delete_all::no response from camera");
		return GP_ERROR_IO;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			GP_DEBUG( "dimagev_delete_all::camera did not acknowledge transmission");
			return GP_ERROR_IO;
		case DIMAGEV_CAN:
			GP_DEBUG( "dimagev_delete_all::camera cancels transmission");
			return GP_ERROR_IO;
		default:
			GP_DEBUG( "dimagev_delete_all::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
	}

	return GP_OK;
}
