/*
 * Jenoptik JD350e Driver
 * Copyright (C) 2001 Michael Trawny <trawny99@yahoo.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <gphoto2.h>
#include <gphoto2-port.h>

#include "jd350e.h"
#include "library.h"
#include "bayer.h"
#include "libgphoto2/pattrec.h"

#define CMD_RETRIES		0x03

#define CMD_PING		0x41
#define CMD_REQUEST_DATA	0x61
#define CMD_FETCH_DATA  	0x15
#define CMD_SET_IMAGE_NUMBER	0xf6
#define CMD_SET_IMAGE_NUMBER_RLEN	0x08

#define CMD_GET_FILE_INFO	0x40
#define CMD_GET_FILE_INFO_RLEN	0x501
#define CMD_GET_IMAGE		0x00
#define CMD_GET_IMAGE_RLEN	0x281
#define CMD_GET_PREVIEW		0x02
#define CMD_GET_PREVIEW_RLEN	0x51
#define CMD_GET_HEADER 		0x80
#define CMD_GET_HEADER_RLEN	0x21

#define CMD_OK			0x00
#define CMD_IO_ERROR		0x01
#define CMD_IO_TIMEOUT		0x02
#define CMD_BAD_RESPONSE	0x03

static int  jd350e_remap_gp_port_error(int error)
{
	switch(error) {
	case GP_ERROR_TIMEOUT:
		printf("Remapping GP_ERROR_TIMEOUT->CMD_IO_TIMEOUT\n");
		return CMD_IO_TIMEOUT;
	case GP_ERROR:
		printf("Remapping GP_ERROR->CMD_IO_ERROR\n");
	default:
		printf("(generic error %d actually)\n", error);
		return CMD_IO_ERROR;
	}
}

static unsigned char  jd350e_checksum(const unsigned char *data, int start, int end)
{
	unsigned char sum = 0;
	int i;

	for(i = start; i <= end; ++i) {
		sum += data[i];
		sum &= 0xff;
	}

	printf("Calculated checksum 0x%X\n", sum);

	return sum;
}

static int jd350e_cmd(GPPort *device, unsigned char cmd, 
		unsigned char cmd_len,
		unsigned char data1, unsigned char data2, unsigned char data3,
		unsigned char data4, 
		unsigned char *response, unsigned response_len)
{
	unsigned char packet[5];
	unsigned char rhdr;
	int ret;

	// build command packet
	packet[0] = cmd;
	packet[1] = data1;
	packet[2] = data2;
	packet[3] = data3;
	packet[4] = data4;

	// write to device
	printf("Writing packet to device [0x%X...]\n",cmd);
	if((ret = gp_port_write(device, packet, cmd_len)) != GP_OK)
		return  jd350e_remap_gp_port_error(ret);

	printf("Reading Command Echo\n");
	// read response echo
	if((ret = gp_port_read(device, &rhdr, 1)) != 1)
		return  jd350e_remap_gp_port_error(ret);

	printf("Validating Echo [0x%X]\n",rhdr);
	if( rhdr != cmd )
		return CMD_BAD_RESPONSE;

	if( response_len == 0 )
		return CMD_OK;

	printf("Read Response\n");
	// read response
	if((ret = gp_port_read(device, response, response_len)) != response_len)
		return  jd350e_remap_gp_port_error(ret);

	printf("Validating Response [0x%X...0x%X]\n",
		response[0],response[response_len-1] );

	// validate response
	if( response[response_len-1] != jd350e_checksum(response, 0, response_len-2) )
		return CMD_BAD_RESPONSE;

	printf("Packet OK\n");
	return CMD_OK;
}

static int jd350e_try_cmd(GPPort *device, unsigned char cmd, unsigned char cmd_len,
		unsigned char data1, unsigned char data2, unsigned char data3,
		unsigned char data4, 
		unsigned char *response, unsigned response_len,
		int retries)
{
	int ret;

	while(retries--) {
		printf("Trying command 0x%X\n", cmd);

		switch(ret =  jd350e_cmd(device, cmd, cmd_len, data1, data2, data3,
					 data4, response, response_len)) {
		case CMD_IO_TIMEOUT:
		case CMD_BAD_RESPONSE:
			break;
		default:
			return ret;
		}
	}

	printf("Failed command, retries exhausted\n");

	return CMD_IO_ERROR;
}

int  jd350e_ping(GPPort *device)
{
	int ret;
	printf("JD350e: pinging camera\n");
	ret =  jd350e_try_cmd(device, CMD_PING,1, 0,0,0,0, 0,0, CMD_RETRIES);

	switch(ret) {
	case CMD_IO_ERROR:
		printf("ping failed\n");
		return GP_ERROR_IO;
	case CMD_OK:
		printf("ping ok\n");
		return GP_OK;
	default:
		//Should not be reached.
		return GP_ERROR;
	}
}

int  jd350e_file_count(GPPort *device, int *count)
{
	unsigned char response[CMD_GET_FILE_INFO_RLEN];
	int ret;

	printf("JD350e: getting file count\n");
	ret =  jd350e_try_cmd(device, CMD_REQUEST_DATA,2, CMD_GET_FILE_INFO,0,0,0,
			      0,0, CMD_RETRIES);

	if( ret == CMD_OK ){
	ret =  jd350e_try_cmd(device, CMD_FETCH_DATA,5, 0,0,0,1,
			      response, sizeof(response), CMD_RETRIES);
	}
	
	sleep(2); // give the camera a chance to stop beeping

	switch(ret) {
	case CMD_IO_ERROR:
		printf("IO error!\n");
		return GP_ERROR_IO;
	case CMD_OK:
		printf("GFI OK, count = %d\n", response[2]);
		*count = response[2];
		return GP_OK;
	default:
		//Should not be reached.
		return GP_ERROR;
	}
}

static int jd350e_get_image(GPPort *device, int image_no, int subcmd,
			char **data, int *size, int w, int h, int interpolate)
{
	unsigned char set_image_no_response[CMD_SET_IMAGE_NUMBER_RLEN], 
                      get_header_response[CMD_GET_HEADER_RLEN];
	unsigned char *raw;
	unsigned char header[128];
	int ret;

	ret =  jd350e_try_cmd(device, CMD_SET_IMAGE_NUMBER,2, image_no,0,0,0,
			      set_image_no_response, sizeof(set_image_no_response), 
	                      CMD_RETRIES);

	if(ret != CMD_OK) return GP_ERROR_IO;

	ret =  jd350e_try_cmd(device, CMD_REQUEST_DATA,2, CMD_GET_HEADER,0,0,0,
			      0, 0, CMD_RETRIES);

	if(ret != CMD_OK) return GP_ERROR_IO;

	ret =  jd350e_try_cmd(device, CMD_FETCH_DATA,5, 0,0,0,1,
			      get_header_response, sizeof(get_header_response), 
	                      CMD_RETRIES);

	if(ret != CMD_OK) return GP_ERROR_IO;

	*size = w * h;

	printf("Image is %dx%d (%ul bytes)\n", w, h, *size);

	raw = malloc(*size+1);

	ret =  jd350e_try_cmd(device, CMD_REQUEST_DATA,2, subcmd,0,0,0,
			      0, 0, CMD_RETRIES);

	if(ret != CMD_OK) return GP_ERROR_IO;

	ret =  jd350e_try_cmd(device, CMD_FETCH_DATA,5, 0,0,h/256,h%256,
			      raw, *size+1, CMD_RETRIES);

	if(ret != CMD_OK) return GP_ERROR_IO;

	if( interpolate ){
		sprintf(header, "P6\n# gPhoto2 JD350e interpolated image\n%d %d\n255\n", w, h);
	}
	else{
		sprintf(header, "P5\n# gPhoto2 JD350e raw image\n%d %d\n255\n", w, h);
	}

	*data = malloc((*size * (interpolate ? 3 : 1) ) + strlen(header));
	strcpy(*data, header);

	if( interpolate ){
		*size *= 3;
		bayer_unshuffle(w, h, raw, *data + strlen(header));
		if(pattrec(w, h, *data + strlen(header)) != 0) {
			// fallback in low memory conditions
			bayer_demosaic(w, h, *data +strlen(header));
		}
	}
	else{
		memcpy( *data + strlen(header), raw, *size );
	}

	free(raw);

	*size += strlen(header);

	return GP_OK;
}

int  jd350e_get_image_full(GPPort *device, int image_no,
		        char **data, int *size)
{
	image_no++;
	printf("JD350e: getting full image %d\n",image_no);
	return
	jd350e_get_image(device,image_no,CMD_GET_IMAGE,data,size,640,480,1);	
}

int  jd350e_get_image_raw(GPPort *device, int image_no,
		        char **data, int *size)
{
	image_no++;
	printf("JD350e: getting raw image %d\n",image_no);
	return
	jd350e_get_image(device,image_no,CMD_GET_IMAGE,data,size,640,480,0);	
}

int  jd350e_get_image_preview(GPPort *device, int image_no,
			char **data, int *size)
{
	image_no++;
	printf("JD350e: getting preview image %d\n",image_no);
	return
	jd350e_get_image(device,image_no,CMD_GET_PREVIEW,data,size,80,60,1);	
}

