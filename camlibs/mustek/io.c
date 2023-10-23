/*
 * Copyright 1999/2000 by Henning Zabel <henning@uni-paderborn.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * gphoto driver for the Mustek MDC800 Digital Camera. The driver
 * supports rs232 and USB. It automatically detects which Kernelnode
 * is used.
 */

/*
 * Implements cummunication-core :
 *
 * Sending commands and receiving data
 */

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>


#include "io.h"
#include <string.h>
#include <sys/time.h>
#include "print.h"
#include "rs232.h"
#include "usb.h"
#include "mdc800_spec.h"

/*
 * Send a Command to the Camera. It is unimportant whether this
 * is a USB or a RS232 Command. The Function implements an automatic
 * retry of a failed command.
 *
 * The Function will only be used in this Layer, execpt on probing
 * the Baudrate of Rs232.
 * if quiet is true, the function won't print notes
 */
int mdc800_io_sendCommand_with_retry (GPPort *port,unsigned char* command, unsigned char* buffer, int length, int maxtries,int quiet)
{
	int try=0;
	int ret=GP_OK;

	while (try < maxtries)
	{
	        usleep(MDC800_DEFAULT_COMMAND_RETRY_DELAY*1000);
		if (port->type == GP_PORT_USB)
			ret=mdc800_usb_sendCommand(port,command, buffer, length );
		else
			ret=mdc800_rs232_sendCommand(port, command, buffer, length);
		if (ret==GP_OK)
			return GP_OK;
		try++;
	}
	if (!quiet)
	{
		printCoreNote ("\nCamera is not responding (Maybe off?)\n");
		printCoreNote ("giving it up after %i times.\n\n", try);
	}
	return GP_ERROR_IO;
}


/*
 * sends a command and receives the answer to this
 * buffer: Stores the answer
 * length: length of answer or null
 */
int mdc800_io_sendCommand (GPPort*port,unsigned char commandid,unsigned char par1,unsigned char par2,unsigned char par3, unsigned char* buffer, int length)
{
	unsigned char command [8];
	command[0]=COMMAND_BEGIN;
	command[1]=commandid;
	command[2]=par1;
	command[3]=par2;
	command[4]=par3;
	command[5]=COMMAND_END;
	command[6]=0;
	command[7]=0;
	return mdc800_io_sendCommand_with_retry (port,command,buffer,length, MDC800_DEFAULT_COMMAND_RETRY,0);
}

/*
 * Helper function for rs232 and usb
 * It returns a timeout for the specified command
 */
int mdc800_io_getCommandTimeout (unsigned char command)
{
	switch (command)
	{
		case COMMAND_SET_STORAGE_SOURCE:
		case COMMAND_SET_TARGET:
		case COMMAND_SET_CAMERA_MODE:
		case COMMAND_DELETE_IMAGE:
			return MDC800_LONG_TIMEOUT;

		case COMMAND_TAKE_PICTURE:
		case COMMAND_PLAYBACK_IMAGE:
		case COMMAND_SET_PLAYBACK_MODE:
			return MDC800_TAKE_PICTURE_TIMEOUT;
	}
	return MDC800_DEFAULT_TIMEOUT;
}
