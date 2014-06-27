/* web2.c
 *
 * Copyright 2002 Marcus Meissner <marcus@jet.franken.de>
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

#include <gphoto2/gphoto2-library.h>
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

#define WEB2_SELECT_PICTURE	0xb2
#define WEB2_GET_NUMPICS	0xb6

#define WEB2_OPEN_CAMERA	0xd7
#define WEB2_CLOSE_CAMERA	0xd8

#define WEB2_GET_EXIF		0xe5
#define WEB2_GET_IMAGE		0x93
#define WEB2_GET_THUMBNAIL	0x9b

#define WEB2_GET_DIRENTRY	0xb9

#define WEB2_GET_PICINFO	0xad

static int
web2_command(
    GPPort *port, int iswrite, int cmd, int param, int index, char *data, int datasize
) {
    int ret;
    if (iswrite)
	ret = gp_port_usb_msg_write(port,0,(cmd<<8)|param,index,data,datasize);
    else
	ret = gp_port_usb_msg_read(port,0,(cmd<<8)|param,index,data,datasize);
#if 0
    fprintf(stderr,"cmd %x, param %d, ret is %d\n",cmd, param, ret);
    if ((ret > 0) && data) {
	for (i=0;i<datasize;i++) { fprintf(stderr,"%02x ",data[i]); }
	fprintf(stderr,"\n");
    }
#endif
    if (ret>=GP_OK) 
	ret=GP_OK;
    return ret;
}

static int
web2_init(GPPort *port, GPContext *context) {
    return web2_command(port, 1, WEB2_OPEN_CAMERA, 1, 0, NULL, 0);
}

static int
web2_exit(GPPort *port, GPContext *context) {
    return web2_command(port, 1, WEB2_CLOSE_CAMERA, 0, 0, NULL, 0);
}

static int
web2_select_picture(GPPort *port, GPContext *context, int picnum) {
    char cmdbuf[2];

    picnum++;
    cmdbuf[0] = picnum & 0xff;
    cmdbuf[1] = (picnum>>8) & 0xff;
    return web2_command(port, 1, WEB2_SELECT_PICTURE, 0, 0, cmdbuf, 2);
}

static const unsigned char ExifHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};

static int
web2_getexif(GPPort *port, GPContext *context, CameraFile *file)
{
    int ret, i;
    char xbuf[16384];

    ret = web2_command(port, 1, WEB2_GET_EXIF, 0x00, 0, NULL, 0);
    if (ret!=GP_OK)
	return ret;
    gp_file_append(file, (char*)ExifHeader, sizeof(ExifHeader));
    ret = gp_port_read(port, xbuf, sizeof(xbuf));
    if (ret < GP_OK) {
	gp_file_clean(file);
	return ret;
    }
    for (i=0;i<ret;i+=2) {
	unsigned char tmp = xbuf[i];

	xbuf[i] = xbuf[i+1];
	xbuf[i+1] = tmp;
    }
    gp_file_append(file, xbuf, ret);
    return GP_OK;
}

static int
web2_getthumb(GPPort *port, GPContext *context, CameraFile *file)
{
    int ret, i;
    unsigned char buf[16384];

    ret = web2_command(port, 1, WEB2_GET_THUMBNAIL, 0x00, 0, NULL, 0);
    if (ret!=GP_OK)
	return ret;
    ret = gp_port_read(port, (char*)buf, sizeof(buf));
    if (ret < GP_OK)
	return ret;
    for (i=0;i<ret;i+=2) {
	unsigned char tmp = buf[i];
	buf[i] = buf[i+1];
	buf[i+1] = tmp;
    }
    gp_file_append(file, (char*)buf, ret);
    return GP_OK;
}

static int
web2_get_file_info(GPPort *port, GPContext *context, char *name, int *filesize) {
    unsigned char cmdbuf[26];
    int i, ret;
    ret = web2_command(port, 0, WEB2_GET_DIRENTRY, 0, 0, (char*)cmdbuf, 26);

    /* flip filename bytes to be in correct order */
    for (i=2;i<16;i++)
	name[i-2] = cmdbuf[i^1];
    *filesize =	(cmdbuf[18]      ) | (cmdbuf[19] <<  8) | \
		(cmdbuf[20] << 16) | (cmdbuf[21] << 24);
    /* 22-25 unused? */
    return ret;
}


static int
web2_getpicture(GPPort *port, GPContext *context, CameraFile *file)
{
    char	xbuf[8192];
    int		wascanceled = 0, id, ret, curread = 0, size;
    char 	fn[20];

    ret = web2_get_file_info(port, context, fn, &size);
    if (ret!=GP_OK)
	return ret;

    id = gp_context_progress_start(context, size, _("Downloading image..."));
    size++; /* sometimes we need to read a byte more to settle the camera */

    /* can be called with 0x10 and 0x1 flag. But what do these do? */
    ret = web2_command(port, 1, WEB2_GET_IMAGE, 0, 0, 0, 0);
    if (ret!=GP_OK)
	return ret;

    while (curread < size) {
	int toread = size-curread;
	if (toread > sizeof(xbuf))
	    toread = sizeof(xbuf);
	ret = gp_port_read(port, xbuf, toread);
	if (ret < GP_OK)
	    return ret;
	gp_file_append(file,xbuf,ret);
	curread+=ret;
	gp_context_progress_update(context, id, curread);
	if (toread!=ret)
	    break;
	if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
	    /* We need to read the rest too or the communication is in 
	     * an undefined state. */
	    wascanceled = 1;
	}
    }
    gp_context_progress_stop(context, id);
    if (wascanceled)
	return GP_ERROR_CANCEL;
    return GP_OK;
}

static int
web2_get_picture_info(
    GPPort *port, GPContext *context,
    int picnum,
    int *perc, int *incamera, int *flags, int *unk
) {
    char cmdbuf[8];
    int ret;

    /* picnum is not in param, but in the control message index! */
    ret = web2_command(port, 0, WEB2_GET_PICINFO, 0, picnum+1, cmdbuf, 8);
    if (ret != GP_OK)
	return ret;
    *perc = cmdbuf[0] | (cmdbuf[1] << 8);
    *incamera = cmdbuf[2] | (cmdbuf[3] << 8);
    *flags = cmdbuf[4] | (cmdbuf[5] << 8);
    *unk = cmdbuf[6] | (cmdbuf[7] << 8);
    return GP_OK;
}

/********************** Unknown and/or incomplete ******************/

/* Called after 0xad and 0xb2 , with 0, 1, 2, or similar x values
 * Seems to global, not picture based. (Select storage? Select camera access
 * mode?) Not clear. You need to set it correctly before download or delete
 * thought
 */
static int
web2_set_xx_mode(GPPort *port, GPContext *context, int mode) {
    char cmdbuf[2];

    cmdbuf[0] = mode & 0xff;
    cmdbuf[1] = (mode>>8) & 0xff;
    return web2_command(port, 1, 0xae, 0, 0, cmdbuf, 2);
}

static int
web2_getnumpics(
    GPPort *port, GPContext *context,
    int *x1, int *numpics, int *x3, int *freemem
) {
    char cmdbuf[10];
    int ret;

    ret = web2_command(port, 0, WEB2_GET_NUMPICS, 0x00, 0, cmdbuf, 10);
    if (ret!=GP_OK)
	return ret;
    *x1 = cmdbuf[0] | (cmdbuf[1] << 8); /* unclear */
    *numpics = cmdbuf[2] | (cmdbuf[3] << 8);
    *x3 = cmdbuf[4] | (cmdbuf[5] << 8); /* unclear */
    
    /* sometimes looks like mem, sometimes it doesn't */
    *freemem = cmdbuf[6] | (cmdbuf[7] << 8) | (cmdbuf[8] << 16) | (cmdbuf[9] << 24);
    return GP_OK;
}

/* called with 0x00, 0x20, or 0x40 */
/* 0x40 is 'delete selected picture' */
/* the others are unknown */
static int
web2_set_picture_attribute(GPPort *port, GPContext *context, int x, int *y) {
    char cmdbuf[2];
    int ret;

    ret = web2_command(port, 0, 0xba, x, 0, cmdbuf, 2);
    if (ret!=GP_OK)
	return ret;
    *y = cmdbuf[0] | (cmdbuf[1] << 8);
    return GP_OK;
}

#if 0
/* gets 7 shorts */
static int
_cmd_e6(GPPort *port, GPContext *context, short *arr) {
    char cmdbuf[14];
    int i;
    int ret;
    ret = web2_command(port, 0, 0xe6, 0, 0, cmdbuf, 14);
    if (ret!=GP_OK)
	return ret;
    for (i=0;i<7;i++)
	arr[i] = cmdbuf[2*i+0] | (cmdbuf[2*i+1]<<8);
    return GP_OK;
}

int
_cmd_d0(GPPort *port, GPContext *context, int flag, short *retval) {
    char buf[4];
    int ret;

    ret = web2_command(port, 0, 0xd0, 0, 0, buf, 4);
    if (ret < 0)
	return GP_ERROR_IO;
    *retval = buf[flag*2] | (buf[flag*2+1]<<8);
    return GP_OK;
}

int /* change mode? args are 30,40,50 ... they will appear in d0 later */
_cmd_1f(GPPort *port, GPContext *context, int x) {
    return web2_command(port, 1, 0x1f, x, 0, NULL, 0);
}


int
_cmd_07(GPPort *port, GPContext *context, int *x) {
    char cmdbuf[2];
    int ret;

    ret = web2_command(port, 0, 0x07, 0, 0, cmdbuf, 2);
    if (ret < 0)
	return GP_ERROR_IO;
    *x = cmdbuf[0] | (cmdbuf[1]<<8);
    return GP_OK;
}

int
_cmd_0a(GPPort *port, GPContext *context, int x1, int x2) {
    char buf[80], cmdbuf[2];
    int ret, i;

    cmdbuf[0] = x2&0xff;
    cmdbuf[1] = (x2>>8)&0xff;
    ret = web2_command(port, 1, 0x0a, x1, 0, cmdbuf, 2);
    if (ret < 0) {
	fprintf(stderr,"0a failed\n");
	return GP_ERROR_IO;
    }
    ret = gp_port_read(port, buf, 80);
    if (ret<80)
	return ret;
    for (i=0;i<sizeof(buf);i++) { fprintf(stderr,"%02x ",buf[i]); }
    fprintf(stderr,"\n");
    return GP_OK;
}

/* similar to 0a, writes the 80 byte table */
int
_cmd_0b(GPPort *port, GPContext *context, int x1, int x2) {
    char cmdbuf[2];
    int ret;

    cmdbuf[0] = x2&0xff;
    cmdbuf[1] = (x2>>8)&0xff;
    ret = web2_command(port, 1, 0x0b, x1, 0, cmdbuf, 2);
    if (ret < 0)
	return GP_ERROR_IO;
    /* Bulk writes stuff */
    return GP_OK;
}


int
_cmd_eb(GPPort *port, GPContext *context, int x) {
    char cmdbuf[2];

    cmdbuf[0] = x & 0xff;
    cmdbuf[1] = (x>>8) & 0xff;
    /* Only 1 is passed as param, and x appears to be a BOOL  */
    return web2_command(port, 1, 0xeb, 1, 0, cmdbuf, 2);
}

int
_cmd_11(GPPort *port, GPContext *context, int x) {
    return web2_command(port, 1, 0x11, x|0x20, 0, NULL, 0);
}


/* gets 1 shorts */
int
_cmd_bf(GPPort *port, GPContext *context, int x, short *val) {
    char cmdbuf[2];
    int ret;
    ret = web2_command(port, 0, 0xbf, x, 0, cmdbuf, 2);
    if (ret!=GP_OK)
	return ret;
    *val = cmdbuf[0] | (cmdbuf[1]<<8);
    return GP_OK;
}

int
_cmd_20(GPPort *port, GPContext *context, int x) {
    return web2_command(port, 1, 0x20, x, 0, NULL, 0);
}

int
_cmd_60(GPPort *port, GPContext *context, int x) {
    return web2_command(port, 1, 0x60, x, 0, NULL, 0);
}


int
_cmd_20_0(GPPort *port, GPContext *context) {
    return _cmd_20(port,context,0);
}

int
_cmd_e1(GPPort *port, GPContext *context, int x) {
    switch (x) {
    case 0: 	/* keep 0 */
		break;
    case 1:	x = 0xe;
	    	break;
    default:	x = 0xe + 0x19;
		break;
    }
    return web2_command(port, 1, 0xe1, x, 0, NULL, 0);
}

static short xshort[3];

int
_cmd_26(GPPort *port, GPContext *context, int x) {
    char cmdbuf[6];
    int i;

    switch (x) {
    case 0:
		for (i=0;i<3;i++) {
		    cmdbuf[2*i+0] = xshort[i] & 0xff;
		    cmdbuf[2*i+1] = (xshort[i] >> 8) & 0xff;
		}
		return web2_command(port, 1, 0x26, 0x10, 0, cmdbuf, 6); 
    case 1:	xshort[0] = 364; /* 16c */
	    	xshort[1] = 256; /* 100 */
	    	xshort[2] = 596; /* 254 */
		break;
    case 2:	xshort[0] = 998; /* 3e6 */
	    	xshort[1] = 256; /* 100 */
	    	xshort[2] = 259; /* 103 */
		break;
    default:	xshort[0] = 625; /* 271 */
	    	xshort[1] = 256; /* 100 */
	    	xshort[2] = 607; /* 25f */
		break;
    }
    for (i=0;i<3;i++) {
	cmdbuf[2*i+0] = xshort[i] & 0xff;
	cmdbuf[2*i+1] = (xshort[i] >> 8) & 0xff;
    }
    return web2_command(port, 1, 0x26, 0x00, 0, cmdbuf, 6);
}

int
_cmd_75(GPPort *port, GPContext *context, int x) {
    switch (x) {
    case 0: x = 1;break;
    case 1: x = 2;break;
    case 2: x = 0;break;
    default:x = 1;break;
    }
    return web2_command(port, 1, 0x75, x, 0, NULL, 0);
}

int
_cmd_fb(GPPort *port, GPContext *context, int *y) {
    char cmdbuf[2];
    int ret;

    ret = web2_command(port, 0, 0xfb, 0, 0xa2b8, cmdbuf, 2);
    if (ret != GP_OK)
	return ret;
    *y = cmdbuf[0] | (cmdbuf[1] << 8);
    return GP_OK;
}
#endif

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "SiPix Web2");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "SiPix:Web2");
	a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port     		= GP_PORT_USB;
	a.speed[0] 		= 0;
	a.usb_vendor		= 0x0c77;
	a.usb_product		= 0x1001;
	a.operations		=  GP_OPERATION_NONE;
	a.file_operations	=  GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW | GP_FILE_OPERATION_EXIF;
	a.folder_operations	=  GP_FOLDER_OPERATION_NONE;
	gp_abilities_list_append(list, a);

	/* reported by Terry Lewis <mrkennie@amscomputers.biz> */
	strcpy(a.model, "SiPix:SC2100");
	a.usb_product		= 0x1002;
	gp_abilities_list_append(list, a);
	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context) 
{
    	return web2_exit(camera->port, context);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int image_no, result, flags, aeflag, junk;

	if (strcmp(folder,"/"))
	    return GP_ERROR_BAD_PARAMETERS;

	image_no = gp_filesystem_number(fs, folder, filename, context);
	if (image_no < GP_OK)
	    return image_no;

	result = web2_get_picture_info(camera->port, context, image_no, &junk, &junk, &flags, &junk);
	if (result != GP_OK)
	    return result;

	if (flags & 1) {
	    aeflag = 1;
	} else {
	    if (flags & 2) {
		aeflag = 2;
	    } else {
		fprintf(stderr,"Oops , 0xAD returned flags %x?!\n",flags);
		return GP_ERROR;
	    }
	}
	result = web2_select_picture(camera->port, context, image_no);
	if (result != GP_OK)
	    return result;

	result = web2_set_xx_mode(camera->port, context, aeflag);
	if (result != GP_OK)
	    return result;

	/*
	result = _cmd_e6(camera->port, context, fupp);
	if (result != GP_OK)
	    return result;

	for (i=0;i<7;i++)
	    fprintf(stderr,"%d (%x)",fupp[i],fupp[i]);
	fprintf(stderr,"\n");
	*/

	gp_file_set_mime_type (file, GP_MIME_JPEG);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
	    /*result = _cmd_ae(camera->port, context, 1);
	    if (result != GP_OK)
		fprintf(stderr,"ae 1 failed with %d\n",result);
		*/
	    result = web2_getpicture(camera->port,context,file);
	    break;
	case GP_FILE_TYPE_PREVIEW:
	    result = web2_getthumb (camera->port,context,file);
	    break;
	case GP_FILE_TYPE_EXIF:
	    result = web2_getexif(camera->port,context,file);
	    break;
	default:
	    return (GP_ERROR_NOT_SUPPORTED);
	}
	if (result < 0)
	    return result;
	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int aeflag, flags, junk, result, image_no;

	image_no = gp_filesystem_number(fs, folder, filename, context);
	if (image_no < GP_OK)
	    return image_no;

	result = web2_get_picture_info(camera->port, context, image_no, &junk, &junk, &flags, &junk);
	if (result != GP_OK)
	    return result;

	if (flags & 1) {
	    aeflag = 1;
	} else {
	    if (flags & 2) {
		aeflag = 2;
	    } else {
		fprintf(stderr,"Oops , 0xAD returned flags %x?!\n",flags);
		return GP_ERROR;
	    }
	}

	result = web2_select_picture(camera->port, context, image_no);
	if (result != GP_OK)
	    return result;


	result = web2_set_xx_mode(camera->port, context, aeflag);
	if (result != GP_OK)
	    return result;

	/* Delete the file from the camera. */
	result = web2_set_picture_attribute(camera->port, context, 0x40, &junk);
	if (result!= GP_OK)
	    return result;
	/* fprintf(stderr,"ba 0x40 returns %d\n",junk);*/
	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("SiPix Web2\n"
			       "Marcus Meissner <marcus@jet.franken.de>\n"
			       "Driver for accessing the SiPix Web2 camera."));

	return (GP_OK);
}

/* 
 * This function makes sure that the <index in folder> is <picture index in
 * camera>. Even delete should keep that relation.
 */
static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int i, count, result, junk, flags, aeflag;

	result =  web2_getnumpics(camera->port, context, &junk, &count, &junk, &junk);
	if (result != GP_OK)
		return result;

	for (i=0;i<count;i++) {
	    char fn[20];
	    int	size;

	    result = web2_get_picture_info(camera->port, context, i, &junk, &junk, &flags, &junk);
	    if (result != GP_OK)
		return result;

	    if (flags & 1) {
		aeflag = 1;
	    } else {
		if (flags & 2) {
		    aeflag = 2;
		} else {
		    fprintf(stderr,"Oops , 0xAD returned flags %x?!\n",flags);
		    return GP_ERROR;
		}
	    }
	    result = web2_select_picture(camera->port, context, i);
	    if (result != GP_OK)
		return result;

	    result = web2_set_xx_mode(camera->port, context, aeflag);
	    if (result != GP_OK)
		return result;

	    result = web2_get_file_info(camera->port, context, fn, &size);
	    if (result != GP_OK)
		return result;
	    gp_list_append(list, fn, NULL);
	}
	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func
};

int
camera_init (Camera *camera, GPContext *context) 
{
        camera->functions->exit                 = camera_exit;
        camera->functions->about                = camera_about;
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
	return web2_init(camera->port, context);
}
