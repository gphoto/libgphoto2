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

#include <gphoto2-port-log.h>

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
	va_list args;
	int loglevel;

	switch (level) {
	case GP_DEBUG_NONE:   loglevel = GP_LOG_ERROR; break;
	case GP_DEBUG_LOW:    loglevel = GP_LOG_VERBOSE; break;
	case GP_DEBUG_MEDIUM: loglevel = GP_LOG_DEBUG; break;
	case GP_DEBUG_HIGH:   loglevel = GP_LOG_DEBUG; break;
	default:              loglevel = GP_LOG_DEBUG; break;
	}

	va_start (args, format);
	gp_logv (loglevel, id, format, args);
	va_end (args);
}
