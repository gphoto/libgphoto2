/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gphoto2-port-usb.c
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
 * Copyright © 1999-2000 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright © 2011 Marcus Meissner <marcus@jet.franken.de>
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
#include "config.h"
#include <gphoto2/gphoto2-port-library.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include <dirent.h>
#include <string.h>

#include <libusb.h>

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-log.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
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
	libusb_context *ctx;
	libusb_device *d;
	libusb_device_handle *dh;

	int config;
	int interface;
	int altsetting;

	int detached;

};

GPPortType
gp_port_library_type (void)
{
	return (GP_PORT_USB);
}

/* FIXME: timeout this cached information */
static time_t devslastchecked = 0;
static int nrofdevs = 0;
static libusb_device **devs = NULL;

static ssize_t
load_devicelist (void) {
	time_t	xtime;

	time(&xtime);
	if (xtime != devslastchecked) {
		if (nrofdevs)
			libusb_free_device_list (devs, 0);
		nrofdevs = 0;
		devs = NULL;
	}
	if (!nrofdevs)
		nrofdevs = libusb_get_device_list (NULL, &devs);
	return nrofdevs;
}

int
gp_port_library_list (GPPortInfoList *list)
{
	GPPortInfo	info;
	int		nrofdevices = 0;
	int		d, i, i1, i2, unknownint;

	/* generic matcher. This will catch passed XXX,YYY entries for instance. */
	gp_port_info_new (&info);
	gp_port_info_set_type (info, GP_PORT_USB);
	gp_port_info_set_name (info, "");
	gp_port_info_set_path (info, "^usb:");
	CHECK (gp_port_info_list_append (list, info));

	libusb_init (NULL);
	nrofdevs = load_devicelist ();

	for (d = 0; d < nrofdevs; d++) {
		struct libusb_device_descriptor desc;
		int ret;

		ret = libusb_get_device_descriptor(devs[d], &desc);
		if (ret) {
			gp_log (GP_LOG_ERROR, "libusb1", "libusb_get_device_descriptor(%d) returned %d", d, ret);
			continue;
		}

		/* Devices which are definitely not cameras. */
		if (	(desc.bDeviceClass == LIBUSB_CLASS_HUB)		||
			(desc.bDeviceClass == LIBUSB_CLASS_HID)		||
			(desc.bDeviceClass == LIBUSB_CLASS_PRINTER)	||
			(desc.bDeviceClass == LIBUSB_CLASS_COMM)	||
			(desc.bDeviceClass == 0xe0)	/* wireless / bluetooth */
		)
			continue;
		/* excepts HUBs, usually the interfaces have the classes, not
		 * the device */
		unknownint = 0;
		for (i = 0; i < desc.bNumConfigurations; i++) {
			struct libusb_config_descriptor *config;
			int ret;

			ret = libusb_get_config_descriptor (devs[d], i, &config);
			if (ret) {
				unknownint++;
				continue;
			}
			for (i1 = 0; i1 < config->bNumInterfaces; i1++)
				for (i2 = 0; i2 < config->interface[i1].num_altsetting; i2++) {
					const struct libusb_interface_descriptor *intf = &config->interface[i1].altsetting[i2]; 
					if (	(intf->bInterfaceClass == LIBUSB_CLASS_HID)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_PRINTER)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_COMM)	||
						(intf->bInterfaceClass == 0xe0)	/* wireless/bluetooth*/
					)
						continue;
					unknownint++;
				}
		}
		/* when we find only hids, printer or comm ifaces  ... skip this */
		if (!unknownint)
			continue;
		/* Note: We do not skip USB storage. Some devices can support both,
		 * and the Ricoh erronously reports it.
		 */ 
		nrofdevices++;
	}

#if 0
	/* If we already added usb:, and have 0 or 1 devices we have nothing to do.
	 * This should be the standard use case.
	 */
	/* We never want to return just "usb:" ... also return "usb:XXX,YYY", and
	 * let upper layers filter out the usb: */
	if (nrofdevices <= 1) 
		return (GP_OK);
#endif

	/* Redo the same bus/device walk, but now add the ports with usb:x,y notation,
	 * so we can address all USB devices.
	 */
	for (d = 0; d < nrofdevs; d++) {
		struct libusb_device_descriptor desc;
		int ret;
		char path[200];

		ret = libusb_get_device_descriptor (devs[d], &desc);
		if (ret) {
			gp_log (GP_LOG_ERROR, "libusb1", "libusb_get_device_descriptor(%d) returned %d", d, ret);
			continue;
		}

		/* Devices which are definitely not cameras. */
		if (	(desc.bDeviceClass == LIBUSB_CLASS_HUB)		||
			(desc.bDeviceClass == LIBUSB_CLASS_HID)		||
			(desc.bDeviceClass == LIBUSB_CLASS_PRINTER)	||
			(desc.bDeviceClass == LIBUSB_CLASS_COMM)
		)
			continue;
		/* excepts HUBs, usually the interfaces have the classes, not
		 * the device */
		unknownint = 0;
		for (i = 0; i < desc.bNumConfigurations; i++) {
			struct libusb_config_descriptor *config;
			int ret;

			ret = libusb_get_config_descriptor (devs[d], i, &config);
			if (ret) {
				gp_log (GP_LOG_ERROR, "libusb1", "libusb_get_config_descriptor(%d) returned %d", d, ret);
				unknownint++;
				continue;
			}
			for (i1 = 0; i1 < config->bNumInterfaces; i1++)
				for (i2 = 0; i2 < config->interface[i1].num_altsetting; i2++) {
					const struct libusb_interface_descriptor *intf = &config->interface[i1].altsetting[i2]; 
					if (	(intf->bInterfaceClass == LIBUSB_CLASS_HID)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_PRINTER)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_COMM))
						continue;
					unknownint++;
				}
		}
		/* when we find only hids, printer or comm ifaces  ... skip this */
		if (!unknownint)
			continue;
		/* Note: We do not skip USB storage. Some devices can support both,
		 * and the Ricoh erronously reports it.
		 */ 
		gp_port_info_new (&info);
		gp_port_info_set_type (info, GP_PORT_USB);
		gp_port_info_set_name (info, "Universal Serial Bus");
		snprintf (path,sizeof(path), "usb:%03d,%03d",
			libusb_get_bus_number (devs[d]),
			libusb_get_device_address (devs[d])
		);
		gp_port_info_set_path (info, path);
		CHECK (gp_port_info_list_append (list, info));
	}
	/* This will only be added if no other device was ever added.
	 * Users doing "usb:" usage will enter the regular expression matcher case. */
	if (nrofdevices == 0) {
		gp_port_info_new (&info);
		gp_port_info_set_type (info, GP_PORT_USB);
		gp_port_info_set_name (info, "Universal Serial Bus");
		gp_port_info_set_path (info, "usb:");
		CHECK (gp_port_info_list_append (list, info));
	}
	libusb_exit (NULL);
	return (GP_OK);
}

static int gp_port_usb_init (GPPort *port)
{
	port->pl = malloc (sizeof (GPPortPrivateLibrary));
	if (!port->pl)
		return (GP_ERROR_NO_MEMORY);
	memset (port->pl, 0, sizeof (GPPortPrivateLibrary));

	port->pl->config = port->pl->interface = port->pl->altsetting = -1;

	libusb_init (&port->pl->ctx);
	libusb_set_debug (port->pl->ctx, 255);

#if 0
	usb_init ();
	usb_find_busses ();
	usb_find_devices ();
#endif

	return (GP_OK);
}

static int
gp_port_usb_exit (GPPort *port)
{
	if (port->pl) {
		libusb_exit (port->pl->ctx);
		free (port->pl);
		port->pl = NULL;
	}

	return (GP_OK);
}

static int
gp_port_usb_open (GPPort *port)
{
	int ret;

	gp_log (GP_LOG_DEBUG,"libusb1","gp_port_usb_open()");
	if (!port || !port->pl->d)
		return GP_ERROR_BAD_PARAMETERS;

	ret = libusb_open (port->pl->d, &port->pl->dh);
	if (ret) {
		gp_log (GP_LOG_ERROR, "libusb1", "libusb_open returned %d", ret);
		return GP_ERROR_IO;
	}
	if (!port->pl->dh) {
		int saved_errno = errno;
		gp_port_set_error (port, _("Could not open USB device (%s)."),
				   strerror(saved_errno));
		return GP_ERROR_IO;
	}
	ret = libusb_kernel_driver_active (port->pl->dh, port->settings.usb.interface);

#if 0
	if (strstr(name,"usbfs") || strstr(name,"storage")) {
		/* other gphoto instance most likely */
		gp_port_set_error (port, _("Camera is already in use."));
		return GP_ERROR_IO_LOCK;
	}
#endif

	if (ret >= 0) {
		gp_log (GP_LOG_DEBUG,"libusb1",_("Device has a kernel driver attached (%d), detaching it now."), ret);
		ret = libusb_detach_kernel_driver (port->pl->dh, port->settings.usb.interface);
		if (ret < 0)
			gp_port_set_error (port, _("Could not detach kernel driver of camera device."));
		else
			port->pl->detached = 1;
	} else {
		if (errno != ENODATA) /* ENODATA - just no driver there */
			gp_port_set_error (port, _("Could not query kernel driver of device."));
	}

	gp_log (GP_LOG_DEBUG,"libusb1","claiming interface %d", port->settings.usb.interface);
	ret = libusb_claim_interface (port->pl->dh,
				   port->settings.usb.interface);
	if (ret < 0) {
		int saved_errno = errno;
		gp_port_set_error (port, _("Could not claim interface %d (%s). "
					   "Make sure no other program "
					   "or kernel module (such as %s) "
					   "is using the device and you have "
					   "read/write access to the device."),
				   port->settings.usb.interface,
				   strerror(saved_errno),
				   "sdc2xx, stv680, spca50x");
		return GP_ERROR_IO_USB_CLAIM;
	}
	return GP_OK;
}

static int
gp_port_usb_close (GPPort *port)
{
	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	if (libusb_release_interface (port->pl->dh,
				   port->settings.usb.interface) < 0) {
		int saved_errno = errno;
		gp_port_set_error (port, _("Could not release "
					   "interface %d (%s)."),
				   port->settings.usb.interface,
				   strerror(saved_errno));
		return (GP_ERROR_IO);
	}

#if 0
	/* This confuses the EOS 5d camera and possible other EOSs. *sigh* */
	/* This is only for our very special Canon cameras which need a good
	 * whack after close, otherwise they get timeouts on reconnect.
	 */
	if (port->pl->d->descriptor.idVendor == 0x04a9) {
		if (usb_reset (port->pl->dh) < 0) {
			int saved_errno = errno;
			gp_port_set_error (port, _("Could not reset "
						   "USB port (%s)."),
					   strerror(saved_errno));
			return (GP_ERROR_IO);
		}
	}
#endif
	if (port->pl->detached) {
		int ret;
		ret = libusb_attach_kernel_driver (port->pl->dh, port->settings.usb.interface);
		if (ret < 0)
			gp_port_set_error (port, _("Could not reattach kernel driver of camera device."));
	}

	libusb_close (port->pl->dh);
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
		ret=libusb_clear_halt(port->pl->dh, port->settings.usb.inep);
		break;
	case GP_PORT_USB_ENDPOINT_OUT :
		ret=libusb_clear_halt(port->pl->dh, port->settings.usb.outep);
		break;
	case GP_PORT_USB_ENDPOINT_INT :
		ret=libusb_clear_halt(port->pl->dh, port->settings.usb.intep);
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
        int ret, curwritten;

	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	ret = libusb_bulk_transfer (port->pl->dh, port->settings.usb.outep,
                           (unsigned char*)bytes, size, &curwritten, port->timeout);
        if (ret < 0)
		return (GP_ERROR_IO_WRITE);
        return curwritten;
}

static int
gp_port_usb_read(GPPort *port, char *bytes, int size)
{
	int ret, curread;

	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	ret = libusb_bulk_transfer (port->pl->dh, port->settings.usb.inep,
			     (unsigned char*)bytes, size, &curread, port->timeout);
        if (ret < 0)
		return GP_ERROR_IO_READ;

        return curread;
}

static int
gp_port_usb_check_int (GPPort *port, char *bytes, int size, int timeout)
{
	int ret, curread;

	if (!port || !port->pl->dh || timeout < 0)
		return GP_ERROR_BAD_PARAMETERS;

	ret = libusb_interrupt_transfer (port->pl->dh, port->settings.usb.intep,
			     (unsigned char*)bytes, size, &curread, timeout);
        if (ret < 0) {
		if ((errno == EAGAIN) || (errno == ETIMEDOUT))
			return GP_ERROR_TIMEOUT;
		return GP_ERROR_IO_READ;
	}
        return curread;
}

static int
gp_port_usb_msg_write_lib(GPPort *port, int request, int value, int index,
	char *bytes, int size)
{
	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	return libusb_control_transfer (port->pl->dh,
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
		request, value, index, (unsigned char*)bytes, size, port->timeout);
}

static int
gp_port_usb_msg_read_lib(GPPort *port, int request, int value, int index,
	char *bytes, int size)
{
	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	return libusb_control_transfer (port->pl->dh,
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
		request, value, index, (unsigned char*)bytes, size, port->timeout);
}

/* The next two functions support the nonstandard request types 0x41 (write) 
 * and 0xc1 (read), which are occasionally needed. 
 */

static int
gp_port_usb_msg_interface_write_lib(GPPort *port, int request, 
	int value, int index, char *bytes, int size)
{
	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	return libusb_control_transfer (port->pl->dh, 
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE,
		request, value, index, (unsigned char*)bytes, size, port->timeout);
}


static int
gp_port_usb_msg_interface_read_lib(GPPort *port, int request, 
	int value, int index, char *bytes, int size)
{
	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	return libusb_control_transfer (port->pl->dh, 
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_IN,
		request, value, index, (unsigned char*)bytes, size, port->timeout);
}


/* The next two functions support the nonstandard request types 0x21 (write) 
 * and 0xa1 (read), which are occasionally needed. 
 */

static int
gp_port_usb_msg_class_write_lib(GPPort *port, int request, 
	int value, int index, char *bytes, int size)
{
	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	return libusb_control_transfer (port->pl->dh,
		LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		request, value, index, (unsigned char*)bytes, size, port->timeout);
}


static int
gp_port_usb_msg_class_read_lib(GPPort *port, int request, 
	int value, int index, char *bytes, int size)
{
	if (!port || !port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	return libusb_control_transfer (port->pl->dh, 
		LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_IN,
		request, value, index, (unsigned char*)bytes, size, port->timeout);
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
	int ret, ifacereleased = FALSE;

	if (!port)
		return GP_ERROR_BAD_PARAMETERS;

	gp_log (GP_LOG_DEBUG, "libusb1", "gp_port_usb_update(old int=%d, conf=%d, alt=%d), (new int=%d, conf=%d, alt=%d)",
		port->settings.usb.interface,
		port->settings.usb.config,
		port->settings.usb.altsetting,
		port->settings_pending.usb.interface,
		port->settings_pending.usb.config,
		port->settings_pending.usb.altsetting
	);

	if (port->pl->interface == -1) port->pl->interface = port->settings.usb.interface;
	if (port->pl->config == -1) port->pl->config = port->settings.usb.config;
	if (port->pl->altsetting == -1) port->pl->altsetting = port->settings.usb.altsetting;

	/* The portname can also be changed with the device still fully closed. */
	memcpy(&port->settings.usb.port, &port->settings_pending.usb.port,
		sizeof(port->settings.usb.port));

 	if (!port->pl->dh)
		return GP_ERROR_BAD_PARAMETERS;

	memcpy(&port->settings.usb, &port->settings_pending.usb,
		sizeof(port->settings.usb));

	/* The interface changed. release the old, claim the new ... */
	if (port->settings.usb.interface != port->pl->interface) {
		gp_log (GP_LOG_DEBUG, "libusb1", "changing interface %d -> %d",
			port->pl->interface,
			port->settings.usb.interface
		);
		if (libusb_release_interface (port->pl->dh,
					   port->pl->interface) < 0) {
			gp_log (GP_LOG_DEBUG, "libusb1","releasing the iface for config failed.");
			/* Not a hard error for now. -Marcus */
		} else {
			gp_log (GP_LOG_DEBUG,"libusb1","claiming interface %d", port->settings.usb.interface);
			ret = libusb_claim_interface (port->pl->dh,
						   port->settings.usb.interface);
			if (ret < 0) {
				gp_log (GP_LOG_DEBUG, "libusb1","reclaiming the iface for config failed.");
				return GP_ERROR_IO_UPDATE;
			}
			port->pl->interface = port->settings.usb.interface;
		}
	}
	if (port->settings.usb.config != port->pl->config) {
		gp_log (GP_LOG_DEBUG, "libusb1", "changing config %d -> %d",
			port->pl->config,
			port->settings.usb.config
		);
		/* This can only be changed with the interface released. 
		 * This is a hard requirement since 2.6.12.
		 */
		if (libusb_release_interface (port->pl->dh,
					   port->settings.usb.interface) < 0) {
			gp_log (GP_LOG_DEBUG, "libusb1","releasing the iface for config failed.");
			ifacereleased = FALSE;
		} else {
			ifacereleased = TRUE;
		}
		ret = libusb_set_configuration(port->pl->dh,
				     port->settings.usb.config);
		if (ret < 0) {
#if 0 /* setting the configuration failure is not fatal */
			int saved_errno = errno;
			gp_port_set_error (port,
					   _("Could not set config %d/%d (%s)"),
					   port->settings.usb.interface,
					   port->settings.usb.config,
					   strerror(saved_errno));
			return GP_ERROR_IO_UPDATE;	
#endif
			gp_log (GP_LOG_ERROR, "libusb1","setting configuration from %d to %d failed with ret = %d, but continue...", port->pl->config, port->settings.usb.config, ret);
		}

		gp_log (GP_LOG_DEBUG, "libusb1",
			"Changed usb.config from %d to %d",
			port->pl->config,
			port->settings.usb.config);

		if (ifacereleased) {
			gp_log (GP_LOG_DEBUG,"libusb1","claiming interface %d", port->settings.usb.interface);
			ret = libusb_claim_interface (port->pl->dh,
						   port->settings.usb.interface);
			if (ret < 0) {
				gp_log (GP_LOG_DEBUG, "libusb1","reclaiming the iface for config failed.");
			}
		}
		/*
		 * Copy at once if something else fails so that this
		 * does not get re-applied
		 */
		port->pl->config = port->settings.usb.config;
	}

	/* This can be changed with interface claimed. (And I think it must be claimed.) */
	if (port->settings.usb.altsetting != port->pl->altsetting) {
		ret = libusb_set_interface_alt_setting (
			port->pl->dh,
			port->settings.usb.interface, 
			port->settings.usb.altsetting
		);
		if (ret < 0) {
			int saved_errno = errno;
			gp_port_set_error (port,
					   _("Could not set altsetting from %d "
					     "to %d (%s)"),
					   port->pl->altsetting,
					   port->settings.usb.altsetting,
					   strerror(saved_errno));
			return GP_ERROR_IO_UPDATE;
		}

		gp_log (GP_LOG_DEBUG, "libusb1",
			"Changed usb.altsetting from %d to %d",
			port->pl->altsetting,
			port->settings.usb.altsetting);
		port->pl->altsetting = port->settings.usb.altsetting;
	}

	return GP_OK;
}

static int
gp_port_usb_find_ep(libusb_device *dev, int config, int interface, int altsetting, int direction, int type)
{
	const struct libusb_interface_descriptor *intf;
	struct libusb_config_descriptor *confdesc;
	int i, ret;

	ret = libusb_get_config_descriptor (dev, config, &confdesc);
	if (ret) return -1;

	intf = &confdesc->interface[interface].altsetting[altsetting];

	for (i = 0; i < intf->bNumEndpoints; i++) {
		if ((intf->endpoint[i].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == direction &&
		    (intf->endpoint[i].bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == type)
			return intf->endpoint[i].bEndpointAddress;
	}

	return -1;
}

static int
gp_port_usb_find_first_altsetting(struct libusb_device *dev, int *config, int *interface, int *altsetting)
{
	int i, i1, i2, ret;
	struct libusb_device_descriptor desc;

	ret = libusb_get_device_descriptor (dev, &desc);
	if (ret) {
		gp_log (GP_LOG_ERROR, "libusb1", "libusb_get_device_descriptor ret %d", ret);
		return -1;
	}

	for (i = 0; i < desc.bNumConfigurations; i++) {
		struct libusb_config_descriptor *confdesc;

		ret = libusb_get_config_descriptor (dev, i, &confdesc);
		if (ret) return -1;

		for (i1 = 0; i1 < confdesc->bNumInterfaces; i1++)
			for (i2 = 0; i2 < confdesc->interface[i1].num_altsetting; i2++)
				if (confdesc->interface[i1].altsetting[i2].bNumEndpoints) {
					*config = i;
					*interface = i1;
					*altsetting = i2;

					return 0;
				}
	}
	return -1;
}

static int
gp_port_usb_find_device_lib(GPPort *port, int idvendor, int idproduct)
{
	char *s;
	int d, busnr = 0, devnr = 0;

	if (!port)
		return (GP_ERROR_BAD_PARAMETERS);

	s = strchr (port->settings.usb.port,':');
	if (s && (s[1] != '\0')) { /* usb:%d,%d */
		if (sscanf (s+1, "%d,%d", &busnr, &devnr) != 2) {
			devnr = 0;
			sscanf (s+1, "%d", &busnr);
		}
	}
	/*
	 * 0x0000 idvendor is not valid.
	 * 0x0000 idproduct is ok.
	 * Should the USB layer report that ? I don't know.
	 * Better to check here.
	 */
	if (!idvendor) {
		gp_port_set_error (port, _("The supplied vendor or product "
			"id (0x%x,0x%x) is not valid."), idvendor, idproduct);
		return GP_ERROR_BAD_PARAMETERS;
	}

	nrofdevs = load_devicelist ();

	for (d = 0; d < nrofdevs; d++) {
		struct libusb_device_descriptor desc;
		struct libusb_config_descriptor *confdesc;
		int ret;
		int config = -1, interface = -1, altsetting = -1;

		ret = libusb_get_device_descriptor(devs[d], &desc);

		if ((desc.idVendor != idvendor) ||
		    (desc.idProduct != idproduct))
			continue;

		if (busnr && (busnr != libusb_get_bus_number (devs[d])))
			continue;
		if (devnr && (devnr != libusb_get_device_address (devs[d])))
			continue;

		port->pl->d = devs[d];

		gp_log (GP_LOG_VERBOSE, "libusb1",
			"Looking for USB device "
			"(vendor 0x%x, product 0x%x)... found.", 
			idvendor, idproduct);

		/* Use the first config, interface and altsetting we find */
		gp_port_usb_find_first_altsetting(devs[d], &config, &interface, &altsetting);

		ret = libusb_get_config_descriptor (devs[d], config, &confdesc);
		if (ret)
			continue;

		/* Set the defaults */
		if (confdesc->interface[interface].altsetting[altsetting].bInterfaceClass
		    == LIBUSB_CLASS_MASS_STORAGE) {
			gp_log (GP_LOG_VERBOSE, "libusb1",
				_("USB device (vendor 0x%x, product 0x%x) is a mass"
				  " storage device, and might not function with gphoto2."
				  " Reference: %s"),
				idvendor, idproduct, URL_USB_MASSSTORAGE);
		}
		port->settings.usb.config = confdesc->bConfigurationValue;
		port->settings.usb.interface = confdesc->interface[interface].altsetting[altsetting].bInterfaceNumber;
		port->settings.usb.altsetting = confdesc->interface[interface].altsetting[altsetting].bAlternateSetting;

		port->settings.usb.inep = gp_port_usb_find_ep(devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_BULK);
		port->settings.usb.outep = gp_port_usb_find_ep(devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_OUT, LIBUSB_TRANSFER_TYPE_BULK);
		port->settings.usb.intep = gp_port_usb_find_ep(devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_INTERRUPT);

		port->settings.usb.maxpacketsize = libusb_get_max_packet_size (devs[d], port->settings.usb.inep);
		gp_log (GP_LOG_VERBOSE, "libusb1",
			"Detected defaults: config %d, "
			"interface %d, altsetting %d, "
			"inep %02x, outep %02x, intep %02x, "
			"class %02x, subclass %02x",
			port->settings.usb.config,
			port->settings.usb.interface,
			port->settings.usb.altsetting,
			port->settings.usb.inep,
			port->settings.usb.outep,
			port->settings.usb.intep,
			confdesc->interface[interface].altsetting[altsetting].bInterfaceClass,
			confdesc->interface[interface].altsetting[altsetting].bInterfaceSubClass
			);

		return GP_OK;
	}
#if 0
	gp_port_set_error (port, _("Could not find USB device "
		"(vendor 0x%x, product 0x%x). Make sure this device "
		"is connected to the computer."), idvendor, idproduct);
#endif
	return GP_ERROR_IO_USB_FIND;
}

/* This function reads the Microsoft OS Descriptor and looks inside to
 * find if it is a MTP device. This is the similar to the way that
 * Windows Media Player 10 uses.
 * It is documented to some degree on various internet pages.
 */
static int
gp_port_usb_match_mtp_device(struct libusb_device *dev,int *configno, int *interfaceno, int *altsettingno)
{
	/* Marcus: Avoid this probing altogether, its too unstable on some devices */
	return 0;

#if 0
	char buf[1000], cmd;
	int ret,i,i1,i2, xifaces,xnocamifaces;
	usb_dev_handle *devh;

	/* All of them are "vendor specific" device class */
#if 0
	if ((desc.bDeviceClass!=0xff) && (desc.bDeviceClass!=0))
		return 0;
#endif
	if (dev->config) {
		xifaces = xnocamifaces = 0;
		for (i = 0; i < desc.bNumConfigurations; i++) {
			unsigned int j;

			for (j = 0; j < dev->config[i].bNumInterfaces; j++) {
				int k;
				xifaces++;

				for (k = 0; k < dev->config[i].interface[j].num_altsetting; k++) {
					struct usb_interface_descriptor *intf = &dev->config[i].interface[j].altsetting[k]; 
					if (	(intf->bInterfaceClass == LIBUSB_CLASS_HID)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_PRINTER)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_AUDIO)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_HUB)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_COMM)	||
						(intf->bInterfaceClass == 0xe0)	/* wireless/bluetooth*/
					)
						xnocamifaces++;
				}
			}
		}
	}
	if (xifaces == xnocamifaces) /* only non-camera ifaces */
		return 0;

	devh = usb_open (dev);
	if (!devh)
		return 0;

	/*
	 * Loop over the device configurations and interfaces. Nokia MTP-capable 
	 * handsets (possibly others) typically have the string "MTP" in their 
	 * MTP interface descriptions, that's how they can be detected, before
	 * we try the more esoteric "OS descriptors" (below).
	 */
	if (dev->config) {
		for (i = 0; i < desc.bNumConfigurations; i++) {
			unsigned int j;

			for (j = 0; j < dev->config[i].bNumInterfaces; j++) {
				int k;
				for (k = 0; k < dev->config[i].interface[j].num_altsetting; k++) {
					buf[0] = '\0';
					ret = usb_get_string_simple(devh, 
						dev->config[i].interface[j].altsetting[k].iInterface, 
						(char *) buf, 
						1024);
					if (ret < 3)
						continue;
					if (strcmp((char *) buf, "MTP") == 0) {
						gp_log (GP_LOG_DEBUG, "mtp matcher", "Configuration %d, interface %d, altsetting %d:\n", i, j, k);
						gp_log (GP_LOG_DEBUG, "mtp matcher", "   Interface description contains the string \"MTP\"\n");
						gp_log (GP_LOG_DEBUG, "mtp matcher", "   Device recognized as MTP, no further probing.\n");
						goto found;
					}
				}
			}
		}
	}
	/* get string descriptor at 0xEE */
	ret = usb_get_descriptor (devh, 0x03, 0xee, buf, sizeof(buf));
	if (ret > 0) gp_log_data("get_MS_OSD",buf, ret);
	if (ret < 10) goto errout;
	if (!((buf[2] == 'M') && (buf[4]=='S') && (buf[6]=='F') && (buf[8]=='T')))
		goto errout;
	cmd = buf[16];
	ret = usb_control_msg (devh, USB_ENDPOINT_IN|USB_RECIP_DEVICE|LIBUSB_REQUEST_TYPE_VENDOR, cmd, 0, 4, buf, sizeof(buf), 1000);
	if (ret == -1) {
		gp_log (GP_LOG_ERROR, "mtp matcher", "control message says %d\n", ret);
		goto errout;
	}
	if (buf[0] != 0x28) {
		gp_log (GP_LOG_ERROR, "mtp matcher", "ret is %d, buf[0] is %x\n", ret, buf[0]);
		goto errout;
	}
	if (ret > 0) gp_log_data("get_MS_ExtDesc",buf, ret);
	if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
		gp_log (GP_LOG_ERROR, "mtp matcher", "buf at 0x12 is %02x%02x%02x\n", buf[0x12], buf[0x13], buf[0x14]);
		goto errout;
	}
	ret = usb_control_msg (devh, USB_ENDPOINT_IN|USB_RECIP_DEVICE|LIBUSB_REQUEST_TYPE_VENDOR, cmd, 0, 5, buf, sizeof(buf), 1000);
	if (ret == -1) goto errout;
	if (buf[0] != 0x28) {
		gp_log (GP_LOG_ERROR, "mtp matcher", "ret is %d, buf[0] is %x\n", ret, buf[0]);
		goto errout;
	}
	if (ret > 0) gp_log_data("get_MS_ExtProp",buf, ret);
	if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
		gp_log (GP_LOG_ERROR, "mtp matcher", "buf at 0x12 is %02x%02x%02x\n", buf[0x12], buf[0x13], buf[0x14]);
		goto errout;
	}

found:
	usb_close (devh);

	/* Now chose a nice interface for us to use ... Just take the first. */

	if (desc.bNumConfigurations > 1)
		gp_log (GP_LOG_ERROR, "mtp matcher", "The device has %d configurations!\n", desc.bNumConfigurations);
	for (i = 0; i < desc.bNumConfigurations; i++) {
		struct usb_config_descriptor *config =
			&dev->config[i];

		if (config->bNumInterfaces > 1)
			gp_log (GP_LOG_ERROR, "mtp matcher", "The configuration has %d interfaces!\n", config->bNumInterfaces);
		for (i1 = 0; i1 < config->bNumInterfaces; i1++) {
			struct usb_interface *interface =
				&config->interface[i1];

			if (interface->num_altsetting > 1)
				gp_log (GP_LOG_ERROR, "mtp matcher", "The interface has %d altsettings!\n", interface->num_altsetting);
			for (i2 = 0; i2 < interface->num_altsetting; i2++) {
				*configno = i;
				*interfaceno = i1;
				*altsettingno = i2;
				return 1;
			}
		}
	}
	return 1;
errout:
	usb_close (devh);
	return 0;
#endif
}

static int
gp_port_usb_match_device_by_class(struct libusb_device *dev, int class, int subclass, int protocol, int *configno, int *interfaceno, int *altsettingno)
{
	int i, i1, i2;
	struct libusb_device_descriptor desc;
	int ret;

	if (class == 666) /* Special hack for MTP devices with MS OS descriptors. */
		return gp_port_usb_match_mtp_device (dev, configno, interfaceno, altsettingno);

	if (desc.bDeviceClass == class &&
	    (subclass == -1 ||
	     desc.bDeviceSubClass == subclass) &&
	    (protocol == -1 ||
	     desc.bDeviceProtocol == protocol))
		return 1;

	ret = libusb_get_device_descriptor(dev, &desc);

	for (i = 0; i < desc.bNumConfigurations; i++) {
		struct libusb_config_descriptor *config;

		ret = libusb_get_config_descriptor (dev, i, &config);

		for (i1 = 0; i1 < config->bNumInterfaces; i1++) {
			const struct libusb_interface *interface =
				&config->interface[i1];

			for (i2 = 0; i2 < interface->num_altsetting; i2++) {
				const struct libusb_interface_descriptor *altsetting =
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
	char *s;
	int d, busnr = 0, devnr = 0;

	if (!port)
		return (GP_ERROR_BAD_PARAMETERS);

	s = strchr (port->settings.usb.port,':');
	if (s && (s[1] != '\0')) { /* usb:%d,%d */
		if (sscanf (s+1, "%d,%d", &busnr, &devnr) != 2) {
			devnr = 0;
			sscanf (s+1, "%d", &busnr);
		}
	}
	/*
	 * 0x00 class is not valid.
	 * 0x00 subclass and protocol is ok.
	 * Should the USB layer report that ? I don't know.
	 * Better to check here.
	 */
	if (!class)
		return GP_ERROR_BAD_PARAMETERS;

	nrofdevs = load_devicelist ();
	for (d = 0; d < nrofdevs; d++) {
		struct libusb_device_descriptor desc;
		struct libusb_config_descriptor *confdesc;
		int i, ret, config = -1, interface = -1, altsetting = -1;

		if (busnr && (busnr != libusb_get_bus_number (devs[d])))
			continue;
		if (devnr && (devnr != libusb_get_device_address (devs[d])))
			continue;

		gp_log (GP_LOG_VERBOSE, "gphoto2-port-usb",
			"Looking for USB device "
			"(class 0x%x, subclass, 0x%x, protocol 0x%x)...", 
			class, subclass, protocol);

		ret = gp_port_usb_match_device_by_class(devs[d], class, subclass, protocol, &config, &interface, &altsetting);
		if (!ret)
			continue;

		port->pl->d = devs[d];
		gp_log (GP_LOG_VERBOSE, "libusb1",
			"Found USB class device "
			"(class 0x%x, subclass, 0x%x, protocol 0x%x)", 
			class, subclass, protocol);

		ret = libusb_get_config_descriptor (devs[d], config, &confdesc);
		if (ret) continue;

		/* Set the defaults */
		port->settings.usb.config = confdesc->bConfigurationValue;
		port->settings.usb.interface = confdesc->interface[interface].altsetting[altsetting].bInterfaceNumber;
		port->settings.usb.altsetting = confdesc->interface[interface].altsetting[altsetting].bAlternateSetting;

		port->settings.usb.inep = gp_port_usb_find_ep(devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_BULK);
		port->settings.usb.outep = gp_port_usb_find_ep(devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_OUT, LIBUSB_TRANSFER_TYPE_BULK);
		port->settings.usb.intep = gp_port_usb_find_ep(devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_INTERRUPT);
		port->settings.usb.maxpacketsize = 0;
		gp_log (GP_LOG_DEBUG, "libusb1", "inep to look for is %02x", port->settings.usb.inep);
		for (i=0;i<confdesc->interface[interface].altsetting[altsetting].bNumEndpoints;i++) {
			if (port->settings.usb.inep == confdesc->interface[interface].altsetting[altsetting].endpoint[i].bEndpointAddress) {
				port->settings.usb.maxpacketsize = confdesc->interface[interface].altsetting[altsetting].endpoint[i].wMaxPacketSize;
				break;
			}
		}
		gp_log (GP_LOG_VERBOSE, "libusb1",
			"Detected defaults: config %d, "
			"interface %d, altsetting %d, "
			"idVendor ID %04x, idProduct %04x, "
			"inep %02x, outep %02x, intep %02x",
			port->settings.usb.config,
			port->settings.usb.interface,
			port->settings.usb.altsetting,
			desc.idVendor,
			desc.idProduct,
			port->settings.usb.inep,
			port->settings.usb.outep,
			port->settings.usb.intep
			);
		return GP_OK;
	}
#if 0
	gp_port_set_error (port, _("Could not find USB device "
		"(class 0x%x, subclass 0x%x, protocol 0x%x). Make sure this device "
		"is connected to the computer."), class, subclass, protocol);
#endif
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
	ops->msg_interface_write  = gp_port_usb_msg_interface_write_lib;
	ops->msg_interface_read   = gp_port_usb_msg_interface_read_lib;
	ops->msg_class_write  = gp_port_usb_msg_class_write_lib;
	ops->msg_class_read   = gp_port_usb_msg_class_read_lib;
	ops->find_device = gp_port_usb_find_device_lib;
	ops->find_device_by_class = gp_port_usb_find_device_by_class_lib;

	return (ops);
}
