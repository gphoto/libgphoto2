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

#include "gphoto2-debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2-result.h>

static int glob_debug = GP_DEBUG_NONE;

static GPDebugFunc debug_func      = NULL;
static void       *debug_func_data = NULL;

static unsigned int debug_history_size = 0;
static char        *debug_history   = NULL;

void
gp_debug_set_func (GPDebugFunc func, void *data)
{
	debug_func      = func;
	debug_func_data = data;
}

void
gp_debug_printf (int level, const char *id, const char *format, ...)
{
	char buffer[2048];
	va_list arg;
	int i;

	va_start (arg, format);
	vsprintf (buffer, format, arg);
	va_end (arg);

	/* Create history if needed */
	if (!debug_history) {
		debug_history_size = 1024;
		debug_history = malloc (sizeof (char) * debug_history_size);
		memset (debug_history, 0, debug_history_size);
	}

	/* Do we have to forget parts of the debug history? */
	while (debug_history_size - strlen (debug_history) <
			strlen (buffer) + 1) {

		/* Determine len of first entry */
		for (i = 0; debug_history[i] != '\0'; i++)
			if (debug_history[i] == '\n')
				break;

		/* Remove the first entry */
		memmove (debug_history, debug_history + i + 1, 
			strlen (debug_history) - i);
	}

	strcat (debug_history, buffer);
	strcat (debug_history, "\n");

	if (glob_debug == GP_DEBUG_NONE)
		return;

	if (glob_debug >= level) {
		if (debug_func) {

			/* Custom debug function */
			va_start (arg, format);
			vsprintf (buffer, format, arg);
			va_end (arg);
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
	glob_debug = level;

	/* Initialize the IO library with the given debug level */
	gp_port_init (level);
}

int
gp_debug_get_level (void)
{
	return (glob_debug);
}

const char *
gp_debug_history_get (void)
{
	return (debug_history);
}

int
gp_debug_history_set_size (unsigned int size)
{
	char *new_debug_history;

	new_debug_history = realloc (debug_history, sizeof (char) * size);

	if (!new_debug_history)
		return (GP_ERROR_NO_MEMORY);

	debug_history_size = size;
	debug_history = new_debug_history;

	return (GP_OK);
}


