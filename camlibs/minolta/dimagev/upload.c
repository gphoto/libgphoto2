/**********************************************************************
*       Minolta Dimage V digital camera communication library         *
*               Copyright (C) 2000,2001 Gus Hartmann                  *
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

int dimagev_put_file(dimagev_t* dimagev, CameraFile *file) {
	unsigned char total_packets=0, sent_packets=0;
	int left_to_send=0;

	dimagev_packet *p, *r;
	unsigned char char_buffer, command_buffer[3], *packet_buffer;

	if ( dimagev == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::null camera device");
		return GP_ERROR_BAD_PARAMETERS;
	}

	if ( dimagev->data->host_mode != 1 ) {

		dimagev->data->host_mode = 1;

		if ( dimagev_send_data(dimagev) < GP_OK ) {
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::unable to set host mode");
			return GP_ERROR_IO;
		}
	}

	/* First make the command packet. */
	command_buffer[0] = 0x0e;
	if ( ( p = dimagev_make_packet(command_buffer, 1, 0) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::unable to allocate command packet");
		return GP_ERROR_NO_MEMORY;
	}

	if ( gp_port_write(dimagev->dev, p->buffer, p->length) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::unable to send command packet");
		return GP_ERROR_IO;
	} else if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::no response from camera");
		return GP_ERROR_IO;
	}
		
	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::camera did not acknowledge transmission");
			return GP_ERROR_IO;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::camera cancels transmission");
			return GP_ERROR_IO;
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
			break;
	}

	/* Now start chopping up the picture and sending it over. */

	total_packets = ( file->size / 993 ) +1;

	/* The first packet is a special case. */
	if ( ( packet_buffer = malloc(993)) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::unable to allocate packet buffer");
		return GP_ERROR_NO_MEMORY;
	}

	packet_buffer[0]= total_packets;
	memcpy(&(packet_buffer[1]), file->data, 992);

	if ( ( p = dimagev_make_packet(packet_buffer, 993, 0) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::unable to allocate command packet");
		return GP_ERROR_NO_MEMORY;
	}

	if ( gp_port_write(dimagev->dev, p->buffer, p->length) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::unable to send data packet");
		return GP_ERROR_IO;
	} else if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::no response from camera");
		return GP_ERROR_IO;
	}
		
	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::camera did not acknowledge transmission");
			return GP_ERROR_IO;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::camera cancels transmission");
			return GP_ERROR_IO;
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
			break;
	}

	left_to_send = ( file->size - 992 );

	for ( sent_packets = 1 ; sent_packets < total_packets ; sent_packets++ ) {
		if ( left_to_send > 993 ) {
			if ( ( p = dimagev_make_packet(&(file->data[(sent_packets * 993) - 1]), 993, sent_packets) ) == NULL ) {
				gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::unable to allocate data packet");
				return GP_ERROR_NO_MEMORY;
			}
			left_to_send-=993;
		} else {
			if ( ( p = dimagev_make_packet(&(file->data[((sent_packets * 993) - 1)]), left_to_send, sent_packets) ) == NULL ) {
				gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::unable to allocate data packet");
				return GP_ERROR_NO_MEMORY;
			}
		}

		if ( gp_port_write(dimagev->dev, p->buffer, p->length) < GP_OK ) {
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::unable to send data packet");
			return GP_ERROR_IO;
		} else if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::no response from camera");
			return GP_ERROR_IO;
		}
			
		free(p);

		switch ( char_buffer ) {
			case DIMAGEV_ACK:
				break;
			case DIMAGEV_NAK:
				gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::camera did not acknowledge transmission");
				return GP_ERROR_IO;
				break;
			case DIMAGEV_CAN:
				gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::camera cancels transmission");
				return GP_ERROR_IO;
				break;
			default:
				gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_put_file::camera responded with unknown value %x", char_buffer);
				return GP_ERROR_IO;
				break;
		}
	}




	return GP_OK;
}
