/* qm150.c
 *
 * Copyright © 2003 Marcus Meissner <marcus@jet.franken.de>
 *                  Aurélien Croc (AP²C) <programming@ap2c.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Modified by Aurélien Croc (AP²C) <programming@ap2c.com>
 * In particular : fix some bugs, and implementation of advanced 
 * functions : delete some images, delete all images, capture new image
 *
 */
#include "config.h"

#include <string.h>
#include <stdio.h>
#include <gphoto2-library.h>
#include <gphoto2-result.h>

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


#define PING		0x58
#define CAPTURE		0x52
#define GETCAMINFO	0x53
#define IMAGEINFO	0x49
#define SETSPEED	0x42
#define IMAGE		0x46
#define GETDATA		0x47
#define GETTHUMBNAIL	0x54
#define ERAZEDATA	0x45

#define IMAGE		0x46
#define NOSE		0x30

#define ESC		0x1b
#define ACK		0x06
#define EOT		0x04
#define CAMERAERROR	0x15

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "konica qm150");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Konica:Q-M150");
	a.status	= GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port		= GP_PORT_SERIAL;
	a.speed[0]	= 115200;
	a.speed[1]	= 0;
	a.operations        = 	GP_CAPTURE_IMAGE;
	a.file_operations   = 	GP_FILE_OPERATION_DELETE;
	a.folder_operations = 	GP_FOLDER_OPERATION_DELETE_ALL;
	gp_abilities_list_append(list, a);
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	unsigned char	cmd[7], buf[512];
	int	image_no, i, len, ret;
		
        image_no = gp_filesystem_number(fs, folder, filename, context);
	if (image_no < 0) return image_no;

	image_no++;
	cmd[0] = ESC;
	cmd[1] = IMAGEINFO;
	cmd[2] = 0x30 + ((image_no/1000)%10);
	cmd[3] = 0x30 + ((image_no/100 )%10);
	cmd[4] = 0x30 + ((image_no/10  )%10);
	cmd[5] = 0x30 + ( image_no      %10);

	ret = gp_port_write (camera->port, cmd, 6); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 256);
	if (ret<GP_OK) return ret;

	len =	(buf[8]	<< 24)| (buf[9]	<< 16)| (buf[10]<<  8)| buf[11];

	cmd[0] = ESC;
	cmd[1] = GETDATA;
	cmd[2] = IMAGE;
	cmd[3] = 0x30 + ((image_no/1000)%10);
	cmd[4] = 0x30 + ((image_no/100 )%10);
	cmd[5] = 0x30 + ((image_no/10  )%10);
	cmd[6] = 0x30 + ( image_no      %10);

	ret = gp_port_write (camera->port, cmd, 7);
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 1);
	if (ret < GP_OK) return ret;
	if (buf[0] == CAMERAERROR) {
		gp_context_error(context, _("This image doesn't exist."));
		return (GP_ERROR);
	}

	gp_file_set_name (file, filename);
	gp_file_set_mime_type (file, GP_MIME_JPEG);
	for (i=0; i<(len+511)/512 ; i++) {
		unsigned char csum;
		int xret;

		xret = gp_port_read (camera->port, buf, 512);
		if (xret < GP_OK) return xret;
		ret = gp_port_read (camera->port, &csum, 1);
		if (ret < GP_OK) return ret;

		ret = gp_file_append (file, buf, xret);
		if (ret < GP_OK) return ret;

		/* acknowledge the packet */
		cmd[0] = ACK; 
		ret = gp_port_write (camera->port, cmd, 1);
		if (ret < GP_OK) return ret;

		ret = gp_port_read (camera->port, buf, 1);
		if (ret < GP_OK) return ret;
		if (buf[0] == EOT) break;

	}
	/* acknowledge the packet */
	cmd[0] = ACK; 
	ret = gp_port_write (camera->port, cmd, 1);
	if (ret < GP_OK) return ret;
	
	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned char	cmd[7], buf[512];
	int	image_no, len, ret;
		
        image_no = gp_filesystem_number(fs, folder, filename, context);
	if (image_no < 0) return image_no;

	image_no++;
	cmd[0] = ESC;
	cmd[1] = IMAGEINFO;
	cmd[2] = 0x30 + ((image_no/1000)%10);
	cmd[3] = 0x30 + ((image_no/100 )%10);
	cmd[4] = 0x30 + ((image_no/10  )%10);
	cmd[5] = 0x30 + ( image_no      %10);
	ret = gp_port_write (camera->port, cmd, 6); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 256);
	if (ret<GP_OK) return ret;

	if (cmd[17] == 0x1) {
		gp_context_error(context, _("Image %s is delete protected."),filename);
		return (GP_ERROR);
	}
	
	len =	(buf[8]	<< 24)| (buf[9]	<< 16)| (buf[10]<<  8)| buf[11];
	cmd[0] = ESC;
	cmd[1] = ERAZEDATA;
	cmd[2] = IMAGE;
	cmd[3] = 0x30 + ((image_no/1000)%10);
	cmd[4] = 0x30 + ((image_no/100 )%10);
	cmd[5] = 0x30 + ((image_no/10  )%10);
	cmd[6] = 0x30 + ( image_no      %10);
	ret = gp_port_write (camera->port, cmd, 7);
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 1);
	if (ret<GP_OK) return ret;
	if (buf[0] != ACK) {
		gp_context_error(context, _("Can't delete image %s."),filename);
		return (GP_ERROR);
	}	
	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		GPContext *context)
{
	unsigned char	cmd[7], buf[512];
	int	ret;
	Camera *camera = data;
	cmd[0] = ESC;
	cmd[1] = ERAZEDATA;
	cmd[2] = IMAGE;
	cmd[3] = 0x30;
	cmd[4] = 0x30;
	cmd[5] = 0x30;
	cmd[6] = 0x30;
	ret = gp_port_write (camera->port, cmd, 7);
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 1);
	if (ret<GP_OK) return ret;
	if (buf[0] != ACK) {
		gp_context_error(context, _("Can't delete all images."));
		return (GP_ERROR);
	}	
	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned char	cmd[7], buf[512];
	int	ret;

	/* Get the file info here and write it into <info> */
	cmd[0] = ESC;
	cmd[1] = GETCAMINFO;
	ret = gp_port_write (camera->port, cmd, 2); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 256);
	if (ret<GP_OK) return ret;

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned char	cmd[2], buf[256];
	int	num, ret;
		
	cmd[0] = ESC;
	cmd[1] = GETCAMINFO;
	ret = gp_port_write (camera->port, cmd, 2); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 256);
	if (ret<GP_OK) return ret;
	num =	(buf[0x10] << 24) | 
		(buf[0x11] << 16) |
		(buf[0x12] << 8)  |
		(buf[0x13]);
	gp_list_populate (list, "image%04d.jpg", num);
	return GP_OK;
}

static int
camera_capture (Camera* camera, CameraCaptureType type, CameraFilePath* path,
		GPContext *context)
{
	unsigned char	cmd[2], buf[256];
	int	image_id, ret;

	cmd[0] = ESC;
	cmd[1] = GETCAMINFO;
	ret = gp_port_write (camera->port, cmd, 2); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 256);
	cmd[0] = ESC;
	cmd[1] = CAPTURE;
	cmd[2] = NOSE;
	ret = gp_port_write (camera->port, cmd, 3);
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 1);
	if (ret<GP_OK) return ret;
	if (buf[0] == CAMERAERROR) {
		gp_context_error(context, _("You must be on record mode to capture image."));
		return (GP_ERROR);
	}
	if (buf[0] != ACK) {
		gp_context_error(context, _("Can't capture picture."));
		return (GP_ERROR);
	}
	cmd[0] = ESC;
	cmd[1] = GETCAMINFO;
	ret = gp_port_write (camera->port, cmd, 2); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 256);


	/* Find the name of the image captured */
	cmd[0] = ESC;
	cmd[1] = GETCAMINFO;
	ret = gp_port_write (camera->port, cmd, 2); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 256);
	if (ret<GP_OK) return ret;
	image_id =	(buf[0x10] << 24) | 
			(buf[0x11] << 16) |
			(buf[0x12] << 8)  |
			(buf[0x13]);
	image_id++;
	sprintf (path->name, "image%04d.jpg", (int) image_id);
	strcpy (path->folder, "/");
	gp_filesystem_append (camera->fs, path->folder,
				path->name, context);
	return (GP_OK);
}

static int
camera_about (Camera* camera, CameraText* about, GPContext *context)
{
	unsigned char	cmd[2], buf[256];
	int	ret;
		
	cmd[0] = ESC;
	cmd[1] = GETCAMINFO;
	ret = gp_port_write (camera->port, cmd, 2); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 256);
	if (ret<GP_OK) return ret;

	return (GP_OK);
}

static int
k_ping (GPPort *port) {
	char	cmd[2], buf[1];
	int	ret;
		
	cmd[0] = ESC;
	cmd[1] = PING;
	ret = gp_port_write (port, cmd, 2); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (port, buf, 1);
	if (ret<GP_OK) return ret;
	if (buf[0] != ACK) return GP_ERROR;
	return GP_OK;
}

int
camera_init (Camera *camera, GPContext *context) 
{
	GPPortSettings settings;
	int speeds[] = { 115200, 9600, 19200, 38400, 57600, 115200 };
	int ret, i;
	char cmd[3], buf[1];

	/* First, set up all the function pointers. */
	camera->functions->capture              = camera_capture;
	/* camera->functions->about		= camera_about; */


	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, delete_file_func, camera);
	gp_filesystem_set_folder_funcs (camera->fs, NULL, delete_all_func, NULL, NULL, camera);

	/*
	 * The port is already provided with camera->port (and
	 * already open). You just have to use functions like
	 * gp_port_timeout_set, gp_port_settings_get, gp_port_settings_set.
	 */
	gp_port_get_settings (camera->port, &settings);
	settings.serial.speed	= 115200;
	settings.serial.bits	= 8;
	settings.serial.stopbits= 1;
	settings.serial.parity	= 0;
	gp_port_set_settings (camera->port, settings);
	/*
	 * Once you have configured the port, you should check if a 
	 * connection to the camera can be established.
	 */
	for (i=0;i<sizeof(speeds)/sizeof(speeds[0]);i++) {
		gp_port_get_settings (camera->port, &settings);
		settings.serial.speed	= speeds[i];
		gp_port_set_settings (camera->port, settings);
		if (GP_OK<=k_ping (camera->port))
			break;
	}
	if (i == sizeof(speeds)/sizeof(speeds[0]))
		return GP_ERROR;
	cmd[0] = ESC;
	cmd[1] = SETSPEED;
	cmd[2] = 0x30 + 4; /* speed nr 4:(9600, 19200, 38400, 57600, 115200) */
	ret = gp_port_write (camera->port, cmd, 3); 
	if (ret<GP_OK) return ret;
	ret = gp_port_read (camera->port, buf, 1);
	if (ret<GP_OK) return ret;
	if (buf[0] != ACK) return GP_ERROR;
	gp_port_get_settings (camera->port, &settings);
	settings.serial.speed	= 115200;
	gp_port_set_settings (camera->port, settings);
	return GP_OK;
}
