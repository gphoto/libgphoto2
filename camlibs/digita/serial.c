/*
 * serial.c
 *
 *  Serial digita support
 *
 * Copyright 1999-2000 Johannes Erdfelt
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <netinet/in.h>

#include "digita.h"

#include <gphoto2-port.h>
int gp_port_serial_set_baudrate(gp_port * dev);

struct beacon {
	unsigned short intro;
	unsigned short vendorid;
	unsigned short deviceid;
	unsigned char checksum;
} __attribute__ ((packed));

struct beacon_ack {
	unsigned short intro;
	unsigned char intftype;
	unsigned int cf_reserved:4;
	unsigned int cf_pod_receive_mode:2;
	unsigned int cf_host_receive_mode:2;
	unsigned int dataspeed;
	unsigned short deviceframesize;
	unsigned short hostframesize;
	unsigned char checksum;
} __attribute__ ((packed));

struct beacon_comp {
	unsigned char result;
	unsigned int cf_reserved:4;
	unsigned int cf_pod_receive_mode:2;
	unsigned int cf_host_receive_mode:2;
	unsigned int dataspeed;
	unsigned short deviceframesize;
	unsigned short hostframesize;
} __attribute__ ((packed));

#define POLL_LENGTH_MASK	0x03FF
#define POLL_BOB		0x0400
#define POLL_EOB		0x0800
#define POLL_CMD		0x1000
#define POLL_POLL_MASK		0xE000
#define POLL_POLL_SHIFT		13

#define POLL_ACK		0x01
#define POLL_NAK		0x02

static unsigned int checksum(const unsigned char *buffer, int len)
{
	int	i;
	int	limit = len - 1;
	unsigned int sum = 0;

	for (i = 0; i < limit; i++)
		sum += *(buffer++);

	return sum & 0xff;
}

static int poll_and_wait(gp_port *dev, int length, int bob, int eob)
{
        unsigned short s, poll, poll_reply;

        do {
                poll = (1 << POLL_POLL_SHIFT) | POLL_CMD | length |
                        (bob ? POLL_BOB : 0) | (eob ? POLL_EOB : 0);

                s = htons(poll);
                if (gp_port_write(dev, (void *)&s, sizeof(s)) < 0)
                        return -1;
                if (gp_port_read(dev, (void *)&s, sizeof(s)) < 0)
                        return -1;
                poll_reply = ntohs(s);
if (poll_reply & POLL_ACK)
printf("POLL_ACK\n");
if (poll_reply & POLL_NAK)
printf("POLL_NAK\n");
        } while (poll_reply & POLL_NAK);

        return 0;
}

static int digita_serial_send(struct digita_device *dev, void *_buffer, int len)
{
        unsigned char *buffer = _buffer;
        unsigned short s;
        int sent = 0, size;

        while (sent < len) {
                if ((len - sent) > dev->deviceframesize)
                        size = dev->deviceframesize;
                else
                        size = len - sent;

                poll_and_wait(dev->gpdev, size, sent == 0, (size + sent) == len);

                if (gp_port_write(dev->gpdev, buffer + sent, size) < 0)
                        return -1;

                sent += size;
        }

        s = 0;
        if (gp_port_write(dev->gpdev, (void *)&s, sizeof(s)) < 0)
                return -1;

        return len;
}

static int poll_and_reply(gp_port *dev, int *length, int *eob, int nak)
{
        unsigned short s, poll, poll_reply;

        if (gp_port_read(dev, (void *)&s, sizeof(s)) < 0)
                return -1;

        poll = ntohs(s);
        if (length)
                *length = poll & POLL_LENGTH_MASK;
        if (eob)
                *eob = poll & POLL_EOB;

        if (nak)
                poll_reply = POLL_NAK;
        else
                poll_reply = POLL_ACK;

        s = htons(poll_reply);
        if (gp_port_write(dev, (void *)&s, sizeof(s)) < 0)
                return -1;

        return 0;
}

static int digita_serial_read(struct digita_device *dev, void *_buffer, int len)
{
        unsigned char *buffer = _buffer;
        unsigned short s;
        int received = 0, size, eob;

        while (received < len) {
                poll_and_reply(dev->gpdev, &size, &eob, 0);

                if (gp_port_read(dev->gpdev, buffer + received, size) < 0)
                        return -1;

                received += size;
                if (eob)
                        break;
        }

        if (gp_port_read(dev->gpdev, (void *)&s, sizeof(s)) < 0)
                return -1;

        return received;
}

int digita_serial_open(struct digita_device *dev, Camera *camera)
{
	gp_port_settings settings;
	struct beacon beacon;
	struct beacon_ack beacon_ack;
	struct beacon_comp beacon_comp;

	dev->gpdev = gp_port_new(GP_PORT_SERIAL);
	if (!dev->gpdev)
		return -1;

	strcpy(settings.serial.port,camera->port->path);

	settings.serial.speed = 9600;
	settings.serial.bits = 8;
	settings.serial.parity = 0;
	settings.serial.stopbits = 1;

	digita_send = digita_serial_send;
	digita_read = digita_serial_read;

	gp_port_set_settings(dev->gpdev, settings);
	if (gp_port_open(dev->gpdev) < 0) {
		fprintf(stderr, "error opening device\n");
		return 0;
	}

	tcsendbreak(dev->gpdev->device_fd, 4);

	dev->gpdev->settings.serial.speed = 0;
	gp_port_serial_set_baudrate(dev->gpdev);

	usleep(50);

	dev->gpdev->settings.serial.speed = camera->port->speed;
	gp_port_serial_set_baudrate(dev->gpdev);

	usleep(2000);

	memset((void *)&beacon, 0, sizeof(beacon));
	if (gp_port_read(dev->gpdev, (void *)&beacon, sizeof(beacon)) < 0) {
		perror("reading beacon");
		return 0;
	}

printf("%04X %04X %04X %02X\n",
	beacon.intro, beacon.vendorid,
	beacon.deviceid, beacon.checksum);

	beacon_ack.intro = htons(0x5aa5);
	beacon_ack.intftype = 0x55;
	beacon_ack.cf_reserved = 0;
	beacon_ack.cf_pod_receive_mode = 0;
	beacon_ack.cf_host_receive_mode = 0;
	beacon_ack.dataspeed = htonl(camera->port->speed);
	beacon_ack.deviceframesize = htons(1023);
	beacon_ack.hostframesize = htons(1023);
	beacon_ack.checksum = 0;
	beacon_ack.checksum = checksum((void *)&beacon_ack, sizeof(beacon_ack));

	if (gp_port_write(dev->gpdev, (void *)&beacon_ack, sizeof(beacon_ack)) < 0) {
		perror("writing beacon_ack");
		return -1;
	}

	if (gp_port_read(dev->gpdev, (void *)&beacon_comp, sizeof(beacon_comp)) < 0) {
		perror("reading beacon_comp");
		return -1;
	}

printf("%d\n", ntohl(beacon_comp.dataspeed));
	usleep(100000);

	dev->deviceframesize = ntohs(beacon_comp.deviceframesize);

	dev->gpdev->settings.serial.speed = ntohl(beacon_comp.dataspeed);
	gp_port_serial_set_baudrate(dev->gpdev);

usleep(100000);

	return 0;
}

