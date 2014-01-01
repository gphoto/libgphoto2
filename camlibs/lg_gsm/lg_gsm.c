/* lg_gsm.c
 *
 * Copyright (C) 2005 Guillaume Bedot <littletux@zarb.org>
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

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#include "lg_gsm.h"

#define GP_MODULE "lg_gsm" 

#define MSGWRITE gp_port_usb_msg_write
#define WRITE gp_port_write
#define READ  gp_port_read

static char sync_start[] = "\x2\x0\x0\x0\x0\x0";
static char sync_stop[] = "\x3\x0\x0\x0\x0\x0";
static char get_firmware[] = "\x1\x0\x0\x0\x0\x0";
static char list_all_photo[]= "\x04\x0\x08\x0\x0\x0\x40\x0\x0\x0\xff\xff\xff\xff";
static char get_photo_cmd[]="\x0b\x0\x8a\x0\x0\x0\x40\x0\x0\x0";

int lg_gsm_init (GPPort *port, Model *model, Info *info) 
{
	char firmware[54];
	char oknok[6];
	memset (oknok,0,6);
	memset (firmware,0,54);

	GP_DEBUG("Running lg_gsm_init\n");
	port->timeout=20000;
	/* syncstart */
	MSGWRITE(port, 0x13, 0x6, 0x0, "", 0);
	WRITE(port, sync_start, 6);
	READ(port, oknok, 6);
	/* getfirmware : write 0x010000000000 */
	MSGWRITE(port, 0x13, 0x6, 0x0, "", 0);
	WRITE(port, get_firmware, 0x6);
	READ(port, firmware, 54);
	/* syncstop */
	MSGWRITE(port, 0x13, 0x6, 0x0, "", 0);
	WRITE(port, sync_stop, 6);
	READ(port, oknok, 6);

	/* This information, too. */
	memcpy (info, &firmware[6], 40);

	GP_DEBUG("info = %s\n", info);
	/*GP_DEBUG("info[20] = 0x%x\n", firmware[26]);*/
	GP_DEBUG("Leaving lg_gsm_init\n");

        return GP_OK;
}

unsigned int lg_gsm_get_picture_size  (GPPort *port, int pic) 
{

        unsigned int size;
	/* example : list photos 2 & 3: 0x04000800000040000000
	   + 0100 : from
	   + 0200 : to
	*/
	char listphotos[] = "\x04\x0\x08\x0\x0\x0\x40\x0\x0\x0\x0\x0\x0\x0";
	char photonumber[22];
	char photodesc[142]; /* 1 * 142 */
	char oknok[6];
	memset (oknok,0,6);
	memset (photonumber,0,22);
	memset (photodesc,0,142);

	/*listphotos[11]=listphotos[13]=pic / 256;*/
	/*listphotos[10]=listphotos[12]=pic % 256;*/
	listphotos[10] = listphotos[12] = pic;

	GP_DEBUG("Running lg_gsm_get_picture_size\n");
	MSGWRITE(port, 0x13, 0x6, 0x0, "", 0);
	WRITE(port, sync_start, 6);
	READ(port, oknok, 6);
	MSGWRITE(port, 0x13, 0xe, 0x0, "", 0);
	WRITE(port, listphotos, 0xe);
	/* read 22 */
	READ(port, photonumber, 0x16);
	/* then read 142 */
	READ(port, photodesc, 0x8e);
	size = (unsigned int)photodesc[138] + (unsigned int)photodesc[139]*0x100 + (unsigned int)photodesc[140]*0x10000+(unsigned int)photodesc[141]*0x1000000;
	GP_DEBUG(" size of picture %i is 0x%x\n", pic, size);
	/* max. 1280x960x24bits ? */
	if ( (size >= 0x384000 ) ) {return GP_ERROR;} 
	MSGWRITE(port, 0x13, 0x6, 0x0, "", 0);
	WRITE(port, sync_stop, 6);
	READ(port, oknok, 6);
	GP_DEBUG("Leaving lg_gsm_get_picture_size\n");
	return size;
}

int lg_gsm_read_picture_data (GPPort *port, char *data, int size, int n) 
{
	char listphotos[] = "\x04\x0\x08\x0\x0\x0\x40\x0\x0\x0\x0\x0\x0\x0";

	char photonumber[22];
	char photodesc[142]; /* 1 * 142 */
	char getphoto[144];
	char getphotorespheader[150];
	char block[50000];
	char oknok[6];

	int pos=0;
	int block_size=50000;
	int header_size=8;
	int nb_blocks;
	int i;
	int remain;

	memset (oknok,0,6);
	memset (photonumber,0,22);
	memset (photodesc,0,142);
	memset (getphoto,0,144);
	memset (getphotorespheader,0,150);
	memset (block,0,50000);

	/*listphotos[11]=listphotos[13]=n / 256;*/
	/*listphotos[10]=listphotos[12]=n % 256;*/
	listphotos[10]=listphotos[12]=n;
	/* port->timeout=20000;*/
	GP_DEBUG("Running lg_gsm_read_picture_data\n");
	/* syncstart */
	MSGWRITE(port, 0x13, 0x6, 0x0, "", 0);
	WRITE(port, sync_start, 6);
	READ(port, oknok, 6);

	MSGWRITE(port, 0x13, 0x0e, 0x0, "", 0);
	WRITE(port, listphotos, 0xe);
	/* read 22 */
	READ(port, photonumber, 0x16);
	/* then read 142 */
	READ(port, photodesc, 142);
	size = (int)photodesc[138] + (int)photodesc[139]*0x100 + (int)photodesc[140]*0x10000+(int)photodesc[141]*0x1000000;
	GP_DEBUG(" size of picture %i is 0x%x\n", n, size);
	/* max. 1280x960x24bits ? */
	if ( (size >= 0x384000 ) ) {
		return GP_ERROR;
	}

	memcpy(getphoto, &get_photo_cmd[0], 10);
	memcpy(getphoto +10, &n, 1); /*TODO: fix this*/
	/*memcpy(getphoto +11, 0, 1);*/
	memcpy(getphoto +12, &photodesc[6],44);
	memcpy(getphoto +56, &photodesc[50],88);
	/* send getphoto cmd */
	MSGWRITE(port, 0x13, 0x90, 0x0, "", 0);
	WRITE(port, getphoto, 0x90);
	/* read */
	READ(port, getphotorespheader, 0x96);

	nb_blocks=size/block_size+1;
	/*port->timeout=15000;*/
	for (i = 1 ; i <= nb_blocks ; i++)
	{
		remain = size - pos;
		if (remain >= block_size - header_size)
		{
			READ(port, block, block_size);
			memcpy(data+pos,&block[header_size],block_size - header_size);
			pos=pos+block_size-header_size;
		}
		else {
			READ(port,block, remain+header_size);
			memcpy(data+pos,&block[header_size],remain);
			pos=pos+remain;
		}
		
	}
	/*port->timeout=5000;*/
	/* syncstop */
	MSGWRITE(port, 0x13, 0x6, 0x0, "", 0);
	WRITE(port, sync_stop, 6);
	READ(port, oknok, 6);
	GP_DEBUG("Leaving lg_gsm_read_picture_data\n");

        return GP_OK;
}

int lg_gsm_list_files (GPPort *port, CameraList *list) 
{
	int num_pics;
	int i;	

	char oknok[6];
	char photonumber[22];
	char photolist[142000]; /* max_photos * 142 */
	char name[44];
	char value[88];

	memset (oknok,0,6);
	memset (photonumber,0,22);
	memset (photolist,0,142000);
	memset (name,0,44);
	memset (value,0,88);

	GP_DEBUG("Running lg_gsm_list_files\n");

	/* set timeout to 3s */
	/*port->timeout=20000;*/
	/* syncstart */
	MSGWRITE(port, 0x13, 0x6, 0x0, "", 0);
	WRITE(port, sync_start, 6);
	READ(port, oknok, 6);

	/* lsphoto : write 0x04000800000040000000ffffffff */
	MSGWRITE(port, 0x13, 0xe, 0x0, "", 0);
	WRITE(port, list_all_photo, 0xe);
	READ(port, photonumber, 0x16);

	num_pics=photonumber[20]+256*photonumber[21];

	/* increase timeout to 20s */
	/*port->timeout=20000;*/
	/* read 142 * nb_photos */
	READ(port, photolist, 142*num_pics);

	for (i = 0; i < num_pics; i++){
		/* sprintf( name, "lg_gsm_pic%03i.jpg", i ); */
		memcpy(name,&photolist[6+142*i],44);
		memcpy(value,&photolist[50+142*i],80);
		gp_list_append(list, name, value);
	}
	/* restore timeout to 5s */
	/*port->timeout=5000; */
	/* syncstop */
	MSGWRITE(port, 0x13, 0x6, 0x0, "", 0);
	WRITE(port, sync_stop, 6);
	READ(port, oknok, 6);
	/*port->timeout=5000;*/

	GP_DEBUG("Number of pics : %03i\n", num_pics);

	GP_DEBUG("Leaving lg_gsm_list_files\n");

        return GP_OK;
}
