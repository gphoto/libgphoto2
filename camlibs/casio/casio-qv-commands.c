/* casio-qv-commands.c
 *
 * Copyright 2001 Lutz Mueller
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
#include "casio-qv-commands.h"

#include <stdlib.h>

#define STX 0x02
#define ETX 0x03
#define ENQ 0x05
#define ACK 0x06
#define DC2 0x12
#define NAK 0x15
#define ETB 0x17
#define UNKNOWN1 0xfe
#define UNKNOWN2 0xe0

#define CASIO_QV_RETRIES 5

#define CR(result) {int r = (result); if (r < 0) return (r);}

int
QVping (Camera *camera)
{
	unsigned char c;
	int result = GP_OK, i = 0;

	/* Send ENQ and wait for ACK */
	while (1) {
		c = ENQ;
		CR (gp_port_write (camera->port, (char *)&c, 1));
		result = gp_port_read (camera->port, (char *)&c, 1);

		/* If we got ACK, everything is fine. */
		if (result >= 0) {
			switch (c) {
			case ACK:
			case ENQ:

				/*
				 * According to gphoto, we need to wait
				 * for ACK. But the camera of
				 * David Wolfskill <david@catwhisker.org>
				 * seems to return ENQ after some NAK.
				 */
				return (GP_OK);

			case NAK:

				/* The camera is not yet ready. */
				break;

			case UNKNOWN1:
			case UNKNOWN2:

				/*
				 * David Wolfskill <david@catwhisker.org>
				 * has seen those two bytes if one sends
				 * only ENQs to the camera. The camera first
				 * answers with some NAKs, then with some
				 * ACKs, and finally with UNKNOWN1 and
				 * UNKNOWN2.
				 */
				while (gp_port_read (camera->port, (char *)&c, 1) >= 0);
				break;

			default:
				while (gp_port_read (camera->port, (char *)&c, 1) >= 0);
				break;
			}
		}

		if (++i < CASIO_QV_RETRIES)
			continue;

		/* If we got an error from libgphoto2_port, pass it along */
		CR (result);

		/* Return some error code */
		return (GP_ERROR_CORRUPTED_DATA);
	}
}

static int
QVsend (Camera *camera, unsigned char *cmd, int cmd_len,
			       unsigned char *buf, int buf_len)
{
	unsigned char c;
	int checksum;
	const unsigned char *cmd_end;

	/* The camera does not insist on a ping each command, but */
	/* sometimes it hangs up without one.                     */
	CR (QVping (camera));

	/* Write the request and calculate the checksum */
	CR (gp_port_write (camera->port, (char *)cmd, cmd_len));
	for (cmd_end = cmd+cmd_len, checksum = 0; cmd < cmd_end; ++cmd)
		checksum += *cmd;

	/* Read the checksum */
	CR (gp_port_read (camera->port, (char *)&c, 1));
	if (c != (unsigned char)(~checksum))
		return (GP_ERROR_CORRUPTED_DATA);

	/* Send ACK */
	c = ACK;
	CR (gp_port_write (camera->port, (char *)&c, 1));

	/* Receive the answer */
	if (buf_len)
		CR (gp_port_read (camera->port, (char *)buf, buf_len));

	return (GP_OK);
}

static int
QVblockrecv (Camera *camera, unsigned char **buf, unsigned long int *buf_len)
{
	/* XXX - does the caller know to free *buf in case of an error? */
	unsigned char c;
	int retries, pos;

	retries = 0;
	*buf = NULL;
	*buf_len = 0;
	pos = 0;

	/* Send DC2 */
	c = DC2;
	CR (gp_port_write (camera->port, (char *)&c, 1));

	while (1) {
		unsigned char buffer[2];
		int size, i;
		int sum;
		unsigned char *new;

		/* Read STX */
		CR (gp_port_read (camera->port, (char *)&c, 1));
		if (c != STX) {
			retries++;
			c = NAK;
			CR (gp_port_write (camera->port, (char *)&c, 1));
			if (retries > CASIO_QV_RETRIES)
				return (GP_ERROR_CORRUPTED_DATA);
			else
				continue;
		}

		/* Read sector size */
		CR (gp_port_read (camera->port, (char *)buffer, 2));
		size = (buffer[0] << 8) | buffer[1];
		sum = buffer[0] + buffer[1];

		/* Allocate the memory */
		new = (unsigned char*)realloc (*buf, sizeof (char) * (*buf_len + size));
		if (new == (unsigned char*)0) {
			if (*buf != (unsigned char*)0) free(*buf);
			return (GP_ERROR_NO_MEMORY);
		}
		*buf = new;
		*buf_len += size;

		/* Get the sector */
		CR (gp_port_read (camera->port, (char *)*buf + pos, size));
		for (i = 0; i < size; i++)
			sum += (*buf)[i + pos];

		/* Get EOT or ETX and the checksum */
		CR (gp_port_read (camera->port, (char *)buffer, 2));
		sum += buffer[0];

		/* Verify the checksum */
		if ((unsigned char)(~sum) != buffer[1]) {
			retries++;
			c = NAK;
			CR (gp_port_write (camera->port, (char *)&c, 1));
			if (retries > CASIO_QV_RETRIES)
				return (GP_ERROR_CORRUPTED_DATA);
			else
				continue;
		}

		/* Acknowledge and prepare for next packet */
		c = ACK;
		CR (gp_port_write (camera->port, (char *)&c, 1));
		pos += size;

		/* Are we done? */
		if (buffer[0] == ETX)
			break;		/* Yes */
		else if (buffer[0] == ETB)
			continue;	/* No  */
		else
			return (GP_ERROR_CORRUPTED_DATA);
	}

	return (GP_OK);
}

int
QVbattery (Camera *camera, float *battery)
{
	unsigned char cmd[6];
	unsigned char b;

	cmd[0] = 'R';
	cmd[1] = 'B';
	cmd[2] = ENQ;
	cmd[3] = 0xff;
	cmd[4] = 0xfe;
	cmd[5] = 0xe6;
	CR (QVsend (camera, cmd, 6, &b, 1));
	*battery = b / 16.;

	return (GP_OK);
}

int
QVrevision (Camera *camera, long int *revision)
{
	unsigned char cmd[2];
	unsigned char buf[4];

	cmd[0] = 'S';
	cmd[1] = 'U';
	CR (QVsend (camera, cmd, 2, buf, 4));
	*revision = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

	return (GP_OK);
}

int
QVnumpic (Camera *camera)
{
	unsigned char cmd[2];
	unsigned char b;

	cmd[0] = 'M';
	cmd[1] = 'P';
	CR (QVsend (camera, cmd, 2, &b, 1));

	return (b);
}

int
QVpicattr (Camera *camera, int n, unsigned char *picattr)
{
	unsigned char cmd[4];
	unsigned char b;

	cmd[0] = 'D';
	cmd[1] = 'Y';
        cmd[2] = STX;
        cmd[3] = n+1;
	CR (QVsend (camera, cmd, 4, &b, 1));
	*picattr = b;

	return (GP_OK);
}

int
QVshowpic (Camera *camera, int n)
{
	unsigned char cmd[3];

	cmd[0] = 'D';
	cmd[1] = 'A';
	cmd[2] = n+1;
	CR (QVsend (camera, cmd, 3, NULL, 0));

	return (GP_OK);
}

int
QVsetpic (Camera *camera)
{
	unsigned char cmd[2];

	cmd[0] = 'D';
	cmd[1] = 'L';
	CR (QVsend (camera, cmd, 2, NULL, 0));

	return (GP_OK);
}

int
QVgetCAMpic (Camera *camera, unsigned char **data, unsigned long int *size, int fine)
{
	unsigned char cmd[2];

	cmd[0] = 'M';
	cmd[1] = fine ? 'g' : 'G';
	CR (QVsend (camera, cmd, 2, NULL, 0));
	CR (QVblockrecv (camera, data, size));

	return (GP_OK);
}

int
QVgetYCCpic (Camera *camera, unsigned char **data, unsigned long int *size)
{
	unsigned char cmd[2];

	cmd[0] = 'M';
	cmd[1] = 'K';
	CR (QVsend (camera, cmd, 2, NULL, 0));
	CR (QVblockrecv (camera, data, size));

	return (GP_OK);
}

int
QVdelete (Camera *camera, int n)
{
	unsigned char cmd[4];

	cmd[0] = 'D';
	cmd[1] = 'F';
	cmd[2] = n+1;
	cmd[3] = 0xff;
	CR (QVsend (camera, cmd, 4, NULL, 0));

	return (GP_OK);
}

int
QVprotect (Camera *camera, int n, int on)
{
	unsigned char cmd[4];

	cmd[0] = 'D';
	cmd[1] = 'Y';
	cmd[2] = on ? 1 : 0;
	cmd[3] = n+1;
	CR (QVsend (camera, cmd, 4, NULL, 0));

	return (GP_OK);
}

int
QVsize (Camera *camera, long int *size)
{
	unsigned char cmd[2];
	unsigned char buf[4];

	cmd[0] = 'E';
	cmd[1] = 'M';
	CR (QVsend (camera, cmd, 2, buf, 4));
	*size = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

	return (GP_OK);
}

int
QVcapture (Camera *camera)
{
	unsigned char cmd[2];
	unsigned char b;

	cmd[0] = 'D';
	cmd[1] = 'R';
	CR (QVsend (camera, cmd, 2, &b, 1));

	return (GP_OK);
}

int
QVstatus (Camera *camera, char *status)
{
        unsigned char cmd[3];

        cmd[0] = 'D';
        cmd[1] = 'S';
        cmd[2] = STX;
        CR (QVsend (camera, cmd, 3, (unsigned char *)status, 2));

        return (GP_OK);
}

int
QVreset (Camera *camera)
{
        unsigned char cmd[2];

	cmd[0] = 'Q';
	cmd[1] = 'R';
	CR (QVsend (camera, cmd, 2, NULL, 0));

        return (GP_OK);
}

int
QVsetspeed (Camera *camera, int speed)
{
	unsigned char cmd[3];
        gp_port_settings settings;

	cmd[0] = 'C';
	cmd[1] = 'B';
	switch (speed) {
	case   9600: cmd[2] = 46; break;
	case  19200: cmd[2] = 22; break;
	case  38400: cmd[2] = 11; break;
	case  57600: cmd[2] =  7; break;
	case 115200: cmd[2] =  3; break;
	default: return (GP_ERROR_NOT_SUPPORTED);
	}
	CR (QVsend (camera, cmd, 3, NULL, 0));

        CR (gp_port_get_settings (camera->port, &settings));
        settings.serial.speed = speed;
        CR (gp_port_set_settings (camera->port, settings));

	CR (QVping (camera));

	return (GP_OK);
}
