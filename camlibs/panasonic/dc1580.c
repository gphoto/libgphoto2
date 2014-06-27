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

        This is a Panasonic PV/NV-DC1580 camera gPhoto library source code.

        An algorithm  for  checksums  borrowed  from  Michael McCormack's  Nikon
        CoolPix 600 library for gPhoto1.
*/

#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
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
#include "dc1580.h"

#ifndef __FILE__
#  define __FILE__ "dc1580.c"
#endif

#define GP_MODULE "dc1580"

/******************************************************************************/

/* Internal utility functions */

/* dsc2_checksum - establish checksum for size bytes in buffer */

static uint8_t dsc2_checksum(char *buffer, int size) {

        int     checksum = 0;
        int     i;

        for (i = 1; i < size - 2; i++) {
                checksum += buffer[i];
                checksum %= 0x100;
        }

        return checksum;
}

/* dsc2_sendcmd - send command with data to DSC */

static int dsc2_sendcmd(Camera *camera, uint8_t cmd, long int data, uint8_t sequence) {

        int     i;

        DEBUG_PRINT_MEDIUM(("Sending command: 0x%02x, data: %i, sequence: %i.", cmd, data, sequence));

        memset(camera->pl->buf, 0, 16);

        camera->pl->buf[0] = 0x08;
        camera->pl->buf[1] = sequence;
        camera->pl->buf[2] = 0xff - sequence;
        camera->pl->buf[3] = cmd;

        for (i = 0; i < sizeof(data); i++)
                camera->pl->buf[4 + i] = (uint8_t)(data >> 8*i);

        camera->pl->buf[14] = dsc2_checksum(camera->pl->buf, 16);

        return gp_port_write(camera->port, camera->pl->buf, 16);
}

/* dsc2_retrcmd - retrieve command and its data from DSC */

static int dsc2_retrcmd(Camera *camera) {
        
        int     result = GP_ERROR;
        int     s;

        if ((s = gp_port_read(camera->port, camera->pl->buf, 16)) == GP_ERROR)
                return GP_ERROR;

/*      Make sense in debug only. Done on gp_port level.
        if (0 < s)
                dsc_dumpmem(camera->pl->buf, s);

*/
        if (s != 16 ||  camera->pl->buf[DSC2_BUF_BASE] != 0x08 ||
                        camera->pl->buf[DSC2_BUF_SEQ] != 0xff - (uint8_t)camera->pl->buf[DSC2_BUF_SEQC]) {
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */
        }
        else
                result = camera->pl->buf[DSC2_BUF_CMD];

        DEBUG_PRINT_MEDIUM(("Retrieved command: %i.", result));

        return result;
}

/* dsc2_connect - try hand shake with camera and establish connection */

static int dsc2_connect(Camera *camera, int speed) {
        
        DEBUG_PRINT_MEDIUM(("Connecting camera with speed: %i.", speed));

        if (dsc1_setbaudrate(camera, speed) != GP_OK)
                return GP_ERROR;

        if (dsc1_getmodel(camera) != DSC2)
                RETURN_ERROR(EDSCBADDSC);
                /* bad camera model */

        if (dsc2_sendcmd(camera, DSC2_CMD_CONNECT, 0, 0) != GP_OK)
                return GP_ERROR;

        if (dsc2_retrcmd(camera) != DSC2_RSP_OK)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        DEBUG_PRINT_MEDIUM(("Camera connected successfully."));

        return GP_OK;
}

/* dsc2_disconnect - reset camera, free buffers and close files */

static int dsc2_disconnect(Camera *camera) {
        
        DEBUG_PRINT_MEDIUM(("Disconnecting the camera."));

        if (dsc2_sendcmd(camera, DSC2_CMD_RESET, 0, 0) != GP_OK)
                return GP_ERROR;

        if (dsc2_retrcmd(camera) != DSC2_RSP_OK)
                RETURN_ERROR(EDSCBADRSP)
                /* bad response */
        else
                sleep(DSC_PAUSE); /* let camera to redraw its screen */

        DEBUG_PRINT_MEDIUM(("Camera disconnected."));

        return GP_OK;
}

/* dsc2_getindex - retrieve the number of images stored in camera memory */

static int dsc2_getindex(Camera *camera) {
        
        int     result = GP_ERROR;

        DEBUG_PRINT_MEDIUM(("Retrieving the number of images."));

        if (dsc2_sendcmd(camera, DSC2_CMD_GET_INDEX, 0, 0) != GP_OK)
                return GP_ERROR;

        if (dsc2_retrcmd(camera) == DSC2_RSP_INDEX)
                result =
                        ((uint32_t)camera->pl->buf[DSC2_BUF_DATA]) |
                        ((uint8_t)camera->pl->buf[DSC2_BUF_DATA + 1] << 8) |
                        ((uint8_t)camera->pl->buf[DSC2_BUF_DATA + 2] << 16) |
                        ((uint8_t)camera->pl->buf[DSC2_BUF_DATA + 3] << 24);
        else
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        DEBUG_PRINT_MEDIUM(("Number of images: %i", result));

        return result;
}

/* dsc2_delete - delete image #index from camera memory */

static int dsc2_delete(Camera *camera, int index) {
        
        DEBUG_PRINT_MEDIUM(("Deleting image: %i.", index));

        if (index < 1)
                RETURN_ERROR(EDSCBADNUM);
                /* bad image number */

        if (dsc2_sendcmd(camera, DSC2_CMD_DELETE, index, 0) != GP_OK)
                return GP_ERROR;

        if (dsc2_retrcmd(camera) != DSC2_RSP_OK)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        DEBUG_PRINT_MEDIUM(("Image: %i deleted.", index));
        return GP_OK;
}

/* dsc2_selectimage - select image to download, return its size */

static int dsc2_selectimage(Camera *camera, int index, int thumbnail) {
        
        int     size = 0;

        DEBUG_PRINT_MEDIUM(("Selecting image: %i, thumbnail: %i.", index, thumbnail));

        if (index < 1)
                RETURN_ERROR(EDSCBADNUM);
                /* bad image number */

        if (thumbnail == DSC_THUMBNAIL) {
                if (dsc2_sendcmd(camera, DSC2_CMD_THUMB, index, 0) != GP_OK)
                        return GP_ERROR;
        } else {
                if (dsc2_sendcmd(camera, DSC2_CMD_SELECT, index, 0) != GP_OK)
                        return GP_ERROR;
        }

        if (dsc2_retrcmd(camera) != DSC2_RSP_IMGSIZE)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        size =  ((uint32_t)camera->pl->buf[DSC2_BUF_DATA]) |
                ((uint8_t)camera->pl->buf[DSC2_BUF_DATA + 1] << 8) |
                ((uint8_t)camera->pl->buf[DSC2_BUF_DATA + 2] << 16) |
                ((uint8_t)camera->pl->buf[DSC2_BUF_DATA + 3] << 24);

        DEBUG_PRINT_MEDIUM(("Selected image: %i, thumbnail: %i, size: %i.", index, thumbnail, size));

        return size;
}

/* gp_port_readimageblock - read #block block (1024 bytes) of an image into buf */

static int dsc2_readimageblock(Camera *camera, int block, char *buffer) {
        
        DEBUG_PRINT_MEDIUM(("Reading image block: %i.", block));

        if (dsc2_sendcmd(camera, DSC2_CMD_GET_DATA, block, block) != GP_OK)
                return GP_ERROR;

        if (gp_port_read(camera->port, camera->pl->buf, DSC_BUFSIZE) != DSC_BUFSIZE)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        if ((uint8_t)camera->pl->buf[0] != 1 ||
                        (uint8_t)camera->pl->buf[1] != block ||
                        (uint8_t)camera->pl->buf[2] != 0xff - block ||
                        (uint8_t)camera->pl->buf[3] != DSC2_RSP_DATA ||
                        (uint8_t)camera->pl->buf[DSC_BUFSIZE - 2] != dsc2_checksum(camera->pl->buf, DSC_BUFSIZE))
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        if (buffer)
                memcpy(buffer, &camera->pl->buf[4], DSC_BLOCKSIZE);

        DEBUG_PRINT_MEDIUM(("Block: %i read in.", block));

        return DSC_BLOCKSIZE;
}

/* dsc2_readimage - read #index image or thumbnail and return its contents */

#if 0
static char *dsc2_readimage(Camera *camera, int index, int thumbnail, int *size) {

        char    kind[16];
        int     blocks, i;
        char    *buffer = NULL;

        DEBUG_PRINT_MEDIUM(("Reading image: %i, thumbnail: %i.", index, thumbnail));

        if ((*size = dsc2_selectimage(camera, index, thumbnail)) < 0)
                return NULL;

        if (thumbnail == DSC_THUMBNAIL)
                strcpy(kind, "thumbnail");
        else
                strcpy(kind, "image");

        if (!(buffer = (char*)malloc(*size))) {
                DEBUG_PRINT_MEDIUM(("Failed to allocate memory for %s data.", kind));
                return NULL;
        }

        blocks = (*size - 1)/DSC_BLOCKSIZE + 1;

        for (i = 0; i < blocks; i++) {
                if (dsc2_readimageblock(camera, i, &buffer[i*DSC_BLOCKSIZE]) == GP_ERROR) {
                        DEBUG_PRINT_MEDIUM(("Error during %s transfer.", kind));
                        free(buffer);
                        return NULL;
                }
        }

        DEBUG_PRINT_MEDIUM(("Image: %i read in.", index));

        return buffer;
}
#endif

/* dsc2_setimagesize - set size of an image to upload to camera */

static int dsc2_setimagesize(Camera *camera, int size) {

        DEBUG_PRINT_MEDIUM(("Setting image size to: %i.", size));

        if (dsc2_sendcmd(camera, DSC2_CMD_SET_SIZE, size, 0) != GP_OK)
                return GP_ERROR;

        if (dsc2_retrcmd(camera) != DSC2_RSP_OK)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        DEBUG_PRINT_MEDIUM(("Image size set to: %i.", size));

        return GP_OK;
}

/* gp_port_writeimageblock - write size bytes from buffer rounded to 1024 bytes to camera */

static int dsc2_writeimageblock(Camera *camera, int block, char *buffer, int size) {

        DEBUG_PRINT_MEDIUM(("Writing image block: %i.", block));

        memset(camera->pl->buf, 0, DSC_BUFSIZE);

        camera->pl->buf[0] = 0x01;
        camera->pl->buf[1] = block;
        camera->pl->buf[2] = 0xff - block;
        camera->pl->buf[3] = DSC2_CMD_SEND_DATA;

        if (DSC_BLOCKSIZE < size)
                size = DSC_BLOCKSIZE;

        memcpy(&camera->pl->buf[4], buffer, size);

        camera->pl->buf[DSC_BUFSIZE - 2] = dsc2_checksum(camera->pl->buf, DSC_BUFSIZE);

        if (gp_port_write(camera->port, camera->pl->buf, DSC_BUFSIZE) != GP_OK)
                return GP_ERROR;

        if (dsc2_retrcmd(camera) != DSC2_RSP_OK)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        DEBUG_PRINT_MEDIUM(("Block: %i of size: %i written.", block, size));

        return GP_OK;
}

/* dsc2_writeimage - write an image to camera memory, size bytes at buffer */

#if 0
static int dsc2_writeimage(Camera *camera, char *buffer, int size) {
        
        int     blocks, blocksize, i;

        DEBUG_PRINT_MEDIUM(("Writing an image of size: %i.", size));

        if ((dsc2_setimagesize(camera, size)) != GP_OK)
                return GP_ERROR;

        blocks = (size - 1)/DSC_BLOCKSIZE + 1;

        for (i = 0; i < blocks; i++) {
                blocksize = size - i*DSC_BLOCKSIZE;
                if (DSC_BLOCKSIZE < blocksize)
                        blocksize = DSC_BLOCKSIZE;
                if (dsc2_writeimageblock(camera, i, &buffer[i*DSC_BLOCKSIZE], blocksize) != GP_OK) {
                        DEBUG_PRINT_MEDIUM(("Error during image transfer."));
                        return GP_ERROR;
                }
        }

        DEBUG_PRINT_MEDIUM(("Image written successfully."));

        return GP_OK;
}
#endif

/* dsc2_preview - show selected image on camera's LCD  */

#if 0
static int dsc2_preview(Camera *camera, int index) {
        
        if (index < 1)
                RETURN_ERROR(EDSCBADNUM);
                /* bad image number */

        if (dsc2_sendcmd(camera, DSC2_CMD_PREVIEW, index, 0) != GP_OK)
                return GP_ERROR;

        if (dsc2_retrcmd(camera) != DSC2_RSP_OK)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        return GP_OK;
}
#endif

/******************************************************************************/

/* Library interface functions */

int camera_id (CameraText *id) {
        
        strcpy(id->text, "panasonic-dc1580");

        return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {
        
        CameraAbilities a;
        char    *models[] = {
                        "Panasonic:DC1580",
                        "Nikon:CoolPix 600",
                        NULL };
        int     i = 0, result;
        
        while (models[i]) {
		memset(&a, 0, sizeof(a));
		a.status = GP_DRIVER_STATUS_PRODUCTION;
                strcpy(a.model, models[i]);
                a.port         = GP_PORT_SERIAL;
                a.speed[0]     = 9600;
                a.speed[1]     = 19200;
                a.speed[2]     = 38400;
                a.speed[3]     = 57600;
                a.speed[4]     = 115200;
                a.speed[5]     = 0;
                a.operations        = 	GP_OPERATION_NONE;
                a.file_operations   = 	GP_FILE_OPERATION_DELETE | 
					GP_FILE_OPERATION_PREVIEW;
                a.folder_operations = 	GP_FOLDER_OPERATION_PUT_FILE;

                CHECK (gp_abilities_list_append(list, a));
                i++;
        }

        return GP_OK;
}

static int camera_exit (Camera *camera, GPContext *context) {
        gp_context_status(context, _("Disconnecting camera."));
        dsc2_disconnect(camera);
	if (camera->pl->buf) {
		free (camera->pl->buf);
		camera->pl->buf = NULL;
	}
	free (camera->pl);
	camera->pl = NULL;
        return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context) {

        Camera  *camera = data;
        int     count, result;
        
	CHECK (count = dsc2_getindex(camera));
        
        CHECK (gp_list_populate(list, DSC_FILENAMEFMT, count));

        return GP_OK;
}

static int get_info_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileInfo *info,
			  void *data, GPContext *context) {

        Camera  *camera = data;
	int     index, result;

        /* index is the 0-based image number on the camera */
        CHECK (index = gp_filesystem_number(camera->fs, folder, filename, context));
        index++;

	info->file.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_SIZE;
	strcpy(info->file.type, GP_MIME_JPEG);
        info->file.size = dsc2_selectimage(camera, index, DSC_FULLIMAGE);

	info->preview.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_SIZE;
	strcpy(info->preview.type, GP_MIME_JPEG);
        info->preview.size = dsc2_selectimage(camera, index, DSC_THUMBNAIL);

        return GP_OK;
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *data, GPContext *context) {

	Camera *camera = data;
        int     index, i, size, blocks, result;
	unsigned int id;

        gp_context_status(context, _("Downloading %s."), filename);

        /* index is the 0-based image number on the camera */
        CHECK (index = gp_filesystem_number(camera->fs, folder, filename, context));
        index++;

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		size = dsc2_selectimage(camera, index, DSC_THUMBNAIL);
		break;
	case GP_FILE_TYPE_NORMAL:
		size = dsc2_selectimage(camera, index, DSC_FULLIMAGE);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
	if (size < 0)
		return (size);

	CHECK (gp_file_set_mime_type(file, GP_MIME_JPEG));

        blocks = (size - 1)/DSC_BLOCKSIZE + 1;

	id = gp_context_progress_start (context, blocks, _("Getting data..."));
        for (i = 0; i < blocks; i++) {
		CHECK (dsc2_readimageblock(camera, i, NULL));
                CHECK (gp_file_append(file, &camera->pl->buf[4], DSC_BLOCKSIZE));
		gp_context_progress_update (context, id, i + 1);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
        }
	gp_context_progress_stop (context, id);

        return GP_OK;
}

static int put_file_func (CameraFilesystem *fs, const char *folder, const char *name,
			  CameraFileType type, CameraFile *file, void *user_data,
			  GPContext *context) {
        
	Camera *camera = user_data;
        int             blocks, blocksize, i, result;
	const char      *data;
	long unsigned int size;
	unsigned int id;

        gp_context_status(context, _("Uploading image: %s."), name);

/*      We can not figure out file type, at least by now. (? curious, mime type -Marcus)

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

	if ((result = dsc2_setimagesize(camera, size)) != GP_OK) return result;

        blocks = (size - 1)/DSC_BLOCKSIZE + 1;

	id = gp_context_progress_start (context, blocks, _("Uploading..."));
        for (i = 0; i < blocks; i++) {
                blocksize = size - i*DSC_BLOCKSIZE;
                if (DSC_BLOCKSIZE < blocksize)
                        blocksize = DSC_BLOCKSIZE;
		result = dsc2_writeimageblock(camera, i, 
					       (char*)&data[i*DSC_BLOCKSIZE], 
					       blocksize);
		if (result != GP_OK)
			return result;
		gp_context_progress_update (context, id, i + 1);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
        }
	gp_context_progress_stop (context, id);

        return GP_OK;
}

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data,
			     GPContext *context) {
        
	Camera *camera = data;
        int     index, result;

        gp_context_status(context, _("Deleting image %s."), filename);

        /* index is the 0-based image number on the camera */
	CHECK (index = gp_filesystem_number (camera->fs, folder, filename, context));
        index++;

        return dsc2_delete(camera, index);
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context) 
{
        strcpy(about->text,
                        _("Panasonic DC1580 gPhoto2 library\n"
                        "Mariusz Zynel <mariusz@mizar.org>\n\n"
                        "Based on dc1000 program written by\n"
                        "Fredrik Roubert <roubert@df.lth.se> and\n"
                        "Galen Brooks <galen@nine.com>."));
        return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.put_file_func = put_file_func,
	.del_file_func = delete_file_func,
};

int camera_init (Camera *camera, GPContext *context) 
{
        GPPortSettings settings;
        int result, selected_speed;

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

	CHECK (gp_port_set_timeout (camera->port, 5000));

	/* Configure the port (and remember the speed) */
	CHECK (gp_port_get_settings (camera->port, &settings));
	selected_speed = settings.serial.speed;
        settings.serial.speed      = 9600; /* hand shake speed */
        settings.serial.bits       = 8;
        settings.serial.parity     = 0;
        settings.serial.stopbits   = 1;
        CHECK (gp_port_set_settings (camera->port, settings));

      	CHECK (gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera));
	/* Connect with the selected speed */
        return dsc2_connect(camera, selected_speed);
}

/* End of dc1580.c */
