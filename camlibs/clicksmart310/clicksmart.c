/* clicksmart.c
 *
 * part of libgphoto2/camlibs/clicksmart310
 *
 * Copyright (C) 2006 Theodore Kilgore <kilgota@auburn.edu>
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

#include "clicksmart.h"
#include "spca50x-jpeg-header.h"

#define GP_MODULE "clicksmart310" 

#define CS_INIT 	0x8000
#define CS_INIT2 	0xd41
#define CS_NUM_PICS 	0xd40
#define CS_CH_READY 	0xd00
#define CS_CH_CLEAR	0xd05
#define CS_READCLOSE	0x8303

static int 
CLICKSMART_READ (GPPort *port, int index, char *data) 
{
	gp_port_usb_msg_interface_read(port, 0, 0, index, data, 1);
	return GP_OK;
}

static int 
CLICKSMART_READ_STATUS (GPPort *port, char *data) 
{		
	gp_port_usb_msg_interface_read(port, 0, 0, CS_CH_READY, data, 1);
	return GP_OK;
}

int clicksmart_init (GPPort *port, CameraPrivateLibrary *priv)
{
	int i, cat_size;
	int full_reads;
	int short_read;
	unsigned char *temp_catalog;
	unsigned char *buffer;
	char c = 0;

	GP_DEBUG("Running clicksmart_init\n");

	CLICKSMART_READ(port, CS_INIT, &c);
	/* We are supposed to get a response of 0x44 ???*/
	CLICKSMART_READ(port, CS_INIT2, &c);
	CLICKSMART_READ(port, CS_NUM_PICS, &c);

	priv->num_pics =  (unsigned)c; 
	full_reads = (unsigned)c/2;
	short_read = (unsigned)c%2;
	cat_size = (unsigned)c*0x10;
	temp_catalog = malloc(cat_size);
	if (!temp_catalog) 
		return GP_ERROR_NO_MEMORY;
	memset (temp_catalog, 0, cat_size);

	/* now we need to get and put away the catalog data. */
	
	CLICKSMART_READ_STATUS (port, &c);
	gp_port_usb_msg_interface_write(port, 6, 0, 9, NULL, 0);
	while (c != 1)
		CLICKSMART_READ_STATUS (port, &c);
	buffer = malloc(0x200);
	if (!buffer) {
		free (temp_catalog);
		return GP_ERROR_NO_MEMORY;
	}
	/*
	 * The catalog data is downloaded in reverse order, which needs to be 
	 * straightened out.
	 */    
	for (i=0; i < full_reads; i++) {
		memset (buffer, 0, 0x200);
		gp_port_read(port, (char *)buffer, 0x200);
		memcpy (temp_catalog + cat_size - (2*i+1)*0x10, buffer, 0x10);
		memcpy (temp_catalog + cat_size - (2*i+2)*0x10, buffer+0x100, 
									0x10);
	}
	if (short_read) {
		memset (buffer, 0, 0x200);		
		gp_port_read(port, (char *)buffer, 0x100);
		memcpy (temp_catalog, buffer, 0x10);
	}
	priv->catalog = temp_catalog;

	clicksmart_reset(port);
	free (buffer);
	GP_DEBUG("Leaving clicksmart_init\n");

	return GP_OK;
}


int clicksmart_get_res_setting (CameraPrivateLibrary *priv, int n)
{
	GP_DEBUG("running clicksmart_get_res_setting for picture %i\n", n+1);
	/*
	 * QCIF (176x144) returns 1, or if the entry is a clip frame, then 3 
	 * and CIF (352x288) returns 0. 
	 */
	return priv->catalog[16*n];
}

int 
clicksmart_read_pic_data (CameraPrivateLibrary *priv, GPPort *port, 
					unsigned char *data, int n) 
{
	int offset=0;
	char c;
	unsigned int size = 0;
	unsigned int remainder = 0;

	GP_DEBUG("running clicksmart_read_picture_data for picture %i\n", n+1);

	CLICKSMART_READ_STATUS (port, &c);

	GP_DEBUG("ClickSmart Read Status at beginning %d\n", c);

	gp_port_usb_msg_interface_write(port, 6, 0x1fff - n, 1, NULL, 0);
	c = 0;
	while (c != 1){
	  CLICKSMART_READ_STATUS (port, &c);
	}
	/* Get the size of the data and calculate the size to download, which
	 * is the next multiple of 0x100. Only for the hi-res photos is the 
	 * true data size actually given.  
	 */

	size=(priv->catalog[16*n + 12] * 0x100)+(priv->catalog[16*n + 11]);
	if (size == 0)	/* for lo-res photos the above calculation gives 0 */
	  size = (priv->catalog[16*n + 5] * 0x100);

	remainder = size%0x200;

	GP_DEBUG("size:  %x, remainder: %x\n", size, remainder);
	/* Download the data */
	while (offset < size-remainder) {
	  GP_DEBUG("offset: %x\n", offset);
	  gp_port_read(port, (char *)data + offset, 0x200);
	  offset = offset + 0x200;
	}

	remainder=((remainder+0xff)/0x100)*0x100;
	GP_DEBUG("Second remainder: %x\n", remainder);
	if (remainder) 
		gp_port_read(port, (char *)data + offset, remainder);

	gp_port_usb_msg_interface_read(port, 0, 0, CS_READCLOSE, &c, 1);
	gp_port_usb_msg_interface_write(port, 0, 2, CS_CH_READY, NULL, 0);

	/* 
	 * For lo-res photos the camera does not tell us the true size, but 
	 * when the data has ended the excess bytes downloaded are all
	 * 00. The photo processing works better if these are suppressed.
	 */
	
	if (priv->catalog[16*n]) {
		while ( data[size-1] == 0)
			size--;
	}
	return size;
}

int 
clicksmart_delete_all_pics      (GPPort *port) 
{
	gp_port_usb_msg_interface_write(port, 0, 2, CS_CH_READY, NULL, 0);
	gp_port_usb_msg_interface_write(port, 2, 0, 5, NULL, 0);
	return GP_OK;
}

int
clicksmart_reset (GPPort *port)
{
	char c;
	/* Release current register */
	CLICKSMART_READ (port, CS_READCLOSE, &c); 
	gp_port_usb_msg_interface_write(port, 0, 2, CS_CH_READY, NULL, 0);
	CLICKSMART_READ (port, CS_CH_CLEAR, &c); 	
	CLICKSMART_READ (port, CS_CH_CLEAR, &c); 	
    	return GP_OK;
}

/* create_jpeg_from_data adapted from camlibs/spca50x */

int create_jpeg_from_data (unsigned char * dst, unsigned char * src, 
			int qIndex, int w, int h, unsigned char format, 
			int o_size, int *size,
		        int omit_huffman_table, int omit_escape)
{
	int i = 0;
	unsigned char *start;
	unsigned char value;
	start = dst;

	/* copy the header from the template */
	memcpy (dst, SPCA50xJPGDefaultHeaderPart1,
			SPCA50X_JPG_DEFAULT_HEADER_PART1_LENGTH);

	/* modify quantization table */
	memcpy (dst + 7, SPCA50xQTable[qIndex * 2], 64);
	memcpy (dst + 72, SPCA50xQTable[qIndex * 2 + 1], 64);
	dst += SPCA50X_JPG_DEFAULT_HEADER_PART1_LENGTH;

	/* copy Huffman table */
	if (!omit_huffman_table) {
	    memcpy (dst, SPCA50xJPGDefaultHeaderPart2,
			    SPCA50X_JPG_DEFAULT_HEADER_PART2_LENGTH);
	    dst += SPCA50X_JPG_DEFAULT_HEADER_PART2_LENGTH;
	}
	memcpy (dst, SPCA50xJPGDefaultHeaderPart3,
			SPCA50X_JPG_DEFAULT_HEADER_PART3_LENGTH);

	/* modify the image width, height */
	*(dst + 8) = w & 0xFF;		/* Image width low byte */
	*(dst + 7) = w >> 8 & 0xFF;	/* Image width high byte */
	*(dst + 6) = h & 0xFF;		/* Image height low byte */
	*(dst + 5) = h >> 8 & 0xFF;	/* Image height high byte */

	/* set the format */
	*(dst + 11) = format;

	/* point to real JPG compress data start position and copy */
	dst += SPCA50X_JPG_DEFAULT_HEADER_PART3_LENGTH;

	for (i = 0; i < o_size; i++) {
		value = *(src + i) & 0xFF;
		*(dst) = value;
		dst++;

		if (value == 0xFF && !omit_escape) {
			*(dst) = 0x00;
			dst++;
		}
	}

	/* Add end of image marker */
	*(dst++) = 0xFF;
	*(dst++) = 0xD9;

	*size = dst - start;
	return GP_OK;

}
