/* debug.c
 *
 * Copyright (C) 2001 Lutz Müller
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
#include "gphoto2-debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2-result.h>
#include <gphoto2-port-debug.h>
#include <gphoto2-port.h>

static GPDebugFunc debug_func      = NULL;
static void       *debug_func_data = NULL;

static void
gp_port_func (const char *msg, void *data)
{
	if (debug_func)
		debug_func ("port", msg, data);
}

void
gp_debug_set_func (GPDebugFunc func, void *data)
{
	debug_func      = func;
	debug_func_data = data;

	if (func)
		gp_port_debug_set_func (gp_port_func, data);
	else
		gp_port_debug_set_func (NULL, NULL);
}

void
gp_debug_printf (int level, const char *id, const char *format, ...)
{
	char buffer[2048];
	va_list arg;

	va_start (arg, format);
#if HAVE_VSNPRINTF
	vsnprintf (buffer, sizeof (buffer), format, arg);
#else
	vsprintf (buffer, format, arg);
#endif
	va_end (arg);

	gp_port_debug_history_append (buffer);

	if (level <= gp_port_debug_get_level ()) {
		if (debug_func) {

			/* Custom debug function */
			debug_func (id, buffer, debug_func_data);

		} else {

			/* Own implementation */
			fprintf (stderr, "%s: ", id);
			va_start (arg, format);
			vfprintf (stderr, format, arg);
			va_end (arg);
			fprintf (stderr, "\n");
		}
	}
}

void
gp_debug_set_level (int level)
{
	gp_port_debug_set_level (level);
}

int
gp_debug_get_level (void)
{
	return (gp_port_debug_get_level ());
}

const char *
gp_debug_history_get (void)
{
	return (gp_port_debug_history_get ());
}

int
gp_debug_history_set_size (unsigned int size)
{
	return (gp_port_debug_history_set_size (size));
}

int
gp_debug_history_get_size (void)
{
	return (gp_port_debug_history_get_size ());
}
