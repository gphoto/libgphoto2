/*
 * canon.c - Canon protocol "native" operations.
 *
 * Written 1999 by Wolfgang G. Reissnegger and Werner Almesberger
 * Additions 2000 by Philippe Marzouk and Edouard Lafargue
 * USB support, 2000, by Mikael Nyström
 *
 * $Id$
 *
 * This file includes both USB and serial support for the cameras
 * manufactured by Canon. These comprise all (or at least almost all)
 * of the digital models of the IXUS, PowerShot and EOS series.
 *
 * We are working at moving serial and USB specific stuff to serial.c
 * and usb.c, keeping the common protocols/busses support in this
 * file.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#ifdef OS2
#include <db.h>
#endif

#include <gphoto2.h>
#include <gphoto2-port-log.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "../../libgphoto2/exif.h"

#include "usb.h"
#include "util.h"
#include "library.h"
#include "canon.h"
#include "serial.h"


/*
 * does operations on a directory based on the value
 * of action : DIR_CREATE, DIR_REMOVE
 *
 */
int
canon_int_directory_operations (Camera *camera, const char *path, int action)
{
	unsigned char *msg;
	int len, canon_usb_funct;
	char type;

	switch (action) {
		case DIR_CREATE:
			type = 0x5;
			canon_usb_funct = CANON_USB_FUNCTION_MKDIR;
			break;
		case DIR_REMOVE:
			type = 0x6;
			canon_usb_funct = CANON_USB_FUNCTION_RMDIR;
			break;
		default:
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "canon_int_directory_operations: "
					 "Bad operation specified : %i\n", action);
			return GP_ERROR_BAD_PARAMETERS;
			break;
	}

	gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_int_directory_operations() "
			 "called to %s the directory '%s'",
			 canon_usb_funct == CANON_USB_FUNCTION_MKDIR ? "create" : "remove",
			 path);
	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			msg = canon_usb_dialogue (camera, canon_usb_funct, &len, path,
						  strlen (path) + 1);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = canon_serial_dialogue (camera, type, 0x11, &len, path,
						     strlen (path) + 1, NULL);
			break;
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return GP_ERROR;
	}

	if (msg[0] != 0x00) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "Could not %s directory %s",
				 canon_usb_funct ==
				 CANON_USB_FUNCTION_MKDIR ? "create" : "remove", path);
		return GP_ERROR;
	}

	return GP_OK;
}

/**
 * canon_int_identify_camera:
 * @camera: the camera to work with
 * @Returns: gphoto2 error code
 *
 * Gets the camera identification string, usually the owner name.
 *
 * This information is then stored in the "camera" structure, which 
 * is a global variable for the driver.
 *
 * This function also gets the firmware revision in the camera struct.
 **/
int
canon_int_identify_camera (Camera *camera)
{
	unsigned char *msg;
	int len;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_int_identify_camera() called");

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			len = 0x4c;
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_IDENTIFY_CAMERA,
						  &len, NULL, 0);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = canon_serial_dialogue (camera, 0x01, 0x12, &len, NULL);
			break;

	}

	if (!msg) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "canon_int_identify_camera: " "msg error");
		canon_serial_error_type (camera);
		return GP_ERROR;
	}

	/* Store these values in our "camera" structure: */
	memcpy (camera->pl->firmwrev, (char *) msg + 8, 4);
	strncpy (camera->pl->ident, (char *) msg + 12, 30);
	strncpy (camera->pl->owner, (char *) msg + 44, 30);
	gp_debug_printf (GP_DEBUG_HIGH, "canon", "canon_int_identify_camera: "
			 "ident '%s' owner '%s'", camera->pl->ident, camera->pl->owner);

	return GP_OK;
}

/**
 * canon_int_get_battery:
 * @camera: the camera to work on
 * @pwr_status: pointer to integer determining power status
 * @pwr_source: pointer to integer determining power source
 * @Returns: gphoto2 error code
 *
 * Gets battery status.
 **/
int
canon_int_get_battery (Camera *camera, int *pwr_status, int *pwr_source)
{
	unsigned char *msg;
	int len;

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			len = 0x8;
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_POWER_STATUS,
						  &len, NULL, 0);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = canon_serial_dialogue (camera, 0x0a, 0x12, &len, NULL);
			break;
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return GP_ERROR;
	}

	if (pwr_status)
		*pwr_status = msg[4];
	if (pwr_source)
		*pwr_source = msg[7];
	gp_debug_printf (GP_DEBUG_LOW, "canon", "Status: %i / Source: %i\n", *pwr_status,
			 *pwr_source);
	return GP_OK;
}


/**
 * canon_int_set_file_attributes:
 * @camera: camera to work with
 * @file: file to work on
 * @dir: directory to work in
 * @attrs: attribute bit field
 * @Returns: gphoto2 error code
 *
 * Sets a file's attributes. See the 'Protocol' file for details.
 **/
int
canon_int_set_file_attributes (Camera *camera, const char *file, const char *dir,
			       unsigned char attrs)
{
	unsigned char payload[300];
	unsigned char *msg;
	unsigned char attr[4];
	int len, payload_length;

	GP_DEBUG ("canon_int_set_file_attributes() "
		  "called for '%s' '%s', attributes 0x%x", dir, file, attrs);

	attr[0] = attr[1] = attr[2] = 0;
	attr[3] = attrs;

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			if ((4 + strlen (dir) + 1 + strlen (file) + 1) > sizeof (payload)) {
				GP_DEBUG ("canon_int_set_file_attributes: "
					  "dir '%s' + file '%s' too long, "
					  "won't fit in payload buffer.", dir, file);
				return GP_ERROR_BAD_PARAMETERS;
			}
			/* create payload (yes, path and filename are two different strings
			 * and not meant to be concatenated)
			 */
			memset (payload, 0, sizeof (payload));
			memcpy (payload, attr, 4);
			memcpy (payload + 4, dir, strlen (dir) + 1);
			memcpy (payload + 4 + strlen (dir) + 1, file, strlen (file) + 1);
			payload_length = 4 + strlen (dir) + 1 + strlen (file) + 1;
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_SET_ATTR, &len,
						  payload, payload_length);
			if (len == 4) {
				/* XXX check camera return value (not canon_usb_dialogue return value
				 * but the bytes in the packet returned)
				 */
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "canon_int_set_file_attributes: "
						 "returned four bytes as expected, "
						 "we should check if they indicate "
						 "error or not. Returned data :");
				gp_log_data ("canon", msg, 4);
			} else {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "canon_int_set_file_attributes: "
						 "setting attribute failed!");
				return GP_ERROR;
			}

			break;
		case CANON_SERIAL_RS232:
		default:
			msg = canon_serial_dialogue (camera, 0xe, 0x11, &len, attr, 4, dir,
						     strlen (dir) + 1, file, strlen (file) + 1,
						     NULL);
			break;
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return GP_ERROR;
	}

	return GP_OK;
}

/**
 * canon_int_set_owner_name:
 * @camera: the camera to set the owner name of
 * @name: owner name to set the camera to
 *
 * Sets the camera owner name. The string should not be more than 30
 * characters long. We call #get_owner_name afterwards in order to
 * check that everything went fine.
 **/
int
canon_int_set_owner_name (Camera *camera, const char *name)
{
	unsigned char *msg;
	int len;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_int_set_owner_name() "
			 "called, name = '%s'", name);
	if (strlen (name) > 30) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "canon_int_set_owner_name: Name too long (%i chars), "
				 "max is 30 characters!", strlen (name));
		gp_camera_status (camera, _("Name too long, max is 30 characters!"));
		return 0;
	}

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_CAMERA_CHOWN,
						  &len, name, strlen (name) + 1);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = canon_serial_dialogue (camera, 0x05, 0x12, &len, name,
						     strlen (name) + 1, NULL);
			break;
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return GP_ERROR;
	}
	return canon_int_identify_camera (camera);
}


/**
 * canon_int_get_time:
 * @camera: camera to get the current time of
 * @Returns: time of camera (local time)
 *
 * Get camera's current time.
 *
 * The camera gives time in little endian format, therefore we need
 * to swap the 4 bytes on big-endian machines.
 *
 * Nota: the time returned is not GMT but local time. Therefore,
 * if you use functions like "ctime", it will be translated to local
 * time _a second time_, and the result will be wrong. Only use functions
 * that don't translate the date into localtime, like "gmtime".
 **/
time_t
canon_int_get_time (Camera *camera)
{
	unsigned char *msg;
	int len;
	int t;
	time_t date;

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			len = 0x10;
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_GET_TIME, &len,
						  NULL, 0);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = canon_serial_dialogue (camera, 0x03, 0x12, &len, NULL);
			break;
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return GP_ERROR;
	}

	/* XXX will fail when sizeof(int) != 4. Should use u_int32_t or
	 * something instead. Investigate portability issues.
	 */
	memcpy (&t, msg + 4, 4);

	date = (time_t) byteswap32 (t);

	/* XXX should strip \n at the end of asctime() return data */
	gp_debug_printf (GP_DEBUG_HIGH, "canon", "Camera time: %s ", asctime (gmtime (&date)));
	return date;
}


int
canon_int_set_time (Camera *camera)
{
	unsigned char *msg;
	int len, i;
	time_t date;
	char pcdate[4];

	date = time (NULL);
	for (i = 0; i < 4; i++)
		pcdate[i] = (date >> (8 * i)) & 0xff;

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			len = 0x10;
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_SET_TIME, &len,
						  NULL, 0);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = canon_serial_dialogue (camera, 0x04, 0x12, &len, pcdate,
						     sizeof (pcdate),
						     "\x00\x00\x00\x00\x00\x00\x00\x00", 8,
						     NULL);
			break;
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return 0;
	}

	return 1;
}

/**
 * canon_int_serial_ready:
 * @camera: camera to get ready
 * @Returns: gphoto2 error code
 *
 * serial part of canon_int_ready
 **/
int
canon_int_serial_ready (Camera *camera)
{
	unsigned char type, seq;
	int good_ack, speed, try, len;
	char *pkt;
	int res;

	GP_DEBUG ("canon_int_ready()");

	serial_set_timeout (camera->port, 900);	// 1 second is the delay for awakening the camera
	serial_flush_input (camera->port);
	serial_flush_output (camera->port);

	camera->pl->receive_error = NOERROR;

	/* First of all, we must check if the camera is already on */
	/*      cts=canon_serial_get_cts();
	   gp_debug_printf(GP_DEBUG_LOW,"canon","cts : %i\n",cts);
	   if (cts==32) {  CTS == 32  when the camera is connected. */
	if (camera->pl->first_init == 0 && camera->pl->cached_ready == 1) {
		/* First case, the serial speed of the camera is the same as
		 * ours, so let's try to send a ping packet : */
		if (!canon_serial_send_packet
		    (camera, PKT_EOT, camera->pl->seq_tx,
		     camera->pl->psa50_eot + PKT_HDR_LEN, 0))
			return GP_ERROR;
		good_ack = canon_serial_wait_for_ack (camera);
		gp_debug_printf (GP_DEBUG_LOW, "canon", "good_ack = %i\n", good_ack);
		if (good_ack == 0) {
			/* no answer from the camera, let's try
			 * at the speed saved in the settings... */
			speed = camera->pl->speed;
			if (speed != 9600) {
				if (!canon_serial_change_speed (camera->port, speed)) {
					gp_camera_status (camera, _("Error changing speed."));
					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "speed changed.\n");
				}
			}
			if (!canon_serial_send_packet
			    (camera, PKT_EOT, camera->pl->seq_tx,
			     camera->pl->psa50_eot + PKT_HDR_LEN, 0))
				return GP_ERROR;
			good_ack = canon_serial_wait_for_ack (camera);
			if (good_ack == 0) {
				gp_camera_status (camera, _("Resetting protocol..."));
				canon_serial_off (camera);
				sleep (3);	/* The camera takes a while to switch off */
				return canon_int_ready (camera);
			}
			if (good_ack == -1) {
				gp_debug_printf (GP_DEBUG_LOW, "canon", "Received a NACK !\n");
				return GP_ERROR;
			}
			gp_camera_status (camera, _("Camera OK.\n"));
			return 1;
		}
		if (good_ack == -1) {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "Received a NACK !\n");
			return GP_ERROR;
		}
		gp_debug_printf (GP_DEBUG_LOW, "canon", "Camera replied to ping, proceed.\n");
		return GP_OK;
	}

	/* Camera was off... */

	gp_camera_status (camera, _("Looking for camera ..."));
	gp_camera_progress (camera, 0);
	if (camera->pl->receive_error == FATAL_ERROR) {
		/* we try to recover from an error
		   we go back to 9600bps */
		if (!canon_serial_change_speed (camera->port, 9600)) {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: Error changing speed");
			return GP_ERROR;
		}
		camera->pl->receive_error = NOERROR;
	}
	for (try = 1; try < MAX_TRIES; try++) {
		gp_camera_progress (camera, (try / (float) MAX_TRIES));
		if (canon_serial_send
		    (camera, "\x55\x55\x55\x55\x55\x55\x55\x55", 8, USLEEP1) < 0) {
			gp_camera_status (camera, _("Communication error 1"));
			return GP_ERROR;
		}
		pkt = canon_serial_recv_frame (camera, &len);
		if (pkt)
			break;
	}
	if (try == MAX_TRIES) {
		gp_camera_status (camera, _("No response from camera"));
		return GP_ERROR;
	}
	if (!pkt) {
		gp_camera_status (camera, _("No response from camera"));
		return GP_ERROR;
	}
	if (len < 40 && strncmp (pkt + 26, "Canon", 5)) {
		gp_camera_status (camera, _("Unrecognized response"));
		return GP_ERROR;
	}
	strncpy (camera->pl->psa50_id, pkt + 26, sizeof (camera->pl->psa50_id) - 1);

	GP_DEBUG ("psa50_id : '%s'", camera->pl->psa50_id);

	camera->pl->first_init = 0;

	if (!strcmp ("DE300 Canon Inc.", camera->pl->psa50_id)) {
		gp_camera_status (camera, "PowerShot A5");
		camera->pl->model = CANON_PS_A5;
		if (camera->pl->speed > 57600)
			camera->pl->slow_send = 1;
		camera->pl->A5 = 1;
	} else if (!strcmp ("Canon PowerShot A5 Zoom", camera->pl->psa50_id)) {
		gp_camera_status (camera, "PowerShot A5 Zoom");
		camera->pl->model = CANON_PS_A5_ZOOM;
		if (camera->pl->speed > 57600)
			camera->pl->slow_send = 1;
		camera->pl->A5 = 1;
	} else if (!strcmp ("Canon PowerShot A50", camera->pl->psa50_id)) {
		gp_camera_status (camera, "Detected a PowerShot A50");
		camera->pl->model = CANON_PS_A50;
		if (camera->pl->speed > 57600)
			camera->pl->slow_send = 1;
	} else if (!strcmp ("Canon PowerShot S20", camera->pl->psa50_id)) {
		gp_camera_status (camera, "Detected a PowerShot S20");
		camera->pl->model = CANON_PS_S20;
	} else if (!strcmp ("Canon PowerShot G1", camera->pl->psa50_id)) {
		gp_camera_status (camera, "Detected a PowerShot G1");
		camera->pl->model = CANON_PS_G1;
	} else if (!strcmp ("Canon PowerShot A10", camera->pl->psa50_id)) {
		gp_camera_status (camera, "Detected a PowerShot A10");
		camera->pl->model = CANON_PS_A10;
	} else if (!strcmp ("Canon PowerShot A20", camera->pl->psa50_id)) {
		gp_camera_status (camera, "Detected a PowerShot A20");
		camera->pl->model = CANON_PS_A20;
	} else if (!strcmp ("Canon EOS D30", camera->pl->psa50_id)) {
		gp_camera_status (camera, "Detected a EOS D30");
		camera->pl->model = CANON_EOS_D30;
	} else if (!strcmp ("Canon PowerShot Pro90 IS", camera->pl->psa50_id)) {
		gp_camera_status (camera, "Detected a PowerShot Pro90 IS");
		camera->pl->model = CANON_PS_PRO90_IS;
	} else if (!strcmp ("Canon PowerShot Pro70", camera->pl->psa50_id)) {
		gp_camera_status (camera, "Detected a PowerShot Pro70");
		camera->pl->model = CANON_PS_A70;
	} else if ((!strcmp ("Canon DIGITAL IXUS", camera->pl->psa50_id))
		   || (!strcmp ("Canon IXY DIGITAL", camera->pl->psa50_id))
		   || (!strcmp ("Canon PowerShot S100", camera->pl->psa50_id))
		   || (!strcmp ("Canon DIGITAL IXUS v", camera->pl->psa50_id))) {
		gp_camera_status (camera,
				  "Detected a Digital IXUS series / IXY DIGITAL / PowerShot S100 series");
		camera->pl->model = CANON_PS_S100;
	} else if ((!strcmp ("Canon DIGITAL IXUS 300", camera->pl->psa50_id))
		   || (!strcmp ("Canon IXY DIGITAL 300", camera->pl->psa50_id))
		   || (!strcmp ("Canon PowerShot S300", camera->pl->psa50_id))) {
		gp_camera_status (camera,
				  "Detected a Digital IXUS 300 / IXY DIGITAL 300 / PowerShot S300");
		camera->pl->model = CANON_PS_S300;
	} else {
		gp_camera_status (camera, "Detected a PowerShot S10");
		camera->pl->model = CANON_PS_S10;
	}

	//  5 seconds  delay should  be enough for   big flash cards.   By
	// experience, one or two seconds is too  little, as a large flash
	// card needs more access time.
	serial_set_timeout (camera->port, 5000);
	(void) canon_serial_recv_packet (camera, &type, &seq, NULL);
	if (type != PKT_EOT || seq) {
		gp_camera_status (camera, _("Bad EOT"));
		return GP_ERROR;
	}
	camera->pl->seq_tx = 0;
	camera->pl->seq_rx = 1;
	if (!canon_serial_send_frame (camera, "\x00\x05\x00\x00\x00\x00\xdb\xd1", 8)) {
		gp_camera_status (camera, _("Communication error 2"));
		return GP_ERROR;
	}
	res = 0;
	switch (camera->pl->speed) {
		case 9600:
			res = canon_serial_send_frame (camera, SPEED_9600, 12);
			break;
		case 19200:
			res = canon_serial_send_frame (camera, SPEED_19200, 12);
			break;
		case 38400:
			res = canon_serial_send_frame (camera, SPEED_38400, 12);
			break;
		case 57600:
			res = canon_serial_send_frame (camera, SPEED_57600, 12);
			break;
		case 115200:
			res = canon_serial_send_frame (camera, SPEED_115200, 12);
			break;
	}

	if (!res || !canon_serial_send_frame (camera, "\x00\x04\x01\x00\x00\x00\x24\xc6", 8)) {
		gp_camera_status (camera, _("Communication error 3"));
		return GP_ERROR;
	}
	speed = camera->pl->speed;
	gp_camera_status (camera, _("Changing speed... wait..."));
	if (!canon_serial_wait_for_ack (camera))
		return GP_ERROR;
	if (speed != 9600) {
		if (!canon_serial_change_speed (camera->port, speed)) {
			gp_camera_status (camera, _("Error changing speed"));
			gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: Error changing speed");
		} else {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "speed changed\n");
		}

	}
	for (try = 1; try < MAX_TRIES; try++) {
		canon_serial_send_packet (camera, PKT_EOT, camera->pl->seq_tx,
					  camera->pl->psa50_eot + PKT_HDR_LEN, 0);
		if (!canon_serial_wait_for_ack (camera)) {
			gp_camera_status (camera,
					  _
					  ("Error waiting ACK during initialization retrying"));
		} else
			break;
	}

	if (try == MAX_TRIES) {
		gp_camera_status (camera, _("Error waiting ACK during initialization"));
		return GP_ERROR;
	}

	gp_camera_status (camera, _("Connected to camera"));
	/* Now is a good time to ask the camera for its owner
	 * name (and Model String as well)  */
	canon_int_identify_camera (camera);
	canon_int_get_time (camera);

	return GP_OK;
}

/**
 * canon_int_usb_ready:
 * @camera: camera to get ready
 * @Returns: gphoto2 error code
 *
 * USB part of canon_int_ready
 **/
int
canon_int_usb_ready (Camera *camera)
{
	int res;

	GP_DEBUG ("canon_int_usb_ready()");

	res = canon_int_identify_camera (camera);
	if (res != GP_OK) {
		gp_camera_set_error (camera, "Camera not ready, "
				  "identify camera request failed (returned %i)", res);
		return GP_ERROR;
	}
	if (!strcmp ("Canon PowerShot S20", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot S20");
		camera->pl->model = CANON_PS_S20;
	} else if (!strcmp ("Canon PowerShot S10", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot S10");
		camera->pl->model = CANON_PS_S10;
	} else if (!strcmp ("Canon PowerShot S30", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot S30");
		camera->pl->model = CANON_PS_S30;
	} else if (!strcmp ("Canon PowerShot S40", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot S40");
		camera->pl->model = CANON_PS_S40;
	} else if (!strcmp ("Canon PowerShot G1", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot G1");
		camera->pl->model = CANON_PS_G1;
	} else if (!strcmp ("Canon PowerShot G2", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot G2");
		camera->pl->model = CANON_PS_G2;
	} else if ((!strcmp ("Canon DIGITAL IXUS", camera->pl->ident))
		   || (!strcmp ("Canon IXY DIGITAL", camera->pl->ident))
		   || (!strcmp ("Canon PowerShot S110", camera->pl->ident))
		   || (!strcmp ("Canon PowerShot S100", camera->pl->ident))
		   || (!strcmp ("Canon DIGITAL IXUS v", camera->pl->ident))) {
		gp_camera_status (camera,
				  "Detected a Digital IXUS series / IXY DIGITAL / PowerShot S100 series");
		camera->pl->model = CANON_PS_S100;
	} else if ((!strcmp ("Canon DIGITAL IXUS 300", camera->pl->ident))
		   || (!strcmp ("Canon IXY DIGITAL 300", camera->pl->ident))
		   || (!strcmp ("Canon PowerShot S300", camera->pl->ident))) {
		gp_camera_status (camera,
				  "Detected a Digital IXUS 300 / IXY DIGITAL 300 / PowerShot S300");
		camera->pl->model = CANON_PS_S300;
	} else if (!strcmp ("Canon PowerShot A10", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot A10");
		camera->pl->model = CANON_PS_A10;
	} else if (!strcmp ("Canon PowerShot A20", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot A20");
		camera->pl->model = CANON_PS_A20;
	} else if (!strcmp ("Canon EOS D30", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a EOS D30");
		camera->pl->model = CANON_EOS_D30;
	} else if (!strcmp ("Canon PowerShot Pro90 IS", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot Pro90 IS");
		camera->pl->model = CANON_PS_PRO90_IS;
	} else {
		gp_camera_set_error (camera, "Unknown camera! (%s)", camera->pl->ident);
		return GP_ERROR;
	}

	res = canon_usb_keylock (camera);
	if (res != GP_OK) {
		gp_camera_set_error (camera, "Camera not ready, "
				     "could not lock camera keys (returned %i)",
				     res);
		return res;
	}

	res = canon_int_get_time (camera);
	if (res == GP_ERROR) {
		gp_camera_set_error (camera, "Camera not ready, "
				  "get time request failed (returned %i)", res);
		return GP_ERROR;
	}

	gp_camera_status (camera, _("Connected to camera"));

	return GP_OK;
}

/**
 * canon_int_ready:
 * @camera: camera to get ready
 * @Returns: gphoto2 error code
 *
 * Switches the camera on, detects the model and sets its speed.
 **/
int
canon_int_ready (Camera *camera)
{
	int res;

	GP_DEBUG ("canon_int_ready()");

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			res = canon_int_usb_ready (camera);
			break;
		case CANON_SERIAL_RS232:
			res = canon_int_serial_ready (camera);
			break;
		default:
			gp_camera_set_error (camera,
					     "Unknown canon_comm_method in canon_int_ready()");
			res = GP_ERROR;
			break;
	}
	
	return (res);
}

/**
 * canon_int_get_disk_name:
 * @camera: camera to ask for disk drive
 * @Returns: name of disk
 *
 * Ask the camera for the name of the flash storage
 * device. Usually "D:" or something like that.
 **/
char *
canon_int_get_disk_name (Camera *camera)
{
	unsigned char *msg;
	int len, res;

	GP_DEBUG ("canon_int_get_disk_name()");

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			res = canon_usb_long_dialogue (camera,
						       CANON_USB_FUNCTION_FLASH_DEVICE_IDENT,
						       &msg, &len, 1024, NULL, 0, 0);
			if (res != GP_OK) {
				GP_DEBUG ("canon_int_get_disk_name: canon_usb_long_dialogue "
					  "failed! returned %i", res);
				return NULL;
			}
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = canon_serial_dialogue (camera, 0x0a, 0x11, &len, NULL);
			break;
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return NULL;
	}
	if (camera->pl->canon_comm_method == CANON_SERIAL_RS232) {
		/* this is correct even though it looks a bit funny. canon_serial_dialogue()
		 * has a static buffer, strdup() part of that buffer and return to our caller.
		 */
		msg = strdup ((char *) msg + 4);	/* @@@ should check length */
		if (!msg) {
			GP_DEBUG ("canon_int_get_disk_name: could not allocate %i "
				  "bytes of memory to hold response",
				  strlen ((char *) msg + 4));
			return NULL;
		}
	}

	GP_DEBUG ("canon_int_get_disk_name: disk '%s'", msg);

	return msg;
}

/**
 * canon_int_get_disk_name_info:
 * @camera: camera to ask about disk
 * @name: name of the disk
 * @capacity: returned maximum disk capacity
 * @available: returned currently available disk capacity
 * @Returns: boolean value denoting success (FIXME: ATTENTION!)
 *
 * Gets available room and max capacity of a disk given by @name.
 **/
int
canon_int_get_disk_name_info (Camera *camera, const char *name, int *capacity, int *available)
{
	unsigned char *msg;
	int len, cap, ava;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_int_get_disk_name_info() name '%s'",
			 name);

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_DISK_INFO, &len,
						  name, strlen (name) + 1);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = canon_serial_dialogue (camera, 0x09, 0x11, &len, name,
						     strlen (name) + 1, NULL);
			break;
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return 0;	/* FALSE */
	}
	if (len < 12) {
		GP_DEBUG ("ERROR: truncated message");
		return 0;	/* FALSE */
	}
	cap = get_int (msg + 4);
	ava = get_int (msg + 8);
	if (capacity)
		*capacity = cap;
	if (available)
		*available = ava;

	GP_DEBUG ("canon_int_get_disk_name_info: capacity %i kb, available %i kb",
		  cap > 0 ? (cap / 1024) : 0, ava > 0 ? (ava / 1024) : 0);

	return 1;		/* TRUE */
}


/**
 * gphoto2canonpath:
 * @path: gphoto2 path 
 *
 * convert gphoto2 path  (e.g.   "/DCIM/116CANON/IMG_1240.JPG")
 * into canon style path (e.g. "D:\DCIM\116CANON\IMG_1240.JPG")
 */
char *gphoto2canonpath(char *path)
{
	static char tmp[2000];
	char *p;
	if (path[0] != '/') {
		return NULL;
	}
	strcpy(tmp, "D:"); // FIXME
	strcpy(tmp + 2, path);
	for (p = tmp + 2; *p != '\0'; p++) {
		if (*p == '/')
			*p = '\\';
	}
	return(tmp);
}

/**
 * cancon2gphotopath:
 * @path: canon style path
 *
 * convert canon style path (e.g. "D:\DCIM\116CANON\IMG_1240.JPG")
 * into gphoto2 path        (e.g.   "/DCIM/116CANON/IMG_1240.JPG")
 */
char *canon2gphotopath(char *path)
{
	static char tmp[2000];
	char *p;
	if (!((path[1] == ':') && (path[2] == '\\'))){
		return NULL;
	}
	// FIXME: just drops the drive letter
	p = path + 2;
	strcpy(tmp,p);
	for (p = tmp; *p != '\0'; p++) {
		if (*p == '\\')
			*p = '/';
	}
	return(tmp);
}



void
canon_int_free_dir (Camera *camera, struct canon_dir *list)
{
	struct canon_dir *walk;

	for (walk = list; walk->name; walk++)
		free ((char *) walk->name);
	free (list);
}

/*
 * Get the directory tree of a given flash device.
 */
struct canon_dir *
canon_int_list_directory (Camera *camera, const char *name)
{
	struct canon_dir *dir = NULL;
	int entries = 0, len, res;
	unsigned int payload_length;
	unsigned char *data, *p, *end_of_data;
	char attributes;
	unsigned char payload[100];

	/* Ask the camera for a full directory listing */
	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			/* build payload :
			 *
			 * 0x00 dirname 0x00 0x00 0x00
			 *
			 * the 0x00 before dirname means 'no recursion'
			 * NOTE: the first 0x00 after dirname is the NULL byte terminating
			 * the string, so payload_length is strlen(name) + 4 
			 */
			if (strlen (name) + 4 > sizeof (payload)) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "canon_int_list_directory: "
						 "Name '%s' too long (%i), won't fit in payload "
						 "buffer.", name, strlen (name));
				return NULL;	// XXX for now
				//return GP_ERROR_BAD_PARAMETERS;
			}
			memset (payload, 0x00, sizeof (payload));
			memcpy (payload + 1, name, strlen (name));
			payload_length = strlen (name) + 4;

			/* 1024 * 1024 is max realistic data size, out of the blue.
			 * 0 is to not show progress status
			 */
			res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_DIRENT,
						       &data, &len, 1024 * 1024, payload,
						       payload_length, 0);
			if (res != GP_OK) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "canon_int_list_directory: "
						 "canon_usb_long_dialogue() failed to fetch direntrys, "
						 "returned %i", res);
				return NULL;
			}
			break;
		case CANON_SERIAL_RS232:
		default:
			data = canon_serial_dialogue (camera, 0xb, 0x11, &len, "", 1, name,
						      strlen (name) + 1, "\x00", 2, NULL);
			break;
	}

	if (!data) {
		canon_serial_error_type (camera);
		return NULL;
	}
	if (len < 16) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: truncated message\n");
		return NULL;
	}

	end_of_data = data + len;
	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			p = data - 1;
			if (p >= end_of_data)
				goto error;
			break;
		case CANON_SERIAL_RS232:
		default:
			if (!data[5])
				return NULL;
			p = data + 15;
			if (p >= end_of_data)
				goto error;
			for (; *p; p++)
				if (p >= end_of_data)
					goto error;
			break;
	}

	/* This is the main loop, for every entry in the structure */
	while (p[0xb] != 0x00) {
		unsigned char *start;
		int is_file;

		//fprintf(stderr,"p %p end_of_data %p len %d\n",p,end_of_data,len);
		if (p == end_of_data - 1) {
			if (data[4])
				break;
			if (camera->pl->canon_comm_method == CANON_SERIAL_RS232)
				data = canon_serial_recv_msg (camera, 0xb, 0x21, &len);
			else
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "USB Driver: this message (%s line %i) "
						 "should never be printed", __FILE__,
						 __LINE__);
			if (!data)
				goto error;
			if (len < 5)
				goto error;
			p = data + 4;
		}
		if (p + 2 >= end_of_data)
			goto error;
		attributes = p[1];
		is_file = !((p[1] & 0x10) == 0x10);
		if (p[1] == 0x10 || is_file)
			p += 11;
		else
			break;
		if (p >= end_of_data)
			goto error;
		for (start = p; *p; p++)
			if (p >= end_of_data)
				goto error;
		dir = realloc (dir, sizeof (*dir) * (entries + 2));
		if (!dir) {
			perror ("realloc");
			exit (1);
		}
		dir[entries].name = strdup (start);
		if (!dir[entries].name) {
			perror ("strdup");
			exit (1);
		}

		memcpy ((unsigned char *) &dir[entries].size, start - 8, 4);
		dir[entries].size = byteswap32 (dir[entries].size);	/* re-order little/big endian */
		memcpy ((unsigned char *) &dir[entries].date, start - 4, 4);
		dir[entries].date = byteswap32 (dir[entries].date);	/* re-order little/big endian */
		dir[entries].is_file = is_file;
		dir[entries].attrs = attributes;
		// Every directory contains a "null" file entry, so we skip it
		if (strlen (dir[entries].name) > 0) {
			// Debug output:
			if (is_file)
				gp_debug_printf (GP_DEBUG_LOW, "canon", "-");
			else
				gp_debug_printf (GP_DEBUG_LOW, "canon", "d");
			if ((dir[entries].attrs & 0x1) == 0x1)
				gp_debug_printf (GP_DEBUG_LOW, "canon", "r- ");
			else
				gp_debug_printf (GP_DEBUG_LOW, "canon", "rw ");
			if ((dir[entries].attrs & 0x20) == 0x20)
				gp_debug_printf (GP_DEBUG_LOW, "canon", "  new   ");
			else
				gp_debug_printf (GP_DEBUG_LOW, "canon", "saved   ");
			gp_debug_printf (GP_DEBUG_LOW, "canon", "%#2x - %8i %.24s %s\n",
					 dir[entries].attrs, dir[entries].size,
					 asctime (gmtime (&dir[entries].date)),
					 dir[entries].name);
			entries++;
		}
	}
	if (camera->pl->canon_comm_method == CANON_USB)
		free (data);
	if (dir)
		dir[entries].name = NULL;
	return dir;
      error:
	gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: truncated message\n");
	if (dir)
		canon_int_free_dir (camera, dir);
	return NULL;
}


int
canon_int_get_file (Camera *camera, const char *name, unsigned char **data, int *length)
{
	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			return canon_usb_get_file (camera, name, data, length);
			break;
		case CANON_SERIAL_RS232:
		default:
			*data = canon_serial_get_file (camera, name, length);
			if (*data)
				return GP_OK;
			return GP_ERROR;
			break;
	}
}

/**
 * canon_int_get_thumbnail:
 * @camera: camera to work with
 * @name: image to get thumbnail of
 * @length: length of data returned
 *
 * Returns the thumbnail data of the picture designated by @name.
 **/
unsigned char *
canon_int_get_thumbnail (Camera *camera, const char *name, int *length)
{
	unsigned char *data = NULL;
	unsigned char *msg;
	exifparser exifdat;
	unsigned int total = 0, expect = 0, size, payload_length, total_file_size;
	int i, j, in;
	unsigned char *thumb;

	GP_DEBUG ("canon_int_get_thumbnail() called for file '%s'", name);

	gp_camera_progress (camera, 0);
	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			i = canon_usb_get_thumbnail (camera, name, &data, length);
			if (i != GP_OK) {
				GP_DEBUG ("canon_usb_get_thumbnail() failed, "
					  "returned %i", i);
				return NULL;	// XXX for now
			}
			break;
		case CANON_SERIAL_RS232:
		default:
			if (camera->pl->receive_error == FATAL_ERROR) {
				GP_DEBUG ("ERROR: can't continue a fatal "
					  "error condition detected");
				return NULL;
			}

			payload_length = strlen (name) + 1;
			msg = canon_serial_dialogue (camera, 0x1, 0x11, &total_file_size,
						     "\x01\x00\x00\x00\x00", 5,
						     &payload_length, 1, "\x00", 2,
						     name, strlen (name) + 1, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return NULL;
			}

			total = get_int (msg + 4);
			if (total > 2000000) {	/* 2 MB thumbnails ? unlikely ... */
				GP_DEBUG ("ERROR: %d is too big", total);
				return NULL;
			}
			data = malloc (total);
			if (!data) {
				perror ("malloc");
				return NULL;
			}
			if (length)
				*length = total;

			while (msg) {
				if (total_file_size < 20 || get_int (msg)) {
					return NULL;
				}
				size = get_int (msg + 12);
				if (get_int (msg + 8) != expect || expect + size > total
				    || size > total_file_size - 20) {
					GP_DEBUG ("ERROR: doesn't fit");
					return NULL;
				}
				memcpy (data + expect, msg + 20, size);
				expect += size;
				gp_camera_progress (camera,
						    total ? (expect / (float) total) : 1.);
				if ((expect == total) != get_int (msg + 16)) {
					GP_DEBUG ("ERROR: end mark != end of data");
					return NULL;
				}
				if (expect == total) {
					/* We finished receiving the file. Parse the header and
					   return just the thumbnail */
					break;
				}
				msg = canon_serial_recv_msg (camera, 0x1, 0x21,
							     &total_file_size);
			}
			break;
	}

	switch (camera->pl->model) {
		case CANON_PS_A70:	/* pictures are JFIF files */
			/* we skip the first FF D8 */
			i = 3;
			j = 0;
			in = 0;

			/* we want to drop the header to get the thumbnail */

			thumb = malloc (total);
			if (!thumb) {
				perror ("malloc");
				break;
			}

			while (i < total) {
				if (data[i] == JPEG_ESC) {
					if (data[i + 1] == JPEG_BEG &&
					    ((data[i + 3] == JPEG_SOS)
					     || (data[i + 3] == JPEG_A50_SOS))) {
						in = 1;
					} else if (data[i + 1] == JPEG_END) {
						in = 0;
						thumb[j++] = data[i];
						thumb[j] = data[i + 1];
						return thumb;
					}
				}

				if (in == 1)
					thumb[j++] = data[i];
				i++;

			}
			return NULL;
			break;

		default:	/* Camera supports EXIF */
			exifdat.header = data;
			exifdat.data = data + 12;

			GP_DEBUG ("Got thumbnail, extracting it with the " "EXIF lib.");
			if (exif_parse_data (&exifdat) > 0) {
				GP_DEBUG ("Parsed exif data.");
				data = exif_get_thumbnail (&exifdat);	// Extract Thumbnail
				if (data == NULL) {
					int f;
					char fn[255];

					if (rindex (name, '\\') != NULL)
						snprintf (fn, sizeof (fn) - 1,
							  "canon-death-dump.dat-%s",
							  rindex (name, '\\') + 1);
					else
						snprintf (fn, sizeof (fn) - 1,
							  "canon-death-dump.dat-%s", name);
					fn[sizeof (fn) - 1] = 0;

					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "canon_int_get_thumbnail: "
							 "Thumbnail conversion error, saving "
							 "%i bytes to '%s'", total, fn);
					/* create with O_EXCL and 0600 for security */
					if ((f =
					     open (fn, O_CREAT | O_EXCL | O_RDWR,
						   0600)) == -1) {
						gp_debug_printf (GP_DEBUG_LOW, "canon",
								 "canon_int_get_thumbnail: "
								 "error creating '%s': %m",
								 fn);
						break;
					}
					if (write (f, data, total) == -1) {
						gp_debug_printf (GP_DEBUG_LOW, "canon",
								 "canon_int_get_thumbnail: "
								 "error writing to file '%s': %m",
								 fn);
					}

					close (f);
					break;
				}
				return data;
			}
			break;
	}

	free (data);
	return NULL;
}

int
canon_int_delete_file (Camera *camera, const char *name, const char *dir)
{
	unsigned char payload[300];
	unsigned char *msg;
	int len, payload_length;

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			memcpy (payload, dir, strlen (dir) + 1);
			memcpy (payload + strlen (dir) + 1, name, strlen (name) + 1);
			payload_length = strlen (dir) + strlen (name) + 2;
			len = 0x4;
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_DELETE_FILE, &len,
						  payload, payload_length);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = canon_serial_dialogue (camera, 0xd, 0x11, &len, dir,
						     strlen (dir) + 1, name, strlen (name) + 1,
						     NULL);
			break;
	}
	if (!msg) {
		canon_serial_error_type (camera);
		return -1;
	}
	if (msg[0] == 0x29) {
		gp_camera_message (camera, _("File protected"));
		return -1;
	}

	return 0;
}

/*
 * Upload a file to the camera
 *
 */
int
canon_int_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath)
{

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			return canon_usb_put_file (camera, file, destname, destpath);
			break;
		case CANON_SERIAL_RS232:
		default:
			return canon_serial_put_file (camera, file, destname, destpath);
			break;
	}
}
