#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <time.h>
#include "sierra.h"
#include "sierra-usbwrap.h"
#include "library.h"

#define		QUICKSLEEP	5

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
	
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_valid_type return '%s'",
			 gp_result_as_string(ret_status));

	return ret_status;
}

/**
 * sierra_is_single_byte_packet:
 * @b : first byte of the packet to be checked
 *
 * Check if the received packet is a single byte length packet
 * Method: Check if the byte is either TYPE_DATA or TYPE_DATA_END.
 *
 * Returns: 1 if the packet is a single byte length packet, 0 otherwise
 */
static int sierra_is_single_byte_packet(char b)
{
	int ret_status = 1;

	if (b == TYPE_DATA ||
	    b == TYPE_DATA_END)
		ret_status = 0;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra",
			 "* sierra_is_single_byte_packet return %d", ret_status);

	return ret_status;
}

int sierra_change_folder (Camera *camera, const char *folder)
{	
	SierraData *fd;
	int st = 0,i = 1;
	char target[128];

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** sierra_change_folder");

	if (!folder || !camera)
		return GP_ERROR_BAD_PARAMETERS;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);

	fd = camera->camlib_data;

	/*
	 * Do not issue the command if the camera doesn't support folders 
	 * or if the folder is the current working folder
	 */
	if (!fd->folders || !strcmp (fd->folder, folder))
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
		CHECK (sierra_set_string_register (camera, 84, "\\", 1));
	}
	st = i;
	for (; target[i]; i++) {
		if (target[i] == '/') {
			target[i] = '\0';
			if (st == i - 1)
				break;
			CHECK (sierra_set_string_register (camera, 84, 
							   target + st, strlen (target + st)));

			st = i + 1;
			target[i] = '/';
		}
	}
	strcpy (fd->folder, folder);

	return GP_OK;
}

int sierra_list_files (Camera *camera, const char *folder, CameraList *list)
{
	int count, i, bsize;
	char buf[1024];

	CHECK (sierra_change_folder (camera, folder));
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** counting files in '%s'"
			 "...", folder);
	CHECK (sierra_get_int_register (camera, 10, &count));
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** found %i files", count);
	for (i = 0; i < count; i++) {
		CHECK (sierra_set_int_register (camera, 4, i + 1));
		gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** getting "
				 "filename of picture %i...", i);
		CHECK (sierra_get_string_register (camera, 79, 0, NULL,
						   buf, &bsize));
		if (bsize <= 0)
			sprintf (buf, "P101%04i.JPG", i);
		CHECK (gp_list_append (list, buf, NULL));
	}

	return (GP_OK);
}

int sierra_list_folders (Camera *camera, const char *folder, CameraList *list)
{
	SierraData *fd = camera->camlib_data;
	int i, j, count, bsize;
	char buf[1024];

	/* List the folders only if the camera supports them */
	if (!fd->folders)
		return (GP_OK);

	CHECK (sierra_change_folder (camera, folder));
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** counting folders "
			 "in '%s'...", folder);
	CHECK (sierra_get_int_register (camera, 83, &count));
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** found %i folders", count);
	for (i = 0; i < count; i++) {
		CHECK (sierra_change_folder (camera, folder));
		CHECK (sierra_set_int_register (camera, 83, i + 1));
		bsize = 1024;
		gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** getting "
				 "name of folder %i...", i + 1);
		CHECK (sierra_get_string_register (camera, 84, 0, 
						   NULL, buf, &bsize));

		/* Remove trailing spaces */
		for (j = strlen (buf) - 1; j >= 0 && buf[j] == ' '; j--)
			buf[j] = '\0';
		gp_list_append (list, buf, NULL);
	}

	return (GP_OK);
}

int sierra_write_packet (Camera *camera, char *packet)
{
	int x, ret, r, checksum=0, length;
	SierraData *fd = (SierraData*)camera->camlib_data;
	
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_write_packet");

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
		if (fd->usb_wrap)
			return (usb_wrap_write_packet (camera->port, packet, 
						       length));
		else
			return (gp_port_write (camera->port, packet, length));
	}

	/* SERIAL */
	for (r = 0; r < RETRIES; r++) {

		ret = gp_port_write (camera->port, packet, length);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;

		return (ret);
	}

	return (GP_ERROR_IO_TIMEOUT);
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
int sierra_read_packet (Camera *camera, char *packet)
{
	int y, r = 0, ret_status = GP_ERROR_IO, done = 0, length;
	int blocksize = 1, bytes_read;
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_read_packet");

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

		if (r > 0) {
			gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* reading again...");
		}

		/* Clear the USB bus
		   (what about the SERIAL bus : do we need to flush it?) */
		if (camera->port->type == GP_PORT_USB && !fd->usb_wrap)
			gp_port_usb_clear_halt(camera->port, GP_PORT_USB_ENDPOINT_IN);


		/* Read data through the bus */
		if (camera->port->type == GP_PORT_USB && fd->usb_wrap)
			bytes_read = usb_wrap_read_packet (camera->port, packet, blocksize);
		else
			bytes_read = gp_port_read (camera->port, packet, blocksize);
		
		gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* bytes_read: %d", bytes_read);
		gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* packet[0] : %x",
				 (unsigned char) packet[0]);

		/* If an error was encoutered reading data though the bus, try again */
		if (bytes_read < 0) {
			continue;
		}

		/* If the first read byte is not known, exit the processing */
		else if ( sierra_valid_type(packet[0]) != GP_OK ) {

			ret_status = GP_ERROR_CORRUPTED_DATA;
			done = 1;
		}

		/* For single byte data packets, the work is done... */      
		else if ( sierra_is_single_byte_packet(packet[0]) ) {
			ret_status = GP_OK;
			done = 1;
		}

		/* Process several byte length packet */
		else {

			/* The data packet size is not known yet if the communication
			   is performed through the serial bus : read it */
			if (camera->port->type == GP_PORT_SERIAL) {
				bytes_read = gp_port_read(camera->port, packet+1, 3);
				if (bytes_read < 0) {
					/* Maybe we should retry here? */
					ret_status = GP_ERROR_IO;
					done = 1;
					break;
				}
				else
					bytes_read = 4;
			}
	 
			/* Determine the packet length (add 6 for the packet type,
			   the packet subtype or sequence, the length of data and the checksum
			   see description) */
			length = (unsigned char)packet[2] + ((unsigned char)packet[3]*256) + 6;

			/* Read until the end of the packet is reached
			   or an IO error occured */
			for (ret_status=GP_OK, y = bytes_read ; ret_status == GP_OK && y < length ; 
			     y += blocksize) {
				bytes_read = gp_port_read (camera->port, &packet[y], blocksize);
				if (bytes_read < 0)
					ret_status = bytes_read;
			}

			/* Retry on IO error only */
			if (ret_status == GP_ERROR_IO_TIMEOUT) {
				sierra_write_nak (camera);
				GP_SYSTEM_SLEEP(10);
			}
			else
				done = 1;
		}
	}

	if (camera->port->type == GP_PORT_USB && !fd->usb_wrap)
		gp_port_usb_clear_halt(camera->port, GP_PORT_USB_ENDPOINT_IN);

	gp_debug_printf(GP_DEBUG_HIGH, "sierra", "* sierra_read_packet return '%s'",
			gp_result_as_string(ret_status));

	return ret_status;
}

int sierra_build_packet (Camera *camera, char type, char subtype, 
			 int data_length, char *packet) 
{
	SierraData *fd = (SierraData*)camera->camlib_data;

	packet[0] = type;
	switch (type) {
	case TYPE_COMMAND:
		if (camera->port->type == GP_PORT_USB)
				/* USB cameras don't care about first packets */
			fd->first_packet = 0;
		if (fd->first_packet)
			packet[1] = SUBTYPE_COMMAND_FIRST;
		else
			packet[1] = SUBTYPE_COMMAND;
		fd->first_packet = 0;
		break;
	case TYPE_DATA:
	case TYPE_DATA_END:
		packet[1] = subtype;
		break;
	default:
		gp_debug_printf (GP_DEBUG_HIGH, "sierra", 
				 "* unknown packet type!");
	}

	packet[2] = data_length &  0xff;
	packet[3] = (data_length >> 8) & 0xff;

	return (GP_OK);
}

int sierra_write_ack (Camera *camera) 
{
	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;
	int ret;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_write_ack");
	
	buf[0] = ACK;
	ret = sierra_write_packet (camera, buf);
	if (camera->port->type == GP_PORT_USB && !fd->usb_wrap)
		gp_port_usb_clear_halt (camera->port, GP_PORT_USB_ENDPOINT_IN);
	return (ret);
}

int sierra_write_nak (Camera *camera) 
{
	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;
	int ret;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_write_nack");

	buf[0] = NAK;
	ret = sierra_write_packet (camera, buf);
	if (camera->port->type == GP_PORT_USB && !fd->usb_wrap)
		gp_port_usb_clear_halt(camera->port, GP_PORT_USB_ENDPOINT_IN);
	return (ret);
}

int sierra_ping (Camera *camera) 
{
	int ret, r = 0;
	char buf[4096];

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_ping");

	if (camera->port->type == GP_PORT_USB) {
		gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_ping no "
				 "ping for USB");
		return (GP_OK);
	}
	
	buf[0] = NUL;
	for (r = 0; r < RETRIES; r++) {

		ret = sierra_write_packet (camera, buf);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return ret;;

		ret = sierra_read_packet (camera, buf);
		if (ret == GP_ERROR_IO_TIMEOUT)
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

int sierra_set_speed (Camera *camera, int speed) 
{
	gp_port_settings settings;
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_set_speed");
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* speed: %i", speed);

	fd->first_packet = 1;

	gp_port_settings_get (camera->port, &settings);

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

	CHECK (sierra_set_int_register (camera, 17, speed));

	CHECK (gp_port_settings_set (camera->port, settings));

	GP_SYSTEM_SLEEP(10);
	return GP_OK;
}

int sierra_set_int_register (Camera *camera, int reg, int value) 
{
	int r=0;
	char p[4096];
	char buf[4096];
	int ret;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_set_int_register");
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* register: %i", reg);
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* value:    %i", value);

	if (value < 0) {
		CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 2, p));
	} else {
		CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 6, p));
	}

	/* Fill in the data */
	p[4] = 0x00;
	p[5] = reg;
	if (value >= 0) {
		p[6] = (value)     & 0xff;
		p[7] = (value>>8)  & 0xff;
		p[8] = (value>>16) & 0xff;
		p[9] = (value>>24) & 0xff;
	}

	for (r = 0; r < RETRIES; r++) {

		CHECK (sierra_write_packet (camera, p));

		ret = sierra_read_packet (camera, buf);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		CHECK (ret);

		if (buf[0] == ACK)
			return GP_OK;

		/* DC1 = invalid register or value */
		if (buf[0] == DC1)
			return GP_ERROR_BAD_PARAMETERS;
	}

	sierra_set_speed (camera, -1);

	return GP_ERROR_IO;
}

int sierra_get_int_register (Camera *camera, int reg, int *value) 
{
	int r = 0, write_nak = 0;
	char packet[4096];
	char buf[4096];
	int ret;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_get_int_register");
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* register: %i", reg);

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 2, packet));

	/* Fill in the data */
	packet[4] = 0x01;
	packet[5] = reg;

	while (r++ < RETRIES) {
		if (write_nak)
			CHECK (sierra_write_nak (camera))
		else
			CHECK (sierra_write_packet (camera, packet));

		ret = sierra_read_packet (camera, buf);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		CHECK (ret);

		/* DC1 = invalid register or value */
		if (buf[0] == DC1)
			return GP_ERROR_BAD_PARAMETERS;

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

int sierra_set_string_register (Camera *camera, int reg, char *s, int length) 
{

	char packet[4096], buf[4096];
	char type;
	unsigned char c;
	int x=0, seq=0, size=0, done, r, ret;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", 
			 "* sierra_set_string_register");
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* register: %i", reg);
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* value: %s", s);

	while (x < length) {
		if (x == 0) {
			type = TYPE_COMMAND;
			size = (length + 2 - x) > 2048? 2048 : length + 2;
		}  else {
			size = (length - x) > 2048? 2048 : length-x;
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
			if (ret == GP_ERROR_IO_TIMEOUT)
				continue;
			CHECK (ret);

			ret = sierra_read_packet (camera, buf);
			if (ret == GP_ERROR_IO_TIMEOUT)
				continue;
			CHECK (ret);

			c = (unsigned char)buf[0];
			if (c == DC1)
				return GP_ERROR_BAD_PARAMETERS;
			if (c == ACK)
				done = 1;
			else	{
				if (c != NAK)
					return GP_ERROR_IO;
			}

		}
		if (r > RETRIES) {
			return GP_ERROR_IO;
		}
	}
	return GP_OK;
}

int sierra_get_string_register (Camera *camera, int reg, int file_number, 
                                CameraFile *file, char *s, int *length) {

	char packet[4096];
	int done, x, packlength, do_percent, l;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", 
			 "* sierra_get_string_register");
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* register: %i", reg);
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* file number: %i", 
			 file_number);

	do_percent = 1;

	/* Set the current picture number */
	if (file_number >= 0)
		CHECK (sierra_set_int_register (camera, 4, file_number));

	switch (reg) {
	case 14:

		/* Get the size of the current image */
		CHECK (sierra_get_int_register (camera, 12, &l));

		break;
	case 15:
	
		/* Get the size of the current thumbnail */
		CHECK (sierra_get_int_register (camera, 13, &l));

		break;
	case 44:

		/* Get the size of the current audio will do */
		//FIXME
		break;
	default:
		do_percent = 0;
	}

	/* Build and send the request */
	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 2, packet));
	packet[4] = 0x04;
	packet[5] = reg;
	CHECK (sierra_write_packet (camera, packet));

	/* Read all the data packets */
	x = 0; done = 0;
	while (!done) {
		CHECK (sierra_read_packet (camera, packet));

		if (packet[0] == DC1)
			return GP_ERROR_BAD_PARAMETERS;

		CHECK (sierra_write_ack (camera));

		packlength = ((unsigned char)packet[2]) +
			((unsigned char)packet[3]  * 256);

		if (s)
			memcpy (&s[x], &packet[4], packlength);

		if (file) {
			CHECK (gp_file_append (file, &packet[4], packlength));
			if (do_percent)
				gp_camera_progress (camera,
					(float)((float)(x+packlength)/(float)(l)));
		}

		x += packlength;
		if (packet[0] == TYPE_DATA_END)
			done = 1;
	}

	if (length)
		*length = x;

	return (GP_OK);
}

int sierra_delete_all (Camera *camera)
{
	char packet[4096], buf[4096], r, done, ret;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_delete_all");

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet));

	packet[4] = 0x02;
	packet[5] = 0x01;
	packet[6] = 0x00;

	/* Send the command delete all */
	CHECK (sierra_write_packet (camera, packet));

	/* Read command acknowledgement */
	CHECK (sierra_read_packet (camera, buf));
	if (buf[0] != ACK)
		return GP_ERROR_CORRUPTED_DATA;

	/* Wait for the action complete notification */
	r = 0, done = 0;
	while ( !done && r < RETRIES ) {

		GP_SYSTEM_SLEEP (QUICKSLEEP);

		ret = sierra_read_packet (camera, buf);
		if (ret == GP_OK) {
			done = 1;
			ret  = (buf[0] == ENQ) ? GP_OK : GP_ERROR_CORRUPTED_DATA;
		}
	}

	return ret;
}

int sierra_delete (Camera *camera, int picture_number) 
{
	char packet[4096], buf[4096];
	int r, done, ret;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_delete");
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* picture: %i", 
			 picture_number);

	CHECK (sierra_set_int_register (camera, 4, picture_number));

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet));

	packet[4] = 0x02;
	packet[5] = 0x07;
	packet[6] = 0x00;

	r = 0; done = 0;
	while ((!done) && (r++<RETRIES)) {

		ret = sierra_write_packet (camera, packet);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		CHECK (ret);

		ret = sierra_read_packet (camera, buf);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		CHECK (ret);

		done = (buf[0] == NAK)? 0 : 1;

		if (done) {
			/* read in the ENQ */
			if (sierra_read_packet (camera, buf) != GP_OK)
				return ((buf[0] == ENQ)? GP_OK : GP_ERROR_IO);
			
		}
	}
	if (r > RETRIES)
		return (GP_ERROR_IO);

	GP_SYSTEM_SLEEP(QUICKSLEEP);

	return (GP_OK);
}

int sierra_end_session (Camera *camera) 
{
	char packet[4096], buf[4096];
	unsigned char c;
	int r, done;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_end_session");

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet));
	packet[4] = 0x02;
	packet[5] = 0x04;
	packet[6] = 0x00;

	r = 0; done = 0;
	while ((!done) && (r++<RETRIES)) {
		CHECK (sierra_write_packet (camera, packet));
		CHECK (sierra_read_packet (camera, buf));

		c = (unsigned char)buf[0];
		if (c == TRM)
			return (GP_OK);

		done = (c == NAK)? 0 : 1;

		if (done) {

			/* read in the ENQ */
			CHECK (sierra_read_packet (camera, buf));
			c = (unsigned char)buf[0];
			return ((c == ENQ)? GP_OK : GP_ERROR_IO);
		}
	}
	if (r > RETRIES)
		return (GP_ERROR_IO);

	return (GP_OK);
}

static int do_capture (Camera *camera, char *packet)
{
	char buf[8];
	int r, ret;

	CHECK (sierra_write_packet (camera, packet));

	for (r = 0; r < RETRIES; r++) {
		
		ret = sierra_read_packet (camera, buf);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		CHECK (ret);

		switch ((unsigned char)buf[0]) {
		case ENQ:
		case TRM:
			return GP_OK;
		case NAK:
		case ACK:
			continue;
		case DC1:
			/* Occurs when there is no space left on smart-card
			   or the lens protection is in place...*/
			return GP_ERROR_BAD_CONDITION;
		default:
			return GP_ERROR_CORRUPTED_DATA;
		}
	}
	
	return (GP_ERROR_IO);
}

int sierra_capture_preview (Camera *camera, CameraFile *file)
{
	char packet[4096];

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_capture_preview");

	if (file == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
	
	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet));

	packet[4] = 0x02;
	packet[5] = 0x05;
	packet[6] = 0x00;

	/* Capture the preview, retreive it and set the mime type */
	CHECK (do_capture (camera, packet));
	CHECK (sierra_get_string_register (camera, 14, 0, file, NULL, NULL));
	CHECK (gp_file_set_mime_type (file, "image/jpeg"));

	return (GP_OK);
}

int sierra_capture (Camera *camera, int capture_type, CameraFilePath *filepath)
{
	int picnum, length;
	char packet[4096];
	char buf[128];
	SierraData *fd;
	const char *folder;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_capture");

	/* Various preliminary checks */
	if (!camera)
		return (GP_ERROR_BAD_PARAMETERS);
	if (filepath == NULL) 
		return (GP_ERROR_BAD_PARAMETERS);
	if (capture_type != GP_OPERATION_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Get camera specific data */
	fd = camera->camlib_data;

	/* Build and send to the camera the capture request */
	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet));
	packet[4] = 0x02;
	packet[5] = 0x02;
	packet[6] = 0x00;
	CHECK (do_capture (camera, packet));

	/* After picture is taken, register 4 is set to current picture */
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* get picture number...");
	CHECK (sierra_get_int_register (camera, 4, &picnum));

	/* Get the picture filename */
	gp_debug_printf (GP_DEBUG_HIGH, "sierra",
			 "*** getting filename of picture %i...", picnum);
	CHECK (sierra_get_string_register (camera, 79, 0, NULL, buf, &length));
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* filename: %s", buf);

	/* Set the picture filename and folder */
	if (length > 0) {

		/*
		 * Get the folder of the new file. The filesystem doesn't
		 * know about the new file yet, therefore just format it.
		 *
		 * If somebody figures out how to find the location where
		 * the picture has been taken, this code can be improved.
		 * Until then...
		 */
		CHECK (gp_filesystem_reset (camera->fs));
		CHECK (gp_filesystem_get_folder (camera->fs, buf, &folder));
		strcpy (filepath->folder, folder);
		strcpy (filepath->name, buf);

	} else {
		/* Filename not supported. May it be possible? */
		//FIXME: Which filename???
		strcpy (filepath->name, "");
	}

	return (GP_OK);
}
