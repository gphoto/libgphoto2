/****************************************************************************
 *
 * File: serial.c
 *
 * Serial communication layer.
 *
 * $Header$
 ****************************************************************************/

/****************************************************************************
 *
 * include files
 *
 ****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <gphoto2.h>

#include "psa50.h"
#include "canon.h"
#include "serial.h"


void
serial_flush_input (GPPort *gdev)
{
}

void
serial_flush_output (GPPort *gdev)
{
}

/*****************************************************************************
 *
 * canon_serial_change_speed
 *
 * change the speed of the communication.
 *
 * speed - the new speed
 *
 * Returns 1 on success.
 * Returns 0 on any error.
 *
 ****************************************************************************/

int
canon_serial_change_speed (GPPort *gdev, int speed)
{
	gp_port_settings settings;

	/* set speed */
	gp_port_get_settings (gdev, &settings);
	settings.serial.speed = speed;
	gp_port_set_settings (gdev, settings);

	usleep (70000);

	return 1;
}


/*****************************************************************************
 *
 * canon_serial_get_cts
 *
 * Gets the status of the CTS (Clear To Send) line on the serial port.
 *
 * CTS is "1" when the camera is ON, and "0" when it is OFF.
 *
 * Returns 1 on CTS high.
 * Returns 0 on CTS low.
 *
 ****************************************************************************/
int
canon_serial_get_cts (GPPort *gdev)
{
	int level;

	gp_port_get_pin (gdev, PIN_CTS, &level);
	return (level);
}

/*****************************************************************************
 *
 * canon_usb_camera_init
 *
 * Initializes the USB camera through a series of read/writes
 *
 * Returns GP_OK on success.
 * Returns GP_ERROR on any error.
 *
 ****************************************************************************/
int
canon_usb_camera_init (Camera *camera)
{
	unsigned char msg[0x58];
	unsigned char buffer[0x44];
	int i;
	char *camstat_str = "NOT RECOGNIZED";
	unsigned char camstat;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_usb_camera_init()");

	memset (msg, 0, sizeof (msg));
	memset (buffer, 0, sizeof (buffer));

	i = gp_port_usb_msg_read (camera->port, 0x0c, 0x55, 0, msg, 1);
	if (i != 1) {
		fprintf (stderr, "canon_usb_camera_init(): step #1 read failed! (returned %i) "
			 "Camera not operational.\n", i);
		return GP_ERROR;
	}
	camstat = msg[0];
	switch (camstat) {
		case 'A':
			camstat_str = "Camera was already active";
			break;
		case 'C':
			camstat_str = "Camera was woken up";
			break;
		case 'I':
		case 'E':
			camstat_str = "Unknown (some kind of error)";
			break;
	}
	if (camstat != 'A' && camstat != 'C') {
		gp_debug_printf (GP_DEBUG_NONE, "canon",
				 "canon_usb_camera_init(): initial camera response: %c/'%s' "
				 "not 'A' or 'C'. Camera not operational.", camstat,
				 camstat_str);
		return GP_ERROR;
	}
	gp_debug_printf (GP_DEBUG_LOW, "canon",
			 "canon_usb_camera_init(): initial camera response: %c/'%s'", camstat,
			 camstat_str);

	i = gp_port_usb_msg_read (camera->port, 0x04, 0x1, 0, msg, 0x58);
	if (i != 0x58) {
		gp_debug_printf (GP_DEBUG_NONE, "canon",
				 "canon_usb_camera_init(): step #2 read failed! (returned %i, expected %i) "
				 "Camera not operational.", i, 0x58);
		return GP_ERROR;
	}

	i = gp_port_usb_msg_write (camera->port, 0x04, 0x11, 0, msg + 0x48, 0x10);
	if (i != 0x10) {
		gp_debug_printf (GP_DEBUG_NONE, "canon",
				 "canon_usb_camera_serial(): step #3 write failed! (returned %i, expected %i) "
				 "Camera not operational.", i, 0x10);
		return GP_ERROR;
	}
	gp_debug_printf (GP_DEBUG_LOW, "canon",
			 "canon_usb_camera_init(): PC sign on LCD should be lit now");

	i = gp_port_read (camera->port, buffer, 0x44);

	if ((i >= 4)
	    && (buffer[i - 4] == 0x54) && (buffer[i - 3] == 0x78)
	    && (buffer[i - 2] == 0x00) && (buffer[i - 1] == 0x00)) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "canon_usb_camera_init(): expected %i and got %i bytes with "
				 "\"54 78 00 00\" at the end, so we just ignore the whole bunch",
				 0x44, i);
	} else {
		gp_debug_printf (GP_DEBUG_NONE, "canon",
				 "canon_usb_camera_init(): step #4 read failed! (returned %i, expected %i) "
				 "Camera might still work though. Continuing.", i, 0x44);
	}
	return GP_OK;
}

/*****************************************************************************
 *
 * canon_serial_init
 *
 * Initializes the given serial or USB device.
 *
 * devname - the name of the device to open
 *
 * Returns 0 on success.
 * Returns -1 on any error.
 *
 ****************************************************************************/

int
canon_serial_init (Camera *camera)
{
	int res;
	GPPortSettings settings;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "Initializing the camera.\n");

	/* Get the current settings */
	gp_port_get_settings (camera->port, &settings);

	/* Adjust the current settings */
	switch (camera->pl->canon_comm_method) {
	case CANON_USB:
		settings.usb.inep = 0x81;
		settings.usb.outep = 0x02;
		settings.usb.config = 1;
		settings.usb.altsetting = 0;
		break;
	case CANON_SERIAL_RS232:
	default:
		settings.serial.speed = 9600;
		settings.serial.bits = 8;
		settings.serial.parity = 0;
		settings.serial.stopbits = 1;
		break;
	}

	/* Set the new settings */
	gp_port_set_settings (camera->port, settings);

	/* Do further initialization */
	switch (camera->pl->canon_comm_method) {
	case CANON_USB:
		res = canon_usb_camera_init (camera);
		if (res != GP_OK) {
			fprintf (stderr, "canon_init_serial(): "
				 "Cannot initialize camera, "
				 "canon_usb_camera_init() "
				 "returned %i\n", res);
			return GP_ERROR;
		}
		break;
	default:
		break;
	}

	return GP_OK;
}

/*****************************************************************************
 *
 * canon_serial_send
 *
 * Send the given buffer with given length over the serial line.
 *
 * buf   - the raw data buffer to send
 * len   - the length of the buffer
 * sleep - time in usec to wait between characters
 *
 * Returns 0 on success, -1 on error.
 *
 ****************************************************************************/

int
canon_serial_send (Camera *camera, const unsigned char *buf, int len, int sleep)
{
	int i;

	/* the A50 does not like to get too much data in a row at 115200
	 * The S10 and S20 do not have this problem */
	if (sleep > 0 && camera->pl->slow_send == 1) {
		for (i = 0; i < len; i++) {
			gp_port_write (camera->port, (char *) buf, 1);
			buf++;
			usleep (sleep);
		}
	} else {
		gp_port_write (camera->port, (char *) buf, len);
	}

	return 0;
}


/**
 * Sets the timeout, in miliseconds.
 */
void
serial_set_timeout (GPPort *gdev, int to)
{
	gp_port_set_timeout (gdev, to);
}

/*****************************************************************************
 *
 * canon_serial_get_byte
 *
 * Get the next byte from the serial line.
 * Actually the function reads chunks of data and keeps them in a cache.
 * Only one byte per call will be returned.
 *
 * Returns the byte on success, -1 on error.
 *
 ****************************************************************************/
int
canon_serial_get_byte (GPPort *gdev)
{
	static unsigned char cache[512];
	static unsigned char *cachep = cache;
	static unsigned char *cachee = cache;
	int recv;

	/* if still data in cache, get it */
	if (cachep < cachee) {
		return (int) *cachep++;
	}

	recv = gp_port_read (gdev, cache, 1);
	if (recv == GP_ERROR || recv == GP_ERROR_IO_TIMEOUT)
		return -1;

	cachep = cache;
	cachee = cache + recv;

	if (recv) {
		return (int) *cachep++;
	}

	return -1;
}

/****************************************************************************
 *
 * End of file: serial.c
 *
 ****************************************************************************/
