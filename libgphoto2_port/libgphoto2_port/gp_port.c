/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gpio.c - Core IO library functions

   Modifications:
   Copyright (C) 1999 Scott Fritzinger <scottf@unr.edu>

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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2-port.h>
#include <gphoto2-port-debug.h>
#include <gphoto2-port-result.h>
#include "library.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#  else
#      define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

gp_port_info     device_list[256];
int              device_count;

int initialized = 0;

/*
   Required library functions
   ----------------------------------------------------------------
 */

int gp_port_init (void)
{
        gp_port_debug_printf(GP_DEBUG_LOW, "Initializing...");
        /* Enumerate all the available devices */
        device_count = 0;
	initialized = 1;
        return (gp_port_library_list(device_list, &device_count));
}

int gp_port_count_get(void)
{
        gp_port_debug_printf(GP_DEBUG_LOW, "Device count: %i", device_count);
	if (!initialized)
		gp_port_init ();

        return device_count;
}

int gp_port_info_get(int device_number, gp_port_info *info)
{
	if (! info) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_info_get: "
					"called on NULL info struct!");
		return GP_ERROR_BAD_PARAMETERS;
	}
	
	if (device_number > device_count) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_info_get: "
					"called on non-existant device!");
		return GP_ERROR_BAD_PARAMETERS;
	}
	
        gp_port_debug_printf(GP_DEBUG_LOW, "Getting device info...");

        memcpy(info, &device_list[device_number], sizeof(device_list[device_number]));

        return GP_OK;
}

int
gp_port_new (gp_port **dev, gp_port_type type)
{
        gp_port_settings settings;
        char buf[1024];
	int result;

	if (!initialized)
		gp_port_init ();

        gp_port_debug_printf(GP_DEBUG_LOW, "Creating new device... ");

        switch(type) {
        case GP_PORT_SERIAL:
#ifndef GP_PORT_SUPPORTED_SERIAL
		return GP_ERROR_IO_SUPPORTED_SERIAL;
#endif
		break;
        case GP_PORT_USB:
#ifndef GP_PORT_SUPPORTED_USB
		gp_port_debug_printf (GP_DEBUG_LOW, "libgphoto2_port has been "
			"compiled without USB support. This probably means "
			"you didn't have libusb installed prior compilation "
			"of gphoto2.");
		return GP_ERROR_IO_SUPPORTED_USB;
#endif
		break;
        case GP_PORT_PARALLEL:
#ifndef GP_PORT_SUPPORTED_PARALLEL
		return GP_ERROR_IO_SUPPORTED_PARALLEL;
#endif
		break;
        case GP_PORT_NETWORK:
#ifndef GP_PORT_SUPPORTED_NETWORK
		return GP_ERROR_IO_SUPPORTED_NETWORK;
#endif
		break;
        case GP_PORT_IEEE1394:
#ifndef GP_PORT_SUPPORTED_IEEE1394
		return GP_ERROR_IO_SUPPORTED_IEEE1394;
#endif
		break;
        default:
		return GP_ERROR_IO_UNKNOWN_PORT;
        }

        *dev = malloc (sizeof (gp_port));
        if (!(*dev))
		return (GP_ERROR_NO_MEMORY);
        memset (*dev, 0, sizeof (gp_port));

	result = gp_port_library_load (*dev, type);
	if (result < 0) {
		free(*dev);
		return (result);
        }

        (*dev)->type = type;
        (*dev)->device_fd = 0;
        (*dev)->ops->init(*dev);

        switch ((*dev)->type) {
        case GP_PORT_SERIAL:
		sprintf(buf, GP_PORT_SERIAL_PREFIX, GP_PORT_SERIAL_RANGE_LOW);
		strcpy(settings.serial.port, buf);

		/* Set some default settings */
		settings.serial.speed = 9600;
		settings.serial.bits = 8;
		settings.serial.parity = 0;
		settings.serial.stopbits = 1;
		gp_port_settings_set (*dev, settings);

		gp_port_timeout_set (*dev, 500);
		break;
        case GP_PORT_PARALLEL:
		sprintf (buf, GP_PORT_SERIAL_PREFIX, GP_PORT_SERIAL_RANGE_LOW);
		strcpy (settings.parallel.port, buf);
		break;
        case GP_PORT_NETWORK:
		gp_port_timeout_set (*dev, 50000);
		break;
        case GP_PORT_USB:
		/* Initialize settings.usb */
		(*dev)->settings.usb.inep = -1;
		(*dev)->settings.usb.outep = -1;
		(*dev)->settings.usb.config = -1;
		(*dev)->settings.usb.interface = 0;
		(*dev)->settings.usb.altsetting = -1;

		gp_port_timeout_set (*dev, 5000);
		break;
        case GP_PORT_IEEE1394:
		/* blah ? */
		break;
        default:
		return GP_ERROR_IO_UNKNOWN_PORT;
        }

        gp_port_debug_printf(GP_DEBUG_LOW, "Created device successfully...");

        return (GP_OK);
}

int
gp_port_open (gp_port *dev)
{
        int retval = 0;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_open: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

	switch (dev->type) {
	case GP_PORT_SERIAL:
		gp_port_debug_printf (GP_DEBUG_MEDIUM, "Opening SERIAL port");
		break;
	case GP_PORT_USB:
		gp_port_debug_printf (GP_DEBUG_MEDIUM, "Opening USB port");
		break;
	default:
		gp_port_debug_printf (GP_DEBUG_MEDIUM, "Opening port");
		break;
	}

        /* Try to open device */
        retval = dev->ops->open (dev);
	if (retval < 0)
		return (retval);

	return GP_OK;
}

int
gp_port_close (gp_port *dev)
{
        int retval = 0;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_close: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

	gp_port_debug_printf (GP_DEBUG_LOW, "Closing port");

        retval = dev->ops->close(dev);
        dev->device_fd = 0;
        return retval;
}

int
gp_port_free (gp_port *dev)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_free: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

	gp_port_debug_printf (GP_DEBUG_MEDIUM, "Freeing port");

	gp_port_debug_printf (GP_DEBUG_HIGH, "Exiting...");
	retval = dev->ops->exit(dev);
	gp_port_debug_printf (GP_DEBUG_HIGH, "... %ssuccessful",
			      retval < 0 ? "un" : "");

        gp_port_library_close (dev);
        free (dev);

        return GP_OK;
}

int
gp_port_write (gp_port *dev, char *bytes, int size)
{
	int retval;

	if (!dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_write: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

	gp_port_debug_printf (GP_DEBUG_MEDIUM, "Writing %i byte(s) to port",
			      size);
	gp_port_debug_print_data (GP_DEBUG_HIGH, bytes, size);

	retval = dev->ops->write (dev, bytes, size);

	if (retval != size)
		gp_port_debug_printf (GP_DEBUG_HIGH, "dev->ops->write returned %i, expected %i",
				      retval, size);

	return (retval);
}

int
gp_port_read (gp_port *dev, char *bytes, int size)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_read: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

	gp_port_debug_printf (GP_DEBUG_MEDIUM, "Reading from port...");

        retval = dev->ops->read (dev, bytes, size);
	if (retval < 0)
		return (retval);

	gp_port_debug_printf (GP_DEBUG_MEDIUM, "... %5i byte(s) read", retval);
	if (retval != size) {
		gp_port_debug_printf(GP_DEBUG_HIGH, "dev->ops->read returned %i, expected %i",
				     retval, size);
	}
	gp_port_debug_print_data (GP_DEBUG_HIGH, bytes, retval);

	return (retval);
}

int
gp_port_timeout_set (gp_port *dev, int millisec_timeout)
{
	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_timeout_set: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

	gp_port_debug_printf (GP_DEBUG_MEDIUM, "Setting timeout to %ims",
			      millisec_timeout);

        dev->timeout = millisec_timeout;

        return GP_OK;
}

int
gp_port_timeout_get (gp_port *dev, int *millisec_timeout)
{
	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_timeout_get: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

	gp_port_debug_printf (GP_DEBUG_MEDIUM, "Getting timeout: %ims",
			      dev->timeout);

        *millisec_timeout = dev->timeout;

        return GP_OK;
}

int
gp_port_settings_set (gp_port *dev, gp_port_settings settings)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_settings_set: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

        /*
	 * We copy the settings to settings_pending and call update on the 
	 * port.
	 */
        memcpy (&dev->settings_pending, &settings,
		sizeof (dev->settings_pending));

	gp_port_debug_printf (GP_DEBUG_MEDIUM, "Setting settings...");
        retval = dev->ops->update(dev);
	gp_port_debug_printf (GP_DEBUG_MEDIUM, "... %ssuccessful",
			      retval < 0 ? "un" : "");

        return (retval);
}

int
gp_port_settings_get (gp_port *dev, gp_port_settings * settings)
{
	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_settings_get: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

        memcpy (settings, &(dev->settings), sizeof(gp_port_settings));

        return GP_OK;
}

/* Serial and Parallel-specific functions */
/* ------------------------------------------------------------------ */

int gp_port_pin_get(gp_port *dev, int pin, int *level)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_pin_get: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

        if (!dev->ops->get_pin) {
                gp_port_debug_printf(GP_DEBUG_LOW, "gp_port_get_pin: get_pin NULL");
                return (GP_ERROR);
        }

        retval = dev->ops->get_pin(dev, pin, level);
        gp_port_debug_printf(GP_DEBUG_LOW,
                "gp_port_get_pin: get_pin %s", retval < 0? "error":"ok");
        return (retval);
}

int gp_port_pin_set(gp_port *dev, int pin, int level)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_pin_set: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

        if (!dev->ops->get_pin) {
                gp_port_debug_printf(GP_DEBUG_LOW, "gp_port_pin_set: set_pin NULL");
                return (GP_ERROR);
        }

        retval = dev->ops->set_pin(dev, pin, level);
        gp_port_debug_printf(GP_DEBUG_LOW,
                "gp_port_set_pin: set_pin %s", retval < 0? "error":"ok");
        return (retval);
}

int gp_port_send_break (gp_port *dev, int duration)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_send_break: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

        if (!dev->ops->send_break) {
                gp_port_debug_printf(GP_DEBUG_LOW, "gp_port_send_break: NULL break operation!");
                return (GP_ERROR);
        }

        retval = dev->ops->send_break(dev, duration);
        gp_port_debug_printf(GP_DEBUG_LOW,
                "gp_port_send_break: send_break %s", retval < 0? "error":"ok");
        return (retval);
}

int gp_port_flush (gp_port *dev, int direction)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_flush: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

        if (!dev->ops->flush) {
                gp_port_debug_printf(GP_DEBUG_LOW, "gp_port_flush: NULL flush operation!");
                return (GP_ERROR);
        }

        retval = dev->ops->flush(dev, direction);
        gp_port_debug_printf(GP_DEBUG_LOW,
                "gp_port_flush: flush %s", retval < 0? "error":"ok");
        return (retval);
}


/* USB-specific functions */
/* ------------------------------------------------------------------ */

int gp_port_usb_find_device (gp_port * dev, int idvendor, int idproduct)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_usb_find_device: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

        if (!dev->ops->find_device) {
                gp_port_debug_printf(GP_DEBUG_LOW,
                        "gp_port_usb_find_device: NULL find_device operation!");
                return (GP_ERROR);
        }

        retval = dev->ops->find_device(dev, idvendor, idproduct);
        gp_port_debug_printf(GP_DEBUG_LOW,
                "gp_port_usb_find_device: find_device (0x%04x 0x%04x) %s",
                idvendor, idproduct, retval < 0? "error":"ok");
        return (retval);
}

int gp_port_usb_clear_halt (gp_port * dev, int ep)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_usb_clear_halt: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

        if (!dev->ops->clear_halt) {
                gp_port_debug_printf(GP_DEBUG_LOW,
                        "gp_port_usb_clear_halt: NULL clear halt operation!");
                return (GP_ERROR);
        }

        retval = dev->ops->clear_halt(dev, ep);
        gp_port_debug_printf(GP_DEBUG_LOW,
                "gp_port_usb_clear_halt: clear_halt %s", retval < 0? "error":"ok");
        return (retval);
}

int gp_port_usb_msg_write (gp_port * dev, int request, int value, int index,
	char *bytes, int size)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_usb_msg_write: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

        if (!dev->ops->msg_write) {
                gp_port_debug_printf(GP_DEBUG_LOW,
                        "gp_port_usb_msg_write: NULL msg_write operation!");
                return (GP_ERROR);
        }

        retval = dev->ops->msg_write(dev, request, value, index, bytes, size);

	gp_port_debug_printf(GP_DEBUG_HIGH, 
			     "gp_port_usb_msg_write: msg_write returns %i, "
			     "request=0x%x value=0x%x index=0x%x size=%i=0x%x",
			     retval, request, value, index, size, size);
	gp_port_debug_print_data(GP_DEBUG_HIGH, bytes, size);

        gp_port_debug_printf(GP_DEBUG_LOW,
                "gp_port_usb_msg_write: msg_write %s", retval < 0? "error":"ok");
        return (retval);
}

int gp_port_usb_msg_read (gp_port * dev, int request, int value, int index,
	char *bytes, int size)
{
        int retval;

	if (! dev) {
		gp_port_debug_printf (GP_DEBUG_NONE, "gp_port_usb_msg_read: "
					"called on NULL device!");
		return GP_ERROR_BAD_PARAMETERS;
	}

        if (!dev->ops->msg_read) {
                gp_port_debug_printf(GP_DEBUG_LOW,
                        "gp_port_usb_msg_read: NULL msg_read operation!");
                return (GP_ERROR);
        }

        retval = dev->ops->msg_read(dev, request, value, index, bytes, size);

        gp_port_debug_printf(GP_DEBUG_LOW,
                "gp_port_usb_msg_read: msg_read %s", retval < 0? "error":"ok");

	gp_port_debug_printf(GP_DEBUG_HIGH, 
			     "gp_port_usb_msg_read: msg_read returns %i, "
			     "request=0x%x value=0x%x index=0x%x size=%i=0x%x",
			     retval, request, value, index, size, size);
	gp_port_debug_print_data(GP_DEBUG_HIGH, bytes, retval);

        return (retval);
}
