/*
 * STV0674 Vision Camera Chipset Driver
 * Copyright (C) 2000 Adam Harrison <adam@antispin.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2.h>
#include <gphoto2-port.h>

#include "stv0674.h"
#include "library.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif


int stv0674_ping(GPPort *port)
{
    int ret;
    unsigned char reply[4];

    ret = gp_port_usb_msg_read (port, CMDID_PING, 0, 0, reply, 1);
    if (ret < GP_OK)
	return ret;

    if(reply[0] != 0)
    {
	printf("CMDID_PING successful, but returned bad values?\n");
	return GP_ERROR_IO;
    }

    return GP_OK;

}

int stv0674_file_count(GPPort *port, int *count)
{
    int ret;
    unsigned char reply[4];

    ret = gp_port_usb_msg_read (port, CMDID_ENUMERATE_IMAGES, 0, 0, reply, 4);
    if (ret < GP_OK)
	return ret;


    *count= ((reply[3]) | (reply[2]<<8) | (reply[1]<<16) | (reply[0]<<24));

    return GP_OK;
}

/**
 * Get image
 */
int stv0674_get_image_raw(GPPort *port, int image_no, CameraFile *file)
{
    return GP_OK;
}

static int setval(unsigned char* where,unsigned long val)
{
    where[3]=(val & 0xff);
    where[2]=((val >> 8) & 0xff);
    where[1]=((val >> 16) & 0xff);
    where[0]=((val >> 24) & 0xff);
    return 0;
}


int stv0674_get_image(GPPort *port, int image_no, CameraFile *file)
{
    unsigned char header[0x200];/*block for header */
    int x,y,size;

    int whole,remain;

    int current;

    int ret;
    unsigned char imagno[8];
    unsigned char reply[4];
    unsigned char * data;

    memset(imagno,0,8);

    image_no+=2;

    setval(imagno,image_no);


    ret = gp_port_usb_msg_write (port, CMDID_SET_IMAGE, 0, 0, imagno, 4);
    if (ret < GP_OK)
	return ret;

    ret = gp_port_usb_msg_read (port, CMDID_IHAVENOIDEA, 0, 0, reply, 2);
    if (ret < GP_OK)
	return ret;

    setval(&imagno[4],0x200);/* we want 512 bytes */

    ret = gp_port_usb_msg_write (port,
				 CMDID_READ_IMAGE,
				 READ_IMAGE_VALUE_RESET,
				 0,
				 imagno,
				 8);
    if (ret < GP_OK)
	return ret;

    gp_port_read(port, header, 0x200);

    size=(header[0x47]<<8) | header[0x48];

    x=(header[0x49]<<8) | header[0x4a];
    y=(header[0x4b]<<8) | header[0x4c];

    /*create data block */
    data=malloc(size);
    if (!data)
	return GP_ERROR_NO_MEMORY;

    setval(&imagno[4],0x1000);/* we want 4096 bytes */

    whole=size / 0x1000;
    remain=size % 0x1000;


    for(current=0;current<whole;current+=1)
    {


	ret = gp_port_usb_msg_write (port,
				     CMDID_READ_IMAGE,
				     READ_IMAGE_VALUE_READ,
				     0,
				     imagno,
				     8);
	if (ret < GP_OK)
	    return ret;

	gp_port_read(port, &data[current*0x1000], 0x1000);
    }

    if(remain)
    {
	setval(&imagno[4],remain);/* we want remaining bytes */
	ret = gp_port_usb_msg_write (port,
				     CMDID_READ_IMAGE,
				     READ_IMAGE_VALUE_READ,
				     0,
				     imagno,
				     8);
	if (ret < GP_OK)
	    return ret;

	gp_port_read(port, &data[current*0x1000], remain);

    }


    gp_file_append(file, data, size);
    free(data);

    ret = gp_port_usb_msg_write (port, CMDID_FINISH_READ, 0, 0, imagno, 4);
    if (ret < GP_OK)
	return ret;

    return GP_OK;
}

int stv0674_get_image_preview(GPPort *port, int image_no, CameraFile *file)
{
	return GP_OK;
}

int stv0674_capture(GPPort *port)
{
	return GP_OK;
}


int stv0674_capture_preview(GPPort *port, char **data, int *size)
{
	return GP_OK;
}


int stv0674_delete_all(GPPort *port) {
    /*    return stv0674_try_cmd(port,CMDID_SET_IMAGE_INDEX,0,NULL,0);*/
	return GP_OK;
}

int stv0674_summary(GPPort *port, char *txt)
{
    return GP_OK;
}
