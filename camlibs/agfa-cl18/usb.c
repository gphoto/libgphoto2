/*
 * agfa.c
 *
 *  USB agfa CL18 support
 *
 * Copyright 2001, Vince Weaver <vince@deater.net>
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "agfa.h"

#include <gphoto2-port.h>

static int agfa_usb_read(struct agfa_device *dev, void *buffer, int len)
{
	return gp_port_read(dev->gpdev, buffer, len);
}

static int agfa_usb_send(struct agfa_device *dev, void *buffer, int len)
{
	return gp_port_write(dev->gpdev, buffer, len);
}

struct camera_to_usb {
	char *name;
	unsigned short idVendor;
	unsigned short idProduct;
} camera_to_usb[] = {
	{ "Agfa CL18", 0x06BD, 0x0403 },
};

static int agfa_usb_probe(struct agfa_device *dev, int i)
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

int agfa_usb_open(struct agfa_device *dev, Camera *camera)
{
	gp_port_settings settings;
	int i, ret;

	fprintf(stderr, "agfa: user selected %s\n", camera->model);

        if ((ret = gp_port_new(&(dev->gpdev), GP_PORT_USB)) < 0)
            return (ret);

	for (i = 0; i < sizeof(camera_to_usb) / sizeof(struct camera_to_usb); i++) {
		fprintf(stderr, "agfa: %s, %s\n", camera->model, camera_to_usb[i].name);

		if (!strcmp(camera->model, camera_to_usb[i].name))
			break;
	}

	if (agfa_usb_probe(dev, i) < 0)
		return -1;
/* ??? */
	settings.usb.inep = 0x83;
	settings.usb.outep = 0x02;
	settings.usb.config = 0;
	settings.usb.interface = 0;
	settings.usb.altsetting = 0;

	agfa_send = agfa_usb_send;
	agfa_read = agfa_usb_read;

	gp_port_settings_set(dev->gpdev, settings);
	if (gp_port_open(dev->gpdev) < 0) {
		fprintf(stderr, "error opening device\n");
		return -1;
	}

	return 0;
}

