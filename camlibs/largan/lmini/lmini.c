/* library.c (for Largan)
 *
 * Copyright (C) 2002 Hubert Figuiere <hfiguiere@teaser.fr>
 * Code largely borrowed to lmini-0.1 by Steve O Connor
 * With the help of specifications for lmini camera by Largan
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2.h>
#include <gphoto2-port.h>
#include <gphoto2-endian.h>

#include "lmini.h"
#include "lmini_ccd.h"


#define GP_MODULE "largan"


/*
 * baud rate enumerations
 */
static struct largan_baud_rate {
	int baud;
	char value;	/* value to be sent in the protocol */
} bauds [] = { 
	{38400, 0x03 },
	{19200, 0x02 },
	{ 9600, 0x01 },
	{ 4800, 0x00 },
	{    0, 0x00 }
};

enum {
	LARGAN_NUM_PICT_CMD = 0xfa,
	LARGAN_GET_PICT_CMD = 0xfb,
	LARGAN_BAUD_ERASE_CMD = 0xfc,	/* set baud rate and erase use the same command */
	LARGAN_CAPTURE_CMD = 0xfd
};


static int purge_camera (Camera * camera);
static int wakeup_camera (Camera * camera);
static int largan_send_command (Camera * camera, uint8_t cmd, 
		uint8_t param1, uint8_t param2);
static int largan_recv_reply (Camera * camera, uint8_t *reply, 
		uint8_t *code, uint8_t * code2);
static int set_serial_speed (Camera * camera, int speed);


largan_pict_info * largan_pict_new (void)
{
	largan_pict_info * ptr = (largan_pict_info *)malloc (sizeof (largan_pict_info));
	memset (ptr, 0, sizeof (largan_pict_info));
	ptr->quality = 0xff; /* something invalid */
	return ptr;
}


void largan_pict_free (largan_pict_info * ptr)
{
	if (ptr->data) {
		free (ptr->data);
	}
	free (ptr);
}


void largan_pict_alloc_data (largan_pict_info * ptr, uint32_t size)
{
	ptr->data = realloc (ptr->data, size);
	ptr->data_size = size;
}


/*
 * Open the camera 
 */
int largan_open (Camera * camera)
{
	int ret;
	
	ret = purge_camera (camera);
	if (ret == GP_ERROR) {
		return ret;
	}
	return wakeup_camera (camera);
}

/*
 * Return the number of pictures in the camera, -1 if error.
 */
int largan_get_num_pict (Camera * camera)
{
	int ret;
	unsigned char reply, code;

	ret = largan_send_command (camera, LARGAN_NUM_PICT_CMD, 0, 0);
	if (ret < 0) {
		return -1;
	}
	
	ret = largan_recv_reply (camera, &reply, &code, NULL);
	if (ret < 0) {
		return -1;
	}
	if (reply != LARGAN_NUM_PICT_CMD) {
		return -1;
	}
	return code;
}

/*
 * get image index and return it in largan_pict_info
 *
 * return GP_OK if OK. Otherwise < 0
 */
int largan_get_pict (Camera * camera, largan_pict_type type, 
		uint8_t index, largan_pict_info * pict)
{
	int ret;
	uint8_t reply,code;
	uint8_t param;
	uint8_t buf[5];
	uint32_t pict_size;	/* size of the picture as returned by the camera */
	
	switch (type) {
		case LARGAN_PICT:
			param = 0x01;
			break;
		case LARGAN_THUMBNAIL:
			param = 0x00;
			break;
		default:
			GP_DEBUG ("largan_get_pict(): wrong picture type !\n");
			return GP_ERROR;
	}
	ret = largan_send_command (camera, LARGAN_GET_PICT_CMD, param, index);
	if (ret < 0) {
		return ret;
	}
	ret = largan_recv_reply (camera, &reply, &code, NULL);
	if (ret < 0) {
		return ret;
	}
	if ((reply != LARGAN_GET_PICT_CMD) || (code != 0x01)) {
		return GP_ERROR;
	}
	
	ret = gp_port_read (camera->port, buf, sizeof(buf));
	if (ret < GP_OK) {
		return ret;
	}
	if (ret < 5) {
		GP_DEBUG ("largan_get_pict(): unexpected short read\n");
		return GP_ERROR;
	}
	if (type == LARGAN_PICT) {
		if (buf[0] != index) {
			GP_DEBUG ("largan_get_pict(): picture index inconsistent\n");
			return GP_ERROR;
		}
	}
	else {
		if (buf[0] > 1) {
			GP_DEBUG ("largan_get_pict(): thumb size inconsistent\n");
			return GP_ERROR;
		}
	}
	pict->type = type;
	pict_size = be32atoh (&buf[1]);
	if (type == LARGAN_PICT) {
		largan_pict_alloc_data (pict, pict_size);
		ret = gp_port_read (camera->port, pict->data, pict->data_size);
		if (ret < GP_OK) {
			return ret;
		}
		if (ret < pict->data_size) {
			GP_DEBUG ("largan_get_pict(): picture data short read\n");
			return GP_ERROR;
		}
		pict->quality = 0xff;	/* this is not a thumbnail */
	}
	else {
		char * buffer = malloc (pict_size);
		ret = gp_port_read (camera->port, buffer, pict_size);
		if (ret < GP_OK) {
			return ret;
		}
		largan_pict_alloc_data (pict, 19200 + sizeof(BMPheader));
		memcpy (pict->data, BMPheader, sizeof(BMPheader));
		largan_ccd2dib (buffer, pict->data + sizeof(BMPheader), 240, 1);
		
		free (buffer);
		pict->quality = buf[0];
	}
	return GP_OK;
}



/*
 * erase a pictures
 * if all > 0 then all pictures are erased, otherwise the last one is
 *
 * return GP_OK if OK.
 */
int largan_erase (Camera *camera, int all)
{
	int ret;
	uint8_t erase_code;
	uint8_t reply,code;
	
	if (all) {
		erase_code = 0x11;
	}
	else {
		erase_code = 0x10;
	}
	ret = largan_send_command (camera, LARGAN_BAUD_ERASE_CMD, erase_code, 0);
	if (ret < 0) {
		return ret;
	}
	
	ret = largan_recv_reply (camera, &reply, &code, NULL);
	if (ret < 0) {
		return ret;
	}
	if ((reply != LARGAN_BAUD_ERASE_CMD) || (code != erase_code)) {
		GP_DEBUG ("largan_erase() wrong error code\n");
		return GP_ERROR;
	}
	return GP_OK;
}

/*
 * Take a picture with the camera
 *
 * return GP_OK if OK
 */
int largan_capture (Camera *camera)
{
	int ret;
	uint8_t reply, code, code2;

	ret = largan_send_command (camera, LARGAN_CAPTURE_CMD, 0, 0);
	if (ret < 0) {
		return ret;
	}

	ret = largan_recv_reply (camera, &reply, &code, &code2);
	if (ret < 0) {
		return ret;
	}
	if (reply != LARGAN_CAPTURE_CMD) {
		GP_DEBUG ("largan_capture(): inconsisten reply code\n");
		return GP_ERROR;
	}
	if (code != code2) {
		return GP_ERROR;
	}
	if (code == 0xee) {
		GP_DEBUG ("Memory full\n");
		return GP_ERROR;
	}
	if (code != 0xff) {
		GP_DEBUG ("largan_capture(): inconsistent reply\n");
		return GP_ERROR;
	}
	return GP_OK;
}


/*
 * Tell the camera to change the speed
 * Then set the serial port speed
 */
int largan_set_serial_speed (Camera * camera, int speed)
{
	int ret;
	int i;
	uint8_t reply, code;

	if (camera->port->type != GP_PORT_SERIAL) {
		GP_DEBUG ("largan_set_serial_speed() called on non serial port\n");
		return GP_ERROR;
	}
	for (i = 0; bauds[i].baud; i++) {
		if (bauds[i].baud == speed) {
			ret = largan_send_command (camera, LARGAN_BAUD_ERASE_CMD, bauds[i].value, 0);
			if (ret < 0) {
				return ret;
			}
			ret = largan_recv_reply (camera, &reply, &code, NULL);
			if (ret < 0) {	
				return ret;
			}
			if (reply !=  LARGAN_BAUD_ERASE_CMD) {
				return ret;
			}
			if (code != bauds[i].baud) {
				return ret;
			}
			return set_serial_speed (camera, bauds[i].baud);	
		}
	}
	GP_DEBUG ("largan_set_serial_speed(): baud rate not found\n");
	return GP_ERROR;
}


/*
 * received a reply from the camera.
 * can accept NULL on imput.
 * TODO: check how much we need to read.
 */
static int largan_recv_reply (Camera * camera, uint8_t *reply, 
		uint8_t *code, uint8_t *code2)
{
	int ret;
	uint8_t packet [4];
	memset (packet, 0, sizeof (packet));
	
	ret = gp_port_read (camera->port, packet, sizeof(packet));
	if (ret < GP_OK) {
		return ret;
	}
	if (reply) {
		*reply = packet[0];
	}
	if (code) {
		*code = packet[1];
	}
	if (code2) {
		*code2 = packet[2];
	}
	return ret;
}

/*
 * Send a command packet
 */
static int largan_send_command (Camera * camera, uint8_t cmd, uint8_t param1, 
		uint8_t param2)
{
	uint8_t packet [3];
	memset (&packet, 0, sizeof (packet));
	
	packet [0] = cmd;

	switch (cmd) {
	case LARGAN_NUM_PICT_CMD:
		break;
	case LARGAN_GET_PICT_CMD:
		if ((param1 != 0) && (param1 != 1)) {
			GP_DEBUG ("wrong parameter for get picture\n");
			return GP_ERROR; /* wrong parameter */
		}
		packet[1] = param1;
		packet[2] = param2;
		break;
	case LARGAN_BAUD_ERASE_CMD:
		if ((param1 > 0x11) || ((param1 > 0x03) && (param1 < 0x10))) {
			GP_DEBUG ("wrong parameter for baud/erase\n");
			return GP_ERROR; /* wrong parameter */
		}
		packet[1] = param1;
		break;
	case LARGAN_CAPTURE_CMD:
		break;
	default:
		GP_DEBUG ("unknown command\n");
		return GP_ERROR; /* unknown command */
	}

	return gp_port_write (camera->port, packet, sizeof (packet));
}


/*
 * set the baud speed for serial port
 */
static int set_serial_speed (Camera * camera, int speed)
{
	int ret;
	GPPortSettings settings;
	
	if (camera->port->type != GP_PORT_SERIAL) {
		GP_DEBUG ("set_serial_speed() called on non serial port\n");
		return GP_ERROR;
	}
	
	ret = gp_port_get_settings (camera->port, &settings);
	if (ret < 0)
		return (ret);
	settings.serial.speed    = speed;
	
	ret = gp_port_set_settings (camera->port, settings);
	return ret;
}

/*
 * purge the camera internal buffers
 */
static int purge_camera (Camera * camera)
{
	int count;
	long t1, t2;
	uint8_t buffer[1];

	t1 = time (NULL);

	while (1)
	{
		count = gp_port_read (camera->port, buffer, 1);
		if (count < GP_OK) 
			return count;

		if (count)
		{
			t1 = time(NULL);
		}
		else
		{
			t2 = time(NULL);
			if ((t2 - t1) > 1)
			{
				GP_DEBUG ("Camera purged\n");
				return GP_OK;
			}
		}
	}
	GP_DEBUG ("Could not purge camera\n");
	return GP_ERROR;
}

/*
 * Try to initiate something with the camera just to make sure it
 * answer. Will check all the speed in case the camera has not been reset
 * Perhaps is there a smarter way. This is the way it is done in lmini.
 */
static int wakeup_camera (Camera * camera)
{
	int num_pix;
	int i;
	int ret;

	if (camera->port->type == GP_PORT_SERIAL) {
	
		for (i = 0; bauds[i].baud; i++)
		{
			ret = set_serial_speed (camera, bauds[i].baud);
			num_pix = largan_get_num_pict (camera);
			if (num_pix >= 0)
			{
				return GP_OK;
			}
			sleep (1);
		}
	}
	purge_camera (camera);
	return GP_ERROR;
}

