/* debug.c
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
#include "gphoto2-port-debug.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <gphoto2-port-result.h>

int glob_debug_level = 0;

static GPPortDebugFunc debug_func      = NULL;
static void           *debug_func_data = NULL;

static unsigned int debug_history_size = 0;
static char        *debug_history   = NULL;

void
gp_port_debug_set_level (int level)
{
	glob_debug_level = level;
}

void
gp_port_debug_set_func (GPPortDebugFunc func, void *data)
{
	debug_func = func;
	debug_func_data = data;
}

void
gp_port_debug_printf (int target_debug_level, int debug_level,
		      char *format, ...)
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

	if ((debug_level > 0) && (debug_level >= target_debug_level)) {
		if (debug_func) {

			/* Custom debug function */
			debug_func (buffer, debug_func_data);

		} else {

			/* Own implementation */
			fprintf(stderr, "gp_port: ");
			va_start(arg, format);
			vfprintf(stderr, format, arg);
			va_end(arg);
			fprintf(stderr, "\n");
		}
	}
}

void
gp_port_debug_history_append (const char *msg)
{
	int i;

	/* Create history if needed */
	if (!debug_history) {
		debug_history_size = 1024;
		debug_history = malloc (sizeof (char) * debug_history_size);
		memset (debug_history, 0, sizeof (char) * debug_history_size);
	}

	/* First of all, skip the whole thing if the message is too long */
	if (strlen (msg) + 2 > debug_history_size)
		return;

	/* Do we have to forget parts of the debug history? */
	while (debug_history_size - strlen (debug_history) <
			strlen (msg) + 1) {

		/* Determine len of first entry */
		for (i = 0; debug_history[i] != '\0'; i++)
			if (debug_history[i] == '\n')
				break;

		/* Remove the first entry */
		memmove (debug_history, debug_history + i + 1,
			 strlen (debug_history) - i);
	}

	/* Append the message */
	strcat (debug_history, msg);
	strcat (debug_history, "\n");
}

int
gp_port_debug_history_set_size (unsigned int size)
{
	char *new_debug_history;

	new_debug_history = realloc (debug_history, sizeof (char) * size);

	if (!new_debug_history)
		return (GP_ERROR_IO_MEMORY);

	debug_history_size = size;
	debug_history = new_debug_history;

	return (GP_OK);
}

const char *
gp_port_debug_history_get (void)
{
	return (debug_history);
}
