/* dc120.c
 *
 * Copyright (C) Scott Fritzinger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _BSD_SOURCE
#define _POSIX_C_SOURCE 199309L
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gphoto2/gphoto2.h>

#include "dc120.h"
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

static char *
dc120_packet_new (int command_byte) {

	char *p = (char *)malloc(sizeof(char) * 8);

	memset (p, 0, 8);

	p[0] = command_byte;
	p[7] = 0x1a;

	return p;
}

/** Checks if a response is ok.
 *  Checks if the response is ok (completed, ready, received) or not ok.
 *  @returns GP_OK if command is ok, GP_ERROR otherwise.
 */
static int
dc120_response_ok (unsigned char response) {
  /* This is stupid. how do you get a constant of type signed char?? (Ralf) */
	 if( response == 0x00 || /* Command completed */
	     response == 0x10 || /* Command ready */
	     response == 0xd1 || /* Command received (ACK) */
	     response == 0xd2 )  /* Correct packet */
	   return (GP_OK);
         else
	   return (GP_ERROR);
}

/** Writes a packet to the port.
 *  This function writes a packet to a port determined by camera->port.
 *  When read_response != FALSE it also reads the response.
 */
static int
dc120_packet_write (Camera *camera, char *packet, int size, int read_response) {
  
	/* Writes the packet and returns the result */

	int x=0;
	unsigned char in[2];

write_again:
	/* If retry, give camera some recup time */
	if (x > 0)
		usleep(SLEEP_TIMEOUT * 1000);

	/* Return error if too many retries */
	if (x++ > RETRIES) 
		return (GP_ERROR);

	if (gp_port_write(camera->port, packet, size) < GP_OK)
		goto write_again;


	/* Read in the response from the camera if requested */
	if (read_response) {
		if (gp_port_read(camera->port, (char *)in, 1) < GP_OK)
			/* On error, write again */
			goto write_again;

		if ( dc120_response_ok(in[0]) != (GP_OK) ) {
			/* If a single byte response, don't rewrite */
			if (size == 1)
				return (GP_ERROR);
			/* Got error response. Go again. */
			goto write_again;
		}
	}

	return (GP_OK);
}

static int
dc120_packet_read (Camera *camera, char *packet, int size) {

	return (gp_port_read(camera->port, packet, size));
}

static int dc120_packet_read_data (Camera *camera, CameraFile *file, char *cmd_packet, int *size, int block_size, GPContext *context) {

	/* Reads in multi-packet data, appending it to the "file". */

	int  num_packets, retval;
	int  packets_received;
	int  bytes_received;
	int  retries=0;
	char packet[2048], p[8];
	unsigned int id;

	/* Determine number of packets needed on-the-fly */
	if (*size > 0) {
		num_packets = (*size + block_size-1) / block_size;
	} else {
		num_packets = 5;
	}

	id = gp_context_progress_start (context, num_packets, _("Getting data..."));
read_data_write_again:
	if (dc120_packet_write(camera, cmd_packet, 8, 1) < GP_OK) 
		return (GP_ERROR);

	packets_received = 0;
	while( packets_received < num_packets ) {
		gp_context_progress_update (context, id, packets_received);
		retval = dc120_packet_read(camera, packet, block_size+1);
		switch (retval) {
		  case GP_ERROR:
		  case GP_ERROR_TIMEOUT:
			/* Write that packet was bad and resend */
			if (retries++ > RETRIES)
				return (GP_ERROR);
			if (packets_received==0) {
				/* If first packet didn't come, write command again */
				goto read_data_write_again;
			} else {
				/* Didn't get data packet. Send retry */
				p[0] = PACK0;
				if (dc120_packet_write(camera, p, 1, 0)== GP_ERROR)
					return (GP_ERROR);
			}
			break;
		  default: 		
			packets_received++;

			/* Default is packet was OK */
			p[0] = PACK1;

			/* Do some command-specific stuff */
			switch (cmd_packet[0]) {
			   case 0x4A:
				/* Set size and num_packets for camera filenames*/
				if (packets_received==1)
					*size = ((unsigned char)packet[0] * 256 +
					         (unsigned char)packet[1])* 20 + 2;
				num_packets = (*size + block_size-1) / block_size;
				break;
			   case 0x64:
			   case 0x54:
				/* If thumbnail, cancel rest of image transfer */
				if ((num_packets == 16)&&(packets_received==16))
					p[0] = CANCL;
				/* No break on purpose */
				break;
			   default:
				/* Nada */
				break;
			}

			/* Write response */
			if (dc120_packet_write(camera, p, 1, FALSE)== GP_ERROR)
				return (GP_ERROR);

			/* Copy in data */
			if (packets_received == num_packets)
			  bytes_received = *size - ((num_packets-1) * block_size);
			else
			  bytes_received = block_size;
			gp_file_append(file, packet, bytes_received );

		}
	}
	gp_context_progress_stop (context, id);

	if ((unsigned char)p[0] != CANCL)
		/* Read in command completed. 
		   (Shouldn't we check for command completed (Ralf)) */
		dc120_packet_read(camera, p, 1);

	return (GP_OK);

}

int dc120_set_speed (Camera *camera, int speed) 
{
	int error = GP_OK;
	GPPortSettings settings;
	char *p = dc120_packet_new(0x41);

	if(p == NULL) {
		/* unlikely to happen, FFS */
		return GP_ERROR;
	}
	gp_port_get_settings(camera->port, &settings);

	switch (speed) {
		case 9600:
			p[2] = (unsigned char)0x96;
			p[3] = (unsigned char)0x00;
			settings.serial.speed = 9600;
			break;
		case 19200:
			p[2] = (unsigned char)0x19;
			p[3] = (unsigned char)0x20;
			settings.serial.speed = 19200;
			break;
		case 38400:
			p[2] = (unsigned char)0x38;
			p[3] = (unsigned char)0x40;
			settings.serial.speed = 38400;
			break;
		case 57600:
			p[2] = (unsigned char)0x57;
			p[3] = (unsigned char)0x60;
			settings.serial.speed = 57600;
			break;
		case 0: /* Default */
		case 115200:
			p[2] = (unsigned char)0x11;
			p[3] = (unsigned char)0x52;
			settings.serial.speed = 115200;
			break;
/* how well supported is 230.4?
		case 230400:
			p[2] = 0x23;
			p[3] = 0x04;
			settings.serial.speed = 230400;
			break;
*/
		default:
			error = GP_ERROR;
			goto fail;
			break;
	}

	if (dc120_packet_write(camera, p, 8, 1) == GP_ERROR) 
	{
		error = GP_ERROR;
		goto fail;
	}

	gp_port_set_settings (camera->port, settings);


	usleep(300 * 1000);

	/* Speed change went OK. */
	error = GP_OK;

 fail:
	free (p);
	return error;
}

int dc120_get_status (Camera *camera, Kodak_dc120_status *status, GPContext *context) {

	CameraFile *file;
	char *p = dc120_packet_new(0x7F);
	int i;
 	int retval;
	int size;

	gp_file_new (&file);
	size = 256;
	retval = dc120_packet_read_data(camera, file, p, &size, 256, context);

        if( retval == (GP_OK) && status != NULL )
        {
	    const char *data;
	    long unsigned int  lsize;

	    gp_file_get_data_and_size( file, &data, &lsize );
	    if( lsize<122 ) {
	      gp_file_free (file);
	      free (p);
	      return (GP_ERROR);
	    }
	    
            memset((char*)status,0,sizeof(*status));

            status->camera_type_id        = data[1];
            status->firmware_major        = data[2];
            status->firmware_minor        = data[3];
            status->batteryStatusId       = data[8];
            status->acStatusId            = data[9];
            
            /* seconds since unix epoc */
            status->time = CAMERA_EPOC + (
		data[12] * 0x1000000 + 
                data[13] * 0x10000 + 
                data[14] * 0x100 + 
                data[15]) / 2;
            
	    status->af_mode               = data[16] & 0x0f;
	    status->zoom_mode             = (data[16] & 0x30)>>4;
            status->flash_charged         = data[18];
            status->compression_mode_id   = data[19];
            status->flash_mode            = data[20];
            status->exposure_compensation = (data[2]&0x40?-1:1) * (data[21] & 0x3f);
            status->light_value           = data[22];
            status->manual_exposure       = data[23];
            status->exposure_time = (data[24] * 0x1000000 + 
				     data[25] * 0x10000 + 
				     data[26] * 0x100 + 
				     data[27]) / 2;
            status->shutter_delay         = data[29];
            status->memory_card           = data[30];
            status->front_cover           = data[31];
            status->date_format           = data[32];
            status->time_format           = data[33];
            status->distance_format       = data[34];
            status->taken_pict_mem        = data[36] * 0x100 + data[37];
            for( i=0; i<4; i++ ) {
		status->remaining_pic_mem[i]     = 
		    data[46+i*2] * 0x100 + 
		    data[47+i*2];
	    }
            status->taken_pict_card       = data[56] * 0x100 + data[57];
            for( i=0; i<4; i++ ) {
		status->remaining_pic_card[i]     = 
		    data[66+i*2] * 0x100 + 
		    data[67+i*2];
	    }
            strncpy( status->card_id,   data + 77, 32 ); 
            strncpy( status->camera_id, data + 90, 32 ); 
        }


	gp_file_free(file);
	free (p);

	return (retval);
}

int dc120_get_albums (Camera *camera, int from_card, CameraList *list, GPContext *context) {

	CameraFile *file;
	int x;
	int size;
	char *f;
	char *p = dc120_packet_new(0x44);
	const char *file_data;
	long unsigned int file_size;

	if (from_card)
		p[1] = 0x01;

	gp_file_new(&file);

	size = 256;
	if (dc120_packet_read_data(camera, file, p, &size, 256, context)==GP_ERROR) {
		gp_file_free(file);
		free (p);
		return GP_ERROR;
	}
	gp_file_get_data_and_size (file, &file_data, &file_size);

	for (x=0; x<8; x++) {
		f = (char*)&file_data[x*15];
		if (strlen(f)>0)
			gp_list_append(list, f, NULL);
	}

	gp_file_free(file);
	free (p);

	return (GP_OK);
}

int dc120_get_filenames (Camera *camera, int from_card, int album_number, CameraList *list, GPContext *context) 
{
	CameraFile *file;
	int x;
	int size;
	char *p;
	char buf[16];
	const char *file_data;
	long unsigned int file_size;

	/* --- now read the files --- */

	p = dc120_packet_new(0x4A);

	if (from_card)
		p[1] = 0x01;

	/* Set the album number */
	p[4] = album_number;

	gp_file_new(&file);
	size = 256; /* packet_read_data has special handling for size */
	if (dc120_packet_read_data(camera, file, p, &size, size, context)==GP_ERROR) {
		gp_file_free(file);
		free (p);
		return (GP_ERROR);
	}


	/* extract the filenames from the packet data */
	gp_file_get_data_and_size (file, &file_data, &file_size);
	x=2;
	while (x < size) {
	        if( file_data[x] != '\0' ) {
		    strncpy(buf, (char*) &file_data[x], 11);
		    buf[7] = '.';
		    buf[11] = '\0';
 		    gp_list_append(list, buf, NULL);
		}
		x += 20;
	}

	/* be a good boy and free up our stuff */
	gp_file_free(file);
	free (p);

	return (GP_OK);
}

static int dc120_get_file_preview (Camera *camera, CameraFile *file, int file_number, char *cmd_packet, int *size, GPContext *context) {

	CameraFile *f;
	int x;
	char buf[16];
	const char *f_data;
	long unsigned int f_size;

	*size = 15680;

	/* Create a file for KDC data */
	gp_file_new(&f);
	if (dc120_packet_read_data(camera, f, cmd_packet, size, 1024, context)==GP_ERROR) {
		gp_file_free(f);
		return (GP_ERROR);
	}
	/* Convert to PPM file for now */
	gp_file_append(file, "P3\n80 60\n255\n", 13);

	gp_file_get_data_and_size (f, &f_data, &f_size);

	for (x=0; x<14400; x+=3) {
		sprintf(buf, "%i %i %i\n",
			(unsigned char)f_data[x+1280],
			(unsigned char)f_data[x+1281],
			(unsigned char)f_data[x+1282]);
		gp_file_append(file, buf, strlen(buf));
	}

	gp_file_free(f);
	/* sleep(1); */ /* FIXME: why?  ... Marcus: removed it for now.*/
	return (GP_OK);
}

static int dc120_get_file (Camera *camera, CameraFile *file, int file_number, char *cmd_packet, int *size, GPContext *context) 
{
        CameraFile *size_file; /* file used to determine the filesize */
  	char *p;
	const char *file_data;
	long unsigned int file_size;
	int offset;

	/* --- first read the file size --- */
	p = dc120_packet_new(0x4A);

	/* Copy over location and album number */
	p[1] = cmd_packet[1];
	p[4] = cmd_packet[4];

	gp_file_new(&size_file);
	*size = 256; /* packet_read_data has special handling for size */
	if (dc120_packet_read_data(camera, size_file, p, size, *size, context)==GP_ERROR) {
		gp_file_free(size_file);
		free (p);
		return (GP_ERROR);
	}

	/* extract the filesize from the packet data */
	gp_file_get_data_and_size (size_file, &file_data, &file_size);

	offset = 2 + (file_number-1) * 20;
	if( file_size < offset+19 )
	  {
	    gp_file_free(size_file);
	    free (p);
	    return (GP_ERROR);
	  }
	
	*size = (unsigned char)file_data[offset + 16] * 256*256*256 + 
		(unsigned char)file_data[offset + 17] * 256*256 +
		(unsigned char)file_data[offset + 18] * 256 +
		(unsigned char)file_data[offset + 19];

	/* be a good boy and free up our stuff */
	gp_file_free(size_file);
	free (p);

	if (dc120_packet_read_data(camera, file, cmd_packet, size, 1024, context)==GP_ERROR)
	    return (GP_ERROR);
	

	return (GP_OK);
}

static int dc120_wait_for_completion (Camera *camera, GPContext *context) {

	char p[8];
	int retval;
	int x=0, done=0;
	unsigned int id;

	/* Wait for command completion */
	id = gp_context_progress_start (context, 25, _("Waiting for completion..."));
	while ((x++ < 25)&&(!done)) {

		retval = dc120_packet_read(camera, p, 1);
		switch (retval) {
		   case GP_ERROR: 
			return (GP_ERROR); 
			break;
		   case GP_ERROR_TIMEOUT: 
			break;
		   default:
			done = 1;
		}
		gp_context_progress_update (context, id, x);
	}
	gp_context_progress_stop (context, id);

	if (x==25)
		return (GP_ERROR);
	return (GP_OK);

}

static int dc120_delete_file (Camera *camera, char *cmd_packet, GPContext *context) {

	char p[8];

	if (dc120_packet_write(camera, cmd_packet, 8, 1) == GP_ERROR)
		return (GP_ERROR);
	
	if (dc120_packet_read(camera, p, 1)==GP_ERROR)
		return (GP_ERROR);	

	if (dc120_wait_for_completion(camera, context)==GP_ERROR)
		return (GP_ERROR);

	return (GP_OK);
}

int dc120_file_action (Camera *camera, int action, int from_card, int album_number, 
		int file_number, CameraFile *file, GPContext *context) {

	int retval;
	int size=0;
	char *p = dc120_packet_new(0x00);

	if (from_card)
		p[1] = 0x01;

	/* Set the picture number */
	p[2] = (file_number >> 8) & 0xFF;
	p[3] =  file_number & 0xFF;

	/* Set the album number */
	p[4] = album_number;

	switch (action) {
	   case DC120_ACTION_PREVIEW:
		p[0] = (from_card? 0x64 : 0x54);
		retval = dc120_get_file_preview(camera, file, file_number, p, &size, context);
		break;
	   case DC120_ACTION_IMAGE:
		p[0] = (from_card? 0x64 : 0x54);
		retval = dc120_get_file(camera, file, file_number, p, &size, context);
		break;
	   case DC120_ACTION_DELETE:
		p[0] = (from_card? 0x7B : 0x7A);
		retval = dc120_delete_file(camera, p, context);
		break;
	   default:
		retval = GP_ERROR;
	}
	free(p);
	return (retval);
}

int dc120_capture (Camera *camera, CameraFilePath *path, GPContext *context) 
{
    int   retval;
    char *p      = dc120_packet_new(0x77);
    
    /* Take the picture to Flash memory */
    if (dc120_packet_write(camera, p, 8, 1) == GP_ERROR) {
	retval = (GP_ERROR);
    }
    else if ( dc120_wait_for_completion(camera, context) == GP_ERROR ) {
	retval = (GP_ERROR);
    }
    else {
	retval = (GP_OK);
    }

    free( p );
    return retval;
}
