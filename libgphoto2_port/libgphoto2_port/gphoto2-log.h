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

#ifndef __GPHOTO2_LOG_H__
#define __GPHOTO2_LOG_H__

typedef enum {
	GP_LOG_ERROR = 1 << 2,
	GP_LOG_DEBUG = 1 << 7
} GPLogLevels;

/* Custom log function */
typedef void (* GPLogFunc) (GPLogLevels levels, const char *domain,
			    const char *msg, void *data);
int  gp_log_add_func    (GPLogLevels levels, GPLogFunc func, void *data);
int  gp_log_remove_func (int id);

void gp_log      (const char *domain, GPLogLevel levels,
		  const char *format, ...);
void gp_log_data (const char *domain, GPLogLevel levels,
		  const char *data, unsigned int size);

#endif /* __GPHOTO2_LOG_H__ */
