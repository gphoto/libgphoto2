/** \file
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sf.net>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <gphoto2/gphoto2-port-result.h>

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

/**
 * gp_port_result_as_string:
 * @result: a gphoto2 error code
 *
 * Returns a string representation of a gphoto2 error code. Those are static
 * error descriptions. You can get dynamic ones that explain the error more
 * in depth using #gp_port_get_error.
 *
 * Return value: a string representation of a gphoto2 error code
 **/
const char *
gp_port_result_as_string (int result)
{
	switch (result) {
	case GP_OK:
		return _("No error");
	case GP_ERROR:
		return _("Unspecified error");
	case GP_ERROR_IO:
		return _("I/O problem");
	case GP_ERROR_BAD_PARAMETERS:
		return _("Bad parameters");
	case GP_ERROR_NOT_SUPPORTED:
		return _("Unsupported operation");
	case  GP_ERROR_FIXED_LIMIT_EXCEEDED:
		return _("Fixed limit exceeded");
	case GP_ERROR_TIMEOUT:
		return _("Timeout reading from or writing to the port");
	case GP_ERROR_IO_SUPPORTED_SERIAL:
		return _("Serial port not supported");
	case GP_ERROR_IO_SUPPORTED_USB:
		return _("USB port not supported");
	case GP_ERROR_UNKNOWN_PORT:
		return _("Unknown port");
	case GP_ERROR_NO_MEMORY:
		return _("Out of memory");
	case GP_ERROR_LIBRARY:
		return _("Error loading a library");
	case GP_ERROR_IO_INIT:
		return _("Error initializing the port");
	case GP_ERROR_IO_READ:
		return _("Error reading from the port");
	case GP_ERROR_IO_WRITE:
		return _("Error writing to the port");
	case GP_ERROR_IO_UPDATE:
		return _("Error updating the port settings");
	case GP_ERROR_IO_SERIAL_SPEED:
		return _("Error setting the serial port speed");
	case GP_ERROR_IO_USB_CLEAR_HALT:
		return _("Error clearing a halt condition on the USB port");
	case GP_ERROR_IO_USB_FIND:
		return _("Could not find the requested device on the USB port");
	case GP_ERROR_IO_USB_CLAIM:
		return _("Could not claim the USB device");
	case GP_ERROR_IO_LOCK:
		return _("Could not lock the device");
	case GP_ERROR_HAL:
		return _("libhal error");
	default:
		return _("Unknown error");
	}
}
