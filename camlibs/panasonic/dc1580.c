/*
	$Id$

	Copyright (c) 2000 Mariusz Zynel <mariusz@mizar.org> (gPhoto port)
	Copyright (C) 2000 Fredrik Roubert <roubert@df.lth.se> (idea)
	Copyright (C) 1999 Galen Brooks <galen@nine.com> (DC1580 code)

	This file is part of the gPhoto project and may only be used,  modified,
	and distributed under the terms of the gPhoto project license,  COPYING.
	By continuing to use, modify, or distribute  this file you indicate that
	you have read the license, understand and accept it fully.
	
	THIS  SOFTWARE IS PROVIDED AS IS AND COME WITH NO WARRANTY  OF ANY KIND,
	EITHER  EXPRESSED OR IMPLIED.  IN NO EVENT WILL THE COPYRIGHT  HOLDER BE
	LIABLE FOR ANY DAMAGES RESULTING FROM THE USE OF THIS SOFTWARE.

	Note:
	
	This is a Panasonic PV/NV-DC1580 camera gPhoto library source code.
	
	An algorithm  for  checksums  borrowed  from  Michael McCormack's  Nikon
	CoolPix 600 library for gPhoto1.
*/	
 
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include "dc.h"
#include "dc1580.h"

#ifndef __FILE__
#  define __FILE__ "dc1580.c"
#endif

/******************************************************************************/

/* Internal utility functions */

/* dsc2_checksum - establish checksum for size bytes in buffer */

u_int8_t dsc2_checksum(char *buffer, int size) {

	int	checksum = 0;
	int	i;
	
	for (i = 1; i < size - 2; i++) {
		checksum += buffer[i];
		checksum %= 0x100;
	}
	
	return checksum;
}

/* dsc2_sendcmd - send command with data to DSC */

int dsc2_sendcmd(dsc_t *dsc, u_int8_t cmd, long int data, u_int8_t sequence) {

	int	i;

	DEBUG_PRINT(("Sending command: 0x%02x, data: %i, sequence: %i.", cmd, data, sequence));

	memset(dsc->buf, 0, 16);

	dsc->buf[0] = 0x08;
	dsc->buf[1] = sequence;
	dsc->buf[2] = 0xff - sequence;
	dsc->buf[3] = cmd;
	
	for (i = 0; i < sizeof(data); i++) 
		dsc->buf[4 + i] = (u_int8_t)(data >> 8*i);
	
	dsc->buf[14] = dsc2_checksum(dsc->buf, 16); 
	
	return gpio_write(dsc->dev, dsc->buf, 16);
}

/* dsc2_retrcmd - retrieve command and its data from DSC */

int dsc2_retrcmd(dsc_t *dsc) {
	
	int	result = GP_ERROR; 
	int	s;
	
	if ((s = gpio_read(dsc->dev, dsc->buf, 16)) == GP_ERROR)
		return GP_ERROR;
	
	if (dsc->debug && 0 < s)
		dsc_dumpmem(dsc->buf, s);
	
	if (s != 16 || 	dsc->buf[DSC2_BUF_BASE] != 0x08 ||
			dsc->buf[DSC2_BUF_SEQ] != 0xff - (u_int8_t)dsc->buf[DSC2_BUF_SEQC]) {
		RETURN_ERROR(EDSCBADRSP, dsc2_retrcmd);
		/* bad response */
	}
	else
		result = dsc->buf[DSC2_BUF_CMD];

	DEBUG_PRINT(("Retrieved command: %i.", result));
	
	return result;
}

/* dsc2_connect - try hand shake with camera and establish connection */

int dsc2_connect(dsc_t *dsc, int speed) {
	
	DEBUG_PRINT(("Connecting camera with speed: %i.", speed));
		
	if (dsc1_setbaudrate(dsc, speed) != GP_OK)
		return GP_ERROR;
	
	if (dsc1_getmodel(dsc) != DSC2)
		RETURN_ERROR(EDSCBADDSC, dsc2_connect);
		/* bad camera model */
	
	if (dsc2_sendcmd(dsc, DSC2_CMD_CONNECT, 0, 0) != GP_OK)
		return GP_ERROR;
	
	if (dsc2_retrcmd(dsc) != DSC2_RSP_OK) 
		RETURN_ERROR(EDSCBADRSP, dsc2_connect);
		/* bad response */

	DEBUG_PRINT(("Camera connected successfully."));
	
	return GP_OK;	
}

/* dsc2_disconnect - reset camera, free buffers and close files */

int dsc2_disconnect(dsc_t *dsc) {
	
	DEBUG_PRINT(("Disconnecting the camera."));
	
	if (dsc2_sendcmd(dsc, DSC2_CMD_RESET, 0, 0) != GP_OK)
		return GP_ERROR;
	
	if (dsc2_retrcmd(dsc) != DSC2_RSP_OK) 
		RETURN_ERROR(EDSCBADRSP, dsc2_disconnect)
		/* bad response */
	else 
		sleep(DSC_PAUSE); /* let camera to redraw its screen */
	
	DEBUG_PRINT(("Camera disconnected."));
	
	return GP_OK;
}

/* dsc2_getindex - retrieve the number of images stored in camera memory */

int dsc2_getindex(dsc_t *dsc) {
	
	int	result = GP_ERROR;

	DEBUG_PRINT(("Retrieving the number of images."));
		
	if (dsc2_sendcmd(dsc, DSC2_CMD_GET_INDEX, 0, 0) != GP_OK)
		return GP_ERROR;
	
	if (dsc2_retrcmd(dsc) == DSC2_RSP_INDEX)
		result =
			((u_int32_t)dsc->buf[DSC2_BUF_DATA]) |
			((u_int8_t)dsc->buf[DSC2_BUF_DATA + 1] << 8) |
			((u_int8_t)dsc->buf[DSC2_BUF_DATA + 2] << 16) |
			((u_int8_t)dsc->buf[DSC2_BUF_DATA + 3] << 24);
	else
		RETURN_ERROR(EDSCBADRSP, dsc2_getindex);
		/* bad response */
	
	DEBUG_PRINT(("Number of images: %i", result));
	
	return result;
}

/* dsc2_delete - delete image #index from camera memory */

int dsc2_delete(dsc_t *dsc, int index) {
	
	DEBUG_PRINT(("Deleting image: %i.", index));
		
	if (index < 1)
		RETURN_ERROR(EDSCBADNUM, dsc2_delete);
		/* bad image number */

	if (dsc2_sendcmd(dsc, DSC2_CMD_DELETE, index, 0) != GP_OK)
		return GP_ERROR;

	if (dsc2_retrcmd(dsc) != DSC2_RSP_OK)
		RETURN_ERROR(EDSCBADRSP, dsc2_delete);
		/* bad response */

	DEBUG_PRINT(("Image: %i deleted.", index));
	return GP_OK;
}

/* dsc2_selectimage - select image to download, return its size */

int dsc2_selectimage(dsc_t *dsc, int index, int thumbnail)
{
	int	size = 0;
	
	DEBUG_PRINT(("Selecting image: %i, thumbnail: %i.", index, thumbnail));
	
	if (index < 1)
		RETURN_ERROR(EDSCBADNUM, dsc2_selectimage);
		/* bad image number */

	if (thumbnail == DSC_THUMBNAIL) {
		if (dsc2_sendcmd(dsc, DSC2_CMD_THUMB, index, 0) != GP_OK)
			return GP_ERROR;
	} else {
		if (dsc2_sendcmd(dsc, DSC2_CMD_SELECT, index, 0) != GP_OK)
			return GP_ERROR;
	}
	
	if (dsc2_retrcmd(dsc) != DSC2_RSP_IMGSIZE)
		RETURN_ERROR(EDSCBADRSP, dsc2_selectimage);
		/* bad response */
		
	size =	((u_int32_t)dsc->buf[DSC2_BUF_DATA]) |
		((u_int8_t)dsc->buf[DSC2_BUF_DATA + 1] << 8) |
		((u_int8_t)dsc->buf[DSC2_BUF_DATA + 2] << 16) |
		((u_int8_t)dsc->buf[DSC2_BUF_DATA + 3] << 24);
	
	DEBUG_PRINT(("Selected image: %i, thumbnail: %i, size: %i.", index, thumbnail, size));

	return size;
}

/* gpio_readimageblock - read #block block (1024 bytes) of an image into buf */

int dsc2_readimageblock(dsc_t *dsc, int block, char *buffer) {
	
	DEBUG_PRINT(("Reading image block: %i.", block));

	if (dsc2_sendcmd(dsc, DSC2_CMD_GET_DATA, block, block) != GP_OK)
		return GP_ERROR;

	if (gpio_read(dsc->dev, dsc->buf, DSC_BUFSIZE) != DSC_BUFSIZE)
		RETURN_ERROR(EDSCBADRSP, dsc2_readimageblock);
		/* bad response */

	if ((u_int8_t)dsc->buf[0] != 1 ||
			(u_int8_t)dsc->buf[1] != block ||
			(u_int8_t)dsc->buf[2] != 0xff - block ||
			(u_int8_t)dsc->buf[3] != DSC2_RSP_DATA ||
			(u_int8_t)dsc->buf[DSC_BUFSIZE - 2] != dsc2_checksum(dsc->buf, DSC_BUFSIZE))
		RETURN_ERROR(EDSCBADRSP, dsc2_readimageblock);
		/* bad response */
	
	if (buffer)
		memcpy(buffer, &dsc->buf[4], DSC_BLOCKSIZE);
	
	DEBUG_PRINT(("Block: %i read in.", block));
	
	return DSC_BLOCKSIZE;
}

/* dsc2_readimage - read #index image or thumbnail and return its contents */

char *dsc2_readimage(dsc_t *dsc, int index, int thumbnail, int *size) {

	char	kind[16];
	int	blocks, i;
	char	*buffer = NULL;

	DEBUG_PRINT(("Reading image: %i, thumbnail: %i.", index, thumbnail));
		
	if ((*size = dsc2_selectimage(dsc, index, thumbnail)) < 0)
		return NULL;
	
	if (thumbnail == DSC_THUMBNAIL)
		strcpy(kind, "thumbnail");
	else
		strcpy(kind, "image");
	
	if (!(buffer = (char*)malloc(*size))) {
		DEBUG_PRINT(("Failed to allocate memory for %s data.", kind));
		return NULL;
	}
	
	blocks = (*size - 1)/DSC_BLOCKSIZE + 1;
			
	for (i = 0; i < blocks; i++) {
		if (dsc2_readimageblock(dsc, i, &buffer[i*DSC_BLOCKSIZE]) == GP_ERROR) {
			DEBUG_PRINT(("Error during %s transfer.", kind));
			free(buffer);
			return NULL;
		}
	}
	
	DEBUG_PRINT(("Image: %i read in.", index));
	
	return buffer;
}

/* dsc2_setimagesize - set size of an image to upload to camera */

int dsc2_setimagesize(dsc_t *dsc, int size) {
	
	DEBUG_PRINT(("Setting image size to: %i.", size));	

	if (dsc2_sendcmd(dsc, DSC2_CMD_SET_SIZE, size, 0) != GP_OK)
		return GP_ERROR;

	if (dsc2_retrcmd(dsc) != DSC2_RSP_OK)
		RETURN_ERROR(EDSCBADRSP, dsc2_setimagesize);
		/* bad response */
		
	DEBUG_PRINT(("Image size set to: %i.", size));

	return GP_OK;
}

/* gpio_writeimageblock - write size bytes from buffer rounded to 1024 bytes to camera */

int dsc2_writeimageblock(dsc_t *dsc, int block, char *buffer, int size) {

	DEBUG_PRINT(("Writing image block: %i.", block));

	memset(dsc->buf, 0, DSC_BUFSIZE);

	dsc->buf[0] = 0x01;
	dsc->buf[1] = block;
	dsc->buf[2] = 0xff - block;
	dsc->buf[3] = DSC2_CMD_SEND_DATA;
	
	if (DSC_BLOCKSIZE < size)
		size = DSC_BLOCKSIZE;

	memcpy(&dsc->buf[4], buffer, size);
	
	dsc->buf[DSC_BUFSIZE - 2] = dsc2_checksum(dsc->buf, DSC_BUFSIZE);

	if (gpio_write(dsc->dev, dsc->buf, DSC_BUFSIZE) != GP_OK)
		return GP_ERROR;
	
	if (dsc2_retrcmd(dsc) != DSC2_RSP_OK)
		RETURN_ERROR(EDSCBADRSP, dsc2_writeimageblock);
		/* bad response */
		
	DEBUG_PRINT(("Block: %i of size: %i written.", block, size));
	
	return GP_OK;
}

/* dsc2_writeimage - write an image to camera memory, size bytes at buffer */

int dsc2_writeimage(dsc_t *dsc, char *buffer, int size)
{
	int	blocks, blocksize, i;

	DEBUG_PRINT(("Writing an image of size: %i.", size));
		
	if ((dsc2_setimagesize(dsc, size)) != GP_OK)
		return GP_ERROR;
	
	blocks = (size - 1)/DSC_BLOCKSIZE + 1;
			
	for (i = 0; i < blocks; i++) {
		blocksize = size - i*DSC_BLOCKSIZE;
		if (DSC_BLOCKSIZE < blocksize) 
			blocksize = DSC_BLOCKSIZE;
		if (dsc2_writeimageblock(dsc, i, &buffer[i*DSC_BLOCKSIZE], blocksize) != GP_OK) {
			DEBUG_PRINT(("Error during image transfer."));
			return GP_ERROR;
		}
	}

	DEBUG_PRINT(("Image written successfully."));
	
	return GP_OK;
}

/* dsc2_preview - show selected image on camera's LCD  */

int dsc2_preview(dsc_t *dsc, int index)
{
	if (index < 1)
		RETURN_ERROR(EDSCBADNUM, dsc2_preview);
		/* bad image number */

	if (dsc2_sendcmd(dsc, DSC2_CMD_PREVIEW, index, 0) != GP_OK)
		return GP_ERROR;
	
	if (dsc2_retrcmd(dsc) != DSC2_RSP_OK)
		RETURN_ERROR(EDSCBADRSP, dsc2_preview);
		/* bad response */
	
	return GP_OK;
}

/******************************************************************************/

/* Library interface functions */

int camera_id (CameraText *id) {

	strcpy(id->text, "panasonic-dc1580");

	return (GP_OK);
}

int camera_abilities (CameraAbilities *abilities, int *count) {

	*count = 2;

	/* Fill in each camera model's abilities */
	/* Make separate entries for each conneciton type (usb, serial, etc...)
	   if a camera supported multiple ways. */

	strcpy(abilities[0].model, "Panasonic DC1580");
	abilities[0].port     = GP_PORT_SERIAL;
	abilities[0].speed[0] = 9600;
	abilities[0].speed[1] = 19200;
	abilities[0].speed[2] = 38400;
	abilities[0].speed[3] = 57600;			
	abilities[0].speed[4] = 115200;	
	abilities[0].speed[5] = 0;	
	abilities[0].capture  = 0;
	abilities[0].config   = 0;
	abilities[0].file_delete  = 1;
	abilities[0].file_preview = 1;
	abilities[0].file_put 	  = 1;

	strcpy(abilities[1].model, "Nikon CoolPix 600");
	abilities[1].port     = GP_PORT_SERIAL;
	abilities[1].speed[1] = 9600;
	abilities[1].speed[1] = 19200;
	abilities[1].speed[2] = 38400;
	abilities[1].speed[3] = 57600;			
	abilities[1].speed[4] = 115200;	
	abilities[1].speed[5] = 0;	
	abilities[1].capture  = 0;
	abilities[1].config   = 0;
	abilities[1].file_delete  = 1;
	abilities[1].file_preview = 1;
	abilities[1].file_put     = 1;
	
	return (GP_OK);
}

int camera_init (Camera *camera, CameraInit *init) {
	
	dsc_t		*dsc = NULL;
	
	dsc_print_status(camera, "Initializing camera %s", init->model);	

	/* First, set up all the function pointers */
	camera->functions->id 			= camera_id;
	camera->functions->abilities 		= camera_abilities;
	camera->functions->init 		= camera_init;
	camera->functions->exit 		= camera_exit;
	camera->functions->folder_list  	= camera_folder_list;
	camera->functions->file_list 		= camera_file_list;
	camera->functions->file_get 		= camera_file_get;
	camera->functions->file_get_preview 	= camera_file_get_preview;
	camera->functions->file_put 		= camera_file_put;
	camera->functions->file_delete 		= camera_file_delete;
	camera->functions->config_get   	= camera_config_get;
	camera->functions->config_set   	= camera_config_set;
	camera->functions->capture 		= camera_capture;
	camera->functions->summary		= camera_summary;
	camera->functions->manual 		= camera_manual;
	camera->functions->about 		= camera_about;

	if (dsc && dsc->dev) {
		gpio_close(dsc->dev);
		gpio_free(dsc->dev);
	}
	free(dsc);

	/* first of all allocate memory for a dsc struct */
	if ((dsc = (dsc_t*)malloc(sizeof(dsc_t))) == NULL) { 
		if (camera->debug)
			dsc_errorprint(EDSCSERRNO, __FILE__, "camera_init", __LINE__);
		return GP_ERROR; 
	} 
	
	camera->camlib_data = dsc;
	
	dsc->debug = camera->debug;

	dsc->dev = gpio_new(GPIO_DEVICE_SERIAL);
	
	gpio_set_timeout(dsc->dev, 5000);
	strcpy(dsc->settings.serial.port, init->port_settings.path);
	dsc->settings.serial.speed 	= 9600; /* hand shake speed */
	dsc->settings.serial.bits	= 8;
	dsc->settings.serial.parity	= 0;
	dsc->settings.serial.stopbits	= 1;

	gpio_set_settings(dsc->dev, dsc->settings);

	gpio_open(dsc->dev);
	
	/* allocate memory for a dsc read/write buffer */
	if ((dsc->buf = (char *)malloc(sizeof(char)*(DSC_BUFSIZE))) == NULL) {
		if (camera->debug)
			dsc_errorprint(EDSCSERRNO, __FILE__, "camera_init", __LINE__);
		free(dsc);
		return GP_ERROR;
	}
	
	/* allocate memory for camera filesystem struct */
	if ((dsc->fs = gp_filesystem_new()) == NULL) {
		if (camera->debug)
			dsc_errorprint(EDSCSERRNO, __FILE__, "camera_init", __LINE__);
		free(dsc);
		return GP_ERROR;
	}
	
	return dsc2_connect(dsc, init->port_settings.speed); 
		/* connect with selected speed */
}

int camera_exit (Camera *camera) {
	
	dsc_t	*dsc = (dsc_t *)camera->camlib_data;
	
	dsc_print_status(camera, "Disconnecting camera.");
	
	dsc2_disconnect(dsc);
	
	if (dsc->dev) {
		gpio_close(dsc->dev);
		gpio_free(dsc->dev);
	}
	if (dsc->fs)
		gp_filesystem_free(dsc->fs);
	
	free(dsc);
	
	return (GP_OK);
}

int camera_folder_list (Camera *camera, CameraList *list, char *folder) {

	return GP_OK; 	/* folders are unsupported but it is OK */
}

int camera_file_list (Camera *camera, CameraList *list, char *folder) {

	dsc_t	*dsc = (dsc_t *)camera->camlib_data;
	int 	count, i;
	
	if ((count = dsc2_getindex(dsc)) == GP_ERROR)
		return GP_ERROR;
	
	gp_filesystem_populate(dsc->fs, "/", DSC_FILENAMEFMT, count);

	for (i = 0; i < count; i++) 
		gp_list_append(list, gp_filesystem_name(dsc->fs, "/", i), GP_LIST_FILE);

	return GP_OK;
}

int camera_file_get_common (Camera *camera, CameraFile *file, char *filename, int thumbnail) {
		
	dsc_t	*dsc = (dsc_t *)camera->camlib_data;
	int	index, i, size, blocks;
	char	kind[16];

	if (thumbnail == DSC_THUMBNAIL)
		strcpy(kind, "thumbnail");
	else
		strcpy(kind, "image");

	dsc_print_status(camera, "Downloading %s %s.", kind, filename);

	/* index is the 0-based image number on the camera */
	if ((index = gp_filesystem_number(dsc->fs, "/", filename)) == GP_ERROR)
		return GP_ERROR;
	
	if ((size = dsc2_selectimage(dsc, index + 1, thumbnail)) < 0)
		return (GP_ERROR);
	
	strcpy(file->name, filename);
	strcpy(file->type, "image/jpg");

	gp_camera_progress(camera, file, 0.00);

	blocks = (size - 1)/DSC_BLOCKSIZE + 1;
			
	for (i = 0; i < blocks; i++) {
		if (dsc2_readimageblock(dsc, i, NULL) == GP_ERROR) 
			return (GP_ERROR);
		gp_file_append(file, &dsc->buf[4], DSC_BLOCKSIZE);
		gp_camera_progress(camera, file, (float)(i+1)/(float)blocks*100.0);
	}

	return (GP_OK);
}
int camera_file_get (Camera *camera, CameraFile *file, char *folder, char *filename) {

	return camera_file_get_common(camera, file, filename, DSC_FULLIMAGE);
}

int camera_file_get_preview (Camera *camera, CameraFile *file, char *folder, char *filename) {

	return camera_file_get_common(camera, file, filename, DSC_THUMBNAIL);
}

int camera_file_put (Camera *camera, CameraFile *file, char *folder) {

	dsc_t	*dsc = (dsc_t *)camera->camlib_data;
	int	blocks, blocksize, i;
		
	dsc_print_status(camera, "Uploading image: %s.", file->name);	
	
/*	We can not figure out file type, at least by now.

	if (strcmp(file->type, "image/jpg") != 0) {
		dsc_print_message(camera, "JPEG image format allowed only.");
		return GP_ERROR;		
	}
*/	
	if (file->size > DSC_MAXIMAGESIZE) {
		dsc_print_message(camera, "File size is %i bytes. The size of the largest file possible to upload is: %i bytes.", file->size, DSC_MAXIMAGESIZE);
		return GP_ERROR;		
	}

	if ((dsc2_setimagesize(dsc, file->size)) != GP_OK)
		return GP_ERROR;
	
	gp_camera_progress(camera, file, 0.00);
	
	blocks = (file->size - 1)/DSC_BLOCKSIZE + 1;
			
	for (i = 0; i < blocks; i++) {
		blocksize = file->size - i*DSC_BLOCKSIZE;
		if (DSC_BLOCKSIZE < blocksize) 
			blocksize = DSC_BLOCKSIZE;
		if (dsc2_writeimageblock(dsc, i, &file->data[i*DSC_BLOCKSIZE], blocksize) != GP_OK) {
			return GP_ERROR;
		}
		gp_camera_progress(camera, file, (float)(i+1)/(float)blocks*100.0);
	}

	return GP_OK;
}

int camera_file_delete (Camera *camera, char *folder, char *filename) {

	dsc_t	*dsc = (dsc_t *)camera->camlib_data;
	int	index;

	dsc_print_status(camera, "Deleting image %s.", filename);

	/* index is the 0-based image number on the camera */
	if ((index = gp_filesystem_number(dsc->fs, folder, filename)) == GP_ERROR)
		return GP_ERROR;

	return dsc2_delete(dsc, index + 1);
}

int camera_config_get (Camera *camera, CameraWidget *window) {

        return GP_ERROR;
}

int camera_config_set (Camera *camera, CameraSetting *setting, int count) {

	return (GP_ERROR);
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	return (GP_ERROR);
}

int camera_summary (Camera *camera, CameraText *summary) {

	strcpy(summary->text, "Summary not available.");

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) {

	strcpy(manual->text, "Manual not available.");

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

	strcpy(about->text,
			"Panasonic DC1580 gPhoto library\n"
			"Mariusz Zynel <mariusz@mizar.org>\n\n"
			"Based on dc1000 program written by\n"
			"Fredrik Roubert <roubert@df.lth.se> and\n"
			"Galen Brooks <galen@nine.com>.");
	return (GP_OK);
}

/* End of dc1580.c */
