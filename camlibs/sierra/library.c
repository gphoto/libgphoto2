#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <time.h>
#include "sierra.h"
#include "sierra-usbwrap.h"
#include "library.h"

#define		QUICKSLEEP	5

int sierra_valid_type (char b) 
{
	unsigned char byte = (unsigned char)b;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_valid_type");

	if ((byte == NUL) ||
	    (byte == ENQ) ||
	    (byte == ACK) ||
	    (byte == DC1) ||
	    (byte == NAK) ||
	    (byte == TRM) ||
	    (byte == TYPE_COMMAND) ||
	    (byte == TYPE_DATA) ||
	    (byte == TYPE_DATA_END))
		return (GP_OK);
	
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* invalid type!");
	return (GP_ERROR_CORRUPTED_DATA);
}

int sierra_valid_packet (Camera *camera, char *packet) {

	int length;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_valid_packet");

	if ((packet[0] == TYPE_COMMAND) ||
	    (packet[0] == TYPE_DATA) ||
	    (packet[0] == TYPE_DATA_END)) {

		/* ??? */
                length = ((unsigned char)packet[2]) +
			 ((unsigned char)packet[3] * 256);
		length += 6;

		return (GP_ERROR);
	}
		
	switch ((unsigned char)packet[0]) {
	case NUL:
	case ENQ:
	case ACK:
	case DC1:
	case NAK:
	case TRM:
		return (GP_OK);
	default:
		return (GP_ERROR);
	}
}

int sierra_write_packet (Camera *camera, char *packet) {

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
	if (fd->type == GP_PORT_USB) {
		if (fd->usb_wrap)
			return (usb_wrap_write_packet (fd->dev, packet, 
						       length));
		else
			return (gp_port_write (fd->dev, packet, length));
	}

	/* SERIAL */
	for (r = 0; r < RETRIES; r++) {

	        ret = gp_port_write (fd->dev, packet, length);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;

		return (ret);
	}

	return (GP_ERROR_IO_TIMEOUT);
}

int sierra_read_packet (Camera *camera, char *packet) {

	int y, r = 0, ret, done, length=0;
	int blocksize, bytes_read;
	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_read_packet");

read_packet_again:
	buf[0] = 0;
	packet[0] = 0;

	if (r > 0)
		gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* reading again...");

	done = 0;
	if (fd->type == GP_PORT_USB && !fd->usb_wrap)
		gp_port_usb_clear_halt(fd->dev, GP_PORT_USB_ENDPOINT_IN);
	
	switch (fd->type) {
	case GP_PORT_USB:
		blocksize = 2054;
		break;
	case GP_PORT_SERIAL:
		blocksize = 1;
		break;
	default:
		return (GP_ERROR_IO_UNKNOWN_PORT);
	}

	for (r = 0; r < RETRIES; r++) {

		if (fd->type == GP_PORT_USB && fd->usb_wrap)
			bytes_read = usb_wrap_read_packet (fd->dev, packet, 
							   blocksize);
		else
			bytes_read = gp_port_read (fd->dev, packet, blocksize);
		
		if (bytes_read == GP_ERROR_IO_TIMEOUT)
			continue;
		if (bytes_read < 0) 
			return (bytes_read);

		ret = sierra_valid_type (packet[0]);
		if (ret == GP_OK)
			done = 1;

		/* Determine the packet type */
		if (!((packet[0] == TYPE_COMMAND) ||
		    (packet[0] == TYPE_DATA) ||
		    (packet[0] == TYPE_DATA_END) ||
		    ((unsigned char)packet[0] == TRM) )) {
		    	if (fd->type == GP_PORT_USB && !fd->usb_wrap)
				gp_port_usb_clear_halt (fd->dev,
						GP_PORT_USB_ENDPOINT_IN);
			return (sierra_valid_packet (camera, packet));
		}

		/* It's a response/command packet */
		if (fd->type == GP_PORT_SERIAL) {
			bytes_read = gp_port_read (fd->dev, 
						   &packet[1], 3);
			if (bytes_read < 0)
				return (bytes_read);

			bytes_read = 4;
		}

		/* Determine the packet length */
       	        length = ((unsigned char)packet[2]) +
			 ((unsigned char)packet[3]  * 256);
		length += 6;

		for (y = bytes_read; y < length; y += blocksize) {

			ret = gp_port_read (fd->dev, &packet[y], blocksize);
			if (ret == GP_ERROR_IO_TIMEOUT) {
				sierra_write_nak (camera);
				goto read_packet_again;
			}
			if (ret < 0)
				return (ret);
		}

		if (done) break;
	}

	if (fd->type == GP_PORT_USB && !fd->usb_wrap)
		gp_port_usb_clear_halt(fd->dev, GP_PORT_USB_ENDPOINT_IN);

	return (GP_ERROR_IO_TIMEOUT);
}

int sierra_build_packet (Camera *camera, char type, char subtype, 
			 int data_length, char *packet) 
{
	SierraData *fd = (SierraData*)camera->camlib_data;

	packet[0] = type;
	switch (type) {
		case TYPE_COMMAND:
			if (fd->type == GP_PORT_USB)
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
	if (fd->type == GP_PORT_USB && !fd->usb_wrap)
		gp_port_usb_clear_halt (fd->dev, GP_PORT_USB_ENDPOINT_IN);
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
	if (fd->type == GP_PORT_USB && !fd->usb_wrap)
		gp_port_usb_clear_halt(fd->dev, GP_PORT_USB_ENDPOINT_IN);
	return (ret);
}

int sierra_ping (Camera *camera) 
{
	int ret, r = 0;
	char buf[4096];
	SierraData *fd = (SierraData *) camera->camlib_data;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_ping");

	if (fd->type == GP_PORT_USB) {
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
			return (ret);

		ret = sierra_read_packet (camera, buf);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return (ret);

		if (buf[0] == NAK)
			return (GP_OK);
		else
			return (GP_ERROR_CORRUPTED_DATA);

	}
	return (GP_ERROR_IO_TIMEOUT);
}

int sierra_set_speed (Camera *camera, int speed) 
{
	gp_port_settings settings;
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_set_speed");
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* speed: %i", speed);

	fd->first_packet = 1;

	gp_port_settings_get (fd->dev, &settings);

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
			return (GP_ERROR_IO_SERIAL_SPEED);
	}

	CHECK (sierra_set_int_register (camera, 17, speed));

	CHECK (gp_port_settings_set (fd->dev, settings));

	GP_SYSTEM_SLEEP(10);
	return (GP_OK);
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
		if (ret != GP_OK)
			return (ret);

		if (buf[0] == ACK)
			return (GP_OK);

		/* DC1 = invalid register or value */
		if (buf[0] == DC1)
			return (GP_ERROR);
	}

	sierra_set_speed (camera, -1);

	return (GP_ERROR);
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
		if (write_nak) {
			CHECK (sierra_write_nak (camera));
		} else {
			CHECK (sierra_write_packet (camera, packet));
		}

		ret = sierra_read_packet (camera, buf);
		if (ret == GP_ERROR)
			return (GP_ERROR);

		/* DC1 = invalid register or value */
		if (buf[0] == DC1)
			return (GP_ERROR_BAD_PARAMETERS);

		if (buf[0] == TYPE_DATA_END) {
			ret = sierra_write_ack(camera);
			r = ((unsigned char)buf[4]) +
			    ((unsigned char)buf[5] * 256) +
			    ((unsigned char)buf[6] * 65536) +
			    ((unsigned char)buf[7] * 16777216);
			*value = r;
			return (ret);
		}

		if (buf[0] == ENQ)
			return (GP_OK);

		write_nak = 1;
	}

	return (GP_ERROR);
}

int sierra_set_string_register (Camera *camera, int reg, char *s, int length) 
{

	char packet[4096], buf[4096];
	char type;
	unsigned char c;
	int x=0, seq=0, size=0, done, r;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", 
			 "* sierra_set_string_register");
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* register: %i", reg);
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* value: %s", s);

	while (x < length) {
		if (x == 0) {
			type = TYPE_COMMAND;
			size = (length + 2 - x) > 2048? 2048 : length + 2;
		}  else {
			size = (length - x) > 2048? 2048 : length;
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
			if (sierra_write_packet (camera, packet) == GP_ERROR)
				return (GP_ERROR);

			if (sierra_read_packet (camera, buf)==GP_ERROR)
				return (GP_ERROR);

			c = (unsigned char)buf[0];
			if (c == DC1)
				return (GP_ERROR);
			if (c == ACK)
				done = 1;
			   else	{
				if (c != NAK)
					return (GP_ERROR);
			}

		}
		if (r > RETRIES) {
			return (GP_ERROR);
		}
	}
	return (GP_OK);
}

int sierra_get_string_register (Camera *camera, int reg, int file_number, 
				 CameraFile *file, char *s, int *length) {

	char packet[4096];
	int done, x, packlength, do_percent, l;
	int ret;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", 
			 "* sierra_get_string_register");
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* register: %i", reg);
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* file number: %i", 
			 file_number);

	do_percent = 1;

	/* Set the current picture number */
	if (file_number >= 0) {

		ret = sierra_set_int_register (camera, 4, file_number);
		if (ret != GP_OK)
			return (ret);
	}

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

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 2, packet));

	packet[4] = 0x04;
	packet[5] = reg;

	/* Send request */
	CHECK (sierra_write_packet (camera, packet));

	/* Read all the data packets */
	x = 0; done = 0;
	while (!done) {
		if (sierra_read_packet (camera, packet) == GP_ERROR)
			return (GP_ERROR);

		if (packet[0] == DC1)
			return (GP_ERROR);

		CHECK (sierra_write_ack (camera));

		packlength = ((unsigned char)packet[2]) +
			     ((unsigned char)packet[3]  * 256);

		if (s) {
			memcpy (&s[x], &packet[4], packlength);
		}

		if (file) {
			gp_file_append (file, &packet[4], packlength);
			if (do_percent)
				gp_frontend_progress (camera, file, 
			   		(float)(100.0*(float)x/(float)(l)));
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
	char packet[4096], buf[4096];

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_delete_all");

	CHECK (sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet));

	packet[4] = 0x02;
	packet[5] = 0x01;
	packet[6] = 0x00;

	CHECK (sierra_write_packet (camera, packet));
	CHECK (sierra_read_packet (camera, buf));

	if (buf[0] == NAK)
		return (GP_ERROR);

	/* Read in the ENQ */
	CHECK (sierra_read_packet (camera, buf));

	GP_SYSTEM_SLEEP (QUICKSLEEP);

	return (GP_OK);
}

int sierra_delete (Camera *camera, int picture_number) 
{
	char packet[4096], buf[4096];
	int r, done;

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
		if (sierra_write_packet (camera, packet) == GP_ERROR)
			return (GP_ERROR);
	
		if (sierra_read_packet (camera, buf) == GP_ERROR)
			return (GP_ERROR);

		done = (buf[0] == NAK)? 0 : 1;

		if (done) {
			/* read in the ENQ */
			if (sierra_read_packet (camera, buf) == GP_ERROR)
				return ((buf[0] == ENQ)? GP_OK : GP_ERROR);
			
		}
	}
	if (r > RETRIES)
		return (GP_ERROR);

	GP_SYSTEM_SLEEP(QUICKSLEEP);

	return (GP_OK);
}

int sierra_end_session (Camera *camera) 
{
	char packet[4096], buf[4096];
	unsigned char c;
	int r, done;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_end_session");

	sierra_build_packet(camera, TYPE_COMMAND, 0, 3, packet);
	packet[4] = 0x02;
	packet[5] = 0x04;
	packet[6] = 0x00;

	r=0; done=0;
	while ((!done) && (r++<RETRIES)) {
		CHECK (sierra_write_packet (camera, packet));
	
		if (sierra_read_packet (camera, buf) == GP_ERROR)
			return (GP_ERROR);

		c = (unsigned char)buf[0];
		if (c == TRM)
			return (GP_OK);

		done = (c == NAK)? 0 : 1;

		if (done) {

			/* read in the ENQ */
			if (sierra_read_packet (camera, buf) == GP_ERROR)
				return (GP_ERROR);
			c = (unsigned char)buf[0];
			return ((c == ENQ)? GP_OK : GP_ERROR);
		}
	}
	if (r > RETRIES)
		return (GP_ERROR);

	return (GP_OK);
}

int sierra_file_count (Camera *camera) {
 
        int value;
	int ret;

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_file_count");
 
 	ret = sierra_get_int_register (camera, 10, &value);
	if (ret != GP_OK) 
		return (ret);
 
        return (value);
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
		if (ret != GP_OK)
			return (ret);

		switch ((unsigned char)buf[0]) {
		case ENQ:
		case TRM:
			return (GP_OK);
		case NAK:
		case ACK:
			continue;
		default:
			return (GP_ERROR_CORRUPTED_DATA);
		}
	}
	
	return (GP_ERROR_IO_TIMEOUT);
}

int sierra_capture_preview (Camera *camera, CameraFile *file)
{
	int ret;
	char packet[4096];

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_capture_preview");

	if (file == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
	
	ret = sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet);
	if (ret != GP_OK)
		return (ret);

	packet[4] = 0x02;
	packet[5] = 0x05;
	packet[6] = 0x00;

	ret = do_capture (camera, packet);
	gp_frontend_progress (camera, file, 0.0);
	if (ret != GP_OK)
		return (ret);

	/* Retrieve the quick preview */
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* get preview...");
	ret = sierra_get_string_register (camera, 14, 0, file, NULL, NULL);
	if (ret != GP_OK)
		return (ret);

	strcpy (file->type, "image/jpeg");

	return (GP_OK);
}

int sierra_capture (Camera *camera, int capture_type, CameraFilePath *filepath)
{
	int ret, picnum, length;
	char packet[4096];
	char buf[128];

	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* sierra_capture");

	if (filepath == NULL) 
		return (GP_ERROR_BAD_PARAMETERS);
	if (capture_type != GP_OPERATION_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	ret = sierra_build_packet (camera, TYPE_COMMAND, 0, 3, packet);
	if (ret != GP_OK)
		return (ret);

	packet[4] = 0x02;
	packet[5] = 0x02;
	packet[6] = 0x00;

	ret = do_capture (camera, packet);
	if (ret != GP_OK)
		return (ret);

	/* After picture is taken, register 4 is set to current picture */
	gp_debug_printf (GP_DEBUG_HIGH, "sierra", "* get picture number...");
	CHECK (sierra_get_int_register (camera, 4, &picnum));

	//FIXME: In which folder???
	strcpy (filepath->folder, "/");

	/* Get the picture filename */
	gp_debug_printf (GP_DEBUG_LOW, "sierra",
			 "*** getting filename of picture %i...", picnum);
	CHECK (sierra_get_string_register (camera, 79, 0, NULL, buf, &length));

	if (length > 0) {
		
		/* Filename supported. Use the camera filename */
		strcpy (filepath->name, buf);
	
	} else {

		/* Filename not supported. */
		//FIXME: Which filename???
		strcpy (filepath->name, "");
	}

	return (GP_OK);
}
