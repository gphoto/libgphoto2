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

/* This is the parent function, who calls most of the functions below.
   It returns GP_ERROR if it cannot get the camera data, and GP_OK otherwise.
   The subroutines will print out more detained information should they fail.
*/
int dimagev_get_camera_status(dimagev_t *dimagev) {
	dimagev_packet *p, *raw;
	unsigned char char_buffer;

	/* Check the device. */
	if ( dimagev->dev == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::device not valid");
		return GP_ERROR;
	}

	/* Let's say hello and get the current status. */
	if ( ( p = dimagev_make_packet(DIMAGEV_GET_STATUS, 1, 0)) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::unable to allocate packet");
		return GP_ERROR;
	}

	if ( gpio_write(dimagev->dev, p->buffer, p->length) == GPIO_ERROR ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::unable to write packet");
		return GP_ERROR;
	} else if ( gpio_read(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::no response from camera");
		return GP_ERROR;
	}

	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::camera did not acknowledge transmission");
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::camera cancels transmission");
			return GP_ERROR;
			break;
		default:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::camera responded with unknown value %x", char_buffer);
			return GP_ERROR;
			break;
	}

	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::unable to read packet");
		return GP_ERROR;
	}

	char_buffer = DIMAGEV_EOT;
	if ( gpio_write(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::unable to send EOT");
		return GP_ERROR;
	}
		
	if ( gpio_read(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::no response from camera");
		return GP_ERROR;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::camera did not acknowledge transmission");
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::camera cancels transmission");
			return GP_ERROR;
			break;
		default:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::camera responded with unknown value %x", char_buffer);
			return GP_ERROR;
			break;
	}

	if ( ( raw = dimagev_strip_packet(p) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::unable to strip data packet");
		return GP_ERROR;
	}
	
	if ( ( dimagev->status = dimagev_import_camera_status(raw->buffer) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_status::unable to read camera status");
		return GP_ERROR;
	}

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
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "Unable to dump NULL status");
		return;
	}

	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "========= Begin Camera Status =========");
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Battery level: %d", status->battery_level);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Number of images: %d", status->number_images);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Minimum images remaining: %d", status->minimum_images_can_take);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Busy: %s ( %d )", status->busy ? "Busy" : "Not Busy", status->busy);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Flash charging: %s ( %d )", status->flash_charging ? "Charging" : "Ready", status->flash_charging);

	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Lens status: ");
	switch ( status->lens_status ) {
		case 0:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Normal ( 0 )");
			break;
		case 1: case 2:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Lens direction does not match flash light ( %d )", status->lens_status );
			break;
		case 3:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Lens is not attached ( 3 )");
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Bad value for lens status ( %d )", status->lens_status);
			break;
	}

	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Card status: ");
	switch ( status->card_status ) {
		case 0:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Normal ( 0 )");
			break;
		case 1:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Full ( 1 )");
			break;
		case 2:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Write-protected ( 2 )");
			break;
		case 3:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Card not valid for this camera ( 3 )");
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Invalid value for card status ( %d )\n", status->card_status);
			break;
	}

	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Card ID Data: %02x", status->id_number);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "========== End Camera Status ==========");

	return;
}
