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
	
	This is a Panasonic PV/NV-DC1000 camera gPhoto library source code.
	
*/	
 
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "dc.h"
#include "dc1000.h"

#ifndef __FILE__
#  define __FILE__ "dc1000.c"
#endif

/******************************************************************************/

/* Internal utility functions */


/* dsc1_connect - try hand shake with camera and establish connection */

int dsc1_connect(Camera *camera, int speed) {
	
	u_int8_t	data = 0;
	
	DEBUG_PRINT_MEDIUM(("Connecting a camera."));
		
	if (dsc1_setbaudrate(camera, speed) != GP_OK)
		return GP_ERROR;
	
	if (dsc1_getmodel(camera) != DSC1)
		RETURN_ERROR(EDSCBADDSC);
		/* bad camera model */
	
	dsc1_sendcmd(camera, DSC1_CMD_CONNECT, &data, 1);
	
	if (dsc1_retrcmd(camera) != DSC1_RSP_OK) 
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */

	DEBUG_PRINT_MEDIUM(("Camera connected successfully."));
	
	return GP_OK;	
}

/* dsc1_disconnect - reset camera, free buffers and close files */

int dsc1_disconnect(Camera *camera) {
	
	DEBUG_PRINT_MEDIUM(("Disconnecting the camera."));
	
	if (dsc1_sendcmd(camera, DSC1_CMD_RESET, NULL, 0) != GP_OK)
		return GP_ERROR;
	
	if (dsc1_retrcmd(camera) != DSC1_RSP_OK) 
		RETURN_ERROR(EDSCBADRSP)
		/* bad response */	
	else 
		sleep(DSC_PAUSE); /* let camera to redraw its screen */
	
	DEBUG_PRINT_MEDIUM(("Camera disconnected."));
	
	return GP_OK;
}

/* dsc1_getindex - retrieve the number of images stored in camera memory */

int dsc1_getindex(Camera *camera) {
	
        dsc_t   *dsc = camera->camlib_data;
	int	result = GP_ERROR;

	DEBUG_PRINT_MEDIUM(("Retrieving the number of images."));
		
	if (dsc1_sendcmd(camera, DSC1_CMD_GET_INDEX, NULL, 0) != GP_OK)
		return GP_ERROR;
	
	if (dsc1_retrcmd(camera) == DSC1_RSP_INDEX)
		result = dsc->size / 2;
	else
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */
	
	DEBUG_PRINT_MEDIUM(("Number of images: %i", result));
	
	return result;
}

/* dsc1_delete - delete image #index from camera memory */

int dsc1_delete(Camera *camera, u_int8_t index) {
	
	DEBUG_PRINT_MEDIUM(("Deleting image: %i.", index));
		
	if (index < 1)
		RETURN_ERROR(EDSCBADNUM);
		/* bad image number */

	if (dsc1_sendcmd(camera, DSC1_CMD_DELETE, &index, 1) != GP_OK)
		return GP_ERROR;

	if (dsc1_retrcmd(camera) != DSC1_RSP_OK)
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */

	DEBUG_PRINT_MEDIUM(("Image: %i deleted.", index));

	return GP_OK;
}	
	
/* dsc1_selectimage - select image to download, return its size */

int dsc1_selectimage(Camera *camera, u_int8_t index)
{
	dsc_t   *dsc = camera->camlib_data;
        int	size = 0;
	
	DEBUG_PRINT_MEDIUM(("Selecting image: %i.", index));	
	
	if (index < 1)
		RETURN_ERROR(EDSCBADNUM);
		/* bad image number */

	if (dsc1_sendcmd(camera, DSC1_CMD_SELECT, &index, 1) != GP_OK)
		return GP_ERROR;

	if (dsc1_retrcmd(camera) != DSC1_RSP_IMGSIZE)
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */
	
	if (dsc->size != 4)
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */

	size =	(u_int32_t)dsc->buf[3] |
		((u_int8_t)dsc->buf[2] << 8) |
		((u_int8_t)dsc->buf[1] << 16) |
		((u_int8_t)dsc->buf[0] << 24);		
	
	DEBUG_PRINT_MEDIUM(("Selected image: %i, size: %i.", index, size));

	return size;
}

/* gp_port_readimageblock - read #block block (1024 bytes) of an image into buf */

int dsc1_readimageblock(Camera *camera, int block, char *buffer) {
	
        dsc_t   *dsc = camera->camlib_data;
	char	buf[2];
	
	DEBUG_PRINT_MEDIUM(("Reading image block: %i.", block));

	buf[0] = (u_int8_t)(block >> 8);
	buf[1] = (u_int8_t)block;
	
	if (dsc1_sendcmd(camera, DSC1_CMD_GET_DATA, buf, 2) != GP_OK)
		return GP_ERROR;
	
	if (dsc1_retrcmd(camera) != DSC1_RSP_DATA)
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */

	if (buffer)
		memcpy(buffer, dsc->buf, dsc->size);
	
	DEBUG_PRINT_MEDIUM(("Block: %i read in.", block));
	
	return dsc->size;
}

/* dsc1_readimage - read #index image or thumbnail and return its contents */

char *dsc1_readimage(Camera *camera, int index, int *size) {

	int	rsize, i, s;
	char	*buffer = NULL;

	DEBUG_PRINT_MEDIUM(("Reading image: %i.", index));
		
	if ((*size = dsc1_selectimage(camera, index)) < 0)
		return NULL;
	
	if (!(buffer = (char*)malloc(*size))) {
		DEBUG_PRINT_MEDIUM(("Failed to allocate memory for image data."));
		return NULL;
	}
	
	for (i = 0, s = 0; s < *size; i++) {
		if ((rsize = dsc1_readimageblock(camera, i, &buffer[s])) == GP_ERROR) {
			DEBUG_PRINT_MEDIUM(("Error during image transfer."));
			free(buffer);
			return NULL;
		}
		s += rsize;
	}

	DEBUG_PRINT_MEDIUM(("Image: %i read in.", index));
	
	return buffer;
}

/* dsc1_setimageres - set image resolution based on its size, used on upload to camera */

int dsc1_setimageres(Camera *camera, int size) {
	
	dsc_quality_t	res;
	
	DEBUG_PRINT_MEDIUM(("Setting image resolution, image size: %i.", size));	
	
	if (size < 65536)
		res = normal;
	else if (size < 196608)
		res = fine;
	else
		res = superfine;

	if (dsc1_sendcmd(camera, DSC1_CMD_SET_RES, &res, 1) != GP_OK)
		return GP_ERROR;

	if (dsc1_retrcmd(camera) != DSC1_RSP_OK)
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */
		
	DEBUG_PRINT_MEDIUM(("Image resolution set to: %i", res));

	return GP_OK;
}

/* gp_port_writeimageblock - write size bytes from buffer rounded to 1024 bytes to camera */

int dsc1_writeimageblock(Camera *camera, int block, char *buffer, int size) {

	DEBUG_PRINT_MEDIUM(("Writing image block: %i", block));

	dsc1_sendcmd(camera, DSC1_CMD_SEND_DATA, buffer, size);

	if (dsc1_retrcmd(camera) != DSC1_RSP_OK)
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */
		
	DEBUG_PRINT_MEDIUM(("Block: %i of size: %i written.", block, size));
	
	return GP_OK;
}

/* gp_port_writeimage - write an image to camera memory, size bytes at buffer */

int dsc1_writeimage(Camera *camera, char *buffer, int size)
{
	int	blocks, blocksize, i;

	DEBUG_PRINT_MEDIUM(("Writing an image of size: %i.", size));
		
	if ((dsc1_setimageres(camera, size)) != GP_OK)
		return GP_ERROR;
	
	blocks = (size - 1)/DSC_BLOCKSIZE + 1;
			
	for (i = 0; i < blocks; i++) {
		blocksize = size - i*DSC_BLOCKSIZE;
		if (DSC_BLOCKSIZE < blocksize) 
			blocksize = DSC_BLOCKSIZE;
		if (dsc1_writeimageblock(camera, i, &buffer[i*DSC_BLOCKSIZE], blocksize) != GP_OK) {
			DEBUG_PRINT_MEDIUM(("Error during image transfer."));
			return GP_ERROR;
		}
	}
	
	DEBUG_PRINT_MEDIUM(("Image written successfully."));
	
	return GP_OK;
}

/* dsc1_preview - show selected image on camera's LCD - is it supported? */

int dsc1_preview(Camera *camera, int index)
{
	if (index < 1)
		RETURN_ERROR(EDSCBADNUM);
		/* bad image number */

	if (dsc1_sendcmd(camera, DSC1_CMD_PREVIEW, &index, 1) != GP_OK)
		return GP_ERROR;
	
	if (dsc1_retrcmd(camera) != DSC1_RSP_OK)
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */
	
	return GP_OK;
}

/******************************************************************************/

/* Library interface functions */

int camera_id (CameraText *id) {

	strcpy(id->text, "panasonic-dc1000");

	return GP_OK;
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities a;
        int             result;

	strcpy(a.model, "Panasonic DC1000");
	a.status = GP_DRIVER_STATUS_PRODUCTION;
	a.port		= GP_PORT_SERIAL;
	a.speed[0] 	= 9600;
	a.speed[1] 	= 19200;
	a.speed[2] 	= 38400;
	a.speed[3] 	= 57600;			
	a.speed[4] 	= 115200;	
	a.speed[5] 	= 0;	
	a.operations        = GP_OPERATION_NONE;
	a.file_operations   = GP_FILE_OPERATION_DELETE;
	a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE;

	CHECK (gp_abilities_list_append(list, a));

	return GP_OK;
}

static int camera_exit (Camera *camera) {
	
	dsc_t	*dsc = (dsc_t *)camera->camlib_data;
	
	dsc_print_status(camera, _("Disconnecting camera."));
	
	dsc1_disconnect(camera);
	
	free(dsc);

	return GP_OK;
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data) 
{
	Camera *camera = data;
	int 	count;
	
	if ((count = dsc1_getindex (camera)) == GP_ERROR)
		return GP_ERROR;
	
	gp_list_populate (list, DSC_FILENAMEFMT, count);

	return GP_OK;
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *data) 
{
	Camera *camera = data;
	dsc_t	*dsc = (dsc_t *)camera->camlib_data;
	int	index, size, rsize, i, s;

	if (type != GP_FILE_TYPE_NORMAL)
		return (GP_ERROR_NOT_SUPPORTED);

	dsc_print_status(camera, _("Downloading image %s."), filename);

	/* index is the 0-based image number on the camera */
	index = gp_filesystem_number (camera->fs, folder, filename);
	if (index < 0) 
		return (index);

	if ((size = dsc1_selectimage(camera, index + 1)) < 0)
		return (GP_ERROR);
	
	gp_file_set_name (file, filename);
	gp_file_set_mime_type (file, "image/jpeg");

	for (i = 0, s = 0; s < size; i++) {
		if ((rsize = dsc1_readimageblock(camera, i, NULL)) == GP_ERROR) 
			return GP_ERROR;
		s += rsize;
		gp_file_append (file, dsc->buf, dsc->size);
		gp_camera_progress (camera, (float)(s)/(float)size);
	}
	
	return GP_OK;
}

static int put_file_func (CameraFilesystem *fs, const char *folder,
			  CameraFile *file, void *user_data) 
{
	Camera *camera = user_data;
	int	blocks, blocksize, i;
	int	result;
	const char *name;
	const char *data;
	long int size;
	
	gp_file_get_name (file, &name);
	dsc_print_status(camera, _("Uploading image: %s."), name);	
	
/*	We can not figure out file type, at least by now.

	if (strcmp(file->type, "image/jpg") != 0) {
		dsc_print_message(camera, "JPEG image format allowed only.");
		return GP_ERROR;		
	}
*/	
	gp_file_get_data_and_size (file, &data, &size);
	if (size > DSC_MAXIMAGESIZE) {
		dsc_print_message (camera, _("File size is %i bytes. "
				   "The size of the largest file possible to "
				   "upload is: %i bytes."), size, 
				   DSC_MAXIMAGESIZE);
		return GP_ERROR;		
	}

	result = dsc1_setimageres (camera, size);
	if (result != GP_OK)
		return (result);
	
	blocks = (size - 1)/DSC_BLOCKSIZE + 1;
			
	for (i = 0; i < blocks; i++) {
		blocksize = size - i*DSC_BLOCKSIZE;
		if (DSC_BLOCKSIZE < blocksize) 
			blocksize = DSC_BLOCKSIZE;
		result = dsc1_writeimageblock (camera, i, 
					       (char*)&data[i*DSC_BLOCKSIZE], 
					       blocksize);
		if (result != GP_OK)
			return (result);

		gp_camera_progress (camera, (float)(i+1)/(float)blocks);
	}

	return GP_OK;
}

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data) 
{
	Camera *camera = data;
	int	index, result;

	dsc_print_status(camera, _("Deleting image %s."), filename);

	/* index is the 0-based image number on the camera */
	CHECK (index = gp_filesystem_number (camera->fs, folder, filename));
        index++;

	return dsc1_delete(camera, index);
}

static int camera_summary (Camera *camera, CameraText *summary) 
{
	strcpy(summary->text, _("Summary not available."));

	return GP_OK;
}

static int camera_manual (Camera *camera, CameraText *manual) 
{
	strcpy (manual->text, _("Manual not available."));

	return GP_OK;
}

static int camera_about (Camera *camera, CameraText *about) 
{
	strcpy(about->text,
			_("Panasonic DC1000 gPhoto library\n"
			"Mariusz Zynel <mariusz@mizar.org>\n\n"
			"Based on dc1000 program written by\n"
			"Fredrik Roubert <roubert@df.lth.se> and\n"
			"Galen Brooks <galen@nine.com>."));
	return GP_OK;
}

int camera_init (Camera *camera) {

        int ret, selected_speed;
        dsc_t           *dsc = NULL;
        
        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

        if (dsc && camera->port) {
                gp_port_close(camera->port);
                gp_port_free(camera->port);
        }
        free(camera);

        /* first of all allocate memory for a dsc struct */
        if ((dsc = (dsc_t*)malloc(sizeof(dsc_t))) == NULL) { 
                dsc_errorprint(EDSCSERRNO, __FILE__, __LINE__);
                return GP_ERROR; 
        } 
        
        camera->camlib_data = dsc;
        
        if ((ret = gp_port_new(&(camera->port), GP_PORT_SERIAL)) < 0) {
            free(camera);
            return (ret);
        }
        
        gp_port_timeout_set(camera->port, 5000);

	/* Get the settings */
	gp_port_settings_get(camera->port, &(dsc->settings));

	/* Remember the selected speed */
	selected_speed = dsc->settings.serial.speed;

	/* Set the initial settings */
        dsc->settings.serial.speed      = 9600; /* hand shake speed */
        dsc->settings.serial.bits       = 8;
        dsc->settings.serial.parity     = 0;
        dsc->settings.serial.stopbits   = 1;
        gp_port_settings_set(camera->port, dsc->settings);

        /* allocate memory for a dsc read/write buffer */
        if ((dsc->buf = (char *)malloc(sizeof(char)*(DSC_BUFSIZE))) == NULL) {
                dsc_errorprint(EDSCSERRNO, __FILE__, __LINE__);
                free(camera);
                return GP_ERROR;
        }

	/* Set up the filesystem */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      delete_file_func, camera);
	gp_filesystem_set_folder_funcs (camera->fs, put_file_func,
				        NULL, camera);
        
	/* Connect with selected speed */
        return dsc1_connect(camera, selected_speed); 
}


/* End of dc1000.c */
