/*
 * sq905.c
 *
 * Copyright (c) 2003 Theodore Kilgore <kilgota@auburn.edu>
 * Camera library support under libgphoto2.1.1 for camera(s) 
 * with chipset from Service & Quality Technologies, Taiwan. 
 * The chip supported by this driver is presumed to be the SQ905,  
 *
 * Licensed under GNU Lesser General Public License, as part of Gphoto
 * camera support project. For a copy of the license, see the file 
 * COPYING in the main source tree of libgphoto2.
 */    

#include <config.h>


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#ifdef OS2
#include <db.h>
#endif

#include <gphoto2.h>
#include <gphoto2-port.h>
#include "sq905.h"

#define GP_MODULE "sq905" 

#define SQ_PING "\x00"
#define SQ_GET  "\xc1"

#define SQWRITE gp_port_usb_msg_write
#define SQREAD  gp_port_usb_msg_read

int
sq_init (GPPort *port, SQModel *m, SQData *data)
{
        unsigned char setup_data[0x400][0x10];  
        unsigned char c[4]; 
	unsigned char pic_data[0x400];
	int i = 0;
 
	while (i <= 1) {

		SQWRITE (port, 0x0c, 0x06, 0xf0, SQ_PING, 1);	
		SQREAD (port, 0x0c, 0x07, 0x00, c, 1);     
		SQREAD (port, 0x0c, 0x07, 0x00, c, 4);     
		SQWRITE (port, 0x0c, 0x06, 0xa0, c, 1);	
		SQREAD (port, 0x0c, 0x07, 0x00, c, 1);    

		/*  Perhaps the above sequence should be coded as a 
		 * separate function because it gets used in capture.
		 */
		SQWRITE (port, 0x0c, 0x06, 0xf0, SQ_PING, 1);	
		SQREAD (port, 0x0c, 0x07, 0x00, c, 1);     
		sq_read_data (port, c, 4);

		/*
		 * If all is well, we receive here "09 05 00 26"  
		 * Translates to "905 &" 
		 * 
		 * Paulo Tribolet Abreu <paulotex@gmx.net> reports that
		 * his camera returns 09 05 01 19.
		 */
		sq_reset (port);
		SQWRITE (port, 0x0c, 0x06, 0x20, SQ_PING, 1);	
		SQREAD (port, 0x0c, 0x07, 0x00, c, 1);
		if (m) {
			if (!memcmp (c, "\x09\x05\x00\x26", 4))
				*m = SQ_MODEL_ARGUS;
			else if (!memcmp (c, "\x09\x05\x01\x19", 4))
				*m = SQ_MODEL_POCK_CAM;
			else
				*m = SQ_MODEL_UNKNOWN;
		}

		sq_read_data (port, *setup_data, 0x4000);
		sq_reset (port);
		SQWRITE (port, 0x0c, 0xc0, 0x00, SQ_PING, 1); 
		SQWRITE (port, 0x0c, 0x06, 0x30, SQ_PING, 1);	
		SQREAD (port, 0x0c, 0x07, 0x00, c, 1);

		/* We throw the setup data away the first time around, 
		 * because it is very likely corrupted. We run through 
		 * this whole sequence twice before using the data.
		 */     

		if (i==0) memset (setup_data, 0 , sizeof (setup_data));
		i++;
	}
	
	for (i = 0; i < 0x400; i++) pic_data[i] = setup_data[i][0]; 

	memcpy (data, pic_data, 0x400);

    	return GP_OK; 
}


/* The first appearance of a zero in pic_data gives the number of 
 * pictures taken. 
 */
int
sq_get_num_pics (SQData *data)
{
	unsigned int i = 0;

	for (i = 0; i < 0x400; i++) if (!data[i]) return i;

	return 0;
}

int
sq_get_comp_ratio (SQData *data, int n)
{
    	switch (data[n]) {
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x76: return 2;
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x56: return 1;
	default:
		GP_DEBUG ("Your camera has unknown resolution settings.\n");
		return (GP_ERROR_NOT_SUPPORTED);
	}
}

int
sq_get_picture_width (SQData *data, int n)
{
    	switch (data[n]) {  
	case 0x41:
	case 0x61: return 352;
	case 0x42:
	case 0x62: return 176;
	case 0x43:
	case 0x63: return 320;
	case 0x56:
	case 0x76: return 640;
	default:
		GP_DEBUG ("Your pictures have unknown width.\n");
		return (GP_ERROR_NOT_SUPPORTED);
	}
}

int
sq_reset (GPPort *port)
{
    	unsigned char c;       

    	SQWRITE (port, 0x0c, 0xc0, 0x00, SQ_PING, 1);  
	SQWRITE (port, 0x0c, 0x06, 0xa0, SQ_PING, 1);  
	SQREAD (port, 0x0c, 0x07, 0x00, &c, 1);

    	return GP_OK;
}

unsigned char *
sq_read_data (GPPort *port, char *data, int size)
{
    	unsigned char msg[size];

    	memset (msg, 0, sizeof (msg));
    	SQWRITE (port, 0x0c, 0x03, size, SQ_GET, 1); 
    	gp_port_read (port, data, size); 
    	return GP_OK;
}

unsigned char *
sq_read_picture_data (GPPort *port, char *data, int size)
{

	int remainder = size % 0x8000;
	int offset = 0;

	while ((offset + 0x8000 < size)) {
		sq_read_data (port, data + offset, 0x8000);
		offset = offset + 0x8000;
	}
 	sq_read_data (port, data + offset, remainder);

    	return GP_OK;
} 
