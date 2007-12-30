#ifndef _IO_H
#define _IO_H

/* Maximum Number of tries, before a command is given up */
#define MDC800_DEFAULT_COMMAND_RETRY			4

/* There is a little delay for the next sending (ms) */
#define MDC800_DEFAULT_COMMAND_RETRY_DELAY 		300

/* Default Timeout */
#define MDC800_DEFAULT_TIMEOUT		 		250

/* Prevent Overruns ( ms) ?? */
#define MDC800_DEFAULT_COMMAND_DELAY 			50

/* Long Timeout for Functions that needs time (Take Photo, delete..) */
#define MDC800_LONG_TIMEOUT			 	5000

/* 20sec Timeout for Flashlight */
#define MDC800_TAKE_PICTURE_TIMEOUT  			20000


int mdc800_io_sendCommand_with_retry(GPPort *, unsigned char*, unsigned char* , int, int,int);

/* The API to the upper Layer */
int mdc800_io_sendCommand(GPPort *, unsigned char ,unsigned char,unsigned char,unsigned char,unsigned char *,int );

/* Helper Function for rs232 and usb */
int mdc800_io_getCommandTimeout(unsigned char);
#endif
