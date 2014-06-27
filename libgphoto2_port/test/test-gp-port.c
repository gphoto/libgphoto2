/* test-gp-port.c
 *
 * Copyright 2002 Lutz Mueller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-info-list.h>

static void
log_func (GPLogLevel level, const char *domain,
	  const char *str, void *data)
{
	fprintf (stdout, "%s\n", str);
}

int
main (int argc, char **argv)
{
	GPPort *dev;
	GPPortSettings settings;
	char buf[32];
	int ret;
	GPPortInfoList *il;
	GPPortInfo info;
	GPLevel level;
	unsigned int i;

	gp_log_add_func (GP_LOG_DATA, log_func, NULL);

	for (i = 0; i < 2; i++) {

		ret = gp_port_info_list_new (&il);
		if (ret < 0) {
			printf ("Could not create list of ports: %s\n",
				gp_port_result_as_string (ret));
			return (1);
		}
	
		ret = gp_port_info_list_load (il);
		if (ret < 0) {
			printf ("Could not load list of ports: %s\n",
				gp_port_result_as_string (ret));
			return (1);
		}
	
		printf ("############\n");
		printf ("############ There are %i IO-drivers "
			"installed on your system.\n",
			gp_port_info_list_count (il));
		printf ("############\n");
	
		ret = gp_port_info_list_get_info (il, 0, &info);
		if (ret < 0) {
			printf ("Could not get info of first port in "
				"list: %s\n",
				gp_port_result_as_string (ret));
			return (1);
		}

		ret = gp_port_new (&dev);
		if (ret < 0) {
			printf ("Could not create device: %s\n",
				gp_port_result_as_string (ret));
			return (1);
		}

		ret = gp_port_set_info (dev, info);
		if (ret < 0) {
			printf ("Could not set port info: %s\n",
				gp_port_result_as_string (ret));
			return (1);
		}

		gp_port_info_list_free (il);

		ret = gp_port_set_timeout (dev, 500);
		if (ret < 0) {
			printf ("Could not set timeout: %s\n",
				gp_port_result_as_string (ret));
			return (1);
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
			return (1);
		}

		ret = gp_port_open (dev);
		if (ret < 0) {
			printf ("Could not open device: %s\n",
				gp_port_result_as_string (ret));
			return (1);
		}

		gp_port_get_settings(dev, &settings);
		settings.serial.speed = 57600;
		gp_port_set_settings(dev, settings);

		ret = gp_port_get_pin (dev, GP_PIN_DTR, &level);
		if (ret < 0) {
			printf ("Could not get level of pin DTR: %s\n",
				gp_port_result_as_string (ret));
			return (1);
		}

		ret = gp_port_set_pin (dev, GP_PIN_CTS, GP_LEVEL_HIGH);
		if (ret < 0) {
			printf ("Could not set level of pin CTS: %s\n",
				gp_port_result_as_string (ret));
			return (1);
		}

		gp_port_write (dev, "AT\n", 3);

		gp_port_read (dev, buf, 3);
		buf[3] = 0;
		printf("recv: %s\n", buf);

		gp_port_close (dev);

		gp_port_free (dev);
	}

	return 0;
}
