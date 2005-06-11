/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gphoto2-port-usb.c
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
 * Copyright © 1999-2000 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright (c) 2005 Hubert Figuiere <hub@figuiere.net>
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
#include <gphoto2-port-library.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include <dirent.h>
#include <string.h>

#include <gphoto2-port.h>
#include <gphoto2-port-result.h>
#include <gphoto2-port-log.h>

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
	char *mount_point;
};

GPPortType
gp_port_library_type (void)
{
        return (GP_PORT_DISK);
}

int
gp_port_library_list (GPPortInfoList *list)
{
	GPPortInfo info;

	/* default port first */
	info.type = GP_PORT_DISK;
	strcpy (info.name, "Mounted Disk");
        strcpy (info.path, "disk:");
	CHECK (gp_port_info_list_append (list, info));

	return GP_OK;
}

static int gp_port_disk_init (GPPort *port)
{
	return GP_OK;
}

static int
gp_port_disk_exit (GPPort *port)
{
	if (port->pl) {
		free (port->pl);
		port->pl = NULL;
	}

	return GP_OK;
}

static int
gp_port_disk_open (GPPort *port)
{
	return GP_OK;
}

static int
gp_port_disk_close (GPPort *port)
{
	return GP_OK;
}

static int
gp_port_disk_write (GPPort *port, const char *bytes, int size)
{
        return GP_OK;
}

static int
gp_port_disk_read(GPPort *port, char *bytes, int size)
{
        return GP_OK;
}

GPPortOperations *
gp_port_library_operations (void)
{
	GPPortOperations *ops;

	ops = malloc (sizeof (GPPortOperations));
	if (!ops) {
		return NULL;
	}
	memset (ops, 0, sizeof (GPPortOperations));

	ops->init   = gp_port_disk_init;
	ops->exit   = gp_port_disk_exit;
	ops->open   = gp_port_disk_open;
	ops->close  = gp_port_disk_close;
	ops->read   = gp_port_disk_read;
	ops->write  = gp_port_disk_write;

	return ops;
}
