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
   The subroutines will print out more detained information should they fail. */
int dimagev_get_camera_info(dimagev_t *dimagev) {
	dimagev_packet *p, *raw;
	unsigned char char_buffer;

	/* Check the device. */
	if ( dimagev->dev == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::device not valid");
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Make sure that the old info struct is fred, and allocate a new one. */
	/*
	if ( dimagev->info != NULL ) {
		free(dimagev->info);
	}
	*/

	/* Let's say hello and get the current status. */
	if ( ( p = dimagev_make_packet(DIMAGEV_INQUIRY, 1, 0)) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::unable to allocate packet");
		return GP_ERROR_IO;
	}

	if ( gp_port_write(dimagev->dev, p->buffer, p->length) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::unable to write packet");
		return GP_ERROR_IO;
	} else if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::no response from camera");
		return GP_ERROR_IO;
	}

	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::camera did not acknowledge transmission");
			dimagev_get_camera_info(dimagev);
/*			return GP_ERROR_IO;*/
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::camera cancels transmission");
			return GP_ERROR_IO;
			break;
		default:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
			break;
	}

	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::unable to read packet");
		return GP_ERROR_IO;
	}

	char_buffer = DIMAGEV_EOT;
	if ( gp_port_write(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::unable to send EOT");
		return GP_ERROR_IO;
	}
		
	if ( gp_port_read(dimagev->dev, &char_buffer, 1) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::no response from camera");
		return GP_ERROR_IO;
	}

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::camera did not acknowledge transmission");
			return GP_ERROR_IO;
			break;
		case DIMAGEV_CAN:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::camera cancels transmission");
			return GP_ERROR_IO;
			break;
		default:
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::camera responded with unknown value %x", char_buffer);
			return GP_ERROR_IO;
			break;
	}

	if ( ( raw = dimagev_strip_packet(p) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::unable to strip data packet");
		return GP_ERROR_NO_MEMORY;
	}

	if ( ( dimagev->info = dimagev_import_camera_info(raw->buffer) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_camera_info::unable to read camera info");
		return GP_ERROR;
	}


	/* Sure it *should* get fred automagically, but why take the risk? */
	free(p);
	free(raw);

	return GP_OK;
}

dimagev_info_t *dimagev_import_camera_info(unsigned char *raw_data) {
	dimagev_info_t *info;

	if ( ( info = malloc(sizeof(dimagev_info_t)) ) == NULL ) {
		perror("dimagev_import_camera_info::unable to allocate dimagev_info_t");
		return NULL;
	}

	memcpy(info->vendor, &(raw_data[0]), 8);
	info->vendor[7]=0;
	memcpy(info->model, &(raw_data[8]), 8);
	info->model[7]=0;
	memcpy(info->hardware_rev, &(raw_data[16]), 4);
	info->hardware_rev[3]=0;
	memcpy(info->firmware_rev, &(raw_data[20]), 4);
	info->firmware_rev[3]=0;
	info->have_storage = raw_data[24];

	return info;
}

void dimagev_dump_camera_info(dimagev_info_t *info) {
	if ( info == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "dimagev_dump_camera_info::unable to read NULL simagev_info_t");
		return;
	}

	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "========= Begin Camera Info =========");
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Vendor: %s", info->vendor);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Model: %s", info->model);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Hardware Revision: %s", info->hardware_rev);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "Firmware Revision: %s", info->firmware_rev);
	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "========== End Camera Info ==========");

	return;
}
