/**********************************************************************
*       Minolta Dimage V digital camera communication library         *
*               Copyright (C) 2000 Gus Hartmann                       *
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
*    License along with this program; if not, write to the Free       *
*    Software Foundation, Inc., 59 Temple Place, Suite 330,           *
*    Boston, MA 02111-1307 USA                                        *
*                                                                     *
**********************************************************************/

/* $Id$ */

#include "dimagev.h"

int dimagev_delete_picture(dimagev_t *dimagev, int file_number) {
	dimagev_packet *p, *raw;
	unsigned char char_buffer, command_buffer[3];

	if ( dimagev == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::unable to use NULL dimagev_t");
		return GP_ERROR;
	}

	dimagev_dump_camera_status(dimagev->status);
	/* An image can only be deleted if the card is normal or full. */
	if ( dimagev->status->card_status > 1 ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::memory card does not permit deletion");
		return GP_ERROR;
	}

	if ( dimagev->data->host_mode != 1 ) {

		dimagev->data->host_mode = 1;

		if ( dimagev_send_data(dimagev) == GP_ERROR ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to set host mode");
			return GP_ERROR;
		}
	}


	/* First make the command packet. */
	command_buffer[0] = 0x05;
	command_buffer[1] = (unsigned char)( file_number / 256 );
	command_buffer[2] = (unsigned char)( file_number % 256 );
	if ( ( p = dimagev_make_packet(command_buffer, 3, 0) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::unable to allocate command packet");
		return GP_ERROR;
	}

	if ( gp_port_write(dimagev->dev, p->buffer, p->length) == GP_ERROR ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::unable to send set_data packet");
		return GP_ERROR;
	} else if ( gp_port_read(dimagev->dev, &char_buffer, 1) == GP_ERROR ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::no response from camera");
		return GP_ERROR;
	}
		
	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera did not acknowledge transmission\n");
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera cancels transmission\n");
			return GP_ERROR;
			break;
		default:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera responded with unknown value %x\n", char_buffer);
			return GP_ERROR;
			break;
	}

	
	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::unable to read packet");
		return GP_ERROR;
	}

	if ( ( raw = dimagev_strip_packet(p) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::unable to strip packet");
		return GP_ERROR;
	}
	
	free(p);

	if ( raw->buffer[0] != 0 ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::delete returned error code");
		return GP_ERROR;
	}

	char_buffer=DIMAGEV_EOT;
	if ( gp_port_write(dimagev->dev, &char_buffer, 1) == GP_ERROR ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to send ACK");
		return GP_ERROR;
	}

	if ( gp_port_read(dimagev->dev, &char_buffer, 1) == GP_ERROR ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::no response from camera");
		return GP_ERROR;
	}
		
	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera did not acknowledge transmission\n");
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera cancels transmission\n");
			return GP_ERROR;
			break;
		default:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera responded with unknown value %x\n", char_buffer);
			return GP_ERROR;
			break;
	}

	return GP_OK;
}

int dimagev_delete_all(dimagev_t *dimagev) {
	return GP_OK;
}

