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
#include "dc1580.h"

#ifndef __FILE__
#  define __FILE__ "dc1580.c"
#endif

/******************************************************************************/

/* Internal utility functions */

/* dsc2_checksum - establish checksum for size bytes in buffer */

u_int8_t dsc2_checksum(char *buffer, int size) {

        int     checksum = 0;
        int     i;

        for (i = 1; i < size - 2; i++) {
                checksum += buffer[i];
                checksum %= 0x100;
        }

        return checksum;
}

/* dsc2_sendcmd - send command with data to DSC */

int dsc2_sendcmd(Camera *camera, u_int8_t cmd, long int data, u_int8_t sequence) {

        dsc_t   *dsc = camera->camlib_data;
        int     i;

        DEBUG_PRINT_MEDIUM(("Sending command: 0x%02x, data: %i, sequence: %i.", cmd, data, sequence));

        memset(dsc->buf, 0, 16);

        dsc->buf[0] = 0x08;
        dsc->buf[1] = sequence;
        dsc->buf[2] = 0xff - sequence;
        dsc->buf[3] = cmd;

        for (i = 0; i < sizeof(data); i++)
                dsc->buf[4 + i] = (u_int8_t)(data >> 8*i);

        dsc->buf[14] = dsc2_checksum(dsc->buf, 16);

        return gp_port_write(camera->port, dsc->buf, 16);
}

/* dsc2_retrcmd - retrieve command and its data from DSC */

int dsc2_retrcmd(Camera *camera) {
        
        dsc_t   *dsc = camera->camlib_data;
        int     result = GP_ERROR;
        int     s;

        if ((s = gp_port_read(camera->port, dsc->buf, 16)) == GP_ERROR)
                return GP_ERROR;

/*      Make sense in debug only. Done on gp_port level.
        if (0 < s)
                dsc_dumpmem(dsc->buf, s);

*/
        if (s != 16 ||  dsc->buf[DSC2_BUF_BASE] != 0x08 ||
                        dsc->buf[DSC2_BUF_SEQ] != 0xff - (u_int8_t)dsc->buf[DSC2_BUF_SEQC]) {
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */
        }
        else
                result = dsc->buf[DSC2_BUF_CMD];

        DEBUG_PRINT_MEDIUM(("Retrieved command: %i.", result));

        return result;
}

/* dsc2_connect - try hand shake with camera and establish connection */

int dsc2_connect(Camera *camera, int speed) {
        
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

int dsc2_disconnect(Camera *camera) {
        
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

int dsc2_getindex(Camera *camera) {
        
        dsc_t   *dsc = camera->camlib_data;
        int     result = GP_ERROR;

        DEBUG_PRINT_MEDIUM(("Retrieving the number of images."));

        if (dsc2_sendcmd(camera, DSC2_CMD_GET_INDEX, 0, 0) != GP_OK)
                return GP_ERROR;

        if (dsc2_retrcmd(camera) == DSC2_RSP_INDEX)
                result =
                        ((u_int32_t)dsc->buf[DSC2_BUF_DATA]) |
                        ((u_int8_t)dsc->buf[DSC2_BUF_DATA + 1] << 8) |
                        ((u_int8_t)dsc->buf[DSC2_BUF_DATA + 2] << 16) |
                        ((u_int8_t)dsc->buf[DSC2_BUF_DATA + 3] << 24);
        else
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        DEBUG_PRINT_MEDIUM(("Number of images: %i", result));

        return result;
}

/* dsc2_delete - delete image #index from camera memory */

int dsc2_delete(Camera *camera, int index) {
        
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

int dsc2_selectimage(Camera *camera, int index, int thumbnail) {
        
        dsc_t   *dsc = camera->camlib_data;
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

        size =  ((u_int32_t)dsc->buf[DSC2_BUF_DATA]) |
                ((u_int8_t)dsc->buf[DSC2_BUF_DATA + 1] << 8) |
                ((u_int8_t)dsc->buf[DSC2_BUF_DATA + 2] << 16) |
                ((u_int8_t)dsc->buf[DSC2_BUF_DATA + 3] << 24);

        DEBUG_PRINT_MEDIUM(("Selected image: %i, thumbnail: %i, size: %i.", index, thumbnail, size));

        return size;
}

/* gp_port_readimageblock - read #block block (1024 bytes) of an image into buf */

int dsc2_readimageblock(Camera *camera, int block, char *buffer) {
        
        dsc_t   *dsc = camera->camlib_data;
        
        DEBUG_PRINT_MEDIUM(("Reading image block: %i.", block));

        if (dsc2_sendcmd(camera, DSC2_CMD_GET_DATA, block, block) != GP_OK)
                return GP_ERROR;

        if (gp_port_read(camera->port, dsc->buf, DSC_BUFSIZE) != DSC_BUFSIZE)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        if ((u_int8_t)dsc->buf[0] != 1 ||
                        (u_int8_t)dsc->buf[1] != block ||
                        (u_int8_t)dsc->buf[2] != 0xff - block ||
                        (u_int8_t)dsc->buf[3] != DSC2_RSP_DATA ||
                        (u_int8_t)dsc->buf[DSC_BUFSIZE - 2] != dsc2_checksum(dsc->buf, DSC_BUFSIZE))
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        if (buffer)
                memcpy(buffer, &dsc->buf[4], DSC_BLOCKSIZE);

        DEBUG_PRINT_MEDIUM(("Block: %i read in.", block));

        return DSC_BLOCKSIZE;
}

/* dsc2_readimage - read #index image or thumbnail and return its contents */

char *dsc2_readimage(Camera *camera, int index, int thumbnail, int *size) {

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

/* dsc2_setimagesize - set size of an image to upload to camera */

int dsc2_setimagesize(Camera *camera, int size) {

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

int dsc2_writeimageblock(Camera *camera, int block, char *buffer, int size) {

        dsc_t   *dsc = camera->camlib_data;
        
        DEBUG_PRINT_MEDIUM(("Writing image block: %i.", block));

        memset(dsc->buf, 0, DSC_BUFSIZE);

        dsc->buf[0] = 0x01;
        dsc->buf[1] = block;
        dsc->buf[2] = 0xff - block;
        dsc->buf[3] = DSC2_CMD_SEND_DATA;

        if (DSC_BLOCKSIZE < size)
                size = DSC_BLOCKSIZE;

        memcpy(&dsc->buf[4], buffer, size);

        dsc->buf[DSC_BUFSIZE - 2] = dsc2_checksum(dsc->buf, DSC_BUFSIZE);

        if (gp_port_write(camera->port, dsc->buf, DSC_BUFSIZE) != GP_OK)
                return GP_ERROR;

        if (dsc2_retrcmd(camera) != DSC2_RSP_OK)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        DEBUG_PRINT_MEDIUM(("Block: %i of size: %i written.", block, size));

        return GP_OK;
}

/* dsc2_writeimage - write an image to camera memory, size bytes at buffer */

int dsc2_writeimage(Camera *camera, char *buffer, int size) {
        
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

/* dsc2_preview - show selected image on camera's LCD  */

int dsc2_preview(Camera *camera, int index) {
        
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

/******************************************************************************/

/* Library interface functions */

int camera_id (CameraText *id) {
        
        strcpy(id->text, "panasonic-dc1580");

        return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {
        
        CameraAbilities a;
        char    *models[] = {
                        "Panasonic DC1580",
                        "Nikon CoolPix 600",
                        NULL };
        int     i = 0, result;
        
        while (models[i]) {
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

static int camera_exit (Camera *camera) {
        
        dsc_t   *dsc = camera->camlib_data;

        dsc_print_status(camera, _("Disconnecting camera."));

        dsc2_disconnect(camera);

        free(dsc);

        return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			      CameraList *list, void *data) {

        Camera  *camera = data;
        int     count, result;
        
	CHECK (count = dsc2_getindex(camera));
        
        CHECK (gp_list_populate(list, DSC_FILENAMEFMT, count));

        return GP_OK;
}

static int get_info_func (CameraFilesystem *fs, const char *folder,
		        const char *filename, CameraFileInfo *info, void *data) {

        Camera  *camera = data;
	int     index, result;

        /* index is the 0-based image number on the camera */
        CHECK (index = gp_filesystem_number(camera->fs, folder, filename));
        index++;

	info->file.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_NAME | GP_FILE_INFO_SIZE;
	strcpy(info->file.type, GP_MIME_JPEG);
        sprintf(info->file.name, DSC_FILENAMEFMT, index);
        info->file.size = dsc2_selectimage(camera, index, DSC_FULLIMAGE);

	info->preview.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_NAME | GP_FILE_INFO_SIZE;
	strcpy(info->preview.type, GP_MIME_JPEG);
        sprintf(info->preview.name, DSC_THUMBNAMEFMT, index);
        info->preview.size = dsc2_selectimage(camera, index, DSC_THUMBNAIL);

        return GP_OK;
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *data) {

	Camera *camera = data;
        dsc_t   *dsc = camera->camlib_data;
        int     index, i, size, blocks, result;

        dsc_print_status(camera, _("Downloading %s."), filename);

        /* index is the 0-based image number on the camera */
        CHECK (index = gp_filesystem_number(camera->fs, folder, filename));
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

	CHECK (gp_file_set_name(file, filename));
	CHECK (gp_file_set_mime_type(file, GP_MIME_JPEG));

        blocks = (size - 1)/DSC_BLOCKSIZE + 1;

        for (i = 0; i < blocks; i++) {
		CHECK (dsc2_readimageblock(camera, i, NULL));
                CHECK (gp_file_append(file, &dsc->buf[4], DSC_BLOCKSIZE));
                gp_camera_progress(camera, (float)(i+1)/(float)blocks);
        }

        return GP_OK;
}

static int put_file_func (CameraFilesystem *fs, const char *folder,
			  CameraFile *file, void *user_data) {
        
	Camera *camera = user_data;
        int             blocks, blocksize, i, result;
	const char      *name;
	const char      *data;
	long int        size;

	gp_file_get_name(file, &name);
        dsc_print_status(camera, _("Uploading image: %s."), name);

/*      We can not figure out file type, at least by now.

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

	if ((result = dsc2_setimagesize(camera, size)) != GP_OK) return result;

        blocks = (size - 1)/DSC_BLOCKSIZE + 1;

        for (i = 0; i < blocks; i++) {
                blocksize = size - i*DSC_BLOCKSIZE;
                if (DSC_BLOCKSIZE < blocksize)
                        blocksize = DSC_BLOCKSIZE;
		result = dsc2_writeimageblock(camera, i, 
					       (char*)&data[i*DSC_BLOCKSIZE], 
					       blocksize);
		if (result != GP_OK)
			return result;
                gp_camera_progress(camera, (float)(i+1)/(float)blocks);
        }

        return GP_OK;
}

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data) {
        
	Camera *camera = data;
        int     index, result;

        dsc_print_status(camera, _("Deleting image %s."), filename);

        /* index is the 0-based image number on the camera */
	CHECK (index = gp_filesystem_number (camera->fs, folder, filename));
        index++;

        return dsc2_delete(camera, index);
}

static int camera_summary (Camera *camera, CameraText *summary) 
{
        strcpy(summary->text, _("Summary not available."));

        return (GP_OK);
}

static int camera_manual (Camera *camera, CameraText *manual) 
{
        strcpy(manual->text, _("Manual not available."));

        return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about) 
{
        strcpy(about->text,
                        _("Panasonic DC1580 gPhoto library\n"
                        "Mariusz Zynel <mariusz@mizar.org>\n\n"
                        "Based on dc1000 program written by\n"
                        "Fredrik Roubert <roubert@df.lth.se> and\n"
                        "Galen Brooks <galen@nine.com>."));
        return (GP_OK);
}

int camera_init (Camera *camera) 
{
        
        dsc_t           *dsc = NULL;
        int             result;

        dsc_print_status(camera, _("Initializing camera %s"), camera->model);

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

        /* first of all allocate memory for a dsc struct */
        if ((dsc = (dsc_t*)malloc(sizeof(dsc_t))) == NULL) {
                dsc_errorprint(EDSCSERRNO, __FILE__, __LINE__);
                return GP_ERROR;
        }

        camera->camlib_data = dsc;

        CHECK (gp_port_timeout_set(camera->port, 5000));
        
        strcpy(dsc->settings.serial.port, camera->port_info->path);
        dsc->settings.serial.speed      = 9600; /* hand shake speed */
        dsc->settings.serial.bits       = 8;
        dsc->settings.serial.parity     = 0;
        dsc->settings.serial.stopbits   = 1;

        CHECK (gp_port_settings_set(camera->port, dsc->settings));

      	CHECK (gp_filesystem_set_list_funcs (camera->fs,
		file_list_func, NULL, camera));
	CHECK (gp_filesystem_set_info_funcs (camera->fs,
		get_info_func, NULL, camera));
	CHECK (gp_filesystem_set_file_funcs (camera->fs,
		get_file_func, delete_file_func, camera));
	CHECK (gp_filesystem_set_folder_funcs (camera->fs,
		put_file_func, NULL, camera));

        /* allocate memory for a dsc read/write buffer */
        if ((dsc->buf = (char *)malloc(sizeof(char)*(DSC_BUFSIZE))) == NULL) {
                dsc_errorprint(EDSCSERRNO, __FILE__, __LINE__);
                free(dsc);
                return GP_ERROR;
        }        
        
        return dsc2_connect(camera, camera->port_info->speed);
                /* connect with selected speed */
}

/* End of dc1580.c */
