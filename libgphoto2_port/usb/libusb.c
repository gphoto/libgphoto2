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

	int config;
	int interface;
	int altsetting;
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

	port->pl->config = port->pl->interface = port->pl->altsetting = -1;

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
			"or kernel module (e.g. dc2xx) is using the device "
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
gp_port_usb_write (GPPort *port, const char *bytes, int size)
{
        int ret;

	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	ret = usb_bulk_write (port->pl->dh, port->settings.usb.outep,
                           (char *) bytes, size, port->timeout);
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
gp_port_usb_check_int (GPPort *port, char *bytes, int size)
{
	int ret;

	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	ret = usb_bulk_read(port->pl->dh, port->settings.usb.intep,
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
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
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

	memcpy(&port->settings.usb, &port->settings_pending.usb,
		sizeof(port->settings.usb));

	if (port->settings.usb.config != port->pl->config) {
		ret = usb_set_configuration(port->pl->dh,
				     port->settings.usb.config);
		if (ret < 0) {
			gp_port_set_error (port, 
				_("Could not set config %d/%d (%m)"),
				port->settings.usb.interface,
				port->settings.usb.config);
			return GP_ERROR_IO_UPDATE;	
		}

		gp_log (GP_LOG_DEBUG, "gphoto2-port-usb",
			"Changed usb.config from %d to %d",
			port->pl->config,
			port->settings.usb.config);

		/*
		 * Copy at once if something else fails so that this
		 * does not get re-applied
		 */
		port->pl->config = port->settings.usb.config;
	}

	if (port->settings.usb.altsetting != port->pl->altsetting) {
		ret = usb_set_altinterface(port->pl->dh, port->settings.usb.altsetting);
		if (ret < 0) {
			gp_port_set_error (port, 
				_("Could not set altsetting "
				"%d/%d (%m)"),
				port->settings.usb.interface,
				port->settings.usb.altsetting);
			return GP_ERROR_IO_UPDATE;
		}

		gp_log (GP_LOG_DEBUG, "gphoto2-port-usb",
			"Changed usb.altsetting from %d to %d",
			port->pl->altsetting,
			port->settings.usb.altsetting);
		port->pl->altsetting = port->settings.usb.altsetting;
	}

	return GP_OK;
}

static int
gp_port_usb_find_ep(struct usb_device *dev, int config, int interface, int altsetting, int direction, int type)
{
	struct usb_interface_descriptor *intf;
	int i;

	if (!dev->config)
		return -1;

	intf = &dev->config[config].interface[interface].altsetting[altsetting];

	for (i = 0; i < intf->bNumEndpoints; i++) {
		if ((intf->endpoint[i].bEndpointAddress & USB_ENDPOINT_DIR_MASK) == direction &&
		    (intf->endpoint[i].bmAttributes & USB_ENDPOINT_TYPE_MASK) == type)
			return intf->endpoint[i].bEndpointAddress;
	}

	return -1;
}

static int
gp_port_usb_find_first_altsetting(struct usb_device *dev, int *config, int *interface, int *altsetting)
{
	int i, i1, i2;

	if (!dev->config)
		return -1;

	for (i = 0; i < dev->descriptor.bNumConfigurations; i++)
		for (i1 = 0; i1 < dev->config[i].bNumInterfaces; i1++)
			for (i2 = 0; i2 < dev->config[i].interface[i1].num_altsetting; i2++)
				if (dev->config[i].interface[i1].altsetting[i2].bNumEndpoints) {
					*config = i;
					*interface = i1;
					*altsetting = i2;

					return 0;
				}

	return -1;
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
			"id (0x%x,0x%x) is not valid."), idvendor, idproduct);
		return GP_ERROR_BAD_PARAMETERS;
	}

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if ((dev->descriptor.idVendor == idvendor) &&
			    (dev->descriptor.idProduct == idproduct)) {
				int config, interface, altsetting;

				port->pl->d = dev;

				gp_log (GP_LOG_VERBOSE, "gphoto2-port-usb",
					"Looking for USB device "
					"(vendor 0x%x, product 0x%x)... found.", 
					idvendor, idproduct);

				/* Use the first config, interface and altsetting we find */
				gp_port_usb_find_first_altsetting(dev, &config, &interface, &altsetting);

				/* Set the defaults */
				if (dev->config) {
					port->settings.usb.config = dev->config[config].bConfigurationValue;
					port->settings.usb.interface = dev->config[config].interface[interface].altsetting[altsetting].bInterfaceNumber;
					port->settings.usb.altsetting = dev->config[config].interface[interface].altsetting[altsetting].bAlternateSetting;

					port->settings.usb.inep = gp_port_usb_find_ep(dev, config, interface, altsetting, USB_ENDPOINT_IN, USB_ENDPOINT_TYPE_BULK);
					port->settings.usb.outep = gp_port_usb_find_ep(dev, config, interface, altsetting, USB_ENDPOINT_OUT, USB_ENDPOINT_TYPE_BULK);
					port->settings.usb.intep = gp_port_usb_find_ep(dev, config, interface, altsetting, USB_ENDPOINT_IN, USB_ENDPOINT_TYPE_INTERRUPT);
					gp_log (GP_LOG_VERBOSE, "gphoto2-port-usb",
						"Detected defaults: config %d, "
						"interface %d, altsetting %d, "
						"inep %02x, outep %02x, intep %02x",
						port->settings.usb.config,
						port->settings.usb.interface,
						port->settings.usb.altsetting,
						port->settings.usb.inep,
						port->settings.usb.outep,
						port->settings.usb.intep
						);
				}

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
gp_port_usb_match_device_by_class(struct usb_device *dev, int class, int subclass, int protocol, int *configno, int *interfaceno, int *altsettingno)
{
	int i, i1, i2;

	if (dev->descriptor.bDeviceClass == class &&
	    (subclass == -1 ||
	     dev->descriptor.bDeviceSubClass == subclass) &&
	    (protocol == -1 ||
	     dev->descriptor.bDeviceProtocol == protocol))
		return 1;

	if (!dev->config)
		return 0;

	for (i = 0; i < dev->descriptor.bNumConfigurations; i++) {
		struct usb_config_descriptor *config =
			&dev->config[i];

		for (i1 = 0; i1 < config->bNumInterfaces; i1++) {
			struct usb_interface *interface =
				&config->interface[i1];

			for (i2 = 0; i2 < interface->num_altsetting; i2++) {
				struct usb_interface_descriptor *altsetting =
					&interface->altsetting[i2];

				if (altsetting->bInterfaceClass == class &&
				    (subclass == -1 ||
				     altsetting->bInterfaceSubClass == subclass) &&
				    (protocol == -1 ||
				     altsetting->bInterfaceProtocol == protocol)) {
					*configno = i;
					*interfaceno = i1;
					*altsettingno = i2;

					return 2;
				}
			}
		}
	}

	return 0;
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
			int ret, config, interface, altsetting;

			ret = gp_port_usb_match_device_by_class(dev, class, subclass, protocol, &config, &interface, &altsetting);
			if (!ret)
				continue;

			port->pl->d = dev;
			gp_log (GP_LOG_VERBOSE, "gphoto2-port-usb",
				"Looking for USB device "
				"(class 0x%x, subclass, 0x%x, protocol 0x%x)... found.", 
				class, subclass, protocol);
			/* Set the defaults */
			if (dev->config) {
				port->settings.usb.config = dev->config[config].bConfigurationValue;
				port->settings.usb.interface = dev->config[config].interface[interface].altsetting[altsetting].bInterfaceNumber;
				port->settings.usb.altsetting = dev->config[config].interface[interface].altsetting[altsetting].bAlternateSetting;

				port->settings.usb.inep = gp_port_usb_find_ep(dev, config, interface, altsetting, USB_ENDPOINT_IN, USB_ENDPOINT_TYPE_BULK);
				port->settings.usb.outep = gp_port_usb_find_ep(dev, config, interface, altsetting, USB_ENDPOINT_OUT, USB_ENDPOINT_TYPE_BULK);
				port->settings.usb.intep = gp_port_usb_find_ep(dev, config, interface, altsetting, USB_ENDPOINT_IN, USB_ENDPOINT_TYPE_INTERRUPT);
				gp_log (GP_LOG_VERBOSE, "gphoto2-port-usb",
					"Detected defaults: config %d, "
					"interface %d, altsetting %d, "
					"inep %02x, outep %02x, intep %02x",
					port->settings.usb.config,
					port->settings.usb.interface,
					port->settings.usb.altsetting,
					port->settings.usb.inep,
					port->settings.usb.outep,
					port->settings.usb.intep
					);
			}

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
	ops->check_int = gp_port_usb_check_int;
	ops->update = gp_port_usb_update;
	ops->clear_halt = gp_port_usb_clear_halt_lib;
	ops->msg_write  = gp_port_usb_msg_write_lib;
	ops->msg_read   = gp_port_usb_msg_read_lib;
	ops->find_device = gp_port_usb_find_device_lib;
	ops->find_device_by_class = gp_port_usb_find_device_by_class_lib;

	return (ops);
}

