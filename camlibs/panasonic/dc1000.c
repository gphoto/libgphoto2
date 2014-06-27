/*
	Copyright 2000 Mariusz Zynel <mariusz@mizar.org> (gPhoto port)
	Copyright 2000 Fredrik Roubert <roubert@df.lth.se> (idea)
	Copyright 1999 Galen Brooks <galen@nine.com> (DC1580 code)

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
 
#include "config.h"

#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <string.h>

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

#include "dc.h"
#include "dc1000.h"

#ifndef __FILE__
#  define __FILE__ "dc1000.c"
#endif

#define GP_MODULE "dc1000"

/******************************************************************************/

/* Internal utility functions */


/* dsc1_connect - try hand shake with camera and establish connection */

static int dsc1_connect(Camera *camera, int speed) {
	
	uint8_t	data = 0;
	
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

static int dsc1_disconnect(Camera *camera) {
	
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

static int dsc1_getindex(Camera *camera) {
	
	int	result = GP_ERROR;

	DEBUG_PRINT_MEDIUM(("Retrieving the number of images."));
		
	if (dsc1_sendcmd(camera, DSC1_CMD_GET_INDEX, NULL, 0) != GP_OK)
		return GP_ERROR;
	
	if (dsc1_retrcmd(camera) == DSC1_RSP_INDEX)
		result = camera->pl->size / 2;
	else
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */
	
	DEBUG_PRINT_MEDIUM(("Number of images: %i", result));
	
	return result;
}

/* dsc1_delete - delete image #index from camera memory */

static int dsc1_delete(Camera *camera, uint8_t index) {
	
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

static int dsc1_selectimage(Camera *camera, uint8_t index)
{
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
	
	if (camera->pl->size != 4)
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */

	size =	(uint32_t)camera->pl->buf[3] |
		((uint8_t)camera->pl->buf[2] << 8) |
		((uint8_t)camera->pl->buf[1] << 16) |
		((uint8_t)camera->pl->buf[0] << 24);		
	
	DEBUG_PRINT_MEDIUM(("Selected image: %i, size: %i.", index, size));

	return size;
}

/* gp_port_readimageblock - read #block block (1024 bytes) of an image into buf */

static int dsc1_readimageblock(Camera *camera, int block, char *buffer) {
	
	char	buf[2];
	
	DEBUG_PRINT_MEDIUM(("Reading image block: %i.", block));

	buf[0] = (uint8_t)(block >> 8);
	buf[1] = (uint8_t)block;
	
	if (dsc1_sendcmd(camera, DSC1_CMD_GET_DATA, buf, 2) != GP_OK)
		return GP_ERROR;
	
	if (dsc1_retrcmd(camera) != DSC1_RSP_DATA)
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */

	if (buffer)
		memcpy(buffer, camera->pl->buf, camera->pl->size);
	
	DEBUG_PRINT_MEDIUM(("Block: %i read in.", block));
	
	return camera->pl->size;
}

/* dsc1_readimage - read #index image or thumbnail and return its contents */
#if 0
static char *dsc1_readimage(Camera *camera, int index, int *size) {

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
#endif

/* dsc1_setimageres - set image resolution based on its size, used on upload to camera */

static int dsc1_setimageres(Camera *camera, int size) {
	
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

static int dsc1_writeimageblock(Camera *camera, int block, char *buffer, int size) {

	DEBUG_PRINT_MEDIUM(("Writing image block: %i", block));

	dsc1_sendcmd(camera, DSC1_CMD_SEND_DATA, buffer, size);

	if (dsc1_retrcmd(camera) != DSC1_RSP_OK)
		RETURN_ERROR(EDSCBADRSP);
		/* bad response */
		
	DEBUG_PRINT_MEDIUM(("Block: %i of size: %i written.", block, size));
	
	return GP_OK;
}

/* gp_port_writeimage - write an image to camera memory, size bytes at buffer */
#if 0
static int dsc1_writeimage(Camera *camera, char *buffer, int size)
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
#endif

/* dsc1_preview - show selected image on camera's LCD - is it supported? */

#if 0
static int dsc1_preview(Camera *camera, int index)
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
#endif

/******************************************************************************/

/* Library interface functions */

int camera_id (CameraText *id) {

	strcpy(id->text, "panasonic-dc1000");

	return GP_OK;
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities a;
        int             result;

	memset (&a, 0, sizeof(a));
	strcpy(a.model, "Panasonic:DC1000");
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

static int camera_exit (Camera *camera, GPContext *context) {

	gp_context_status(context, _("Disconnecting camera."));
	dsc1_disconnect(camera);

	if (camera->pl->buf) {
		free (camera->pl->buf);
		camera->pl->buf = NULL;
	}
	free (camera->pl);
	camera->pl = NULL;
	return GP_OK;
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context) 
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
			  CameraFile *file, void *data, GPContext *context) 
{
	Camera *camera = data;
	int	index, size, rsize, i, s;
	unsigned int id;

	if (type != GP_FILE_TYPE_NORMAL)
		return (GP_ERROR_NOT_SUPPORTED);

	gp_context_status(context, _("Downloading image %s."), filename);

	/* index is the 0-based image number on the camera */
	index = gp_filesystem_number (camera->fs, folder, filename, context);
	if (index < 0) 
		return (index);

	if ((size = dsc1_selectimage(camera, index + 1)) < 0)
		return (GP_ERROR);
	
	gp_file_set_mime_type (file, GP_MIME_JPEG);

	id = gp_context_progress_start (context, size, _("Getting data..."));
	for (i = 0, s = 0; s < size; i++) {
		if ((rsize = dsc1_readimageblock(camera, i, NULL)) == GP_ERROR) 
			return GP_ERROR;
		s += rsize;
		gp_file_append (file, camera->pl->buf, camera->pl->size);
		gp_context_progress_update (context, id, s);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
	}
	gp_context_progress_stop (context, id);
	
	return GP_OK;
}

static int put_file_func (CameraFilesystem *fs, const char *folder, const char *name,
			  CameraFileType type, CameraFile *file, void *user_data,
			  GPContext *context) 
{
	Camera *camera = user_data;
	int	blocks, blocksize, i;
	int	result;
	const char *data;
	unsigned long int size;
	unsigned int id;
	
	gp_context_status(context, _("Uploading image: %s."), name);	
	
/*	We can not figure out file type, at least by now.

	if (strcmp(file->type, "image/jpg") != 0) {
		dsc_print_message(camera, "JPEG image format allowed only.");
		return GP_ERROR;		
	}
*/	
	gp_file_get_data_and_size (file, &data, &size);
	if (size > DSC_MAXIMAGESIZE) {
		gp_context_message (context, _("File size is %ld bytes. "
				   "The size of the largest file possible to "
				   "upload is: %i bytes."), size, 
				   DSC_MAXIMAGESIZE);
		return GP_ERROR;		
	}

	result = dsc1_setimageres (camera, size);
	if (result != GP_OK)
		return (result);
	
	blocks = (size - 1)/DSC_BLOCKSIZE + 1;

	id = gp_context_progress_start (context, blocks, _("Uploading..."));
	for (i = 0; i < blocks; i++) {
		blocksize = size - i*DSC_BLOCKSIZE;
		if (DSC_BLOCKSIZE < blocksize) 
			blocksize = DSC_BLOCKSIZE;
		result = dsc1_writeimageblock (camera, i, 
					       (char*)&data[i*DSC_BLOCKSIZE], 
					       blocksize);
		if (result != GP_OK)
			return (result);

		gp_context_progress_update (context, id, i + 1);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
	}
	gp_context_progress_stop (context, id);

	return GP_OK;
}

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data,
			     GPContext *context) 
{
	Camera *camera = data;
	int	index, result;

	gp_context_status(context, _("Deleting image %s."), filename);

	/* index is the 0-based image number on the camera */
	CHECK (index = gp_filesystem_number (camera->fs, folder, filename,
					     context));
        index++;

	return dsc1_delete(camera, index);
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context) 
{
	strcpy(about->text,
			_("Panasonic DC1000 gPhoto library\n"
			"Mariusz Zynel <mariusz@mizar.org>\n\n"
			"Based on dc1000 program written by\n"
			"Fredrik Roubert <roubert@df.lth.se> and\n"
			"Galen Brooks <galen@nine.com>."));
	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.put_file_func = put_file_func,
};

int camera_init (Camera *camera, GPContext *context) {

        int ret, selected_speed;
	GPPortSettings settings;

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->about                = camera_about;

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	camera->pl->buf = malloc (sizeof (char) * DSC_BUFSIZE);
	if (!camera->pl->buf) {
		free (camera->pl);
		camera->pl = NULL;
		return (GP_ERROR_NO_MEMORY);
	}

	/* Configure the port (remember the selected speed) */
        gp_port_set_timeout(camera->port, 5000);
	gp_port_get_settings(camera->port, &settings);
	selected_speed = settings.serial.speed;
        settings.serial.speed      = 9600; /* hand shake speed */
        settings.serial.bits       = 8;
        settings.serial.parity     = 0;
        settings.serial.stopbits   = 1;
        gp_port_set_settings(camera->port, settings);

	/* Set up the filesystem */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
	/* Connect with selected speed */
	ret = dsc1_connect(camera, selected_speed);
	if (ret < 0) {
		free (camera->pl->buf);
		free (camera->pl);
		camera->pl = NULL;
		return (ret);
	}

	return (GP_OK);
}
/* End of dc1000.c */
