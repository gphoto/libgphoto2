/* library.c
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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
#include <config.h>
#include "library.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include <gphoto2-port-log.h>

#include "sierra.h"
#include "sierra-usbwrap.h"

#define GP_MODULE "sierra"

/* Short comm. */
#define NUL             0x00
#define ENQ             0x05
#define ACK             0x06
#define DC1             0x11
#define NAK             0x15
#define TRM             0xff

/* Packet types */
#define TYPE_COMMAND    0x1b
#define TYPE_DATA       0x02
#define TYPE_DATA_END   0x03

/* Sub-types */
#define SUBTYPE_COMMAND_FIRST   0x53
#define SUBTYPE_COMMAND         0x43

/* Maximum length of the data field within a packet */
#define MAX_DATA_FIELD_LENGTH   2048

#define RETRIES                 10

#define		QUICKSLEEP	5

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
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define GP_MODULE "sierra"

/**
 * sierra_valid_type:
 * @b : first byte of the packet to be checked
 *
 * Check if the type of the received packet is known.
 * Method: check if the byte is one of NUL, ENQ, ACK, DC1, NAK, TRM,
 * TYPE_COMMAND, TYPE_DATA, TYPE_DATA_END.
 *
 * Returns: GP_OK if the byte is known, GP_ERROR_CORRUPTED_DATA otherwise
 */
static int sierra_valid_type (char b) 
{
	unsigned char byte = (unsigned char) b;
	int ret_status = GP_ERROR_CORRUPTED_DATA;

	if ((byte == NUL) ||
	    (byte == ENQ) ||
	    (byte == ACK) ||
	    (byte == DC1) ||
	    (byte == NAK) ||
	    (byte == TRM) ||
	    (byte == TYPE_COMMAND) ||
	    (byte == TYPE_DATA) ||
	    (byte == TYPE_DATA_END))
		ret_status = GP_OK;

	return ret_status;
}

int sierra_change_folder (Camera *camera, const char *folder, GPContext *context)
{	
	int st = 0,i = 1;
	char target[128];

	GP_DEBUG ("*** sierra_change_folder");
	GP_DEBUG ("*** folder: %s", folder);

	/*
	 * Do not issue the command if the camera doesn't support folders 
	 * or if the folder is the current working folder
	 */
	if (!camera->pl->folders || !strcmp (camera->pl->folder, folder))
		return GP_OK;

	if (folder[0]) {
		strncpy(target, folder, sizeof(target)-1);
		target[sizeof(target)-1]='\0';
		if (target[strlen(target)-1] != '/') strcat(target, "/");
	} else
		strcpy(target, "/");

	if (target[0] != '/')
		i = 0;
	else {
		CHECK (sierra_set_string_register (camera, 84, "\\", 1, context));
	}
	st = i;
	for (; target[i]; i++) {
		if (target[i] == '/') {
			target[i] = '\0';
			if (st == i - 1)
				break;
			CHECK (sierra_set_string_register (camera, 84, 
							   target + st, strlen (target + st), context));

			st = i + 1;
			target[i] = '/';
		}
	}
	strcpy (camera->pl->folder, folder);

	return GP_OK;
}

int sierra_list_files (Camera *camera, const char *folder, CameraList *list, GPContext *context)
{
	int count, i, len = 0;
	char filename[1024];

	/* We need to change to the folder first */
	CHECK (sierra_change_folder (camera, folder, context));

	/* Then, we count the files */
	GP_DEBUG ("Counting files in '%s'...", folder);
	CHECK (sierra_get_int_register (camera, 10, &count, context));
	GP_DEBUG ("... done. Found %i files.", count);

	/*
	 * Finally, we get the filename of each file. Not that some cameras
	 * that don't support filenames return 8 blanks instead of 
	 * reporting an error.
	 */
	for (i = 0; i < count; i++) {
		GP_DEBUG ("Getting filename of file %i...", i + 1);
		CHECK (sierra_set_int_register (camera, 4, i + 1, context));
		CHECK (sierra_get_string_register (camera, 79, 0, NULL,
						   filename, &len, context));
		if ((len <= 0) || !strcmp (filename, "        "))
			snprintf (filename, sizeof (filename),
				  "P101%04i.JPG", i + 1);
		GP_DEBUG ("... done ('%s').", filename);
		CHECK (gp_list_append (list, filename, NULL));
	}

	return (GP_OK);
}

int sierra_list_folders (Camera *camera, const char *folder, CameraList *list,
			 GPContext *context)
{
	int i, j, count, bsize;
	char buf[1024];

	/* List the folders only if the camera supports them */
	if (!camera->pl->folders)
		return (GP_OK);

	CHECK (sierra_change_folder (camera, folder, context));
	GP_DEBUG ("*** counting folders in '%s'...", folder);
	CHECK (sierra_get_int_register (camera, 83, &count, context));
	GP_DEBUG ("*** found %i folders", count);
	for (i = 0; i < count; i++) {
		CHECK (sierra_change_folder (camera, folder, context));
		CHECK (sierra_set_int_register (camera, 83, i + 1, context));
		bsize = 1024;
		GP_DEBUG ("*** getting name of folder %i...", i + 1);
		CHECK (sierra_get_string_register (camera, 84, 0, 
						   NULL, buf, &bsize, context));

		/* Remove trailing spaces */
		for (j = strlen (buf) - 1; j >= 0 && buf[j] == ' '; j--)
			buf[j] = '\0';
		gp_list_append (list, buf, NULL);
	}

	return (GP_OK);
}

/**
 * sierra_get_picture_folder:
 * @camera : camera data stucture
 * @folder : folder name (to be freed by the caller)
 *     
 * Return the name of the folder that stores pictures. It is assumed that
 * the camera conforms with the JEIDA standard. Thus, look for the picture
 * folder into the /DCIM directory.
 *
 * Returns: a gphoto2 error code
 */
int sierra_get_picture_folder (Camera *camera, char **folder)
{
	int i;
	CameraList *list;
	const char *name = NULL;

	GP_DEBUG ("* sierra_get_picture_folder");

	*folder = NULL;

	/* If the camera does not support folders, the picture
	   folder is the root folder */
	if (!camera->pl->folders) {
		*folder = (char*) calloc(2, sizeof(char));
		strcpy(*folder, "/");
		return (GP_OK);
	}

	/* It is assumed that the camera conforms with the JEIDA standard .
	   Thus, look for the picture folder into the /DCIM directory */
	CHECK( gp_list_new(&list) );
	CHECK( gp_filesystem_list_folders(camera->fs, "/DCIM", list, NULL) );
	for(i=0 ; i < gp_list_count(list) ; i++) {
		CHECK( gp_list_get_name(list, i, &name) );
		GP_DEBUG ("* check folder %s", name);
		if (isdigit(name[0]) && isdigit(name[1]) && isdigit(name[2]))
			break;
		name = NULL;
	}
	//CHECK( gp_list_free(list) );

	if (name) {
		*folder = (char*) calloc(strlen(name)+7, sizeof(char));
		strcpy(*folder, "/DCIM/");
		strcat(*folder, name);
		return (GP_OK);
	}
	else
		return GP_ERROR_DIRECTORY_NOT_FOUND;
}

/**
 * sierra_check_battery_capacity:
 * @camera : camera data stucture
 *     
 * Check if the battery capacity is high enough.
 *
 * Returns: a gphoto2 error code
 */
int sierra_check_battery_capacity (Camera *camera, GPContext *context)
{
	int ret, capacity;

	GP_DEBUG ("* sierra_check_battery_capacity");

	if ((ret = sierra_get_int_register(camera, 16, &capacity, context)) != GP_OK) {
		gp_context_error (context,
				     _("Cannot retrieve the battery capacity"));
		return ret;
	}

	if (capacity < 5) {
		gp_context_error (context,
				     _("The battery level of the camera is too low (%d%%). "
				       "The operation is aborted."), capacity);
		return GP_ERROR;
	}

	return GP_OK;
}

/**
 * sierra_get_memory_left:
 * @camera : camera data stucture
 * @memory : memory left
 *     
 * Provide the available memory left 
 *
 * Returns: a gphoto2 error code
 */
int
sierra_get_memory_left (Camera *camera, int *memory, GPContext *context)
{
	int ret;

	if ((ret = sierra_get_int_register(camera, 28, memory, context)) != GP_OK) {
		gp_context_error (context,
				     _("Cannot retrieve the available memory left"));
		return ret;
	}

	return GP_OK;
}

static int
sierra_write_packet (Camera *camera, char *packet)
{
	int x, ret, r, checksum=0, length;

	GP_DEBUG ("* sierra_write_packet");

	/* Determing packet length */
	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_DATA) ||
	    (packet[0] == TYPE_DATA_END)) {
		length = ((unsigned char)packet[2]) +
			((unsigned char)packet[3]  * 256);
		length += 6;
	} else {
		length = 1;
	}

	/* Generate the checksum */
	if (length > 1) {
		for (x = 4; x < length - 2; x++)
			checksum += (unsigned char)packet[x];
		packet[length-2] = checksum & 0xff;
		packet[length-1] = (checksum >> 8) & 0xff; 
	}

	/* USB */
	if (camera->port->type == GP_PORT_USB) {
		if (camera->pl->usb_wrap)
			return (usb_wrap_write_packet (camera->port, packet, 
						       length));
		else
			return (gp_port_write (camera->port, packet, length));
	}

	/* SERIAL */
	for (r = 0; r < RETRIES; r++) {

		ret = gp_port_write (camera->port, packet, length);
		if (ret == GP_ERROR_TIMEOUT)
			continue;

		return (ret);
	}

	return (GP_ERROR_TIMEOUT);
}

static int
sierra_write_nak (Camera *camera)
{
        char buf[4096];
        int ret;

        GP_DEBUG ("* sierra_write_nack");

        buf[0] = NAK;
        ret = sierra_write_packet (camera, buf);
        if (camera->port->type == GP_PORT_USB && !camera->pl->usb_wrap)
                gp_port_usb_clear_halt(camera->port, GP_PORT_USB_ENDPOINT_IN);
        return (ret);
}

/**
 * sierra_read_packet:
 * @camera : camera data stucture
 * @packet : to return the read packet
 *     
 * Read a data packet from the camera.
 *
 * Method:
 *   Two kinds of packet may be received from the camera :
 *     - either single byte length packet,
 *     - or several byte length packet.
 *
 *   Structure of the several byte length packet is the following :
 *	 Offset Length    Meaning
 *          0     1       Packet type
 *          1     1       Packet subtype/sequence
 *          2     2       Length of data
 *          4   variable  Data
 *         -2     2       Checksum
 *
 *   The operation is successful when the packet is recognised
 *   and is completely retrieved without IO errors.
 *
 * Returns: error code
 *     - GP_OK if the operation is successful (see description),
 *     - GP_ERROR_UNKNOWN_PORT if no port was specified within
 *       the camera data structure,
 *     - GP_ERROR_CORRUPTED_DATA if the received data are invalid,
 *     - GP_ERROR_IO_* on other IO error.
 */
static int
sierra_read_packet (Camera *camera, char *packet, GPContext *context)
{
	int y, r = 0, ret_status = GP_ERROR_IO, done = 0, length;
	int blocksize = 1, br;

	GP_DEBUG ("Reading packet...");

	switch (camera->port->type) {
	case GP_PORT_USB:
		blocksize = 2054;
		break;
	case GP_PORT_SERIAL:
		blocksize = 1;
		break;
	default:
		ret_status = GP_ERROR_UNKNOWN_PORT;
		done = 1;
	}

	/* Try several times before leaving on error... */
	for (r=0; !done && r<RETRIES; r++) {

		/* Reset the return data packet */
		memset(packet, 0, sizeof(packet));

		/* Reset the return status */
		ret_status = GP_ERROR_IO;

		/* Clear the USB bus
		   (what about the SERIAL bus : do we need to flush it?) */
		if (camera->port->type == GP_PORT_USB && !camera->pl->usb_wrap)
			gp_port_usb_clear_halt(camera->port, GP_PORT_USB_ENDPOINT_IN);

		/*
		 * Read data through the bus. If an error occurred,
		 * try again.
		 */
		if (camera->port->type == GP_PORT_USB && camera->pl->usb_wrap)
			br = usb_wrap_read_packet (camera->port, packet,
						   blocksize);
		else
			br = gp_port_read (camera->port, packet, blocksize);
		if (br < 0) {
			GP_DEBUG ("Read failed ('%s').",
				  gp_result_as_string (br));
			if (r + 1 >= RETRIES) {
				if (camera->port->type == GP_PORT_USB &&
				    !camera->pl->usb_wrap)
					gp_port_usb_clear_halt (camera->port,
						GP_PORT_USB_ENDPOINT_IN);
				return (br);
			}
			GP_DEBUG ("Retrying...");
			continue;
		}

		/*
		 * If the first read byte is not known, 
		 * report an error and exit the processing.
		 */
		if (sierra_valid_type (packet[0]) != GP_OK ) {
			if (camera->port->type == GP_PORT_USB &&
			    !camera->pl->usb_wrap)
				gp_port_usb_clear_halt (camera->port,
						GP_PORT_USB_ENDPOINT_IN);
			gp_context_error (context, _("The first byte "
				"received (0x%x) is not valid."), packet[0]);
			return (GP_ERROR_CORRUPTED_DATA);
		}

		/* For single byte data packets, the work is done... */
		if ((packet[0] == TYPE_DATA) ||
		    (packet[0] == TYPE_DATA_END)) {
			if (camera->port->type == GP_PORT_USB &&
			    !camera->pl->usb_wrap)
				gp_port_usb_clear_halt (camera->port,
						GP_PORT_USB_ENDPOINT_IN);
			return (GP_OK);
		}

		/*
		 * We've got a packet containing several bytes.
		 * The data packet size is not known yet if the communication
		 * is performed through the serial bus : read it then.
		 */
		if (camera->port->type == GP_PORT_SERIAL) {
			br = gp_port_read (camera->port, packet + 1, 3);
			if (br < 0) {
				if (camera->port->type == GP_PORT_USB &&
				    !camera->pl->usb_wrap)
					gp_port_usb_clear_halt (camera->port,
						GP_PORT_USB_ENDPOINT_IN);
				return (br);
			} else
				br = 4;
		}

		/*
		 * Determine the packet length (add 6 for the packet type,
		 * the packet subtype or sequence, the length
		 * of data and the checksum, see description).
		 */
		length = (unsigned char) packet[2] +
			((unsigned char) packet[3] * 256) + 6;

		/* 
		 * Read until the end of the packet is reached
		 * or an IO error occured
		 */
		for (ret_status = GP_OK, y = br;
		     ret_status == GP_OK && y < length; y += blocksize) {
			br = gp_port_read (camera->port, &packet[y], blocksize);
			if (br < 0)
				ret_status = br;
		}

		/* Retry on IO error only */
		if (ret_status == GP_ERROR_TIMEOUT) {
			GP_DEBUG ("Timeout!");
			sierra_write_nak (camera);
			GP_SYSTEM_SLEEP(10);
		} else
			break;
	}

	if (camera->port->type == GP_PORT_USB && !camera->pl->usb_wrap)
		gp_port_usb_clear_halt(camera->port, GP_PORT_USB_ENDPOINT_IN);

	return ret_status;
}

static int
sierra_build_packet (Camera *camera, char type, char subtype,
		     int data_length, char *packet) 
{
	packet[0] = type;
	switch (type) {
	case TYPE_COMMAND:
		if (camera->port->type == GP_PORT_USB)
				/* USB cameras don't care about first packets */
			camera->pl->first_packet = 0;
		if (camera->pl->first_packet)
			packet[1] = SUBTYPE_COMMAND_FIRST;
		else
			packet[1] = SUBTYPE_COMMAND;
		camera->pl->first_packet = 0;
		break;
	case TYPE_DATA:
	case TYPE_DATA_END:
		packet[1] = subtype;
		break;
	default:
		GP_DEBUG ("* unknown packet type!");
	}

	packet[2] = data_length &  0xff;
	packet[3] = (data_length >> 8) & 0xff;

	return (GP_OK);
}

static int
sierra_write_ack (Camera *camera) 
{
	char buf[4096];
	int ret;

	GP_DEBUG ("* sierra_write_ack");
	
	buf[0] = ACK;
	ret = sierra_write_packet (camera, buf);
	if (camera->port->type == GP_PORT_USB && !camera->pl->usb_wrap)
		gp_port_usb_clear_halt (camera->port, GP_PORT_USB_ENDPOINT_IN);
	return (ret);
}

int sierra_ping (Camera *camera, GPContext *context) 
{
	int ret, r = 0;
	char buf[4096];

	GP_DEBUG ("* sierra_ping");

	if (camera->port->type == GP_PORT_USB) {
		GP_DEBUG ("* sierra_ping no ping for USB");
		return (GP_OK);
	}
	
	buf[0] = NUL;
	for (r = 0; r < RETRIES; r++) {

		ret = sierra_write_packet (camera, buf);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return ret;;

		ret = sierra_read_packet (camera, buf, context);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return ret;
      
		if (buf[0] == NAK)
			return GP_OK;
		else
			return GP_ERROR_CORRUPTED_DATA;
	}
	return GP_ERROR_IO;
}

int sierra_set_speed (Camera *camera, int speed, GPContext *context) 
{
	GPPortSettings settings;

	GP_DEBUG ("Setting speed to %i...", speed);

	camera->pl->first_packet = 1;

	gp_port_get_settings (camera->port, &settings);

	switch (speed) {
	case 9600:
		speed = 1;
		settings.serial.speed = 9600;
		break;
	case 19200:
		speed = 2;
		settings.serial.speed = 19200;
		break;
	case 38400:
		speed = 3;
		settings.serial.speed = 38400;
		break;
	case 57600:
		speed = 4;
		settings.serial.speed = 57600;
		break;
	case 0:		/* Default speed */
	case 115200:
		speed = 5;
		settings.serial.speed = 115200;
		break;

	case -1:	/* End session */
		settings.serial.speed = 19200;
		speed = 2;
		break;
	default:
		return GP_ERROR_IO_SERIAL_SPEED;
	}

	CHECK (sierra_set_int_register (camera, 17, speed, context));
	CHECK (gp_port_set_settings (camera->port, settings));

	GP_SYSTEM_SLEEP(10);
	return GP_OK;
}

/**
 * sierra_write_action_command_and_wait:
 * @camera : camera data stucture
 * @action_code : action code of the command to be sent
 *     
 * Send an action command to the camera (i.e. a command that
 * is not related to registers). Wait for the Action Complete
 * Notification.
 *
 * Returns: error code
 *     - GP_OK if the operation is successful,
 *     - GP_ERROR if the camera returns an 'Enable to
 *       Execute Command' status,
 *     - GP_ERROR_CORRUPTED_DATA if the received data are invalid,
 *     - GP_ERROR_IO_* on other IO error.
 */
static int
sierra_write_action_command_and_wait (Camera *camera, int action_code,
				      GPContext *context)
{
	int r, ret;
	char buf[4096];

	/* Build and send the command packet */
	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 3, buf));
	buf[4] = 0x02;
	buf[5] = action_code;
	buf[6] = 0x00;
	CHECK (sierra_write_packet (camera, buf));

	/* Wait for the Action Complete Notification */
	for (r = 0; r < RETRIES; r++) {
		
		ret = sierra_read_packet (camera, buf, context);
		if (ret == GP_OK) {
			switch ((unsigned char)buf[0]) {
			case ENQ:
			case TRM:
				return GP_OK;
			case NAK:
			case ACK:
				continue;
			case DC1:
				/* Unable to execute the command */
				gp_context_error (context, _("The camera "
					"is not able to execute this command. "
					"Please report this error to "
					"<gphoto-devel@gphoto.org>."));
				return GP_ERROR;
			default:
				return GP_ERROR_CORRUPTED_DATA;
			}
		}
	}

	return GP_ERROR_IO;
}

int sierra_set_int_register (Camera *camera, int reg, int value, GPContext *context) 
{
	int r=0;
	char p[4096];
	char buf[4096];
	int ret;
	unsigned int id;

	GP_DEBUG ("Setting int register %i to %i...", reg, value);

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0,
				    (value < 0) ? 2 : 6, p));

	/* Fill in the data */
	p[4] = 0x00;
	p[5] = reg;
	if (value >= 0) {
		p[6] = (value)     & 0xff;
		p[7] = (value>>8)  & 0xff;
		p[8] = (value>>16) & 0xff;
		p[9] = (value>>24) & 0xff;
	}

	id = gp_context_progress_start (context, RETRIES,
					_("Transmitting data..."));
	for (r = 0; r < RETRIES; r++) {

		CHECK (sierra_write_packet (camera, p));

		ret = sierra_read_packet (camera, buf, context);
		if (ret == GP_ERROR_TIMEOUT) {
			if (r == RETRIES - 1)
				GP_DEBUG ("Transmission timed out.");
			else
				GP_DEBUG ("Transmission timed out "
					  "- retrying...");
			continue;
		}
		CHECK (ret);

		if (buf[0] == ACK) {
			GP_DEBUG ("Transmission successful.");
			break;
		}

		/* DC1 = invalid register or value */
		if (buf[0] == DC1) {
			gp_context_error (context, _("Could not set "
				"int register %i. Please contact "
				"<gphoto-devel@gphoto.org>."), reg);
			return GP_ERROR_BAD_PARAMETERS;
		}

		gp_context_progress_update (context, id, r + 1);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
	}
	gp_context_progress_stop (context, id);

	if (r == RETRIES) {

		/* Set to default speed, don't care about the return value */
		sierra_set_speed (camera, -1, context);

		return (GP_ERROR_IO);
	}

	return (GP_OK);
}

int sierra_get_int_register (Camera *camera, int reg, int *value, GPContext *context) 
{
	int r = 0, write_nak = 0;
	char packet[4096];
	char buf[4096];
	int ret;

	GP_DEBUG ("* sierra_get_int_register");
	GP_DEBUG ("* register: %i", reg);

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 2, packet));

	/* Fill in the data */
	packet[4] = 0x01;
	packet[5] = reg;

	while (r++ < RETRIES) {
		if (write_nak)
			CHECK (sierra_write_nak (camera))
		else
			CHECK (sierra_write_packet (camera, packet));

		ret = sierra_read_packet (camera, buf, context);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		CHECK (ret);

		/* DC1 = invalid register or value */
		if (buf[0] == DC1) {
			gp_context_error (context, _("Could not get "
				"register %i. Please contact "
				"<gphoto-devel@gphoto.org>."), reg);
			return GP_ERROR_BAD_PARAMETERS;
		}

		if (buf[0] == TYPE_DATA_END) {
			ret = sierra_write_ack(camera);
			r = ((unsigned char)buf[4]) +
				((unsigned char)buf[5] * 256) +
				((unsigned char)buf[6] * 65536) +
				((unsigned char)buf[7] * 16777216);
			*value = r;
			return ret;
		}

		if (buf[0] == ENQ)
			return GP_OK;

		write_nak = 1;
	}

	return GP_ERROR_IO;
}

int sierra_set_string_register (Camera *camera, int reg, const char *s, long int length, GPContext *context) 
{

	char packet[4096], buf[4096];
	char type;
	unsigned char c;
	long int x=0;
	int seq=0, size=0, done, r, ret, do_percent;
	unsigned int id = 0;

	GP_DEBUG ("* sierra_set_string_register");
	GP_DEBUG ("* register: %i", reg);
	GP_DEBUG ("* value: %s", s);

	/* Make use of the progress bar when the packet is "large enough" */
	if (length > MAX_DATA_FIELD_LENGTH) {
		do_percent = 1;
		id = gp_context_progress_start (context, length, _("Sending data..."));
	}
	else
		do_percent = 0;

	while (x < length) {
		if (x == 0) {
			type = TYPE_COMMAND;
			size = (length + 2 - x) > MAX_DATA_FIELD_LENGTH
				? MAX_DATA_FIELD_LENGTH : length + 2;
		}  else {
			size = (length - x) > MAX_DATA_FIELD_LENGTH
				? MAX_DATA_FIELD_LENGTH : length-x;
			if (x + size < length)
				type = TYPE_DATA;
			else
				type = TYPE_DATA_END;
		}
		CHECK (sierra_build_packet (camera, type, seq, size, packet));

		if (type == TYPE_COMMAND) {
			packet[4] = 0x03;
			packet[5] = reg;
			memcpy (&packet[6], &s[x], size-2);
			x += size - 2;
		} else  {
			packet[1] = seq++;
			memcpy (&packet[4], &s[x], size);
			x += size;
		}

		r = 0; done = 0;
		while ((r++ < RETRIES) && (!done)) {

			ret = sierra_write_packet (camera, packet);
			if (ret == GP_ERROR_TIMEOUT)
				continue;
			CHECK (ret);

			ret = sierra_read_packet (camera, buf, context);
			if (ret == GP_ERROR_TIMEOUT)
				continue;
			CHECK (ret);

			c = (unsigned char)buf[0];
			if (c == DC1) {
				gp_context_error (context, _("Could not "
					"set string register %i. Please "
					"contact <gphoto-devel@gphoto.org>."),
					reg);
				return GP_ERROR_BAD_PARAMETERS;
			} else if (c == ACK) {
				done = 1;
				if (do_percent)
					gp_context_progress_update (context, id, x);
			}

			else	{
				if (c != NAK)
					return GP_ERROR_IO;
			}

		}
		if (r > RETRIES) {
			return GP_ERROR_IO;
		}
	}
	if (do_percent)
		gp_context_progress_stop (context, id);

	return GP_OK;
}

int sierra_get_string_register (Camera *camera, int reg, int file_number, 
                                CameraFile *file, unsigned char *b,
				unsigned int *b_len, GPContext *context) {

	unsigned char packet[4096];
	unsigned int packlength, total = *b_len;
	unsigned int id = 0;

	GP_DEBUG ("* sierra_get_string_register");
	GP_DEBUG ("* register: %i", reg);
	GP_DEBUG ("* file number: %i", file_number);

	/* Set the current picture number */
	if (file_number >= 0)
		CHECK (sierra_set_int_register (camera, 4, file_number, context));

	/* Build and send the request */
	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 2, packet));
	packet[4] = 0x04;
	packet[5] = reg;
	CHECK (sierra_write_packet (camera, packet));

	if (file)
		id = gp_context_progress_start (context, total, _("Downloading..."));

	/* Read all the data packets */
	*b_len = 0;
	do {

		/* Read one packet */
		CHECK (sierra_read_packet (camera, packet, context));
		if (packet[0] == DC1) {
			gp_context_error (context, _("Could not get "
				"string register %i. Please contact "
				"<gphoto-devel@gphoto.org>."), reg);
			return GP_ERROR_BAD_PARAMETERS;
		}
		CHECK (sierra_write_ack (camera));

		packlength = packet[2] | (packet[3] << 8);
		GP_DEBUG ("Packet length: %d", packlength);

		if (b)
			memcpy (&b[*b_len], &packet[4], packlength);
		*b_len += packlength;

		if (file) {
			CHECK (gp_file_append (file, &packet[4], packlength));
			gp_context_progress_update (context, id, *b_len);
		}

	} while (packet[0] != TYPE_DATA_END);
	gp_context_progress_stop (context, id);

	return (GP_OK);
}

int sierra_delete_all (Camera *camera, GPContext *context)
{
	char packet[4096], buf[4096], r, done, ret;

	GP_DEBUG ("* sierra_delete_all");

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet));

	packet[4] = 0x02;
	packet[5] = 0x01;
	packet[6] = 0x00;

	/* Send the command delete all */
	CHECK (sierra_write_packet (camera, packet));

	/* Read command acknowledgement */
	CHECK (sierra_read_packet (camera, buf, context));
	if (buf[0] != ACK)
		return GP_ERROR_CORRUPTED_DATA;

	/* Wait for the action complete notification */
	r = 0, done = 0;
	while ( !done && r < RETRIES ) {

		GP_SYSTEM_SLEEP (QUICKSLEEP);

		ret = sierra_read_packet (camera, buf, context);
		if (ret == GP_OK) {
			done = 1;
			ret  = (buf[0] == ENQ) ? GP_OK : GP_ERROR_CORRUPTED_DATA;
		}
	}

	return ret;
}

int sierra_delete (Camera *camera, int picture_number, GPContext *context) 
{
	char packet[4096], buf[4096];
	int r, done, ret;

	GP_DEBUG ("* sierra_delete");
	GP_DEBUG ("* picture: %i", picture_number);

	CHECK (sierra_set_int_register (camera, 4, picture_number, context));

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet));

	packet[4] = 0x02;
	packet[5] = 0x07;
	packet[6] = 0x00;

	r = 0; done = 0;
	while ((!done) && (r++<RETRIES)) {

		ret = sierra_write_packet (camera, packet);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		CHECK (ret);

		ret = sierra_read_packet (camera, buf, context);
		if (ret == GP_ERROR_TIMEOUT)
			continue;
		CHECK (ret);

		done = (buf[0] == NAK)? 0 : 1;

		if (done) {
			/* read in the ENQ */
			if (sierra_read_packet (camera, buf, context) != GP_OK)
				return ((buf[0] == ENQ)? GP_OK : GP_ERROR_IO);
			
		}
	}
	if (r > RETRIES)
		return (GP_ERROR_IO);

	GP_SYSTEM_SLEEP(QUICKSLEEP);

	return (GP_OK);
}

int sierra_end_session (Camera *camera, GPContext *context) 
{
	char packet[4096], buf[4096];
	unsigned char c;
	int r, done;

	GP_DEBUG ("* sierra_end_session");

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet));
	packet[4] = 0x02;
	packet[5] = 0x04;
	packet[6] = 0x00;

	r = 0; done = 0;
	while ((!done) && (r++<RETRIES)) {
		CHECK (sierra_write_packet (camera, packet));
		CHECK (sierra_read_packet (camera, buf, context));

		c = (unsigned char)buf[0];
		if (c == TRM)
			return (GP_OK);

		done = (c == NAK)? 0 : 1;

		if (done) {

			/* read in the ENQ */
			CHECK (sierra_read_packet (camera, buf, context));
			c = (unsigned char)buf[0];
			return ((c == ENQ)? GP_OK : GP_ERROR_IO);
		}
	}
	if (r > RETRIES)
		return (GP_ERROR_IO);

	return (GP_OK);
}

int sierra_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	unsigned int size;

	/* Send to the camera the capture request and wait
	   for the completion */
	CHECK (sierra_write_action_command_and_wait(camera, 5, context));

	/* Retrieve the preview and set the MIME type */
	CHECK (sierra_get_int_register (camera, 12, &size, context));
	CHECK (sierra_get_string_register (camera, 14, 0, file, NULL, &size, context));
	CHECK (gp_file_set_mime_type (file, "image/jpeg"));

	return (GP_OK);
}

int sierra_capture (Camera *camera, CameraCaptureType type,
		    CameraFilePath *filepath, GPContext *context)
{
	int n, len = 0;
	char filename[128];
	const char *folder;

	GP_DEBUG ("* sierra_capture");

	/* We can only capture images */
	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Send to the camera the capture request and wait
	   for the completion */
	CHECK (sierra_write_action_command_and_wait(camera, 2, context));

	/* After picture is taken, register 4 is set to current picture */
	GP_DEBUG ("Getting picture number...");
	CHECK (sierra_get_int_register (camera, 4, &n, context));

	/*
	 * We need to tell the frontend where the new image can be found. 
	 * Unfortunatelly, we can only figure out the filename. Therefore,
	 * reset the CameraFilesystem and let it search for the filename.
	 *
	 * If you know how to figure out where the image gets saved,
	 * please submit a patch.
	 *
	 * Not that some cameras that don't support filenames will return
	 * 8 blanks instead of reporting an error.
	 */
	GP_DEBUG ("Getting filename of file %i...", n);
	CHECK (sierra_get_string_register (camera, 79, 0, NULL,
					   filename, &len, context));
	if ((len <= 0) || !strcmp (filename, "        "))
		snprintf (filename, sizeof (filename), "P101%04i.JPG", n);
	GP_DEBUG ("... done ('%s')", filename);
	CHECK (gp_filesystem_reset (camera->fs));
	CHECK (gp_filesystem_get_folder (camera->fs, filename, &folder, context));
	strncpy (filepath->folder, folder, sizeof (filepath->folder));
	strncpy (filepath->name, filename, sizeof (filepath->name));

	return (GP_OK);
}

int sierra_upload_file (Camera *camera, CameraFile *file, GPContext *context)
{
	const char *data;
	long data_size;

	/* Put the "magic spell" in register 32 */
	CHECK (sierra_set_int_register (camera, 32, 0x0FEC000E, context));

	/* Upload the file */
	CHECK (gp_file_get_data_and_size (file, &data, &data_size));
	CHECK (sierra_set_string_register (camera, 29, data, data_size, context));

	/* Send command to order the transfer into NVRAM and wait
	   for the completion */
	CHECK (sierra_write_action_command_and_wait (camera, 11, context));

	return GP_OK;
}

static unsigned int
get_int (const unsigned char b[])
{
	return (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
}

int sierra_get_pic_info (Camera *camera, unsigned int n,
			 SierraPicInfo *pic_info, GPContext *context)
{
	unsigned char buf[1024];
	unsigned int buf_len = 0;

	CHECK (sierra_get_string_register (camera, 47, n, NULL, buf, &buf_len, context));
	if (buf_len != 32)
		return (GP_ERROR_CORRUPTED_DATA);
	pic_info->size_file      = get_int (buf);
	pic_info->size_preview   = get_int (buf + 4);
	pic_info->size_audio     = get_int (buf + 8);
	pic_info->resolution     = get_int (buf + 12);
	pic_info->locked         = get_int (buf + 16);
	pic_info->date           = get_int (buf + 20);
	pic_info->animation_type = get_int (buf + 28);

	/* Make debugging easier */
	GP_DEBUG ("File size: %d", pic_info->size_file);
	GP_DEBUG ("Preview size: %i", pic_info->size_preview);
	GP_DEBUG ("Audio size: %i", pic_info->size_audio);
	GP_DEBUG ("Resolution: %i", pic_info->resolution);
	GP_DEBUG ("Locked: %i", pic_info->locked);
	GP_DEBUG ("Date: %i", pic_info->date);
	GP_DEBUG ("Animation type: %i", pic_info->animation_type);

	return (GP_OK);
}

int sierra_set_locked (Camera *camera, unsigned int n, SierraLocked locked,
		       GPContext *context)
{
	CHECK (sierra_set_int_register (camera, 4, n, context));

	gp_context_error (context, _("Not implemented!"));
	return (GP_ERROR);

	return (GP_OK);
}
