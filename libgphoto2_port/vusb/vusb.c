/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vusb.c
 *
 * Copyright (c) 2001 Lutz Mueller <lutz@users.sf.net>
 * Copyright (c) 1999-2000 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright (c) 2005, 2007 Hubert Figuiere <hub@figuiere.net>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <gphoto2/gphoto2-port-library.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <string.h>

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-log.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CHECK(result) {int r=(result); if (r<0) return (r);}

struct _GPPortPrivateLibrary {
};

GPPortType
gp_port_library_type (void)
{
        return GP_PORT_USB;
}

int
gp_port_library_list (GPPortInfoList *list)
{
	GPPortInfo info;

	gp_port_info_new (&info);
	gp_port_info_set_type (info, GP_PORT_USB);
	gp_port_info_set_name (info, "usb:001,001");
			
	CHECK (gp_port_info_list_append (list, info));

	/* generic usb:/xxx/ matcher */
	gp_port_info_new (&info);
	gp_port_info_set_type (info, GP_PORT_USB);
	gp_port_info_set_name (info, "");
	gp_port_info_set_path (info, "^usb:");
	CHECK (gp_port_info_list_append (list, info));
	return GP_OK;
}

static int gp_port_vusb_init (GPPort *dev)
{
	C_MEM (dev->pl = calloc (1, sizeof (GPPortPrivateLibrary)));

	return GP_OK;
}

static int
gp_port_vusb_exit (GPPort *port)
{
	free (port->pl);
	port->pl = NULL;

	return GP_OK;
}

static int
gp_port_vusb_open (GPPort *port)
{
	return GP_OK;
}

static int
gp_port_vusb_close (GPPort *port)
{
	return GP_OK;
}

static int
gp_port_vusb_write (GPPort *port, const char *bytes, int size)
{
        return GP_OK;
}

static int
gp_port_vusb_read(GPPort *port, char *bytes, int size)
{
        return GP_OK;
}

static int
gp_port_vusb_reset(GPPort *port)
{
        C_PARAMS (port && port->pl);

        return GP_OK;
}

static int
gp_port_vusb_check_int (GPPort *port, char *bytes, int size, int timeout)
{
        C_PARAMS (port && port->pl && timeout >= 0);

        return GP_ERROR_TIMEOUT;
}

static int
gp_port_vusb_update (GPPort *port)
{
	return GP_OK;
}

static int
gp_port_vusb_clear_halt_lib(GPPort *port, int ep)
{
        unsigned char internal_ep;

        C_PARAMS (port && port->pl);

        switch (ep) {
        case GP_PORT_USB_ENDPOINT_IN :
                internal_ep = port->settings.usb.inep;
                break;
        case GP_PORT_USB_ENDPOINT_OUT :
                internal_ep = port->settings.usb.outep;
                break;
        case GP_PORT_USB_ENDPOINT_INT :
                internal_ep = port->settings.usb.intep;
                break;
        default:
                gp_port_set_error (port, "Bad EndPoint argument 0x%x", ep);
                return GP_ERROR_BAD_PARAMETERS;
        }
	gp_log(GP_LOG_DEBUG,"gp_port_vusb_clear_halt_lib","clearing halt on ep 0x%x", internal_ep);
	/* now clear halt */
        return GP_OK;
}

/* The next two functions support the nonstandard request types 0x41 (write) 
 * and 0xc1 (read), which are occasionally needed. 
 */
static int
gp_port_vusb_msg_interface_write_lib(GPPort *port, int request, 
        int value, int index, char *bytes, int size)
{
        return GP_OK;
}


static int
gp_port_vusb_msg_interface_read_lib(GPPort *port, int request, 
        int value, int index, char *bytes, int size)
{
        return GP_OK;	/* or bytes */
}


/* The next two functions support the nonstandard request types 0x21 (write) 
 * and 0xa1 (read), which are occasionally needed. 
 */
static int
gp_port_vusb_msg_class_write_lib(GPPort *port, int request, 
        int value, int index, char *bytes, int size)
{
	return GP_OK;
}



static int
gp_port_vusb_msg_class_read_lib(GPPort *port, int request, 
        int value, int index, char *bytes, int size)
{
        return GP_OK;	/* or bytes */
}


static int
gp_port_vusb_msg_write_lib(GPPort *port, int request, int value, int index,
        char *bytes, int size)
{
	return GP_OK;
}

static int
gp_port_vusb_msg_read_lib(GPPort *port, int request, int value, int index,
        char *bytes, int size)
{
	return GP_OK; /* bytes */
}

static int
gp_port_vusb_find_device_lib(GPPort *port, int idvendor, int idproduct)
{
        return GP_ERROR_IO_USB_FIND;
}

static int
gp_port_vusb_find_device_by_class_lib(GPPort *port, int class, int subclass, int protocol)
{
        return GP_ERROR_IO_USB_FIND;
}



GPPortOperations *
gp_port_library_operations (void)
{
	GPPortOperations *ops;

	ops = calloc (1, sizeof (GPPortOperations));
	if (!ops)
		return NULL;

	ops->init	= gp_port_vusb_init;
	ops->exit	= gp_port_vusb_exit;
	ops->open	= gp_port_vusb_open;
	ops->close	= gp_port_vusb_close;
	ops->read	= gp_port_vusb_read;
	ops->write	= gp_port_vusb_write;
	ops->reset	= gp_port_vusb_reset;

        ops->check_int 	= gp_port_vusb_check_int;
        ops->update 	= gp_port_vusb_update;
        ops->clear_halt = gp_port_vusb_clear_halt_lib;
        ops->msg_write  = gp_port_vusb_msg_write_lib;
        ops->msg_read   = gp_port_vusb_msg_read_lib;

        ops->msg_interface_write	= gp_port_vusb_msg_interface_write_lib;
        ops->msg_interface_read		= gp_port_vusb_msg_interface_read_lib;
        ops->msg_class_write  		= gp_port_vusb_msg_class_write_lib;
        ops->msg_class_read   		= gp_port_vusb_msg_class_read_lib;
        ops->find_device 		= gp_port_vusb_find_device_lib;
        ops->find_device_by_class	= gp_port_vusb_find_device_by_class_lib;
	return ops;
}
