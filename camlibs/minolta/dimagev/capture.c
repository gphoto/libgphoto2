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

/* This function handles sending the camera command to take a new picture, and
   the Dimage V automatically stores it in the lowest number available, which
   is the current number of images plus one. Since gphoto2 assumes that images
   can be captured without being saved on the camera first, camera_capture()
   is actually made up of a few steps, the primary ones being dimagev_shutter(),
   dimagev_get_picture(), and dimagev_delete_picture().
*/
int dimagev_shutter(dimagev_t *dimagev) {
	dimagev_packet *p, *raw;
	unsigned char char_buffer;

	/* Check the device. */
	if ( dimagev->dev == NULL ) {
		GP_DEBUG( "dimagev_shutter::device not valid");
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Verify that we can write to the memory card. */
	if ( dimagev->status->card_status != (unsigned char) 0 ) {
		GP_DEBUG( "dimagev_shutter::unable to write to memory card - check status");
		return GP_ERROR;
	}

	/* Make sure we're in record mode and in host mode. */
	dimagev->data->play_rec_mode = (unsigned char) 1;
	dimagev->data->host_mode = (unsigned char) 1;

	/* Make sure we can set the data into the camera. */
	if ( dimagev_send_data(dimagev) < GP_OK ) {
		GP_DEBUG( "dimagev_shutter::unable to set host mode or record mode");
		return GP_ERROR;
	}

	/* Maybe reduce this later? */
	if ( sleep(2) != 0 ) {
		GP_DEBUG( "dimagev_shutter::sleep() returned non-zero value");
	}

	/* Let's say hello and get the current status. */
	if ( ( p = dimagev_make_packet(DIMAGEV_SHUTTER, 1, 0)) == NULL ) {
		GP_DEBUG( "dimagev_shutter::unable to allocate packet");
		return GP_ERROR_NO_MEMORY;
	}

	if ( gp_port_write(dimagev->dev, (char *)p->buffer, p->length) < GP_OK ) {
		GP_DEBUG( "dimagev_shutter::unable to write packet");

		if ( p != NULL ) {
			free(p);
		}

		return GP_ERROR_IO;
	}

	if ( p != NULL ) {
		free(p);
	}


	if ( sleep(1) != 0 ) {
		GP_DEBUG( "dimagev_shutter::sleep() returned non-zero value");
	}

	if ( gp_port_read(dimagev->dev, (char *)&char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_shutter::no response from camera");
		return GP_ERROR_IO;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			/* Sleep for a few seconds while the image is written to flash. */
			if ( sleep(5) != 0 ) {
				GP_DEBUG( "dimagev_shutter::sleep() returned non-zero value");
			}
			break;
		case DIMAGEV_NAK:
			GP_DEBUG( "dimagev_shutter::camera did not acknowledge transmission");
			return GP_ERROR_IO;
		case DIMAGEV_CAN:
			GP_DEBUG( "dimagev_shutter::camera cancels transmission");
			return GP_ERROR_IO;
		default:
			GP_DEBUG( "dimagev_shutter::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
	}

	if ( sleep(1) != 0 ) {
		GP_DEBUG( "dimagev_shutter::sleep() returned non-zero value");
	}


	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		GP_DEBUG( "dimagev_shutter::unable to read packet");
		return GP_ERROR_IO;
	}

	if ( ( raw = dimagev_strip_packet(p) ) == NULL ) {
		GP_DEBUG( "dimagev_shutter::unable to strip data packet");
		free(p);
		return GP_ERROR;
	}

	free(p);

	if ( raw->buffer[0] != (unsigned char) 0 ) {
		GP_DEBUG( "dimagev_shutter::camera returned error code");
		free(raw);
		return GP_ERROR;
	}

	free(raw);

	if ( sleep(1) != 0 ) {
		GP_DEBUG( "dimagev_shutter::sleep() returned non-zero value");
	}

	char_buffer = DIMAGEV_EOT;
	if ( gp_port_write(dimagev->dev, (char *)&char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_shutter::unable to send EOT");
		return GP_ERROR_IO;
	}

	if ( gp_port_read(dimagev->dev, (char *)&char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_shutter::no response from camera");
		return GP_ERROR_IO;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			GP_DEBUG( "dimagev_shutter::camera did not acknowledge transmission");
			return GP_ERROR_IO;
		case DIMAGEV_CAN:
			GP_DEBUG( "dimagev_shutter::camera cancels transmission");
			return GP_ERROR_IO;
		default:
			GP_DEBUG( "dimagev_shutter::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
	}

	dimagev->data->play_rec_mode = (unsigned char) 0;

	if ( dimagev_send_data(dimagev) < GP_OK ) {
		GP_DEBUG( "dimagev_shutter::unable to set host mode or record mode - non-fatal!");
/*		return GP_ERROR;*/
	}

	return GP_OK;
}
