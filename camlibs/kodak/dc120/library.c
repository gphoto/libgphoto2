#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>

#include "dc120.h"
#include "library.h"


char *dc120_packet_new (int command_byte) {

	char *p = (char *)malloc(sizeof(char) * 8);

	memset (p, 0, 8);

	p[0] = command_byte;
	p[7] = 0x1a;

	return p;
}

int dc120_packet_write (DC120Data *dd, char *packet, int size, int read_response) {

	/* Writes the packet and returns the result */

	int x=0;
	char in[2];

write_again:
	/* If retry, give camera some recup time */
	if (x > 0)
		GP_SYSTEM_SLEEP(SLEEP_TIMEOUT);

	/* Return error if too many retries */
	if (x++ > RETRIES) 
		return (GP_ERROR);

	if (gp_port_write(dd->dev, packet, size) < 0)
		goto write_again;

	/* Read in the response from the camera if requested */
	if (read_response) {
		if (gp_port_read(dd->dev, in, 1) < 0)
			/* On error, write again */
			goto write_again;

		if ((unsigned char)in[0] >= 0xe0) {
			/* If a single byte response, don't rewrite */
			if (size == 1)
				return (GP_OK);
			/* Got error response. Go again. */
			goto write_again;
		}
	}

	return (GP_OK);
}

int dc120_packet_read (DC120Data *dd, char *packet, int size) {

	return (gp_port_read(dd->dev, packet, size));
}

int dc120_packet_read_data (DC120Data *dd, CameraFile *file, char *cmd_packet, int *size, int block_size) {

	/* Reads in multi-packet data, appending it to the "file". */

	int num_packets=1, num_bytes, retval;
	int x=0, retries=0;
	float t;
	char packet[2048], p[8];

	/* Determine number of packets needed on-the-fly */
	if (*size > 0) {
		t = (float)*size / (float)(block_size);
		num_packets = (int)t;
		/* If remainder, we need another packet */
		if (t - (float)num_packets > 0)
			num_packets++;
	} else {
		num_packets = 5;
	}

read_data_write_again:
	if (dc120_packet_write(dd, cmd_packet, 8, 1) < 0) 
		return (GP_ERROR);

	while (x < num_packets) {
		retval = dc120_packet_read(dd, packet, block_size+1);
		switch (retval) {
		  case GP_ERROR:
		  case GP_ERROR_TIMEOUT:
			/* Write that packet was bad and resend */
			if (retries++ > RETRIES)
				return (GP_ERROR);
			if (x==0) {
				/* If first packet didn't come, write command again */
				goto read_data_write_again;
			} else {
				/* Didn't get data packet. Send retry */
				p[0] = PACK0;
				if (dc120_packet_write(dd, p, 1, 0)== GP_ERROR)
					return (GP_ERROR);
			}
			break;
		  default: 		
			x++;

			/* Default is packet was OK */
			p[0] = PACK1;

			/* Do some command-specific stuff */
			switch (cmd_packet[0]) {
			   case 0x4A:
				/* Set size and num_packets for camera filenames*/
				if (x==1)
					*size = ((unsigned char)packet[0] * 256 +
					         (unsigned char)packet[1])* 20 + 2;
					t = (float)*size / (float)(block_size);
					num_packets = (int)t;
					/* If remainder, we need another packet */
					if (t - (float)num_packets > 0)
						num_packets++;
				break;
			   case 0x64:
			   case 0x54:
				/* If thumbnail, cancel rest of image transfer */
				if ((num_packets == 16)&&(x==16))
					p[0] = CANCL;
				/* No break on purpose */
				gp_frontend_progress(NULL, file, (float)(100.0 * (float)((x-1)*block_size)/(float)(*size)));
				break;
			   default:
				/* Nada */
				break;
			}

			/* Write response */
			if (dc120_packet_write(dd, p, 1, 0)== GP_ERROR)
				return (GP_ERROR);

			/* Copy in data */
			if (x == num_packets)
				num_bytes = *size - ((num_packets-1) * block_size);
			   else
				num_bytes = block_size;
			gp_file_append(file, packet, num_bytes);
		}
	}

	if ((unsigned char)p[0] != CANCL)
		/* Read in command completed */
		dc120_packet_read(dd, p, 1);

	return (GP_OK);

}

int dc120_set_speed (DC120Data *dd, int speed) {

	char *p = dc120_packet_new(0x41);
	gp_port_settings settings;

	gp_port_settings_get(dd->dev, &settings);

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
			return (GP_ERROR);
	}

	if (dc120_packet_write(dd, p, 8, 1) == GP_ERROR)
		return (GP_ERROR);

	if (gp_port_settings_set (dd->dev, settings) == GP_ERROR)
		return (GP_ERROR);

	free (p);

	GP_SYSTEM_SLEEP(300);

	/* Speed change went OK. */
	return (GP_OK);
}

int dc120_get_status (DC120Data *dd) {

	CameraFile *file = gp_file_new();
	char *p = dc120_packet_new(0x7F);
 	int retval;
	int size = 256;

	retval = dc120_packet_read_data(dd, file, p, &size, 256);

	gp_file_free(file);
	free (p);

	return (retval);
}

int dc120_get_albums (DC120Data *dd, int from_card, CameraList *list) {

	CameraFile *file;
	int x;
	int size = 256;
	char *f;
	char *p = dc120_packet_new(0x44);

	if (from_card)
		p[1] = 0x01;

	file = gp_file_new();

	if (dc120_packet_read_data(dd, file, p, &size, 256)==GP_ERROR) {
		gp_file_free(file);
		free (p);
	}

	for (x=0; x<8; x++) {
		f = &file->data[x*15];
		if (strlen(f)>0)
			gp_list_append(list, f, GP_LIST_FOLDER);
	}

	gp_file_free(file);
	free (p);

	return (GP_OK);
}

int dc120_get_filenames (DC120Data *dd, int from_card, int album_number, CameraList *list) {

	CameraFile *file;
	char *f;
	int x=0, size=0;
	char *p = dc120_packet_new(0x4A);
	char buf[16];

	if (from_card)
		p[1] = 0x01;

	/* Set the album number */
	p[4] = album_number;

	file = gp_file_new();
	if (dc120_packet_read_data(dd, file, p, &size, 256)==GP_ERROR) {
		gp_file_free(file);
		free (p);
	}

	/* extract the filenames from the packet data */
	x=2;
	while (x < size) {
		f=&file->data[x];
		strncpy(buf, f, 7);
		buf[7] = 0;
		strcat(buf, ".kdc");
		gp_list_append(list, buf, GP_LIST_FILE);
		x += 20;
	}

	/* be a good boy and free up our stuff */
	gp_file_free(file);
	free (p);

	return (GP_OK);
}

int dc120_get_file_preview (DC120Data *dd, CameraFile *file, int file_number, char *cmd_packet, int *size) {

	CameraFile *f;
	int x;
	char buf[16];

	*size = 15680;

	/* Create a file for KDC data */
	f = gp_file_new();
	if (dc120_packet_read_data(dd, f, cmd_packet, size, 1024)==GP_ERROR) {
		gp_file_free(file);
		return (GP_ERROR);
	}
	/* Convert to PPM file for now */
	gp_file_append(file, "P3\n80 60\n255\n", 13);
	for (x=0; x<14400; x+=3) {
		sprintf(buf, "%i %i %i\n",
			(unsigned char)f->data[x+1280],
			(unsigned char)f->data[x+1281],
			(unsigned char)f->data[x+1282]);
		gp_file_append(file, buf, strlen(buf));
	}

	GP_SYSTEM_SLEEP(1000);
	return (GP_OK);
}

int dc120_get_file (DC120Data *dd, CameraFile *file, int file_number, char *cmd_packet, int *size) {

	CameraFile *f;
	char *p = dc120_packet_new(0x4A);
	int offset = 2 + (file_number-1) * 20 + 16;

	/* Copy over location and album number */
	p[1] = cmd_packet[1];
	p[4] = cmd_packet[4];

	f = gp_file_new();
	*size = 0;
	if (dc120_packet_read_data(dd, f, p, size, 256)==GP_ERROR) {
		gp_file_free(file);
		return (GP_ERROR);
	}
	*size = (unsigned char)f->data[offset]     * 16777216 + 
		(unsigned char)f->data[offset + 1] * 65536 +
		(unsigned char)f->data[offset + 2] * 256 +
		(unsigned char)f->data[offset + 3];

	if (dc120_packet_read_data(dd, file, cmd_packet, size, 1024)==GP_ERROR)
		return (GP_ERROR);
	gp_file_free(f);

	return (GP_OK);
}

int dc120_wait_for_completion (DC120Data *dd) {

	char p[8];
	int retval;
	int x=0, done=0;

	/* Wait for command completion */
	while ((x++ < 25)&&(!done)) {
		retval = dc120_packet_read(dd, p, 1);
		switch (retval) {
		   case GP_ERROR: 
			return (GP_ERROR); 
			break;
		   case GP_ERROR_TIMEOUT: 
			break;
		   default:
			done = 1;
		}
		gp_frontend_progress(NULL, NULL, (float)(100.0*(float)x/25.0));
	}

	if (x==25)
		return (GP_ERROR);
	return (GP_OK);

}

int dc120_delete_file (DC120Data *dd, char *cmd_packet) {

	char p[8];

	if (dc120_packet_write(dd, cmd_packet, 8, 1) == GP_ERROR)
		return (GP_ERROR);
	
	if (dc120_packet_read(dd, p, 1)==GP_ERROR)
		return (GP_ERROR);	

	if (dc120_wait_for_completion(dd)==GP_ERROR)
		return (GP_ERROR);

	return (GP_OK);
}

int dc120_file_action (DC120Data *dd, int action, int from_card, int album_number, 
		int file_number, CameraFile *file) {

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
		retval = dc120_get_file_preview(dd, file, file_number, p, &size);
		break;
	   case DC120_ACTION_IMAGE:
		p[0] = (from_card? 0x64 : 0x54);
		retval = dc120_get_file(dd, file, file_number, p, &size);
		break;
	   case DC120_ACTION_DELETE:
		p[0] = (from_card? 0x7B : 0x7A);
		retval = dc120_delete_file(dd, p);
		break;
	   default:
		retval = GP_ERROR;
	}
	free(p);
	return (retval);
}

int dc120_capture (DC120Data *dd, CameraFile *file) {

	CameraList *list;
	CameraListEntry *entry;
	char *cmd_packet = dc120_packet_new(0x77);
	int count;

	/* Take the picture to Flash memory */
	gp_frontend_message (NULL, "Taking picture...");
	if (dc120_packet_write(dd, cmd_packet, 8, 1) == GP_ERROR)
		return (GP_ERROR);

	gp_frontend_message (NULL, "Waiting for completion...");
	if (dc120_wait_for_completion(dd)==GP_ERROR)
		return (GP_ERROR);

	/* Get the last picture in the Flash memory */
	list = gp_list_new();
	dc120_get_filenames (dd, 0, 0, list);
	count = gp_list_count(list);
	entry = gp_list_entry(list, count-1);
	strcpy(file->name, entry->name);

	/* Download it */
	dc120_file_action (dd, DC120_ACTION_IMAGE, 0, 0, count, file);

	gp_list_free(list);

	return (GP_OK);
}
