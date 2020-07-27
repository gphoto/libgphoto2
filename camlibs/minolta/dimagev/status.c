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

/* This is the parent function, who calls most of the functions below.
   It returns GP_ERROR if it cannot get the camera data, and GP_OK otherwise.
   The subroutines will print out more detained information should they fail.
*/
int dimagev_get_camera_status(dimagev_t *dimagev) {
	dimagev_packet *p, *raw;
	unsigned char char_buffer;

	/* Check the device. */
	if ( dimagev->dev == NULL ) {
		GP_DEBUG( "dimagev_get_camera_status::device not valid");
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Let's say hello and get the current status. */
	if ( ( p = dimagev_make_packet(DIMAGEV_GET_STATUS, 1, 0)) == NULL ) {
		GP_DEBUG( "dimagev_get_camera_status::unable to allocate packet");
		return GP_ERROR_IO;
	}

	if ( gp_port_write(dimagev->dev, (char *)p->buffer, p->length) < GP_OK ) {
		GP_DEBUG( "dimagev_get_camera_status::unable to write packet");
		free(p);
		return GP_ERROR_IO;
	} else if ( gp_port_read(dimagev->dev, (char *)&char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_get_camera_status::no response from camera");
		free(p);
		return GP_ERROR_IO;
	}

	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			GP_DEBUG( "dimagev_get_camera_status::camera did not acknowledge transmission");
			return dimagev_get_camera_status(dimagev);
/*			return GP_ERROR_IO;*/
		case DIMAGEV_CAN:
			GP_DEBUG( "dimagev_get_camera_status::camera cancels transmission");
			return GP_ERROR_IO;
		default:
			GP_DEBUG( "dimagev_get_camera_status::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
	}

	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		GP_DEBUG( "dimagev_get_camera_status::unable to read packet");
		return GP_ERROR_IO;
	}

	char_buffer = DIMAGEV_EOT;
	if ( gp_port_write(dimagev->dev, (char *)&char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_get_camera_status::unable to send EOT");
		free(p);
		return GP_ERROR_IO;
	}

	if ( gp_port_read(dimagev->dev, (char *)&char_buffer, 1) < GP_OK ) {
		GP_DEBUG( "dimagev_get_camera_status::no response from camera");
		free(p);
		return GP_ERROR_IO;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			GP_DEBUG( "dimagev_get_camera_status::camera did not acknowledge transmission");
			free(p);
			return GP_ERROR_IO;
		case DIMAGEV_CAN:
			GP_DEBUG( "dimagev_get_camera_status::camera cancels transmission");
			free(p);
			return GP_ERROR_IO;
		default:
			GP_DEBUG( "dimagev_get_camera_status::camera responded with unknown value %x", char_buffer);
			free(p);
			return GP_ERROR_IO;
	}

	if ( ( raw = dimagev_strip_packet(p) ) == NULL ) {
		GP_DEBUG( "dimagev_get_camera_status::unable to strip data packet");
		free(p);
		return GP_ERROR;
	}

	free(p);

	if ( ( dimagev->status = dimagev_import_camera_status(raw->buffer) ) == NULL ) {
		GP_DEBUG( "dimagev_get_camera_status::unable to read camera status");
		free(raw);
		return GP_ERROR;
	}

	free(raw);
	return GP_OK;
}

/* This function converts a few chars of data, such as is given by the camera
   to indicate status, and massages it into a dimagev_status_t.
*/
dimagev_status_t *dimagev_import_camera_status(unsigned char *raw_data) {
	dimagev_status_t *processed_data;

	if ( raw_data == NULL ) {
		return NULL;
	}

	if ( ( processed_data = malloc(sizeof(dimagev_status_t)) ) == NULL ) {
		return NULL;
	}

	processed_data->battery_level = raw_data[0];
	processed_data->number_images = ( raw_data[1] * 256 ) + ( raw_data[2] );
	processed_data->minimum_images_can_take = ( raw_data[3] * 256 ) + ( raw_data[4] );
	processed_data->busy = ( raw_data[5] & 0x40 ) >> 6;
	processed_data->flash_charging = ( raw_data[5] & 0x10 ) >> 4;
	processed_data->lens_status = ( raw_data[5] & 0x0c ) >> 2 ;
	processed_data->card_status = ( raw_data[5] & 0x03 );
	processed_data->id_number = raw_data[6];

	return processed_data;
}

/* This is a fairly simple dump function, using the GPIO debug funtions. It
   just prints the current status to stderr in a reasonably readable format.
*/
void dimagev_dump_camera_status(dimagev_status_t *status) {

	if ( status == NULL ) {
		GP_DEBUG( "Unable to dump NULL status");
		return;
	}

	GP_DEBUG( "========= Begin Camera Status =========");
	GP_DEBUG( "Battery level: %d", status->battery_level);
	GP_DEBUG( "Number of images: %d", status->number_images);
	GP_DEBUG( "Minimum images remaining: %d", status->minimum_images_can_take);
	GP_DEBUG( "Busy: %s ( %d )", status->busy != (unsigned char) 0 ? "Busy" : "Not Busy", status->busy);
	GP_DEBUG( "Flash charging: %s ( %d )", status->flash_charging != (unsigned char) 0 ? "Charging" : "Ready", status->flash_charging);

	GP_DEBUG( "Lens status: ");
	switch ( status->lens_status ) {
		case 0:
			GP_DEBUG( "Normal ( 0 )");
			break;
		case 1: case 2:
			GP_DEBUG( "Lens direction does not match flash light ( %d )", status->lens_status );
			break;
		case 3:
			GP_DEBUG( "Lens is not attached ( 3 )");
			break;
		default:
			GP_DEBUG( "Bad value for lens status ( %d )", status->lens_status);
			break;
	}

	GP_DEBUG( "Card status: ");
	switch ( status->card_status ) {
		case 0:
			GP_DEBUG( "Normal ( 0 )");
			break;
		case 1:
			GP_DEBUG( "Full ( 1 )");
			break;
		case 2:
			GP_DEBUG( "Write-protected ( 2 )");
			break;
		case 3:
			GP_DEBUG( "Card not valid for this camera ( 3 )");
			break;
		default:
			GP_DEBUG( "Invalid value for card status ( %d )", status->card_status);
			break;
	}

	GP_DEBUG( "Card ID Data: %02x", status->id_number);
	GP_DEBUG( "========== End Camera Status ==========");

	return;
}
