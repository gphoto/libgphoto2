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

/**
 * GPLogLevel:
 *
 * foo bar blah blah
 **/

typedef enum {

/**
 * GP_LOG_ERROR:
 * 
 * An error occured, i.e. the attempted operation failed. E.g. 
 * "Cannot open device /dev/ttyS1: permission denied"
 * These messages will usually be logged by #gp_port_set_error.
 **/
	GP_LOG_ERROR = 0,

/**
 * GP_LOG_VERBOSE:
 *
 * These messages help keep roughly track of what the
 * libraries are doing internally. Useful distinction
 * from GP_LOG_DEBUG for approximate determination of 
 * the region an bug may be located.
 * E.g. "getting file %s from cam".
 **/
	GP_LOG_VERBOSE = 1,

/**
 * GP_LOG_DEBUG:
 * 
 * Debug messages, e.g. "fetching chunk %i/%i of file %s"
 * These messages will usually logged by any kind of code.
 **/
	GP_LOG_DEBUG = 2,

/**
 * GP_LOG_DATA:
 * 
 * Log hex dump of all traffic between library and camera
 * E.g. "Sending 4 bytes:\n0000 12 af fe 7c\n"
 * This creates LOTS of data!
 * These messages will usually be logged by port interface libraries
 * in their send and receive functions
 **/
	GP_LOG_DATA = 3
} GPLogLevel;

/**
 * GP_LOG_ALL:
 * 
 * Used by frontends if they want to be sure their 
 * callback function receives all messages. Defined 
 * as the highest debug level. Can make frontend code 
 * more understandable and extension of log levels
 * easier.
 **/
#define GP_LOG_ALL GP_LOG_DATA

/**
 * GPLogFunc:
 * @level: a #GPLogLevel
 * @domain: describes the context in which the message was created
 * 	    (driver, src file)
 * @format: formatting string of the message
 * @args:   arguments for formatting string
 * @data: some static data, is provided when registering this function with
 * 	  #gp_log_add_func
 *
 * This is a prototype for a log function a frontend may provide to the 
 * library, thus
 * being able to digest and process log messages in an appropriate way.
 **/
typedef void (* GPLogFunc) (GPLogLevel level, const char *domain,
			    const char *format, va_list args, void *data);
int  gp_log_add_func    (GPLogLevel level, GPLogFunc func, void *data);
int  gp_log_remove_func (int id);

/* Logging */
void gp_log      (GPLogLevel level, const char *domain,
		  const char *format, ...);
void gp_logv     (GPLogLevel level, const char *domain, const char *format,
		  va_list args);
void gp_log_data (const char *domain, const char *data, unsigned int size);

/*#ifndef GP_MODULE
 *#define GP_MODULE "unknown"
 *#endif
 */

/**
 * GP_LOG:
 * @level: gphoto2 log level to log message under
 * @msg: message to log
 * @params: params to message
 *
 * calls #gp_log with an automatically generated domain
 **/

#define GP_LOG(level, msg, params...) \
        gp_log(level, GP_MODULE "/" __FILE__, msg, ##params)

/**
 * GP_DEBUG:
 * @msg: message to log
 * @params: params to message
 *
 * Logs message at log level #GP_LOG_DEBUG by calling #gp_log() with
 * an automatically generated domain
 **/

#define GP_DEBUG(msg, params...) \
        gp_log(GP_LOG_DEBUG, GP_MODULE "/" __FILE__, msg, ##params)

#endif /* __GPHOTO2_PORT_LOG_H__ */
