/****************************************************************/
/* lowlevel.c - Implementation of the protocol defining the way */
/*		how to send and receive bytes to and from the 	*/
/*		camera.						*/
/*								*/
/* Copyright (C) 2000 The Free Software Foundation		*/
/*								*/
/* Author: Lutz Müller <Lutz.Mueller@stud.uni-karlsruhe.de>	*/
/*								*/
/* This program is free software; you can redistribute it 	*/
/* and/or modify it under the terms of the GNU General Public	*/ 
/* License as published by the Free Software Foundation; either */
/* version 2 of the License, or (at your option) any later 	*/
/* version.							*/
/*								*/
/* This program is distributed in the hope that it will be 	*/
/* useful, but WITHOUT ANY WARRANTY; without even the implied 	*/
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 	*/
/* PURPOSE.  See the GNU General Public License for more 	*/
/* details.							*/
/*								*/
/* You should have received a copy of the GNU General Public 	*/
/* License along with this program; if not, write to the Free 	*/
/* Software Foundation, Inc., 59 Temple Place, Suite 330, 	*/
/* Boston, MA 02111-1307, USA.					*/
/****************************************************************/


/****************************************************************/
/* Type Definitions						*/
/****************************************************************/
typedef enum {
	L_SUCCESS,
	L_TRANSMISSION_ERROR,
	L_IO_ERROR
} l_return_status_t;


/****************************************************************/
/* Prototypes							*/
/****************************************************************/
l_return_status_t l_init (
	CameraPortSettings port_settings, 
	gboolean debug_flag);


l_return_status_t l_exit (void);


/****************************************************************/
/* Some comments on the structure of send_buffer, image_buffer,	*/
/* and receive_buffer:						*/
/* 								*/
/* send_buffer:	The first two bytes are low order and high 	*/
/* 		byte of the command, followed by at least two	*/
/*		more bytes. The number of sent bytes has to be	*/
/*		even.						*/
/*								*/
/* image_buffer: 	This buffer will contain raw data like	*/
/*			jpeg or exif data.			*/
/*								*/
/* receive_buffer:	The first two bytes will be the same as	*/
/*			in send_buffer. This will be checked 	*/
/*			here. Then will follow low and high	*/
/*			order byte of the return status. After 	*/
/*			that, depending on command and return	*/
/*			status, other bytes will follow.	*/
/****************************************************************/
l_return_status_t l_send_receive (
	guchar  *send_buffer,
	guint    send_buffer_size,
        guchar **receive_buffer, 
        guint   *receive_buffer_size);


l_return_status_t l_send_receive_receive (
	guchar  *send_buffer,
	guint    send_buffer_size,
        guchar **image_buffer, 
        guint   *image_buffer_size, 
        guchar **receive_buffer, 
        guint   *receive_buffer_size, 
	guint    timeout);

