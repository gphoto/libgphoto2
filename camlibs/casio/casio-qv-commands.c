/* casio-qv-commands.c
 *
 * Copyright (C) 2001 Lutz Müller
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "casio-qv-commands.h"

#define STX 0x02
#define ETX 0x03
#define ENQ 0x05
#define ACK 0x06
#define DC2 0x12
#define NAK 0x15
#define ETB 0x17

#define CASIO_QV_RETRIES 5

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

int
QVping (Camera *camera)
{
	unsigned char c;
	int result = GP_OK, i;

	/* Send ENQ and wait for ACK */
	for (i = 0; i < CASIO_QV_RETRIES; i++) {
		c = ENQ;
		CHECK_RESULT (gp_port_write (camera->port, &c, 1));
		result = gp_port_read (camera->port, &c, 1);
		if (result >= 0)
			break;
		if (c == ACK)
			break;
	}
	if (i == CASIO_QV_RETRIES)
		return (result);

	return (GP_OK);
}

static int
QVsend (Camera *camera, unsigned char *cmd, int cmd_len,
			       unsigned char *buf, int buf_len)
{
	unsigned char c, checksum;
	int i;

	/* Write the request and calculate the checksum */
	CHECK_RESULT (gp_port_write (camera->port, cmd, cmd_len));
	for (i = 0, checksum = 0; i < cmd_len; i++)
		checksum += cmd[i];

	/* Read the checksum */
	CHECK_RESULT (gp_port_read (camera->port, &c, 1));
	if (c != ~checksum)
		return (GP_ERROR_CORRUPTED_DATA);

	/* Send ACK */
	c = ACK;
	CHECK_RESULT (gp_port_write (camera->port, &c, 1));

	/* Receive the answer */
	if (buf_len)
		CHECK_RESULT (gp_port_read (camera->port, buf, buf_len));

	return (GP_OK);
}

static int
QVblockrecv (Camera *camera, unsigned char *buf, int buf_size)
{
	unsigned char c;
	unsigned char buffer[2];
	int pos = 0, size, retries = 0, i;
	unsigned char sum;

	/* Send DC2 */
	CHECK_RESULT (gp_port_write (camera->port, &c, 1));

	while (1) {

		/* Read STX */
		CHECK_RESULT (gp_port_read (camera->port, &c, 1));
		if (c != STX) {
			retries++;
			c = NAK;
			CHECK_RESULT (gp_port_write (camera->port, &c, 1));
			if (retries > CASIO_QV_RETRIES)
				return (GP_ERROR_CORRUPTED_DATA);
			else
				continue;
		}

		/* Read sector size */
		CHECK_RESULT (gp_port_read (camera->port, buffer, 2));
		size = (buffer[0] << 8) | buffer[1];
		sum = buffer[0] + buffer[1];

		/* Get the sector */
		CHECK_RESULT (gp_port_read (camera->port, buf + pos, size));
		for (i = 0; i < size; i++)
			sum += buf[i + pos];

		/* Get EOT or ETX and the checksum */
		CHECK_RESULT (gp_port_read (camera->port, buffer, 2));
		sum += buffer[0];

		/* Verify the checksum */
		if (sum != ~buffer[1]) {
			c = NAK;
			CHECK_RESULT (gp_port_write (camera->port, &c, 1));
			continue;
		}	

		/* Acknowledge and prepare for next packet */
		c = ACK;
		CHECK_RESULT (gp_port_write (camera->port, &c, 1));
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
	CHECK_RESULT (QVsend (camera, cmd, 6, &b, 1));
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
	CHECK_RESULT (QVsend (camera, cmd, 2, buf, 4));
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
	CHECK_RESULT (QVsend (camera, cmd, 2, &b, 1));

	return (b);
}

int
QVpicattr (Camera *camera, int n, unsigned char *picattr)
{
	unsigned char cmd[2];
	unsigned char b;

	cmd[0] = 'D';
	cmd[1] = 'Y';
	CHECK_RESULT (QVsend (camera, cmd, 2, &b, 1));
	*picattr = b;

	return (GP_OK);
}

int
QVshowpic (Camera *camera, int n)
{
	unsigned char cmd[3];

	cmd[0] = 'D';
	cmd[1] = 'A';
	cmd[2] = n;
	CHECK_RESULT (QVsend (camera, cmd, 3, NULL, 0));

	return (GP_OK);
}

int
QVsetpic (Camera *camera)
{
	unsigned char cmd[2];

	cmd[0] = 'D';
	cmd[1] = 'L';
	CHECK_RESULT (QVsend (camera, cmd, 2, NULL, 0));

	return (GP_OK);
}

int
QVgetpic (Camera *camera, unsigned char *data, long int size)
{
	unsigned char cmd[2];

	cmd[0] = 'M';
	cmd[1] = 'G';
	CHECK_RESULT (QVsend (camera, cmd, 2, NULL, 0));
	CHECK_RESULT (QVblockrecv (camera, data, size));

	return (GP_OK);
}

int
QVdelete (Camera *camera, int n)
{
	unsigned char cmd[4];

	cmd[0] = 'D';
	cmd[1] = 'F';
	cmd[2] = n;
	cmd[3] = 0xff;
	CHECK_RESULT (QVsend (camera, cmd, 4, NULL, 0));

	return (GP_OK);
}

int
QVsize (Camera *camera, long int *size)
{
	unsigned char cmd[2];
	unsigned char buf[4];

	cmd[0] = 'E';
	cmd[1] = 'M';
	CHECK_RESULT (QVsend (camera, cmd, 2, buf, 4));
	*size = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

	return (GP_OK);
}

int
QVcapture (Camera *camera)
{
	unsigned char cmd[2];

	cmd[0] = 'D';
	cmd[1] = 'R';
	CHECK_RESULT (QVsend (camera, cmd, 2, NULL, 0));

	return (GP_OK);
}
