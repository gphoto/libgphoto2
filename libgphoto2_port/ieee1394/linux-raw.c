/* -*- Mode: C { indent-tabs-mode: t { c-basic-offset: 8 { tab-width: 8 -*- */
/* gphoto2-port-ieee1394.c - ieee1394 IO functions

   Modifications:
   Copyright (C) 1999 Scott Fritzinger <scottf@unr.edu>

   The GPIO Library is free software { you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation { either version 2 of the
   License, or (at your option) any later version.

   The GPIO Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY { without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GPIO Library { see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
 */

#include "gphoto2-port.h"

/* IEEE1394 prototypes
   ------------------------------------------------------------------ */
int 		gp_port_ieee1394_list(gp_port_info *list, int *count);

int 		gp_port_ieee1394_init(gp_port *dev);
int 		gp_port_ieee1394_exit(gp_port *dev);

int 		gp_port_ieee1394_open(gp_port *dev);
int 		gp_port_ieee1394_close(gp_port *dev);

int 		gp_port_ieee1394_read(gp_port *dev, char *bytes, int size);
int 		gp_port_ieee1394_write(gp_port *dev, char *bytes, int size);

int		gp_port_ieee1394_get_pin(gp_port *dev, int pin);
int		gp_port_ieee1394_set_pin(gp_port *dev, int pin, int level);

int 		gp_port_ieee1394_update (gp_port *dev);

int 		gp_port_ieee1394_set_baudrate(gp_port *dev);


/* Dynamic library functions
   ------------------------------------------------------------------ */

gp_port_type gp_port_library_type () {

	return (GP_PORT_IEEE1394);
}

gp_port_operations *gp_port_library_operations () {

	gp_port_operations *ops;

        ops = (gp_port_operations*)malloc(sizeof(gp_port_operations));
        memset(ops, 0, sizeof(gp_port_operations));

        ops->init   = gp_port_ieee1394_init;
	ops->exit   = gp_port_ieee1394_exit;
        ops->open   = gp_port_ieee1394_open;
        ops->close  = gp_port_ieee1394_close;
        ops->read   = gp_port_ieee1394_read;
        ops->write  = gp_port_ieee1394_write;
        ops->update = gp_port_ieee1394_update;
	return (ops);
}

int gp_port_library_list(gp_port_info *list, int *count) {

        list[*count].type = GP_PORT_IEEE1394;
        strcpy(list[*count].name, "IEEE1394 (Firewire(tm))");
        strcpy(list[*count].path, "ieee1394");
	list[*count].argument_needed = 0;
        *count += 1;

        return (GP_OK);

}

/* IEEE1394 API functions
   ------------------------------------------------------------------ */

int gp_port_ieee1394_init(gp_port *dev) {

}

int gp_port_ieee1394_exit(gp_port *dev) {

}

int gp_port_ieee1394_open(gp_port *dev) {

}

int gp_port_ieee1394_close(gp_port *dev) {

}
int gp_port_ieee1394_read(gp_port *dev, char *bytes, int size) {

}

int gp_port_ieee1394_write(gp_port *dev, char *bytes, int size) {

}

int gp_port_ieee1394_get_pin(gp_port *dev, int pin) {

}

int gp_port_ieee1394_set_pin(gp_port *dev, int pin, int level) {

}

int gp_port_ieee1394_update (gp_port *dev) {

}
