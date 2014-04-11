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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <gphoto2/gphoto2-port.h>

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
	char buffer[128];
	int ret;

	ret = gp_port_get_settings(camera->port, &settings);
	if (ret < 0)
		return ret;

	/* We'll take the defaults. The core should have the done what's */
	/* necessary to find the config, interface, altsetting and endpoints */

	ret = gp_port_set_settings(dev->gpdev, settings);
	if (ret < 0)
		return ret;

	dev->send = digita_usb_send;
	dev->read = digita_usb_read;

	gp_port_set_timeout(camera->port, 100);

	/* Mop up anything still pending */
	while (gp_port_read(dev->gpdev, buffer, sizeof(buffer)) > 0)
		;

	gp_port_set_timeout(camera->port, 10000);

	return GP_OK;
}

