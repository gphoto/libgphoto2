
#include <stdio.h>
#include <string.h>
#include <gphoto2-port.h>
#include <gphoto2-port-result.h>

int
main (int argc, char **argv)
{
	GPPort *dev;
	GPPortSettings settings;
	char buf[32];
	int ret;

	ret = gp_port_new (&dev);
	if (ret < 0) {
		printf ("Could not create device: %s\n",
			gp_port_result_as_string (ret));
		exit (1);
	}

	ret = gp_port_set_timeout (dev, 500);
	if (ret < 0) {
		printf ("Could not set timeout: %s\n",
			gp_port_result_as_string (ret));
		exit (1);
	}

	strcpy (settings.serial.port, "serial:/dev/ttyS0");
	settings.serial.speed = 19200;
	settings.serial.bits = 8;
	settings.serial.parity = 0;
	settings.serial.stopbits = 1;

	ret = gp_port_set_settings (dev, settings);
	if (ret < 0) {
		printf ("Could not set settings: %s\n",
			gp_port_result_as_string (ret));
		exit (1);
	}

	ret = gp_port_open (dev);
	if (ret < 0) {
		printf ("Could not open device: %s\n",
			gp_port_result_as_string (ret));
		exit (1);
	}

	gp_port_get_settings(dev, &settings);
	settings.serial.speed = 57600;
	gp_port_set_settings(dev, settings);

	/*printf("CTS: %i", gp_port_pin_get( dev, PIN_CTS, LOW ) );*/

	gp_port_write(dev, "AT\n", 3);	/* write bytes to the device */

	gp_port_read(dev, buf, 3);	/* read bytes from the device */
	buf[3] = 0;
	printf("recv: %s\n", buf);

	gp_port_close(dev);	/* close the device */

	gp_port_free(dev);

	return 0;
}
