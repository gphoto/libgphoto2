/* blink2.c
 *
 * Copyright 2003 Marcus Meissner <marcus@jet.franken.de>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_LIBJPEG
# include <jpeglib.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-result.h>

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

#define BLINK2_GET_NUMPICS		0x08
#define BLINK2_GET_MEMORY		0x0a
#define BLINK2_GET_DIR			0x0d
#define BLINK2_INIT_CAPTURE		0x0e
#define BLINK2_DELETE_ALL		0x12
#define BLINK2_CHECK_CAPTURE_FINISH	0x16
#define BLINK2_SET_EXPOSURE_COUNT	0x17
#define BLINK2_GET_FIRMWARE_ID		0x18

static int
blink2_getnumpics(
    GPPort *port, GPContext *context,int *numpics
) {
    char buf[6];
    int ret;
    
    ret = gp_port_usb_msg_read(port, BLINK2_GET_NUMPICS, 0x03, 0, buf, 2);
    if (ret<GP_OK)
	return ret;
    *numpics = (buf[0]<<8) | buf[1];
    return GP_OK;
}

#ifdef HAVE_LIBJPEG
/* for the jpeg decompressor source manager. */
static void _jpeg_init_source(j_decompress_ptr cinfo) { }

static boolean _jpeg_fill_input_buffer(j_decompress_ptr cinfo) {
    fprintf(stderr,"(), should not get here.\n");
    return FALSE;
}

static void _jpeg_skip_input_data(j_decompress_ptr cinfo,long num_bytes) {
    fprintf(stderr,"(%ld), should not get here.\n",num_bytes);
}

static boolean _jpeg_resync_to_restart(j_decompress_ptr cinfo, int desired) {
    fprintf(stderr,"(desired=%d), should not get here.\n",desired);
    return FALSE;
}
static void _jpeg_term_source(j_decompress_ptr cinfo) { }
#endif

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int	i, ret, numpics, bytes;
	unsigned char	*xbuf, buf[8];

	ret = blink2_getnumpics( camera->port, context, &numpics );
	if (ret < GP_OK) return ret;

	bytes = ((8*(1+numpics))+0x3f) & ~0x3f;
	xbuf = malloc(bytes);
	ret = gp_port_usb_msg_read( camera->port,BLINK2_GET_DIR,0x03,0,(char*)buf,1 );
	if (ret < GP_OK)  {
		free(xbuf);
		return ret;
	}
	ret = gp_port_read( camera->port, (char*)xbuf, bytes);
	if (ret < GP_OK) {
		free(xbuf);
		return ret;
	}
	for ( i=0; i < numpics; i++) {
		char name[20];
		if (xbuf[8*(i+1)])
			sprintf( name, "image%04d.avi", i);
		else
			sprintf( name, "image%04d.pnm", i);
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
	int	i, ret, numpics, bytes;
	unsigned char	*xbuf, buf[8];

	struct xaddr {
		unsigned int type, start,len;
	} *addrs;

	ret = blink2_getnumpics( camera->port, context, &numpics );
	if (ret < GP_OK) return ret;
	gp_log(GP_LOG_DEBUG, "blink2","numpics is %d", numpics);
	bytes = ((8*(1+numpics))+0x3f) & ~0x3f;
	xbuf = malloc(bytes);
	if (!xbuf)
		return GP_ERROR_NO_MEMORY;
	addrs = (struct xaddr*)malloc(sizeof(struct xaddr)*numpics);
	if (!addrs) {
		free(xbuf);
		return GP_ERROR_NO_MEMORY;
	}
	ret = gp_port_usb_msg_read( camera->port,BLINK2_GET_DIR,0x03,0,(char*)buf,1 );
	if (ret < GP_OK) {
		free(addrs);
		free(xbuf);
		return ret;
	}
	ret = gp_port_read (camera->port, (char*)xbuf, bytes);
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
        switch (type) {
        case GP_FILE_TYPE_NORMAL:
#ifdef HAVE_LIBJPEG
{
		char *convline,*convline2,*rawline;
		unsigned char	*jpegdata;
		unsigned int start, len, pitch;
		int curread;
		struct jpeg_decompress_struct	dinfo;
		struct jpeg_source_mgr		xjsm;
		struct jpeg_error_mgr		jerr;

		memset( buf, 0, sizeof(buf));
		if (addrs[image_no].type)
        		gp_file_set_mime_type (file, GP_MIME_AVI);
		else
        		gp_file_set_mime_type (file, GP_MIME_JPEG);
		start = addrs[image_no].start;
		len = addrs[image_no].len;
		jpegdata = (unsigned char*)malloc (len*8);
		if (!jpegdata) {
			result = GP_ERROR_NO_MEMORY;
			break;
		}
		buf[0] = (start >> 24) & 0xff;
		buf[1] = (start >> 16) & 0xff;
		buf[2] = (start >>  8) & 0xff;
		buf[3] =  start        & 0xff;
		buf[4] = (len >> 24) & 0xff;
		buf[5] = (len >> 16) & 0xff;
		buf[6] = (len >>  8) & 0xff;
		buf[7] =  len        & 0xff;
		result = gp_port_usb_msg_write (camera->port,BLINK2_GET_MEMORY,0x03,0,(char*)buf,8);
		if (result < GP_OK) {
			free (jpegdata);
			break;
		}
		len	*= 8;
		curread  = 0;
		do {
			int res;
			res = gp_port_read (camera->port, (char*)(jpegdata+curread), len );
			if (res < GP_OK) {
				result = GP_OK;
				break;
			}
			curread += res;
		} while (curread<=len);

		memset( &dinfo, 0, sizeof(dinfo));
		xjsm.next_input_byte	= jpegdata;
		xjsm.bytes_in_buffer	= len;
		xjsm.init_source	= _jpeg_init_source;
		xjsm.fill_input_buffer	= _jpeg_fill_input_buffer;
		xjsm.skip_input_data	= _jpeg_skip_input_data;
		xjsm.resync_to_restart	= _jpeg_resync_to_restart;
		xjsm.term_source	= _jpeg_term_source;
		dinfo.err		= jpeg_std_error(&jerr);
		jpeg_create_decompress(&dinfo);
		dinfo.src		= &xjsm;
		ret = jpeg_read_header(&dinfo,TRUE);
		if (ret != JPEG_HEADER_OK) {
			jpeg_destroy_decompress(&dinfo);
			free (jpegdata);
			break;
		}
		dinfo.out_color_space = JCS_RGB;
		jpeg_start_decompress(&dinfo);

		pitch = (dinfo.output_width*dinfo.output_components+3)&~3;

		if (ret != JPEG_HEADER_OK)
			return GP_ERROR;

		rawline = malloc(pitch);
		convline = malloc(dinfo.output_width*2*3);
		convline2 = malloc(dinfo.output_width*2*2*3);
		{
			char foo[30];
			sprintf(foo,"P6\n%d %d 255\n",dinfo.output_width, dinfo.output_height*2);
			gp_file_append (file, foo, strlen(foo));
		}
		for (i = 0; i < dinfo.output_height ; i++ ) {
			int j;
			JSAMPROW row[1];
			JSAMPARRAY arr = row;

			row[0] = (JSAMPROW)rawline;
			jpeg_read_scanlines(&dinfo,arr,1);

			memcpy(convline+((dinfo.output_width/16-1)*16+8)*3, rawline+((dinfo.output_width/16-1)*16+8)*3, 8*3);
			memcpy(convline+pitch/2, rawline, 8*3);
			for (j=0;j<(dinfo.output_width/16-1);j++) {
				if ((j&1) == 0)
					memcpy(convline+((j/2)*16)*3,rawline+(j*16+8)*3,16*3);
				else
					memcpy(convline+pitch/2+((j/2)*16+8)*3,rawline+(j*16+8)*3,16*3);
			}
			for (j=0;j<dinfo.output_width*2;j++) {
				memcpy(convline2+ j*2*3   , convline+j*3,3);
				memcpy(convline2+(j*2+1)*3, convline+j*3,3);
			}
			gp_file_append (file, convline2, pitch*2);
		}
		free(convline2);
		free(convline);
		free(rawline);
		free(jpegdata);
        	gp_file_set_mime_type (file, GP_MIME_PPM);
		jpeg_destroy_decompress(&dinfo);
		break;
	}
#else
	/* fall through to raw mode if no libjpeg */
#endif
        case GP_FILE_TYPE_RAW: {
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
		result = gp_port_usb_msg_write(camera->port,BLINK2_GET_MEMORY,0x03,0,(char*)buf,8);
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

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
                 GPContext *context)
{
	Camera *camera = data;
	int ret;
	char buf[1];

	ret = gp_port_usb_msg_read( camera->port, BLINK2_DELETE_ALL, 0x03, 0, buf, 1);
	if (ret < GP_OK)
		return ret;
        return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
                GPContext *context)
{
	int oldnumpics, numpics, ret;
	char buf[1];
	
	ret = blink2_getnumpics (camera->port, context, &oldnumpics);
	if (ret < GP_OK)
		return ret;

	ret = gp_port_usb_msg_read( camera->port, BLINK2_INIT_CAPTURE, 0x03, 0, buf, 1);
	if (ret < GP_OK)
		return ret;
	do {
		ret = gp_port_usb_msg_read( camera->port, BLINK2_CHECK_CAPTURE_FINISH, 0x03, 0, buf, 1);
		if (ret < GP_OK)
			return ret;
		sleep(1);
	} while (buf[0] == 0x00);

	ret = blink2_getnumpics (camera->port, context, &numpics);
	if (ret < GP_OK)
		return ret;
	if (numpics == oldnumpics)
		return (GP_ERROR);
        strcpy (path->folder,"/");
        sprintf (path->name,"image%04d.pnm",numpics-1);
        return (GP_OK);
}


int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model,	"SiPix:Blink 2");
	a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port     		= GP_PORT_USB;
	a.speed[0] 		= 0;
	a.usb_vendor		= 0x0c77;
	a.usb_product		= 0x1011;
	a.operations		=  GP_OPERATION_CAPTURE_IMAGE;
	a.file_operations	=  GP_FILE_OPERATION_NONE;
	a.folder_operations	=  GP_FOLDER_OPERATION_DELETE_ALL;
	gp_abilities_list_append(list, a);

	a.usb_product		= 0x1010;
	strcpy(a.model, "SiPix:Snap");
	gp_abilities_list_append(list, a);

	a.usb_product		= 0x1015;
	strcpy(a.model, "SiPix:CAMeleon");
	gp_abilities_list_append(list, a);

	return (GP_OK);
}

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "SiPix Blink2");
	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.delete_all_func = delete_all_func
};

int
camera_init (Camera *camera, GPContext *context) 
{
	char buf[6];
	int ret;
	GPPortSettings settings;

        camera->functions->capture              = camera_capture;

	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
	gp_port_get_settings( camera->port, &settings);
	settings.usb.config = 1;
	settings.usb.interface = 0;
	settings.usb.altsetting = 0;
	ret = gp_port_set_settings (camera->port, settings);
	if (ret < GP_OK) return ret;

	ret = gp_port_usb_msg_read( camera->port, BLINK2_GET_FIRMWARE_ID, 0x03, 0, buf, 6);
	if (ret < GP_OK)
		return ret;
	ret = gp_port_usb_msg_read( camera->port, 0x04, 0x03, 0, buf, 1);
	if (ret < GP_OK)
		return ret;
	return GP_OK;
}
