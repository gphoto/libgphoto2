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

#include <gphoto2.h>
#include <gphoto2-port.h>

#include "sonix.h"

#define GP_MODULE "sonix" 


int sonix_init (GPPort *port, CameraPrivateLibrary *priv)
{
	int i;
	unsigned char c[6];
	unsigned char status;
	unsigned char reading[4];

	memset(c,0,sizeof(c));
	c[0] = 0x0c;
	GP_DEBUG("Running sonix_init\n");

	SONIX_READ(port, &status);

	if (status) {
		i = 0;
		
		while (status > 0)  {
			SONIX_READ(port, &status);
			i++;
			if (i==1000) break;
		}
	}
		
	SONIX_COMMAND ( port, c);


	while (status !=2)
		SONIX_READ(port, &status);	
		
		SONIX_READ(port, &status);			

	/* We are supposed to get c[0]=0x8c here */
	SONIX_READ4 (port, reading);	
	SONIX_READ(port, &status);

	memset (c,0,6);
	c[0]=0x16; 

	SONIX_COMMAND ( port, c );

	/*
	 * This should always cause a read of the same 4 bytes, namely
	 * 96 0a 76 07 so it could be that this is a read of the amount 
	 * of memory in the camera. but if so, in what units?
	 * Webcam-osx driver says the 0x0a gives the sensor type, which 
	 * here is OV7630.
	 */
	 
	SONIX_READ4 (port, reading);		
	SONIX_READ(port, &status);

	/* This sequence gives the photos in the camera and sets a flag 
	 * in reading[3] if the camera is full. 
	 */

	memset (c,0,6);
	c[0]=0x18;

	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );

	SONIX_READ(port, &status);

	SONIX_READ4 (port, reading);		
	
	GP_DEBUG ("number of photos is %d\n", reading[1]+ 256*reading[2]);

	/* 
	 * If reading[3] = 0x01, it means the camera is full.  
	 * Important in webcam mode, or in case that capture is to be run. 
	 * Not much use here if we can't make the capture to work :(
	 * but if capture can be made to work it is needed here, too. 
	 */

	if (!(reading[3])) priv->full = 0;
	
	SONIX_READ(port, &status);

	priv->num_pics = 	(reading[1] + 256*reading[2]);



	GP_DEBUG("Leaving sonix_init\n");

        return GP_OK;
}


int sonix_get_picture_width (GPPort *port, int n)
{
	char status;
	unsigned char c[6];
	unsigned char reading[4];
	
	memset(c,0,sizeof(c));

	GP_DEBUG("running sonix_get_picture_width for picture %i\n", n+1);
	/* Gets the resolution setting, just prior to download of a photo. */
	c[0] = 0x19;
	c[1] = n+1;
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);
	SONIX_READ4 (port, reading);		
	SONIX_READ(port, &status);
	
	/* Only the likely stillcam modes are listed here; others may exist. */ 

	switch (reading[1]&0x0f) {
	case 0:
		return 352; /* CIF */
		break;
	case 1:
		return 176; /* QCIF */
		break;

	case 2: 
		return 640; /* VGA */
		break;
	case 3:
		return 320; /* SIF */
		break;
	default:
		GP_DEBUG("Width of photo %i is unknown\n", n+1);
		return (GP_ERROR_NOT_SUPPORTED);
	}
}


int 
sonix_read_picture_data (Camera *camera, GPPort *port, 
					char *data, int n) 
{
	char status;
	unsigned char c[6];
	unsigned char reading[4];
	unsigned int size;
	unsigned int remainder = 0;

	GP_DEBUG("running sonix_read_picture_data for picture %i\n", n+1);
	memset (c, 0, 6);
	c[0] = 0x1a;
	c[1] = (n+1)%256;
	c[2] = (n+1)/256;
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);

	SONIX_READ4 (port, reading);	

	if (reading[1]%0x40) remainder = 0x40;

	size = 0x40*(reading[1]/0x40) + reading[2]*0x100 
			+ reading[3] * 0x10000+ remainder;
	gp_port_read(port, data, size);
		
	return size;
}

int 
sonix_delete_all_pics      (GPPort *port) 
{
	char status;
	unsigned char c[6];

	memset (c,0,6);
	c[0]=0x05;

	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);
	return GP_OK;
}

/* 
 * The decoding algorithm originates with Bertrik Sikken. This version adapted
 * from the one written for the webcam-osx project, for the sn9c2028.
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
    if ((bits&0x00000200)==0) { \
	EAT_BITS(1); \
    } \
    else if ((bits&0x00000380)==0x00000280) { \
	EAT_BITS(3); \
	val+=3; \
	if (val>255) val=255; \
    }\
    else if ((bits&0x00000380)==0x00000300) { \
	EAT_BITS(3); \
	val-=3; \
	if (val<0) val=0; \
    }\
    else if ((bits&0x000003c0)==0x00000200) { \
	EAT_BITS(4); \
	val+=8; \
	if (val>255) val=255;\
    }\
    else if ((bits&0x000003c0)==0x00000240) { \
	EAT_BITS(4); \
	val-=8; \
	if (val<0) val=0;\
    }\
    else if ((bits&0x000003c0)==0x000003c0) { \
	EAT_BITS(4); \
	val-=20; \
	if (val<0) val=0;\
    }\
    else if ((bits&0x000003e0)==0x00000380) { \
	EAT_BITS(5); \
	val+=20; \
	if (val>255) val=255;\
    }\
    else { \
	EAT_BITS(10); \
	val=8*(bits&0x0000001f)+0; \
    }\
}


#define PUT_PIXEL_PAIR {\
    long pp;\
    pp=(c1val<<8)+c2val;\
    *((unsigned short *) (dst+dst_index))=pp;\
    dst_index+=2; }

/* Now the decode function itself */


int sonix_decode(unsigned char * dst, unsigned char * src, int width, int height) 
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
		c1val = bits & 0x000000ff;

		PEEK_BITS(8, bits);
		EAT_BITS(8);
		c2val = bits & 0x000000ff;

		PUT_PIXEL_PAIR;

		for (x = 2; x < width ; x += 2) {
    			PARSE_PIXEL(c1val);
    			PARSE_PIXEL(c2val);
    			PUT_PIXEL_PAIR;
		}
	}
	return GP_OK;
}

int 
SONIX_READ (GPPort *port, char *data) 
{
	gp_port_usb_msg_interface_read(port, 0, 1, 0, data, 1);
	return GP_OK;
}

int 
SONIX_READ4 (GPPort *port, char *data) 
{
	gp_port_usb_msg_interface_read(port, 0, 4, 0, data, 4);
	return GP_OK;
}

int 
SONIX_COMMAND (GPPort *port, char *command) 
{
	gp_port_usb_msg_interface_write(port, 8, 2, 0, command ,6);	
	return GP_OK;
}

