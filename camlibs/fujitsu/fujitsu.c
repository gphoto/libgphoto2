#include <gphoto2.h>
#include <gpio/gpio.h>
#include "fujitsu.h"
#include "gphoto.h"

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

        buf[0] = 0x00;
        if (gpio_write(dev, buf, 1)==GPIO_ERROR)
                return (GP_ERROR);

	if (gpio_read(dev, buf, 1)==GPIO_ERROR)
		return (GP_ERROR);

	if (buf[0] == 0x15)
		return (GP_OK);
	return (GP_ERROR);
}
