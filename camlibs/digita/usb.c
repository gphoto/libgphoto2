/*
 * usb.c
 *
 *  USB digita support
 *
 * Copyright 1999-2000, Johannes Erdfelt <jerdfelt@valinux.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "digita.h"

#include <gphoto2-port.h>

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

	digita_send = digita_usb_send;
	digita_read = digita_usb_read;

	gp_port_set_settings(dev->gpdev, settings);

	return GP_OK;
}

