/* g3.c
 *
 * Copyright © 2003 Marcus Meissner <marcus@jet.franken.de>
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
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gphoto2-library.h>
#include <gphoto2-result.h>
#include <gphoto2-port-log.h>

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

/* channel header: 
 *
 * channel  host/client  length       data     checksum padding
 * 01 01    00 00        07 00 00 00  <7 byte> <1 byte> <fill to next 4>
 * 
 */

static int
g3_channel_read(GPPort *port, int *channel, char **buffer, int *len)
{
	unsigned char xbuf[0x800];
	int ret;

	ret = gp_port_read(port, xbuf, 0x800);
	if (ret < GP_OK) { 
		gp_log(GP_LOG_ERROR, "g3", "read error in g3_channel_read\n");
		return ret;
	}

	if ((xbuf[2] != 0xff) && (xbuf[3] != 0xff)) {
		gp_log(GP_LOG_ERROR, "g3" ,"first bytes do not match.\n");
		return GP_ERROR_IO;
	}

	*channel = xbuf[1];
	*len = xbuf[4] + (xbuf[5]<<8) + (xbuf[6]<<16) + (xbuf[7]<<24);
	if (!*buffer)
		*buffer = malloc(*len + 1);
	else
		*buffer = realloc(*buffer, *len + 1);
	memcpy(*buffer, xbuf+8, *len);
	(*buffer)[*len] = 0x00;
	return GP_OK;
}

static int
g3_channel_read_bytes(GPPort *port, int *channel, char **buffer, int expected)
{
	unsigned char *xbuf;
	int ret, len, xlen = 0;

	if (!*buffer)
		*buffer = malloc (expected);
	else
		*buffer = realloc (*buffer, expected);

	xbuf = malloc(65536 + 12);

	while (expected > 0) {
		ret = gp_port_read(port, xbuf, 65536 + 12);
		if (ret < GP_OK) {
			gp_log(GP_LOG_ERROR, "g3", "read error in g3_channel_read\n");
			return ret;
		}

		if ((xbuf[2] != 0xff) || (xbuf[3] != 0xff)) {
			gp_log(GP_LOG_ERROR, "g3", "first bytes do not match.\n");
			free(xbuf);
			return GP_ERROR_IO;
		}
		len = xbuf[4] + (xbuf[5]<<8) + (xbuf[6]<<16) + (xbuf[7]<<24);
		*channel = xbuf[1];
		if (len > expected)
			gp_log(GP_LOG_ERROR,"g3","len %d is > rest expected %d\n", len, expected);
		memcpy(*buffer+xlen, xbuf+8, len);
		expected	-= len;
		xlen		+= len;
	}
	free(xbuf);
	return GP_OK;
}


static int
g3_channel_write(GPPort *port, int channel, char *buffer, int len)
{
	unsigned char *xbuf;
	int ret, nlen;


	nlen = (4 + 4 + len + 1 +  3) & ~3;
	xbuf = calloc(nlen,1);

	xbuf[0] = 1;
	xbuf[1] = channel;
	xbuf[2] = 0;
	xbuf[3] = 0;
	xbuf[4] =  len      & 0xff;
	xbuf[5] = (len>>8)  & 0xff;
	xbuf[6] = (len>>16) & 0xff;
	xbuf[7] = (len>>24) & 0xff;
	memcpy(xbuf+8, buffer, len);
	xbuf[8+len] = 0x03;
	ret = gp_port_write( port, xbuf, nlen);
	free(xbuf);
	return ret;
}

static int
g3_ftp_command_and_reply(GPPort *port, char *cmd, char **reply) {
	int ret, channel, len;
	char *realcmd, *s;

	realcmd = malloc(strlen(cmd)+2+1);strcpy(realcmd, cmd);strcat(realcmd, "\r\n");

	gp_log(GP_LOG_DEBUG, "g3" , "sending %s", cmd);
	ret = g3_channel_write(port, 1, realcmd, strlen(realcmd));
	free(realcmd);
	if (ret < GP_OK) {
		gp_log (GP_LOG_ERROR, "g3", "ftp command write failed? %d\n", ret);
		return ret;
	}
	ret = g3_channel_read(port, &channel, reply, &len);
	if (ret < GP_OK) {
		gp_log (GP_LOG_ERROR, "g3", "ftp reply read failed? %d\n", ret);
		return ret;
	}
	s = strchr(*reply, '\r');
	if (s) *s='\0';

	gp_log(GP_LOG_DEBUG, "g3" , "reply %s", *reply);
	return GP_OK;
}

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "ricoh_g3");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Ricoh:Caplio G3");
	a.status	= GP_DRIVER_STATUS_TESTING;
	a.port		= GP_PORT_USB;
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2204;
	a.operations        = 	GP_OPERATION_NONE;
	a.file_operations   = 	GP_FILE_OPERATION_NONE;
	a.folder_operations = 	GP_FOLDER_OPERATION_NONE;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio RR30");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2202;
	gp_abilities_list_append(list, a);

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context) 
{
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	char *buf = NULL, *reply = NULL, *cmd =NULL;
	int ret = GP_OK, channel, bytes, len;

	cmd = malloc(5 + strlen(folder) + 2 + 1);
	sprintf(cmd,"CWD %s", folder);
	ret = g3_ftp_command_and_reply(camera->port, cmd, &buf);
	free(cmd);
	if (ret < GP_OK) goto out;

	cmd = malloc(5 + strlen(filename) + 2 + 1);
	sprintf(cmd,"RETR %s", filename);
	ret = g3_ftp_command_and_reply(camera->port, cmd, &buf);
	free(cmd);
	if (ret < GP_OK) goto out;

	sscanf(buf, "150 data connection for RETR.(%d)", &bytes);

	ret = g3_channel_read_bytes(camera->port, &channel, &buf, bytes); /* data */
	if (ret < GP_OK) goto out;
	ret = g3_channel_read(camera->port, &channel, &reply, &len); /* reply */
	if (ret < GP_OK) goto out;
	gp_file_set_data_and_size (file, buf, bytes);
	buf = NULL; /* now owned by libgphoto2 filesystem */

out:
	if (buf) free(buf);
	if (reply) free(reply);
	return (GP_OK);
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, CameraFile *file,
	       void *data, GPContext *context)
{
	/*Camera *camera;*/

	/*
	 * Upload the file to the camera. Use gp_file_get_data_and_size,
	 * gp_file_get_name, etc.
	 */

	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/* Delete the file from the camera. */

	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	/*Camera *camera = data;*/

	/*
	 * Delete all files in the given folder. If your camera doesn't have
	 * such a functionality, just don't implement this function.
	 */

	return (GP_OK);
}

static int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context) 
{
	gp_widget_new (GP_WIDGET_WINDOW, "Camera Configuration", window);

	/* Append your sections and widgets here. */

	return (GP_OK);
}

static int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *context) 
{
	/*
	 * Check if the widgets' values have changed. If yes, tell the camera.
	 */

	return (GP_OK);
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	/*
	 * Capture a preview and return the data in the given file (again,
	 * use gp_file_set_data_and_size, gp_file_set_mime_type, etc.).
	 * libgphoto2 assumes that previews are NOT stored on the camera's 
	 * disk. If your camera does, please delete it from the camera.
	 */

	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	/*
	 * Capture an image and tell libgphoto2 where to find it by filling
	 * out the path.
	 */

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	/*
	 * Fill out the summary with some information about the current 
	 * state of the camera (like pictures taken, etc.).
	 */

	return (GP_OK);
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	/*
	 * If you would like to tell the user some information about how 
	 * to use the camera or the driver, this is the place to do.
	 */

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Ricoh Caplio G3.\n"
			       "Marcus Meissner <marcus@jet.franken.de>\n"
			       "Reverse engineered using USB Snoopy, looking\n"
			       "at the firmware update image and wild guessing.\n"
				));

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/* Get the file info here and write it into <info> */

	return (GP_OK);
}
static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/* Set the file info here from <info> */

	return (GP_OK);
}


static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	Camera *camera = data;
	char *buf = NULL, *reply = NULL;
	char *cmd;
	int ret = GP_OK;

	cmd = malloc(6 + strlen(folder) + 1);
	strcpy(cmd, "-NLST ");
	strcat(cmd, folder);

	ret = g3_ftp_command_and_reply(camera->port, cmd, (char**)&buf);
	if (ret < GP_OK) goto out;
	if (buf[0] == '1') { /* 1xx means another reply follows */
		int n = 0, channel, len, rlen;

		ret = g3_channel_read(camera->port, &channel, &buf, &len); /* data. */
		if (ret < GP_OK) goto out;
		g3_channel_read(camera->port, &channel, &reply, &rlen); /* next reply  */
		if (ret < GP_OK) goto out;

		for (n=0;n<len/32;n++) {
			if (buf[n*32+11] == 0x10) {
				if (buf[n*32] == '.')
					continue;
				ret = gp_list_append (list, buf+n*32, NULL);
				if (ret != GP_OK) goto out;
			}
		}
		/* special case / folder, since we don't get the size data records */
		if (!strcmp("/",folder) && (len == 5)) /* "/EXT0" */
			ret = gp_list_append (list, "EXT0", NULL);
	} else {
		ret = GP_ERROR_IO;
	}
out:
	if (buf) free(buf);
	if (reply) free(reply);
	return ret;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	char *buf = NULL, *reply = NULL, *cmd;
	int ret = GP_OK;

	cmd = malloc(6 + strlen(folder) + 1);
	strcpy(cmd, "-NLST ");
	strcat(cmd, folder);
	ret = g3_ftp_command_and_reply(camera->port, cmd, (char**)&buf);
	free(cmd); cmd = NULL;
	if (ret < GP_OK) goto out;
	if (buf[0] == '1') { /* 1xx means another reply follows */
		int n = 0, channel, len, rlen;
		ret = g3_channel_read(camera->port, &channel, &buf, &len); /* data. */
		if (ret < GP_OK) goto out;
		g3_channel_read(camera->port, &channel, &reply, &rlen); /* next reply  */
		if (ret < GP_OK) goto out;

		for (n=0;n < len/32;n++) {
			if (buf[n*32+11] == 0x20) {
				CameraFileInfo  info;
				int bytes, width, height, k;

				info.file.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_NAME |
                                		GP_FILE_INFO_SIZE;
				strcpy(info.file.name, buf+n*32);
				info.file.name[8] = '.';
				strcpy(info.file.name+9, buf+n*32+8);
				info.file.name[8+1+3] = '\0';
				strcpy(info.file.type,GP_MIME_UNKNOWN);
				if (	!strcmp(info.file.name+9,"JPG") ||
					!strcmp(info.file.name+9,"jpg")
				)
					strcpy(info.file.type,GP_MIME_JPEG);
				if (	!strcmp(info.file.name+9,"AVI") ||
					!strcmp(info.file.name+9,"avi")
				)
					strcpy(info.file.type,GP_MIME_AVI);

				if (	!strcmp(info.file.name+9,"MTA") ||
					!strcmp(info.file.name+9,"mta")
				)
					strcpy(info.file.type,"text/plain");

				info.file.size =(((unsigned char*)buf)[n*32+31])	|
				 		(((unsigned char*)buf)[n*32+30]<<8)	|
				 		(((unsigned char*)buf)[n*32+29]<<16)	|
				 		(((unsigned char*)buf)[n*32+28]<<24);
				ret = gp_filesystem_append (fs, folder, info.file.name, context);
				cmd = malloc(6+strlen(folder)+1+strlen(info.file.name)+1);
				sprintf(cmd, "-FDAT %s/%s", folder,info.file.name);
				g3_ftp_command_and_reply(camera->port, cmd, &reply);
				if (ret < GP_OK) goto out;

				sprintf(cmd, "-INFO %s/%s", folder,info.file.name);
				g3_ftp_command_and_reply(camera->port, cmd, &reply);
				if (ret < GP_OK) goto out;

				/* 200 1330313 byte W=2048 H=1536 K=0 for -INFO */
				if (sscanf(reply, "200 %d byte W=%d H=%d K=%d for -INFO", &bytes, &width, &height , &k)) {
					if (width != 0 && height != 0) {
						info.file.fields |= GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;
						info.file.height = height;
						info.file.width = width;
					}
					if (k != 0)
						gp_log(GP_LOG_ERROR, "g3", "k is %d for %s/%s\n", k, folder,info.file.name);
					if (bytes != info.file.size)
						gp_log(GP_LOG_ERROR, "g3", "size is %d from -INFO, but %d from listing!\n", bytes, info.file.size);
				}
				ret = gp_filesystem_set_info_noop(fs, folder, info, context);
				if (ret < GP_OK) goto out;
			}
		}
	} else {
		ret = GP_ERROR_IO;
	}
out:
	if (buf) free(buf);
	if (reply) free(reply);
	if (cmd) free(cmd);
	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context) 
{
	char *buf;
	GPPortSettings settings;

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->get_config           = camera_config_get;
        camera->functions->set_config           = camera_config_set;
        camera->functions->capture              = camera_capture;
        camera->functions->capture_preview      = camera_capture_preview;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func,
				      folder_list_func, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, set_info_func,
				      camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      delete_file_func, camera);
	gp_filesystem_set_folder_funcs (camera->fs, put_file_func,
					delete_all_func, NULL, NULL, camera);


        gp_port_get_settings(camera->port, &settings);
	settings.usb.inep = 0x81;
	settings.usb.outep = 0x02;
	settings.usb.intep = 0x83;
        gp_port_set_settings(camera->port, settings);
	/*
	 * The port is already provided with camera->port (and
	 * already open). You just have to use functions like
	 * gp_port_timeout_set, gp_port_settings_get, gp_port_settings_set.
	 */
	
	/*
	 * Once you have configured the port, you should check if a 
	 * connection to the camera can be established.
	 */

	buf = NULL;
	g3_ftp_command_and_reply(camera->port, "-VER", &buf);
	g3_ftp_command_and_reply(camera->port, "-RTST", &buf); /* RTC test */
	g3_ftp_command_and_reply(camera->port, "-TIME", &buf);
	g3_ftp_command_and_reply(camera->port, "-GCID", &buf);
	g3_ftp_command_and_reply(camera->port, "-GSID", &buf);
	g3_ftp_command_and_reply(camera->port, "-GTPN", &buf);
	g3_ftp_command_and_reply(camera->port, "-DCAP /EXT0", &buf);
	return (GP_OK);
}
