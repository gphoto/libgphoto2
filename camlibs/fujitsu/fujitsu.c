#include <gphoto2.h>
#include <gpio/gpio.h>
#include "fujitsu.h"
#include "library.h"

int glob_first_packet = 1;

void fujitsu_dump_packet (char *packet) {

	int x, 	length=0;
	char buf[4096];

	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_SEQUENCE) ||
	    (packet[0] == TYPE_END)) {
                length = ((unsigned char)packet[2] & 0xff) | ((unsigned char)packet[3] << 8);
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

	strcpy(buf, "  packet:");
	for (x=0; x<length; x++)
		sprintf(buf, "%s 0x%02x", buf, (unsigned char)packet[x]);
	
	debug_print(buf);
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
	    (byte == TYPE_SEQUENCE) ||
	    (byte == TYPE_END))
		return (GP_OK);
	return (GP_ERROR);
}

int fujitsu_valid_packet (char *packet) {

	int length;

	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_SEQUENCE) ||
	    (packet[0] == TYPE_END)) {
                length = ((unsigned char)packet[2] & 0xff) | ((unsigned char)packet[3] << 8);
		length += 6;
	} else {
		switch(packet[0]) {
			case NUL:
			case ENQ:
			case ACK:
			case DC1:
			case NAK:
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

	int x, ret, checksum=0, length;
	char buf[4096], p[4096];
	
	debug_print(" fujitsu_write_packet");

	/* Determing packet length */
	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_SEQUENCE) ||
	    (packet[0] == TYPE_END)) {
                length = ((unsigned char)packet[2] & 0xff) | ((unsigned char)packet[3] << 8);
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


	/* Dump packet if debug */
	fujitsu_dump_packet(packet);

        ret = gpio_write(dev, packet, length);

	if (ret != GPIO_OK) {
		if (ret == GPIO_TIMEOUT)
			debug_print(" write failed (timeout)");
		   else
			debug_print(" write failed");
                return (GP_ERROR);
	}

	return (GP_OK);
}

int fujitsu_read_packet (gpio_device *dev, char *packet) {

	int x, r, ret, done, length=0;
	char buf[4096], msg[4096];

	buf[0] = 0;
	packet[0] = 0;

	debug_print(" fujitsu_read_packet");

	done = 0;
	while (!done && (r++<RETRIES)) {
		ret = gpio_read(dev, packet, 1);
		if (ret == GPIO_ERROR)
			return (GP_ERROR);
		if (fujitsu_valid_type(packet[0])==GP_OK)
			done = 1;
	}

	if (r==RETRIES)
		return (GP_ERROR);

	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_SEQUENCE) ||
	    (packet[0] == TYPE_END)) {
		if (gpio_read(dev, &packet[1], 3)==GPIO_ERROR)
			return (GP_ERROR);
                length = ((unsigned char)packet[2] & 0xff) | ((unsigned char)packet[3] << 8);
		length += 6;
	} else {
		fujitsu_dump_packet(packet);
		return (fujitsu_valid_packet(packet));
	}

	sprintf(buf, "  packet length: %i", length);
	debug_print(buf);

	if (gpio_read(dev, &packet[4], length-4)==GPIO_ERROR)
		return (GP_ERROR);

	fujitsu_dump_packet(packet);

	return (GP_OK);

}

int fujitsu_build_packet (char type, char subtype, int data_length, char *packet) {

	packet[0] = type;
	if (type == TYPE_COMMAND) {
		if (glob_first_packet)
			packet[1] = SUBTYPE_COMMAND_FIRST;
		   else
			packet[1] = SUBTYPE_COMMAND;
		glob_first_packet = 0;
	}
	packet[2] = data_length &  0xff;
	packet[3] = data_length >> 8;

	return (GP_OK);
}

int fujitsu_read_ack(gpio_device *dev) {

	char buf[4096];

	if (fujitsu_read_packet(dev, buf)==GP_ERROR)
		return (GP_ERROR);

	if (buf[0] == ACK)
		return (GP_OK);

	return (GP_ERROR);	
}


int fujitsu_write_ack(gpio_device *dev) {

	char buf[4096];

	buf[0] = ACK;
	return (fujitsu_write_packet(dev, buf));
}

int fujitsu_write_nak(gpio_device *dev) {

	char buf[4096];

	buf[0] = NAK;
	return (fujitsu_write_packet(dev, buf));
}

int fujitsu_ping(gpio_device *dev) {

	int r=0;
	char buf[4096];

        buf[0] = 0x00;

	while (r++ < RETRIES) {
		debug_print("pinging the camera");
		if (fujitsu_write_packet(dev, buf)==GP_ERROR)
			return (GP_ERROR);

		if (fujitsu_read_packet(dev, buf)==GP_ERROR)
			return (GP_ERROR);

		if (buf[0] == 0x15) {
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
		case 115200:
			speed = 5;
			break;
		default:
			return (GP_ERROR);
	}

	if (fujitsu_set_int_register(dev, 17, speed)==GP_ERROR)
		return (GP_ERROR);
	sleep(1);
	if (gpio_set_settings(dev, settings)==GPIO_ERROR)
		return (GP_ERROR);

	return (GP_OK);
}

int fujitsu_set_int_register (gpio_device *dev, int reg, int value) {

	int l=0, r=0;
	char p[4096];
	char buf[4096];

	sprintf(buf, "Setting register #%i to %i", reg, value);
	debug_print(buf);

	fujitsu_build_packet(TYPE_COMMAND, 0, 6, p);

        /* Fill in the data */
        p[4] = 0x00;
        p[5] = reg;
	p[6] = (value)     & 0xff;
	p[7] = (value>>8)  & 0xff;
	p[8] = (value>>16) & 0xff;
	p[9] = (value>>24) & 0xff;

	while (r++<RETRIES) {
		if (fujitsu_write_packet(dev, p)==GP_ERROR)
			return (GP_ERROR);

		if (fujitsu_read_ack(dev)==GP_OK)
			return (GP_OK);
	}

	debug_print("too many retries");
	return (GP_ERROR);
}

int fujitsu_get_int_register (gpio_device *dev, int reg) {

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

		if (buf[0] == 0x03) {
			fujitsu_write_ack(dev);
			r = (int)buf[4] | 
			   ((int)buf[5] << 8)  | 
			   ((int)buf[6] << 16) | 
			   ((int)buf[7] << 24);
			return (r);
		} else {
			write_nak = 1;
		}
	}

	debug_print("too many retries");
	return (GP_ERROR);
}
