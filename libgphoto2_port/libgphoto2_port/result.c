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
#      define N_(String) gettext_noop (String)
#  else
#      define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define GP_ERR_RES(num,str) {if (result == (num)) return (N_(str));}

const char *
gp_port_result_as_string (int result)
{
	if ((result < -99) || (result > 0))
		return (N_("Unknown error"));

	GP_ERR_RES (GP_OK, "No error");
	GP_ERR_RES (GP_ERROR, "Unspecified error");
	GP_ERR_RES (GP_ERROR_BAD_PARAMETERS, N_("Bad parameters"));
	GP_ERR_RES (GP_ERROR_TIMEOUT,
		    "Timeout reading from or writing to the port");
	GP_ERR_RES (GP_ERROR_IO_SUPPORTED_SERIAL, "Serial port not supported");
	GP_ERR_RES (GP_ERROR_IO_SUPPORTED_USB, "USB port not supported");
	GP_ERR_RES (GP_ERROR_IO_SUPPORTED_PARALLEL,
		    "Parallel port not supported");
	GP_ERR_RES (GP_ERROR_IO_SUPPORTED_NETWORK,
		    "Network port not supported");
	GP_ERR_RES (GP_ERROR_IO_SUPPORTED_IEEE1394,
		    "IEEE1394 port not supported");
	GP_ERR_RES (GP_ERROR_IO_UNKNOWN_PORT, "Unknown port");
	GP_ERR_RES (GP_ERROR_MEMORY, "Out of memory");
	GP_ERR_RES (GP_ERROR_LIBRARY, "Error loading a required library");
	GP_ERR_RES (GP_ERROR_IO_INIT, "Error initializing the port");
	GP_ERR_RES (GP_ERROR_IO_OPEN, "Error opening the port");
	GP_ERR_RES (GP_ERROR_IO_TIMEOUT,
		    "Timeout reading from or writing to the port");
	GP_ERR_RES (GP_ERROR_IO_READ, "Error reading from the port");
	GP_ERR_RES (GP_ERROR_IO_WRITE, "Error writing to the port");
	GP_ERR_RES (GP_ERROR_IO_CLOSE, "Error closing the port");
	GP_ERR_RES (GP_ERROR_IO_UPDATE, "Error updating the port settings");
	GP_ERR_RES (GP_ERROR_IO_PIN, "Error with the port");
	GP_ERR_RES (GP_ERROR_IO_SERIAL_SPEED,
		    "Error setting the serial port speed");
	GP_ERR_RES (GP_ERROR_IO_SERIAL_BREAK,
		    "Error sending a break to the serial port");
	GP_ERR_RES (GP_ERROR_IO_SERIAL_FLUSH,
		    "Error flushing the serial line");
	GP_ERR_RES (GP_ERROR_IO_USB_CLEAR_HALT,
		    "Error clearing a halt condition on the USB port");
	GP_ERR_RES (GP_ERROR_IO_USB_FIND,
		    "Could not find the requested device on the USB port");
	GP_ERR_RES (GP_ERROR_IO_USB_CLAIM,
		    "Could not claim the USB device");
	GP_ERR_RES (GP_ERROR_IO_LOCK, "Could not lock the device");

	return (N_("Unknown error"));
}

#undef GP_ERR_RES
