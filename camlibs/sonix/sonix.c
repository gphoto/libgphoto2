/* sonix.c
 *
 * Copyright (C) 2005 Theodore Kilgore <kilgota@auburn.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#include "sonix.h"
#define GP_MODULE "sonix" 


int sonix_init (GPPort *port, CameraPrivateLibrary *priv)
{
	int i, command_done=1;
	unsigned char c[6];
	unsigned char status;
	unsigned char reading[4];

	memset(c,0,sizeof(c));
	c[0] = 0x0c;
	GP_DEBUG("Running sonix_init\n");


	SONIX_READ(port, &status);

	if (status == 0x02) goto skip_a_step; 
	/* If status is 2 we can skip several steps. Otherwise, status
	 * needs to be reset to 0 and massaged until it is 2. 
	*/
	
	if (status) {
		i = 0;
		
		while (status > 0)  {
			SONIX_READ(port, &status);
			i++;
			if (i==1000) break;
		}
	}
		
	command_done = SONIX_COMMAND ( port, c);


	while (status !=2)
		SONIX_READ(port, &status);	
		
		SONIX_READ(port, &status);			

	skip_a_step:
	
	/*
	 * We are supposed to get c[0]=0x8c here; in general the reply
	 * string ought to start with the command byte c[0], plus 0x80
	 */
	SONIX_READ4 (port, reading);	
	SONIX_READ(port, &status);

	memset (c,0,6);
	c[0]=0x16; 

	SONIX_COMMAND ( port, c );

	/*
	 * For the Vivicam 3350b this always gives
	 * 96 0a 76 07. This could be a firmware version. The  
	 * webcam-osx driver says the 0x0a gives the sensor type, which 
	 * is OV7630. For the Sakar Digital Keychain 11199 the string is 
	 * 96 03 31 08, instead. Since the two cameras have different 
	 * abilities, we must distinguish. 
	 */
	 
	SONIX_READ4 (port, reading);

	GP_DEBUG("Above is the 4-byte ID string of your camera.");
	GP_DEBUG("Please report if it is anything other than");
	GP_DEBUG("96 0a 76 07   or   96 03 31 08	Thanks!\n" );		
	priv->fwversion = reading[1];
	SONIX_READ(port, &status);

	/*
	 * This sequence gives the photos in the camera and sets a flag 
	 * in reading[3] if the camera is full. Alas, it does not count 
	 * clip frames separately on the Sakar Digital Keychain camera. 
	 */

	memset (c,0,6);
	c[0]=0x18;

	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );

	SONIX_READ(port, &status);

	SONIX_READ4 (port, reading);		
	if (reading[0] != 0x98)
		return GP_ERROR_CAMERA_ERROR;	
	
	GP_DEBUG ("number of photos is %d\n", reading[1]+ 256*reading[2]);

	/* 
	 * If reading[3] = 0x01, it means the camera is full.  
	 * Not much use here if capture doesn't work, but for some models 
	 * capture does work. So we store this information.  
	 */

	if (!(reading[3])) priv->full = 0;
	
	SONIX_READ(port, &status);

	priv->num_pics = 	(reading[1] + 256*reading[2]);

	memset(c,0,sizeof(c));

	for (i = 0; i < priv->num_pics; i++) {
		GP_DEBUG("getting size_code for picture %i\n", i+1);
		/* Gets the resolution setting, also flags a multi-frame entry. */
		c[0] = 0x19;
		c[1] = (i+1)%256;
		c[2] = (i+1)/256;
	
		SONIX_COMMAND ( port, c );
		SONIX_READ(port, &status);
		SONIX_READ4 (port, reading);		
		if (reading[0] != 0x99)
			return GP_ERROR_CAMERA_ERROR;	
		SONIX_READ(port, &status);
		/* For the code meanings, see get_file_func() in library.c */
		priv->size_code[i] = (reading[1]&0x0f);
	}

	priv->sonix_init_done = 1;
	GP_DEBUG("Leaving sonix_init\n");

        return GP_OK;
}

int 
sonix_read_data_size (Camera *camera, GPPort *port, char *data, int n) 
{
	char status;
	unsigned char c[6];
	unsigned char reading[4];

	GP_DEBUG("running sonix_read_picture_data for picture %i\n", n+1);
	memset (c, 0, 6);
	c[0] = 0x1a;
	c[1] = (n+1)%256;
	c[2] = (n+1)/256;
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);
	SONIX_READ4 (port, reading);	
	if (reading[0] != 0x9a)
		return GP_ERROR_CAMERA_ERROR;	
	return (reading[1] + reading[2]*0x100 + reading[3] *0x10000);
}

int 
sonix_delete_all_pics      (GPPort *port) 
{
	char status;
	unsigned char c[6];
	unsigned char reading[4];

	memset (c,0,6);

	c[0]=0x05; 	
	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);
	SONIX_READ4 (port, reading);	
	if (reading[0] != 0x85)
		return GP_ERROR_CAMERA_ERROR;	

	return GP_OK;
}

int 
sonix_delete_last      (GPPort *port) 
{
	char status;
	unsigned char c[6];
	unsigned char reading[4];
	
	memset (c,0,6);

	c[0]= 0x05; c[1] = 0x01;
	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);
	SONIX_READ4 (port, reading);	
	if (reading[0] != 0x85)
		return GP_ERROR_CAMERA_ERROR;	

	return GP_OK;
}

int 
sonix_capture_image      (GPPort *port) 
{
	char status;
	unsigned char c[6];
	unsigned char reading[4];
	memset (c,0,6);
	c[0]=0x0e; 	
	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);
	SONIX_READ4 (port, reading);	
	if (reading[0] != 0x8e)
		return GP_ERROR_CAMERA_ERROR;	
	return GP_OK;
}

int 
sonix_exit      (GPPort *port) 
{
	char status;
	unsigned char c[6];

	memset (c,0,6);
	c[0]=0x14;
	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );
	/* No READ4 after exit! */
	return GP_OK;
}

/* 
 * The decoding algorithm originates with Bertrik Sikken. This version adapted
 * from the webcam-osx (macam) project. See README for details.
 */

/* Four defines for bitstream operations, for the decode function */

#define PEEK_BITS(num,to) {\
    	if (bitBufCount<num){\
    		do {\
    			bitBuf=(bitBuf<<8)|(*(src++));\
    			bitBufCount+=8; \
		}\
		while(bitBufCount<24);\
	}\
	to=bitBuf>>(bitBufCount-num);\
}

/*
 * PEEK_BITS puts the next <num> bits into the low bits of <to>. 
 * when the buffer is empty, it is completely refilled. 
 * This strategy tries to reduce memory access. Note that the high bits 
 * are NOT set to zero!
 */

#define EAT_BITS(num) { bitBufCount-=num; }

/*
 * EAT_BITS consumes <num> bits (PEEK_BITS does not consume anything, 
 * it just peeks)
 */
  
#define PARSE_PIXEL(val) {\
    PEEK_BITS(10,bits);\
    if ((bits&0x200)==0) { \
	EAT_BITS(1); \
    } \
    else if ((bits&0x380)==0x280) { \
	EAT_BITS(3); \
	val+=3; \
	if (val>255) val=255; \
    }\
    else if ((bits&0x380)==0x300) { \
	EAT_BITS(3); \
	val-=3; \
	if (val<0) val=0; \
    }\
    else if ((bits&0x3c0)==0x200) { \
	EAT_BITS(4); \
	val+=8; \
	if (val>255) val=255;\
    }\
    else if ((bits&0x3c0)==0x240) { \
	EAT_BITS(4); \
	val-=8; \
	if (val<0) val=0;\
    }\
    else if ((bits&0x3c0)==0x3c0) { \
	EAT_BITS(4); \
	val-=20; \
	if (val<0) val=0;\
    }\
    else if ((bits&0x3e0)==0x380) { \
	EAT_BITS(5); \
	val+=20; \
	if (val>255) val=255;\
    }\
    else { \
	EAT_BITS(10); \
	val=8*(bits&0x1f)+0; \
    }\
}


#define PUT_PIXEL_PAIR {\
    long pp;\
    pp=(c1val<<8)+c2val;\
    *((unsigned short *) (dst+dst_index))=pp;\
    dst_index+=2; }

/* Now the decode function itself */


int 
sonix_decode(unsigned char * dst, unsigned char * src, int width, int height) 
{
	long dst_index = 0;
	int starting_row = 0;
	unsigned short bits;
	short c1val, c2val; 
	int x, y;
	unsigned long bitBuf = 0;
	unsigned long bitBufCount = 0;

	src += 8;
	
	for (y = starting_row; y < height; y++) {
		PEEK_BITS(8, bits);
		EAT_BITS(8);
		c1val = bits & 0xff;

		PEEK_BITS(8, bits);
		EAT_BITS(8);
		c2val = bits & 0xff;

		PUT_PIXEL_PAIR;

		for (x = 2; x < width ; x += 2) {
    			PARSE_PIXEL(c1val);
    			PARSE_PIXEL(c2val);
    			PUT_PIXEL_PAIR;
		}
	}
	return GP_OK;
}

/* Three often-used generic commands */


/* This reads a one-byte "status" response */
int 
SONIX_READ (GPPort *port, char *data) 
{
	gp_port_usb_msg_interface_read(port, 0, 1, 0, data, 1);
	return GP_OK;
}

/* This reads a 4-byte response to a command */
int 
SONIX_READ4 (GPPort *port, char *data) 
{
	gp_port_usb_msg_interface_read(port, 0, 4, 0, data, 4);
	return GP_OK;
}

/* A command to the camera is a 6-byte string, and this sends it. */
int 
SONIX_COMMAND (GPPort *port, char *command) 
{
	gp_port_usb_msg_interface_write(port, 8, 2, 0, command ,6);	
	return GP_OK;
}

