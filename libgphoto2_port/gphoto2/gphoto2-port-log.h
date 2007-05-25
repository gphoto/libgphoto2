/** \file gphoto2-port-log.h
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

/**
 * \brief Logging level
 * Specifies the logging severity level.
 */
typedef enum {
	GP_LOG_ERROR = 0,	/**< \brief Log message is an error infomation. */
	GP_LOG_VERBOSE = 1,	/**< \brief Log message is an verbose debug infomation. */
	GP_LOG_DEBUG = 2,	/**< \brief Log message is an debug infomation. */
	GP_LOG_DATA = 3		/**< \brief Log message is a data hex dump. */
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
 * \brief Logging function hook
 * 
 * This is the function frontends can use to receive logging information
 * from the libgphoto2 framework. It is set using gp_log_add_func() and 
 * removed using gp_log_remove_func() and will then receive the logging
 * messages of the level specified.
 *
 * \param level the log level of the passed message, as set by the camera driver or libgphoto2
 * \param domain the logging domain as set by the camera driver, or libgphoto2 function
 * \param format the printf style format string of the logmessage, without linefeed
 * \param args the arguments as va_list, depending on the format string
 * \param data the caller private data that was passed to gp_log_add_func()
 */
typedef void (* GPLogFunc) (GPLogLevel level, const char *domain,
			    const char *format, va_list args, void *data)
#if (__GNUC__ >= 3)
	__attribute__((__format__(printf,3,0)))
#endif
;

#ifndef DISABLE_DEBUGGING

int  gp_log_add_func    (GPLogLevel level, GPLogFunc func, void *data);
int  gp_log_remove_func (int id);

/* Logging */
void gp_log      (GPLogLevel level, const char *domain,
		  const char *format, ...)
#ifdef __GNUC__
	__attribute__((__format__(printf,3,4)))
#endif
;
void gp_logv     (GPLogLevel level, const char *domain, const char *format,
		  va_list args)
#ifdef __GNUC__
	__attribute__((__format__(printf,3,0)))
#endif
;
void gp_log_data (const char *domain, const char *data, unsigned int size);


/*
 * GP_DEBUG:
 * msg: message to log
 * params: params to message
 *
 * Logs message at log level #GP_LOG_DEBUG by calling #gp_log() with
 * an automatically generated domain
 * You have to define GP_MODULE as "mymod" for your module
 * mymod before using #GP_DEBUG().
 */

#ifdef _GPHOTO2_INTERNAL_CODE
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define GP_DEBUG(...) \
        gp_log(GP_LOG_DEBUG, GP_MODULE "/" __FILE__, __VA_ARGS__)
#elif defined(__GNUC__) &&  __GNUC__ >= 2
#define GP_DEBUG(msg, params...) \
        gp_log(GP_LOG_DEBUG, GP_MODULE "/" __FILE__, msg, ##params)
#else
# ifdef __GNUC__
#  warning Disabling GP_DEBUG because variadic macros are not allowed
# endif
#define GP_DEBUG (void) 
#endif
#endif /* _GPHOTO2_INTERNAL_CODE */

#else /* DISABLE_DEBUGGING */

/* Stub these functions out if debugging is disabled */
#define gp_log_add_func(level, func, data) (0)
#define gp_log_remove_func(id) (0)
#define gp_log(level, domain, format, args...) /**/
#define gp_logv(level, domain, format, args) /**/
#define gp_log_data(domain, data, size) /**/

#ifdef _GPHOTO2_INTERNAL_CODE
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define GP_DEBUG(...) /* no-op */
#elif defined(__GNUC__)
#define GP_DEBUG(msg, params...) /* no-op */
#else
#define GP_DEBUG (void)
#endif
#endif /* _GPHOTO2_INTERNAL_CODE */

#endif /* DISABLE_DEBUGGING */

#ifdef _GPHOTO2_INTERNAL_CODE

  typedef struct StringFlagItem {
    char *str;
    unsigned int flag;
  } StringFlagItem;

  typedef void (*string_item_func) (const char *str, void *data);

  const char * 
  gpi_enum_to_string(const unsigned int _enum, 
		     const StringFlagItem *map);

  int
  gpi_string_to_enum(const char *str,
		     unsigned int *result,
		     const StringFlagItem *map);

  void 
  gpi_flags_to_string_list(const unsigned int flags, 
			   const StringFlagItem *map,
			   string_item_func func, void *data);
  
  int 
  gpi_string_or_to_flags(const char *str, 
			 unsigned int *flags,
			 const StringFlagItem *map);

  unsigned int 
  gpi_string_to_flag(const char *str, 
		     const StringFlagItem *map);

  unsigned int 
  gpi_string_list_to_flags(const char *str[], 
			   const StringFlagItem *map);
  
#endif /* _GPHOTO2_INTERNAL_CODE */


#endif /* __GPHOTO2_PORT_LOG_H__ */
