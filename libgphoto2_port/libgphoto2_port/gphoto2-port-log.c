/* gphoto2-port-log.c
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
#include "gphoto2-port-log.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include <gphoto2-port-result.h>

typedef struct {
	unsigned int id;
	GPLogLevel  level;
	GPLogFunc    func;
	void        *data;
} LogFunc;

static LogFunc *log_funcs = NULL;
static unsigned int log_funcs_count = 0;

/**
 * gp_log_add_func:
 * @level: gphoto2 log level
 * @func: a #GPLogFunc
 * @data: data
 *
 * Adds a log function that will be called for each log message that is flagged
 * with a log level that appears in given log @level. This function returns
 * an id that you can use for removing the log function again (using
 * #gp_log_remove_func).
 *
 * Return value: an id or a gphoto2 error code
 **/
int
gp_log_add_func (GPLogLevel level, GPLogFunc func, void *data)
{
	LogFunc *new_log_funcs;

	if (!func)
		return (GP_ERROR_BAD_PARAMETERS);

	if (!log_funcs)
		new_log_funcs = malloc (sizeof (LogFunc));
	else
		new_log_funcs = realloc (log_funcs, sizeof (LogFunc) * 
					 (log_funcs_count + 1));
	if (!new_log_funcs)
		return (GP_ERROR_NO_MEMORY);

	log_funcs = new_log_funcs;
	log_funcs_count++;

	log_funcs[log_funcs_count - 1].id = log_funcs_count;
	log_funcs[log_funcs_count - 1].level = level;
	log_funcs[log_funcs_count - 1].func = func;
	log_funcs[log_funcs_count - 1].data = data;

	return (log_funcs_count);
}

/**
 * gp_log_remove_func:
 * @id: an id (return value of #gp_log_add_func)
 *
 * Removes the log function with given @id.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_log_remove_func (int id)
{
	if (id < 1 || id > log_funcs_count)
		return (GP_ERROR_BAD_PARAMETERS);

	memmove (log_funcs + id - 1, log_funcs + id, log_funcs_count - id);
	log_funcs_count--;

	return (GP_OK);
}

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

/**
 * gp_log_data:
 * @domain: the domain
 * @data: the data to be logged
 * @size: the size of the @data
 *
 * Takes the @data and creates a formatted hexdump string. If you would like
 * to log text messages, use #gp_log instead.
 **/
void
gp_log_data (const char *domain, const char *data, unsigned int size)
{
	static char hexchars[16] = "0123456789abcdef";
	char *curline, *result;
	int x = HEXDUMP_INIT_X;
	int y = HEXDUMP_INIT_Y;
	int index;
	unsigned char value;

	if (!data) {
		gp_log (GP_LOG_DATA, domain, "No hexdump (NULL buffer)");
		return;
	}

	if (!size) {
		gp_log (GP_LOG_DATA, domain, "Empty hexdump of empty buffer");
		return;
	}

	curline = result = malloc ((HEXDUMP_LINE_WIDTH+1)*(((size-1)/16)+1)+1);
	if (!result) {
		gp_log (GP_LOG_ERROR, "gphoto2-log", "Malloc for %i bytes "
			"failed", (HEXDUMP_LINE_WIDTH+1)*(((size-1)/16)+1)+1);
		return;
	}

	for (index = 0; index < size; ++index) {
                value = (unsigned char)data[index];
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

	gp_log (GP_LOG_DATA, domain, "Hexdump of %i = 0x%x bytes follows:\n%s",
		size, size, result);
	free (result);
}

#undef HEXDUMP_COMPLETE_LINE
#undef HEXDUMP_MIDDLE
#undef HEXDUMP_LINE_WIDTH
#undef HEXDUMP_INIT_Y
#undef HEXDUMP_INIT_X
#undef HEXDUMP_BLOCK_DISTANCE
#undef HEXDUMP_OFFSET_WIDTH

/**
 * gp_logv:
 * @level: gphoto2 log level
 * @domain: the domain
 * @format: the format
 * @args: the va_list corresponding to @format
 *
 * Logs a message at the given log @level. You would normally use this
 * function to log as yet unformatted strings. 
 **/
void
gp_logv (GPLogLevel level, const char *domain, const char *format,
	 va_list args)
{
	int i;

	for (i = 0; i < log_funcs_count; i++) {
		if (log_funcs[i].level >= level)
			log_funcs[i].func (level, domain, format, args,
					   log_funcs[i].data);
	}
}

/**
 * gp_log:
 * @level: gphoto2 log level
 * @domain: the domain
 * @format: the format
 * @...:
 *
 * Logs a message at the given log @level. You would normally use this
 * function to log strings.
 **/
void
gp_log (GPLogLevel level, const char *domain, const char *format, ...)
{
	va_list args;

	va_start (args, format);
	gp_logv (level, domain, format, args);
	va_end (args);
}
