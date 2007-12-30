/*
 	Header for USB Interface
*/
#ifndef _USB_H
#define _USB_H

#define MDC800_USB_ENDPOINT_COMMAND  0x01
#define MDC800_USB_ENDPOINT_STATUS   0x82
#define MDC800_USB_ENDPOINT_DOWNLOAD 0x84
#define MDC800_USB_IRQ_INTERVAL		 255    /* 255ms */

int mdc800_usb_sendCommand (GPPort*,unsigned char* , unsigned char * , int );

#endif
