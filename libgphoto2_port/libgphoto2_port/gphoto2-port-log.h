/* gphoto2-port-log.h
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
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
	GP_LOG_ERROR = 0,
	GP_LOG_VERBOSE = 1,
	GP_LOG_DEBUG = 2,
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

typedef void (* GPLogFunc) (GPLogLevel level, const char *domain,
			    const char *format, va_list args, void *data);

#ifndef DISABLE_DEBUGGING

int  gp_log_add_func    (GPLogLevel level, GPLogFunc func, void *data);
int  gp_log_remove_func (int id);

/* Logging */
void gp_log      (GPLogLevel level, const char *domain,
		  const char *format, ...);
void gp_logv     (GPLogLevel level, const char *domain, const char *format,
		  va_list args);
void gp_log_data (const char *domain, const char *data, unsigned int size);

/**
 * GP_LOG:
 * @level: gphoto2 log level to log message under
 * @msg: message to log
 * @params: params to message
 *
 * Calls #gp_log with an automatically generated domain.
 * You have to define GP_MODULE as "mymod" for your module
 * mymod before you can use #GP_LOG().
 **/

#ifdef __GNUC__
#define GP_LOG(level, msg, params...) \
        gp_log(level, GP_MODULE "/" __FILE__, msg, ##params)

#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define GP_LOG(level, ...) \
        gp_log(level, GP_MODULE "/" __FILE__, __VA_ARGS__)

#else
# ifdef __GCC__
#  warning Disabling GP_LOG because variadic macros are not allowed
# endif
#define GP_LOG (void) 
#endif

/**
 * GP_DEBUG:
 * @msg: message to log
 * @params: params to message
 *
 * Logs message at log level #GP_LOG_DEBUG by calling #gp_log() with
 * an automatically generated domain
 * You have to define GP_MODULE as "mymod" for your module
 * mymod before using #GP_DEBUG().
 **/

#ifdef __GNUC__
#define GP_DEBUG(msg, params...) \
        gp_log(GP_LOG_DEBUG, GP_MODULE "/" __FILE__, msg, ##params)

#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define GP_DEBUG(...) \
        gp_log(GP_LOG_DEBUG, GP_MODULE "/" __FILE__, __VA_ARGS__)

#else
# ifdef __GCC__
#  warning Disabling GP_DEBUG because variadic macros are not allowed
# endif
#define GP_DEBUG (void) 
#endif

#else /* DISABLE_DEBUGGING */

/* Stub these functions out if debugging is disabled */
#define gp_log_add_func(level, func, data) (0)
#define gp_log_remove_func(id) (0)
#define gp_log(level, domain, format, args...) /**/
#define gp_logv(level, domain, format, args) /**/
#define gp_log_data(domain, data, size) /**/

#ifdef __GNUC__
#define GP_LOG(level, msg, params...) /**/
#define GP_DEBUG(msg, params...) /**/

#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define GP_LOG(level, ...) /**/
#define GP_DEBUG(...) /**/

#else
#define GP_LOG (void) 
#define GP_DEBUG (void)
#endif

#endif /* DISABLE_DEBUGGING */

#endif /* __GPHOTO2_PORT_LOG_H__ */
