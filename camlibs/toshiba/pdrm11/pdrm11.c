/* pdrm11.c -- interfaces directly with the camera
 *
 * Copyright 2003 David Hogue <david@jawa.gotdns.org>
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

#include "config.h"
#include "pdrm11.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <gphoto2/gphoto2.h>
#include "gphoto2-endian.h"

#define	GP_MODULE	"pdrm11"
#define	ETIMEDOUT	110

static int pdrm11_select_file(GPPort *port, uint16_t file);

int pdrm11_init(GPPort *port)
{
	unsigned char buf[20];
	int timeout = 50;
	
	gp_port_set_timeout(port,1000);

	/* exactly what windows driver does */
	gp_port_usb_msg_read (port, 0x01, PDRM11_CMD_READY, 0, (char *)buf, 4);
	gp_port_usb_msg_write(port, 0x01, PDRM11_CMD_PING3, 0, NULL, 0);
	gp_port_usb_msg_read (port, 0x01, PDRM11_CMD_READY, 0, (char *)buf, 4);
	gp_port_usb_msg_write(port, 0x01, PDRM11_CMD_INIT1, 0, NULL, 0);
	gp_port_usb_msg_read (port, 0x01, PDRM11_CMD_READY, 0, (char *)buf, 4);
	gp_port_usb_msg_write(port, 0x01, PDRM11_CMD_INIT2, 0, NULL, 0);
	gp_port_usb_msg_read (port, 0x01, PDRM11_CMD_READY, 0, (char *)buf, 4);

	gp_port_usb_msg_read (port, 0x01, PDRM11_CMD_ZERO, 0, (char *)buf, 2);
	if(buf[0] || buf[1]) {
		/* I haven't seen anything other than 00 00 yet
		 * unless the connection is bad */
		GP_DEBUG("PDRM11_CMD_ZERO: %x %x", buf[0], buf[1]);
		return(GP_ERROR);
	}
		

	/* wait til the camera is ready */
	do {
		usleep(200000);
		GP_DEBUG("waiting...");

		timeout--;
		if( gp_port_usb_msg_read(port, 0x01, PDRM11_CMD_READY, 0, (char *)buf, 4) == -ETIMEDOUT )
			timeout = 0;
	} while( !((buf[3] == 0x25) && (buf[0] == 1)) && timeout );
	
	/* what good is this? */
	usleep(400000);

	if(!timeout)
		return(GP_ERROR_TIMEOUT);
	else
		return(GP_OK);
}



int pdrm11_get_filenames(GPPort *port, CameraList *list)
{
	int i, j;
	uint32_t numPics;
	char name[20];
	char buf[30];


	gp_port_set_timeout(port, 10000);
	CHECK(gp_port_usb_msg_read(port, 0x01, PDRM11_CMD_GET_NUMPICS, 0, buf, 10));
	/* trying to remain endian friendly */
	numPics = le16atoh(&buf[2]) + (le16atoh(&buf[0]) * 1024);
	GP_DEBUG("found %d pictures", numPics);

	

	for(i=1;  i<numPics+1;  i++) {
		CHECK( pdrm11_select_file(port, i) );

		CHECK(gp_port_usb_msg_read(port, 0x01, 0xe600, i, buf, 14));

		/* the filename is 12 chars starting at the third byte */
		CHECK(gp_port_usb_msg_read(port, 0x01, PDRM11_CMD_GET_FILENAME, i, buf, 26));
		for(j=0; j<12; j+=2) {
			name[j] = buf[j+2+1];
			name[j+1] = buf[j+2];
		}
		name[12] = '\0';

		GP_DEBUG("%s",name);
		gp_list_append(list, name, NULL);
	}


	return(GP_OK);
}

static int pdrm11_select_file(GPPort *port, uint16_t file)
{
	char buf[10];

	uint16_t picNum = htole16(file);
	uint16_t file_type;
	
	/* byte 4 of PDRM11_CMD_GET_INFO determines if the file is a jpeg or tiff */
	CHECK(gp_port_usb_msg_read(port, 0x01, PDRM11_CMD_GET_INFO, file, buf, 8));
	file_type = htole16(buf[4]);

	CHECK( gp_port_usb_msg_write(port, 0x01, PDRM11_CMD_SELECT_PIC1, file, (char*)&picNum, 2) );
	CHECK( gp_port_usb_msg_write(port, 0x01, PDRM11_CMD_SELECT_PIC2, file, (char*)&file_type, 2) );

	return(GP_OK);
}

#if 0
static int pdrm11_ping(GPPort *port)
{
	CHECK( gp_port_usb_msg_write(port, 0x01, PDRM11_CMD_PING1, 1, NULL, 0) );
	CHECK( gp_port_usb_msg_write(port, 0x01, PDRM11_CMD_PING2, 1, NULL, 0) );

	return(GP_OK);
}
#endif


int pdrm11_get_file(CameraFilesystem *fs, const char *filename, CameraFileType type, 
			CameraFile *file, GPPort *port, uint16_t picNum)
{
	uint32_t size = 0;
	uint16_t thumbsize = 0;
	uint8_t buf[30];
	uint8_t *image;
	uint8_t temp;
	int i;
	int ret;
	int file_type;

		
	gp_port_set_timeout(port,10000);
	CHECK( pdrm11_select_file(port, picNum) );

	if(type == GP_FILE_TYPE_PREVIEW) {
		CHECK(gp_port_usb_msg_read(port, 0x01, PDRM11_CMD_GET_INFO, picNum, (char *)buf, 8));
		file_type = buf[4];

		CHECK( gp_port_usb_msg_read(port, 0x01, PDRM11_CMD_GET_THUMBSIZE, picNum, (char *)buf, 14) );
		thumbsize = le16atoh( &buf[8] );
		
		/* add 1 to file size only for jpeg thumbnails */
		if(file_type == 1) { 
			GP_DEBUG("thumbnail file_type: %s.", "jpeg");
			size = (uint32_t)thumbsize + 1;
		} else if(file_type == 2) {
			/* NOTE: tiff thumbnails are 160x120 pixel 8bpc rgb images, NOT jpegs... */
			GP_DEBUG("thumbnail file_type: %s.", "tiff");
			size = (uint32_t)thumbsize;
		} else {
			GP_DEBUG("Unknown thumbnail file format!");
			return(GP_ERROR_NOT_SUPPORTED);
		}

	} else if(type == GP_FILE_TYPE_NORMAL) {
		CHECK( gp_port_usb_msg_read(port, 0x01, PDRM11_CMD_GET_FILESIZE, picNum, (char *)buf, 26) );
		size = le32atoh( &buf[18] );
	} else {
		GP_DEBUG("Unsupported file type!");
		return(GP_ERROR_NOT_SUPPORTED);
	}

	GP_DEBUG("size: %d 0x%x", size, size);

	image = malloc(sizeof(char)*size);
	if(!image)
		return(GP_ERROR_NO_MEMORY);



	if(type == GP_FILE_TYPE_PREVIEW) {
		CHECK_AND_FREE( gp_port_usb_msg_write(port, 0x01, PDRM11_CMD_GET_THUMB, picNum, NULL, 0), image );
	} else {
		CHECK_AND_FREE( gp_port_usb_msg_write(port, 0x01, PDRM11_CMD_GET_PIC, picNum, NULL, 0), image );
	}

	ret = gp_port_read(port, (char *)image, size);
	if(ret != size) {
		GP_DEBUG("failed to read from port.  Giving it one more try...");
		ret = gp_port_read(port, (char *)image, size);
		if(ret != size) {
			GP_DEBUG("gp_port_read returned %d 0x%x.  size: %d 0x%x", ret, ret, size, size);
			free (image);
			return(GP_ERROR_IO_READ);
		}
	}

	/* swap the bytes for the thumbnail, but not the file */
	if(type == GP_FILE_TYPE_PREVIEW) {
		for(i=0; i<size;  i+=2) {
			temp = image[i];
			image[i] = image[i+1];
			image[i+1] = temp;
		}
	}
	

	gp_file_set_mime_type(file, GP_MIME_JPEG);
	gp_file_set_data_and_size(file, (char *)image, size);

	return(GP_OK);
}



int pdrm11_delete_file(GPPort *port, int picNum)
{
	uint8_t buf[2];

	/* for some reason the windows driver sends b200 twice
	 * once in pdrm11_select_file and once before. dunno why */
	CHECK( gp_port_usb_msg_write(port, 0x01, PDRM11_CMD_SELECT_PIC1, picNum, (char*)&picNum, 2) );
	CHECK( pdrm11_select_file(port, picNum) );

	/* should always be 00 00 */
	gp_port_usb_msg_read(port, 0x01, PDRM11_CMD_DELETE, picNum, (char *)buf, 2);
	if( (buf[0] != 0) || (buf[1] !=0) ) {
		GP_DEBUG("should have read 00 00.  actually read %2x %2x.", buf[0], buf[1]);
		return(GP_ERROR);
	}


	return(GP_OK);
}

