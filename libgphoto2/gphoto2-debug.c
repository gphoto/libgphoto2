/* gphoto2-debug.c
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

/**
 * gp_debug_set_func:
 * @func: a #GPDebugFunc
 * @data: data that should be passed to the func
 *
 * Sets a user defined debugging function. Per default, the function is set
 * to %NULL and a built in debugging function is used which prints messages
 * onto stderr.
 **/
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

/**
 * gp_debug_printf:
 * @level: a gphoto2 debug level
 * @id: an id
 * @format: the debugging message
 * @...:
 *
 * Prints a debugging message. The debugging message gets added to the
 * builtin history. If the given @level is higher than the global debug level
 * (set with #gp_debug_set_level), the message gets printed out either via
 * the user defined debugging function (set with #gp_debug_set_func) or the
 * builtin function (default).
 **/
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

/**
 * gp_debug_set_level:
 * @level: a gphoto2 debug level
 *
 * Sets the debugging level. Only debugging messages with a debugging level
 * higher than the given @level will be printed.
 **/
void
gp_debug_set_level (int level)
{
	gp_port_debug_set_level (level);
}

/**
 * gp_debug_get_level:
 *
 * Gets the current debugging level.
 *
 * Return value: the current gphoto2 debug level or a gphoto2 error code
 **/
int
gp_debug_get_level (void)
{
	return (gp_port_debug_get_level ());
}

/**
 * gp_debug_history_get:
 *
 * Retreives the current debugging history. The size of the history is 
 * limited - you can change it with #gp_debug_history_set_size.
 *
 * Return value: The current debugging history
 **/
const char *
gp_debug_history_get (void)
{
	return (gp_port_debug_history_get ());
}

/**
 * gp_debug_history_set_size:
 * @size: the new size of the debugging history
 *
 * Sets the size of the builtin debugging history.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_debug_history_set_size (unsigned int size)
{
	return (gp_port_debug_history_set_size (size));
}

/**
 * gp_debug_history_get_size:
 * 
 * Retreives the size of the builtin debuggin history.
 *
 * Return value: The current debugging history size or a gphoto2 error code
 **/
int
gp_debug_history_get_size (void)
{
	return (gp_port_debug_history_get_size ());
}
