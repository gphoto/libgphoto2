/* -*- Mode: C { indent-tabs-mode: t { c-basic-offset: 8 { tab-width: 8 -*- */
/* gphoto2-port-network.c - network IO functions

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

/* network prototypes
   --------------------------------------------------------------------- */
int 		gp_port_network_list(gp_port_info *list, int *count);

int 		gp_port_network_init(gp_port *dev);
int 		gp_port_network_exit(gp_port *dev);

int 		gp_port_network_open(gp_port *dev);
int 		gp_port_network_close(gp_port *dev);

int 		gp_port_network_read(gp_port *dev, char *bytes, int size);
int 		gp_port_network_write(gp_port *dev, char *bytes, int size);

int		gp_port_network_get_pin(gp_port *dev, int pin);
int		gp_port_network_set_pin(gp_port *dev, int pin, int level);

int 		gp_port_network_update (gp_port *dev);

int 		gp_port_network_set_baudrate(gp_port *dev);

/* Dynamic library functions
   --------------------------------------------------------------------- */

gp_port_type gp_port_library_type () {

        return (GP_PORT_NETWORK);
}

gp_port_operations *gp_port_library_operations () {

        gp_port_operations *ops;

	ops = (gp_port_operations*)malloc(sizeof(gp_port_operations));
        memset(ops, 0, sizeof(gp_port_operations));

        ops->init   = gp_port_network_init;
        ops->exit   = gp_port_network_exit;
        ops->open   = gp_port_network_open;
        ops->close  = gp_port_network_close;
        ops->read   = gp_port_network_read;
        ops->write  = gp_port_network_write;
        ops->update = gp_port_network_update;
        ops->get_pin = NULL;
        ops->set_pin = NULL;
        ops->clear_halt = NULL;
        ops->msg_write  = NULL;
        ops->msg_read   = NULL;

        return (ops);
}

int gp_port_library_list(gp_port_info *list, int *count) {

        list[*count].type = GP_PORT_NETWORK;
        strcpy(list[*count].name, "Network connection");
        strcpy(list[*count].path, "network");
	list[*count].argument_needed = 1;
	strcpy(list[*count].argument_description, "host");
        *count += 1;

        return (GP_OK);
}

/* Network API functions
   --------------------------------------------------------------------- */

int gp_port_network_init(gp_port *dev) {

}

int gp_port_network_exit(gp_port *dev) {

}

int gp_port_network_open(gp_port *dev) {

}

int gp_port_network_close(gp_port *dev) {

}
int gp_port_network_read(gp_port *dev, char *bytes, int size) {

}

int gp_port_network_write(gp_port *dev, char *bytes, int size) {

}

int gp_port_network_get_pin(gp_port *dev, int pin) {

}

int gp_port_network_set_pin(gp_port *dev, int pin, int level) {

}

int gp_port_network_update (gp_port *dev) {

}
