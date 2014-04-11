/* sonix.c
 *
 * Copyright (C) 2005 Theodore Kilgore <kilgota@auburn.edu>
 *
 * A previous version of the white_balance() function intended for use in
 * libgphoto2/camlibs/aox is copyright (c) 2008 Amauri Magagna.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
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
#include <gamma.h>


#include "sonix.h"
#define GP_MODULE "sonix" 


/* Three often-used generic commands */

/* This reads a one-byte "status" response */
static int
SONIX_READ (GPPort *port, char *data) 
{
	gp_port_usb_msg_interface_read(port, 0, 1, 0, data, 1);
	return GP_OK;
}

/* This reads a 4-byte response to a command */
static int
SONIX_READ4 (GPPort *port, char *data) 
{
	gp_port_usb_msg_interface_read(port, 0, 4, 0, data, 4);
	return GP_OK;
}

/* A command to the camera is a 6-byte string, and this sends it. */
static int
SONIX_COMMAND (GPPort *port, char *command) 
{
	gp_port_usb_msg_interface_write(port, 8, 2, 0, command ,6);	
	return GP_OK;
}


int sonix_init (GPPort *port, CameraPrivateLibrary *priv)
{
	int i;
	char c[6];
	char status;
	unsigned char reading[4];

	memset(c,0,sizeof(c));
	c[0] = 0x0c;
	GP_DEBUG("Running sonix_init\n");


	SONIX_READ(port, &status);

	if (status == 0x02) goto skip_a_step; 
	/* If status is 2 we can skip several steps. Otherwise, status
	 * needs to be reset to 0 and massaged until it is 2. 
	*/
	
	if ((unsigned)status) {
		i = 0;
		
		while ((unsigned)status > 0)  {
			SONIX_READ(port, &status);
			i++;
			if (i==1000) break;
		}
	}
		
	SONIX_COMMAND ( port, c);


	while (status !=2)
		SONIX_READ(port, &status);	
		
		SONIX_READ(port, &status);			

	skip_a_step:
	
	/*
	 * We are supposed to get c[0]=0x8c here; in general the reply
	 * string ought to start with the command byte c[0], plus 0x80
	 */
	memset(reading, 0, 4);
	SONIX_READ4 (port, (char *)reading);
	SONIX_READ(port, &status);

	memset (c, 0, 6);

	while (!reading[1]&&!reading[2]&&!reading[3]) {
		c[0]=0x16; 
		SONIX_COMMAND ( port, c );
		/*
		 * For the Vivicam 3350b this always gives
		 * 96 0a 76 07. This is apparently the firmware version. 
		 * The webcam-osx comments that the 0x0a gives the sensor 
		 * type, which is OV7630. For the Sakar Digital Keychain 
		 * 11199 the string is 96 03 31 08, instead. For the 
		 * Mini-Shotz ms350 it is 96 08 26 09. For the Genius 
		 * Smart 300 it is 96 00 67 09 and for the Digital
		 * Spy Camera 70137 it is 96 01 31 09.  Since the cameras
		 * have different abilities, we ought to distinguish. 
		 */
		SONIX_READ4 (port, (char *)reading);
	}
	GP_DEBUG("%02x %02x %02x %02x\n", reading[0], reading[1],
		 reading[2], reading[3]);
	GP_DEBUG("Above is the 4-byte ID string of your camera. ");
	GP_DEBUG("Please report if it is anything other than\n");
	GP_DEBUG("96 0a 76 07  or  96 03 31 08  or  96 08 26 09\n");
	GP_DEBUG("or  96 00 67 09  or  96 01 31 09\n");
	GP_DEBUG("Thanks!\n" );
	for(i=0;i<4;i++)
		priv->fwversion[i] = reading[i];
	GP_DEBUG("fwversion[1] is %#02x\n", reading[1]);
	SONIX_READ(port, &status);
	/* case 0x03: uses the default settings, above. */
	switch(priv->fwversion[1]) {
	case 0x08:
		priv->offset=0;
		priv->avi_offset=0;
		priv->can_do_capture=1;
		priv->post = DECOMP;
		break;
	case 0x0a:
		priv->offset=8;
		priv->avi_offset=8;
		priv->can_do_capture=0;
		priv->post = DECOMP|REVERSE;
		break;
	case 0x00:
		priv->offset=0;
		priv->avi_offset=0;
		priv->can_do_capture=0;
		priv->post=0;
		break;
	case 0x01:
		priv->offset=8;
		priv->avi_offset=8;
		priv->can_do_capture=0;
		priv->post=0;
		break;
	case 0x03:
	default:
		priv->offset=8;
		priv->avi_offset=8;
		priv->can_do_capture=1;
		priv->post=0;
	}

	/*
	 * This sequence gives the photos in the camera and sets a flag 
	 * in reading[3] if the camera is full. Alas, clip frames are 
	 * not counted; the AVI constructor will need to count them 
	 */
	memset (c,0,6);
	c[0]=0x18;
	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);
	SONIX_READ4 (port, (char *)reading);		
	if (reading[0] != 0x98)
		return GP_ERROR_CAMERA_ERROR;	
	GP_DEBUG ("number of photos is %d\n", reading[1]+ 256*reading[2]);
	/* 
	 * If reading[3] = 0x01, it means the camera is full of data. 
	 * The capture_image() function must then be disabled.
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
		SONIX_READ4 (port, (char *)reading);
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
sonix_read_data_size (GPPort *port, int n) 
{
	char status;
	unsigned char c[6];
	unsigned char reading[4];
	GP_DEBUG("running sonix_read_data_size for picture %i\n", n+1);
	memset (c, 0, 6);
	c[0] = 0x1a;
	c[1] = (n+1)%256;
	c[2] = (n+1)/256;
	SONIX_COMMAND ( port, (char *)c );
	SONIX_READ(port, &status);
	SONIX_READ4 (port, (char *)reading);
	if (reading[0] != 0x9a)
		return GP_ERROR_CAMERA_ERROR;
	return (reading[1] + reading[2]*0x100 + reading[3] *0x10000);
}

int 
sonix_delete_all_pics      (GPPort *port) 
{
	char status;
	char c[6];
	unsigned char reading[4];
	memset (c,0,6);
	c[0]=0x05; 
	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);
	SONIX_READ4 (port, (char *)reading);
	if (reading[0] != 0x85)
		return GP_ERROR_CAMERA_ERROR;
	return GP_OK;
}

int 
sonix_delete_last      (GPPort *port) 
{
	char status;
	char c[6];
	unsigned char reading[4];
	memset (c,0,6);
	c[0]= 0x05; c[1] = 0x01;
	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);
	SONIX_READ4 (port, (char *)reading);
	if (reading[0] != 0x85)
		return GP_ERROR_CAMERA_ERROR;
	return GP_OK;
	}

int 
sonix_capture_image      (GPPort *port)
{
	char status;
	char c[6];
	unsigned char reading[4];
	GP_DEBUG("Running sonix_capture_image\n");
	memset (c,0,6);
	c[0]=0x0e; 
	SONIX_READ(port, &status);
	SONIX_COMMAND ( port, c );
	SONIX_READ(port, &status);
	SONIX_READ4 (port, (char *)reading);
	if (reading[0] != 0x8e)
		return GP_ERROR_CAMERA_ERROR;
	return GP_OK;
}

int 
sonix_exit      (GPPort *port) 
{
	char status;
	char c[6];

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
	/* Columns were reversed during compression ! */
	for (y = starting_row; y < height; y++) {
		PEEK_BITS(8, bits);
		EAT_BITS(8);
		c2val = bits & 0xff;
		PEEK_BITS(8, bits);
		EAT_BITS(8);
		c1val = bits & 0xff;
		PUT_PIXEL_PAIR;
		for (x = 2; x < width ; x += 2) {
 			PARSE_PIXEL(c2val);
			PARSE_PIXEL(c1val);
			PUT_PIXEL_PAIR;
		}
	}
	return GP_OK;
}

int sonix_byte_reverse (unsigned char *imagedata, int datasize) 
{
	int i;
	unsigned char temp;
	for (i=0; i< datasize/2 ; ++i) {
		temp = imagedata[i];
		imagedata[i] = imagedata[datasize - 1 - i];
		imagedata[datasize - 1 - i] = temp;
	}
	return GP_OK;
}

int sonix_rows_reverse (unsigned char *imagedata, int width, int height) 
{
	int col, row;
	unsigned char temp;
	for (col = 0; col < width; col++)
		for (row=0; row< height /2 ; row++) {
			temp = imagedata[row*width+col];
			imagedata[row*width+col] = 
			imagedata[(height-row-1)*width +col];
			imagedata[(height-row-1)*width + col] = temp; 
	}
	return GP_OK;
}

int sonix_cols_reverse (unsigned char *imagedata, int width, int height) 
{
	int col, row;
	unsigned char temp;
	for (row=0; row < height ; row++) {
		for (col =0; col< width/2 ; col++) {
			temp = imagedata[row*width + col];
			imagedata[row*width + col] = 
			    imagedata[row*width + width - 1 - col];
			imagedata[row*width + width - 1 - col] = temp;
		}
	}
	return GP_OK;
}

/*
 *	=== White Balance / Color Enhance / Gamma adjust ===
 *
 *	Get histogram for each color plane
 *	Expand to reach 0.5% of white dots in image
 *
 *	Get new histogram for each color plane
 *	Expand to reach 0.5% of black dots in image
 *
 *	Get new histogram
 *	Calculate and apply gamma correction
 *
 *	if not a dark image:
 *	For each dot, increases color separation
 */

static int
histogram (unsigned char *data, unsigned int size, int *htable_r, 
						int *htable_g, int *htable_b)
{
	int x;
	/* Initializations */
	for (x = 0; x < 256; x++) { 
		htable_r[x] = 0; 
		htable_g[x] = 0; 
		htable_b[x] = 0; 
	}
	/* Building the histograms */
	for (x = 0; x < (size * 3); x += 3)
	{
		htable_r[data[x+0]]++;	/* red histogram */
		htable_g[data[x+1]]++;	/* green histogram */
		htable_b[data[x+2]]++;	/* blue histogram */
	}
	return 0;
}


int
white_balance (unsigned char *data, unsigned int size, float saturation)
{
	int x, r, g, b, max, d;
	double r_factor, g_factor, b_factor, max_factor, MAX_FACTOR=1.6;
	int htable_r[256], htable_g[256], htable_b[256];
	unsigned char gtable[256];
	double new_gamma, gamma;

	/* ------------------- GAMMA CORRECTION ------------------- */

	histogram(data, size, htable_r, htable_g, htable_b);
	x = 1;
	for (r = 64; r < 192; r++)
	{
		x += htable_r[r]; 
		x += htable_g[r];
		x += htable_b[r];
	}
        gamma = sqrt((double) (x ) / (double) (size * 2));
        GP_DEBUG("Provisional gamma correction = %1.2f\n", gamma);

	if(gamma < .1) {
		new_gamma = .50;
		MAX_FACTOR=1.2;
	}
	else if (gamma < 0.60) 
		new_gamma = 0.60;
	else
		new_gamma = gamma;
        if (new_gamma > 1.2) new_gamma = 1.2;
        GP_DEBUG("Gamma correction = %1.2f\n", new_gamma);
	gp_gamma_fill_table(gtable, new_gamma);
	gp_gamma_correct_single(gtable,data,size);

	/* ---------------- BRIGHT DOTS ------------------- */
	max = size / 200; 
	histogram(data, size, htable_r, htable_g, htable_b);

	for (r=254, x=0; (r > 64) && (x < max); r--)  
		x += htable_r[r]; 
	for (g=254, x=0; (g > 64) && (x < max); g--) 
		x += htable_g[g];
	for (b=254, x=0; (b > 64) && (x < max); b--) 
		x += htable_b[b];

	r_factor = (double) 254 / r;
	g_factor = (double) 254 / g;
	b_factor = (double) 254 / b;
	max_factor = r_factor;
	if (g_factor > max_factor) max_factor = g_factor;
	if (b_factor > max_factor) max_factor = b_factor;

	if (max_factor > MAX_FACTOR) {

		r_factor = (r_factor / max_factor) * MAX_FACTOR;
		g_factor = (g_factor / max_factor) * MAX_FACTOR;
		b_factor = (b_factor / max_factor) * MAX_FACTOR;
	}

	GP_DEBUG("White balance (bright): r=%1d, g=%1d, b=%1d, fr=%1.3f, fg=%1.3f, fb=%1.3f\n", r, g, b, r_factor, g_factor, b_factor);

	for (x = 0; x < (size * 3); x += 3)
	{
		d = (int) data[x+0] * r_factor;
		if (d > 255) { d = 255; }
		data[x+0] = d;
		d = (int) data[x+1] * g_factor;
		if (d > 255) { d = 255; }
		data[x+1] = d;
		d = (int) data[x+2] * b_factor;
		if (d > 255) { d = 255; }
		data[x+2] = d;
	}
	/* ---------------- DARK DOTS ------------------- */


	max = size / 200;  /*  1/200 = 0.5%  */

	histogram(data, size, htable_r, htable_g, htable_b);

	for (r=0, x=0; (r < 64) && (x < max); r++)  
		x += htable_r[r]; 
	for (g=0, x=0; (g < 64) && (x < max); g++) 
		x += htable_g[g];
	for (b=0, x=0; (b < 64) && (x < max); b++) 
		x += htable_b[b];

	r_factor = (double) 254 / (255-r);
	g_factor = (double) 254 / (255-g);
	b_factor = (double) 254 / (255-b);

	GP_DEBUG("White balance (dark): r=%1d, g=%1d, b=%1d, fr=%1.3f, fg=%1.3f, fb=%1.3f\n", r, g, b, r_factor, g_factor, b_factor);

	for (x = 0; x < (size * 3); x += 3)
	{
		d = (int) 255-((255-data[x+0]) * r_factor);
		if (d < 0) { d = 0; }
		data[x+0] = d;
		d = (int) 255-((255-data[x+1]) * g_factor);
		if (d < 0) { d = 0; }
		data[x+1] = d;
		d = (int) 255-((255-data[x+2]) * b_factor);
		if (d < 0) { d = 0; }
		data[x+2] = d;
	}

	/* ------------------ COLOR ENHANCE ------------------ */


	for (x = 0; x < (size * 3); x += 3)
	{
		r = data[x+0]; g = data[x+1]; b = data[x+2];
		d = (int) (r + 2*g + b) / 4.;
		if ( r > d )
			r = r + (int) ((r - d) * (255-r)/(256-d) * saturation);
		else 
			r = r + (int) ((r - d) * (255-d)/(256-r) * saturation);
		if (g > d)
			g = g + (int) ((g - d) * (255-g)/(256-d) * saturation);
		else 
			g = g + (int) ((g - d) * (255-d)/(256-g) * saturation);
		if (b > d)
			b = b + (int) ((b - d) * (255-b)/(256-d) * saturation);
		else 
			b = b + (int) ((b - d) * (255-d)/(256-b) * saturation);

		if (r < 0) { r = 0; }
		if (r > 255) { r = 255; }
		data[x+0] = r;
		if (g < 0) { g = 0; }
		if (g > 255) { g = 255; }
		data[x+1] = g;
		if (b < 0) { b = 0; }
		if (b > 255) { b = 255; }
		data[x+2] = b;
	}

	
	return 0;
}
