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

/*
 * gp_port_debug_print_data:
 * 
 * Takes the buffer and creates a hexdump string formatted like
 * 00000000 00 11 22 33 44 55 66 77-88 99 aa bb cc dd ee ff ...ASCIIcodes...
 * 00000010 00 11 22 33 44 55 66 77-88 99                   ...ASCIIco
 *
 * At the moment, this routine calls the following expensive methods:
 *  - malloc()   1 time per call
 *  - and nothing more :-)
 *
 * If you want to make it faster, use a static buffer instead of
 * dynamically malloc'ing the result - and don't forget to check
 * the size parameter if the hexdump fit into that static buffer.
 */

/*
 * Width of offset field in characters. Note that HEXDUMP_COMPLETE_LINE 
 * needs to be changed when this value is changed.
 */
#define HEXDUMP_OFFSET_WIDTH 4

/*
 * Distance between offset, hexdump and ascii blocks. Note that
 * HEXDUMP_COMPLETE_LINE needs to be changed when this value is changed.
 */
#define HEXDUMP_BLOCK_DISTANCE 2

/* Initial value for x "pointer" (for hexdump) */
#define HEXDUMP_INIT_X (HEXDUMP_OFFSET_WIDTH + HEXDUMP_BLOCK_DISTANCE)

/* Initial value for y "pointer" (for ascii values) */
#define HEXDUMP_INIT_Y (HEXDUMP_INIT_X + 3 * 16 - 1 + HEXDUMP_BLOCK_DISTANCE)

/* Used to switch to next line */
#define HEXDUMP_LINE_WIDTH (HEXDUMP_INIT_Y + 16)

/* Used to put the '-' character in the middle of the hexdumps */
#define HEXDUMP_MIDDLE (HEXDUMP_INIT_X + 3 * 8 - 1)

/*
 * We are lazy and do our typing only once. Please not that you have to
 * add/remove some lines when increasing/decreasing the values of 
 * HEXDUMP_BLOCK_DISTANCE and/or HEXDUMP_OFFSET_WIDTH.
 */
#define HEXDUMP_COMPLETE_LINE {\
	curline[HEXDUMP_OFFSET_WIDTH - 4] = hexchars[(index >> 12) & 0xf]; \
	curline[HEXDUMP_OFFSET_WIDTH - 3] = hexchars[(index >>  8) & 0xf]; \
	curline[HEXDUMP_OFFSET_WIDTH - 2] = hexchars[(index >>  4) & 0xf]; \
	curline[HEXDUMP_OFFSET_WIDTH - 1] = '0'; \
	curline[HEXDUMP_OFFSET_WIDTH + 0] = ' '; \
	curline[HEXDUMP_OFFSET_WIDTH + 1] = ' '; \
	curline[HEXDUMP_MIDDLE] = '-'; \
	curline[HEXDUMP_INIT_Y-2] = ' '; \
	curline[HEXDUMP_INIT_Y-1] = ' '; \
	curline[HEXDUMP_LINE_WIDTH] = '\n'; \
	curline = curline + (HEXDUMP_LINE_WIDTH + 1);}

void
gp_port_debug_print_data (int target_debug_level, int debug_level,
			  const char *bytes, int size)
{
	static char hexchars[16] = "0123456789abcdef";
	char *curline, *result;
	int x = HEXDUMP_INIT_X;
	int y = HEXDUMP_INIT_Y;
	int index;
	unsigned char value;

	if (!bytes) {
		gp_port_debug_printf (target_debug_level, debug_level,
				      "No hexdump (NULL buffer)");
		return;
	}

	if (size < 0) {
		gp_port_debug_printf (target_debug_level, debug_level,
				      "No hexdump of buffer with negative "
				      "size");
		return;
	}

	if (size == 0) {
		gp_port_debug_printf (target_debug_level, debug_level,
				      "Empty hexdump of empty buffer");
		return;
	}

	curline = result = malloc ((HEXDUMP_LINE_WIDTH+1)*(((size-1)/16)+1)+1);
	if (!result)
		return;

	for (index = 0; index < size; ++index) {
		value = (unsigned char)bytes[index];
		curline[x] = hexchars[value >> 4];
		curline[x+1] = hexchars[value & 0xf];
		curline[x+2] = ' ';
		curline[y++] = ((value>=32)&&(value<127))?value:'.';
		x += 3;
		if ((index & 0xf) == 0xf) { /* end of line */
			x = HEXDUMP_INIT_X;
			y = HEXDUMP_INIT_Y;
			HEXDUMP_COMPLETE_LINE;
		}
	}
	if ((index & 0xf) != 0) { /* not at end of line yet? */
		/* if so, complete this line */
		while (y < HEXDUMP_INIT_Y + 16) {
			curline[x+0] = ' ';
			curline[x+1] = ' ';
			curline[x+2] = ' ';
			curline[y++] = ' ';
			x += 3;
		}
		HEXDUMP_COMPLETE_LINE;
	}
	curline[0] = '\0';
	gp_port_debug_printf (target_debug_level, debug_level, "%s", result);
	free (result);
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
