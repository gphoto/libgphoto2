/* test-port-list.c
 *
 * Copyright (C) 2002 Lutz Mueller <lutz@users.sourceforge.net>
 * Copyright (C) 2007 Hans Ulrich Niedermann <gp@n-dimensional.de>
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


#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif


static void
log_func (GPLogLevel level, const char *domain __unused__,
	  const char *str, void *data __unused__)
{
	if (level <= GP_LOG_ERROR) {
		fprintf (stdout, "%s\n", str);
		fflush (stdout);
	}
}


static int
run_test ()
{
	int i;
	int ret;
	GPPortInfoList *il;

	int count;

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
		return (2);
	}
	
	count = gp_port_info_list_count (il);
	if (count < 0) {
		printf("gp_port_info_list_count error: %d\n", count);
		return 1;
	} else if (count == 0) {
		/* Unlike gphoto2-abilities-list,
		 * gphoto2-port-info-list.c does not just load
		 * the driver libs, but also scans for
		 * ports. Thus 0 ports are a valid results.
		 */
	}
	/* Non-empty list */
	printf ("%i ports found on your system.\n", count);

	for (i = 0; i < count; i++) {
		const char *port_type_str;
		char *name, *path;
		GPPortInfo	info;
		GPPortType	type;
		ret = gp_port_info_list_get_info (il, i, &info);
		if (ret < 0) {
		  	printf ("ERROR getting iolib info: %s\n",
				gp_port_result_as_string (ret));
			return (1);
		}
		gp_port_info_get_type (info, &type);
		gp_port_info_get_name (info, &name);
		gp_port_info_get_path (info, &path);
		switch (type) {
		case GP_PORT_NONE:   port_type_str = "NONE"; break;
		case GP_PORT_SERIAL: port_type_str = "SERIAL"; break;
		case GP_PORT_USB:    port_type_str = "USB"; break;
		case GP_PORT_USB_DISK_DIRECT:    port_type_str = "USBDISKDIRECT"; break;
		case GP_PORT_USB_SCSI:    port_type_str = "USBSCSI"; break;
		case GP_PORT_DISK:   port_type_str = "DISK"; break;
		case GP_PORT_PTPIP:  port_type_str = "PTPIP"; break;
		default:             port_type_str = "UNKNOWN"; return 3; break;
		}
		printf ("No:    %d\n"
			"Type:  %s\n"
			"Name:  %s\n"
			"Path:  %s\n",
			i,
			port_type_str,
			name,
			path
			);
	}


	gp_port_info_list_free (il);
	return 0;
}


int
main ()
{
	gp_log_add_func (GP_LOG_DATA, log_func, NULL);
	return run_test();
}
