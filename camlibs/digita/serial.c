/*
 * serial.c
 *
 *  Serial digita support
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

#define _BSD_SOURCE

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#ifdef OS2
#include <db.h>
#endif

#include "gphoto2-endian.h"
#include "digita.h"

#define GP_MODULE "digita"

#include <gphoto2/gphoto2-port.h>

#define MAX_BEACON_RETRIES	5
#define MAX_BEACON_TIMEOUTS	2

/*
 * u8  0xA5
 * u8  0x5A
 * u16 vendorid
 * u16 deviceid
 * u8  checksum
 */
#define BEACON_LEN		7

/*
 * u8  0x5A
 * u8  0xA5
 * u8  0x55 (iftype)
 * u8  commflag
 *  u4 reserved
 *  u2 pod_receive_mode
 *  u2 host_receive_mode
 * u32 dataspeed
 * u16 deviceframesize
 * u16 hostframesize
 * u8  checksum
 */
#define BEACON_ACK_LENGTH	13

/*
 * s8  result
 * u8  commflag
 *  u4 reserved
 *  u2 pod_receive_mode
 *  u2 host_receive_mode
 * u32 dataspeed
 * u16 deviceframesize
 * u16 hostframesize
 */
#define BEACON_COMP_LENGTH	10

#define POLL_LENGTH_MASK        0x03FF
#define POLL_BOB                0x0400
#define POLL_EOB                0x0800
#define POLL_CMD                0x1000
#define POLL_POLL_MASK          0xE000
#define POLL_POLL		(1 << 13)

#define POLL_ACK                0x01
#define POLL_NAK                0x02

static unsigned int checksum(const unsigned char *buffer, int len)
{
	int i;
	int limit = len - 1;
	unsigned int sum = 0;

	for (i = 0; i < limit; i++)
		sum += *(buffer++);

	return sum & 0xff;
}

static int poll_and_wait(gp_port *dev, int length, int bob, int eob)
{
	unsigned short s, poll, poll_reply;

	poll = POLL_POLL | POLL_CMD | (length & POLL_LENGTH_MASK) |
		(bob ? POLL_BOB : 0) | (eob ? POLL_EOB : 0);

	do {
		s = htobe16(poll);
		if (gp_port_write(dev, (void *)&s, sizeof(s)) < 0)
			return -1;
		if (gp_port_read(dev, (void *)&s, sizeof(s)) < 0)
			return -1;
		poll_reply = be16toh(s);
	} while (poll_reply & POLL_NAK);

	return 0;
}

static int digita_serial_send(CameraPrivateLibrary *dev, void *_buffer, int len)
{
	char *buffer = _buffer;
	unsigned short s;
	int sent = 0, size;

	while (sent < len) {
		if ((len - sent) > dev->deviceframesize)
			size = dev->deviceframesize;
		else
			size = len - sent;

		if (poll_and_wait(dev->gpdev, size, sent == 0, (size + sent) == len) < 0)
			return -1;

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
	unsigned short s, poll;

	if (gp_port_read(dev, (void *)&s, sizeof(s)) < 0)
		return -1;

	poll = be16toh(s);
	if (length)
		*length = poll & POLL_LENGTH_MASK;
	if (eob)
		*eob = poll & POLL_EOB;

	s = htobe16(nak ? POLL_NAK : POLL_ACK);
	if (gp_port_write(dev, (void *)&s, sizeof(s)) < 0)
		return -1;

	return 0;
}

static int digita_serial_read(CameraPrivateLibrary *dev, void *_buffer, int len)
{
	char *buffer = _buffer;
	unsigned short s;
	int received = 0, size, eob;

	while (received < len) {
		if (poll_and_reply(dev->gpdev, &size, &eob, 0) < 0)
			return -1;

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

int digita_serial_open(CameraPrivateLibrary *dev, Camera *camera)
{
	GPPortSettings settings;
	int selected_speed;
	int ret, retries, negotiated = 0;
	unsigned char buffer[20];
	unsigned short s;
	unsigned int l;

	/* Get the settings */
	ret = gp_port_get_settings(camera->port, &settings);
	if (ret < 0)
		return ret;

	/* Remember the selected speed */
	selected_speed = settings.serial.speed;
	if (!selected_speed)
		selected_speed = 115200;	/* Try the maximum speed */

	/* Set the settings */
	settings.serial.speed = 9600;
	settings.serial.bits = 8;
	settings.serial.parity = 0;
	settings.serial.stopbits = 1;
	ret = gp_port_set_settings(camera->port, settings);
	if (ret < 0)
		return ret;

	dev->send = digita_serial_send;
	dev->read = digita_serial_read;

	gp_port_send_break(dev->gpdev, 4);

	usleep(10000);

	/* FIXME: In some situations we may want to try a slower speed */
	for (retries = 0; !negotiated && retries < MAX_BEACON_RETRIES; retries++) {
		unsigned char csum;
		int i, timeouts = 0;

		/*
		 * Read the beacon
		 */
		memset(buffer, 0, sizeof(buffer));

		for (i = 0; (i < BEACON_LEN * 2) && (timeouts < MAX_BEACON_TIMEOUTS); i++) {
			/* We specifically eat as much as we can to catch any */
			/* garbage before or after */
			ret = gp_port_read(dev->gpdev, (void *)buffer, 1);
			if (ret < 0) {
				GP_DEBUG("couldn't read beacon (ret = %d)", ret);
				timeouts++;
				continue;
			}

			if (buffer[0] == 0xA5)
				break;
		}

		if (timeouts >= MAX_BEACON_TIMEOUTS)
			continue;

		ret = gp_port_read(dev->gpdev, (void *)(buffer + 1), BEACON_LEN - 1);
		if (ret < 0) {
			GP_DEBUG("couldn't read beacon (ret = %d)", ret);
			continue;
		}

		if (buffer[0] != 0xA5 || buffer[1] != 0x5A) {
			GP_DEBUG("Invalid header for beacon 0x%02x 0x%02x", buffer[0], buffer[1]);
			continue;
		}

		csum = buffer[6];
		buffer[6] = 0;
		if (checksum(buffer, BEACON_LEN) != csum) {
			GP_DEBUG("Beacon checksum failed (calculated 0x%02x, received 0x%02x)",
				checksum(buffer, BEACON_LEN), csum);
			continue;
		}

		memcpy((void *)&s, &buffer[2], 2);
		GP_DEBUG("Vendor: 0x%04x", be16toh(s));
		memcpy((void *)&s, &buffer[4], 2);
		GP_DEBUG("Product: 0x%04x", be16toh(s));

		/*
		 * Send the beacon acknowledgement
		 */
		buffer[0] = 0x5A;	/* Magic */
		buffer[1] = 0xA5;
		buffer[2] = 0x55;	/* I/F Type */
		buffer[3] = 0;		/* Comm Flag */
		l = htobe32(selected_speed);
		memcpy(&buffer[4], (void *)&l, 4);	/* Data speed */
		s = htobe16(1023);
		memcpy(&buffer[8], (void *)&s, 2);	/* Device Frame Size */
		s = htobe16(1023);
		memcpy(&buffer[10], (void *)&s, 2);	/* Host Frame Size */
		buffer[12] = 0;
		buffer[12] = checksum(buffer, BEACON_ACK_LENGTH);

		ret = gp_port_write(dev->gpdev, (void *)buffer, BEACON_ACK_LENGTH);
		if (ret < 0) {
			GP_DEBUG("couldn't write beacon (ret = %d)", ret);
			return -1;
		}

		/*
		 * Read the beacon completion
		 */
		ret = gp_port_read(dev->gpdev, (void *)buffer, BEACON_COMP_LENGTH);
		if (ret < 0) {
			GP_DEBUG("couldn't read beacon_comp (ret = %d)", ret);
			continue;
		}

		if ((signed char)buffer[0] < 0) {
			GP_DEBUG("Bad status %d during beacon completion", (signed int)buffer[0]);
			continue;
		}

		memcpy((void *)&s, &buffer[6], sizeof(s));
		dev->deviceframesize = be16toh(s);

		memcpy((void *)&l, &buffer[2], sizeof(l));
		GP_DEBUG("negotiated %d", be32toh(l));
		settings.serial.speed = be32toh(l);

		usleep(100000);	/* Wait before */

		ret = gp_port_set_settings(dev->gpdev, settings);
		if (ret < 0)
			return ret;

		usleep(100000);	/* Wait after */

		/*
		 * The host interface spec mentions kTestSerialData, but
		 * doesn't elaborate anywhere else on it. So, I guess we
		 * assume everything is working here
		 */
		negotiated = 1;
	}

	return negotiated ? 0 : -1;
}

