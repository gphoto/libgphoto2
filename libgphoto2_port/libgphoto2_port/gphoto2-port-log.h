/* gphoto2-port-log.h
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

#ifndef __GPHOTO2_PORT_LOG_H__
#define __GPHOTO2_PORT_LOG_H__

#include <stdarg.h>

typedef enum {
	GP_LOG_ERROR = 1 << 2,
	GP_LOG_DATA  = 1 << 6,
	GP_LOG_DEBUG = 1 << 7
} GPLogLevels;

/* Custom log function */
typedef void (* GPLogFunc) (GPLogLevels levels, const char *domain,
			    const char *format, va_list args, void *data);
int  gp_log_add_func    (GPLogLevels levels, GPLogFunc func, void *data);
int  gp_log_remove_func (int id);

/* Logging */
void gp_log      (GPLogLevels levels, const char *domain,
		  const char *format, ...);
void gp_logv     (GPLogLevels level, const char *domain, const char *format,
		  va_list args);
void gp_log_data (const char *domain, const char *data, unsigned int size);

#endif /* __GPHOTO2_PORT_LOG_H__ */
