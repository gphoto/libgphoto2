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
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::device not valid\n");
		}
		return GP_ERROR;
	}

	/* Make sure we're in record mode and in host mode. */
	dimagev->data->play_rec_mode = 1;
	dimagev->data->host_mode = 1;

	/* Make sure we can set the data into the camera. */
	if ( dimagev_send_data(dimagev) == GP_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::unable to set host mode or record mode");
		}
		return GP_ERROR;
	}

	/* Maybe reduce this later? */
	sleep(2);

	/* Let's say hello and get the current status. */
	if ( ( p = dimagev_make_packet(DIMAGEV_SHUTTER, 1, 0)) == NULL ) {
		if ( dimagev->debug != 0 ) {
			perror("dimagev_shutter::unable to allocate packet");
		}
		return GP_ERROR;
	}

	if ( gpio_write(dimagev->dev, p->buffer, p->length) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::unable to write packet");
		}
		return GP_ERROR;
	}

	sleep(1);
	
	if ( gpio_read(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::no response from camera");
		}
		return GP_ERROR;
	}

	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			/* Sleep for a few seconds while the image is written to flash. */
			sleep(5);
			break;
		case DIMAGEV_NAK:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::camera did not acknowledge transmission\n");
			}
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::camera cancels transmission\n");
			}
			return GP_ERROR;
			break;
		default:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::camera responded with unknown value %x\n", char_buffer);
			}
			return GP_ERROR;
			break;
	}

	sleep(1);

	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::unable to read packet\n");
		}
		return GP_ERROR;
	}

	if ( dimagev->debug != 0 ) {
		dimagev_dump_packet(p);
	}

	if ( ( raw = dimagev_strip_packet(p) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::unable to strip data packet");
		}
		return GP_ERROR;
	}

	free(p);

	if ( raw->buffer[0] != 0 ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::camera returned error code");
		}
		return GP_ERROR;
	}

	sleep(1);

	char_buffer = DIMAGEV_EOT;
	if ( gpio_write(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			perror("dimagev_shutter::unable to send EOT");
		}
		return GP_ERROR;
	}
		
	if ( gpio_read(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			perror("dimagev_shutter::no response from camera");
		}
		return GP_ERROR;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::camera did not acknowledge transmission\n");
			}
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::camera cancels transmission\n");
			}
			return GP_ERROR;
			break;
		default:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::camera responded with unknown value %x", char_buffer);
			}
			return GP_ERROR;
			break;
	}

	dimagev->data->play_rec_mode = 0;
	dimagev->data->host_mode = 0;

	if ( dimagev_send_data(dimagev) == GP_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_shutter::unable to set host mode or record mode");
		}
		return GP_ERROR;
	}

	return GP_OK;
}
