/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gphoto2-port-usb.c - USB transport functions

   Copyright 1999-2000 Johannes Erdfelt <johannes@erdfelt.com>

   The GPIO Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GPIO Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GPIO Library; see the file COPYING.LIB.  If not,
   write to the 
   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include <dirent.h>
#include <string.h>

/*#include <usb.h>*/
#include <gphoto2/gphoto2-port.h>


int gp_port_usb_list(gp_port_info *list, int *count);
int gp_port_usb_init(gp_port *dev);
int gp_port_usb_exit(gp_port *dev);
int gp_port_usb_open(gp_port *dev);
int gp_port_usb_close(gp_port *dev);
int gp_port_usb_reset(gp_port *dev);
int gp_port_usb_write(gp_port * dev, char *bytes, int size);
int gp_port_usb_read(gp_port * dev, char *bytes, int size);
int gp_port_usb_update(gp_port * dev);

int gp_port_usb_clear_halt_lib(gp_port * dev, int ep);
int gp_port_usb_msg_read_lib(gp_port * dev, int value, char *bytes, int size);
int gp_port_usb_msg_write_lib(gp_port * dev, int value, char *bytes, int size);
int gp_port_usb_find_device_lib(gp_port *dev, int idvendor, int idproduct);

/* Dynamic library functions
   --------------------------------------------------------------------- */

gp_port_type gp_port_library_type () {

        return (GP_PORT_USB);
}

gp_port_operations *gp_port_library_operations () {

        gp_port_operations *ops;

        ops = calloc(1, sizeof(gp_port_operations));
        if (!ops)
                return NULL;

        ops->init   = gp_port_usb_init;
        ops->exit   = gp_port_usb_exit;
        ops->open   = gp_port_usb_open;
        ops->close  = gp_port_usb_close;
        ops->read   = gp_port_usb_read;
        ops->write  = gp_port_usb_write;
        ops->update = gp_port_usb_update;
        ops->clear_halt = gp_port_usb_clear_halt_lib;
        ops->msg_write  = gp_port_usb_msg_write_lib;
        ops->msg_read   = gp_port_usb_msg_read_lib;
        ops->find_device = gp_port_usb_find_device_lib;

        return (ops);
}

int gp_port_library_list(gp_port_info *list, int *count)
{

        list[*count].type = GP_PORT_USB;
        strcpy(list[*count].name, "Universal Serial Bus");
        strcpy(list[*count].path, "usb:");
        /* list[*count].argument_needed = 0; */
        *count += 1;

        return GP_OK;
}

int gp_port_usb_init(gp_port *dev)
{
/*        usb_init();
        usb_find_busses();
        usb_find_devices();*/
        return (GP_OK);
}

int gp_port_usb_exit(gp_port *dev)
{
        return (GP_OK);
}

int gp_port_usb_open(gp_port *dev)
{
        int ret;
        void *udev;

        if (dev->debug_level)
            printf ("gp_port_usb_open() called\n");

        /* Open the device using the previous usb_handle returned by find_device */
        udev = dev->device_handle;
        /*dev->device_handle = usb_open(udev);*/
        if (!dev->device_handle)
                return GP_ERROR_IO_OPEN;

        /*ret = usb_set_configuration(dev->device_handle, dev->settings.usb.config);*/
        if (ret < 0) {
                fprintf(stderr, "gp_port_usb_open: could not set config %d: %s\n",
                        dev->settings.usb.config, strerror(errno));
                return GP_ERROR_IO_OPEN;
        }

        /*ret = usb_claim_interface(dev->device_handle, dev->settings.usb.interface);*/
        if (ret < 0) {
                fprintf(stderr, "gp_port_usb_open: could not claim intf %d: %s\n",
                        dev->settings.usb.interface, strerror(errno));
                return GP_ERROR_IO_OPEN;
        }

        /*ret = usb_set_altinterface(dev->device_handle, dev->settings.usb.altsetting);*/
        if (ret < 0) {
                fprintf(stderr, "gp_port_usb_open: could not set intf %d/%d: %s\n",
                        dev->settings.usb.interface,
                        dev->settings.usb.altsetting, strerror(errno));
                return GP_ERROR_IO_OPEN;
        }

        return GP_OK;
}

int gp_port_usb_close(gp_port *dev)
{
        if (dev->debug_level)
            printf ("gp_port_usb_close() called\n");

        /*if (usb_close(dev->device_handle) < 0)
                fprintf(stderr, "gp_port_usb_close: %s\n",
                        strerror(errno));       */

        dev->device_handle = NULL;

        return GP_OK;
}

int gp_port_usb_reset(gp_port *dev)
{
        /*gp_port_usb_close(dev);
        return gp_port_usb_open(dev);*/
}

int gp_port_usb_clear_halt_lib(gp_port * dev, int ep)
{
        int ret=0;

        switch (ep) {
                case GP_PORT_USB_ENDPOINT_IN :
                        /*ret=usb_clear_halt(dev->device_handle, dev->settings.usb.inep);*/
                        break;
                case GP_PORT_USB_ENDPOINT_OUT :
                        /*ret=usb_clear_halt(dev->device_handle, dev->settings.usb.outep);*/
                case GP_PORT_USB_ENDPOINT_INT :
                        /*ret=usb_clear_halt(dev->device_handle, dev->settings.usb.intep);*/
                        break;
                default:
                        fprintf(stderr,"gp_port_usb_clear_halt: bad EndPoint argument\n");
                        return GP_ERROR_IO_USB_CLEAR_HALT;
        }
        return (ret ? GP_ERROR_IO_USB_CLEAR_HALT : GP_OK);
}

int gp_port_usb_write(gp_port * dev, char *bytes, int size)
{
        int ret;
/*
        if (dev->debug_level) {
            int i;

            printf("gp_port_usb_write(): ");
            for (i = 0; i < size; i++)
                printf("%02x ",(unsigned char)bytes[i]);
            printf("\n");
        }
*/
        /*ret = usb_bulk_write(dev->device_handle, dev->settings.usb.outep,
                           bytes, size, dev->timeout);*/
        if (ret < 0)
            return (GP_ERROR_IO_WRITE);
        return (ret);
}

int gp_port_usb_read(gp_port * dev, char *bytes, int size)
{
        int ret;

        /*ret = usb_bulk_read(dev->device_handle, dev->settings.usb.inep,
                             bytes, size, dev->timeout);*/
        if (ret < 0)
                return GP_ERROR_IO_READ;

/*
        if (dev->debug_level) {
            int i;

            printf("gp_port_usb_read(timeout=%d): ", dev->timeout);
            for (i = 0; i < ret; i++)
                printf("%02x ",(unsigned char)(bytes[i]));
            printf("\n");
        }
*/

        return ret;
}

int gp_port_usb_msg_write_lib(gp_port * dev, int value, char *bytes, int size)
{
        /*return usb_control_msg(dev->device_handle,
                USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                size > 1 ? 0x04 : 0x0c, value, 0, bytes, size, dev->timeout);*/
}

int gp_port_usb_msg_read_lib(gp_port * dev, int value, char *bytes, int size)
{
        /*return usb_control_msg(dev->device_handle,
                USB_TYPE_VENDOR | USB_RECIP_DEVICE | 0x80,
                size > 1 ? 0x04 : 0x0c, value, 0, bytes, size, dev->timeout);*/
}

/*
 * This function applys changes to the device
 * (At this time it does nothing)
 */
int gp_port_usb_update(gp_port * dev)
{
        memcpy(&dev->settings, &dev->settings_pending, sizeof(dev->settings));

        return GP_OK;
}

int gp_port_usb_find_device_lib(gp_port * d, int idvendor, int idproduct)
{
        /*struct usb_bus *bus;
        struct usb_device *dev;
        for (bus = usb_busses; bus; bus = bus->next) {
                for (dev = bus->devices; dev; dev = dev->next) {
                        if ((dev->descriptor.idVendor == idvendor) &&
                            (dev->descriptor.idProduct == idproduct)) {
                                if (d)
                                    d->device_handle = dev;
                                return GP_OK;
                        }
                }
        } */

        return GP_ERROR_IO_USB_FIND;
}
