/*
 * Copyright (C) 1999/2000 by Henning Zabel <henning@uni-paderborn.de>
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

/*
	Implemenation of the USB Version of ExecuteCommand
*/
#include <string.h>

#include <gphoto2-library.h>
#include <gphoto2-result.h>

#include "print.h"
#include "usb.h"
#include "io.h"
#include "mdc800_spec.h"

#include <fcntl.h>
#include <sys/time.h>


/*
 * Checks wether the camera responds busy
 */
static int mdc800_usb_isBusy (char* ch)
{
	int i;
	for (i=0;i<8;i++)
		if (ch [i] != (char)0x99)
			return 0;
	return 1;
}


/*
 * Checks wether the Camera is ready
 */
static int mdc800_usb_isReady (char *ch)
{
	int i;
	for (i=0;i<8;i++)
		if (ch [i] != (char)0xbb)
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
static int mdc800_usb_readFromIrq (GPPort *port,int type,char* data,int timeout)
{
	int ret;
	struct timeval tv;

	gp_port_set_timeout(port,1);
	timeout+=10*MDC800_USB_IRQ_INTERVAL;
	gettimeofday(&tv,NULL);
	while (timeout>=0)
	{
		/* try a read */
		ret = gp_port_check_int(port,data,8);
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
				printf ("%i ", (unsigned char)data [i]);
			}
			printf ("\n");
		}
		/* check data */
		if (type)
		{
			if (!(mdc800_usb_isReady(data)||mdc800_usb_isBusy(data))) {
			    fprintf(stderr,"got data.\n");
				/* Data received successfull */
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
		if (0) {
		    struct timeval tv1,result;

		    gettimeofday(&tv1,NULL);
		    timersub(&tv1,&tv,&result);
		    if ((result.tv_sec > 0) || (result.tv_usec>=timeout*1000)) {
			fprintf(stderr,"time out\n");
			break;
		    }
		}
		if (1) {
		    struct timeval t;
		    t.tv_usec = MDC800_USB_IRQ_INTERVAL*1000;
		    t.tv_sec  = 0;
		    select (1 , NULL, NULL, NULL, &t);
		    timeout-=MDC800_USB_IRQ_INTERVAL;
		}
	}
	/* Timeout */
	printCError ("(mdc800_usb_readFromIrq) timeout\n");
	return GP_ERROR_IO;
}

int mdc800_usb_sendCommand(GPPort*port,char*command,char*buffer,int length)
{
	char tmp_buffer [16];
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
	
	if ((ret=gp_port_write(port,command,8)) != 8)
	{
		printCError ("(mdc800_usb_sendCommand) sending Command fails (%d)!\n",ret);
		return ret;
	}

	/* receive the answer */
	switch ((unsigned char) command [1])
	{
	case COMMAND_GET_THUMBNAIL:
	case COMMAND_GET_IMAGE:
		gp_port_set_timeout (port, 2000);
		if (gp_port_read (port,buffer,64) != 64)
		{
			printCError ("(mdc800_usb_sendCommand) requesting 64Byte dummy data fails.\n");
			return GP_ERROR_IO;
		}
		fprintf(stderr,"got 64 byte\n");
		{ /* Download loop */
			int readen=0;
			while (readen < length)
			{
				if (gp_port_read(port,buffer+readen,64) != 64)
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
			int ret;
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
