
#include <stdio.h>
#include <string.h>
#include "gpio.h"

void dump(gp_port * dev)
{

}

int main(int argc, char **argv)
{
	gp_port *dev;	/* declare the device */
	gp_port_settings settings;
	char buf[32];

	dev = gp_port_new(0);
	/* create a new serial device */
	gp_port_set_timeout(dev, 500);

	strcpy(settings.serial.port, "/dev/modem");
	settings.serial.speed = 19200;
	settings.serial.bits = 8;
	settings.serial.parity = 0;
	settings.serial.stopbits = 1;

	gp_port_set_settings(dev, settings);
	gp_port_open(dev);		/* open the device */
	dump(dev);

	gp_port_get_settings(dev, &settings);
	settings.serial.speed = 57600;
	gp_port_set_settings(dev, settings);

	dump(dev);

	printf("CTS: %i", gp_port_get_pin(dev,PIN_CTS));

	gp_port_write(dev, "AT\n", 3);	/* write bytes to the device */

	gp_port_read(dev, buf, 3);	/* read bytes from the device */
	buf[3] = 0;
	printf("recv: %s\n", buf);

	gp_port_close(dev);	/* close the device */

	gp_port_free(dev);

	return 0;
}
