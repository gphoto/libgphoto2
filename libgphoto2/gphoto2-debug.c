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
#include <gphoto2-port-log.h>
#include <gphoto2-port.h>

static int port_func_set = 0;

static int debug_level = GP_DEBUG_NONE;

static void
gp_port_func (GPLogLevels levels, const char *domain,
	      const char *msg, void *data)
{
	if (debug_level) {
		fprintf (stderr, msg);
		fprintf (stderr, "\n");
	}
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

	if (!port_func_set)
		port_func_set = gp_log_add_func (
			GP_LOG_DEBUG | GP_LOG_ERROR, gp_port_func, NULL);

	va_start (arg, format);
#if HAVE_VSNPRINTF
	vsnprintf (buffer, sizeof (buffer), format, arg);
#else
	vsprintf (buffer, format, arg);
#endif
	va_end (arg);

	if (level <= debug_level)
		gp_log (GP_LOG_DEBUG, id, buffer);
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
	debug_level = level;
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
	return (debug_level);
}
