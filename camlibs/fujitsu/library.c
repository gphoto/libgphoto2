#include <unistd.h>
#include <gphoto2.h>
#include <gpio.h>
#include "fujitsu.h"
#include "library.h"

int glob_first_packet = 1;

#define		QUICKSLEEP	5000

void fujitsu_dump_packet (char *packet) {

	int x, 	length=0;
	char buf[4096], msg[4096];

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
		debug_print(buf);
		return;
	}
	sprintf(msg, "  packet length: %i", length);
	debug_print(msg);
	if (length > 256)
		length = 256;
	strcpy(msg, "  packet: ");
	for (x=0; x<length; x++)
		sprintf(msg, "%s 0x%02x ", msg, (unsigned char)packet[x]);

	debug_print(msg);

}

int fujitsu_valid_type(char b) {

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

int fujitsu_valid_packet (char *packet) {

	int length;

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

	debug_print("VALID_PACKET NOT DONE FOR DATA PACKETS!");
	return (GP_ERROR);
}

int fujitsu_write_packet (gpio_device *dev, char *packet) {

	int x, ret, r, checksum=0, length;
	char buf[4096], p[4096];
	
	debug_print(" fujitsu_write_packet");

	usleep(QUICKSLEEP);

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

	fujitsu_dump_packet(packet);
	r=0;
	x=0;
	while (x<length) {
	        ret = gpio_write(dev, &packet[x], 1);

		if (ret != GPIO_OK) {
			if (ret == GPIO_TIMEOUT) {
				debug_print(" write timed out. trying again.");
				if (r++ > RETRIES) {
					debug_print(" write failed (too many retries)");
					return (GP_ERROR);
				}
			} else 	if (ret == GPIO_ERROR) {
				debug_print(" write failed");
		                return (GP_ERROR);
			} else  {
				debug_print(" write failed (unknown error)");
			}
		} else {
			r=0;	/* reset retries */
			x++;	/* next byte */
		}
	}

	return (GP_OK);
}

int fujitsu_read_packet (gpio_device *dev, char *packet) {

	int x, y, r=0, ret, done, length=0;
	char buf[4096], msg[4096];

read_packet_again:
	buf[0] = 0;
	packet[0] = 0;

	debug_print(" fujitsu_read_packet");

	usleep(QUICKSLEEP);

	done = 0;
	while (!done && (r++<RETRIES)) {
		ret = gpio_read(dev, packet, 1);
		if (ret == GPIO_ERROR) {
			debug_print("  read error (packet type)");
			return (GP_ERROR);
		}
		if (fujitsu_valid_type(packet[0])==GP_OK)
			done = 1;
	}
	if (r>RETRIES) {
		debug_print("  read error (too many retries on packet type)");
		return (GP_ERROR);
	}

	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_DATA) ||
	    (packet[0] == TYPE_DATA_END)) {
		if (gpio_read(dev, &packet[1], 3)==GPIO_ERROR) {
			debug_print("  read error (header)");
			return (GP_ERROR);
		}
                length = ((unsigned char)packet[2]) +
			 ((unsigned char)packet[3]  * 256);
		length += 6;
	} else {
		fujitsu_dump_packet(packet);
		return (fujitsu_valid_packet(packet));
	}

	for (y=4; y < length; y++) {
		ret = gpio_read(dev, &packet[y], 1);
		if (ret == GPIO_TIMEOUT) {
			sprintf(msg, "   timeout! (%i)\n", y);
			debug_print(msg);
			fujitsu_write_nak(dev);
			goto read_packet_again;
		}

		if (ret ==GPIO_ERROR) {
			debug_print("  read error (data)");
			return (GP_ERROR);
		}
	}

	fujitsu_dump_packet(packet);

	return (GP_OK);

}

int fujitsu_build_packet (char type, char subtype, int data_length, char *packet) {

	packet[0] = type;
	switch (type) {
		case TYPE_COMMAND:
			if (glob_first_packet)
				packet[1] = SUBTYPE_COMMAND_FIRST;
			   else
				packet[1] = SUBTYPE_COMMAND;
			glob_first_packet = 0;
			break;
		case TYPE_DATA:
		case TYPE_DATA_END:
			packet[1] = subtype;
			break;
		default:
			debug_print("unknown packet type");
	}

	packet[2] = data_length &  0xff;
	packet[3] = (data_length >> 8) & 0xff;

	return (GP_OK);
}

int fujitsu_read_ack(gpio_device *dev) {

	char buf[4096];

	debug_print(" fujitsu_read_ack");

	if (fujitsu_read_packet(dev, buf)==GP_ERROR) {
		debug_print("Could not read ACK");
		return (GP_ERROR);
	}

	if (buf[0] == ACK)
		return (GP_OK);

	debug_print("Could not read ACK");
	return (GP_ERROR);
}


int fujitsu_write_ack(gpio_device *dev) {

	char buf[4096];

	debug_print(" fujitsu_write_ack");

	buf[0] = ACK;
	if (fujitsu_write_packet(dev, buf)==GP_OK)
		return (GP_OK);
	debug_print("Could not write ACK");
	return (GP_ERROR);
}

int fujitsu_write_nak(gpio_device *dev) {

	char buf[4096];

	debug_print(" fujitsu_write_nak");

	buf[0] = NAK;
	if (fujitsu_write_packet(dev, buf)==GP_OK)
		return (GP_OK);

	debug_print("Could not write NAK");
	return (GP_ERROR);
}

int fujitsu_ping(gpio_device *dev) {

	int r=0;
	char buf[4096];

        buf[0] = NUL;

	while (r++ < RETRIES) {
		debug_print("pinging the camera");
		if (fujitsu_write_packet(dev, buf)==GP_ERROR)
			return (GP_ERROR);

		if (fujitsu_read_packet(dev, buf)==GP_ERROR)
			return (GP_ERROR);

		if (buf[0] == NAK) {
			debug_print("ping succeeded");
			return (GP_OK);
		}
		debug_print("ping failed");
	}
	return (GP_ERROR);
}

int fujitsu_set_speed(gpio_device *dev, int speed) {

	gpio_device_settings settings;
	char buf[1024];

	sprintf(buf, "Setting speed to %i", speed);
	debug_print(buf);

	glob_first_packet = 1;

	gpio_get_settings(dev, &settings);

	settings.serial.speed = speed;

	switch (speed) {		
		case 9600:
			speed = 1;
			break;
		case 19200:
			speed = 2;
			break;
		case 38400:
			speed = 3;
			break;
		case 57600:
			speed = 4;
			break;
		case 0:		/* Default speed */
			settings.serial.speed = 115200;
		case 115200:
			speed = 5;
			break;

		case -1:	/* End session */
			settings.serial.speed = 19200;
			speed = 0;
			break;
		default:
			return (GP_ERROR);
	}

	if (fujitsu_set_int_register(dev, 17, speed)==GP_ERROR)
		return (GP_ERROR);
	if (gpio_set_settings(dev, settings)==GPIO_ERROR)
		return (GP_ERROR);

	usleep(100000);
	return (GP_OK);
}

int fujitsu_set_int_register (gpio_device *dev, int reg, int value) {

	int l=0, r=0;
	char p[4096];
	char buf[4096];

	sprintf(buf, "Setting register #%i to %i", reg, value);
	debug_print(buf);

	if (value < 0)
		fujitsu_build_packet(TYPE_COMMAND, 0, 2, p);
	   else
		fujitsu_build_packet(TYPE_COMMAND, 0, 6, p);

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
		if (fujitsu_write_packet(dev, p)==GP_ERROR)
			return (GP_ERROR);

		if (fujitsu_read_packet(dev, buf)==GP_ERROR)
			return (GP_ERROR);

		if (buf[0] == ACK)
			return (GP_OK);

		/* DC1 = invalid register or value */
		if (buf[0] == DC1)
			return (GP_ERROR);
	}

	debug_print("too many retries");

	fujitsu_set_speed(dev, -1);

	return (GP_ERROR);
}

int fujitsu_get_int_register (gpio_device *dev, int reg, int *value) {

	int l=0, r=0, write_nak=0;
	char packet[4096];
	char buf[4096];

	sprintf(buf, "Getting register #%i value", reg);
	debug_print(buf);

	fujitsu_build_packet(TYPE_COMMAND, 0, 2, packet);

        /* Fill in the data */
	packet[4] = 0x01;
	packet[5] = reg;

	while (r++<RETRIES) {
		if (write_nak) {
			if (fujitsu_write_nak(dev)==GP_ERROR)
				return (GP_ERROR);
		}  else {
			if (fujitsu_write_packet(dev, packet)==GP_ERROR)
				return (GP_ERROR);
		}

		if (fujitsu_read_packet(dev, buf)==GP_ERROR)
			return (GP_ERROR);

		/* DC1 = invalid register or value */
		if (buf[0] == DC1)
			return (GP_ERROR);

		if (buf[0] == TYPE_DATA_END) {
//			fujitsu_write_ack(dev);
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

	debug_print("too many retries");
	return (GP_ERROR);
}

int fujitsu_set_string_register (gpio_device *dev, int reg, char *s, int length) {

	char packet[4096], buf[4096];
	char type;
	unsigned char c;
	int x=0, seq=0, size=0, done, r;

	sprintf(buf, "Setting string in register #%i to \"%s\"", reg, s);
	debug_print(buf);

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
		fujitsu_build_packet(type, seq, size, packet);

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
			if (fujitsu_write_packet(dev, packet)==GP_ERROR)
				return (GP_ERROR);

			if (fujitsu_read_packet(dev, buf)==GP_ERROR)
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
			debug_print("too many NAKs from camera");
			return (GP_ERROR);
		}
	}
	return (GP_OK);
}

int fujitsu_get_string_register (gpio_device *dev, int reg, char *s, int *length) {

	char packet[4096], buf[2048];
	int done, x, packlength, do_percent;

	sprintf(buf, "Getting string in register #%i", reg);
	debug_print(buf);

	do_percent = 1;
	switch (reg) {
		case 14:
		        /* Get the size of the current thumbnail */

	        	if (fujitsu_get_int_register(dev, 12, length)==GP_ERROR) {
	        	        interface_message("Can not get current image length");
	        	        return (GP_ERROR);
		        }
			break;
		case 15:
		        /* Get the size of the current picture */
	        	if (fujitsu_get_int_register(dev, 12, length)==GP_ERROR) {
	        	        interface_message("Can not get current image length");
	        	        return (GP_ERROR);
		        }
			break;
		case 44:
		        /* Get the size of the current audio */
			/* will do */
		default:
			do_percent = 0;
	}
	/* Send request */
	fujitsu_build_packet(TYPE_COMMAND, 0, 2, packet);
	packet[4] = 0x04;
	packet[5] = reg;
	if (fujitsu_write_packet(dev, packet)==GP_ERROR)
		return (GP_ERROR);

	/* Read all the data packets */
	x=0; done=0;
	while (!done) {
		if (fujitsu_read_packet(dev, packet)==GP_ERROR)
			return (GP_ERROR);
		if (packet[0] == DC1)
			return (GP_ERROR);
		if (do_percent)
			gp_progress(100.0*(float)x/(float)(*length));
		if (fujitsu_write_ack(dev)==GP_ERROR)
			return (GP_ERROR);
		packlength = ((unsigned char)packet[2]) +
			     ((unsigned char)packet[3]  * 256);
		memcpy(&s[x], &packet[4], packlength);
		x += packlength;
		if (packet[0] == TYPE_DATA_END)
			done = 1;
	}
	*length = x;
	return (GP_OK);
}

int fujitsu_delete(gpio_device *dev, int picture_number) {

	char packet[4096], buf[4096];
	int r, done;

       	if (fujitsu_set_int_register(dev, 4, picture_number)==GP_ERROR) {
		debug_print("Can not set current image");
       	        return (GP_ERROR);
        }

	fujitsu_build_packet(TYPE_COMMAND, 0, 3, packet);
	packet[4] = 0x02;
	packet[5] = 0x07;
	packet[6] = 0x00;

	r=0; done=0;
	while ((!done)&&(r++<RETRIES)) {
		if (fujitsu_write_packet(dev, packet)==GP_ERROR)
			return (GP_ERROR);
	
		if (fujitsu_read_packet(dev, buf)==GP_ERROR)
			return (GP_ERROR);

		done = (buf[0] == NAK)? 0 : 1;

		if (done) {
			/* read in the ENQ */
			if (fujitsu_read_packet(dev, buf)==GP_ERROR)
				return ((buf[0]==ENQ)? GP_OK : GP_ERROR);
			
		}
	}
	if (r > RETRIES) {
		debug_print("too many NAKs from camera");
		return (GP_ERROR);
	}

	return (GP_OK);
}

int fujitsu_end_session(gpio_device *dev) {

	char packet[4096], buf[4096];
	unsigned char c;
	int r, done;

	fujitsu_build_packet(TYPE_COMMAND, 0, 3, packet);
	packet[4] = 0x02;
	packet[5] = 0x04;
	packet[6] = 0x00;

	r=0; done=0;
	while ((!done)&&(r++<RETRIES)) {
		if (fujitsu_write_packet(dev, packet)==GP_ERROR)
			return (GP_ERROR);
	
		if (fujitsu_read_packet(dev, buf)==GP_ERROR)
			return (GP_ERROR);

		c = (unsigned char)buf[0];
		if (c==TRM)
			return (GP_OK);

		done = (c == NAK)? 0 : 1;

		if (done) {
			/* read in the ENQ */
			if (fujitsu_read_packet(dev, buf)==GP_ERROR)
				return (GP_ERROR);
			c = (unsigned char)buf[0];
			return ((c==ENQ)? GP_OK : GP_ERROR);
		}
	}
	if (r > RETRIES) {
		debug_print("too many NAKs from camera");
		return (GP_ERROR);
	}

	return (GP_OK);
}
