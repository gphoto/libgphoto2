#include <stdlib.h>
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

	int x=0, index=0, error=0;
	char in[2];

write_again:
	/* If retry, give camera some recup time */
	if (x > 0)
		GPIO_SLEEP(SLEEP_TIMEOUT);

	/* Return error if too many retries */
	if (x++ > RETRIES) 
		return (GP_ERROR);

	if (gpio_write(dd->dev, packet, size) < 0)
		goto write_again;

#if 0
	/* UPDATE: this is obsolete. timing issues resolved. -Scott */

	/* Write out the packet 1 byte at a time. (just like Sierra. weird.) */
	index = 0;
	while (index < size) {
		if (gpio_write(dd->dev, &packet[index++], 1) < 0)
			error = 1;
	}
	if (error) {
		error = 0;
		goto write_again;
	}
#endif
	/* Read in the response from the camera if requested */
	if (read_response) {
		if (gpio_read(dd->dev, in, 1) < 0)
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

	int retval = gpio_read(dd->dev, packet, size);

	return (retval < 0? GP_ERROR : retval);
}

int dc120_packet_read_data (DC120Data *dd, CameraFile *file, char *cmd_packet, int *size, int block_size) {

	/* Reads in multi-packet data, appending it to the "file". */

	int num_packets, num_bytes, retval;
	int x=0, index=0, retries=0;
	char packet[2048], p[8];

	if (*size > 0)
		/* Size is specified */
		num_packets = *size / (block_size + 1) + 1;
	  else
		/* Give it 5 packets to determine size. otherwise, it's screwed */
		num_packets = 5;
read_data_write_again:
	if (dc120_packet_write(dd, cmd_packet, 8, 1) < 0) 
		return (GP_ERROR);

	while (x < num_packets) {
		retval = dc120_packet_read(dd, packet, block_size+1);
		switch (retval) {
		  case GPIO_ERROR:
		  case GPIO_TIMEOUT:
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
				if (x==1) {
					*size = ((unsigned char)packet[0] * 256 +
					         (unsigned char)packet[1])* 20;
					num_packets = *size / 256 + 1;
				}
				break;
			   case 0x64:
			   case 0x54:
				/* If thumbnail, cancel rest of image transfer */
				if ((num_packets == 16)&&(x==16))
					p[0] = CANCL;
				/* No break on purpose */
				gp_camera_progress(NULL, file, 100.0 * (float)((x-1)*block_size)/(float)(*size));
				break;
			   default:
				/* Nada */
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
// printf("x=%i num_packets=%i size=%i num_bytes=%i\n", x, num_packets, *size, num_bytes);
		}
	}

	if ((unsigned char)p[0] != CANCL)
		/* Read in command completed */
		dc120_packet_read(dd, p, 1);

	return (GP_OK);

}

int dc120_set_speed (DC120Data *dd, int speed) {

	char *p = dc120_packet_new(0x41);
	gpio_device_settings settings;

	gpio_get_settings(dd->dev, &settings);

	switch (speed) {
		case 9600:
			p[2] = 0x96;
			p[3] = 0x00;
			settings.serial.speed = 9600;
			break;
		case 19200:
			p[2] = 0x19;
			p[3] = 0x20;
			settings.serial.speed = 19200;
			break;
		case 38400:
			p[2] = 0x38;
			p[3] = 0x40;
			settings.serial.speed = 38400;
			break;
		case 57600:
			p[2] = 0x57;
			p[3] = 0x60;
			settings.serial.speed = 57600;
			break;
		case 0: /* Default */
		case 115200:
			p[2] = 0x11;
			p[3] = 0x52;
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

	if (gpio_set_settings (dd->dev, settings) == GP_ERROR)	
		return (GP_ERROR);

	free (p);

	GPIO_SLEEP(300);

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

int dc120_read_file_preview_data (DC120Data *dd, CameraFile *file, int file_number, char *cmd_packet, int *size) {

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

	GPIO_SLEEP(1000);
	return (GP_OK);
}

int dc120_read_file_data (DC120Data *dd, CameraFile *file, int file_number, char *cmd_packet, int *size) {

	CameraFile *f;
	char *p = dc120_packet_new(0x4A);
	int offset = 2 + (file_number - 1) * 20 + 16;

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
	gp_file_free(f);
	if (dc120_packet_read_data(dd, file, cmd_packet, size, 1024)==GP_ERROR)
		return (GP_ERROR);

	return (GP_OK);
}

int dc120_get_file (DC120Data *dd, int get_preview, int from_card, int album_number, 
		int file_number, CameraFile *file) {

	int retval;
	int size=0;
	char *p;

	p = dc120_packet_new(from_card? 0x64 : 0x54);

	if (from_card)
		p[1] = 0x01;

	/* Set the picture number */
	p[2] = (file_number >> 8) & 0xFF;
	p[3] = file_number & 0xFF;

	/* Set the album number */
	p[4] = album_number;

	if (get_preview)
		retval = dc120_read_file_preview_data(dd, file, file_number, p, &size);
	   else
		retval = dc120_read_file_data(dd, file, file_number, p, &size);

	free(p);
	return (retval);
}
