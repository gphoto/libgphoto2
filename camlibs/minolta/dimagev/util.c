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

int dimagev_get_picture(dimagev_t *dimagev, int file_number, CameraFile *file) {
	int length, total_packets, i;
	dimagev_packet *p, *r;
	unsigned char char_buffer, command_buffer[8];
#ifdef _gphoto_exif_
	exifparser exifdat;
#endif

	if ( dimagev->data->host_mode != 1 ) {

		dimagev->data->host_mode = 1;

		if ( dimagev_send_data(dimagev) == GP_ERROR ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to set host mode");
			}
			return GP_ERROR;
		}
	}

	gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::file_number is %d", file_number);

	/* Maybe check if it exists? Check the file type? */
	
	/* First make the command packet. */
	command_buffer[0] = 0x04;
	command_buffer[1] = (unsigned char)( file_number / 256 );
	command_buffer[2] = (unsigned char)( file_number % 256 );
	if ( ( p = dimagev_make_packet(command_buffer, 3, 0) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to allocate command packet");
		}
		return GP_ERROR;
	}

	if ( gpio_write(dimagev->dev, p->buffer, p->length) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to send set_data packet");
		}
		return GP_ERROR;
	} else if ( gpio_read(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			perror("dimagev_get_picture::no response from camera");
		}
		return GP_ERROR;
	}
		
	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::camera did not acknowledge transmission\n");
			}
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::camera cancels transmission\n");
			}
			return GP_ERROR;
			break;
		default:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::camera responded with unknown value %x\n", char_buffer);
			}
			return GP_ERROR;
			break;
	}

	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to read packet");
		}
		return GP_ERROR;
	}

	if ( ( r = dimagev_strip_packet(p) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to strip packet");
		}
		return GP_ERROR;
	}
		
	free(p);

	total_packets = r->buffer[0];
	length = ( r->length - 1 );

	/* Allocate an extra byte just in case. */
	if ( ( file->data = malloc((993 * total_packets) + 1) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to allocate buffer for file");
		}
		return GP_ERROR;
	}

	memcpy(file->data, &(r->buffer[1]), r->length );
	file->size += ( r->length - 2 );

	free(r);

	for ( i = 0 ; i < ( total_packets -1 ) ; i++ ) {

		char_buffer=DIMAGEV_ACK;
		if ( gpio_write(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to send ACK");
			}
			return GP_ERROR;
		}
	
		if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to read packet");
			}
			return GP_ERROR;
		}

		if ( ( r = dimagev_strip_packet(p) ) == NULL ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to strip packet");
			}
			return GP_ERROR;
		}
		
		free(p);

		memcpy(&( file->data[ ( file->size + 1) ] ), r->buffer, r->length );
		file->size += r->length;

		free(r);
	}

	file->size++;

	char_buffer=DIMAGEV_EOT;
	if ( gpio_write(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to send ACK");
		}
		return GP_ERROR;
	}

	if ( gpio_read(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			perror("dimagev_get_picture::no response from camera");
		}
		return GP_ERROR;
	}
		
	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::camera did not acknowledge transmission\n");
			}
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::camera cancels transmission\n");
			}
			return GP_ERROR;
			break;
		default:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::camera responded with unknown value %x\n", char_buffer);
			}
			return GP_ERROR;
			break;
	}

	/* Now set the camera back into local mode. */
	dimagev->data->host_mode = 0;

	if ( dimagev_send_data(dimagev) == GP_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to set host mode");
		}
		return GP_ERROR;
	}

	sleep(1);

#ifdef _gphoto_exif_
	exifdat.header = file->data;
	exifdat.data = file->data + 12 ;

	if ( stat_exif(&exifdat) != 0 ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to stat EXIF tags");
		}
		return GP_OK;
	}

	gpe_dump_exif(&exifdat);

#endif

	return GP_OK;
}

int dimagev_delete_picture(dimagev_t *dimagev, int file_number) {
	dimagev_packet *p, *raw;
	unsigned char char_buffer, command_buffer[8];

	if ( dimagev == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::unable to use NULL dimagev_t");
		return GP_ERROR;
	}

	if ( dimagev->data->host_mode != 1 ) {

		dimagev->data->host_mode = 1;

		if ( dimagev_send_data(dimagev) == GP_ERROR ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to set host mode");
			}
			return GP_ERROR;
		}
	}


	/* First make the command packet. */
	command_buffer[0] = 0x05;
	command_buffer[1] = (unsigned char)( file_number / 256 );
	command_buffer[2] = (unsigned char)( file_number % 256 );
	if ( ( p = dimagev_make_packet(command_buffer, 3, 0) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::unable to allocate command packet");
		}
		return GP_ERROR;
	}

	if ( gpio_write(dimagev->dev, p->buffer, p->length) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::unable to send set_data packet");
		}
		return GP_ERROR;
	} else if ( gpio_read(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			perror("dimagev_delete_picture::no response from camera");
		}
		return GP_ERROR;
	}
		
	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera did not acknowledge transmission\n");
			}
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera cancels transmission\n");
			}
			return GP_ERROR;
			break;
		default:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera responded with unknown value %x\n", char_buffer);
			}
			return GP_ERROR;
			break;
	}

	
	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::unable to read packet");
		}
		return GP_ERROR;
	}

	if ( ( raw = dimagev_strip_packet(p) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::unable to strip packet");
		}
		return GP_ERROR;
	}
	
	free(p);

	if ( raw->buffer[0] != 0 ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::delete returned error code");
		}
		return GP_ERROR;
	}

	char_buffer=DIMAGEV_EOT;
	if ( gpio_write(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to send ACK");
		}
		return GP_ERROR;
	}

	if ( gpio_read(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			perror("dimagev_delete_picture::no response from camera");
		}
		return GP_ERROR;
	}
		
	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera did not acknowledge transmission\n");
			}
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera cancels transmission\n");
			}
			return GP_ERROR;
			break;
		default:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_delete_picture::camera responded with unknown value %x\n", char_buffer);
			}
			return GP_ERROR;
			break;
	}

	if ( dimagev->data->host_mode != 0 ) {

		dimagev->data->host_mode = 0;

		if ( dimagev_send_data(dimagev) == GP_ERROR ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_picture::unable to set host mode");
			}
			return GP_ERROR;
		}
	}


	return GP_OK;
}

int dimagev_delete_all(dimagev_t *dimagev) {
	return GP_OK;
}

int dimagev_get_thumbnail(dimagev_t *dimagev, int file_number, CameraFile *file) {
	dimagev_packet *p, *r;
	unsigned char char_buffer, command_buffer[8], *ycrcb_data;

	if ( dimagev->data->host_mode != 1 ) {

		dimagev->data->host_mode = 1;

		if ( dimagev_send_data(dimagev) == GP_ERROR ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to set host mode");
			}
			return GP_ERROR;
		}
	}

	gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::file_number is %d", file_number);

	/* Maybe check if it exists? Check the file type? */
	
	/* First make the command packet. */
	command_buffer[0] = 0x0d;
	command_buffer[1] = (unsigned char)( file_number / 256 );
	command_buffer[2] = (unsigned char)( file_number % 256 );
	if ( ( p = dimagev_make_packet(command_buffer, 3, 0) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to allocate command packet");
		}
		return GP_ERROR;
	}

	if ( gpio_write(dimagev->dev, p->buffer, p->length) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to send set_data packet");
		}
		return GP_ERROR;
	} else if ( gpio_read(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			perror("dimagev_get_thumbnail::no response from camera");
		}
		return GP_ERROR;
	}
		
	free(p);

	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::camera did not acknowledge transmission\n");
			}
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::camera cancels transmission\n");
			}
			return GP_ERROR;
			break;
		default:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::camera responded with unknown value %x\n", char_buffer);
			}
			return GP_ERROR;
			break;
	}

	if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to read packet");
		}
		return GP_ERROR;
	}

	if ( ( r = dimagev_strip_packet(p) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to strip packet");
		}
		return GP_ERROR;
	}
		
	free(p);

	/* Unlike normal images, we are guaranteed 9600 bytes *exactly* */

	/* Allocate an extra byte just in case. */
	if ( ( ycrcb_data = malloc(9601) ) == NULL ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to allocate buffer for file");
		}
		return GP_ERROR;
	}

	memcpy(ycrcb_data, r->buffer, r->length );
	file->size +=  r->length - 1 ;

	free(r);

	while ( file->size < 9599 ) {

		char_buffer=DIMAGEV_ACK;
		if ( gpio_write(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to send ACK");
			}
			return GP_ERROR;
		}
	
		if ( ( p = dimagev_read_packet(dimagev) ) == NULL ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to read packet");
			}
			return GP_ERROR;
		}

		if ( ( r = dimagev_strip_packet(p) ) == NULL ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to strip packet");
			}
			return GP_ERROR;
		}
		
		free(p);

		memcpy(&( ycrcb_data[ ( file->size + 1) ] ), r->buffer, r->length );
		file->size += r->length;

		free(r);

		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::current file size is %d\n", file->size);
		}
	}

	file->size++;

	char_buffer=DIMAGEV_EOT;
	if ( gpio_write(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to send ACK");
		}
		return GP_ERROR;
	}

	if ( gpio_read(dimagev->dev, &char_buffer, 1) == GPIO_ERROR ) {
		if ( dimagev->debug != 0 ) {
			perror("dimagev_get_thumbnail::no response from camera");
		}
		return GP_ERROR;
	}
		
	switch ( char_buffer ) {
		case DIMAGEV_ACK:
			break;
		case DIMAGEV_NAK:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::camera did not acknowledge transmission\n");
			}
			return GP_ERROR;
			break;
		case DIMAGEV_CAN:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::camera cancels transmission\n");
			}
			return GP_ERROR;
			break;
		default:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::camera responded with unknown value %x\n", char_buffer);
			}
			return GP_ERROR;
			break;
	}

	/* Now set the camera back into local mode. */
	dimagev->data->host_mode = 0;

	if ( dimagev_send_data(dimagev) == GP_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_get_thumbnail::unable to set host mode");
		}
		return GP_ERROR;
	}

	sleep(1);

	file->data = dimagev_ycbcr_to_ppm(ycrcb_data, 9600);
	file->size = 14413;

	return GP_OK;
}

/* This function handles the ugliness of converting Y:Cb:Cr data to RGB. The
   return value is a pointer to an array of unsigned chars that has (size +13)
   members. The additional thirteen bytes are the PPM "rawbits" header.
*/
unsigned char *dimagev_ycbcr_to_ppm(unsigned char *ycbcr, int size) {
	unsigned char *rgb_data, *ycrcb_current, *rgb_current;
	int count=0;
	unsigned int magic_r, magic_g, magic_b;

	if ( ( rgb_data = malloc( ( 1.5 * size ) + 13 ) ) == NULL ) {
		gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "dimagev_ycbcr_to_ppm::unable to allocate buffer for Y:Cb:Cr conversion");
		return NULL;
	}

	ycrcb_current = ycbcr;
	rgb_current = &(rgb_data[13]);

	rgb_data[0]='P';
	rgb_data[1]='6';
	rgb_data[2]='\n';
	rgb_data[3]='8';
	rgb_data[4]='0';
	rgb_data[5]=' ';
	rgb_data[6]='6';
	rgb_data[7]='0';
	rgb_data[8]='\n';
	rgb_data[9]='2';
	rgb_data[10]='5';
	rgb_data[11]='5';
	rgb_data[12]='\n';

	while ( count < size ) {
		magic_b = ( ( ycrcb_current[2] > 128 ? 128 : ycrcb_current[2] ) - 128 ) * ( 2 - ( 2 * CR_COEFF ) ) + ycrcb_current[0];
		rgb_current[2] = ( magic_b > 255 ? 255 : magic_b );
		magic_r = ( ( ycrcb_current[3] > 128 ? 128 : ycrcb_current[3] ) - 128 ) * ( 2 - ( 2 - Y_COEFF ) ) + ycrcb_current[0];
		rgb_current[0] = ( magic_r > 255 ? 255 : magic_r );
		magic_g = (( ycrcb_current[0] - ( CR_COEFF * rgb_current[2] ) ) - ( Y_COEFF * rgb_current[0])) / CB_COEFF ; 
		rgb_current[1] = ( magic_g > 255 ? 255 : magic_g );

		magic_b = ( ( ycrcb_current[2] > 128 ? 128 : ycrcb_current[2] ) - 128 ) * ( 2 - ( 2 * CR_COEFF ) ) + ycrcb_current[1];
		rgb_current[5] = ( magic_b > 255 ? 255 : magic_b );
		magic_r = ( ( ycrcb_current[3] > 128 ? 128 : ycrcb_current[3] ) - 128 ) * ( 2 - ( 2 - Y_COEFF ) ) + ycrcb_current[1];
		rgb_current[3] = ( magic_r > 255 ? 255 : magic_r );
		magic_g = (( ycrcb_current[1] - ( CR_COEFF * rgb_current[5] ) ) - ( Y_COEFF * rgb_current[3])) / CB_COEFF ; 
		rgb_current[4] = ( magic_g > 255 ? 255 : magic_g );

		/* Wipe everything clean */
		magic_b = magic_r = magic_g = 0;

		count += 4;
		ycrcb_current += 4;
		rgb_current += 6;
	}

	return rgb_data;
}
