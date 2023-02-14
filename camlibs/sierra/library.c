/* library.c
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sf.net>
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
#define _DARWIN_C_SOURCE
#include "config.h"
#include "library.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <gphoto2/gphoto2-port-log.h>

#include "libgphoto2/i18n.h"

#include "sierra.h"
#include "sierra-usbwrap.h"

#define GP_MODULE "sierra"

enum _SierraPacket {
	NUL				= 0x00,
	SIERRA_PACKET_DATA		= 0x02,
	SIERRA_PACKET_DATA_END		= 0x03,
	SIERRA_PACKET_ENQ		= 0x05,
	ACK				= 0x06,
	SIERRA_PACKET_INVALID		= 0x11,
	SIERRA_PACKET_NAK		= 0x15,
	SIERRA_PACKET_CANCEL		= 0x18,
	SIERRA_PACKET_COMMAND		= 0x1b,
	SIERRA_PACKET_WRONG_SPEED	= 0x8c,
	SIERRA_PACKET_SESSION_ERROR	= 0xfc,
	SIERRA_PACKET_SESSION_END	= 0xff
};
typedef enum _SierraPacket SierraPacket;

/* Size of requested packet */
#define SIERRA_PACKET_SIZE		32774

/* Sub-types */
#define SUBSIERRA_PACKET_COMMAND_FIRST   0x53
#define SUBSIERRA_PACKET_COMMAND         0x43

/* Maximum length of the data field within a packet */
#define MAX_DATA_FIELD_LENGTH   2048

#define RETRIES                 10

#define		QUICKSLEEP	5

int sierra_change_folder (Camera *camera, const char *folder, GPContext *context)
{
	int st = 0, i;
	char target[128];

	GP_DEBUG ("*** sierra_change_folder");
	GP_DEBUG ("*** folder: %s", folder);

	/*
	 * Do not issue the command if the camera doesn't support folders
	 * or if the folder is the current working folder
	 */
	if (!camera->pl->folders || !strcmp (camera->pl->folder, folder))
		return GP_OK;

	/* We will later need a path ending with '/'. */
	memset (target, 0, sizeof (target));
	if (folder[0])
		strncpy (target, folder, sizeof (target) - 1);
	if (target[strlen (target) - 1] != '/')
		strcat (target, "/");

	/* If the given path is absolute, we need to start in '/'. */
	if (target[0] == '/') {
		CHECK (sierra_set_string_register (camera, 84, "\\", 1,
						   context));
		i = 1;
	} else
		i = 0;

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
	int count, i, r;
	unsigned int len = 0;
	char filename[1024];

	GP_DEBUG ("Listing files in folder '%s'", folder);

	/*
	 * This is a non-fatal check if a memory card is present. Some
	 * cameras don't understand this command and error out here.
	 * Since some cameras do not like it at all, do not send it for those.
	 */
	if (!(camera->pl->flags & SIERRA_NO_51)) {
		r = sierra_get_int_register (camera, 51, &i, NULL);
		if ((r >= 0) && (i == 1)) {
			gp_context_error (context, _("No memory card present"));
			return GP_ERROR_NOT_SUPPORTED;
		}
	}

	/* We need to change to the folder first */
	CHECK (sierra_change_folder (camera, folder, context));

	/* Then, we count the files */
	GP_DEBUG ("Counting files in '%s'...", folder);
	CHECK (sierra_get_int_register (camera, 10, &count, context));
	GP_DEBUG ("... done. Found %i file(s).", count);

	/* No files? Then, we are done. */
	if (!count)
		return GP_OK;

	/*
	 * Get the filename of the first picture. Note that some cameras
	 * that don't support filenames return 8 blanks instead of
	 * reporting an error. If this is indeed the case, just fill
	 * the list with dummy entries and return.
	 */
	GP_DEBUG ("Getting filename of first file");
	r = sierra_get_string_register (camera, 79, 1, NULL,
					(unsigned char *)filename,
					&len, context);
	if ((r < 0) || (len <= 0) || !strcmp (filename, "        ")) {
		CHECK (gp_list_populate (list, "P101%04i.JPG", count));
		return GP_OK;
	}

	/*
	 * The camera supports filenames. Append the first one to the list
	 * and get the remaining ones.
	 */
	CHECK (gp_list_append (list, filename, NULL));
	for (i = 1; i < count; i++) {
		GP_DEBUG ("Getting filename of file %i...", i + 1);
		CHECK (sierra_get_string_register (camera, 79, i + 1, NULL,
						   (unsigned char *)filename,
						   &len, context));
		if ((len <= 0) || !strcmp (filename, "        "))
			snprintf (filename, sizeof (filename),
				  "P101%04i.JPG", i + 1);
		GP_DEBUG ("... done ('%s').", filename);
		CHECK (gp_list_append (list, filename, NULL));
	}

	return GP_OK;
}

int sierra_list_folders (Camera *camera, const char *folder, CameraList *list,
			 GPContext *context)
{
	int i, j, count;
	unsigned int bsize;
	char buf[1024];

	/* List the folders only if the camera supports them */
	if (!camera->pl->folders)
		return GP_OK;

	CHECK (sierra_change_folder (camera, folder, context));
	GP_DEBUG ("*** counting folders in '%s'...", folder);
	CHECK (sierra_get_int_register (camera, 83, &count, context));
	GP_DEBUG ("*** found %i folders", count);
	for (i = 0; i < count; i++) {
		CHECK (sierra_change_folder (camera, folder, context));
		CHECK (sierra_set_int_register (camera, 83, i + 1, context));
		bsize = 1024;
		GP_DEBUG ("*** getting name of folder %i", i + 1);
		CHECK (sierra_get_string_register (camera, 84, 0,
						   NULL, (unsigned char *)buf,
						   &bsize, context));

		/* Remove trailing spaces */
		for (j = strlen (buf) - 1; j >= 0 && buf[j] == ' '; j--)
			buf[j] = '\0';
		gp_list_append (list, buf, NULL);
	}

	return GP_OK;
}

/**
 * sierra_get_picture_folder:
 * @camera : camera data structure
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
		return GP_OK;
	}

	CHECK (gp_list_new (&list));

	/* It is assumed that the camera conforms with the JEIDA standard .
	   Thus, look for the picture folder into the /DCIM directory */
	CHECK (gp_filesystem_list_folders (camera->fs, "/DCIM", list, NULL) );
	for(i=0 ; i < gp_list_count (list) ; i++) {
		CHECK (gp_list_get_name (list, i, &name) );
		GP_DEBUG ("* check folder %s", name);
		if (isdigit(name[0]) && isdigit(name[1]) && isdigit(name[2]))
			break;
		name = NULL;
	}

	if (name) {
		*folder = (char*) calloc(strlen(name)+7, sizeof(char));
		strcpy(*folder, "/DCIM/");
		strcat(*folder, name);
		gp_list_free (list);
		return GP_OK;
	} else {
		gp_list_free (list);
		return GP_ERROR_DIRECTORY_NOT_FOUND;
	}
}

/**
 * sierra_check_battery_capacity:
 * @camera : camera data structure
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
	if (!capacity) /* 0% is unlikely */
		return GP_OK;

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
 * @camera : camera data structure
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

	ret = sierra_get_int_register (camera, 28, memory, context);
	if (ret < 0) {
		gp_context_error (context, _("Cannot retrieve the available "
					     "memory left"));
		return ret;
	}

	return GP_OK;
}

static int
sierra_check_connection (Camera *camera, GPContext *context)
{
	int r = 0, ret, timeout;
	unsigned char c;

	/* Only serial cameras close the connection after some timeout. */
	if (camera->port->type != GP_PORT_SERIAL)
		return GP_OK;

	GP_DEBUG ("Checking if connection is still open");
	while (1) {

		/*
		 * Do like the Windows driver - check for 20ms if something
		 * has been sent by the camera.
		 */
		CHECK (gp_port_get_timeout (camera->port, &timeout));
		CHECK (gp_port_set_timeout (camera->port, 20));
		ret = gp_port_read (camera->port, (char *)&c, 1);
		CHECK (gp_port_set_timeout (camera->port, timeout));
		switch (ret) {
		case GP_ERROR_TIMEOUT:
		case GP_ERROR_IO_READ:

			/*
			 * If we got GP_ERROR_TIMEOUT or GP_ERROR_IO_READ,
			 * everything's just fine.
			 */
			return GP_OK;

		default:

			/*
			 * If any error (except timeout) has occurred,
			 * report it.
			 */
			CHECK (ret);
		}

		/*
		 * We have received something. This is not good. Check if
		 * the camera has closed the connection and told us about
		 * that by sending SIERRA_PACKET_SESSION_END.
		 */
		if (c == SIERRA_PACKET_SESSION_END) {
			if (++r > 2) {
				gp_context_error (context, _("Camera refused "
					"3 times to keep a connection open."));
				return GP_ERROR;
			}
			CHECK (sierra_init (camera, context));
			CHECK (sierra_set_speed (camera, SIERRA_SPEED_19200,
						 context));
			continue;
		}

		/*
		 * The camera sends us unexpected data. Dump it and assume
		 * that everything's just fine.
		 */
		while (gp_port_read (camera->port, (char *)&c, 1) >= 0);
		return GP_OK;
	}
}

static int
sierra_write_packet (Camera *camera, char *packet, GPContext *context)
{
	int x, checksum=0, length;

	CHECK (sierra_check_connection (camera, context));

	/*
	 * We need to check if this is the first packet. Note that USB
	 * cameras don't care.
	 */
	switch (packet[0]) {
	case SIERRA_PACKET_COMMAND:
		switch (camera->port->type) {
		case GP_PORT_SERIAL:
			packet[1] = (camera->pl->first_packet ?
					SUBSIERRA_PACKET_COMMAND_FIRST :
					SUBSIERRA_PACKET_COMMAND);
			camera->pl->first_packet = 0;
			break;
		default:
			packet[1] = SUBSIERRA_PACKET_COMMAND;
			break;
		}
		break;
	default:
		break;
	}

	/* Determining packet length */
	if ((packet[0] == SIERRA_PACKET_COMMAND) ||
	    (packet[0] == SIERRA_PACKET_DATA) ||
	    (packet[0] == SIERRA_PACKET_DATA_END)) {
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

	if (camera->pl->flags & SIERRA_WRAP_USB_MASK) {
		CHECK (usb_wrap_write_packet (camera->port,
			(camera->pl->flags & SIERRA_WRAP_USB_MASK),
			packet, length));
	} else {
		if (camera->pl->flags & SIERRA_NO_BLOCK_WRITE) {
			for (x = 0; x < length; x++)
				CHECK (gp_port_write(camera->port, &packet[x], 1));
		} else {
			CHECK (gp_port_write (camera->port, packet, length));
		}
	}

	return GP_OK;
}

static int
sierra_clear_usb_halt(Camera *camera)
{

	if ( (camera->port->type == GP_PORT_USB) &&
	     !(camera->pl->flags & SIERRA_WRAP_USB_MASK) &&
	     !(camera->pl->flags & SIERRA_NO_USB_CLEAR) )
		gp_port_usb_clear_halt(camera->port, GP_PORT_USB_ENDPOINT_IN);
        return (GP_OK);
}

static int
sierra_write_nak (Camera *camera, GPContext *context)
{
        char buf[4096];
        int ret;

        GP_DEBUG ("* sierra_write_nack");

        buf[0] = SIERRA_PACKET_NAK;
        ret = sierra_write_packet (camera, buf, context);
	sierra_clear_usb_halt(camera);
        return (ret);
}

/**
 * sierra_read_packet:
 * @camera : camera data structure
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
sierra_read_packet (Camera *camera, unsigned char *packet, GPContext *context)
{
	int result;
	unsigned int i, c, r = 0, length, blocksize = 1, br;

	GP_DEBUG ("Reading packet...");

	switch (camera->port->type) {
	case GP_PORT_USB_SCSI:
	case GP_PORT_USB:
		blocksize = SIERRA_PACKET_SIZE;
		break;
	case GP_PORT_SERIAL:
		blocksize = 1;
		break;
	default:
		return (GP_ERROR_UNKNOWN_PORT);
	}

	/* Try several times before leaving on error... */
	while (1) {

		/* Clear the USB bus
		   (what about the SERIAL bus : do we need to flush it?) */
		sierra_clear_usb_halt(camera);

		/*
		 * Read data through the bus. If an error occurred,
		 * try again.
		 */
		if ((camera->port->type & (GP_PORT_USB_SCSI|GP_PORT_USB)) && (camera->pl->flags & SIERRA_WRAP_USB_MASK))
			result = usb_wrap_read_packet (camera->port,
					(camera->pl->flags & SIERRA_WRAP_USB_MASK),
					(char *)packet, blocksize);
		else
			result = gp_port_read (camera->port, (char *)packet, blocksize);
		if (result < 0) {
			GP_DEBUG ("Read failed (%i: '%s').", result,
				  gp_result_as_string (result));
			if (++r > 2) {
				sierra_clear_usb_halt(camera);
				GP_DEBUG ("Giving up...");
				return (result);
			}
			GP_DEBUG ("Retrying...");
			continue;
		}
		if (result == 0) {
			GP_DEBUG ("Read got 0 bytes..");
			if (++r > 2) {
				sierra_clear_usb_halt(camera);
				GP_DEBUG ("Giving up...");
				return GP_ERROR_IO_READ;
			}
			GP_DEBUG ("Retrying...");
			continue;
		}
		br = result;

		/*
		 * If the first read byte is not known,
		 * report an error and exit the processing.
		 */
		switch (packet[0]) {
		case NUL:
		case SIERRA_PACKET_ENQ:
		case ACK:
		case SIERRA_PACKET_INVALID:
		case SIERRA_PACKET_NAK:
		case SIERRA_PACKET_CANCEL:
		case SIERRA_PACKET_SESSION_ERROR:
		case SIERRA_PACKET_SESSION_END:
		case SIERRA_PACKET_WRONG_SPEED:

			/* Those are all single byte packets. */
			sierra_clear_usb_halt(camera);
			GP_DEBUG ("Packet type 0x%02x read. Returning GP_OK.", packet[0]);
			return GP_OK;

		case SIERRA_PACKET_DATA:
		case SIERRA_PACKET_DATA_END:
		case SIERRA_PACKET_COMMAND:

			/* Those are packets with more than one byte. */
			break;

		default:

			/*
			 * This byte is not valid. Dump everything that
			 * coming in.
			 */
			gp_context_error (context, _("The first byte "
				"received (0x%x) is not valid."), packet[0]);
			while (gp_port_read (camera->port, (char *)packet, 1) > 0)
				;
			sierra_clear_usb_halt(camera);
			return (GP_ERROR_CORRUPTED_DATA);
		}

		/*
		 * We've got a packet containing several bytes.
		 * The data packet size is not known yet if the communication
		 * is performed through the serial bus : read it then.
		 */
		if (br < 4) {
			result = gp_port_read (camera->port,
					       (char *)packet + br, 4 - br);
			if (result < 0) {
				sierra_clear_usb_halt(camera);
				GP_DEBUG ("Could not read length of "
					"packet (%i: '%s'). Giving up...",
					result, gp_result_as_string (result));
				return (result);
			}
			br += result;
		}

		/*
		 * Determine the packet length: 2 for packet type and
		 * subtype or sequence number, 2 for length, the actual
		 * data, and 2 for the checksum.
		 */
		length = packet[2] + (packet[3] * 256) + 6;
		if (length > SIERRA_PACKET_SIZE) {
			GP_DEBUG ("Packet too long (%d)!", length);
			return GP_ERROR_IO;
		}

		/*
		 * Read until the end of the packet is reached
		 * or an error occurred.
		 */
		while (br < length) {
			result = gp_port_read (camera->port, (char *)packet + br,
					       length - br);
			if (result == GP_ERROR_TIMEOUT) {

				/* We will retry later on. */
				GP_DEBUG ("Timeout!");
				break;

			} else if (result < 0) {
				GP_DEBUG ("Could not read remainder of "
					"packet (%i: '%s'). Giving up...",
					result, gp_result_as_string (result));
				return (result);
			} else
				br += result;
		}

		/*
		 * If we have, at this point, received the full packet,
		 * check the checksum. If the checksum is correct, we are
		 * done.
		 */
		if (br == length) {
			for (c = 0, i = 4; i < br - 2; i++)
				c += packet[i];
			c &= 0xffff;
			if (c == ((unsigned int)packet[br - 2] + ((unsigned int)packet[br - 1] * 256)))
				break;

			/*
			 * At least the Nikon CoolPix 880 sets the checksum
			 * always to 0xffff. In that case, we can't check it
			 * and return successfully.
			 */
			if ((packet[br - 2] == 0xff) &&
			    (packet[br - 1] == 0xff))
				break;
			/*
			 * The checksum for the Toshiba PDR-M6X cameras is
			 * almost always set to 0x00 0x00.
			 */
			if ((packet[br - 2] == 0x00) &&
			    (packet[br - 1] == 0x00))
				break;


			GP_DEBUG ("Checksum wrong (calculated 0x%x, "
				"found 0x%x)!", c,
				packet[br - 2] + (packet[br - 1] * 256));
		}

		/*
		 * We will retry if we couldn't receive the full packet
		 * (GP_ERROR_TIMEOUT) or if the checksum is wrong.
		 */
		r++;
		if (r + 1 >= RETRIES) {
			sierra_clear_usb_halt(camera);
			GP_DEBUG ("Giving up...");
			return ((br == length) ? GP_ERROR_CORRUPTED_DATA :
						 GP_ERROR_TIMEOUT);
		}
		GP_DEBUG ("Retrying...");
		sierra_write_nak (camera, context);
		usleep (10 * 1000);
	}

	sierra_clear_usb_halt(camera);

	return GP_OK;
}

static int
sierra_read_packet_wait (Camera *camera, char *buf, GPContext *context)
{
	int result, r = 0;

	while (1) {
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);

		result = sierra_read_packet (camera, (unsigned char *)buf, context);
		if (result == GP_ERROR_TIMEOUT) {
			if (++r > 2) {
				gp_context_error (context, _("Transmission "
					"of packet timed out even after "
					"%i retries. Please contact "
					"%s."), r, MAIL_GPHOTO_DEVEL);
				return GP_ERROR;
			}
			GP_DEBUG ("Retrying...");
			usleep (QUICKSLEEP * 1000);
			continue;
		}

		CHECK (result);

		GP_DEBUG ("Packet successfully read.");
		return GP_OK;
	}
}

static int
sierra_transmit_ack (Camera *camera, char *packet, GPContext *context)
{
	int r = 0, result;
	unsigned char buf[SIERRA_PACKET_SIZE];

	while (1) {
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);

		/* Write packet and read the answer */
		CHECK (sierra_write_packet (camera, packet, context));
		buf[0] = 0;
		result = sierra_read_packet_wait (camera, (char *)buf, context);
		switch (result) {
		case GP_ERROR_CORRUPTED_DATA:
			if (++r > 2) {
				gp_context_error (context, _("Could not "
					"transmit packet even after several "
					"retries."));
				return (result);
			}
			continue;
		default:
			CHECK (result);
		}

		switch (buf[0]) {
		case SIERRA_PACKET_ENQ:
		case ACK:
			GP_DEBUG ("Transmission successful.");
			return GP_OK;
		case SIERRA_PACKET_INVALID:
		case SIERRA_PACKET_CANCEL:
			gp_context_error (context, _("Packet was rejected "
				"by camera. Please contact "
				"%s."), MAIL_GPHOTO_DEVEL);
			return GP_ERROR;

		case SIERRA_PACKET_SESSION_END:
		case SIERRA_PACKET_SESSION_ERROR:
		case SIERRA_PACKET_WRONG_SPEED:
			if (++r > 2) {
				gp_context_error (context, _("Could not "
					"transmit packet even after several "
					"retries."));
				return GP_ERROR;
			}

			/*
                         * The camera has ended this session and
                         * reverted the speed back to 19200. Reinitialize
			 * the connection.
                         */
			CHECK (sierra_init (camera, context));
			CHECK (sierra_set_speed (camera, SIERRA_SPEED_19200,
						 context));
			continue;

		default:
			if (++r > 2) {
				gp_context_error (context, _("Could not "
					"transmit packet (error code %i). "
					"Please contact "
					"%s."), buf[0], MAIL_GPHOTO_DEVEL);
				return GP_ERROR;
			}
			GP_DEBUG ("Did not receive ACK. Retrying...");
			continue;
		}
	}
}

static int
sierra_build_packet (Camera *camera, char type, char subtype,
		     int data_length, char *packet)
{
	packet[0] = type;
	switch (type) {
	case SIERRA_PACKET_COMMAND:

		/* sierra_write_packet will check for the first packet */
		packet[1] = SUBSIERRA_PACKET_COMMAND;

		break;
	case SIERRA_PACKET_DATA:
	case SIERRA_PACKET_DATA_END:
		packet[1] = subtype;
		break;
	default:
		GP_DEBUG ("* unknown packet type!");
	}

	packet[2] = data_length &  0xff;
	packet[3] = (data_length >> 8) & 0xff;

	return GP_OK;
}

static int
sierra_write_ack (Camera *camera, GPContext *context)
{
	char buf[4096];
	int ret;

	GP_DEBUG ("Writing acknowledgement...");

	buf[0] = ACK;
	ret = sierra_write_packet (camera, buf, context);
	sierra_clear_usb_halt(camera);
	CHECK (ret);

	GP_DEBUG ("Successfully wrote acknowledgement.");
	return GP_OK;
}

int
sierra_init (Camera *camera, GPContext *context)
{
	unsigned char buf[SIERRA_PACKET_SIZE], packet[4096];
	int ret, r = 0;
	GPPortSettings settings;

	GP_DEBUG ("Sending initialization sequence to the camera");

	/* Only serial connections need to be initialized. */
	if (camera->port->type != GP_PORT_SERIAL)
		return GP_OK;

	/*
	 * It seems that the initialization sequence needs to be sent at
	 * 19200.
	 */
	CHECK (gp_port_get_settings (camera->port, &settings));
	if (settings.serial.speed != 19200) {
		settings.serial.speed = 19200;
		CHECK (gp_port_set_settings (camera->port, settings));
	}
	CHECK (gp_port_set_pin (camera->port, GP_PIN_DTR, GP_LEVEL_HIGH));

	packet[0] = NUL;

	while (1) {

		/* Send NUL */
		CHECK (sierra_write_packet (camera, (char *)packet, context));

		/* Read the response */
		ret = sierra_read_packet (camera, buf, context);
		if (ret == GP_ERROR_TIMEOUT) {
			if (++r > 2) {
				gp_context_error (context,
					_("Transmission timed out even after "
					  "2 retries. Giving up..."));
				return (GP_ERROR_TIMEOUT);
			}
			GP_DEBUG ("Retrying...");
			continue;
		}
		CHECK (ret);

		switch (buf[0]) {
		case SIERRA_PACKET_NAK:

			/* Everything is fine. */
			return GP_OK;

		case SIERRA_PACKET_SESSION_END:
		default:
			if (++r > 3) {
				gp_context_error (context,
					_("Got unexpected result "
					  "0x%x. Please contact "
					  "%s."),
					buf[0], MAIL_GPHOTO_DEVEL);
				return GP_ERROR;
			}
			break;
		}
	}
}

static struct {
        SierraSpeed speed;
        int bit_rate;
} SierraSpeeds[] = {
        {SIERRA_SPEED_9600  ,   9600},
        {SIERRA_SPEED_19200 ,  19200},
        {SIERRA_SPEED_38400 ,  38400},
        {SIERRA_SPEED_57600 ,  57600},
        {SIERRA_SPEED_115200, 115200},
        {0, 0}
};

int
sierra_set_speed (Camera *camera, SierraSpeed speed, GPContext *context)
{
	GPPortSettings settings;
	unsigned int i, bit_rate;

	/* Only serial connections have different speeds. */
	if (camera->port->type != GP_PORT_SERIAL)
		return GP_OK;

	/*
	 * Check that the requested speed is valid. We don't want to bug
	 * the user with our coding errors, therefore if the requested
	 * speed is invalid, we use 19200.
	 */
	for (i = 0; SierraSpeeds[i].bit_rate; i++)
		if (SierraSpeeds[i].speed == speed)
			break;
	if (SierraSpeeds[i].bit_rate)
		bit_rate = SierraSpeeds[i].bit_rate;
	else {
		GP_DEBUG ("Invalid speed %i. Using %i (19200, default).",
			  speed, SIERRA_SPEED_19200);
		speed = SIERRA_SPEED_19200;
		bit_rate = 19200;
	}

	/*
	 * Check if we need to change the speed or if we are currently
	 * using the requested speed. The reason why we are doing that is
	 * that (at least some) cameras reject the request for a speed if
	 * they are currently operating at that speed.
	 */
	CHECK (gp_port_get_settings (camera->port, &settings));
	if ((unsigned int)settings.serial.speed == bit_rate)
		return GP_OK;

	/*
	 * Tell the camera about the new speed. Note that a speed change
	 * automatically starts a new session.
	 */
	GP_DEBUG ("Setting speed to %i (%i bps)", speed, bit_rate);
	camera->pl->first_packet = 1;
	CHECK (sierra_set_int_register (camera, 17, speed, context));

	/* Now switch the port to the new speed. */
	CHECK (gp_port_get_settings (camera->port, &settings));
	settings.serial.speed = bit_rate;
	CHECK (gp_port_set_settings (camera->port, settings));
	CHECK (gp_port_set_pin (camera->port, GP_PIN_DTR, GP_LEVEL_HIGH));

	usleep (10 * 1000);
	return GP_OK;
}

int sierra_sub_action (Camera *camera, SierraAction action, int sub_action,
		       GPContext *context)
{
	char buf[SIERRA_PACKET_SIZE];

	CHECK (sierra_build_packet (camera, SIERRA_PACKET_COMMAND, 0, 3, buf));
	buf[4] = 0x02;
	buf[5] = action;
	buf[6] = sub_action;

	GP_DEBUG ("sierra_sub_action: action %d, sub action %d", action,
		  sub_action);
	CHECK (sierra_transmit_ack (camera, buf, context));
	GP_DEBUG ("Waiting for acknowledgement...");
	CHECK (sierra_read_packet_wait (camera, buf, context));

	switch (buf[0]) {
	case ACK: /* reported on capture on Olympus 4040 */
	case SIERRA_PACKET_ENQ:
		return (GP_OK);
	default:
		gp_context_error (context, _("Received unexpected answer "
		"(%i). Please contact %s."), buf[0], MAIL_GPHOTO_DEVEL);
		return (GP_ERROR);
	}

	return GP_OK;
}

static int
sierra_action (Camera *camera, SierraAction action, GPContext *context)
{
	return(sierra_sub_action (camera, action, 0, context));
}

int
sierra_set_int_register (Camera *camera, int reg, int value,
			 GPContext *context)
{
	char p[4096];

	GP_DEBUG ("sierra_set_int_register: register %i value %i", reg, value);

	CHECK (sierra_build_packet (camera, SIERRA_PACKET_COMMAND, 0,
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

	/* Send packet */
	CHECK (sierra_transmit_ack (camera, p, context));

	return GP_OK;
}

int sierra_get_int_register (Camera *camera, int reg, int *value, GPContext *context)
{
	int r = 0;
	unsigned char p[4096], buf[SIERRA_PACKET_SIZE];

	GP_DEBUG ("sierra_get_int_register: register 0x%02x...", reg);

	/* Build and send the packet. */
	CHECK (sierra_build_packet (camera, SIERRA_PACKET_COMMAND, 0, 2, (char *)p));
	p[4] = 0x01;
	p[5] = reg;
	CHECK (sierra_write_packet (camera, (char *)p, context));

	while (1) {

		/* Read the response */
		buf[0] = 0;
		CHECK (sierra_read_packet_wait (camera, (char *)buf, context));
		GP_DEBUG ("Successfully read packet. Interpreting result "
			  "(0x%02x)", buf[0]);
		switch (buf[0]) {
		case SIERRA_PACKET_INVALID:
			gp_context_error (context, _("Could not get "
				"register %i. Please contact "
				"%s."), reg, MAIL_GPHOTO_DEVEL);
			return GP_ERROR;
		case SIERRA_PACKET_DATA_END:
			r = ((unsigned char) buf[4]) +
			    ((unsigned char) buf[5] * 256) +
			    ((unsigned char) buf[6] * 65536) +
			    ((unsigned char) buf[7] * 16777216);
			*value = r;
			GP_DEBUG ("Value of register 0x%02x: 0x%02x. ", reg, r);
			CHECK (sierra_write_ack (camera, context));
			GP_DEBUG ("Read value of register 0x%02x and wrote "
				  "acknowledgement. Returning.", reg);
			return GP_OK;

		case SIERRA_PACKET_SESSION_END:
		case SIERRA_PACKET_SESSION_ERROR:
		case SIERRA_PACKET_WRONG_SPEED:
			if (++r > 2) {
				gp_context_error (context, _("Too many "
						"retries failed."));
				return GP_ERROR;
			}

			/*
			 * The camera has ended this session and
			 * reverted the speed back to 19200. Reinitialize
			 * the connection and resend the packet. We use the
			 * default speed from now on.
			 */
			CHECK (sierra_init (camera, context));
			CHECK (sierra_set_speed (camera, SIERRA_SPEED_19200,
						 context));
			CHECK (sierra_write_packet (camera, (char *)p, context));

			break;

		default:
			if (++r > 2) {
				gp_context_error (context, _("Too many "
					"retries failed."));
				return GP_ERROR;
			}

			/* Tell the camera to send again. */
			CHECK (sierra_write_nak (camera, context));
			break;
		}
	}

	return GP_ERROR_IO;
}

int
sierra_set_string_register (Camera *camera, int reg, const char *s,
			    long int length, GPContext *context)
{

	char packet[4096];
	char type;
	long int x=0;
	int seq=0, size=0;
	int do_percent;
	unsigned int id = 0;

	GP_DEBUG ("sierra_set_string_register: reg %i, value '%s'", reg, s);

	/* Make use of the progress bar when the packet is "large enough" */
	if (length > MAX_DATA_FIELD_LENGTH) {
		do_percent = 1;
		/*
		 * This code can lead to recursive calls in gtkam - the
		 * main problem is when thumbnails are being downloaded,
		 * gtkam tries to download another thumbnail when
		 * gp_context_progress_update() is called. There are also
		 * other areas in gtkam that can recursively call into
		 * this function.
		 */
		id = gp_context_progress_start (context, length,
						_("Sending data..."));
	}
	else
		do_percent = 0;

	while (x < length) {
		if (x == 0) {
			type = SIERRA_PACKET_COMMAND;
			size = (length + 2 - x) > MAX_DATA_FIELD_LENGTH
				? MAX_DATA_FIELD_LENGTH : length + 2;
		}  else {
			size = (length - x) > MAX_DATA_FIELD_LENGTH
				? MAX_DATA_FIELD_LENGTH : length-x;
			if (x + size < length)
				type = SIERRA_PACKET_DATA;
			else
				type = SIERRA_PACKET_DATA_END;
		}
		CHECK (sierra_build_packet (camera, type, seq, size, packet));

		if (type == SIERRA_PACKET_COMMAND) {
			packet[4] = 0x03;
			packet[5] = reg;
			memcpy (&packet[6], &s[x], size-2);
			x += size - 2;
		} else  {
			packet[1] = seq++;
			memcpy (&packet[4], &s[x], size);
			x += size;
		}

		/* Transmit packet */
		CHECK (sierra_transmit_ack (camera, packet, context));
		if (do_percent)
			gp_context_progress_update (context, id, x);
	}
	if (do_percent)
		gp_context_progress_stop (context, id);

	return GP_OK;
}

int sierra_get_string_register (Camera *camera, int reg, int fnumber,
                                CameraFile *file, unsigned char *b,
				unsigned int *b_len, GPContext *context)
{
	unsigned char p[34816];
	unsigned int packlength, total = (b_len) ? *b_len : 0;
	int retries, r;
	unsigned int min_progress_bytes;
	static int in_function = 0;
	unsigned int id = 0;

	GP_DEBUG ("sierra_get_string_register:  reg %i, file number %i, "
		  " total %d, flags 0x%x", reg, fnumber, total,
		  camera->pl->flags);

	if (in_function != 0) {
		gp_context_error (context, _("recursive calls are not"
			" supported by the sierra driver! Please contact"
			" %s."), MAIL_GPHOTO_DEVEL);
		return GP_ERROR;
	}
	in_function = 1;

	/* Set the current picture number */
	if (fnumber >= 0)
		CHECK (sierra_set_int_register (camera, 4, fnumber, context));

	/* Build and send the request */
	CHECK (sierra_build_packet (camera, SIERRA_PACKET_COMMAND, 0, 2, (char *)p));
	/*
	 * If the extended protocol is enabled, use code 0x06 so we can send
	 * and receive 32k size packages, otherwise code 0x04 is used and
	 * we have 2k size packages.
	 */
	if (camera->pl->flags & SIERRA_EXT_PROTO) {
		p[4] = 0x06;
		min_progress_bytes = 32 * 1024;
	} else {
		p[4] = 0x04;
		min_progress_bytes = 2 * 1024;
	}
	p[5] = reg;
	CHECK (sierra_write_packet (camera, (char *)p, context));

	if (file && total > min_progress_bytes) {
		id = gp_context_progress_start (context, total, _("Downloading data..."));
	}

	/* Read all the data packets */
	*b_len = 0;
	retries = 0;
	do {

		/* Read one packet and retry on timeout. */
		r = sierra_read_packet (camera, p, context);
		if (r == GP_ERROR_TIMEOUT) {
			if (++retries > RETRIES) {
				in_function = 0;
				return (r);
			}
			GP_DEBUG ("Timeout! Retrying (%i of %i)...",
				  retries, RETRIES);
			CHECK (sierra_write_nak (camera, context));
			continue;
		}
		CHECK (r);

		GP_DEBUG ("sierra_get_string_register p[0] is %d", p[0]);
		switch (p[0]) {
		case SIERRA_PACKET_INVALID:
			gp_context_error (context, _("Could not get "
				"string register %i. Please contact "
				"%s."), reg, MAIL_GPHOTO_DEVEL);
			in_function = 0;
			return GP_ERROR;
		default:
			break;
		}
		CHECK (sierra_write_ack (camera, context));

		packlength = p[2] | (p[3] << 8);
		GP_DEBUG ("Packet length: %d", packlength);

		if (b)
			memcpy (&b[*b_len], &p[4], packlength);
		*b_len += packlength;

		if (file) {
		    CHECK (gp_file_append (file, (char *)&p[4], packlength));
		    if (total > min_progress_bytes)
			gp_context_progress_update (context, id, *b_len);
		}

	} while (p[0] != SIERRA_PACKET_DATA_END);
	if (file && total > min_progress_bytes)
		gp_context_progress_stop (context, id);

	GP_DEBUG ("sierra_get_string_register: completed OK, *b_len %d",
		  *b_len);
	in_function = 0;
	return GP_OK;
}

int
sierra_delete_all (Camera *camera, GPContext *context)
{
	CHECK (sierra_action (camera, SIERRA_ACTION_DELETE_ALL, context));

	return GP_OK;
}

int
sierra_delete (Camera *camera, int picture_number, GPContext *context)
{
	/* Tell the camera which picture to delete and execute command. */
	CHECK (sierra_set_int_register (camera, 4, picture_number, context));
	CHECK (sierra_action (camera, SIERRA_ACTION_DELETE, context));

	return GP_OK;
}

int
sierra_end_session (Camera *camera, GPContext *context)
{
	CHECK (sierra_action (camera, SIERRA_ACTION_END, context));

	return GP_OK;
}

int
sierra_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	unsigned int size;

	/* Send to the camera the capture request and wait
	   for the completion */
	CHECK (sierra_action (camera, SIERRA_ACTION_PREVIEW, context));

	/* Retrieve the preview and set the MIME type */
	CHECK (sierra_get_int_register (camera, 12, (int *)&size, context));
	CHECK (sierra_get_string_register (camera, 14, 0, file, NULL, &size,
					   context));
	CHECK (gp_file_set_mime_type (file, GP_MIME_JPEG));

	return GP_OK;
}

int
sierra_capture (Camera *camera, CameraCaptureType type,
		CameraFilePath *filepath, GPContext *context)
{
	int n, r;
	unsigned int len = 0;
	char filename[128];
	char *folder;
	int timeout;

	GP_DEBUG ("* sierra_capture");

	/* We can only capture images */
	if (type != GP_CAPTURE_IMAGE)
		return GP_ERROR_NOT_SUPPORTED;

	/*
	 * This is a non-fatal check if a memory card is present. Some
	 * cameras don't understand this command and error out here.
	 * Since some cameras do not like it at all, do not send it for those.
	 */
	if (!(camera->pl->flags & SIERRA_NO_51)) {
		r = sierra_get_int_register (camera, 51, &n, context);
		if ((r >= 0) && (n == 1)) {
			gp_context_error (context, _("No memory card present"));
			return GP_ERROR_NOT_SUPPORTED;
		}
	}

	/*
	 * Raise the timeout before the capture, since it can take longer,
	 * the shutter speed alone could push us over 10 seconds.
	 */
	CHECK (gp_port_get_timeout (camera->port, &timeout));
	CHECK (gp_port_set_timeout (camera->port, 20000)); /* 20 seconds */
	/* Send to the camera the capture request and wait
	   for the completion */
	CHECK (sierra_action (camera, SIERRA_ACTION_CAPTURE, context));
	CHECK (gp_port_set_timeout (camera->port, timeout));

	if (filepath != NULL) {
		/*
		 * After the picture is taken, register 4 is set to the
		 * current picture.
		 */
		GP_DEBUG ("Getting picture number.");
		r = sierra_get_int_register (camera, 4, &n, context);
		if (r == GP_OK) {
			GP_DEBUG ("Getting filename of file %i.", n);
		}
		/*
		 * We need to tell the frontend where the new image can be
		 * found.  Unfortunately, we can only figure out the
		 * filename. Therefore, reset the CameraFilesystem and let
		 * it search for the filename.
		 *
		 * If you know how to figure out where the image gets
		 * saved, please submit a patch.
		 *
		 * Not that some cameras that don't support filenames will
		 * return 8 blanks instead of reporting an error.
		 *
		 * Some newer cameras (Nikon*) do not return register 4, so
		 * ignore errors from that.
		 */
		CHECK (sierra_get_string_register (camera, 79, 0, NULL,
						   (unsigned char *)filename,
						   &len, context));
		if ((len <= 0) || !strcmp (filename, "        "))
			snprintf (filename, sizeof (filename), "P101%04i.JPG", n);
		GP_DEBUG ("... done ('%s')", filename);
		CHECK (gp_filesystem_reset (camera->fs));
		CHECK (gp_filesystem_get_folder (camera->fs, filename,
						 &folder, context));
		strncpy (filepath->folder, folder, sizeof (filepath->folder));
		strncpy (filepath->name, filename, sizeof (filepath->name));
	}

	GP_DEBUG ("* sierra_capture completed OK");
	return GP_OK;
}

int sierra_upload_file (Camera *camera, CameraFile *file, GPContext *context)
{
	const char *data;
	long unsigned int data_size;

	/* Put the "magic spell" in register 32 */
	CHECK (sierra_set_int_register (camera, 32, 0x0FEC000E, context));

	/* Upload the file */
	CHECK (gp_file_get_data_and_size (file, &data, &data_size));
	CHECK (sierra_set_string_register (camera, 29, data, data_size, context));

	/* Send command to order the transfer into NVRAM and wait
	   for the completion */
	CHECK (sierra_action (camera, SIERRA_ACTION_UPLOAD, context));

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
	int value, audio_info[8];

	CHECK (sierra_get_string_register (camera, 47, n, NULL, buf,
					   &buf_len, context));
	if (buf_len == 0) {
		/*
		 * Many (older) serial cameras seem to return 0 bytes back
		 * and effectively do not support register 47. Try using
		 * other registers to get the data.
		 *
		 * Don't fail on error so unsupported or semi-broken
		 * behaviour does not prevent downloads or other use.
		 */
		memset(pic_info, 0, sizeof(*pic_info));
		if (sierra_get_size(camera, 12, n, &value, context) == GP_OK) {
			pic_info->size_file = value;
		}
		if (sierra_get_size(camera, 13, n, &value, context) == GP_OK) {
			pic_info->size_preview = value;
		}
		/*
		 * If the following hangs any cameras, just delete this
		 * code, and assume that cameras with audio will get audio
		 * file information via reg 47.
		 *
		 * Note that the value variable as used here is number of
		 * bytes transferred, not the returned data. Some
		 * unsupported sierra commands return zero bytes.
		 */
		if ((sierra_get_string_register (camera, 43, n, NULL,
				(unsigned char *) &audio_info,
				(unsigned int *)&value,
				context) == GP_OK) && (value != 0)) {
			pic_info->size_audio = audio_info[0];
		}
		if (sierra_get_int_register (camera, 39, &value, context)
		    == GP_OK) {
			pic_info->locked = value;
		} else {
			/*
			 * Default to marking as locked (not-deletable).
			 * It looks the file can still be deleted if the
			 * file is not really locked.
			 */
			pic_info->locked = SIERRA_LOCKED_YES;
		}
		return (GP_OK);
	}

	if (buf_len != 32) {
		gp_context_error (context, _("Expected 32 bytes, got %i. "
			"Please contact %s."), buf_len, MAIL_GPHOTO_DEVEL);
		return (GP_ERROR_CORRUPTED_DATA);
	}

	pic_info->size_file      = get_int (buf);
	pic_info->size_preview   = get_int (buf + 4);
	pic_info->size_audio     = get_int (buf + 8);
	pic_info->resolution     = get_int (buf + 12);
	pic_info->locked         = get_int (buf + 16);
	pic_info->date           = get_int (buf + 20);
	pic_info->animation_type = get_int (buf + 28);

	/* Make debugging easier */
	GP_DEBUG ("sierra_get_pic_info ");
	GP_DEBUG ("File size: %d",      pic_info->size_file);
	GP_DEBUG ("Preview size: %i",   pic_info->size_preview);
	GP_DEBUG ("Audio size: %i",     pic_info->size_audio);
	GP_DEBUG ("Resolution: %i",     pic_info->resolution);
	GP_DEBUG ("Locked: %i",         pic_info->locked);
	GP_DEBUG ("Date: %i",           pic_info->date);
	GP_DEBUG ("Animation type: %i", pic_info->animation_type);

	return GP_OK;
}

int sierra_set_locked (Camera *camera, unsigned int n, SierraLocked locked,
		       GPContext *context)
{
	CHECK (sierra_set_int_register (camera, 4, n, context));
	CHECK (sierra_sub_action (camera, SIERRA_ACTION_PROT_STATE,
				  (int) locked, context));
	return GP_OK;
}

int sierra_get_size (Camera *camera, int reg, unsigned int n, int *value,
		     GPContext *context)
{
	CHECK (sierra_set_int_register (camera, 4, n, context));
	CHECK (sierra_get_int_register (camera, reg, value, context));
	return GP_OK;
}
