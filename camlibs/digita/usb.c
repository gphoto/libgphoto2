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

static int digita_usb_read(struct digita_device *dev, void *buffer, int len)
{
	return gp_port_read(dev->gpdev, buffer, len);
}

static int digita_usb_send(struct digita_device *dev, void *buffer, int len)
{
	return gp_port_write(dev->gpdev, buffer, len);
}

int digita_usb_open(struct digita_device *dev, Camera *camera)
{
	gp_port_settings settings;

	fprintf(stderr, "digita: user selected %s\n", camera->model);

	settings.usb.inep = 0x81;
	settings.usb.outep = 0x02;
	settings.usb.config = 1;
	settings.usb.interface = 0;
	settings.usb.altsetting = 0;

	digita_send = digita_usb_send;
	digita_read = digita_usb_read;

	gp_port_settings_set(dev->gpdev, settings);

	return GP_OK;
}

