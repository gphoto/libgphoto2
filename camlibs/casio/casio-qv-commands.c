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

#define ENQ 0x05
#define ACK 0x06

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
QVcapture (Camera *camera)
{
	unsigned char cmd[2];

	cmd[0] = 'D';
	cmd[1] = 'R';
	CHECK_RESULT (QVsend (camera, cmd, 2, NULL, 0));

	return (GP_OK);
}
