#include <gphoto2.h>
#include <gpio/gpio.h>
#include "fujitsu.h"
#include "library.h"

void fujitsu_dump_packet (char *packet) {
	/* packet is assumed to have gone through ntohs() already */

	int x, 	length=0;
	char buf[4096];


	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_SEQUENCE) ||
	    (packet[0] == TYPE_END)) {
		length = 6 + packet[2];
	} else {
		switch((unsigned char)packet[0]) {
			case NUL:
				strcpy(buf, " packet: NUL");
				break;
			case ENQ:
				strcpy(buf, " packet: ENQ");
				break;
			case ACK:
				strcpy(buf, " packet: ACK");
				break;
			case DC1:
				strcpy(buf, " packet: DC1");
				break;
			case NAK:
				strcpy(buf, " packet: NAK (Camera Signature)");
				break;
			case TRM:
				strcpy(buf, " packet: TRM");
				break;
			default:
				sprintf(buf, " packet: 0x%02x (UNKNOWN!)", packet[0]);
		}
		debug_print(buf);
		return;
	}

	strcpy(buf, " packet:");
	for (x=0; x<length; x++)
		sprintf(buf, "%s 0x%02x", buf, (unsigned char)packet[x]);
	
	debug_print(buf);
}

int fujitsu_valid_type(char b) {

	unsigned char byte = (unsigned char)b;

	if (
		(byte == NUL) ||
		(byte == ENQ) ||
		(byte == ACK) ||
		(byte == DC1) ||
		(byte == NAK) ||
//		(byte == TRM) ||
		(byte == TYPE_COMMAND) ||
		(byte == TYPE_SEQUENCE) ||
		(byte == TYPE_END)
	    )
		return (GP_OK);
	return (GP_ERROR);
}

int fujitsu_valid_packet (char *packet) {
	/* assumes ntohs() done already */

	int length;

	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_SEQUENCE) ||
	    (packet[0] == TYPE_END)) {
		length = 6 + packet[2];
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
	
	debug_print("fujitsu_write_packet");

	/* Determing packet length */
	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_SEQUENCE) ||
	    (packet[0] == TYPE_END)) {
		length = 6 + (((unsigned char)packet[3]<<8)|((unsigned char)packet[2]));
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

	strncpy(p, packet, length);

	/* Convert to network order */
	if (length > 1) {
		for (x=2; x<length-1; x++)
			p[x] = (unsigned char)htons(packet[x]);
	}

        ret = gpio_write(dev, p, length);
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

	debug_print("fujitsu_read_packet");

	done = 0;
	while (!done && (r++<RETRIES)) {
		ret = gpio_read(dev, buf, 1);
		if (ret == GPIO_ERROR)
			return (GP_ERROR);
		if (fujitsu_valid_type(buf[0])==GP_OK)
			done = 1;
	}

	if (r==RETRIES)
		return (GP_ERROR);

	if ((buf[0] == TYPE_COMMAND) ||
	    (buf[0] == TYPE_SEQUENCE) ||
	    (buf[0] == TYPE_END)) {
		if (gpio_read(dev, &buf[1], 2)==GPIO_ERROR)
			return (GP_ERROR);
                length = 6 + ((packet[3]<<8)|(packet[2]));
	} else {
		packet[0] = buf[0];
		fujitsu_dump_packet(packet);
		return (fujitsu_valid_packet(packet));
	}

	sprintf(buf, " length %i", length);
	debug_print(buf);

	strncpy(packet, buf, 3);

	if (gpio_read(dev, packet, length-3)==GPIO_ERROR)
		return (GP_ERROR);

	for (x=2; x<length; x++)
		packet[x] = ntohs((unsigned char)packet[x]);

	fujitsu_dump_packet(packet);


	return (GP_OK);

}

int fujitsu_build_packet (char type, char subtype, int data_length, char *packet) {

	packet[0] = type;
	packet[1] = subtype;
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

int fujitsu_set_int_register (gpio_device *dev, int reg, int value) {

	int l=0, r=0;
	char p[4096];
	char buf[4096];

	sprintf(buf, "Setting register #%i to %i", reg, value);
	debug_print(buf);

	fujitsu_build_packet(TYPE_COMMAND, SUBTYPE_COMMAND_FIRST,6, p);

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

	int l=0, r=0, do_ack;
	char packet[4096];
	char buf[4096];

	sprintf(buf, "Getting register #%i value", reg);
	debug_print(buf);

	fujitsu_build_packet(TYPE_COMMAND, SUBTYPE_COMMAND_FIRST,2, packet);

        /* Fill in the data */
	packet[4] = 0x01;
	packet[5] = reg;

	while (r++<RETRIES) {
		do_ack = 0;
		if (fujitsu_write_packet(dev, packet)==GP_ERROR)
			return (GP_ERROR);

		if (fujitsu_read_ack(dev)==GP_ERROR)
			return (GP_ERROR);

		if (fujitsu_read_packet(dev, buf)==GP_ERROR)
			return (GP_ERROR);
		   else
			fujitsu_write_ack(dev);
	}

	debug_print("too many retries");
	return (GP_ERROR);
}
