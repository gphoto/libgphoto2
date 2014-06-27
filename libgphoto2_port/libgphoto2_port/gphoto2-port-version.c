/** \file
 *
 * Copyright 2002 Hans Ulrich Niedermann <gp@n-dimensional.de
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
#include <stdlib.h>

#include <gphoto2/gphoto2-port-version.h>

const char **gp_port_library_version(GPVersionVerbosity verbose)
{
	/* we could also compute/parse the short strings from the long
	   ones, but the current method is easier for now :-) */
	static const char *shrt[] =
		{
			PACKAGE_VERSION,
#ifdef HAVE_CC
			HAVE_CC,
#else
			"unknown cc",
#endif
#ifdef HAVE_LTDL
			"ltdl",
#else
			"no ltdl",
#endif
#ifdef HAVE_LIBUSB
			"USB",
#else
			"no USB",
#endif
#ifdef HAVE_SERIAL
			"serial "

#ifdef HAVE_RESMGR
			"resmgr locking",
#elif HAVE_TTYLOCK
			"ttylock locking",
#elif HAVE_LOCKDEV
			"lockdev locking",
#else
			"without locking",
#endif

#else
			"no serial",
#endif
			NULL
		};
	static const char *verb[] =
		{
			PACKAGE_VERSION,
#ifdef HAVE_CC
			HAVE_CC " (C compiler used)",
#else
			"unknown (C compiler used)",
#endif
#ifdef HAVE_LTDL
			"ltdl (for portable loading of camlibs)",
#else
			"no ltdl (for portable loading of camlibs)",
#endif
#ifdef HAVE_LIBUSB_WIN32
			"USB (libusb-win32, for USB cameras)",
#elif defined(HAVE_LIBUSB1)
			"USB (libusb1, for USB cameras)",
#elif defined(HAVE_LIBUSB)
			"USB (libusb0, for USB cameras)",
#else
			"no USB (for USB cameras)",
#endif
#ifdef HAVE_SERIAL
			"serial (for serial cameras)",
#else
			"no serial (for serial cameras)",
#endif
#ifdef HAVE_RESMGR
			"resmgr (serial port access and locking)",
#else
			"no resmgr (serial port access and locking)",
#endif
#ifdef HAVE_TTYLOCK
			"ttylock (serial port locking)",
#else
			"no ttylock (serial port locking)",
#endif
#ifdef HAVE_LOCKDEV
			"lockdev (serial port locking)",
#else
			"no lockdev (serial port locking)",
#endif
			NULL
		};
	return((verbose == GP_VERSION_VERBOSE)?verb:shrt);
}

/*
static void foobar() {
	GPVersionFunc gp_port_lib_ver_func = NULL;
	gp_port_lib_ver_func = gp_port_library_version;
	gp_port_lib_ver_func(GP_VERSION_VERBOSE);
}
*/

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
