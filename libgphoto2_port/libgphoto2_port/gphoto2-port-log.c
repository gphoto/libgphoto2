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
	GPLogLevels  levels;
	GPLogFunc    func;
	void        *data;
} LogFunc;

static LogFunc *log_funcs = NULL;
static unsigned int log_funcs_count = 0;

static char        *log_history = NULL;
static unsigned int log_history_size = 0;

/**
 * gp_log_add_func:
 * @levels: gphoto2 log levels
 * @func: a #GPLogFunc
 * @data: data
 *
 * Adds a log function that will be called for each log message that is flagged
 * with a log level that appears in given log @levels. This function returns
 * an id that you can use for removing the log function again (using
 * #gp_log_remove_func).
 *
 * Return value: an id or a gphoto2 error code
 **/
int
gp_log_add_func (GPLogLevels levels, GPLogFunc func, void *data)
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
	log_funcs[log_funcs_count - 1].levels = levels;
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

static void
gp_log_history_append (const char *msg)
{
	int needed, available, delta;

	/* First of all, skip the whole thing if the message is too long */
	if (strlen (msg) + 2 > log_history_size)
		return;

	/* Do we have to forget parts of the log history? */
	needed = strlen (msg) + 1;
	available = log_history_size - strlen (log_history) - 1;
	delta = available - needed;
	if (delta < 0)
		 memmove (log_history, log_history - delta,
			  log_history_size + delta);

	/* Append the message */
	strcat (log_history, msg);
	strcat (log_history, "\n");
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

/**
 * gp_log:
 * @levels: gphoto2 log levels
 * @domain: the domain
 * @format: the format
 * @...:
 *
 * Logs a message at the given log @levels. You would normally use this
 * function to log strings.
 **/
void
gp_log (GPLogLevels levels, const char *domain, const char *format, ...)
{
	char buffer[2048];
	va_list args;
	int i;

	va_start (args, format);
	for (i = 0; i < log_funcs_count; i++) {
		if (log_funcs[i].levels & levels)
			log_funcs[i].func (levels, domain, format, args,
					   log_funcs[i].data);
	}
	vsnprintf (buffer, sizeof (buffer), format, args);
	gp_log_history_append (buffer);
	va_end (args);
}

/**
 * gp_log_history_set_size:
 * @size: the new size
 *
 * Sets the size of the builtin history to @size.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_log_history_set_size (unsigned int size)
{
	char *new_log_history;

	if (!log_history)
		new_log_history = malloc (sizeof (char) * size);
	else
		new_log_history = realloc (log_history, sizeof (char) * size);

	if (size && !new_log_history)
		return (GP_ERROR_NO_MEMORY);

	log_history_size = size;
	log_history = new_log_history;

	return (GP_OK);
}

/**
 * gp_log_history_get_size:
 *
 * Retreives the current size of the builtin log history. You can adjust the
 * size with #gp_log_history_set_size.
 *
 * Return value: The current size of the builtin log history or a
 * 		 gphoto2 error code
 **/
int
gp_log_history_get_size (void)
{
	return (log_history_size);
}

/**
 * gp_log_history_get:
 *
 * Retreives the current log history.
 *
 * Return value: the current log history
 **/
const char *
gp_log_history_get (void)
{
	return (log_history);
}
