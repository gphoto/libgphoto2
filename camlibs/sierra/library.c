#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <gpio.h>
#include <time.h>
#include "sierra.h"
#include "library.h"

#define		QUICKSLEEP	5

void sierra_dump_packet (Camera *camera, char *packet) {

	int x, 	length=0;
	char buf[4096], msg[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

	return;

	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_DATA) ||
	    (packet[0] == TYPE_DATA_END)) {
                length = ((unsigned char)packet[2]) +
			 ((unsigned char)packet[3]  * 256);
		length += 6;
	} else {
		switch((unsigned char)packet[0]) {
			case NUL:
				strcpy(buf, "  packet: NUL");
				break;
			case ENQ:
				strcpy(buf, "  packet: ENQ");
				break;
			case ACK:
				strcpy(buf, "  packet: ACK");
				break;
			case DC1:
				strcpy(buf, "  packet: DC1");
				break;
			case NAK:
				strcpy(buf, "  packet: NAK (Camera Signature)");
				break;
			case TRM:
				strcpy(buf, "  packet: TRM");
				break;
			default:
				sprintf(buf, "  packet: 0x%02x (UNKNOWN!)", packet[0]);
		}
		sierra_debug_print(fd, buf);
		return;
	}
	sprintf(msg, "  packet length: %i", length);
	sierra_debug_print(fd, msg);
	if (length > 256)
		length = 256;
	strcpy(msg, "  packet: ");
	for (x=0; x<length; x++)
		sprintf(msg, "%s 0x%02x ", msg, (unsigned char)packet[x]);

	sierra_debug_print(fd, msg);

}

int sierra_valid_type(char b) {

	unsigned char byte = (unsigned char)b;

	if ((byte == NUL) ||
	    (byte == ENQ) ||
	    (byte == ACK) ||
	    (byte == DC1) ||
	    (byte == NAK) ||
	    (byte == TRM) ||
	    (byte == TYPE_COMMAND) ||
	    (byte == TYPE_DATA) ||
	    (byte == TYPE_DATA_END))
		return (GP_OK);
	return (GP_ERROR);
}

int sierra_valid_packet (Camera *camera, char *packet) {

	int length;
	SierraData *fd = (SierraData*)camera->camlib_data;

	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_DATA) ||
	    (packet[0] == TYPE_DATA_END)) {
                length = ((unsigned char)packet[2]) +
			 ((unsigned char)packet[3] * 256);
		length += 6;
	} else {
		switch((unsigned char)packet[0]) {
			case NUL:
			case ENQ:
			case ACK:
			case DC1:
			case NAK:
			case TRM:
				return (GP_OK);
				break;
			default:
				return (GP_ERROR);
		}
	}

	sierra_debug_print(fd, "VALID_PACKET NOT DONE FOR DATA PACKETS!");
	return (GP_ERROR);
}

int sierra_write_packet (Camera *camera, char *packet) {

	int x, ret, r, checksum=0, length;
	SierraData *fd = (SierraData*)camera->camlib_data;
	
	sierra_debug_print(fd, " sierra_write_packet");

//	usleep(QUICKSLEEP);

	/* Determing packet length */
	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_DATA) ||
	    (packet[0] == TYPE_DATA_END)) {
                length = ((unsigned char)packet[2]) +
			 ((unsigned char)packet[3]  * 256);
		length += 6;
	} else {
		length = 1;
	}

	/* Generate the checksum */
	if (length > 1) {
		for (x=4; x<length-2; x++)
			checksum += (unsigned char)packet[x];
		packet[length-2]= checksum & 0xff;
	        packet[length-1]= (checksum >> 8) & 0xff; 
	}

	sierra_dump_packet(camera, packet);

	/* For USB support */
	if (fd->type == GP_PORT_USB) {
		return (gpio_write(fd->dev, packet, length));
	}

	r=0;
	x=0;
	while (x<length) {
	        ret = gpio_write(fd->dev, &packet[x], 1);

		if (ret != GPIO_OK) {
			if (ret == GPIO_TIMEOUT) {
				sierra_debug_print(fd, " write timed out. trying again.");
				if (r++ > RETRIES) {
					sierra_debug_print(fd, " write failed (too many retries)");
					return (GP_ERROR);
				}
			} else 	if (ret == GPIO_ERROR) {
				sierra_debug_print(fd, " write failed");
		                return (GP_ERROR);
			} else  {
				sierra_debug_print(fd, " write failed (unknown error)");
			}
		} else {
			r=0;	/* reset retries */
			x++;	/* next byte */
		}
	}

	return (GP_OK);
}

int sierra_read_packet (Camera *camera, char *packet) {

	int y, r=0, ret, done, length=0;
	int blocksize, bytes_read;
	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

read_packet_again:
	buf[0] = 0;
	packet[0] = 0;

	sierra_debug_print(fd, " sierra_read_packet");

//	usleep(QUICKSLEEP);

	done = 0;
#ifdef GPIO_USB
	if (fd->type == GP_PORT_USB)
		gpio_usb_clear_halt(fd->dev);
#endif
	while (!done && (r++<RETRIES)) {

		switch (fd->type) {
#ifdef GPIO_USB
		   case GP_PORT_USB:
			blocksize = 2054;
			break;
#endif
		   case GP_PORT_SERIAL:
			blocksize = 1;
			break;
		   default:
			return (GP_ERROR);
		}
		bytes_read = gpio_read(fd->dev, packet, blocksize);


		if (bytes_read == GPIO_ERROR) {
			sierra_debug_print(fd, "  read error (packet type)");
			return (GP_ERROR);
		}
		if (sierra_valid_type(packet[0])==GP_OK)
			done = 1;
		if (r>RETRIES) {
			sierra_debug_print(fd, "  read error (too many retries on packet type)");
			return (GP_ERROR);
		}

		/* Determine the packet type */
		if ((packet[0] == TYPE_COMMAND) ||
		    (packet[0] == TYPE_DATA) ||
		    (packet[0] == TYPE_DATA_END)) {
			/* It's a response/command packet */
			if (fd->type == GP_PORT_SERIAL) {
				bytes_read = gpio_read(fd->dev, &packet[1], 3);
				if (bytes_read == GPIO_ERROR) {
					sierra_debug_print(fd, "  read error (header)");
					return (GP_ERROR);
				}
				bytes_read = 4;
			}
			/* Determine the packet length */
        	        length = ((unsigned char)packet[2]) +
				 ((unsigned char)packet[3]  * 256);
			length += 6;
		} else {
			/* It's a single byte response. dump and validate */
			sierra_dump_packet(camera, packet);
			return (sierra_valid_packet(camera, packet));
		}
		for (y=bytes_read; y < length; y+=blocksize) {
			ret = gpio_read(fd->dev, &packet[y], blocksize);
			if (ret == GPIO_TIMEOUT) {
				sierra_write_nak(camera);
				goto read_packet_again;
			}

			if (ret ==GPIO_ERROR)
				return (GP_ERROR);
		}
	}

	sierra_dump_packet(camera, packet);
#ifdef GPIO_USB
	if (fd->type == GP_PORT_USB)
		gpio_usb_clear_halt(fd->dev);
#endif
	return (GP_OK);

}

int sierra_build_packet (Camera *camera, char type, char subtype, int data_length, char *packet) {

	SierraData *fd = (SierraData*)camera->camlib_data;

	packet[0] = type;
	switch (type) {
		case TYPE_COMMAND:
			if (fd->type == GP_PORT_USB)
				/* USB cameras don't care about first packets */
				fd->first_packet = 0;
			if (fd->first_packet)
				packet[1] = SUBTYPE_COMMAND_FIRST;
			   else
				packet[1] = SUBTYPE_COMMAND;
			fd->first_packet = 0;
			break;
		case TYPE_DATA:
		case TYPE_DATA_END:
			packet[1] = subtype;
			break;
		default:
			sierra_debug_print(fd, "unknown packet type");
	}

	packet[2] = data_length &  0xff;
	packet[3] = (data_length >> 8) & 0xff;

	return (GP_OK);
}

int sierra_read_ack(Camera *camera) {

	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

	sierra_debug_print(fd, " sierra_read_ack");

	if (sierra_read_packet(camera, buf)==GP_ERROR) {
		sierra_debug_print(fd, "Could not read ACK");
		return (GP_ERROR);
	}

	if (buf[0] == ACK)
		return (GP_OK);

	sierra_debug_print(fd, "Could not read ACK");
	return (GP_ERROR);
}


int sierra_write_ack(Camera *camera) {

	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

	sierra_debug_print(fd, " sierra_write_ack");

	buf[0] = ACK;
	if (sierra_write_packet(camera, buf)==GP_OK) {
#ifdef GPIO_USB
		gpio_usb_clear_halt(fd->dev);
#endif
		return (GP_OK);
	}
	sierra_debug_print(fd, "Could not write ACK");
#ifdef GPIO_USB
	gpio_usb_clear_halt(fd->dev);
#endif
	return (GP_ERROR);
}

int sierra_write_nak(Camera *camera) {

	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

	sierra_debug_print(fd, " sierra_write_nak");

	buf[0] = NAK;
	if (sierra_write_packet(camera, buf)==GP_OK) {
#ifdef GPIO_USB
		gpio_usb_clear_halt(fd->dev);
#endif
		return (GP_OK);
	}

	sierra_debug_print(fd, "Could not write NAK");
#ifdef GPIO_USB
	gpio_usb_clear_halt(fd->dev);
#endif
	return (GP_ERROR);
}

int sierra_ping(Camera *camera) {

	int r=0;
	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

        buf[0] = NUL;

	while (r++ < RETRIES) {
		sierra_debug_print(fd, "pinging the camera");
		if (sierra_write_packet(camera, buf)==GP_ERROR)
			return (GP_ERROR);

		if (sierra_read_packet(camera, buf)==GP_ERROR)
			return (GP_ERROR);

		if (buf[0] == NAK) {
			sierra_debug_print(fd, "ping succeeded");
			return (GP_OK);
		}
		sierra_debug_print(fd, "ping failed");
	}
	return (GP_ERROR);
}

int sierra_set_speed (Camera *camera, int speed) {

	gpio_device_settings settings;
	char buf[1024];
	SierraData *fd = (SierraData*)camera->camlib_data;

	sprintf(buf, "Setting speed to %i", speed);
	sierra_debug_print(fd, buf);

	fd->first_packet = 1;

	gpio_get_settings(fd->dev, &settings);

	switch (speed) {
		case 9600:
			speed = 1;
			settings.serial.speed = 9600;
			break;
		case 19200:
			speed = 2;
			settings.serial.speed = 19200;
			break;
		case 38400:
			speed = 3;
			settings.serial.speed = 38400;
			break;
		case 57600:
			speed = 4;
			settings.serial.speed = 57600;
			break;
		case 0:		/* Default speed */
		case 115200:
			speed = 5;
			settings.serial.speed = 115200;
			break;

		case -1:	/* End session */
			settings.serial.speed = 19200;
			speed = 2;
			break;
		default:
			return (GP_ERROR);
	}

	if (sierra_set_int_register(camera, 17, speed)==GP_ERROR)
		return (GP_ERROR);
	if (gpio_set_settings(fd->dev, settings)==GPIO_ERROR)
		return (GP_ERROR);

	GPIO_SLEEP(10);
	return (GP_OK);
}

int sierra_set_int_register (Camera *camera, int reg, int value) {

	int r=0;
	char p[4096];
	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

	sprintf(buf, "Setting register #%i to %i", reg, value);
	sierra_debug_print(fd, buf);

	if (value < 0)
		sierra_build_packet(camera, TYPE_COMMAND, 0, 2, p);
	   else
		sierra_build_packet(camera, TYPE_COMMAND, 0, 6, p);

        /* Fill in the data */
        p[4] = 0x00;
        p[5] = reg;
	if (value >= 0) {
		p[6] = (value)     & 0xff;
		p[7] = (value>>8)  & 0xff;
		p[8] = (value>>16) & 0xff;
		p[9] = (value>>24) & 0xff;
	}

	while (r++<RETRIES) {
		if (sierra_write_packet(camera, p)==GP_ERROR)
			return (GP_ERROR);

		if (sierra_read_packet(camera, buf)==GP_ERROR)
			return (GP_ERROR);

		if (buf[0] == ACK)
			return (GP_OK);

		/* DC1 = invalid register or value */
		if (buf[0] == DC1)
			return (GP_ERROR);
	}

	sierra_debug_print(fd, "too many retries");

	sierra_set_speed(camera, -1);

	return (GP_ERROR);
}

int sierra_get_int_register (Camera *camera, int reg, int *value) {

	int r=0, write_nak=0;
	char packet[4096];
	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

	sprintf(buf, "Getting register #%i value", reg);
	sierra_debug_print(fd, buf);

	sierra_build_packet(camera, TYPE_COMMAND, 0, 2, packet);

        /* Fill in the data */
	packet[4] = 0x01;
	packet[5] = reg;

	while (r++<RETRIES) {
		if (write_nak) {
			if (sierra_write_nak(camera)==GP_ERROR)
				return (GP_ERROR);
		}  else {
			if (sierra_write_packet(camera, packet)==GP_ERROR)
				return (GP_ERROR);
		}

		if (sierra_read_packet(camera, buf)==GP_ERROR)
			return (GP_ERROR);

		/* DC1 = invalid register or value */
		if (buf[0] == DC1)
			return (GP_ERROR);
		if (buf[0] == TYPE_DATA_END) {
			sierra_write_ack(camera);
			r =((unsigned char)buf[4]) +
			   ((unsigned char)buf[5] * 256) +
			   ((unsigned char)buf[6] * 65536) +
			   ((unsigned char)buf[7] * 16777216);
			*value = r;
			return (GP_OK);
		} else {
			write_nak = 1;
		}
	}

	sierra_debug_print(fd, "too many retries");
	return (GP_ERROR);
}

int sierra_set_string_register (Camera *camera, int reg, char *s, int length) {

	char packet[4096], buf[4096];
	char type;
	unsigned char c;
	int x=0, seq=0, size=0, done, r;
	SierraData *fd = (SierraData*)camera->camlib_data;

	sprintf(buf, "Setting string in register #%i to \"%s\"", reg, s);
	sierra_debug_print(fd, buf);

	while (x < length) {
		if (x==0) {
			type = TYPE_COMMAND;
			size = (length+2-x)>2048? 2048 : length+2;
		}  else {
			size = (length-x)>2048? 2048 : length;
			if (x+size < length)
				type = TYPE_DATA;
			   else
				type = TYPE_DATA_END;
		}
		sierra_build_packet(camera, type, seq, size, packet);

		if (type == TYPE_COMMAND) {
			packet[4] = 0x03;
			packet[5] = reg;
			memcpy(&packet[6], &s[x], size-2);
			x += size - 2;
		} else  {
			packet[1] = seq++;
			memcpy(&packet[4], &s[x], size);
			x += size;
		}

		r = 0; done = 0;
		while ((r++<RETRIES)&&(!done)) {
			if (sierra_write_packet(camera, packet)==GP_ERROR)
				return (GP_ERROR);

			if (sierra_read_packet(camera, buf)==GP_ERROR)
				return (GP_ERROR);

			c = (unsigned char)buf[0];
			if (c == DC1)
				return (GP_ERROR);
			if (c == ACK)
				done = 1;
			   else	{
				if (c != NAK)
					return (GP_ERROR);
			}

		}
		if (r > RETRIES) {
			sierra_debug_print(fd, "too many NAKs from camera");
			return (GP_ERROR);
		}
	}
	return (GP_OK);
}

int sierra_get_string_register (Camera *camera, int reg, int file_number, 
				 CameraFile *file, char *s, int *length) {

	char packet[4096], buf[2048];
	int done, x, packlength, do_percent, l;
	SierraData *fd = (SierraData*)camera->camlib_data;

	sprintf(buf, "Getting string in register #%i", reg);
	sierra_debug_print(fd, buf);

	do_percent = 1;
	/* Set the current picture number */
	if (file_number >= 0) {
	        if (sierra_set_int_register(camera, 4, file_number)==GP_ERROR)
	                return (GP_ERROR);
	}

	switch (reg) {
		case 14:
		        /* Get the size of the current image */
	        	if (sierra_get_int_register(camera, 12, &l)==GP_ERROR)
	        	        return (GP_ERROR);

			break;
		case 15:
		        /* Get the size of the current thumbnail */
	        	if (sierra_get_int_register(camera, 13, &l)==GP_ERROR)
	        	        return (GP_ERROR);
			break;
		case 44:
		        /* Get the size of the current audio */
			/* will do */
			break;
		default:
			do_percent = 0;
	}

	/* Send request */
	sierra_build_packet(camera, TYPE_COMMAND, 0, 2, packet);
	packet[4] = 0x04;
	packet[5] = reg;
	if (sierra_write_packet(camera, packet)==GP_ERROR)
		return (GP_ERROR);

	/* Read all the data packets */
	x=0; done=0;
	while (!done) {
		if (sierra_read_packet(camera, packet)==GP_ERROR)
			return (GP_ERROR);
		if (packet[0] == DC1)
			return (GP_ERROR);
		if (sierra_write_ack(camera)==GP_ERROR)
			return (GP_ERROR);
		packlength = ((unsigned char)packet[2]) +
			     ((unsigned char)packet[3]  * 256);

		if (s) {
			memcpy(&s[x], &packet[4], packlength);
		}
		if (file) {
		/* How to support chunk image data transfers 		*/
		/* ==================================================== */

		/* 1) Use gp_file_append to write a chunk of new data   */
		/*    to the CameraFile struct. This will automatcially */
		/*    update file->size.				*/

			gp_file_append(file, &packet[4], packlength);

		/* 2) Call gp_frontend_progress to let the front-end know */
		/*    the current transfer status. The front-end has 	*/
		/*    the option of reading the data that was just	*/
		/*    transferred by using gp_file_chunk		*/

			if (do_percent)
			   gp_frontend_progress(camera, file, (float)(100.0*(float)x/(float)(l)));
		}

		x += packlength;
		if (packet[0] == TYPE_DATA_END)
			done = 1;
	}
	if (length)
		*length = x;
	return (GP_OK);
}

int sierra_delete(Camera *camera, int picture_number) {

	char packet[4096], buf[4096];
	int r, done;
	SierraData *fd = (SierraData*)camera->camlib_data;

       	if (sierra_set_int_register(camera, 4, picture_number)==GP_ERROR) {
		sierra_debug_print(fd, "Can not set current image");
       	        return (GP_ERROR);
        }

	sierra_build_packet(camera, TYPE_COMMAND, 0, 3, packet);
	packet[4] = 0x02;
	packet[5] = 0x07;
	packet[6] = 0x00;

	r=0; done=0;
	while ((!done)&&(r++<RETRIES)) {
		if (sierra_write_packet(camera, packet)==GP_ERROR)
			return (GP_ERROR);
	
		if (sierra_read_packet(camera, buf)==GP_ERROR)
			return (GP_ERROR);

		done = (buf[0] == NAK)? 0 : 1;

		if (done) {
			/* read in the ENQ */
			if (sierra_read_packet(camera, buf)==GP_ERROR)
				return ((buf[0]==ENQ)? GP_OK : GP_ERROR);
			
		}
	}
	if (r > RETRIES) {
		sierra_debug_print(fd, "too many NAKs from camera");
		return (GP_ERROR);
	}

	GPIO_SLEEP(QUICKSLEEP);

	return (GP_OK);
}

int sierra_end_session(Camera *camera) {

	char packet[4096], buf[4096];
	unsigned char c;
	int r, done;
	SierraData *fd = (SierraData*)camera->camlib_data;

	sierra_build_packet(camera, TYPE_COMMAND, 0, 3, packet);
	packet[4] = 0x02;
	packet[5] = 0x04;
	packet[6] = 0x00;

	r=0; done=0;
	while ((!done)&&(r++<RETRIES)) {
		if (sierra_write_packet(camera, packet)==GP_ERROR)
			return (GP_ERROR);
	
		if (sierra_read_packet(camera, buf)==GP_ERROR)
			return (GP_ERROR);

		c = (unsigned char)buf[0];
		if (c==TRM)
			return (GP_OK);

		done = (c == NAK)? 0 : 1;

		if (done) {
			/* read in the ENQ */
			if (sierra_read_packet(camera, buf)==GP_ERROR)
				return (GP_ERROR);
			c = (unsigned char)buf[0];
			return ((c==ENQ)? GP_OK : GP_ERROR);
		}
	}
	if (r > RETRIES) {
		sierra_debug_print(fd, "too many NAKs from camera");
		return (GP_ERROR);
	}

	return (GP_OK);
}

int sierra_file_count (Camera *camera) {
 
        int value;
        SierraData *fd = (SierraData*)camera->camlib_data;
 
        sierra_debug_print(fd, "Counting files");
 
        if (sierra_get_int_register(camera, 10, &value)==GP_ERROR)
                return (GP_ERROR);
 
        return (value);
} 


int sierra_capture (Camera *camera, CameraFile *file) {

	SierraData *fd = (SierraData*)camera->camlib_data;
	int r, done, retval, picnum;
	char packet[4096], buf[8];
	unsigned char c;
	struct tm *t;
	time_t tt;

	/* Take a picture */
	sierra_build_packet(camera, TYPE_COMMAND, 0, 3, packet);
	packet[4] = 0x02;
	packet[5] = 0x02;
	packet[6] = 0x00;

#if 0
	/* Take a quick preview */
	sierra_build_packet(camera, TYPE_COMMAND, 0, 3, packet);
	packet[4] = 0x02;
	packet[5] = 0x05;
	packet[6] = 0x00;
#endif

	r=0; done=0;
	while ((!done)&&(r++<RETRIES)) {
		if (sierra_write_packet(camera, packet)==GP_ERROR)
			return (GP_ERROR);
	
		if (sierra_read_packet(camera, buf)==GP_ERROR)
			return (GP_ERROR);

		c = (unsigned char)buf[0];
		if (c==TRM)
			return (GP_OK);

		done = (c == NAK)? 0 : 1;

		gp_frontend_progress(camera, file, 0.0);
	}
	if (r >= RETRIES) {
		sierra_debug_print(fd, "too many NAKs from camera");
		return (GP_ERROR);
	}
	r = 0;done=0;
	while ((!done)&&(r++<RETRIES)) {
		/* read in the ENQ */
		retval = gpio_read(fd->dev, buf, 1);
		switch(retval) {
		   case GPIO_ERROR:
			return (GP_ERROR);
			break;
		   case GPIO_TIMEOUT:
			break;
		   default:
			done = 1;
		}
		c = (unsigned char)buf[0];
		gp_frontend_progress(camera, file, 0.0);
	}
	if ((r >= RETRIES) || (c!=ENQ))
		return (GP_ERROR);

	/* After picture is taken, register 4 is set to current picture */
	if (sierra_get_int_register(camera, 4, &picnum)==GP_ERROR)
		return (GP_ERROR);

	/* Retrieve the just-taken picture */
	if (sierra_get_string_register(camera, 14, picnum, file, NULL, NULL)==GP_ERROR)
                return (GP_ERROR);

	/* Delete the just-taken picture */
	if (sierra_delete(camera, picnum)==GP_ERROR)
		return (GP_ERROR);
#if 0
	/* Retrieve the quick preview */
	if (sierra_get_string_register(camera, 14, 0, file, NULL, NULL)==GP_ERROR)
                return (GP_ERROR);
#endif

	tt = time(&tt);
	t = gmtime(&tt);

	/* fix it :P */
	if (t->tm_year < 1900)
		t->tm_year += 1900;

	sprintf(file->name, "%04i%02i%02i-%02i%02i%02i.jpg", 
			t->tm_year, t->tm_mon, t->tm_mday, 
			t->tm_hour, t->tm_min, t->tm_sec);

	return (GP_OK);
}
