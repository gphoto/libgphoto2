#include <stdio.h>
#include <stdlib.h>
#include "pdrm65.h"
#define GP_MODULE "setting"

int pdrm65_init(GPPort *port)
{
	unsigned char out_buf[1024];
	unsigned char in_buf[1024];
	//int timeout = 50;
	int ret = GP_OK;
	char ok = 0x06;
	gp_port_set_timeout(port,1000);
	memset(out_buf, 0, sizeof(out_buf));
	memset(in_buf, 0, sizeof(in_buf));
	/* exactly what windows driver does */
	//gp_port_usb_msg_read (port, 0x01, PDRM11_CMD_READY, 0, buf, 4);
	out_buf[0] = 0x1b;
	out_buf[1] = 0x43;
	out_buf[2] = 0x02;
	out_buf[3] = 0x00;
	out_buf[4] = 0x04;
	out_buf[5] = 0x1b;
	out_buf[6] = 0x1f;
	out_buf[7] = 0x00;
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 11);
	gp_port_write(port, &ok, 1);
	
	//out_buf[0-3]=0x1b 43 02 00
	out_buf[4]=0x01;
	out_buf[5]=0x02;
	out_buf[6]=0x03;
	//out_buf[7]=0x00
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 10);
	gp_port_write(port, &ok, 1);
	
	//out_buf[0-3]=0x1b 43 02 00
	out_buf[4] = 0x04;
	out_buf[5] = 0x1b;
	out_buf[6] = 0x1f;
	//out_buf[7]=0x00
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 11);
	//in_buf[4-7] == "SI15"
	gp_port_write(port, &ok, 1);
	
	//out_buf[0-3]=0x1b 43 02 00
	out_buf[4]=0x04;
	out_buf[5]=0x16;
	out_buf[6]=0x1a;
	//out_buf[7]=0x00
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 14);
	//in_buf[4-10] == "PDR-M65
	gp_port_write(port, &ok, 1);
	
	return(ret);
}
int pdrm65_get_filenames(GPPort *port, CameraList *list)
{

	uint32_t numPics;
	char out_buf[1024];
	char in_buf[1024];
	char ok = 0x06;
	int i;
	char name[10];
	
	gp_port_set_timeout(port, 10000);
	out_buf[0] = 0x1b;
	out_buf[1] = 0x43; 
	out_buf[2] = 0x02;
	out_buf[3] = 0x00;
	out_buf[4] = 0x01;
	out_buf[5] = 0x10;
	out_buf[6] = 0x11;
	out_buf[7] = 0x00;
	
	
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 10);
	/* trying to remain endian friendly */
	gp_port_write(port, &ok, 1);
	
	
	out_buf[0] = 0x1b;
	out_buf[1] = 0x43; 
	out_buf[2] = 0x02;
	out_buf[3] = 0x00;
	out_buf[4] = 0x01;
	out_buf[5] = 0x0a;
	out_buf[6] = 0x0b;
	out_buf[7] = 0x00;
	
	
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 10);
	/* trying to remain endian friendly */
	gp_port_write(port, &ok, 1);
	
	numPics = in_buf[4] + (in_buf[5] * 256);
	GP_DEBUG("found %d pictures", numPics);

	
	for(i=1;  i<numPics+1;  i++) {
		pdrm65_select_file(port, i);

		/* There do not appear to be any filenames in the protocol, just
		an index, first image is index "1" second is index "2" etc...  */
		sprintf(name,"%d",i);
		

		GP_DEBUG("Found picture number %s",name);
		gp_list_append(list, name, NULL);
	}
	return(GP_OK);
}
int pdrm65_select_file(GPPort *port, int current_pic)
{
	char out_buf[1024] = "\033\103\006\000\000\004\001\000\000\000\005\000";
	char in_buf[1024];
	
	//sprintf(&out_buf[6],"%o",current_pic);
	gp_port_write(port, out_buf, 12);

	gp_port_read (port, in_buf, 1);
	
	if (in_buf[0] != 0x06)
	{
		return -1;
	}
	return(GP_OK);
}

int pdrm65_get_pict(GPPort *port, const char *pic_number, CameraFile *file)
{
	char out_buf[1024];
	char in_buf[1024];
	char ok = 0x06;
	char pic_buf[JPEG_CHUNK];
	uint8_t *image;
	int pic_size = 0;
	int data_remaining = 0;
	
	int image_number = atoi(pic_number);
	
	// Set active image to image_number
	//1b 43 06 00 00 04 01 00 00 00 05 00
	pdrm65_select_file(port, image_number);
	// Get size of the image for checking during the transfer.
	//1b 43 02 00 01 0c 0d 00
	out_buf[0]=0x1b;
	out_buf[1]=0x43;
	out_buf[2]=0x02;
	out_buf[3]=0x00;
	out_buf[4]=0x01;
	out_buf[5]=0x0c;
	out_buf[6]=0x0d;
	out_buf[7]=0x00;
	
	gp_port_write(port,out_buf,8);
	gp_port_read(port, in_buf,10);
	gp_port_write(port,&ok,1);
	
	pdrm65_select_file(port, image_number);
	
	pic_size = ( (in_buf[6]*(256*256)) + (in_buf[5]*256) + in_buf[4] );
	GP_DEBUG("Picture is size %d KBytes", pic_size);
	
	image = malloc(pic_size + (((pic_size / JPEG_CHUNK)+1)*60));
	
	//1b 43 02 00 04 0e 12 00
	//out_bf[0-1]=0x1b 0x43
	out_buf[2]=0x02;
	out_buf[3]=0x00;
	out_buf[4]=0x04;
	out_buf[5]=0x0e;
	out_buf[6]=0x12;
	out_buf[7]=0x00;
	gp_port_write(port,out_buf,8);
	while(data_remaining < sizeof(image))
	{
		GP_DEBUG("Size of image = %d, data_remaining = %d", sizeof(image), data_remaining);
		//Get EXIF Header information for this packet
		gp_port_read(port, in_buf, 64);
		memcpy(image+data_remaining, &in_buf[4],60);
				
		if ((pic_size - data_remaining) > JPEG_CHUNK)
		{
			gp_port_read(port, pic_buf, JPEG_CHUNK);
			memcpy(image+data_remaining+60, pic_buf, JPEG_CHUNK);
			data_remaining += (JPEG_CHUNK + 60);
		}
		else
		{
			gp_port_read(port, pic_buf, (pic_size - data_remaining));
			memcpy(image+(pic_size-data_remaining+60), in_buf, (pic_size - data_remaining));
			data_remaining += (pic_size - data_remaining + 60 );
		}

		gp_port_read(port, in_buf, 6);
		gp_port_write(port, &ok, 1);
		
		
		
		GP_DEBUG("data_remaining = %d, sizeof image = %d, size of pic_size = %d", data_remaining, sizeof(image), pic_size);
		
	}
	gp_file_set_mime_type(file, GP_MIME_EXIF);
	gp_file_set_data_and_size(file, image, pic_size);
	return (GP_OK);
}
