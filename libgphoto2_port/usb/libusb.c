/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gphoto2-port-usb.c - USB transport functions

   Copyright (C) 1999-2000 Johannes Erdfelt <johannes@erdfelt.com>

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include <dirent.h>
#include <string.h>

#include <usb.h>
#include <gphoto2-port.h>
#include <gphoto2-port-result.h>
#include <gphoto2-port-debug.h>

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
int gp_port_usb_msg_read_lib(gp_port * dev, int request, int value, int index,
	char *bytes, int size);
int gp_port_usb_msg_write_lib(gp_port * dev, int request, int value, int index,
	char *bytes, int size);
int gp_port_usb_find_device_lib(gp_port *dev, int idvendor, int idproduct);

/* Dynamic library functions
   --------------------------------------------------------------------- */

gp_port_type gp_port_library_type () {

        return (GP_PORT_USB);
}

gp_port_operations *gp_port_library_operations () {

        gp_port_operations *ops;

        ops = (gp_port_operations*)malloc(sizeof(gp_port_operations));
        memset(ops, 0, sizeof(gp_port_operations));

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
	usb_init();
	usb_find_busses();
	usb_find_devices();
	return (GP_OK);
}

int gp_port_usb_exit(gp_port *dev)
{
	return (GP_OK);
}

int gp_port_usb_open(gp_port *dev)
{
	int ret;

	if (!dev) {
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_open: called on NULL device");
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_port_debug_printf (GP_DEBUG_HIGH, "gp_port_usb_open() called");

        /*
	 * Open the device using the previous usb_handle returned by
	 * find_device
	 */
	if (!dev->device) {
		gp_port_debug_printf (GP_DEBUG_LOW, "dev->device is NULL!");
		return GP_ERROR_BAD_PARAMETERS;
	}
        dev->device_handle = usb_open (dev->device);
	if (!dev->device_handle)
		return GP_ERROR_IO_OPEN;

	ret = usb_claim_interface (dev->device_handle,
				   dev->settings.usb.interface);
	if (ret < 0) {
		gp_port_debug_printf (GP_DEBUG_LOW, "Could not claim "
				      "interface %d: %s",
				      dev->settings.usb.interface,
				      strerror (errno));
		return GP_ERROR_IO_USB_CLAIM;
	}
	
	return GP_OK;
}

int gp_port_usb_close (gp_port *dev)
{
	if (!dev || dev->device_handle == NULL) {
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_close: called on ",
					(!dev)?"NULL device":"non-open device handle");
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_port_debug_printf (GP_DEBUG_HIGH, "gp_port_usb_close() called");

	if (usb_release_interface (dev->device_handle,
				   dev->settings.usb.interface) < 0) {
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_close: could not release "
					"interface %d: %s\n",
			 dev->settings.usb.interface, strerror (errno));
		return (GP_ERROR_IO_CLOSE);
	}

	if (usb_close (dev->device_handle) < 0)
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_close: %s\n",
					strerror (errno));

	dev->device_handle = NULL;

	return GP_OK;
}

int gp_port_usb_reset(gp_port *dev)
{
	if (!dev) {
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_reset: called on NULL device");
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_port_debug_printf (GP_DEBUG_HIGH, "gp_port_usb_reset() called");

	/* XXX should we check for errors on close too or
	 * should we do what we do now, return the result of a
	 * fresh open?
	 */
	gp_port_usb_close(dev);
	return gp_port_usb_open(dev);
}

int gp_port_usb_clear_halt_lib(gp_port * dev, int ep)
{
	int ret=0;

	if (!dev || dev->device_handle == NULL) {
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_clear_halt_lib: called on ",
					(!dev)?"NULL device":"non-open device handle");
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_port_debug_printf (GP_DEBUG_HIGH, "gp_port_usb_clear_halt_lib() called");

	switch (ep) {
		case GP_PORT_USB_ENDPOINT_IN :
			ret=usb_clear_halt(dev->device_handle, dev->settings.usb.inep);
			break;
		case GP_PORT_USB_ENDPOINT_OUT :
			ret=usb_clear_halt(dev->device_handle, dev->settings.usb.outep);
			break;
		default:
			gp_port_debug_printf (GP_DEBUG_NONE,
				"gp_port_usb_clear_halt: bad EndPoint argument\n");
			return GP_ERROR_BAD_PARAMETERS;
	}
	return (ret ? GP_ERROR_IO_USB_CLEAR_HALT : GP_OK);
}

int gp_port_usb_write(gp_port * dev, char *bytes, int size)
{
        int ret;

	if (!dev || dev->device_handle == NULL) {
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_write: called on ",
					(!dev)?"NULL device":"non-open device handle");
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_port_debug_printf (GP_DEBUG_HIGH, "gp_port_usb_write() called");

	ret = usb_bulk_write(dev->device_handle, dev->settings.usb.outep,
                           bytes, size, dev->timeout);
        if (ret < 0)
		return (GP_ERROR_IO_WRITE);

        return (ret);
}

int gp_port_usb_read(gp_port * dev, char *bytes, int size)
{
	int ret;

	if (!dev || dev->device_handle == NULL) {
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_read: called on ",
					(!dev)?"NULL device":"non-open device handle");
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_port_debug_printf (GP_DEBUG_HIGH, "gp_port_usb_read() called");

	ret = usb_bulk_read(dev->device_handle, dev->settings.usb.inep,
			     bytes, size, dev->timeout);
        if (ret < 0)
		return GP_ERROR_IO_READ;

        return ret;
}

int gp_port_usb_msg_write_lib(gp_port * dev, int request, int value, int index,
	char *bytes, int size)
{
	if (!dev || dev->device_handle == NULL) {
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_msg_write_lib: called on ",
					(!dev)?"NULL device":"non-open device handle");
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_port_debug_printf (GP_DEBUG_HIGH, "gp_port_usb_write_lib() called");

	return usb_control_msg(dev->device_handle,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		request, value, index, bytes, size, dev->timeout);
}

int gp_port_usb_msg_read_lib(gp_port * dev, int request, int value, int index,
	char *bytes, int size)
{
	if (!dev || dev->device_handle == NULL) {
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_msg_read_lib: called on ",
					(!dev)?"NULL device":"non-open device handle");
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_port_debug_printf (GP_DEBUG_HIGH, "gp_port_usb_msg_read_lib() called");

	return usb_control_msg(dev->device_handle,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | 0x80,
		request, value, index, bytes, size, dev->timeout);
}

/*
 * This function applys changes to the device.
 *
 * New settings are in dev->settings_pending and the old ones
 * are in dev->settings. Compare them first and only call
 * usb_set_configuration() and usb_set_altinterface() if needed
 * since some USB devices does not like it if this is done
 * more than necessary (Canon Digital IXUS 300 for one).
 *
 */
int gp_port_usb_update(gp_port * dev)
{
	int ret;

	if (!dev || dev->device_handle == NULL) {
		gp_port_debug_printf (GP_DEBUG_NONE,
					"gp_port_usb_update: called on %s",
					(!dev)?"NULL device":"non-open device handle");
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_port_debug_printf (GP_DEBUG_HIGH, "gp_port_usb_update() called.");

	if (memcmp(&dev->settings.usb, &dev->settings_pending.usb, sizeof(dev->settings.usb))) {

		/* settings.usb structure is different than the old one */
		 
		if (dev->settings.usb.config != dev->settings_pending.usb.config) {
			ret = usb_set_configuration(dev->device_handle,
					     dev->settings_pending.usb.config);
			if (ret < 0) {
				gp_port_debug_printf (GP_DEBUG_LOW,
					"gp_port_usb_update: "
					"Could not set config %d/%d: %s",
					dev->settings_pending.usb.interface,
					dev->settings_pending.usb.config,
					strerror(errno));
				return GP_ERROR_IO_UPDATE;	
			}

			gp_port_debug_printf (GP_DEBUG_LOW,
				"gp_port_usb_update: "
				"Changed usb.config from %d to %d",
				dev->settings.usb.config,
				dev->settings_pending.usb.config);
			
			/*
			 * Copy at once if something else fails so that this
			 * does not get re-applied
			 */
			dev->settings.usb.config = dev->settings_pending.usb.config;
		}

		if (dev->settings.usb.altsetting != dev->settings_pending.usb.altsetting) { 
			ret = usb_set_altinterface(dev->device_handle, dev->settings_pending.usb.altsetting);
			if (ret < 0) {
				gp_port_debug_printf (GP_DEBUG_LOW,
					"gp_port_usb_update: "
					"Could not set interface %d/%d: %s",
					dev->settings_pending.usb.interface,
					dev->settings_pending.usb.altsetting,
					strerror(errno));
				return GP_ERROR_IO_UPDATE;
			}

			gp_port_debug_printf (GP_DEBUG_HIGH,
				"gp_port_usb_update: "
				"Changed usb.altsetting from %d to %d",
				dev->settings.usb.altsetting,
				dev->settings_pending.usb.altsetting);
		}
		
		memcpy (&dev->settings.usb, &dev->settings_pending.usb,
			sizeof (dev->settings.usb));
	}
	
	return GP_OK;
}

int gp_port_usb_find_device_lib(gp_port * d, int idvendor, int idproduct)
{
	struct usb_bus *bus;
	struct usb_device *dev;

	/*
	 * NULL idvendor and idproduct is not valid.
	 * Should the USB layer report that ? I don't know.
	 * Better to check here.
	 */
	if ((idvendor == 0) || (idproduct == 0)) {
		return GP_ERROR_BAD_PARAMETERS;
	}
	gp_port_debug_printf (GP_DEBUG_HIGH, "gp_port_usb_find_device_lib() called "
			      "(idvendor=0x%x, idproduct=0x%x", idvendor, idproduct);

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if ((dev->descriptor.idVendor == idvendor) &&
			    (dev->descriptor.idProduct == idproduct)) {
				/* XXX shouldn't we check 'if (d)' at the
				 * beginning of this function? 
				 */
                                if (d)
                                    d->device = dev;
				return GP_OK;
			}
		}
	}

	return GP_ERROR_IO_USB_FIND;
}
