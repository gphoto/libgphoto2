/* -*- Mode: C { indent-tabs-mode: t { c-basic-offset: 8 { tab-width: 8 -*- */
/* gphoto2-port-parallel.c - parallel IO functions

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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "gphoto2-port-parallel.h"
#include "gphoto2-port.h"

/* Parallel prototypes
   --------------------------------------------------------------------- */
int             gp_port_parallel_list(gp_port_info *list, int *count);

int             gp_port_parallel_init(gp_port *dev);
int             gp_port_parallel_exit(gp_port *dev);

int             gp_port_parallel_open(gp_port *dev);
int             gp_port_parallel_close(gp_port *dev);

int             gp_port_parallel_read(gp_port *dev, char *bytes, int size);
int             gp_port_parallel_write(gp_port *dev, char *bytes, int size);

int             gp_port_parallel_get_pin(gp_port *dev, int pin, int *level);
int             gp_port_parallel_set_pin(gp_port *dev, int pin, int level);

int             gp_port_parallel_update (gp_port *dev);


/* Dynamic library functions
   --------------------------------------------------------------------- */

gp_port_type gp_port_library_type () {

        return (GP_PORT_PARALLEL);
}

gp_port_operations *gp_port_library_operations () {

        gp_port_operations *ops;

        ops = (gp_port_operations*)malloc(sizeof(gp_port_operations));
        memset(ops, 0, sizeof(gp_port_operations));

        ops->init   = gp_port_parallel_init;
        ops->exit   = gp_port_parallel_exit;
        ops->open   = gp_port_parallel_open;
        ops->close  = gp_port_parallel_close;
        ops->read   = gp_port_parallel_read;
        ops->write  = gp_port_parallel_write;
        ops->update = gp_port_parallel_update;
        ops->get_pin = gp_port_parallel_get_pin;
        ops->set_pin = gp_port_parallel_set_pin;

        return (ops);
}

int gp_port_library_list(gp_port_info *list, int *count) {

        char buf[1024], prefix[1024];
        int x, fd, use_int=0, use_char=0;
#ifdef __linux
	/* devfs */	
	struct stat s;
#endif

#ifdef OS2
        int rc,fh,option;
#endif

	strcpy(prefix, GP_PORT_PARALLEL_PREFIX);

#ifdef __linux
	/* devfs */	
	if (stat("/dev/parports", &s) == 0)
		strcpy(prefix, "/dev/parports/%i");
#endif

        for (x=GP_PORT_PARALLEL_RANGE_LOW; x<=GP_PORT_PARALLEL_RANGE_HIGH; x++) {
                sprintf(buf, prefix, x);
                #ifdef OS2
                rc = DosOpen(buf,&fh,&option,0,0,1,OPEN_FLAGS_FAIL_ON_ERROR|OPEN_SHARE_DENYREADWRITE,0);
                if(rc==0)
                {
                #endif

                fd = open (buf, O_RDONLY | O_NDELAY);
                if (fd != -1) {
                        close(fd);
                        list[*count].type = GP_PORT_PARALLEL;
                        strcpy(list[*count].path, buf);
                        sprintf(buf, "Parallel Port %i", x);
                        strcpy(list[*count].name, buf);
                        list[*count].argument_needed = 0;
                        *count += 1;
                }
                #ifdef OS2
                }
                #endif
        }

        return (GP_OK);
}

/* Parallel API functions
   --------------------------------------------------------------------- */

int gp_port_parallel_init(gp_port *dev) {

}

int gp_port_parallel_exit(gp_port *dev) {

}

int gp_port_parallel_open(gp_port *dev) {

}

int gp_port_parallel_close(gp_port *dev) {

}
int gp_port_parallel_read(gp_port *dev, char *bytes, int size) {

}

int gp_port_parallel_write(gp_port *dev, char *bytes, int size) {

}

int gp_port_parallel_get_pin(gp_port *dev, int pin, int *level) {

}

int gp_port_parallel_set_pin(gp_port *dev, int pin, int level) {

}

int gp_port_parallel_update (gp_port *dev) {

}
