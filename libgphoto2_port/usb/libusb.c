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

#include <config.h>

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
#include <gphoto2-port-library.h>

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

	if (!dev || !dev->device)
		return GP_ERROR_BAD_PARAMETERS;

        /*
	 * Open the device using the previous usb_handle returned by
	 * find_device
	 */
        dev->device_handle = usb_open (dev->device);
	if (!dev->device_handle) {
		gp_log (GP_LOG_ERROR, "gphoto2-port-usb", _("Could not "
			"open USB device"));
		return GP_ERROR_IO_OPEN;
	}

	ret = usb_claim_interface (dev->device_handle,
				   dev->settings.usb.interface);
	if (ret < 0) {
		gp_log (GP_LOG_ERROR, "gphoto2-port-usb", _("Could not claim "
			"interface %d (%m). Make sure no other program "
			"or kernel module (i.e. dcxxx) is using the device."),
			dev->settings.usb.interface);
		return GP_ERROR_IO_USB_CLAIM;
	}
	
	return GP_OK;
}

int gp_port_usb_close (gp_port *dev)
{
	if (!dev || !dev->device_handle)
		return GP_ERROR_BAD_PARAMETERS;

	if (usb_release_interface (dev->device_handle,
				   dev->settings.usb.interface) < 0) {
		gp_log (GP_LOG_ERROR, "gphoto2-port-usb", _("Could not "
			"release interface %d (%m)"),
			dev->settings.usb.interface);
		return (GP_ERROR_IO_CLOSE);
	}

	if (usb_close (dev->device_handle) < 0) {
		gp_log (GP_LOG_ERROR, "gphoto2-port-usb", _("Could not close "
			"USB port (%m)"));
		return (GP_ERROR_IO_CLOSE);
	}

	dev->device_handle = NULL;

	return GP_OK;
}

int gp_port_usb_reset(gp_port *dev)
{
	/* We don't care if we cannot close the port */
	gp_port_usb_close (dev);

	return (gp_port_usb_open(dev));
}

int gp_port_usb_clear_halt_lib(gp_port * dev, int ep)
{
	int ret=0;

	if (!dev || !dev->device_handle)
		return GP_ERROR_BAD_PARAMETERS;

	switch (ep) {
	case GP_PORT_USB_ENDPOINT_IN :
		ret=usb_clear_halt(dev->device_handle, dev->settings.usb.inep);
		break;
	case GP_PORT_USB_ENDPOINT_OUT :
		ret=usb_clear_halt(dev->device_handle, dev->settings.usb.outep);
		break;
	default:
		gp_log (GP_LOG_DEBUG, "gphoto2-port-usb",
			"gp_port_usb_clear_halt: bad EndPoint argument");
		return GP_ERROR_BAD_PARAMETERS;
	}
	return (ret ? GP_ERROR_IO_USB_CLEAR_HALT : GP_OK);
}

int gp_port_usb_write(gp_port * dev, char *bytes, int size)
{
        int ret;

	if (!dev || !dev->device_handle)
		return GP_ERROR_BAD_PARAMETERS;

	ret = usb_bulk_write(dev->device_handle, dev->settings.usb.outep,
                           bytes, size, dev->timeout);
        if (ret < 0)
		return (GP_ERROR_IO_WRITE);

        return (ret);
}

int gp_port_usb_read(gp_port * dev, char *bytes, int size)
{
	int ret;

	if (!dev || !dev->device_handle)
		return GP_ERROR_BAD_PARAMETERS;

	ret = usb_bulk_read(dev->device_handle, dev->settings.usb.inep,
			     bytes, size, dev->timeout);
        if (ret < 0)
		return GP_ERROR_IO_READ;

        return ret;
}

int gp_port_usb_msg_write_lib(gp_port * dev, int request, int value, int index,
	char *bytes, int size)
{
	if (!dev || !dev->device_handle)
		return GP_ERROR_BAD_PARAMETERS;

	return usb_control_msg(dev->device_handle,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		request, value, index, bytes, size, dev->timeout);
}

int gp_port_usb_msg_read_lib(gp_port * dev, int request, int value, int index,
	char *bytes, int size)
{
	if (!dev || !dev->device_handle)
		return GP_ERROR_BAD_PARAMETERS;

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

	if (!dev || !dev->device_handle)
		return GP_ERROR_BAD_PARAMETERS;

	if (memcmp(&dev->settings.usb, &dev->settings_pending.usb, sizeof(dev->settings.usb))) {

		/* settings.usb structure is different than the old one */
		 
		if (dev->settings.usb.config != dev->settings_pending.usb.config) {
			ret = usb_set_configuration(dev->device_handle,
					     dev->settings_pending.usb.config);
			if (ret < 0) {
				gp_log (GP_LOG_ERROR, "gphoto2-port-usb",
					_("Could not set config %d/%d (%m)"),
					dev->settings_pending.usb.interface,
					dev->settings_pending.usb.config);
				return GP_ERROR_IO_UPDATE;	
			}

			gp_log (GP_LOG_DEBUG, "gphoto2-port-usb",
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
				gp_log (GP_LOG_ERROR, "gphoto2-port-usb",
					_("Could not set interface "
					"%d/%d (%m)"),
					dev->settings_pending.usb.interface,
					dev->settings_pending.usb.altsetting);
				return GP_ERROR_IO_UPDATE;
			}

			gp_log (GP_LOG_DEBUG, "gphoto2-port-usb",
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
