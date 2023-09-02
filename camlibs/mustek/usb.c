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

#define _DEFAULT_SOURCE

/*
	Implemenation of the USB Version of ExecuteCommand
*/
#include <string.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>

#include "print.h"
#include "usb.h"
#include "io.h"
#include "mdc800_spec.h"

#include <fcntl.h>
#include <sys/time.h>


/*
 * Checks whether the camera responds busy
 */
static int mdc800_usb_isBusy (unsigned char* ch)
{
	int i;
	for (i=0;i<8;i++)
		if (ch [i] != 0x99)
			return 0;
	return 1;
}


/*
 * Checks whether the Camera is ready
 */
static int mdc800_usb_isReady (unsigned char *ch)
{
	int i;
	for (i=0;i<8;i++)
		if (ch [i] != 0xbb)
			return 0;
	return 1;
}


/*
 * Waits for the camera
 * type: 0:  Wait until the camera gets ready
 *       1:  Read data from irq
 * The function stores the readen 8 Bytes in data.
 * and return 0 on success else -1.
 */
static int mdc800_usb_readFromIrq (GPPort *port,int type,unsigned char* data,int timeout)
{
	int ret;
	struct timeval tv;

	gp_port_set_timeout(port,1);
	timeout+=10*MDC800_USB_IRQ_INTERVAL;
	gettimeofday(&tv,NULL);
	while (timeout>=0)
	{
		/* try a read */
		ret = gp_port_check_int(port,(char*)data,8);
		if (ret!=8)
		{
			printCError ("(mdc800_usb_readFromIRQ) reading bytes from irq fails (%d)\n",ret);
			return ret;
		}
		if (0) {
			int i=0;
			printf ("irq :");
			for (i=0; i<8; i++)
			{
				printf ("%i ", data [i]);
			}
			printf ("\n");
		}
		/* check data */
		if (type)
		{
			if (!(mdc800_usb_isReady(data)||mdc800_usb_isBusy(data))) {
			    fprintf(stderr,"got data.\n");
				/* Data received successfully */
				return GP_OK;
			}
		} else {
			if (mdc800_usb_isReady (data))
			{
			    fprintf(stderr,"got readiness.\n");
				/* Camera response ready */
				return GP_OK;
			}
		}
		/* wait the specified time */
		if (1) {

		    usleep(MDC800_USB_IRQ_INTERVAL * 1000);

		    timeout-=MDC800_USB_IRQ_INTERVAL;
		}
	}
	/* Timeout */
	printCError ("(mdc800_usb_readFromIrq) timeout\n");
	return GP_ERROR_IO;
}

int mdc800_usb_sendCommand(GPPort*port,unsigned char*command,unsigned char*buffer,int length)
{
	unsigned char tmp_buffer [16];
	GPPortSettings settings;
	int ret;

	printf ("(mdc800_usb_sendCommand) id:%i (%i,%i,%i,%i,%i,%i),answer:%i\n",command[1],command[2],command[3],command[4],command[5],command[6],command[7],length);
	/* Send the Command */
	gp_port_set_timeout(port,MDC800_DEFAULT_TIMEOUT);

	gp_port_get_settings(port,&settings);
	settings.usb.outep = MDC800_USB_ENDPOINT_COMMAND;
	gp_port_set_settings(port,settings);

	ret = mdc800_usb_readFromIrq(port,0,tmp_buffer,MDC800_DEFAULT_TIMEOUT);
	if (ret != GP_OK) {
	    fprintf(stderr,"Camera did not get ready before mdc800_usb_sendCommand!\n");
	}

	if ((ret=gp_port_write(port,(char*)command,8)) != 8)
	{
		printCError ("(mdc800_usb_sendCommand) sending Command fails (%d)!\n",ret);
		return ret;
	}

	/* receive the answer */
	switch (command [1])
	{
	case COMMAND_GET_THUMBNAIL:
	case COMMAND_GET_IMAGE:
		gp_port_set_timeout (port, 2000);
		if (gp_port_read (port,(char*)buffer,64) != 64)
		{
			printCError ("(mdc800_usb_sendCommand) requesting 64Byte dummy data fails.\n");
			return GP_ERROR_IO;
		}
		fprintf(stderr,"got 64 byte\n");
		{ /* Download loop */
			int readen=0;
			while (readen < length)
			{
				if (gp_port_read(port,(char*)(buffer+readen),64) != 64)
				{
					printCError ("(mdc800_usb_sendCommand) reading image data fails.\n");
					return 0;
				}
				fprintf(stderr,"got 64 byte\n");
				readen+=64;
			}
		}
		break;
	default :
		if (length > 0)
		{
			ret = mdc800_usb_readFromIrq (port,1,tmp_buffer, mdc800_io_getCommandTimeout(command[1]));
			if (ret != GP_OK)
			{
				/* Reading fails */
				printCError ("(mdc800_usb_sendCommand) receiving answer fails (%d).\n",ret);
				return ret;
			}
			memcpy(buffer,tmp_buffer,length);
		}
	}
#if 1
	/* Waiting for camera to get ready */
	ret = mdc800_usb_readFromIrq(port,0,tmp_buffer,mdc800_io_getCommandTimeout (command[1]));
	if (ret!=GP_OK)
	{
		/* Reading fails */
		printCError ("(mdc800_usb_sendCommand) camera didn't get ready in the defined intervall ?!\n");
		return ret;
	}
#endif
	return GP_OK;
}
