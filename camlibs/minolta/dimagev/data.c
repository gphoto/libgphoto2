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

/* This is the parent function, who calls most of the functions below.
   It returns GP_ERROR if it cannot get the camera data, and GP_OK otherwise.
   The subroutines will print out more detained information should they fail.
*/
int dimagev_get_camera_data(dimagev_t *dimagev) {
	dimagev_packet *p, *raw;
	unsigned char char_buffer;

	/* Check the device. */
	if ( dimagev->dev == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::device not valid");
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Let's say hello and get the current status. */
	if ( ( p = dimagev_make_packet(DIMAGEV_GET_DATA, 1, 0)) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::unable to allocate packet");
		return GP_ERROR_NO_MEMORY;
	}

	if ( gp_port_write(dimagev->dev, p->buffer, p->length) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::unable to write packet");
		return GP_ERROR_IO;
	} else if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::no response from camera");
		return GP_ERROR_IO;
	}

	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::camera did not acknowledge transmission");
			return GP_ERROR_IO;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::camera cancels transmission");
			return GP_ERROR_IO;
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
			break;
	}

	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::unable to read packet");
		return GP_ERROR_IO;
	}

	char_buffer = DIMAGEV_EOT;
	if ( gp_port_write(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::unable to send EOT");
		return GP_ERROR_IO;
	}
		
	if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::no response from camera");
		return GP_ERROR_IO;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::camera did not acknowledge transmission");
			return GP_ERROR_IO;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::camera cancels transmission");
			return GP_ERROR_IO;
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
			break;
	}

	if ( ( raw = dimagev_strip_packet(p) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::unable to strip data packet");
		return GP_ERROR;
	}
	
	if ( ( dimagev->data = dimagev_import_camera_data(raw->buffer)) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_get_camera_data::unable to read camera data");
		return GP_ERROR;
	}

	/* Sure it *should* get freed automagically, but why take the risk? */
	free(p);
	free(raw);

	return GP_OK;
}

/* This function sends the contents of a dimagev_data_t to the current camera.
   This allows many changes to be made (e.g. entering host mode and record
   mode) while only sending a single set_data command.
*/
int dimagev_send_data(dimagev_t *dimagev) {
	dimagev_packet *p;
	unsigned char *export_data, char_buffer;


	if ( dimagev == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::unable to use NULL dimagev_t");
		return GP_ERROR_BAD_PARAMETERS;
	}

	if ( ( export_data = dimagev_export_camera_data(dimagev->data) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::unable to export camera data");
		return GP_ERROR;
	}

	dimagev_dump_camera_data(dimagev->data);

	if ( ( p = dimagev_make_packet(DIMAGEV_SET_DATA, 1, 0) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::unable to create set_data packet");
		return GP_ERROR_NO_MEMORY;
	}

	if ( gp_port_write(dimagev->dev, p->buffer, p->length) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::unable to send set_data packet");
		return GP_ERROR_IO;
	} else if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::no response from camera");
		return GP_ERROR_IO;
	}
		
	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::camera did not acknowledge transmission");
			return GP_ERROR_IO;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::camera cancels transmission");
			return GP_ERROR_IO;
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
			break;
	}

	if ( ( p = dimagev_make_packet(export_data, 9, 1) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::unable to create set_data packet");
		return GP_ERROR_NO_MEMORY;
	}

	if ( gp_port_write(dimagev->dev, p->buffer, p->length) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::unable to send data packet");
		return GP_ERROR_IO;
	}
		
	if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::no response from camera");
		return GP_ERROR_IO;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::camera did not acknowledge transmission");
			return GP_ERROR_IO;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::camera cancels transmission");
			return GP_ERROR_IO;
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
			break;
	}


	char_buffer = DIMAGEV_EOT;
	if ( gp_port_write(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::unable to send EOT");
		return GP_ERROR_IO;
	}
		
	if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::no response from camera");
		return GP_ERROR_IO;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::camera did not acknowledge transmission");
			return GP_ERROR_IO;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::camera cancels transmission");
			return GP_ERROR_IO;
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_send_data::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
			break;
	}

	sleep(3);

	return GP_OK;
}

/* This function populates and retuens a dimagev_data_t with the values
   contained inside an array of bytes, as returned by the Dimage V. See
   the description of these fields in the appropriate header file for
   more information on the bit-packing done here.
*/
dimagev_data_t *dimagev_import_camera_data(unsigned char *raw_data) {
	dimagev_data_t *processed_data;

	if ( raw_data == NULL ) {
		return NULL;
	}

	if ( ( processed_data = malloc(sizeof(dimagev_data_t)) ) == NULL ) {
		return NULL;
	}

	/* They really pack those bit fields. */
	processed_data->host_mode = ( raw_data[0] & 0x80) >> 7;
	processed_data->exposure_valid = (raw_data[0] & 0x40 ) >> 6;
	processed_data->date_valid = (raw_data[0] & 0x20 ) >> 5;
	processed_data->self_timer_mode = (raw_data[0] & 0x10 ) >> 4;
	processed_data->flash_mode = ( raw_data[0] & 0x0C ) >> 2;
	processed_data->quality_setting = ( raw_data[0] & 0x02 ) >> 1;
	processed_data->play_rec_mode = (raw_data[0] & 0x01 );
	processed_data->year = dimagev_bcd_to_decimal(raw_data[1]);
	processed_data->month = dimagev_bcd_to_decimal(raw_data[2]);
	processed_data->day = dimagev_bcd_to_decimal(raw_data[3]);
	processed_data->hour = dimagev_bcd_to_decimal(raw_data[4]);
	processed_data->minute = dimagev_bcd_to_decimal(raw_data[5]);
	processed_data->second = dimagev_bcd_to_decimal(raw_data[6]);
	processed_data->exposure_correction = raw_data[7];
	processed_data->valid = ( raw_data[8] & 0x80) >> 7;
	processed_data->id_number = ( raw_data[8] & 0x7F);

	return processed_data;
}

unsigned char *dimagev_export_camera_data(dimagev_data_t *good_data) {
	unsigned char *export_data;

	if ( ( export_data = malloc(9) ) == NULL ) {
		perror("dimagev_export_camera_data::unable to allocate buffer");
		return NULL;
	}

	export_data[0] = 0;
	export_data[0] = export_data[0] | ( ( ( good_data->host_mode ) << 7 ) & 0x80 );
	export_data[0] = export_data[0] | ( ( ( good_data->exposure_valid ) << 6 ) & 0x40 );
	export_data[0] = export_data[0] | ( ( ( good_data->date_valid ) << 5 ) & 0x20 );
	export_data[0] = export_data[0] | ( ( ( good_data->self_timer_mode ) << 4 ) & 0x10 );
	export_data[0] = export_data[0] | ( ( ( good_data->flash_mode ) << 2 ) & 0x0C );
	export_data[0] = export_data[0] | ( ( ( good_data->quality_setting ) << 1 ) & 0x02 );
	export_data[0] = export_data[0] | ( ( ( good_data->play_rec_mode )) & 0x01 );
	export_data[1] = 0;
	export_data[1] = dimagev_decimal_to_bcd(good_data->year);
	export_data[2] = 0;
	export_data[2] = dimagev_decimal_to_bcd(good_data->month);
	export_data[3] = 0;
	export_data[3] = dimagev_decimal_to_bcd(good_data->day);
	export_data[4] = 0;
	export_data[4] = dimagev_decimal_to_bcd(good_data->hour);
	export_data[5] = 0;
	export_data[5] = dimagev_decimal_to_bcd(good_data->minute);
	export_data[6] = 0;
	export_data[6] = dimagev_decimal_to_bcd(good_data->second);
	export_data[7] = 0;
	export_data[7] = good_data->exposure_correction;
	export_data[8] = 0;
	export_data[8] = export_data[8] & ( ( ( good_data->valid ) << 7 ) & 0x80 );
	export_data[8] = export_data[8] & ( ( ( good_data->id_number ) ) & 0x7F );

	return export_data;
}

void dimagev_dump_camera_data(dimagev_data_t *data) {
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "========= Begin Camera Data =========");
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Host mode: %s ( %d )", data->host_mode ? "Host mode" : "Camera mode", data->host_mode);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Exposure valid: %s ( %d )", data->exposure_valid ? "Valid" : "Not Valid", data->exposure_valid);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Exposure correction: %d", (signed char)data->exposure_correction);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Date valid: %s ( %d )", data->date_valid ? "Valid" : "Not Valid", data->exposure_valid);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Self timer mode: %s ( %d )", data->self_timer_mode ? "Yes" : "No", data->self_timer_mode);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Flash mode: ");

	switch ( data->flash_mode ) {
		case 0:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "\tAuto ( 0 )");
			break;
		case 1:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "\tForce Flash ( 1 )");
			break;
		case 2:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "\tProhibit Flash ( 2 )");
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "\tInvalid mode for flash ( %d )", data->flash_mode);
			break;
	}

	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Quality mode: %s ( %d )", data->quality_setting ? "High" : "Low", data->quality_setting);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Play or Record mode: %s ( %d )", data->play_rec_mode ? "Record" : "Play", data->play_rec_mode);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Date: %02d/%02d/%02d %02d:%02d:%02d", data->year, data->month, data->day, data->hour, data->minute, data->second);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Card ID Valid: %s ( %d )", data->valid ? "Valid" : "Invalid", data->valid);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Card ID Data: %02x", data->id_number);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "========== End Camera Data ==========");

	return;
}

/* This function gets the current system time, sets the contents of the current
   dimagev->data struct appropriately, and then sends the data.
*/
int dimagev_set_date(dimagev_t *dimagev) {
	struct tm *this_time;
	time_t now;

	if ( dimagev == NULL ) {
		return GP_ERROR_BAD_PARAMETERS;
	}

	if ( ( now = time(NULL) ) < 0 ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_set_date::unable to get system time");
		return GP_ERROR;
	}

	if ( ( this_time = localtime(&now) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_set_date::unable to convert system time to local time");
		return GP_ERROR;
	}

	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Setting clock to %02d/%02d/%02d %02d:%02d:%02d", this_time->tm_year % 100, ( this_time->tm_mon + 1 ), this_time->tm_mday, this_time->tm_hour, this_time->tm_min, this_time->tm_sec);

	dimagev->data->date_valid = 1;
	dimagev->data->year = (unsigned char) ( this_time->tm_year % 100 );
	dimagev->data->month = (unsigned char) ( this_time->tm_mon + 1 );
	dimagev->data->day = (unsigned char) this_time->tm_mday;
	dimagev->data->hour = (unsigned char) this_time->tm_hour;
	dimagev->data->minute = (unsigned char) this_time->tm_min;
	dimagev->data->second = (unsigned char) this_time->tm_sec;

	if ( dimagev_send_data(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_set_date::unable to set time");
		return GP_ERROR_IO;
	}

	/* So we don't set this date again later by mistake. */
	dimagev->data->date_valid = 0;

	if ( dimagev_send_data(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_set_date::unable to set time");
		return GP_ERROR_IO;
	}

	return GP_OK;
}
