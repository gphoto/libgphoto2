/*
 * STV0680 Vision Camera Chipset Driver
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
 *
 * 10/11/01/cw	Added support for STMicroelectronics USB Dual-mode camera
 *		(Aiptek PenCam Trio)  
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <gphoto2-port.h>

#include "stv0680.h"
#include "library.h"
#include "bayer.h"
#include "libgphoto2/pattrec.h"

#define CMD_RETRIES		0x03

#define CMD_PING		0x88
#define CMD_PING_RLEN		0x02
#define CMD_GET_FILE_INFO	0x86
#define CMD_GET_FILE_INFO_RLEN	0x10
#define CMD_GET_IMAGE		0x83
#define CMD_GET_IMAGE_RLEN	0x10
#define CMD_GET_PREVIEW		0x84
#define CMD_GET_PREVIEW_RLEN	0x10

#define CMD_GET_IMAGE_INFO	0x8f
#define CMD_GET_IMAGE_INFO_RLEN	0x10
#define CMD_CAPTURE_IMAGE	0x09
#define CMD_TO_DSC		0x0a

#define CMD_OK			0x00
#define CMD_IO_ERROR		0x01
#define CMD_IO_TIMEOUT		0x02
#define CMD_BAD_RESPONSE	0x03

#define STV0680_CIF_WIDTH	356
#define STV0680_CIF_HEIGHT	292

static int stv0680_remap_gp_port_error(int error)
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

static unsigned char stv0680_checksum(const unsigned char *data, int start, int end)
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

static int stv0680_cmd(GPPort *device, unsigned char cmd,
		unsigned char data1, unsigned char data2, unsigned char data3,
		unsigned char *response, unsigned char response_len)
{
	unsigned char packet[] = { 0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x03 };
	unsigned char rhdr[6];
	int ret;

	// build command packet
	packet[1] = cmd;
	packet[2] = response_len;
	packet[3] = data1;
	packet[4] = data2;
	packet[5] = data3;
	packet[6] = stv0680_checksum(packet, 1, 5);

	// write to device
	printf("Writing packet to device\n");
	if((ret = gp_port_write(device, packet, 8)) != GP_OK)
		return stv0680_remap_gp_port_error(ret);

	printf("Reading response header\n");
	// read response header
	if((ret = gp_port_read(device, rhdr, 6)) != 6)
		return stv0680_remap_gp_port_error(ret);

	printf("Read response\n");
	// read response
	if((ret = gp_port_read(device, response, response_len)) != response_len)
		return stv0680_remap_gp_port_error(ret);

	printf("Validating packet [0x%X,0x%X,0x%X,0x%X,0x%X,0x%X]\n",
rhdr[0], rhdr[1], rhdr[2], rhdr[3], rhdr[4], rhdr[5]);
	// validate response
	if(rhdr[0] != 0x02 || rhdr[1] != cmd ||
	   rhdr[2] != response_len ||
	   rhdr[3] != stv0680_checksum(response, 0, response_len - 1) ||
	   rhdr[4] != stv0680_checksum(rhdr, 1, 3) ||
	   rhdr[5] != 0x03)
		return CMD_BAD_RESPONSE;

	printf("Packet OK\n");
	return CMD_OK;
}

static int stv0680_try_cmd(GPPort *device, unsigned char cmd,
		unsigned char data1, unsigned char data2, unsigned char data3,
		unsigned char *response, unsigned char response_len,
		int retries)
{
	int ret;

	while(retries--) {
		printf("Trying command 0x%X\n", cmd);

		switch(ret = stv0680_cmd(device, cmd, data1, data2, data3,
					 response, response_len)) {
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

int stv0680_ping(GPPort *device)
{
	unsigned char response[CMD_PING_RLEN];
	int ret;

	switch (device->type) {
	case GP_PORT_USB:
		if ((ret = gp_port_usb_msg_read(device, CMD_PING, 0x55, 0xAA,
			response, sizeof(response)) < 0))
			ret = stv0680_remap_gp_port_error(ret);
		break;
	default:
	case GP_PORT_SERIAL:
		ret = stv0680_try_cmd(device, CMD_PING, 0x55, 0xAA, 0x00,
			      response, sizeof(response), CMD_RETRIES);
		break;
	}

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

int stv0680_file_count(GPPort *device, int *count)
{
	unsigned char response[CMD_GET_FILE_INFO_RLEN];
	int ret;

printf("STV: getting file count\n");

	switch (device->type) {

		case GP_PORT_USB:
			if ((ret = gp_port_usb_msg_read(device, CMD_GET_FILE_INFO, 0x00, 0x00,
				response, sizeof(response)) < 0))
				
				ret = stv0680_remap_gp_port_error(ret);
			
			break;

		default:
		case GP_PORT_SERIAL:
			ret = stv0680_try_cmd(device, CMD_GET_FILE_INFO, 0x00, 0x00, 0x00,
				response, sizeof(response), CMD_RETRIES);
			break;
	}

	switch(ret) {
	case CMD_IO_ERROR:
		printf("IO error!\n");
		return GP_ERROR_IO;
	case CMD_OK:
		printf("GFI OK, count = %d\n", response[1]);
		*count = response[1];
		return GP_OK;
	default:
		//Should not be reached.
		return GP_ERROR;
	}
}

int stv0680_get_image(GPPort *device, int image_no,
			char **data, int *size)
{
	unsigned char response[CMD_GET_IMAGE_RLEN], header[64];
	unsigned char *raw;
	int h,w;
	int ret;

	switch (device->type) {
	case GP_PORT_USB:
		if ((ret = gp_port_usb_msg_read(device, CMD_GET_IMAGE,
			image_no, 0x00, response, sizeof(response)) < 0))
			ret = stv0680_remap_gp_port_error(ret);
		w = response[4] << 8 | response[5];
		h = response[6] << 8 | response[7];
		*size = w * h;
		*size += (*size % 8) ? 8 - (*size % 8) : 0;
		break;
	default:
	case GP_PORT_SERIAL:
		ret = stv0680_try_cmd(device, CMD_GET_IMAGE, 0x00, image_no,
			0x00, response, sizeof(response), CMD_RETRIES);
		w = response[4] << 8 | response[5];
		h = response[6] << 8 | response[7];
		*size = w * h;
		break;
	}

	if(ret == CMD_IO_ERROR)
		return GP_ERROR_IO;

	printf("Image is %dx%d (%ul bytes)\n", w, h, *size);

	raw = malloc(*size);

	switch(gp_port_read(device, raw, *size)) {
	case GP_ERROR_TIMEOUT:
		printf("read timeout\n"); break;
	case GP_ERROR:
		printf("IO error\n"); break;
	default:
		printf("Read bytes!\n"); break;
	}

	sprintf(header, "P6\n# gPhoto2 stv0680 image\n%d %d\n255\n", w, h);

	*data = malloc((*size * 3) + strlen(header));
	strcpy(*data, header);

	bayer_unshuffle(w, h, raw, *data + strlen(header));
	if(pattrec(w, h, *data + strlen(header)) != 0) {
		// fallback in low memory conditions
		bayer_demosaic(w, h, *data +strlen(header));
	}

	free(raw);

	*size *= 3;
	*size += strlen(header);

	return GP_OK;
}

int stv0680_get_image_preview(GPPort *device, int image_no,
			char **data, int *size)
{
	unsigned char response[CMD_GET_IMAGE_RLEN], header[64];
	unsigned char *raw;
	int h,w,scale;
	int ret;

	switch (device->type) {
	case GP_PORT_USB:
		if ((ret = gp_port_usb_msg_read(device, CMD_GET_IMAGE,
			image_no, 0x00, response, sizeof(response)) < 0))
			ret = stv0680_remap_gp_port_error(ret);
		w = response[4] << 8 | response[5];
		h = response[6] << 8 | response[7];
		*size = w * h;			
		*size += (*size % 8) ? 8 - (*size % 8) : 0;
		scale = (w>>8)+1;
		break;
	default:
	case GP_PORT_SERIAL:
		ret = stv0680_try_cmd(device, CMD_GET_PREVIEW, 0x00, image_no,
			0x00, response, sizeof(response), CMD_RETRIES);
		w = response[4] << 8 | response[5];
		h = response[6] << 8 | response[7];
		*size = w * h;
		scale = 0;
		break;
	}

	if(ret == CMD_IO_ERROR)
		return GP_ERROR_IO;

	raw = calloc(1, *size);

	switch(gp_port_read(device, raw, *size)) {
	case GP_ERROR_TIMEOUT:
		printf("read timeout\n"); break;
	case GP_ERROR:
		printf("IO error\n"); break;
	default:
		printf("Read bytes!\n"); break;
	}

	w >>=scale;
	h >>=scale;
	*size = w * h;

	printf("Image is %dx%d (%ul bytes)\n", w, h, *size);

	sprintf(header, "P6\n# gPhoto2 stv0680 image\n%d %d\n255\n", w, h);

	*data = calloc(1,(*size * 3) + strlen(header));
	strcpy(*data, header);

	bayer_unshuffle_preview(w<<scale, h<<scale, scale, raw, *data + strlen(header));
	// if(pattrec(w, h, *data + strlen(header)) != 0) {
	//	// fallback in low memory conditions
	//	bayer_demosaic(w, h, *data +strlen(header));
	//}

	free(raw);

	*size *= 3;
	*size += strlen(header);

	return GP_OK;
}

int stv0680_capture_preview(GPPort *device, char **data, int *size)
{
	unsigned char response[CMD_GET_IMAGE_RLEN], header[64];
	unsigned char *raw;
	int h,w;
	int ret;

	switch (device->type) {
	case GP_PORT_USB:
		if ((ret = gp_port_usb_msg_write(device, CMD_CAPTURE_IMAGE,
					0x00, 0x02 , response, 0x0) < 0))
			ret = stv0680_remap_gp_port_error(ret);
		break;
	default:
	case GP_PORT_SERIAL:
		return (GP_ERROR_NOT_SUPPORTED);
		break;
	}

	if(ret == CMD_IO_ERROR)
		return GP_ERROR_IO;

	w = STV0680_CIF_WIDTH;
	h = STV0680_CIF_HEIGHT;
	*size = w * h;

	printf("Image is %dx%d (%u bytes)\n", w, h, *size);

	raw = malloc(*size);

	switch(gp_port_read(device, raw, *size)) {
	case GP_ERROR_TIMEOUT:
		printf("read timeout\n"); break;
	case GP_ERROR:
		printf("IO error\n"); break;
	default:
		printf("Read bytes!\n"); break;
	}

	switch (device->type) {
	case GP_PORT_USB:
		if ((ret = gp_port_usb_msg_write(device, CMD_TO_DSC, 0x00,
						0x00, response, 0x00) < 0))
			ret = stv0680_remap_gp_port_error(ret);
		break;
	default:
	case GP_PORT_SERIAL:
		return (GP_ERROR_NOT_SUPPORTED);
		break;
	}

	sprintf(header, "P6\n# gPhoto2 stv0680 image\n%d %d\n255\n", w, h);

	*data = malloc((*size * 3) + strlen(header));
	strcpy(*data, header);

	bayer_unshuffle(w, h, raw, *data + strlen(header));
	if(pattrec(w, h, *data + strlen(header)) != 0) {
		// fallback in low memory conditions
		bayer_demosaic(w, h, *data + strlen(header));
	}

	free(raw);

	*size *= 3;
	*size += strlen(header);

	return GP_OK;
}
