/* gphoto2-log.h
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
#include "gphoto2-log.h"

typedef struct {
	unsigned int id;
	GPLogLevels  levels;
	GPLogFunc   *func;
	void        *data;
} LogFunc;

static LogFunc *log_funcs = NULL;
static unsigned int log_funcs_count = 0;

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
					 log_funcs_count + 1);
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

int
gp_log_remove_func (int id)
{
	if (id < 1 || id > log_funcs_count)
		return (GP_ERROR_BAD_PARAMETERS);

	memmove (log_funcs + id - 1, log_funcs + id, log_funcs_count - id);
	log_funcs_count--;

	return (GP_OK);
}

void
gp_log_data (GPLogLevels levels, const char *domain, const char *data,
	     unsigned int size)
{
	printf ("Implement...\n");
}

void
gp_log (GPLogLevels levels, const char *domain, const char *format, ...)
{
	char buffer[2048];
	va_list arg;
	int i;

	va_start (arg, format);
	vsnprintf (buffer, sizeof (buffer), format, arg);
	va_end (arg);

	for (i = 0; i < log_funcs_count; i++) {
		if (log_funcs[i].levels & levels)
			log_funcs[i].func (levels, domain, buffer,
					   log_funcs[i].data);
	}
}
