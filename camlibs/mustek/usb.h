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
	Header for USB Interface
*/

#ifndef CAMLIBS_MUSTEK_USB_H
#define CAMLIBS_MUSTEK_USB_H

#define MDC800_USB_ENDPOINT_COMMAND  0x01
#define MDC800_USB_ENDPOINT_STATUS   0x82
#define MDC800_USB_ENDPOINT_DOWNLOAD 0x84
#define MDC800_USB_IRQ_INTERVAL		 255    /* 255ms */

int mdc800_usb_sendCommand (GPPort*,unsigned char* , unsigned char * , int );

#endif /* !defined(CAMLIBS_MUSTEK_USB_H) */
