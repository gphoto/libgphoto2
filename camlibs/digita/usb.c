/*
 * usb.c
 *
 *  USB digita support
 *
 * Copyright 1999-2001 Johannes Erdfelt
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <gphoto2-port.h>

#include "digita.h"

static int digita_usb_read(CameraPrivateLibrary *dev, void *buffer, int len)
{
	return gp_port_read(dev->gpdev, buffer, len);
}

static int digita_usb_send(CameraPrivateLibrary *dev, void *buffer, int len)
{
	return gp_port_write(dev->gpdev, buffer, len);
}

int digita_usb_open(CameraPrivateLibrary *dev, Camera *camera)
{
	GPPortSettings settings;

	settings.usb.inep = 0x81;
	settings.usb.outep = 0x02;
	settings.usb.config = 1;
	settings.usb.interface = 0;
	settings.usb.altsetting = 0;

	dev->send = digita_usb_send;
	dev->read = digita_usb_read;

	gp_port_set_settings(dev->gpdev, settings);

	return GP_OK;
}

