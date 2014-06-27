/****************************************************************/
/* library.c  - Gphoto2 library for the KBGear JamCam v2 and v3 */
/*                                                              */
/* Copyright 2001 Chris Pinkham                                 */
/*                                                              */
/* Author: Chris Pinkham <cpinkham@infi.net>                    */
/*                                                              */
/* This library is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU Library General  */
/* Public License as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any     */
/* later version.                                               */
/*                                                              */
/* This library is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU Library General Public License for     */
/* more details.                                                */
/*                                                              */
/* You should have received a copy of the GNU Library General   */
/* Public License along with this library; if not, write to the */
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gphoto2/gphoto2.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "library.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define GP_MODULE

static struct jamcam_file jamcam_files[1024];
static int jamcam_count = 0;
static int jamcam_mmc_card_size = 0;

static int jamcam_read_packet (Camera *camera, unsigned char *packet, int length);
static int jamcam_write_packet (Camera *camera, unsigned char *packet, int length);
static int jamcam_fetch_memory( Camera *camera, CameraFile *file,
		unsigned char *data, int start, int length, GPContext *context);
static int jamcam_query_mmc_card (Camera *camera);

static int jamcam_set_int_at_pos( unsigned char *buf, int pos, int value ) {
	buf[pos + 0] = ( value       ) & 0xff;
	buf[pos + 1] = ( value >>  8 ) & 0xff;
	buf[pos + 2] = ( value >> 16 ) & 0xff;
	buf[pos + 3] = ( value >> 24 ) & 0xff;

	return( value );
}

static int jamcam_get_int_at_pos( unsigned char *buf, int pos ) {
	int ret = 0;

	ret += buf[pos + 0];
	ret += buf[pos + 1] * 256;
	ret += buf[pos + 2] * 256 * 256;
	ret += buf[pos + 3] * 256 * 256 * 256;

	return( ret );
}

static int jamcam_set_usb_mem_pointer( Camera *camera, int position ) {
	char reply[8];

	GP_DEBUG ("* jamcam_set_usb_mem_pointer");
	GP_DEBUG ("*** position:  %d (0x%x)",
		position, position);

	gp_port_usb_msg_write( camera->port,
		0xa1,
		( position       ) & 0xffff,
		( position >> 16 ) & 0xffff,
		NULL, 0 );

	gp_port_usb_msg_read( camera->port,
		0xa0,
		0,
		0,
		reply, 8 );

	return( GP_OK );
}


/* get the number of images on the mmc card */
static int jamcam_mmc_card_file_count (Camera *camera) {
	unsigned char buf[16];
	unsigned char reply[512];
	unsigned int position = 0x40000000;
	int data_incr;
	int width;
	int height;

	GP_DEBUG ("* jamcam_mmc_card_file_count");

	memset( buf, 0, sizeof( buf ));

	switch( camera->port->type ) {
		default:
		case GP_PORT_SERIAL:
			memcpy( buf, "KB00", 4 );
			jamcam_set_int_at_pos( buf, 4, position );
			jamcam_write_packet( camera, buf, 8 );

			jamcam_read_packet( camera, reply, 16 );

			while( memcmp( reply, "KB", 2 ) == 0 ) {
				width  = (reply[5] * 256) + reply[4];
				height = (reply[7] * 256) + reply[6];

				data_incr = jamcam_get_int_at_pos( reply, 8 );

				jamcam_files[jamcam_count].position = position;
				jamcam_files[jamcam_count].width = width;
				jamcam_files[jamcam_count].height = height;
				jamcam_files[jamcam_count].data_incr = data_incr;

				jamcam_count++;

				position += data_incr;

				jamcam_set_int_at_pos( buf, 4, position );
				jamcam_write_packet( camera, buf, 8 );
			
				jamcam_read_packet( camera, reply, 16 );
			}
			break;

		case GP_PORT_USB:
			gp_port_usb_msg_write( camera->port,
				0xa5,
				0x0005,
				0x0000,
				NULL, 0 );

			jamcam_set_usb_mem_pointer( camera, position );

			CHECK( gp_port_read (camera->port, (char*)reply, 0x10 ));

			width  = (reply[13] * 256) + reply[12];
			height = (reply[15] * 256) + reply[14];

			jamcam_set_usb_mem_pointer( camera, position + 8 );

			CHECK( gp_port_read (camera->port, (char*)reply, 512 ));

			gp_port_usb_msg_write( camera->port,
				0xa5,
				0x0006,
				0x0000,
				NULL, 0 );

			while((reply[0] != 0xff ) &&
			      (reply[0] != 0xaa ) &&
				  ((reply[0] != 0x00 ) ||
				   (reply[1] != 0x00 ))) {
				data_incr = jamcam_get_int_at_pos( reply, 0 );

				jamcam_files[jamcam_count].position = position;
				jamcam_files[jamcam_count].width = width;
				jamcam_files[jamcam_count].height = height;
				jamcam_files[jamcam_count].data_incr = data_incr;
				jamcam_count++;

				position += data_incr;

				gp_port_usb_msg_write( camera->port,
					0xa5,
					0x0005,
					0x0000,
					NULL, 0 );

				jamcam_set_usb_mem_pointer( camera, position );

				CHECK( gp_port_read (camera->port, (char*)reply, 0x10 ));

				width  = (reply[13] * 256) + reply[12];
				height = (reply[15] * 256) + reply[14];

				jamcam_set_usb_mem_pointer( camera, position + 8 );

				CHECK( gp_port_read (camera->port, (char*)reply, 512 ));

				gp_port_usb_msg_write( camera->port,
					0xa5,
					0x0006,
					0x0000,
					NULL, 0 );
			}
			break;
	}

	GP_DEBUG (		"*** returning with jamcam_count = %d", jamcam_count);
	return( 0 );
}

int jamcam_file_count (Camera *camera) {
	unsigned char buf[16];
	unsigned char reply[16];
	int position = 0;
	int data_incr;
	int width;
	int height;
	int last_offset_size = 0;

	GP_DEBUG ("* jamcam_file_count");

	jamcam_count = 0;

	memset( buf, 0, sizeof( buf ));

	switch( camera->port->type ) {
		default:
		case GP_PORT_SERIAL:
			memcpy( buf, "KB00", 4 );
			jamcam_set_int_at_pos( buf, 4, position );
			jamcam_write_packet( camera, buf, 8 );

			jamcam_read_packet( camera, reply, 16 );

			while( reply[0] == 'K' ) {
				width  = (reply[5] * 256) + reply[4];
				height = (reply[7] * 256) + reply[6];

				data_incr = jamcam_get_int_at_pos( reply, 8 );

				last_offset_size = data_incr;

				jamcam_files[jamcam_count].position = position;
				jamcam_files[jamcam_count].width = width;
				jamcam_files[jamcam_count].height = height;
				jamcam_files[jamcam_count].data_incr = data_incr;

				jamcam_count++;

				position += data_incr;

				jamcam_set_int_at_pos( buf, 4, position );
				jamcam_write_packet( camera, buf, 8 );
			
				jamcam_read_packet( camera, reply, 16 );
			}

			/* the v3 camera uses 0x3fdf0 data increments so check for MMC */
			if ( last_offset_size == 0x03fdf0 ) {
				jamcam_query_mmc_card( camera );
			}
			break;

		case GP_PORT_USB:
			jamcam_set_usb_mem_pointer( camera, position );

			CHECK( gp_port_read (camera->port, (char*)reply, 0x10 ));

			width  = (reply[13] * 256) + reply[12];
			height = (reply[15] * 256) + reply[14];

			jamcam_set_usb_mem_pointer( camera, position + 8 );

			CHECK( gp_port_read (camera->port, (char*)reply, 0x10 ));

			while(reply[0] != 0xff ) {
				data_incr = jamcam_get_int_at_pos( reply, 0 );

				jamcam_files[jamcam_count].position = position;
				jamcam_files[jamcam_count].width = width;
				jamcam_files[jamcam_count].height = height;
				jamcam_files[jamcam_count].data_incr = data_incr;
				jamcam_count++;

				position += data_incr;

				jamcam_set_usb_mem_pointer( camera, position );

				CHECK( gp_port_read (camera->port, (char*)reply, 0x10 ));

				width  = (reply[13] * 256) + reply[12];
				height = (reply[15] * 256) + reply[14];

				jamcam_set_usb_mem_pointer( camera, position + 8 );

				CHECK( gp_port_read (camera->port, (char*)reply, 0x10 ));
			}
			break;
	}

	if ( jamcam_mmc_card_size ) {
		jamcam_count += jamcam_mmc_card_file_count( camera );
	}

	GP_DEBUG (		"*** returning jamcam_count = %d", jamcam_count);
	return( jamcam_count );
}

static int jamcam_fetch_memory( Camera *camera, CameraFile *file,
		unsigned char *data, int start, int length, GPContext *context) {
	unsigned char tmp_buf[16];
	unsigned char packet[16];
	int new_start;
	int new_end;
	int bytes_read = 0;
	int bytes_to_read;
	int bytes_left = length;
	int res = GP_OK;
	unsigned int id = 0;

	GP_DEBUG ("* jamcam_fetch_memory");
	GP_DEBUG ("  * start:  %d (0x%x)",
		start, start);
	GP_DEBUG ("  * length: %d (0x%x)",
		length, length);

	if ( length > 1000 )
		id = gp_context_progress_start (context, length,
			_("Downloading data..."));

	while( bytes_left ) {
		switch( camera->port->type ) {
			default:
			case GP_PORT_SERIAL:
				bytes_to_read =
					bytes_left > SER_PKT_SIZE ? SER_PKT_SIZE : bytes_left;

				memset( packet, 0, sizeof( packet ));
				memcpy( packet, "KB01", 4 );

				new_start = start + bytes_read;
				new_end   = start + bytes_read + bytes_to_read - 1;

				/* start and end (inclusive) */
				jamcam_set_int_at_pos( packet, 4, new_start );
				jamcam_set_int_at_pos( packet, 8, new_end );

				jamcam_write_packet( camera, packet, 12 );

				CHECK (jamcam_read_packet( camera, data + bytes_read,
					bytes_to_read ));
				break;
			case GP_PORT_USB:
				bytes_to_read = bytes_left > USB_PKT_SIZE ? USB_PKT_SIZE : bytes_left;

				/* for some reason this priming read fixes an offset problem */
				/* in the images, we are only reading the first 16 bytes of  */
				/* data twice by doing this, so I don't know why it works    */
				jamcam_set_usb_mem_pointer( camera, start + bytes_read );
				CHECK( gp_port_read (camera->port, (char*)tmp_buf, 16 ));


				jamcam_set_usb_mem_pointer( camera, start + bytes_read );
				CHECK( gp_port_read (camera->port, (char*)data + bytes_read, bytes_to_read ));


				break;
		}
				
		bytes_left -= bytes_to_read;
		bytes_read += bytes_to_read;

		/* hate this hardcoded, but don't want to update here */
		/* when downloading parts of a thumbnail              */
		if ( length > 1000 ) {
			gp_context_progress_update (context, id, bytes_read);
			if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
				GP_DEBUG ("  * CANCELED");
				break;
			}
		}
	}

	if ( length > 1000 )
		gp_context_progress_stop (context, id);

	if ( res == GP_OK ) {
		GP_DEBUG ("  * returning OK");
	}
	return( res );
}

int jamcam_request_image( Camera *camera, CameraFile *file,
		char *buf, int *len, int number, GPContext *context ) {
	int position;
	int result;
	unsigned char *tmp_buf;

	GP_DEBUG ("* jamcam_request_image");
	tmp_buf = malloc(640*480*3);

	position = jamcam_files[number].position;

	/* don't know why this is necessary, but do it anyway */
	if ( camera->port->type == GP_PORT_USB ) {
		position += 8;
	}

	if ( camera->port->type == GP_PORT_USB ) {
		gp_port_usb_msg_write( camera->port,
			0xa5,
			0x0005,
			0x0000,
			NULL, 0 );
	}
	
	result = jamcam_fetch_memory( camera, file, tmp_buf, position,
		jamcam_files[number].data_incr, context );

	/* this seems to reset the camera to a sane status */
	if ( camera->port->type == GP_PORT_USB ) {
		gp_port_usb_msg_write( camera->port,
			0xa5,
			0x0006,
			0x0000,
			NULL, 0 );
	}

	if ( result == GP_OK ) {
		*len = jamcam_files[number].width * jamcam_files[number].height;
		memcpy( buf, tmp_buf + 0x10, *len );
	}
	free (tmp_buf);

	return( result );
}

struct jamcam_file *jamcam_file_info(Camera *camera, int number)
{
	GP_DEBUG(" * jamcam_file_info, nr is %d", number);
	return( &jamcam_files[number] );
}

int jamcam_request_thumbnail( Camera *camera, CameraFile *file,
		char *buf, int *len, int number, GPContext *context ) {
	unsigned char line[2048];
	char packet[16];
	int position;
	int x, y;
	int res = GP_OK;
	char *ptr;
	int bytes_to_read;
	unsigned int id;

	GP_DEBUG ("* jamcam_request_thumbnail");

	memset( packet, 0, sizeof( packet ));

	position = jamcam_files[number].position + 0x10;

	*len = 4800;

	ptr = buf;

	if ( camera->port->type == GP_PORT_USB ) {
		/* windows driver does this sometimes */
		gp_port_usb_msg_write( camera->port,
			0xa5,
			0x0005,
			0x0000,
			NULL, 0 );

		/* just read one row of data at a time */
		bytes_to_read = jamcam_files[number].width;
	} else {
		/* MMC card is quirky, need to fetch larger amounts of data */
		if ( position >= 0x40000000 ) {
			/* serial with mmc card needs bigger packets */
			bytes_to_read = 2048;
		} else {
			/* just read one row of data at a time */
			bytes_to_read = jamcam_files[number].width;
		}
	}

	/* fetch thumbnail lines and build the thumbnail */
	position += 10 * jamcam_files[number].width;
	id = gp_context_progress_start (context, 60.,
					_("Downloading thumbnail..."));
	for( y = 0 ; y < 60 ; y++ ) {
		jamcam_fetch_memory( camera, file, line, position, bytes_to_read, context );

		gp_context_progress_update (context, id, y);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			res = GP_ERROR_CANCEL;
			break;
		}

		if ( jamcam_files[number].width == 600 ) {
			for( x = 22; x < 578 ; x += 7 ) {
				*(ptr++) = line[x];
			}
			position += 7 * 600;
		} else {
			for( x = 0; x < 320 ; ) {
				*(ptr++) = line[x];
				x += 3;
				*(ptr++) = line[x];
				x += 5;
			}

			if ( y % 2 ) {
				position += 5 * 320;
			} else {
				position += 3 * 320;
			}
		}
	}
	gp_context_progress_stop (context, id);

	/* this seems to reset the camera to a sane status */
	if ( camera->port->type == GP_PORT_USB ) {
		gp_port_usb_msg_write( camera->port,
			0xa5,
			0x0006,
			0x0000,
			NULL, 0 );
	}

	return( res );
}

static int jamcam_write_packet (Camera *camera, unsigned char *packet, int length) {
	int ret, r;

	GP_DEBUG ("* jamcam_write_packet");

	for (r = 0; r < RETRIES; r++) {
		ret = gp_port_write (camera->port, (char*)packet, length);
		if (ret == GP_ERROR_TIMEOUT)
			continue;

		return (ret);
	}

	return (GP_ERROR_TIMEOUT);
}

static int jamcam_read_packet (Camera *camera, unsigned char *packet, int length) {
	int r = 0;
	int bytes_read;

	GP_DEBUG ("* jamcam_read_packet");
	GP_DEBUG ("*** length: %d (0x%x)",
		length, length);

	for (r = 0; r < RETRIES; r++) {
		bytes_read = gp_port_read (camera->port, (char*)packet, length);
		if (bytes_read == GP_ERROR_TIMEOUT)
			continue;
		if (bytes_read < 0)
			return (bytes_read);

		if ( bytes_read == length ) {
			return( GP_OK );
		}
	}

	return (GP_ERROR_TIMEOUT);
}


int jamcam_enq (Camera *camera)
{
	int ret, r = 0;
	unsigned char buf[16];

	GP_DEBUG ("* jamcam_enq");

	memset( buf, 0, 16 );

	switch( camera->port->type ) {
		default:
		case GP_PORT_SERIAL:
			for (r = 0; r < RETRIES; r++) {
				memcpy (buf, "KB99", 4 );

				ret = jamcam_write_packet (camera, buf, 4);
				if (ret == GP_ERROR_TIMEOUT)
					continue;
				if (ret != GP_OK)
					return (ret);

				ret = jamcam_read_packet (camera, buf, 4);
				if (ret == GP_ERROR_TIMEOUT)
					continue;
				if (ret != GP_OK)
					return (ret);

				if ( !memcmp( buf, "KIDB", 4 )) {
					return (GP_OK);
				}
			}
			return (GP_ERROR_CORRUPTED_DATA);
			break;

		case GP_PORT_USB:
			for (r = 0; r < RETRIES; r++) {
				gp_port_usb_msg_write( camera->port,
					0xa5,
					0x0004,
					0x0000,
					NULL, 0 );
				jamcam_set_usb_mem_pointer( camera, 0x0000 );

				CHECK( gp_port_read( camera->port, (char *)buf, 0x0c ));

				if (( !memcmp( buf, "KB00", 4 )) ||
					(( buf[0] == 0xff ) && ( buf[1] == 0xff ) &&
					 ( buf[2] == 0xff ) && ( buf[3] == 0xff ) &&
					 ( buf[4] == 0xff ) && ( buf[5] == 0xff ) &&
					 ( buf[6] == 0xff ) && ( buf[7] == 0xff ))) {
					/* found a JamCam v3 camera */
					/* reply contains 4-bytes with MMC card size if present */
					/* set to 0 if none */
					jamcam_mmc_card_size = jamcam_get_int_at_pos( buf, 8 );

					if ( jamcam_mmc_card_size ) {
						GP_DEBUG (							"* jamcam_enq, MMC card size = %d",
							jamcam_mmc_card_size );
					}

					return (GP_OK);
				} else if ( !memcmp( buf + 8, "KB00", 4 )) {
					/* found a JamCam v2 camera */
					/* JamCam v2 doesn't support MMC card so no need to check */
					return (GP_OK);
				} else if (( buf[0] == 0xf0 ) &&
						 ( buf[1] == 0xfd ) &&
						 ( buf[2] == 0x03 )) {
					return( GP_OK );
				}
			}
			return (GP_ERROR_CORRUPTED_DATA);
			break;
	}

	return (GP_ERROR_TIMEOUT);
}

static int jamcam_query_mmc_card (Camera *camera)
{
	int ret, r = 0;
	unsigned char buf[16];

	GP_DEBUG ("* jamcam_query_mmc_card");

	/* usb port doesn't need this packet, this info found in enquiry reply */
	if ( camera->port->type == GP_PORT_USB ) {
		return( GP_OK );
	}

	memcpy( buf, "KB04", 4 );

	for (r = 0; r < RETRIES; r++) {

		ret = jamcam_write_packet (camera, buf, 4);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return (ret);

		ret = jamcam_read_packet (camera, buf, 4);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return (ret);

		/* reply is 4-byte int showing length of MMC card if any, 0 if none */
		jamcam_mmc_card_size = jamcam_get_int_at_pos( buf, 0 );

		if ( jamcam_mmc_card_size ) {
			GP_DEBUG ("* jamcam_query_mmc_card, MMC card size = %d",
				jamcam_mmc_card_size );
		}

		return (GP_OK);
	}
	return (GP_ERROR_TIMEOUT);
}

