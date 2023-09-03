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
 * supports rs232 and USB.
 */

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>

#include "print.h"
#include "rs232.h"
#include "mdc800_spec.h"
#include "io.h"

#include <fcntl.h>
#include <sys/time.h>


/*
 * sends a command and receives the answer to this
 * buffer: Stores the answer
 * length: length of answer or null
 */
int mdc800_rs232_sendCommand(GPPort *port,unsigned char* command, unsigned char* buffer, int length)
{
	char answer;
	int fault=0,ret;
	int i;

	printFnkCall ("(mdc800_rs232_sendCommand) id:%i (%i,%i,%i),answer:%i\n",command[1],command[2],command[3],command[4],length);

	usleep(MDC800_DEFAULT_COMMAND_DELAY*1000);
	gp_port_set_timeout (port, MDC800_DEFAULT_TIMEOUT );

	/* Send command */
	for (i=0; i<6; i++)
	{
		if (gp_port_write (port, (char*)&command[i] ,1) < GP_OK)
		{
			printCError ("(mdc800_rs232_sendCommand) sending Byte %i fails!\n",i);
			fault=1;
		}

		ret = gp_port_read (port,&answer,1);
		if ( ret != 1)
		{
			printCError ("(mdc800_rs232_sendCommand) receiving resend of Byte %i fails.\n",i);
			fault=1;
		}
		if (command [i] != answer)
		{
			printCError ("(mdc800_rs232_sendCommand) Byte %i differs : send %i, received %i \n",i,command[i],answer);
			fault=1;
		}
	}
	if (fault)
		return GP_ERROR_IO;

	/* Receive answer */
	if (length)
	{
		/* Some Commands needs a download. */
		switch (command[1])
		{
			case COMMAND_GET_IMAGE:
			case COMMAND_GET_THUMBNAIL:
				if (!mdc800_rs232_download(port,buffer,length))
				{
					printCError ("(mdc800_rs232_sendCommand) download of %i Bytes fails.\n",length);
					fault=1;
				}
				break;
			default:
				if (!mdc800_rs232_receive(port,buffer,length))
				{
					printCError ("(mdc800_rs232_sendCommand) receiving %i Bytes fails.\n",length);
					fault=1;
				}
		}
	}

	if (fault)
		return GP_ERROR_IO;

	/* commit */
	if (!(command[1] == COMMAND_CHANGE_RS232_BAUD_RATE)) {
		if (!mdc800_rs232_waitForCommit(port,command[1]))
		{
			printCError ("(mdc800_rs232_sendCommand) receiving commit fails.\n");
			fault=1;
		}
	}
	if (fault)
		return GP_ERROR_IO;
	return GP_OK;
}


/*
 * waits for the Commit value
 * The function expects the commandid of the corresponding command
 * to determine whether a long timeout is needed or not. (Take Photo)
 */
int mdc800_rs232_waitForCommit (GPPort *port,char commandid)
{
	unsigned char ch[1];
	int ret;

	gp_port_set_timeout(port,mdc800_io_getCommandTimeout(commandid));
	ret = gp_port_read(port,(char *)ch,1);
	if (ret!=1)
	{
		printCError ("(mdc800_rs232_waitForCommit) Error receiving commit !\n");
		return GP_ERROR_IO;
	}

	if (ch[0] != ANSWER_COMMIT )
	{
		printCError ("(mdc800_rs232_waitForCommit) Byte \"%i\" was not the commit !\n",ch[0]);
		return GP_ERROR_IO;
	}
	return GP_OK;
}

/*
 * receive Bytes from camera
 */
int mdc800_rs232_receive (GPPort *port,unsigned char* buffer, int b)
{
	int ret;
	gp_port_set_timeout (port,MDC800_DEFAULT_TIMEOUT );

	ret=gp_port_read(port,(char*)buffer,b);
	if (ret!=b)
	{
		printCError ("(mdc800_rs232_receive) can't read %i Bytes !\n",b);
		return GP_ERROR_IO;
	}
	return GP_OK;
}


/*
 * downloads data from camera and send
 * a checksum every 512 bytes.
 */
int mdc800_rs232_download (GPPort *port,unsigned char* buffer, int size)
{
	int readen=0,i,j;
	unsigned char checksum;
	unsigned char DSC_checksum;
	int numtries=0;

	gp_port_set_timeout(port, MDC800_DEFAULT_TIMEOUT );

	while (readen < size)
	{
		if (!mdc800_rs232_receive (port,&buffer[readen],512))
			return readen;
		checksum=0;
		for (i=0; i<512; i++)
			checksum=(checksum+buffer[readen+i])%256;
		if (gp_port_write (port,(char*) &checksum,1) < GP_OK)
			return readen;

		if (!mdc800_rs232_receive (port,&DSC_checksum,1))
			return readen;


		if (checksum != DSC_checksum)
		{
			numtries++;
			printCError ("(mdc800_rs232_download) checksum: software %i, DSC %i , reload block! (%i) \n",checksum,DSC_checksum,numtries);
			if (numtries > 10)
			{
				printCError ("(mdc800_rs232_download) to many retries, giving up..");
				return 0;
			}
		}
		else
		{
			readen+=512;
			numtries=0;
		}
	}


	for (i=0; i<4; i++)
	{
		printCError ("%i: ",i);
		for (j=0; j<8; j++)
			printCError (" %i", buffer[i*8+j]);
		printCError ("\n");
	}
	return readen;
}
