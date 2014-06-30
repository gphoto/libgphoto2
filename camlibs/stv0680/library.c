/*
 * STV0680 Vision Camera Chipset Driver
 * Copyright 2000 Adam Harrison <adam@antispin.org>
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#include "stv0680.h"
#include "library.h"
#include "sharpen.h"
#include "bayer.h"
#include "saturate.h"
#include "../../libgphoto2/bayer.h"
#include "demosaic_sharpen.h"

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

#define CMD_RETRIES		0x03

#define STV0680_QCIF_WIDTH	178
#define STV0680_QCIF_HEIGHT	146
#define STV0680_CIF_WIDTH	356
#define STV0680_CIF_HEIGHT	292
#define STV0680_QVGA_WIDTH	324
#define STV0680_QVGA_HEIGHT	244
#define STV0680_VGA_WIDTH	644
#define STV0680_VGA_HEIGHT	484

static unsigned char
stv0680_checksum(const unsigned char *data, int start, int end)
{
	unsigned char sum = 0;
	int i;

	for(i = start; i <= end; ++i) {
		sum += data[i];
		sum &= 0xff;
	}
	return sum;
}

static int stv0680_cmd(GPPort *port, unsigned char cmd,
		unsigned char data1, unsigned char data2, unsigned char data3,
		unsigned char *response, unsigned char response_len)
{
	unsigned char packet[] = { 0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x03 };
	unsigned char rhdr[6];
	int ret;

	/* build command packet */
	packet[1] = cmd;
	packet[2] = response_len;
	packet[3] = data1;
	packet[4] = data2;
	packet[5] = data3;
	packet[6] = stv0680_checksum(packet, 1, 5);

	/* write to port */
	printf("Writing packet to port\n");
	if((ret = gp_port_write(port, (char *)packet, 8)) < GP_OK)
		return ret;

	printf("Reading response header\n");
	/* read response header */
	if((ret = gp_port_read(port, (char *)rhdr, 6)) != 6)
		return ret;

	printf("Read response\n");
	/* read response */
	if((ret = gp_port_read(port, (char *)response, response_len)) != response_len)
	    return ret;

	printf("Validating packet [0x%X,0x%X,0x%X,0x%X,0x%X,0x%X]\n",
	       rhdr[0], rhdr[1], rhdr[2], rhdr[3], rhdr[4], rhdr[5]);
	/* validate response */
	if(rhdr[0] != 0x02 || rhdr[1] != cmd ||
	   rhdr[2] != response_len ||
	   rhdr[3] != stv0680_checksum(response, 0, response_len - 1) ||
	   rhdr[4] != stv0680_checksum(rhdr, 1, 3) ||
	   rhdr[5] != 0x03)
		return GP_ERROR_BAD_PARAMETERS;

	printf("Packet OK\n");
	return GP_OK;
}

int stv0680_try_cmd(GPPort *port, unsigned char cmd,
		unsigned short data,
		unsigned char *response, unsigned char response_len
) {
	int ret;
	switch (port->type) {
	case GP_PORT_USB:
	    /* Most significant bit set, data flows from camera->host */
	    if (cmd & 0x80)
		ret=gp_port_usb_msg_read(port,cmd,data,0,(char *)response,response_len);
	    else
		ret=gp_port_usb_msg_write(port,cmd,data,0,(char *)response,response_len);

	    if (ret == response_len)
		return GP_OK;
	    return ret;
	    break;
	case GP_PORT_SERIAL: {
	    int retries = CMD_RETRIES;
	    while(retries--) {
		/* data3 was always 0 */
		switch(ret=stv0680_cmd(port,cmd,(data>>8)&0xff,data&0xff,0,response,response_len)) {
		case GP_ERROR_TIMEOUT:
		case GP_ERROR_BAD_PARAMETERS:
			break;
		default:
			return ret;
		}
	    }
	}
	default:
	    return GP_ERROR_NOT_SUPPORTED;
	}
	return GP_ERROR_IO;
}

static int
stv0680_last_error(GPPort *port, struct stv680_error_info *errinf) {
	int ret;

	ret = stv0680_try_cmd(port, CMDID_CLEAR_COMMS_ERROR, 0,
		(void*)errinf, sizeof(*errinf));
	if (ret != GP_OK)
	    return ret;
	return GP_OK;
}

int stv0680_ping(GPPort *port)
{
	unsigned char pong[2];
	int ret;

	ret=stv0680_try_cmd(port, CMDID_PING, 0x55AA, pong, sizeof(pong));
	if (ret != GP_OK)
	    return ret;
	if ((pong[0]!=0x55) || (pong[1]!=0xAA)) {
	    printf("CMDID_PING successful, but returned bad values?\n");
	    return GP_ERROR_IO;
	}
	return GP_OK;
}

int stv0680_file_count(GPPort *port, int *count)
{
	struct stv680_image_info imginfo;
	int ret;

	ret = stv0680_try_cmd(port,CMDID_GET_IMAGE_INFO,0,
		(void*)&imginfo,sizeof(imginfo));
	if (ret != GP_OK)
	    return ret;
	*count = (imginfo.index[0]<<8)|imginfo.index[1];
	return GP_OK;
}

/**
 * Get image, with just bayer applied.
 */
int stv0680_get_image_raw(GPPort *port, int image_no, CameraFile *file)
{
	struct stv680_image_header imghdr;
	char header[80];
	unsigned char *raw, *data;
	int h,w,ret,size;

	ret = stv0680_try_cmd(port, CMDID_UPLOAD_IMAGE, image_no,
		(void*)&imghdr, sizeof(imghdr));
	if(ret != GP_OK)
	    return ret;

	w = (imghdr.width[0]  << 8) | imghdr.width[1];
	h = (imghdr.height[0] << 8) | imghdr.height[1];
	size = (imghdr.size[0] << 24) | (imghdr.size[1] << 16) |
	    (imghdr.size[2]<<8) | imghdr.size[3];
	raw = malloc(size);
	if (!raw)
	    return GP_ERROR_NO_MEMORY;
	if ((ret=gp_port_read(port, (char *)raw, size))<0) {
	    free (raw);
	    return ret;
	}

	sprintf(header, "P6\n# gPhoto2 stv0680 image\n%d %d\n255\n", w, h);
	gp_file_append(file, header, strlen(header));
	data = malloc(size * 3);
	if (!data) {
	    free (raw);
	    return GP_ERROR_NO_MEMORY;
	}
	gp_bayer_decode(raw,w,h,data,BAYER_TILE_GBRG_INTERLACED);
	free(raw);
	gp_file_append(file, (char *)data, size*3);
	free(data);
	return GP_OK;
}

int stv0680_get_image(GPPort *port, int image_no, CameraFile *file)
{
	struct stv680_image_header imghdr;
	char header[200];
	unsigned char buf[16];
	unsigned char *raw, *tmpdata1, *tmpdata2, *data;
	int h,w,ret,coarse,fine,size;

	/* Despite the documentation saying so, CMDID_UPLOAD_IMAGE does not
	 * return an image_header. The first 8 byte are correct, but the
	 * next 8 appear strange. So we call CMDID_GET_IMAGE_HEADER before.
	 */
	ret = stv0680_try_cmd(port, CMDID_GET_IMAGE_HEADER, image_no,
		(void*)&imghdr, sizeof(imghdr));
	if(ret != GP_OK)
		return ret;
	ret = stv0680_try_cmd(port, CMDID_UPLOAD_IMAGE, image_no,
		(void*)buf, sizeof(buf));
	if(ret != GP_OK)
		return ret;
	w = (imghdr.width[0]  << 8) | imghdr.width[1];
	h = (imghdr.height[0] << 8) | imghdr.height[1];
	size = (imghdr.size[0] << 24) | (imghdr.size[1] << 16) |
	    (imghdr.size[2]<<8) | imghdr.size[3];
	fine = (imghdr.fine_exposure[0]<<8)|imghdr.fine_exposure[1];
	coarse = (imghdr.coarse_exposure[0]<<8)|imghdr.coarse_exposure[1];
	raw = malloc(size);
	if (!raw)
	    return GP_ERROR_NO_MEMORY;
	sprintf(header, "P6\n# gPhoto2 stv0680 image\n#flags %x sgain %d sclkdiv %d avgpix %d fine %d coarse %d\n%d %d\n255\n", imghdr.flags, imghdr.sensor_gain, imghdr.sensor_clkdiv, imghdr.avg_pixel_value, fine, coarse , w, h);

	gp_file_append(file, header, strlen(header));
	if ((ret=gp_port_read(port, (char *)raw, size))<0) {
		free (raw);
		return ret;
	}

	data = malloc(size * 3);
	if (!data) {
		free (raw);
		return GP_ERROR_NO_MEMORY;
	}
	tmpdata1 = malloc(size * 3);
	if (!tmpdata1) {
		free (raw);
		free (data);
		return GP_ERROR_NO_MEMORY;
	}
	tmpdata2 = malloc(size * 3);
	if (!tmpdata2) {
		free (raw);
		free (data);
		free (tmpdata1);
		return GP_ERROR_NO_MEMORY;
	}
	gp_bayer_expand (raw, w, h, tmpdata1, BAYER_TILE_GBRG_INTERLACED);
	light_enhance(w,h,coarse,imghdr.avg_pixel_value,fine,tmpdata1);
	/* gp_bayer_interpolate (tmpdata1, w, h, BAYER_TILE_GBRG_INTERLACED); */
	stv680_hue_saturation (w, h, tmpdata1, tmpdata2 );
	demosaic_sharpen (w, h, tmpdata2, tmpdata1, 2, BAYER_TILE_GBRG_INTERLACED);
	sharpen (w, h, tmpdata1, data, 16);
	free(tmpdata2);
	free(tmpdata1);
	free(raw);
	gp_file_append(file, (char *)data, 3*size);
	free(data);
	return GP_OK;
}

int stv0680_get_image_preview(GPPort *port, int image_no, CameraFile *file)
{
	struct stv680_image_header imghdr;
	char header[64];
	unsigned char *raw, *data;
	int h,w,rh,rw,scale,rsize,size;
	int ret;

	switch (port->type) {
	case GP_PORT_USB:
		if ((ret = stv0680_try_cmd(port, CMDID_UPLOAD_IMAGE,
			image_no, (void*)&imghdr, sizeof(imghdr)) < 0)) {
			return ret;
		}
		rw = (imghdr.width[0]  << 8) | imghdr.width[1];
		rh = (imghdr.height[0] << 8) | imghdr.height[1];
		rsize = (imghdr.size[0] << 24) | (imghdr.size[1] << 16) |
		    (imghdr.size[2]<<8) | imghdr.size[3];
		scale = (rw>>8)+1;
		break;
	default:
	case GP_PORT_SERIAL:
		ret = stv0680_try_cmd(port, CMDID_UPLOAD_THUMBNAIL, image_no,
			(void*)&imghdr, sizeof(imghdr));
		if(ret != GP_OK)
			return ret;
		rw = (imghdr.width[0]  << 8) | imghdr.width[1];
		rh = (imghdr.height[0] << 8) | imghdr.height[1];
		rsize = (imghdr.size[0] << 24) | (imghdr.size[1] << 16) |
		    (imghdr.size[2]<<8) | imghdr.size[3];
		scale = 0;
		break;
	}
	raw = calloc(1, rsize);
	if (!raw) return GP_ERROR_NO_MEMORY;
	if ((ret=gp_port_read(port, (char *)raw, rsize))<0) {
		free(raw);
		return ret;
	}
	w = rw >> scale;
	h = rh >> scale;
	size = w * h;

	sprintf(header, "P6\n# gPhoto2 stv0680 image\n%d %d\n255\n", w, h);
	gp_file_append(file, header, strlen(header));
	data = calloc(1,size * 3);

	if (!scale)
	    gp_bayer_decode(raw, rw, rh, data, BAYER_TILE_GBRG_INTERLACED);
	else
	    bayer_unshuffle_preview(rw, rh, scale, raw, data);
	free(raw);
	gp_file_append(file, (char *)data, size*3);
	free(data);
	return GP_OK;
}

int stv0680_capture(GPPort *port)
{
	int ret;
	struct stv680_error_info errinf;

	ret = stv0680_try_cmd(port, CMDID_GRAB_IMAGE, GRAB_UPDATE_INDEX|GRAB_USE_CAMERA_INDEX, NULL, 0);
	if (ret!=GP_OK)
		return ret;
	/* wait until it is done */
	do {
	    ret = stv0680_last_error(port, &errinf);
	    if (ret != GP_OK)
		return ret;
	    if (errinf.error == CAMERR_BAD_EXPOSURE) {
		gp_port_set_error(port,_("Bad exposure (not enough light probably)"));
		return GP_ERROR;
	    }
	    if (errinf.error != CAMERR_BUSY)
		fprintf(stderr,"stv680_capture: error was %d.%d\n",errinf.error,errinf.info);
	} while (errinf.error == CAMERR_BUSY);
	return GP_OK;
}

#if 0
/* this somehow terminates after some images. timeouts due to low exposure?
 * I haven't found out yet. But this would be the right way to do it.
 */
int stv0680_capture_preview(GPPort *port, char **data, int *size)
{
	struct stv680_image_header imghdr;
	struct stv680_image_info imginfo;
	struct stv680_error_info errinf;
	char header[64];
	unsigned char *raw, *bayerpre;
	int fine,coarse,h,w,ret;

	ret = stv0680_last_error(port, &errinf);
	if (ret != GP_OK)
	    return ret;

	ret = stv0680_try_cmd(port, CMDID_GRAB_IMAGE, GRAB_USE_CAMERA_INDEX, NULL,0);
	if(ret != GP_OK)
		return ret;
	do {
		ret = stv0680_try_cmd(port, CMDID_CLEAR_COMMS_ERROR, 0, (void*)&errinf, sizeof(errinf));
		if (ret != GP_OK)
		    return ret;
		if (errinf.error == CAMERR_BAD_EXPOSURE) {
		    gp_port_set_error(port,_("Bad exposure (not enough light probably)"));
		    return GP_ERROR;
		}
		if (errinf.error != CAMERR_BUSY)
		    fprintf(stderr,"stv680_capture: error was %d.%d\n",errinf.error,errinf.info);
	} while (errinf.error == CAMERR_BUSY);

#if 0
	fprintf(stderr,"image flag %x\n",imghdr.flags);
	fprintf(stderr,"sensor gain %d\n",imghdr.sensor_gain);
	fprintf(stderr,"sensor clkdiv %d\n",imghdr.sensor_clkdiv);
	fprintf(stderr,"avg pixel value %d\n",imghdr.avg_pixel_value);
#endif
	ret = stv0680_try_cmd(port, CMDID_GET_IMAGE_INFO, 0, (void*)&imginfo, sizeof(imginfo));
	if (ret!=GP_OK)
	    return ret;
	ret = stv0680_try_cmd(port, CMDID_UPLOAD_IMAGE, (imginfo.index[0]<<8)|imginfo.index[1], (void*)&imghdr, sizeof(imghdr));
	if(ret != GP_OK)
		return ret;
	fine = (imghdr.fine_exposure[0]<<8)|imghdr.fine_exposure[1];
	coarse = (imghdr.coarse_exposure[0]<<8)|imghdr.coarse_exposure[1];
	w = (imghdr.width[0]  << 8) | imghdr.width[1];
	h = (imghdr.height[0] << 8) | imghdr.height[1];
	*size = (imghdr.size[0] << 24) | (imghdr.size[1] << 16) |
	    (imghdr.size[2]<<8) | imghdr.size[3];

	raw = malloc(*size);
	if ((ret=gp_port_read(port, raw, *size))<0)
	    return ret;

	sprintf(header, "P6\n# gPhoto2 stv0680 image\n%d %d\n255\n", w, h);
	*data = malloc(((*size) * 3) + strlen(header));
	strcpy(*data, header);
	bayerpre = malloc(((*size)*3));
	gp_bayer_expand (raw, w, h, bayerpre, BAYER_TILE_GBRG_INTERLACED);
	light_enhance(w,h,coarse,fine,imghdr.avg_pixel_value,bayerpre);
	/* gp_bayer_interpolate (bayerpre, w, h, BAYER_TILE_GBRG_INTERLACED); */
	demosaic_sharpen (w, h, bayerpre, *data + strlen(header), 2, BAYER_TILE_GBRG_INTERLACED);
	/* sharpen (w, h, bayerpre,*data + strlen(header), 20); */
	free(bayerpre);
	free(raw);
	*size *= 3;
	*size += strlen(header);
	return GP_OK;
}
#endif

int stv0680_capture_preview(GPPort *port, char **data, int *size)
{
	char header[64];
	struct stv680_camera_info caminfo;
	unsigned char *raw,*bayerpre;
	int xsize,h,w,i;
	int ret;
	struct capmode {
	    int mask,w,h,mode;
	} capmodes[4] = {
	    { 1, STV0680_CIF_WIDTH,  STV0680_CIF_HEIGHT,  0x000 },
	    { 2, STV0680_VGA_WIDTH,  STV0680_VGA_HEIGHT,  0x100 },
	    { 4, STV0680_QCIF_WIDTH, STV0680_QCIF_HEIGHT, 0x200 },
	    { 8, STV0680_QVGA_WIDTH, STV0680_QVGA_HEIGHT, 0x300 },
	};

	/* Get Camera Information */
	if ((ret = stv0680_try_cmd(port, CMDID_GET_CAMERA_INFO, 0,
				(void*)&caminfo, sizeof(caminfo)) < 0))
		return ret;

	/* serial cameras don't have video... Too slow. */
	if (!(caminfo.hardware_config & HWCONFIG_HAS_VIDEO))
	    return GP_ERROR_NOT_SUPPORTED;

	/* Look for the first supported mode, with decreasing size */
	for (i=0;i<4;i++)
	    if (caminfo.capabilities & capmodes[i].mask)
		break;
	if (i==4) {
	    fprintf(stderr,"Neither CIF, QCIF, QVGA nor VGA supported?\n");
	    return GP_ERROR;
	}
	w = capmodes[i].w;
	h = capmodes[i].h;

	xsize = (w+2)*(h+2);

	/* Send parameter according to mode */
	ret = stv0680_try_cmd(port,CMDID_START_VIDEO,capmodes[i].mode,NULL,0x0);

	if (ret!=GP_OK)
		return ret;

	*size= xsize;
	raw = malloc(*size);
	switch(gp_port_read(port, (char *)raw, *size)) {
	case GP_ERROR_TIMEOUT:
		printf("read timeout\n");
		break;
	case GP_ERROR:
		printf("IO error\n");
		break;
	default:break;
	}
	if ((ret = stv0680_try_cmd(port, CMDID_STOP_VIDEO, 0, NULL, 0)!=GP_OK)) {
		free (raw);
		return ret;
	}
	sprintf(header, "P6\n# gPhoto2 stv0680 image\n%d %d\n255\n", w, h);
	*data = malloc((*size * 3) + strlen(header));
	strcpy(*data, header);
	bayerpre = malloc(((*size)*3));
	/* no light enhancement here, we do not get the exposure values? */
	gp_bayer_decode (raw, w, h, bayerpre, BAYER_TILE_GBRG_INTERLACED);
	demosaic_sharpen (w, h, bayerpre, (unsigned char *)*data + strlen(header), 2, BAYER_TILE_GBRG_INTERLACED);
	/* sharpen (w, h, bayerpre,*data + strlen(header), 20); */
	free(raw);
	free(bayerpre);
	*size *= 3;
	*size += strlen(header);
	return GP_OK;
}


int stv0680_delete_all(GPPort *port) {
    return stv0680_try_cmd(port,CMDID_SET_IMAGE_INDEX,0,NULL,0);
}

int stv0680_summary(GPPort *port, char *txt)
{
    struct stv680_camera_info caminfo;
    struct stv680_image_info imginfo;
    int ret;

    strcpy(txt,_("Information on STV0680-based camera:\n"));
    /* Get Camera Information */
    if ((ret = stv0680_try_cmd(port, CMDID_GET_CAMERA_INFO,
				0, (void*)&caminfo, sizeof(caminfo)) < 0))
	return ret;
    sprintf(txt+strlen(txt),_("Firmware Revision: %d.%d\n"),
	    caminfo.firmware_revision[0],
	    caminfo.firmware_revision[1]
    );
    sprintf(txt+strlen(txt),_("ASIC Revision: %d.%d\n"),
	    caminfo.asic_revision[0],
	    caminfo.asic_revision[1]
    );
    sprintf(txt+strlen(txt),_("Sensor ID: %d.%d\n"),
	    caminfo.sensor_id[0],
	    caminfo.sensor_id[1]
    );
    /* HWCONFIG_COMMSLINK_MASK ... not really needed, the user knows, he
     * plugged it in. */
    sprintf(txt+strlen(txt),_("Camera is configured for lights flickering by %dHz.\n"),
    	(caminfo.hardware_config & HWCONFIG_FLICKERFREQ_60HZ)?60:50
    );
    sprintf(txt+strlen(txt),_("Memory in camera: %d Mbit.\n"),
    	(caminfo.hardware_config & HWCONFIG_MEMSIZE_16MBIT)?16:64
    );
    if (caminfo.hardware_config & HWCONFIG_HAS_THUMBNAILS)
	strcat(txt,_("Camera supports Thumbnails.\n"));
    if (caminfo.hardware_config & HWCONFIG_HAS_VIDEO)
	strcat(txt,_("Camera supports Video.\n"));
    /* HWCONFIG_STARTUP_COMPLETED ... Would the camera even answer if not ? */
    if (caminfo.hardware_config & HWCONFIG_IS_MONOCHROME)
	strcat(txt,_("Camera pictures are monochrome.\n"));
    if (caminfo.hardware_config & HWCONFIG_MEM_FITTED) /* Is this useful? */
	strcat(txt,_("Camera has memory.\n"));

    strcat(txt,_("Camera supports videoformats: "));
    if (caminfo.capabilities & CAP_CIF) strcat(txt,"CIF ");
    if (caminfo.capabilities & CAP_VGA) strcat(txt,"VGA ");
    if (caminfo.capabilities & CAP_QCIF) strcat(txt,"QCIF ");
    if (caminfo.capabilities & CAP_QVGA) strcat(txt,"QVGA ");
    strcat(txt,"\n");
    sprintf(txt+strlen(txt),_("Vendor ID: %02x%02x\n"),
	    caminfo.vendor_id[0],
	    caminfo.vendor_id[1]
    );
    sprintf(txt+strlen(txt),_("Product ID: %02x%02x\n"),
	    caminfo.product_id[0],
	    caminfo.product_id[1]
    );
    if ((ret = stv0680_try_cmd(port, CMDID_GET_IMAGE_INFO, 0,
		    (void*)&imginfo, sizeof(imginfo))!=GP_OK))
	return ret;
    sprintf(txt+strlen(txt),_("Number of Images: %d\n"),
	    (imginfo.index[0]<<8)|imginfo.index[1]
    );
    sprintf(txt+strlen(txt),_("Maximum number of Images: %d\n"),
	    (imginfo.maximages[0]<<8)|imginfo.maximages[1]
    );
    sprintf(txt+strlen(txt),_("Image width: %d\n"),
	    (imginfo.width[0]<<8)|imginfo.width[1]
    );
    sprintf(txt+strlen(txt),_("Image height: %d\n"),
	    (imginfo.height[0]<<8)|imginfo.height[1]
    );
    sprintf(txt+strlen(txt),_("Image size: %d\n"),
	    (imginfo.size[0]<<24)|(imginfo.size[1]<<16)|(imginfo.size[2]<<8)|
	     imginfo.size[3]
    );
    sprintf(txt+strlen(txt),_("Thumbnail width: %d\n"),imginfo.thumb_width);
    sprintf(txt+strlen(txt),_("Thumbnail height: %d\n"),imginfo.thumb_height);
    sprintf(txt+strlen(txt),_("Thumbnail size: %d\n"),
	    (imginfo.thumb_size[0]<<8)|imginfo.thumb_size[1]
    );
    return GP_OK;
}
