#include <gphoto2.h>
#include <gpio/gpio.h>
#include "fujitsu.h"
#include "library.h"

char *fujitsu_build_packet (char type, char subtype, int data_length) {

	char *packet;

	packet = (char*)malloc(sizeof(char)*(data_length+4));

	packet[0] = type;
	packet[1] = subtype;
	packet[2] = data_length;

	return (packet);
}


int fujitsu_process_packet (char *packet) {

	short int x, checksum=0;
	int length = 4 + packet[2];

	packet[2] = htons(packet[2]);
	for (x=3; x<length-1; x++) {
		packet[x] = htons(packet[x]);
		checksum+=packet[x];
	}

	packet[length-1] = htons(checksum);

	return (length);
}

int fujitsu_ping(gpio_device *dev) {

	char buf[2];

	debug_print("pinging the camera");

        buf[0] = 0x00;
        if (gpio_write(dev, buf, 1)==GPIO_ERROR) {
		debug_print("write failed");
                return (GP_ERROR);
	}

	if (gpio_read(dev, buf, 1)==GPIO_ERROR) {
		debug_print("read failed");
		return (GP_ERROR);
	}

	if (buf[0] == 0x15) {
		debug_print("ping succeeded");
		return (GP_OK);
	}
	debug_print("ping failed");
	return (GP_ERROR);
}

int fujitsu_set_int_register (gpio_device *dev, int reg, int value) {

	int l=0, r=0;
	char *p;
	char buf[64];

	sprintf(buf, "Setting register #%i to %i", reg, value);
	debug_print(buf);

        p = fujitsu_build_packet(TYPE_COMMAND, SUBTYPE_COMMAND_FIRST,3);

        /* Fill in the data */
        p[3] = 0x00;
        p[4] = reg;
        p[5] = value;

        l = fujitsu_process_packet(p);

	sprintf(buf, "packet length: %i", l);
	debug_print(buf);

	while (r++<RETRIES) {
	        if (gpio_write(dev, p, l)==GPIO_ERROR) {
			debug_print("write failed");
			return (GP_ERROR);
		}
	        if (gpio_read(dev, buf, 1)==GPIO_ERROR) {
			debug_print("read failed");		
			return (GP_ERROR);
		}
		if (buf[0] == ACK) {
			debug_print("got ACK");
			return (GP_OK);
		}
	}

	debug_print("excessive retries");
	return (GP_ERROR);
}

int fujitsu_get_int_register (gpio_device *dev, int reg) {


}
