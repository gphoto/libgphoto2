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

#include "digita.h"

#include <gpio/gpio.h>

static int digita_usb_read(struct digita_device *dev, void *buffer, int len)
{
#ifdef GPIO_USB
	return gpio_read(dev->gpdev, buffer, len);
#else
	return GPIO_ERROR;
#endif
}

static int digita_usb_send(struct digita_device *dev, void *buffer, int len)
{
#ifdef GPIO_USB
	return gpio_write(dev->gpdev, buffer, len);
#else
	return GPIO_ERROR;
#endif
}

#ifdef GPIO_USB
int digita_usb_probe(struct usb_device **udev)
{
	if (gpio_usb_find_device(0x040A, 0x0110, udev)) {
		printf("found '%s' @ %s/%s\n", "Kodak DC260",
			(*udev)->bus->dirname, (*udev)->filename);
		return 1;
	}

	fprintf(stderr, "unable to find any compatible USB cameras\n");

	return 0;
}
#endif

struct digita_device *digita_usb_open(void)
{
#ifdef GPIO_USB
	struct digita_device *dev;
	gpio_device_settings settings;
	struct usb_device *udev;

	if (!digita_usb_probe(&udev))
		return NULL;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return NULL;

	dev->gpdev = gpio_new(GPIO_DEVICE_USB);
	if (!dev->gpdev)
		return NULL;

	settings.usb.udev = udev;

	settings.usb.inep = 0x81;
	settings.usb.outep = 0x02;
	settings.usb.config = 1;
	settings.usb.interface = 0;
	settings.usb.altsetting = 0;

	digita_send = digita_usb_send;
	digita_read = digita_usb_read;

	gpio_set_settings(dev->gpdev, settings);
	if (gpio_open(dev->gpdev) < 0) {
		fprintf(stderr, "error opening device\n");
		return NULL;
	}

	return dev;
#else
	return NULL;
#endif
}

