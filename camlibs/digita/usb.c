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

struct camera_to_usb {
	char *name;
	unsigned short idVendor;
	unsigned short idProduct;
} camera_to_usb[] = {
	{ "Kodak DC220", 0x040A, 0x0100 },
	{ "Kodak DC260", 0x040A, 0x0110 },
	{ "Kodak DC265", 0x040A, 0x0111 },
	{ "Kodak DC290", 0x040A, 0x0112 },
};

int digita_usb_probe(struct digita_device *dev, int i)
{
	if (i >= sizeof(camera_to_usb) / sizeof(struct camera_to_usb))
		goto err;

	if (gp_port_usb_find_device(dev->gpdev, camera_to_usb[i].idVendor,
				camera_to_usb[i].idProduct) == GP_OK) {
		printf("found '%s'\n", camera_to_usb[i].name);
		return 0;
	}

err:
	fprintf(stderr, "unable to find any compatible USB cameras\n");

	return -1;
}

int digita_usb_open(struct digita_device *dev, Camera *camera)
{
	gp_port_settings settings;
	int i, ret;

	fprintf(stderr, "digita: user selected %s\n", camera->model);

        if ((ret = gp_port_new(&(dev->gpdev), GP_PORT_USB)) < 0)
            return (ret);

	for (i = 0; i < sizeof(camera_to_usb) / sizeof(struct camera_to_usb); i++) {
		fprintf(stderr, "digita: %s, %s\n", camera->model, camera_to_usb[i].name);

		if (!strcmp(camera->model, camera_to_usb[i].name))
			break;
	}

	if (digita_usb_probe(dev, i) < 0)
		return -1;

	settings.usb.inep = 0x81;
	settings.usb.outep = 0x02;
	settings.usb.config = 1;
	settings.usb.interface = 0;
	settings.usb.altsetting = 0;

	digita_send = digita_usb_send;
	digita_read = digita_usb_read;

	gp_port_settings_set(dev->gpdev, settings);
	if (gp_port_open(dev->gpdev) < 0) {
		fprintf(stderr, "error opening device\n");
		return -1;
	}

	return 0;
}

