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
	
	This is a Panasonic DC1580 camera gPhoto library source code.
	
	An algorithm  for  checksums  borrowed  from  Michael McCormack's  Nikon
	CoolPix 600 library for gPhoto1.
*/	
 
#include <stdlib.h>
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

/* dsc_checksum - establish checksum for size bytes in buffer */

u_int8_t dsc_checksum(char *buffer, int size) {

	int	checksum = 0;
	int	i;
	
	for (i = 1; i < size - 2; i++) {
		checksum += buffer[i];
		checksum %= 0x100;
	}
	
	return checksum;
}

/* dsc_sendcmd - send command with data to DSC */

int dsc_sendcmd(dsc_t *dsc, u_int8_t cmd, long int data, u_int8_t sequence) {

	int	i;

	DBUG_PRINT_3("Sending command: 0x%02x, data: %i, sequence: %i", cmd, data, sequence);

	memset(dsc->buf, 0, 16);

	dsc->buf[0] = 0x08;
	dsc->buf[1] = sequence;
	dsc->buf[2] = 0xff - sequence;
	dsc->buf[3] = cmd;
	
	for (i = 0; i < sizeof(data); i++) 
		dsc->buf[4 + i] = (u_int8_t)(data >> 8*i);
	
	dsc->buf[14] = dsc_checksum(dsc->buf, 16); 
	
	return gpio_write(dsc->dev, dsc->buf, 16);
}

/* dsc_retrcmd - retrieve command and its data from DSC */

int dsc_retrcmd(dsc_t *dsc) {
	
	int	result = GP_ERROR; 
	int	s;
	
	s = gpio_read(dsc->dev, dsc->buf, 16);
	if (glob_debug)
		dsc_dumpmem(dsc->buf, s);
	if (s != 16 || 	dsc->buf[DSC_BUF_BASE] != 0x08 ||
			dsc->buf[DSC_BUF_SEQ] != 0xff - (u_int8_t)dsc->buf[DSC_BUF_SEQC]) {
		DBUG_RETURN(EDSCBADRSP, dsc_retrcmd, GP_ERROR); 
		/* bad response */
	}
	else
		result = dsc->buf[DSC_BUF_CMD];

	DBUG_PRINT_1("Retrieved command: %d", result);
	
	return result;
}

/* dsc_connect - try hand shake with camera and establish connection */

int dsc_connect(dsc_t *dsc, int speed) {
	
	DBUG_PRINT("Connecting a camera.");
		
	if (dsc_handshake(dsc, speed) != GP_OK)
		return GP_ERROR;
	
	dsc_sendcmd(dsc, DSC_CMD_CONNECT, 0, 0);
	
	if (dsc_retrcmd(dsc) != DSC_RSP_OK) 
		DBUG_RETURN(EDSCBADRSP, dsc_connect, GP_ERROR);
		/* bad response */

	DBUG_PRINT("Camera connected successfully");
	
	return GP_OK;	
}

/* dsc_disconnect - reset camera, free buffers and close files */

int dsc_disconnect(dsc_t *dsc) {
	DBUG_PRINT("Disconnecting the camera.");
	
	dsc_sendcmd(dsc, DSC_CMD_RESET, 0, 0);
	
	if (dsc_retrcmd(dsc) != DSC_RSP_OK) 
		DBUG_RETURN(EDSCNRESET, dsc_disconnect, GP_ERROR)
		/* could not reset camera */	
	else 
		sleep(DSC_PAUSE); /* let camera to redraw its screen */
	
	DBUG_PRINT("Camera disconnected.");
	
	return GP_OK;
}

/* dsc_getindex - retrieve the number of images stored in camera memory */

int dsc_getindex(dsc_t *dsc) {
	
	int	result = GP_ERROR;

	DBUG_PRINT("Retrieving the index.");
		
	dsc_sendcmd(dsc, DSC_CMD_GET_INDEX, 0, 0);
	
	if (dsc_retrcmd(dsc) == DSC_RSP_INDEX)
		result = dsc->buf[DSC_BUF_DATA];
	else
		DBUG_RETURN(EDSCBADRSP, dsc_getindex, GP_ERROR);
		/* bad response */
	
	DBUG_PRINT_1("Index retrieved, number of images: %d", result);
	
	return result;
}

/* dsc_delete - delete image #index from camera memory */

int dsc_delete(dsc_t *dsc, int index) {
	
	int 	s;
	char	str[80];
	
	DBUG_PRINT_1("Deleting the image of index: %d", index);	
		
	if (index < 1)
		DBUG_RETURN(EDSCBADNUM, dsc_delete, GP_ERROR);
		/* bad image number */

	sprintf(str, "Deleting image #%i.", index);
	gp_status(str);	
	
	dsc_sendcmd(dsc, DSC_CMD_DELETE, index, 0);

	if (dsc_retrcmd(dsc) != DSC_RSP_OK)
		DBUG_RETURN(EDSCBADRSP, dsc_delete, GP_ERROR);
		/* bad response */

	DBUG_PRINT_1("Image index: %d deleted.", index);	
	
	return GP_OK;
}

/* dsc_selectimage - select image to download, return its size */

int dsc_selectimage(dsc_t *dsc, int index, int thumbnail)
{
	int	size = 0;
	
	DBUG_PRINT_2("Selecting the image of index: %i, thumbnail: %i", index, thumbnail);	
	
	if (index < 1)
		DBUG_RETURN(EDSCBADNUM, dsc_selectimage, GP_ERROR);
		/* bad image number */

	if (thumbnail == DSC_THUMBNAIL)
		dsc_sendcmd(dsc, DSC_CMD_THUMB, index, 0);
	else
		dsc_sendcmd(dsc, DSC_CMD_SELECT, index, 0);

	if (dsc_retrcmd(dsc) != DSC_RSP_IMGSIZE)
		DBUG_RETURN(EDSCBADNUM, dsc_selectimage, GP_ERROR);
		/* bad image number */
	
	size =	(u_int32_t)0 |
		((u_int8_t)dsc->buf[6] << 16) |
		((u_int8_t)dsc->buf[5] << 8);
	
	DBUG_PRINT_3("Selected image index: %i, thumbnail: %i, size: %i", index, thumbnail, size);

	return size;
}

/* gpio_readimageblock - read #block block (1024 bytes) of an image into buf */

int dsc_readimageblock(dsc_t *dsc, int block, char *buffer) {
	
	DBUG_PRINT_1("Reading block: %i", block);

	dsc_sendcmd(dsc, DSC_CMD_GET_DATA, block, block);

	if (gpio_read(dsc->dev, dsc->buf, DSC_BUFSIZE) != DSC_BUFSIZE)
		DBUG_RETURN(EDSCNOANSW, dsc_readimageblock, GP_ERROR);
		/* no answer from camera */
	
	if ((u_int8_t)dsc->buf[0] != 1 ||
			(u_int8_t)dsc->buf[1] != block ||
			(u_int8_t)dsc->buf[2] != 0xff - block ||
			(u_int8_t)dsc->buf[3] != DSC_RSP_DATA ||
			(u_int8_t)dsc->buf[DSC_BUFSIZE - 2] != dsc_checksum(dsc->buf, DSC_BUFSIZE))
		DBUG_RETURN(EDSCBADRSP, dsc_readimageblock, GP_ERROR);
		/* bad response */
	
	memcpy(buffer, &dsc->buf[4], DSC_BLOCKSIZE);
	
	DBUG_PRINT_1("Block: %d read in.", block);
	
	return DSC_BLOCKSIZE;
}

/* dsc_readimage - read #index image or thumbnail and return its contents */

char *dsc_readimage(dsc_t *dsc, int index, int thumbnail, int *size) {

	char	kind[16];
	int	blocks, i;
	char 	str[80];
	char	*buffer = NULL;

	DBUG_PRINT_2("Reading the image of index: %i, thumbnail: %i", index, thumbnail);
		
	if ((*size = dsc_selectimage(dsc, index, thumbnail)) < 0)
		return NULL;
	
	if (thumbnail == DSC_THUMBNAIL)
		strcpy(kind, "thumbnail");
	else
		strcpy(kind, "image");
	
	sprintf(str, "Downloading %s #%i of size: %d bytes.",
			kind, index, *size);
	gp_status(str);
	gp_progress(0.00);

	if (!(buffer = (char*)malloc(*size))) {
		DBUG_PRINT_1("Failed to allocate memory for %s data.", kind);
		sprintf(str, "Failed to allocate memory for %s data.", kind);
		gp_message(str);
		return NULL;
	}
	
	blocks = (*size - 1)/DSC_BLOCKSIZE + 1;
			
	for (i = 0; i < blocks; i++) {
		if (dsc_readimageblock(dsc, i, &buffer[i*DSC_BLOCKSIZE]) == GP_ERROR) {
			DBUG_PRINT_1("Error during %s transfer.", kind);
			sprintf(str, "Error during %s transfer.", kind);
			gp_message(str);
			free(buffer);
			return NULL;
		}
		gp_progress((float)(i+1)/(float)blocks);
	}

	return buffer;
}

/* dsc_setimagesize - set size of an image to upload to camera */

int dsc_setimagesize(dsc_t *dsc, int size) {
	
	DBUG_PRINT_1("Setting image size to: %i", size);	

	dsc_sendcmd(dsc, DSC_CMD_SET_SIZE, size, 0);

	if (dsc_retrcmd(dsc) != DSC_RSP_OK)
		DBUG_RETURN(EDSCBADRSP, dsc_setimagesize, GP_ERROR);
		/* bad response */
		
	DBUG_PRINT_1("Image size set to: %i", size);

	return GP_OK;
}

/* gpio_writeimageblock - write size bytes from buffer rounded to 1024 bytes to camera */

int dsc_writeimageblock(dsc_t *dsc, int block, char *buffer, int size) {

	DBUG_PRINT_1("Writing block: %i", block);

	memset(dsc->buf, 0, DSC_BUFSIZE);

	dsc->buf[0] = 0x01;
	dsc->buf[1] = block;
	dsc->buf[2] = 0xff - block;
	dsc->buf[3] = DSC_CMD_SEND_DATA;
	
	if (DSC_BLOCKSIZE < size)
		size = DSC_BLOCKSIZE;

	memcpy(&dsc->buf[4], buffer, size);
	
	dsc->buf[DSC_BUFSIZE - 2] = dsc_checksum(dsc->buf, DSC_BUFSIZE);

	if (gpio_write(dsc->dev, dsc->buf, DSC_BUFSIZE) != GP_OK)
		DBUG_RETURN(EDSCSERRNO, dsc_writeimageblock, GP_ERROR);
		/* see errno value */
	
	if (dsc_retrcmd(dsc) != DSC_RSP_OK)
		DBUG_RETURN(EDSCNOANSW, dsc_writeimageblock, GP_ERROR);
		/* no answer from camera */
		
	DBUG_PRINT_2("Block: %i of size: %i written.", block, size);
	
	return GP_OK;
}

/* gpio_writeimage - write an image to camera memory, size bytes at buffer */

int dsc_writeimage(dsc_t *dsc, char *buffer, int size)
{
	int	blocks, blocksize, i;

	DBUG_PRINT_1("Writing an image of size: %i", size);
		
	if ((dsc_setimagesize(dsc, size)) != GP_OK)
		return GP_ERROR;
	
	gp_progress(0.00);

	blocks = (size - 1)/DSC_BLOCKSIZE + 1;
			
	for (i = 0; i < blocks; i++) {
		blocksize = size - i*DSC_BLOCKSIZE;
		if (DSC_BLOCKSIZE < blocksize) 
			blocksize = DSC_BLOCKSIZE;
		if (dsc_writeimageblock(dsc, i, &buffer[i*DSC_BLOCKSIZE], blocksize) != GP_OK) {
			DBUG_PRINT("Error during image transfer.");
			gp_message("Error during image transfer.");
			return GP_ERROR;
		}
		gp_progress((float)(i+1)/(float)blocks);
	}

	return GP_OK;
}

/* dsc_preview - show selected image on camera's LCD - is it supported? */

int dsc_preview(dsc_t *dsc, int index)
{
	if (index < 1)
		DBUG_RETURN(EDSCBADNUM, dsc_preview, GP_ERROR);
		/* bad image number */

	dsc_sendcmd(dsc, DSC_CMD_PREVIEW, index, 0);
	
	if (dsc_retrcmd(dsc) != DSC_RSP_OK)
		DBUG_RETURN(EDSCNOANSW, dsc_preview, GP_ERROR);
		/* no answer from camera */
	
	return GP_OK;
}

/******************************************************************************/

/* Library interface functions */

int camera_id (char *id) {

	strcpy(id, "panasonic-dc1580");

	return (GP_OK);
}

int camera_debug_set (int onoff) {

	glob_debug = onoff;
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
	abilities[0].file_put = 1;

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
	abilities[1].file_put = 1;
	
	return (GP_OK);
}

int camera_init (CameraInit *init) {
	
	dsc_error	dscerror;

	DBUG_PRINT("Initializing Panasonic DC1580 compatible camera");
	gp_status("Initializing camera.");

	if (dsc && dsc->dev) {		
		gpio_close(dsc->dev);
		gpio_free(dsc->dev);
	}

	/* first of all allocate memory for a dsc struct */
	if ((dsc = (dsc_t*)malloc(sizeof(dsc_t))) == NULL) { 
		dscerror.lerror = EDSCSERRNO; 
 		dscerror.lerrno = errno; 
		DBUG_PRINT_ERROR(dscerror, camera_init); 
		return GP_ERROR; 
	} 
	
	dsc->dev = gpio_new(GPIO_DEVICE_SERIAL);
	
	gpio_set_timeout(dsc->dev, 5000);
	strcpy(dsc->settings.serial.port, init->port_settings.path);
	dsc->settings.serial.speed 	= 9600; /* hand shake speed */
	dsc->settings.serial.bits	= 8;
	dsc->settings.serial.parity	= 0;
	dsc->settings.serial.stopbits	= 1;

	gpio_set_settings(dsc->dev, dsc->settings);

	gpio_open(dsc->dev);
	
	strcpy(dsc->model, init->model);
	
	/* allocate memory for a dsc read/write buffer */
	if ((dsc->buf = (char *)malloc(sizeof(char)*(DSC_BUFSIZE))) == NULL) {
		dscerror.lerror = EDSCSERRNO;
		dscerror.lerrno = errno;
		DBUG_PRINT_ERROR(dscerror, camera_init);
		free(dsc);
		return GP_ERROR;
	}
	
	return dsc_connect(dsc, init->port_settings.speed); 
		/* connect with selected speed */
}

int camera_exit () {
	
	gp_status("Disconnecting camera.");
	
	dsc_disconnect(dsc);
	
	if (dsc->dev) {
		gpio_close(dsc->dev);
		gpio_free(dsc->dev);
	}

	return (GP_OK);
}

int camera_folder_list(char *folder_name, CameraFolderInfo *list) {

	strcpy(list[0].name, "<photos>");
	
	return (GP_OK);
}

int camera_folder_set(char *folder_name) {

	return (GP_OK);
}

int camera_file_count () {

	return dsc_getindex(dsc);
}

int camera_file_get (int file_number, CameraFile *file) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/
	
	int		size;

	DBUG_PRINT_1("Retrieving an image %i from camera", file_number + 1);	

	sprintf(file->name, "dsc%04i.jpg", file_number + 1);
	strcpy(file->type, "image/jpg");
	
	if (!(file->data = dsc_readimage(dsc, file_number + 1, DSC_FULLIMAGE, &size)))
		return GP_ERROR;

	file->size = size;

	return (GP_OK);
}

int camera_file_get_preview (int file_number, CameraFile *preview) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/
	
	int		size;	

	DBUG_PRINT_1("Retrieving a thumbnail %d from camera", file_number + 1);
	
	sprintf(preview->name, "dsc%04i-thumbnail.jpg", file_number + 1);	
	strcpy(preview->type, "image/jpg");
	
	if (!(preview->data = dsc_readimage(dsc, file_number + 1, DSC_THUMBNAIL, &size)))
		return GP_ERROR;

	preview->size = size;

	return (GP_OK);
}

int camera_file_put (CameraFile *file) {

	char	str[80];
	
	DBUG_PRINT_2("Uploading image: %s of size: %d bytes", file->name, file->size);
	
/*	We can not figure out file type, at least by now.

	if (strcmp(file->type, "image/jpg") != 0) {
		DBUG_PRINT("JPEG image format allowed only.");
		sprintf(str, "JPEG image format allowed only.");
		gp_message(str);
		return GP_ERROR;		
	}
*/	
	if (file->size > DSC_MAXIMAGESIZE) {
		DBUG_PRINT_1("File size is %i. Too big to fit in the camera memory.", file->size);
		sprintf(str, "File size is %i. Too big to fit in the camera memory.", file->size);
		gp_message(str);
		return GP_ERROR;		
	}

	sprintf(str, "Uploading image: %s of size: %d bytes.", file->name, file->size);
	gp_status(str);
		
	return dsc_writeimage(dsc, file->data, file->size);
}

int camera_file_delete (int file_number) {

	return dsc_delete(dsc, file_number + 1);
}

int camera_config_get (CameraWidget *window) {

        return GP_ERROR;
}

int camera_config_set (CameraSetting *setting, int count) {

	return (GP_ERROR);
}

int camera_capture (CameraFile *file, CameraCaptureInfo *info) {

	return (GP_ERROR);
}

int camera_summary (CameraText *summary) {

	strcpy(summary->text, "Summary not available.");

	return (GP_OK);
}

int camera_manual (CameraText *manual) {

	strcpy(manual->text, "Manual not available.");

	return (GP_OK);
}

int camera_about (CameraText *about) {

	strcpy(about->text,
			"Panasonic PV-DC1580 gPhoto library\n"
			"Mariusz Zynel <mariusz@mizar.org>\n\n"
			"Based on dc1000 program written by\n"
			"Fredrik Roubert <roubert@df.lth.se> and\n"
			"Galen Brooks <galen@nine.com>.");
	return (GP_OK);
}

/* End of dc1580.c */
