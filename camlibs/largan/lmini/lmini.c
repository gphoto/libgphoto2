/* library.c (for Largan)
 *
 * Copyright 2002 Hubert Figuiere <hfiguiere@teaser.fr>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include "libgphoto2/gphoto2-endian.h"

#include "lmini.h"
#include "lmini_ccd.h"


#define GP_MODULE "largan"

static const char BMPheader[54]={
 0x42, 0x4d, 0x36, 0x10, 0xe, 0x0, 0x0, 0x0, 0x0, 0x0, 0x36, 0x0, 0x0, 0x0, 0x28,
  0x0,  0x0,  0x0, 0x54, 0x0, 0x0, 0x0,0x40, 0x0, 0x0,  0x0, 0x1, 0x0,0x18,  0x0,
  0x0,  0x0,  0x0,  0x0, 0x0,0x10, 0xe, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0,  0x0,
  0x0,  0x0,  0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

#if 0
/*
 * baud rate enumerations
 */
static struct largan_baud_rate {
	int baud;
	char value;	/* value to be sent in the protocol */
} bauds [] = {
	{19200, 0x02 },	/* default */
	{38400, 0x03 },
	{ 9600, 0x01 },
	{ 4800, 0x00 },
	{    0, 0x00 }
};
#endif

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


static void
largan_pict_alloc_data (largan_pict_info * ptr, uint32_t size)
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

        ret = largan_get_num_pict (camera);
        if (ret < 0){
                ret = purge_camera (camera);
                if (ret == GP_ERROR) {
                        return ret;
                }
                ret = wakeup_camera (camera);
	}
        return ret;     /* here: number of pictures */
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
		GP_DEBUG ("largan_send_command() failed: %d\n", ret);
		return -1;
	}
	ret = largan_recv_reply (camera, &reply, &code, NULL);
	if (ret < 0) {
		GP_DEBUG ("largan_recv_reply() failed: %d\n", ret);
		return -1;
	}
	if (reply != LARGAN_NUM_PICT_CMD) {
		GP_DEBUG ("Reply incorrect\n");
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
			GP_DEBUG ("largan_get_pict(): wrong picture type requested !\n");
			return GP_ERROR;
	}
	ret = largan_send_command (camera, LARGAN_GET_PICT_CMD, param, index);
	if (ret < 0) {
		return ret;
	}
        ret = largan_recv_reply (camera, &reply, &code, NULL);
/* here we have to receive 7 bytes back
 the 2nd contains 00 for thumbnail or 01 for original picture
 the 3rd contains the picturenumber for an original picture
 or 00 for an thumbnail for an lowres picture
 or 01 for an thumbnail for an hires picture
 bytes 4 5 6 7 contain the number of byte coming in the datastream */
/* the 1st and the 2nd byte are read here */
	if (ret < 0) {
                /* clean up and give it a 2nd chance */
                wakeup_camera (camera);
                largan_send_command (camera, LARGAN_GET_PICT_CMD, param, index);
                GP_DEBUG ("largan_get_pict(): command sent 2nd time\n");
                ret = largan_recv_reply (camera, &reply, &code, NULL);
                if (ret < 0) {
                        /* clean up and give it even a 3rd chance */
                        wakeup_camera (camera);
                        sleep (5);
                        largan_send_command (camera, LARGAN_GET_PICT_CMD, param, index);
                        GP_DEBUG ("largan_get_pict(): command sent 3rd time\n");
                        ret = largan_recv_reply (camera, &reply, &code, NULL);
                        if (ret < 0) {
                                GP_DEBUG ("largan_get_pict(): timeout after command sent 3rd time\n");
                                return ret;     /* timeout after 3rd trial */
                        }
                }
        }
        if ((reply != LARGAN_GET_PICT_CMD) || ((code != 0x01) && (code != 0x00))) {
                GP_DEBUG ("largan_get_pict(): code != 0x01 && 0x00\n");
		return GP_ERROR;
	}

	/* the remaining 5 bytes are read here */

	ret = gp_port_read (camera->port, (char *)buf, sizeof(buf));
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
	switch (type) {
	case LARGAN_PICT:
		largan_pict_alloc_data (pict, pict_size);
		ret = gp_port_read (camera->port, pict->data, pict->data_size);
		if (ret < GP_OK) {
			return ret;
		}
		if ((unsigned int)ret < pict->data_size) {
			GP_DEBUG ("largan_get_pict(): picture data short read\n");
			return GP_ERROR;
		}
		pict->quality = 0xff;	/* this is not a thumbnail */
		break;
	case LARGAN_THUMBNAIL:
		{
			char * buffer = (char*)malloc(pict_size);

			if (!buffer)
				return GP_ERROR_NO_MEMORY;
			ret = gp_port_read (camera->port, buffer, pict_size);
			if (ret < GP_OK) {
				free (buffer);
				return ret;
			}
			largan_pict_alloc_data (pict, 19200 + sizeof(BMPheader));
			memcpy (pict->data, BMPheader, sizeof(BMPheader));
			/*this is the segfaulty function */
			largan_ccd2dib (buffer, pict->data + sizeof(BMPheader), 240, 1);

			free (buffer);
			pict->quality = buf[0];
			break;
		}
	default:
		GP_DEBUG ("largan_get_pict(): type not LARGAN_PICT nor LARGAN_THUMBNAIL\n");
                return GP_ERROR;
	}
	return GP_OK;
}



/*
 * erase a pictures
 * if all > 0 then all pictures are erased, otherwise the last one is
 *
 * return GP_OK if OK.
 */
int largan_erase (Camera *camera, int pict_num)
{
	int ret;
	int num_pix;
	uint8_t erase_code;
	uint8_t reply,code;

	if (pict_num == 0xff) {
		erase_code = 0x11;
		GP_DEBUG ("largan_erase() all sheets \n");
	}
	else {
		num_pix = largan_get_num_pict (camera);
		if (pict_num == num_pix) {
			erase_code = 0x10;
			GP_DEBUG ("largan_erase() last sheet \n");
		}
		else {
			GP_DEBUG ("Only the last sheet can be erased!\n");
			return GP_ERROR;
		}
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
		GP_DEBUG ("return ret\n");
		return ret;
	}
	if (reply != LARGAN_CAPTURE_CMD) {
		GP_DEBUG ("largan_capture(): inconsistent reply code\n");
		return GP_ERROR;
	}
	if (code != code2) {
		GP_DEBUG ("code != code2\n");
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


#if 0
/*
 * Tell the camera to change the speed
 * Then set the serial port speed
 */
static int largan_set_serial_speed (Camera * camera, int speed)
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
#endif


/*
 * received a reply from the camera.
 * can accept NULL on input.
 * TODO: check how much we need to read.
 */
static int largan_recv_reply (Camera * camera, uint8_t *reply,
		uint8_t *code, uint8_t *code2)
{
	int ret;
	uint8_t packet [4];
	uint8_t	packet_size = 0;
	memset (packet, 0, sizeof (packet));
	ret = gp_port_read (camera->port, (char *)packet, 1);
	if (ret < GP_OK) {
		return ret;
	}
	switch (packet[0])
	{
	case LARGAN_NUM_PICT_CMD:
		packet_size = 2;
		break;
	case LARGAN_GET_PICT_CMD:
		packet_size = 2;
		break;
	case LARGAN_BAUD_ERASE_CMD:
		packet_size = 2;
		break;
	case LARGAN_CAPTURE_CMD:
		packet_size = 3;
		break;
	default:
		packet_size = 0;
		GP_DEBUG("largan_receive_reply: Unknown reply.\n");
		break;
	}
	if (reply) {
		*reply = packet[0];
	}
	if (packet_size >= 2) {
		ret = gp_port_read (camera->port, (char *)&packet[1], 1);
		if (ret < GP_OK) {
			return ret;
		}
		if (code) {
			*code = packet[1];
		}
	}
	if (packet_size >= 3) {
		ret = gp_port_read (camera->port, (char *)&packet[2], 1);
		if (ret < GP_OK) {
			return ret;
		}
		if (code2) {
			*code2 = packet[2];
		}
	}

	return ret;
}

/*
 * Send a command packet
 */
static int largan_send_command (Camera * camera, uint8_t cmd, uint8_t param1,
		uint8_t param2)
{
	uint8_t	packet_size = 0;
	uint8_t packet [3];
	memset (&packet, 0, sizeof (packet));

	packet [0] = cmd;

	switch (cmd) {
	case LARGAN_NUM_PICT_CMD:
		packet_size = 1;
		break;
	case LARGAN_GET_PICT_CMD:
		if ((param1 != 0) && (param1 != 1)) {
			GP_DEBUG ("wrong parameter for get picture\n");
			return GP_ERROR; /* wrong parameter */
		}
		packet[1] = param1;
		packet[2] = param2;
		packet_size = 3;
		break;
	case LARGAN_BAUD_ERASE_CMD:
		if ((param1 > 0x11) || ((param1 > 0x03) && (param1 < 0x10))) {
			GP_DEBUG ("wrong parameter for baud/erase\n");
			return GP_ERROR; /* wrong parameter */
		}
		packet[1] = param1;
		packet_size = 2;
		break;
	case LARGAN_CAPTURE_CMD:
		packet_size = 1;
		break;
	default:
		GP_DEBUG ("unknown command\n");
		return GP_ERROR; /* unknown command */
	}

	return gp_port_write (camera->port, (char *)packet, packet_size);
}


/*
 * set the baud speed for serial port
 */
static int set_serial_speed (Camera * camera, int speed)
{
	int ret;
	GPPortSettings settings;
	GP_DEBUG ("set_serial_speed() called ***************\n");

	if (camera->port->type != GP_PORT_SERIAL) {
		GP_DEBUG ("set_serial_speed() called on non serial port\n");
		return GP_ERROR;
	}

	ret = gp_port_get_settings (camera->port, &settings);
	if (ret < 0) {
		return ret;
	}
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
		count = gp_port_read (camera->port, (char *)buffer, 1);
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
	/*	int i;*/

	if (camera->port->type == GP_PORT_SERIAL) {
		set_serial_speed (camera, 4800);	/*wakes camera best*/
		num_pix = largan_get_num_pict (camera);	/*this wakes*/
		set_serial_speed (camera, 19200);	/*back to normal*/
		sleep (1);
		num_pix = largan_get_num_pict (camera);	/*this wakes*/
		if (num_pix >= 0){
			return GP_OK;
		}
	}
	purge_camera (camera);
	return GP_ERROR;
}
