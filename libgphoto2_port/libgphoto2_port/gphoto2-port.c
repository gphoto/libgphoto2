/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gphoto2-port.c
 * 
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
 * Copyright © 1999 Scott Fritzinger <scottf@unr.edu>
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

#include "config.h"
#include "gphoto2-port.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_LTDL
#include <ltdl.h>
#endif

#include "gphoto2-port-result.h"
#include "gphoto2-port-info-list.h"
#include "gphoto2-port-library.h"
#include "gphoto2-port-log.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#  else
#      define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CHECK_RESULT(result) {int r=(result); if (r<0) return (r);}
#define CHECK_NULL(m) {if (!(m)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_SUPP(p,t,o) {if (!(o)) {gp_port_set_error ((p), _("The operation '%s' is not supported by this device"), (t)); return (GP_ERROR_NOT_SUPPORTED);}}
#define CHECK_INIT(p) {if (!(p)->pc->ops) {gp_port_set_error ((p), _("The port has not yet been initialized")); return (GP_ERROR_BAD_PARAMETERS);}}

/**
 * GPPortPrivateCore:
 *
 * This structure contains private data.
 **/
struct _GPPortPrivateCore {
	char error[2048];

	GPPortInfo info;
	GPPortOperations *ops;
#ifdef HAVE_LTDL
	lt_dlhandle lh;
#else
	void *lh; /* Library handle */
#endif
};

/**
 * gp_port_new:
 * @port:
 *
 * Allocates the memory for a new #GPPort. After you called this function, 
 * you probably want to call #gp_port_set_info in order to make the newly
 * created @port functional.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_new (GPPort **port)
{
	CHECK_NULL (port);

        gp_log (GP_LOG_DEBUG, "gphoto2-port", "Creating new device...");

	*port = malloc (sizeof (GPPort));
        if (!(*port))
		return (GP_ERROR_NO_MEMORY);
        memset (*port, 0, sizeof (GPPort));

	(*port)->pc = malloc (sizeof (GPPortPrivateCore));
	if (!(*port)->pc) {
		gp_port_free (*port);
		return (GP_ERROR_NO_MEMORY);
	}
	memset ((*port)->pc, 0, sizeof (GPPortPrivateCore));

        return (GP_OK);
}

static int
gp_port_init (GPPort *port)
{
	CHECK_NULL (port);
	CHECK_INIT (port);

	if (port->pc->ops->init)
		CHECK_RESULT (port->pc->ops->init (port));

	return (GP_OK);
}

static int
gp_port_exit (GPPort *port)
{
	CHECK_NULL (port);
	CHECK_INIT (port);

	if (port->pc->ops->exit)
		CHECK_RESULT (port->pc->ops->exit (port));

	return (GP_OK);
}

/**
 * gp_port_set_info:
 * @port: a #GPPort
 * @info: the #GPPortInfo
 *
 * Makes a @port functional. After calling this function, you can 
 * access the port using for example #gp_port_open.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_set_info (GPPort *port, GPPortInfo info)
{
	GPPortLibraryOperations ops_func;

	CHECK_NULL (port);

	memcpy (&port->pc->info, &info, sizeof (GPPortInfo));
	port->type = info.type;

	/* Clean up */
	if (port->pc->ops) {
		gp_port_exit (port);
		free (port->pc->ops);
		port->pc->ops = NULL;
	}
	if (port->pc->lh) {
#ifdef HAVE_LTDL
		lt_dlclose (port->pc->lh);
		lt_dlexit ();
#else
		GP_SYSTEM_DLCLOSE (port->pc->lh);
#endif
	}

#ifdef HAVE_LTDL
	lt_dlinit ();
	port->pc->lh = lt_dlopenext (info.library_filename);
	if (!port->pc->lh) {
		gp_log (GP_LOG_ERROR, "gphoto2-port", "Could not load "
			"'%s' ('%s').", info.library_filename,
			lt_dlerror ());
		lt_dlexit ();
		return (GP_ERROR_LIBRARY);
	}
#else
	port->pc->lh = GP_SYSTEM_DLOPEN (info.library_filename);
	if (!port->pc->lh) {
		gp_log (GP_LOG_ERROR, "gphoto2-port", "Could not load "
			"'%s' ('%s')", info.library_filename,
			GP_SYSTEM_DLERROR ());
		return (GP_ERROR_LIBRARY);
	}
#endif

	/* Load the operations */
#ifdef HAVE_LTDL
	ops_func = lt_dlsym (port->pc->lh, "gp_port_library_operations");
#else
	ops_func = GP_SYSTEM_DLSYM (port->pc->lh, "gp_port_library_operations");
#endif
	if (!ops_func) {
#ifdef HAVE_LTDL
		gp_log (GP_LOG_ERROR, "gphoto2-port", "Could not find "
			"'gp_port_library_operations' in '%s' ('%s')",
			info.library_filename, lt_dlerror ());
		lt_dlclose (port->pc->lh);
		lt_dlexit ();
#else
		gp_log (GP_LOG_ERROR, "gphoto2-port", "Could not find "
			"'gp_port_library_operations' in '%s' ('%s')",
			info.library_filename, GP_SYSTEM_DLERROR ());
		GP_SYSTEM_DLCLOSE (port->pc->lh);
#endif
		port->pc->lh = NULL;
		return (GP_ERROR_LIBRARY);
	}
	port->pc->ops = ops_func ();
	gp_port_init (port);

	/* Initialize the settings to some default ones */
	switch (info.type) {
	case GP_PORT_SERIAL:
		strncpy (port->settings.serial.port, info.path,
			 sizeof (port->settings.serial.port));
		port->settings.serial.speed = 0;
		port->settings.serial.bits = 8;
		port->settings.serial.parity = 0;
		port->settings.serial.stopbits = 1;
		gp_port_set_timeout (port, 500);
		break;
	case GP_PORT_USB:
		strncpy (port->settings.usb.port, info.path,
			 sizeof (port->settings.usb.port));
		port->settings.usb.inep = -1;
		port->settings.usb.outep = -1;
		port->settings.usb.config = -1;
		port->settings.usb.interface = 0;
		port->settings.usb.altsetting = -1;
		gp_port_set_timeout (port, 5000);
		break;
	default:
		/* Nothing in here */
		break;
	}
	gp_port_set_settings (port, port->settings);

	return (GP_OK);
}

/**
 * gp_port_get_info:
 * @port: a #GPPort
 * @info: a #GPPortInfo
 *
 * Retreives @info about the port.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_get_info (GPPort *port, GPPortInfo *info)
{
	CHECK_NULL (port && info);

	memcpy (info, &port->pc->info, sizeof (GPPortInfo));

	return (GP_OK);
}

/**
 * gp_port_open:
 * @port: a #GPPort
 *
 * Opens a port which should have been created with #gp_port_new and 
 * configured with #gp_port_set_info and #gp_port_set_settings
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_open (GPPort *port)
{
	CHECK_NULL (port);
	CHECK_INIT (port);

	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Opening %s port...",
		port->type == GP_PORT_SERIAL ? "SERIAL" : 
			(port->type == GP_PORT_USB ? "USB" : ""));

	CHECK_SUPP (port, _("open"), port->pc->ops->open);
	CHECK_RESULT (port->pc->ops->open (port));

	return GP_OK;
}

/**
 * gp_port_close:
 * @port: a #GPPort
 *
 * Closes a @port temporarily. It can afterwards be reopened using
 * #gp_port_open.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_close (GPPort *port)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Closing port...");

	CHECK_NULL (port);
	CHECK_INIT (port);

	CHECK_SUPP (port, _("close"), port->pc->ops->close);
        CHECK_RESULT (port->pc->ops->close(port));

	return (GP_OK);
}

/**
 * gp_port_free:
 * @port: a #GPPort
 *
 * Closes the @port and frees the memory.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_free (GPPort *port)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Freeing port...");

	CHECK_NULL (port);

	if (port->pc) {
		if (port->pc->ops) {

			/* We don't care for return values */
			gp_port_close (port);
			gp_port_exit (port);

			free (port->pc->ops);
			port->pc->ops = NULL;
		}

		if (port->pc->lh) {
#ifdef HAVE_LTDL
			lt_dlclose (port->pc->lh);
			lt_dlexit ();
#else
		        GP_SYSTEM_DLCLOSE (port->pc->lh);
#endif
			port->pc->lh = NULL;
		}

		free (port->pc);
		port->pc = NULL;
	}

        free (port);

        return GP_OK;
}

/**
 * gp_port_write:
 * @port: a #GPPort
 * @data: the data to write to the port
 * @size: the number of bytes to write to the port
 *
 * Writes a specified amount of @data to a @port.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_write (GPPort *port, const char *data, int size)
{
	int retval;

	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Writing %i=0x%x byte(s) "
		"to port...", size, size);

	CHECK_NULL (port && data);
	CHECK_INIT (port);

	gp_log_data ("gphoto2-port", data, size);

	/* Check if we wrote all bytes */
	CHECK_SUPP (port, _("write"), port->pc->ops->write);
	retval = port->pc->ops->write (port, data, size);
	CHECK_RESULT (retval);
	if ((port->type != GP_PORT_SERIAL) && (retval != size))
		gp_log (GP_LOG_DEBUG, "gphoto2-port", "Could only write %i "
			"out of %i byte(s)", retval, size);

	return (retval);
}

/**
 * gp_port_read:
 * @port: a #GPPort
 * @data: a pointer to an allocated buffer
 * @size: the number of bytes that should be read
 *
 * Reads a specified number of bytes from the port into the supplied buffer.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_read (GPPort *port, char *data, int size)
{
        int retval;

	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Reading %i=0x%x bytes from port...",
		size, size);

	CHECK_NULL (port);
	CHECK_INIT (port);

	/* Check if we read as many bytes as expected */
	CHECK_SUPP (port, _("read"), port->pc->ops->read);
	retval = port->pc->ops->read (port, data, size);
	CHECK_RESULT (retval);
	if (retval != size)
		gp_log (GP_LOG_DEBUG, "gphoto2-port", "Could only read %i "
			"out of %i byte(s)", retval, size);

	gp_log_data ("gphoto2-port", data, retval);

	return (retval);
}

/**
 * gp_port_check_int:
 * @port: a #GPPort
 * @data: a pointer to an allocated buffer
 * @size: the number of bytes that should be read
 *
 * Reads a specified number of bytes from the inerrupt endpoint
 * into the supplied buffer.
 * Function waits port->timeout miliseconds for data on interrupt endpoint.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_check_int (GPPort *port, char *data, int size)
{
        int retval;

	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Reading %i=0x%x bytes from inerrupt ep...",
		size, size);

	CHECK_NULL (port);
	CHECK_INIT (port);

	/* Check if we read as many bytes as expected */
	CHECK_SUPP (port, _("check_int"), port->pc->ops->check_int);
	retval = port->pc->ops->check_int (port, data, size, port->timeout);
	CHECK_RESULT (retval);
	if (retval != size)
		gp_log (GP_LOG_DEBUG, "gphoto2-port", "Could only read %i "
			"out of %i byte(s)", retval, size);

	gp_log_data ("gphoto2-port", data, retval);

	return (retval);
}

/**
 * gp_port_check_int_fast:
 * @port: a #GPPort
 * @data: a pointer to an allocated buffer
 * @size: the number of bytes that should be read
 *
 * Reads a specified number of bytes from the inerrupt endpoint
 * into the supplied buffer.
 * Function waits 50 miliseconds for data on interrupt endpoint.
 *
 * Return value: a gphoto2 error code
 **/
#define FAST_TIMEOUT	50
int
gp_port_check_int_fast (GPPort *port, char *data, int size)
{
        int retval;

	CHECK_NULL (port);
	CHECK_INIT (port);

	/* Check if we read as many bytes as expected */
	CHECK_SUPP (port, _("check_int"), port->pc->ops->check_int);
	retval = port->pc->ops->check_int (port, data, size, FAST_TIMEOUT);
	CHECK_RESULT (retval);

#ifdef IGNORE_EMPTY_INTR_READS
	if (retval != size && retval != 0 )
#else
	if (retval != size )
#endif
		gp_log (GP_LOG_DEBUG, "gphoto2-port", "Could only read %i "
			"out of %i byte(s)", retval, size);

#ifdef IGNORE_EMPTY_INTR_READS
	if ( retval != 0 ) {
#endif
		/* For Canon cameras, we will make lots of
		   reads that will return zero length. Don't
		   bother to log them as errors. */
		gp_log (GP_LOG_DEBUG, "gphoto2-port",
			"Reading %i=0x%x bytes from interrupt ep (fast)...",
			size, size);
		gp_log_data ("gphoto2-port", data, retval);
#ifdef IGNORE_EMPTY_INTR_READS
	}
#endif

	return (retval);
}


/**
 * gp_port_set_timeout:
 * @port: a #GPPort
 * @timeout: the timeout
 *
 * Sets the @timeout of a @port. #gp_port_read will wait @timeout milliseconds
 * for data. If no data will be received in that period, %GP_ERROR_TIMEOUT
 * will be returned.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_set_timeout (GPPort *port, int timeout)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Setting timeout to %i "
		"millisecond(s)...", timeout);

	CHECK_NULL (port);

        port->timeout = timeout;

        return GP_OK;
}

/* Deprecated */
int gp_port_timeout_set (GPPort *, int);
int gp_port_timeout_set (GPPort *port, int timeout)
{
	return (gp_port_set_timeout (port, timeout));
}

/* Deprecated */
int gp_port_timeout_get (GPPort *, int *);
int gp_port_timeout_get (GPPort *port, int *timeout)
{
	return (gp_port_get_timeout (port, timeout));
}

/**
 * gp_port_get_timeout:
 * @port: a #GPPort
 * @timeout:
 *
 * Retreives the current timeout of the port.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_get_timeout (GPPort *port, int *timeout)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Getting timeout...");

	CHECK_NULL (port);

	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Current timeout: %i "
		"milliseconds", port->timeout);

        *timeout = port->timeout;

        return GP_OK;
}

/**
 * gp_port_set_settings:
 * @port: a #GPPort
 * @settings: the #GPPortSettings to be set
 *
 * Adjusts the settings of a port. You should always call
 * #gp_port_get_settings, adjust the values depending on the type of the port,
 * and then call #gp_port_set_settings.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_set_settings (GPPort *port, GPPortSettings settings)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Setting settings...");

	CHECK_NULL (port);
	CHECK_INIT (port);

        /*
	 * We copy the settings to settings_pending and call update on the 
	 * port.
	 */
        memcpy (&port->settings_pending, &settings,
		sizeof (port->settings_pending));
	CHECK_SUPP (port, _("update"), port->pc->ops->update);
        CHECK_RESULT (port->pc->ops->update (port));

        return (GP_OK);
}

/* Deprecated */
int gp_port_settings_get (GPPort *, GPPortSettings *);
int gp_port_settings_get (GPPort *port, GPPortSettings *settings)
{
	return (gp_port_get_settings (port, settings));
}
int gp_port_settings_set (GPPort *, GPPortSettings);
int gp_port_settings_set (GPPort *port, GPPortSettings settings)
{
	return (gp_port_set_settings (port, settings));
}

/**
 * gp_port_get_settings:
 * @port: a #GPPort
 * @settings:
 *
 * Retreives the current @settings of a @port.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_get_settings (GPPort *port, GPPortSettings *settings)
{
	CHECK_NULL (port);

        memcpy (settings, &(port->settings), sizeof (gp_port_settings));

        return GP_OK;
}

int
gp_port_get_pin (GPPort *port, GPPin pin, GPLevel *level)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Getting level of pin %i...",
		pin);

	CHECK_NULL (port && level);
	CHECK_INIT (port);

	CHECK_SUPP (port, _("get_pin"), port->pc->ops->get_pin);
        CHECK_RESULT (port->pc->ops->get_pin (port, pin, level));

	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Level of pin %i: %i",
		pin, *level);

	return (GP_OK);
}

static struct {
	GPPin pin;
	unsigned char number;
	const char *description_short;
	const char *description_long;
} PinTable[] = {
	{GP_PIN_RTS , 7, "RTS" , "Request To Send"    },
	{GP_PIN_DTR , 4, "DTR" , "Data Terminal Ready"},
	{GP_PIN_CTS , 8, "CTS" , "Clear To Send"      },
	{GP_PIN_DSR , 6, "DSR" , "Data Set Ready"     },
	{GP_PIN_CD  , 1, "CD"  , "Carrier Detect"     },
	{GP_PIN_RING, 9, "RING", "Ring Indicator"     },
	{0, 0, NULL, NULL}
};

static struct {
	GPLevel level;
	const char *description;
} LevelTable[] = {
	{GP_LEVEL_LOW, "low"},
	{GP_LEVEL_HIGH, "high"},
	{0, NULL}
};

int
gp_port_set_pin (GPPort *port, GPPin pin, GPLevel level)
{
	unsigned int i, j;

	for (i = 0; PinTable[i].description_short; i++)
		if (PinTable[i].pin == pin)
			break;
	for (j = 0; LevelTable[j].description; j++)
		if (LevelTable[j].level == level)
			break;
	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Setting pin %i "
		"(%s: '%s') to '%s'...", 
		PinTable[i].number, PinTable[i].description_short,
		PinTable[i].description_long, LevelTable[j].description);

	CHECK_NULL (port);
	CHECK_INIT (port);

	CHECK_SUPP (port, _("set_pin"), port->pc->ops->set_pin);
	CHECK_RESULT (port->pc->ops->set_pin (port, pin, level));

	return (GP_OK);
}

int
gp_port_send_break (GPPort *port, int duration)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Sending break (%i "
		"milliseconds)...", duration);

	CHECK_NULL (port);
	CHECK_INIT (port);

        CHECK_SUPP (port, _("send_break"), port->pc->ops->send_break);
        CHECK_RESULT (port->pc->ops->send_break (port, duration));

	return (GP_OK);
}

int
gp_port_flush (GPPort *port, int direction)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Flushing port...");

	CHECK_NULL (port);

	CHECK_SUPP (port, _("flush"), port->pc->ops->flush);
	CHECK_RESULT (port->pc->ops->flush (port, direction));

        return (GP_OK);
}


/* USB-specific functions */
/* ------------------------------------------------------------------ */

int
gp_port_usb_find_device (GPPort *port, int idvendor, int idproduct)
{
	CHECK_NULL (port);
	CHECK_INIT (port);

	CHECK_SUPP (port, _("find_device"), port->pc->ops->find_device);
	CHECK_RESULT (port->pc->ops->find_device (port, idvendor, idproduct));

        return (GP_OK);
}

int
gp_port_usb_find_device_by_class (GPPort *port, int mainclass, int subclass, int protocol)
{
	CHECK_NULL (port);
	CHECK_INIT (port);

	CHECK_SUPP (port, _("find_device_by_class"), port->pc->ops->find_device_by_class);
	CHECK_RESULT (port->pc->ops->find_device_by_class (port, mainclass, subclass, protocol));

        return (GP_OK);
}

int
gp_port_usb_clear_halt (GPPort *port, int ep)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Clear halt...");

	CHECK_NULL (port);
	CHECK_INIT (port);

	CHECK_SUPP (port, _("clear_halt"), port->pc->ops->clear_halt);
        CHECK_RESULT (port->pc->ops->clear_halt (port, ep));

        return (GP_OK);
}

int
gp_port_usb_msg_write (GPPort *port, int request, int value, int index,
	char *bytes, int size)
{
        int retval;

	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Writing message "
		"(request=0x%x value=0x%x index=0x%x size=%i=0x%x)...",
		request, value, index, size, size);
	gp_log_data ("gphoto2-port", bytes, size);

	CHECK_NULL (port);
	CHECK_INIT (port);

	CHECK_SUPP (port, _("msg_write"), port->pc->ops->msg_write);
        retval = port->pc->ops->msg_write(port, request, value, index, bytes, size);
	CHECK_RESULT (retval);

        return (retval);
}

int
gp_port_usb_msg_read (GPPort *port, int request, int value, int index,
	char *bytes, int size)
{
        int retval;

	gp_log (GP_LOG_DEBUG, "gphoto2-port", "Reading message "
		"(request=0x%x value=0x%x index=0x%x size=%i=0x%x)...",
		request, value, index, size, size);

	CHECK_NULL (port);
	CHECK_INIT (port);

	CHECK_SUPP (port, _("msg_read"), port->pc->ops->msg_read);
        retval = port->pc->ops->msg_read (port, request, value, index, bytes, size);
	CHECK_RESULT (retval);

	if (retval != size)
		gp_log (GP_LOG_DEBUG, "gphoto2-port", "Could only read %i "
			"out of %i byte(s)", retval, size);

	gp_log_data ("gphoto2-port", bytes, retval);

        return (retval);
}

/**
 * gp_port_set_error:
 * @port: a #GPPort
 * @format:
 * @...:
 *
 * Sets an error message that can later be retrieved using #gp_port_get_error.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_set_error (GPPort *port, const char *format, ...)
{
	va_list args;

	if (!port)
		return (GP_ERROR_BAD_PARAMETERS);

	if (format) {
		va_start (args, format);
		vsnprintf (port->pc->error, sizeof (port->pc->error),
			   _(format), args);
		gp_logv (GP_LOG_ERROR, "gphoto2-port", format, args);
		va_end (args);
	} else
		port->pc->error[0] = '\0';

	return (GP_OK);
}

/**
 * gp_port_get_error:
 * @port: a #GPPort
 *
 * Retrieves an error message from a @port. If you want to make sure that
 * you get correct error messages, you need to call #gp_port_set_error with
 * an error message of %NULL each time before calling another port-related
 * function of which you want to check the return value.
 *
 * Return value: an error message
 **/
const char *
gp_port_get_error (GPPort *port)
{
	if (port && port->pc && strlen (port->pc->error))
		return (port->pc->error);

	return (_("No error description available"));
}
