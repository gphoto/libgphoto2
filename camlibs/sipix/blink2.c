/* blink2.c
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

#include <gphoto2-library.h>
#include <gphoto2-port-log.h>
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

#define BLINK2_GET_NUMPICS	0x08
#define BLINK2_GET_DIR		0x0d
#define BLINK2_GET_MEMORY	0x0a

static int
blink2_getnumpics(
    GPPort *port, GPContext *context,int *numpics
) {
    char buf[6];
    int ret;

    ret = gp_port_usb_msg_read(port, BLINK2_GET_NUMPICS, 0x03, 0, buf, 6);
    if (ret<GP_OK)
	return ret;
    *numpics = (buf[0]<<8) | buf[1];
    return GP_OK;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int	i, ret, numpics;
	unsigned char	*xbuf, buf[8];

	ret = blink2_getnumpics( camera->port, context, &numpics );
	if (ret < GP_OK) return ret;

	xbuf = malloc(8*(1+numpics));
	ret = gp_port_usb_msg_read( camera->port,BLINK2_GET_DIR,0x03,0,buf,8 );
	if (ret < GP_OK)  {
		free(xbuf);
		return ret;
	}
	ret = gp_port_read( camera->port, xbuf, 8*(1+numpics));
	if (ret < GP_OK) {
		free(xbuf);
		return ret;
	}
	for ( i=0; i < numpics; i++) {
		char name[20];
		if (xbuf[8*(i+1)])
			sprintf( name, "image%04d.avi", i);
		else
			sprintf( name, "image%04d.jpg", i);
		gp_list_append( list, name, NULL);
	}
	free(xbuf);
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
        int image_no, result;
	int	i, ret, numpics;
	unsigned char	*xbuf, buf[8];

	struct xaddr {
		unsigned int type, start,len;
	} *addrs;

	ret = blink2_getnumpics( camera->port, context, &numpics );
	if (ret < GP_OK) return ret;
	gp_log(GP_LOG_DEBUG, "blink2","numpics is %d", numpics);
	xbuf = malloc(8*(1+numpics));
	if (!xbuf)
		return GP_ERROR_NO_MEMORY;
	addrs = (struct xaddr*)malloc(sizeof(struct xaddr)*numpics);
	if (!addrs) {
		free(xbuf);
		return GP_ERROR_NO_MEMORY;
	}
	ret = gp_port_usb_msg_read( camera->port,BLINK2_GET_DIR,0x03,0,buf,8 );
	if (ret < GP_OK) {
		free(addrs);
		free(xbuf);
		return ret;
	}
	ret = gp_port_read( camera->port, xbuf, 8*(1+numpics));
	if (ret < GP_OK) {
		free(addrs);
		free(xbuf);
		return ret;
	}
	for ( i=0; i < numpics; i++) {
		int end, start;

		start = (xbuf[8*i+ 5] << 16) |
			(xbuf[8*i+ 6] <<  8) |
			(xbuf[8*i+ 7]);
		end = 	(xbuf[8*i+13] << 16) |
			(xbuf[8*i+14] <<  8) |
			(xbuf[8*i+15]);
		addrs[i].start = start;
		addrs[i].len = (end-start)/4;
		addrs[i].type = xbuf[8*(i+1)];
		gp_log(GP_LOG_DEBUG, "blink2","%d - %d", start, (end-start)/4);
	}
	free(xbuf);

        image_no = gp_filesystem_number(fs, folder, filename, context);
        if(image_no < 0) {
		free(addrs);
                return image_no;
	}
        gp_file_set_name (file, filename);
        switch (type) {
        case GP_FILE_TYPE_RAW:
        case GP_FILE_TYPE_NORMAL: {
		char buf2[4096];
		unsigned int start, len;
		int curread;
		memset( buf, 0, sizeof(buf));
		
		if (addrs[image_no].type)
        		gp_file_set_mime_type (file, GP_MIME_AVI);
		else
        		gp_file_set_mime_type (file, GP_MIME_JPEG);

		start = addrs[image_no].start;
		len = addrs[image_no].len;
		buf[0] = (start >> 24) & 0xff;
		buf[1] = (start >> 16) & 0xff;
		buf[2] = (start >>  8) & 0xff;
		buf[3] =  start        & 0xff;
		buf[4] = (len >> 24) & 0xff;
		buf[5] = (len >> 16) & 0xff;
		buf[6] = (len >>  8) & 0xff;
		buf[7] =  len        & 0xff;
		result = gp_port_usb_msg_write(camera->port,BLINK2_GET_MEMORY,0x03,0,buf,8);
		if (result < GP_OK)
			break;
		len = len*8;
		do {
			curread = 4096;
			if (curread > len) curread = len;
			curread = gp_port_read( camera->port, buf2, curread);
			if (curread < GP_OK) {
				result = GP_OK;
				break;
			}
			len -= curread;
			result = gp_file_append( file, buf2, curread);
			if (result < GP_OK)
				break;
		} while (len>0);
                break;
        }
	default:
                result = GP_ERROR_NOT_SUPPORTED;
		break;
        }
	free(addrs);
        if (result < 0)
                return result;
        return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "SiPix:Blink 2");
	a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port     		= GP_PORT_USB;
	a.speed[0] 		= 0;
	a.usb_vendor		= 0x0c77;
	a.usb_product		= 0x1011;
	a.operations		=  GP_OPERATION_NONE;
	a.file_operations	=  GP_FILE_OPERATION_NONE;
	a.folder_operations	=  GP_FOLDER_OPERATION_NONE;
	gp_abilities_list_append(list, a);

	a.usb_product		= 0x1010;
	strcpy(a.model, "SiPix:Snap");
	gp_abilities_list_append(list, a);

	return (GP_OK);
}

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "SiPix Blink2");
	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context) 
{
	char buf[8];
	int ret;
	GPPortSettings settings;

	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);
	gp_port_get_settings( camera->port, &settings);
	settings.usb.interface = 0;
	ret = gp_port_set_settings( camera->port, settings);
	if (ret < GP_OK) return ret;
	ret = gp_port_usb_msg_read( camera->port, 0x18, 0x0300, 0x0000, buf, 6);
	if (ret < GP_OK)
		return ret;
	ret = gp_port_usb_msg_read( camera->port, 0x04, 0x0300, 0x0000, buf, 8);
	if (ret < GP_OK)
		return ret;
	return GP_OK;
}
