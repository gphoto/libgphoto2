/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gphoto2-port-usb.c
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
 * Copyright (C) 1999-2000 Johannes Erdfelt <johannes@erdfelt.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <config.h>
#include <gphoto2-port-library.h>

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
#include <gphoto2-port-log.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CHECK(result) {int r=(result); if (r<0) return (r);}

struct _GPPortPrivateLibrary {
	void *dh;
	struct usb_device *d;
};

GPPortType
gp_port_library_type (void)
{
        return (GP_PORT_USB);
}

int
gp_port_library_list (GPPortInfoList *list)
{
	GPPortInfo info;

	info.type = GP_PORT_USB;
	strcpy (info.name, "Universal Serial Bus");
        strcpy (info.path, "usb:");
	CHECK (gp_port_info_list_append (list, info));

	return (GP_OK);
}

static int gp_port_usb_init (GPPort *port)
{
	port->pl = malloc (sizeof (GPPortPrivateLibrary));
	if (!port->pl)
		return (GP_ERROR_NO_MEMORY);
	memset (port->pl, 0, sizeof (GPPortPrivateLibrary));

	usb_init ();
	usb_find_busses ();
	usb_find_devices ();

	return (GP_OK);
}

static int
gp_port_usb_exit (GPPort *port)
{
	if (port->pl) {
		free (port->pl);
		port->pl = NULL;
	}

	return (GP_OK);
}

static int
gp_port_usb_open (GPPort *port)
{
	int ret;

	if (!port || !port->pl->d)
		return GP_ERROR_BAD_PARAMETERS;

        /*
	 * Open the device using the previous usb_handle returned by
	 * find_device
	 */
        port->pl->dh = usb_open (port->pl->d);
	if (!port->pl->dh) {
		gp_port_set_error (port, _("Could not open USB device (%m)."));
		return GP_ERROR_IO;
	}

	ret = usb_claim_interface (port->pl->dh,
				   port->settings.usb.interface);
	if (ret < 0) {
		gp_port_set_error (port, _("Could not claim "
			"interface %d (%m). Make sure no other program "
			"or kernel module (i.e. dc2xx) is using the device "
			"and you have read/write access to the device."),
			port->settings.usb.interface);
		return GP_ERROR_IO_USB_CLAIM;
	}
	
	return GP_OK;
}

static int
gp_port_usb_close (GPPort *port)
{
	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	if (usb_release_interface (port->pl->dh,
				   port->settings.usb.interface) < 0) {
		gp_port_set_error (port, _("Could not "
			"release interface %d (%m)."),
			port->settings.usb.interface);
		return (GP_ERROR_IO);
	}

	if (usb_close (port->pl->dh) < 0) {
		gp_port_set_error (port, _("Could not close USB port (%m)."));
		return (GP_ERROR_IO);
	}

	port->pl->dh = NULL;

	return GP_OK;
}

static int
gp_port_usb_clear_halt_lib(GPPort *port, int ep)
{
	int ret=0;

	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	switch (ep) {
	case GP_PORT_USB_ENDPOINT_IN :
		ret=usb_clear_halt(port->pl->dh, port->settings.usb.inep);
		break;
	case GP_PORT_USB_ENDPOINT_OUT :
		ret=usb_clear_halt(port->pl->dh, port->settings.usb.outep);
		break;
	default:
		gp_port_set_error (port, "gp_port_usb_clear_halt: "
				   "bad EndPoint argument");
		return GP_ERROR_BAD_PARAMETERS;
	}
	return (ret ? GP_ERROR_IO_USB_CLEAR_HALT : GP_OK);
}

static int
gp_port_usb_write(GPPort *port, char *bytes, int size)
{
        int ret;

	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	ret = usb_bulk_write(port->pl->dh, port->settings.usb.outep,
                           bytes, size, port->timeout);
        if (ret < 0)
		return (GP_ERROR_IO_WRITE);

        return (ret);
}

static int
gp_port_usb_read(GPPort *port, char *bytes, int size)
{
	int ret;

	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	ret = usb_bulk_read(port->pl->dh, port->settings.usb.inep,
			     bytes, size, port->timeout);
        if (ret < 0)
		return GP_ERROR_IO_READ;

        return ret;
}

static int
gp_port_usb_msg_write_lib(GPPort *port, int request, int value, int index,
	char *bytes, int size)
{
	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	return usb_control_msg(port->pl->dh,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		request, value, index, bytes, size, port->timeout);
}

static int
gp_port_usb_msg_read_lib(GPPort *port, int request, int value, int index,
	char *bytes, int size)
{
	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	return usb_control_msg(port->pl->dh,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | 0x80,
		request, value, index, bytes, size, port->timeout);
}

/*
 * This function applys changes to the device.
 *
 * New settings are in port->settings_pending and the old ones
 * are in port->settings. Compare them first and only call
 * usb_set_configuration() and usb_set_altinterface() if needed
 * since some USB devices does not like it if this is done
 * more than necessary (Canon Digital IXUS 300 for one).
 *
 */
static int
gp_port_usb_update (GPPort *port)
{
	int ret;

	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	if (memcmp (&port->settings.usb, &port->settings_pending.usb,
		    sizeof(port->settings.usb))) {

		/* settings.usb structure is different than the old one */
		 
		if (port->settings.usb.config != port->settings_pending.usb.config) {
			ret = usb_set_configuration(port->pl->dh,
					     port->settings_pending.usb.config);
			if (ret < 0) {
				gp_port_set_error (port, 
					_("Could not set config %d/%d (%m)"),
					port->settings_pending.usb.interface,
					port->settings_pending.usb.config);
				return GP_ERROR_IO_UPDATE;	
			}

			gp_log (GP_LOG_DEBUG, "gphoto2-port-usb",
				"Changed usb.config from %d to %d",
				port->settings.usb.config,
				port->settings_pending.usb.config);
			
			/*
			 * Copy at once if something else fails so that this
			 * does not get re-applied
			 */
			port->settings.usb.config = port->settings_pending.usb.config;
		}

		if (port->settings.usb.altsetting != port->settings_pending.usb.altsetting) { 
			ret = usb_set_altinterface(port->pl->dh, port->settings_pending.usb.altsetting);
			if (ret < 0) {
				gp_port_set_error (port, 
					_("Could not set interface "
					"%d/%d (%m)"),
					port->settings_pending.usb.interface,
					port->settings_pending.usb.altsetting);
				return GP_ERROR_IO_UPDATE;
			}

			gp_log (GP_LOG_DEBUG, "gphoto2-port-usb",
				"Changed usb.altsetting from %d to %d",
				port->settings.usb.altsetting,
				port->settings_pending.usb.altsetting);
		}
		
		memcpy (&port->settings.usb, &port->settings_pending.usb,
			sizeof (port->settings.usb));
	}
	
	return GP_OK;
}

static int
gp_port_usb_find_device_lib(GPPort *port, int idvendor, int idproduct)
{
	struct usb_bus *bus;
	struct usb_device *dev;

	if (!port)
		return (GP_ERROR_BAD_PARAMETERS);

	/*
	 * NULL idvendor is not valid.
	 * NULL idproduct is ok.
	 * Should the USB layer report that ? I don't know.
	 * Better to check here.
	 */
	if (!idvendor) {
		gp_port_set_error (port, _("The supplied vendor or product "
			"id (%i,%i) is not valid."));
		return GP_ERROR_BAD_PARAMETERS;
	}

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if ((dev->descriptor.idVendor == idvendor) &&
			    (dev->descriptor.idProduct == idproduct)) {
                                    port->pl->d = dev;
				    gp_log (GP_LOG_VERBOSE, "gphoto2-port-usb",
					    "Looking for USB device "
					    "(vendor 0x%x, product 0x%x)... found.", 
					    idvendor, idproduct);
				    return GP_OK;
			}
		}
	}

	gp_port_set_error (port, _("Could not find USB device "
		"(vendor 0x%x, product 0x%x). Make sure this device "
		"is connected to the computer."), idvendor, idproduct);
	return GP_ERROR_IO_USB_FIND;
}

static int
gp_port_usb_find_device_by_class_lib(GPPort *port, int class, int subclass, int protocol)
{
	struct usb_bus *bus;
	struct usb_device *dev;

	if (!port)
		return (GP_ERROR_BAD_PARAMETERS);

	/*
	 * NULL class is not valid.
	 * NULL subclass and protocol is ok.
	 * Should the USB layer report that ? I don't know.
	 * Better to check here.
	 */
	if (!class)
		return GP_ERROR_BAD_PARAMETERS;

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.bDeviceClass != class)
				continue;

			if (subclass != -1 &&
			    dev->descriptor.bDeviceSubClass != subclass)
				continue;

			if (protocol != -1 &&
			    dev->descriptor.bDeviceProtocol != protocol)
				continue;

			port->pl->d = dev;
			gp_log (GP_LOG_VERBOSE, "gphoto2-port-usb",
				"Looking for USB device "
				"(class 0x%x, subclass, 0x%x, protocol 0x%x)... found.", 
				class, subclass, protocol);
			return GP_OK;
		}
	}

	gp_port_set_error (port, _("Could not find USB device "
		"(class 0x%x, subclass 0x%x, protocol 0x%x). Make sure this device "
		"is connected to the computer."), class, subclass, protocol);
	return GP_ERROR_IO_USB_FIND;
}

GPPortOperations *
gp_port_library_operations (void)
{
	GPPortOperations *ops;
	
	ops = malloc (sizeof (GPPortOperations));
	if (!ops)
		return (NULL);
	memset (ops, 0, sizeof (GPPortOperations));

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
	ops->find_device_by_class = gp_port_usb_find_device_by_class_lib;

	return (ops);
}

