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

#include "digita.h"

#include <gpio.h>

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

#define POLL_LENGTH_MASK        0x03FF
#define POLL_BOB                0x0400
#define POLL_EOB                0x0800
#define POLL_CMD                0x1000
#define POLL_POLL_MASK          0xE000
#define POLL_POLL_SHIFT         13

#define POLL_ACK        0x01
#define POLL_NAK        0x02

static unsigned int checksum(const unsigned char *buffer, int len)
{
        int     i;
        int     limit = len - 1;
        unsigned int sum = 0;

        for (i = 0; i < limit; i++)
                sum += *(buffer++);

        return sum & 0xff;
}

static int poll_and_wait(gpio_device *dev, int length, int bob, int eob)
{
        unsigned short s, poll, poll_reply;

        do {
                poll = (1 << POLL_POLL_SHIFT) | POLL_CMD | length |
                        (bob ? POLL_BOB : 0) | (eob ? POLL_EOB : 0);

                s = htons(poll);
                if (gpio_write(dev, (void *)&s, sizeof(s)) < 0)
                        return -1;
                if (gpio_read(dev, (void *)&s, sizeof(s)) < 0)
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

                if (gpio_write(dev->gpdev, buffer + sent, size) < 0)
                        return -1;

                sent += size;
        }

        s = 0;
        if (gpio_write(dev->gpdev, (void *)&s, sizeof(s)) < 0)
                return -1;

        return len;
}

static int poll_and_reply(gpio_device *dev, int *length, int *eob, int nak)
{
        unsigned short s, poll, poll_reply;

        if (gpio_read(dev, (void *)&s, sizeof(s)) < 0)
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
        if (gpio_write(dev, (void *)&s, sizeof(s)) < 0)
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

                if (gpio_read(dev->gpdev, buffer + received, size) < 0)
                        return -1;

                received += size;
                if (eob)
                        break;
        }

        if (gpio_read(dev->gpdev, (void *)&s, sizeof(s)) < 0)
                return -1;

        return received;
}

struct digita_device *digita_serial_open(Camera *camera, CameraInit *init)
{
        struct digita_device *dev;
        gpio_device_settings settings;
        struct beacon beacon;
        struct beacon_ack beacon_ack;
        struct beacon_comp beacon_comp;

        dev = malloc(sizeof(*dev));
        if (!dev)
                return NULL;

        dev->gpdev = gpio_new(GPIO_DEVICE_SERIAL);
        if (!dev->gpdev)
                return NULL;


        strcpy(settings.serial.port,init->port_settings.path);

        //strcpy(settings.serial.port, "/dev/ttyS0");
        settings.serial.speed = 9600;
        settings.serial.bits = 8;
        settings.serial.parity = 0;
        settings.serial.stopbits = 1;

        digita_send = digita_serial_send;
        digita_read = digita_serial_read;

        gpio_set_settings(dev->gpdev, settings);
        if (gpio_open(dev->gpdev) < 0) {
                fprintf(stderr, "error opening device\n");
                return NULL;
        }

        tcsendbreak(dev->gpdev->device_fd, 4);

        dev->gpdev->settings.serial.speed = 0;
        gpio_serial_set_baudrate(dev->gpdev);

        usleep(50);


        dev->gpdev->settings.serial.speed =init->port_settings.speed ;
        gpio_serial_set_baudrate(dev->gpdev);

        usleep(2000);

        memset((void *)&beacon, 0, sizeof(beacon));
        if (gpio_read(dev->gpdev, (void *)&beacon, sizeof(beacon)) < 0) {
                perror("reading beacon");
                return NULL;
        }

        printf("%04X %04X %04X %02X\n",
                beacon.intro, beacon.vendorid,
                beacon.deviceid, beacon.checksum);

// #define SPEED 57600
#define SPEED 115200
        beacon_ack.intro = htons(0x5aa5);
        beacon_ack.intftype = 0x55;
        beacon_ack.cf_reserved = 0;
        beacon_ack.cf_pod_receive_mode = 0;
        beacon_ack.cf_host_receive_mode = 0;
        beacon_ack.dataspeed = htonl(init->port_settings.speed);
        beacon_ack.deviceframesize = htons(1023);
        beacon_ack.hostframesize = htons(1023);
        beacon_ack.checksum = 0;
        beacon_ack.checksum = checksum((void *)&beacon_ack, sizeof(beacon_ack));

        if (gpio_write(dev->gpdev, (void *)&beacon_ack, sizeof(beacon_ack)) <
0) {
                perror("writing beacon_ack");
                return NULL;
        }

        if (gpio_read(dev->gpdev, (void *)&beacon_comp, sizeof(beacon_comp)) < 0) {
                perror("reading beacon_comp");
                return NULL;
        }

printf("%d\n", ntohl(beacon_comp.dataspeed));
        usleep(100000);

        dev->deviceframesize = ntohs(beacon_comp.deviceframesize);

        dev->gpdev->settings.serial.speed = ntohl(beacon_comp.dataspeed);
        gpio_serial_set_baudrate(dev->gpdev);

        usleep(100000);

        return dev;
}

