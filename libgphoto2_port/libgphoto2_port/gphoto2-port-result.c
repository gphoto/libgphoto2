/* result.c
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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
#include <gphoto2-port-result.h>

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

#define GP_ERR(num,str) {if (result == (num)) return (str);}

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
	GP_ERR (GP_OK,                   N_("No error"));
	GP_ERR (GP_ERROR,                N_("Unspecified error"));
	GP_ERR (GP_ERROR_IO,             N_("I/O problem"));
	GP_ERR (GP_ERROR_BAD_PARAMETERS, N_("Bad parameters"));
	GP_ERR (GP_ERROR_NOT_SUPPORTED,  N_("Unsupported operation"));
	GP_ERR (GP_ERROR_TIMEOUT,        N_("Timeout reading from or "
						"writing to the port"));
	GP_ERR (GP_ERROR_IO_SUPPORTED_SERIAL, N_("Serial port not "
						     "supported"));
	GP_ERR (GP_ERROR_IO_SUPPORTED_USB, N_("USB port not supported"));
	GP_ERR (GP_ERROR_UNKNOWN_PORT, N_("Unknown port"));
	GP_ERR (GP_ERROR_NO_MEMORY,    N_("Out of memory"));
	GP_ERR (GP_ERROR_LIBRARY,      N_("Error loading a library"));
	GP_ERR (GP_ERROR_IO_INIT,      N_("Error initializing the port"));
	GP_ERR (GP_ERROR_IO_OPEN,      N_("Error opening the port"));
	GP_ERR (GP_ERROR_IO_TIMEOUT,   N_("Timeout reading from or writing "
					      "to the port"));
	GP_ERR (GP_ERROR_IO_READ,   N_("Error reading from the port"));
	GP_ERR (GP_ERROR_IO_WRITE,  N_("Error writing to the port"));
	GP_ERR (GP_ERROR_IO_CLOSE,  N_("Error closing the port"));
	GP_ERR (GP_ERROR_IO_UPDATE, N_("Error updating the port settings"));
	GP_ERR (GP_ERROR_IO_PIN,    N_("Error with the port"));
	GP_ERR (GP_ERROR_IO_SERIAL_SPEED, N_("Error setting the serial "
						 "port speed"));
	GP_ERR (GP_ERROR_IO_SERIAL_BREAK, N_("Error sending a break to the "
						 "serial port"));
	GP_ERR (GP_ERROR_IO_SERIAL_FLUSH, N_("Error flushing the serial "
						 "line"));
	GP_ERR (GP_ERROR_IO_USB_CLEAR_HALT, N_("Error clearing a halt "
						"condition on the USB port"));
	GP_ERR (GP_ERROR_IO_USB_FIND,  N_("Could not find the requested "
					      "device on the USB port"));
	GP_ERR (GP_ERROR_IO_USB_CLAIM, N_("Could not claim the USB "
					      "device"));
	GP_ERR (GP_ERROR_IO_LOCK,      N_("Could not lock the device"));

	return N_("Unknown error");
}

#undef GP_ERR
