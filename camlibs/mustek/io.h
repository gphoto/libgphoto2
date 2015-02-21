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
