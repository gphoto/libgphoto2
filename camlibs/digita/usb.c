/*
 * usb.c
 *
 *  USB digita support
 *
 * Copyright 1999-2001 Johannes Erdfelt
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

