/* gphoto2-port-version.c
 *
 * Copyright (C) 2002 Hans Ulrich Niedermann <gp@n-dimensional.de
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
#include <stdlib.h>

#include <gphoto2-port-version.h>

const char **gp_port_library_version(GPVersionVerbosity verbose)
{
	static const char *shrt[] =
		{
			VERSION,
#ifdef HAVE_USB
			"USB",
#else
			"no USB",
#endif
#ifdef HAVE_SERIAL
			"serial "
#ifdef HAVE_BAUDBOY
			"baudboy locking",
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
			VERSION,
#ifdef HAVE_USB
			"USB (for USB cameras)",
#else
			"no USB (for USB cameras)",
#endif
#ifdef HAVE_SERIAL
			"serial (for serial cameras)",
#else
			"no serial (for serial cameras)",
#endif
#ifdef HAVE_BAUDBOY
			"baudboy (serial port locking)",
#else
			"no baudboy (serial port locking)",
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
