
#include <stdio.h>
#include <string.h>
//#include "gpio.h"
#include <gphoto2-port.h>

void dump(gp_port * dev)
{

}

int main(int argc, char **argv)
{
	gp_port *dev;	/* declare the device */
	gp_port_settings settings;
	char buf[32];
	int ret;

	if( (ret=gp_port_init ( GP_DEBUG_HIGH ))<0)
	{
		printf( "%s\n", gp_port_result_as_string(ret) );
		exit( 1 );
	}
	dev = (gp_port *) malloc( sizeof(gp_port) );
	if( !dev )
	{
		printf( "Sorry can't get enought memory\n" );
		exit( 1 );
	}
	if( (ret = gp_port_new( &dev, GP_PORT_SERIAL )) <0 )
	{
		printf( "%s\n", gp_port_result_as_string(ret) );
		exit( 1 );
	}
	/* create a new serial device */
	gp_port_timeout_set(dev, 500);

	strcpy(settings.serial.port, "/dev/ttyS1");
	settings.serial.speed = 19200;
	settings.serial.bits = 8;
	settings.serial.parity = 0;
	settings.serial.stopbits = 1;

	if( (ret=gp_port_settings_set(dev, settings))<0 )
	{
		printf( "%s\n", gp_port_result_as_string(ret) );
		exit( 1 );
	}
	/* open the device */
	if( (ret=gp_port_open(dev))<0 )
	{
		printf( "%s\n", gp_port_result_as_string(ret) );
		exit( 1 );
	}
	dump(dev);

	gp_port_settings_get(dev, &settings);
	settings.serial.speed = 57600;
	gp_port_settings_set(dev, settings);

	dump(dev);

	//printf("CTS: %i", gp_port_pin_get( dev, PIN_CTS, LOW ) );

	gp_port_write(dev, "AT\n", 3);	/* write bytes to the device */

	gp_port_read(dev, buf, 3);	/* read bytes from the device */
	buf[3] = 0;
	printf("recv: %s\n", buf);

	gp_port_close(dev);	/* close the device */

	gp_port_free(dev);

	return 0;
}
