/* blink2.c
 *
 * Copyright 2004 Olivier Fauchon <olivier@aixmarseille.com>
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
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-result.h>
#include "gphoto2-endian.h"

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

#define ENIGMA13_BLK_FLASH_ALIGN 0x2000
#define ENIGMA13_BLK_CARD_ALIGN 0x4000
#define ENIGMA13_USB_TIMEOUT_MS 5000
#define ENIGMA13_WAIT_FOR_READY_TIMEOUT_S 5
#define ENIGMA13_WAIT_TOC_DELAY_MS 500
#define ENIGMA13_WAIT_IMAGE_READY_MS 300
#define GP_MODULE "enigma13"


#define CHECK(result) {int res; res = result; if (res < 0) return (res);}
#define CHECK_AND_FREE(result, buf) {int res; res = result; if (res < 0) { free(buf); return (res); }}


/* the table of content of the camera
   it's filled in enigma_get_toc, and used in enigma_download_img */    
static char* enigma13_static_toc=NULL;




static int enigma13_about (Camera *camera, CameraText *about, GPContext *context)
{
        strcpy (about->text, _("Download program for Digital Dream Enigma 1.3. "
                "by <olivier@aixmarseille.com>, and adapted from spca50x driver."
                "Thanks you, spca50x team, it was easy to port your driver on this cam! "));
        return (GP_OK);
}


#if 0
/*
 * This function probes the camera to get all the storages media
 * supported (sdram, flash, card)
 *
 * Status: Working but not used
 */
static int enigma13_detect_storage_type (Camera *camera)
{
        int i;
        char buf[3];

        for (i=0;i<3;i++)
        {
                CHECK (gp_port_usb_msg_read (camera->port, 0x28, 0x0000,
                                        i, &buf[i], 0x01));           
        }
        gp_log(GP_LOG_DEBUG, "enigma13","Camera storage information sdram: 0x%x flash 0x%x card: 0x%x\n"
                        , buf[0], buf[1], buf[2]);
        return GP_OK;
}
#endif





/*
 * This function deletes all the images
 * supported (sdram, flash, card)
 *
 * Status: not verified 
 */
static int
enigma13_flash_delete_all(CameraFilesystem *fs, const char *folder, void *data,
                 GPContext *context)
{
   Camera *cam;
   gp_log(GP_LOG_DEBUG, "enigma13","Delete all files");
   cam = data;
   CHECK (gp_port_usb_msg_write (cam->port, 0x52, 0x0, 0x0, NULL, 0x0) );
   return GP_OK;                             
}




/*
 * This function waits camera is ready
 *
 * Status: Not sure what it realy does, but the camera replies
 * Can this be some kind of ping for keepalive, or realy a ready status ?
 */
static int enigma13_wait_for_ready(Camera *camera)
{
        int timeout = ENIGMA13_WAIT_FOR_READY_TIMEOUT_S;
        char ready = 1;
        while (timeout--) {
                sleep(1);
                CHECK (gp_port_usb_msg_read (camera->port,
                              0x21, 0x0000, 0x0000,
                              (char*)&ready, 0x01));
                if (ready==0)
                        return GP_OK;
        }
        return GP_ERROR;
}



#if 0
/*
 * Get the number of images in cam
 * 
 * Status: Should work, but not used as enigma13_get_TOC does all.
 *
 */
static int enigma13_get_filecount (Camera *camera, int *filecount)
{
        uint16_t response = 0;
        CHECK(enigma13_wait_for_ready(camera));
        CHECK (gp_port_usb_msg_read (camera->port,
                             0x54, 0x0000, 0x0000,
                             (char*)&response, 0x02));
        LE16TOH (response);
        *filecount = response;
        return GP_OK;
}
#endif




/*
 * Get the table of contents (directory) from the cam 
 *
 * Verified: Yes
 */
static int enigma13_get_toc(Camera *camera, int *filecount, char** toc)
{
        char* flash_toc=NULL;
        int toc_size = 0;
        char buf[10];
        uint16_t response = 0;
        int ret=1;

        CHECK(enigma13_wait_for_ready(camera));
        CHECK (gp_port_usb_msg_read (camera->port,
                             0x54, 0x0000, 0x0000,
                             (char*)&response, 0x02));
        LE16TOH (response);
        *filecount = response;

        /* Calc toc size */
        toc_size = (response) * 0x20;

        if (toc_size % 0x200 != 0)
           toc_size = ((toc_size / 0x200) + 1) * 0x200;

	CHECK(enigma13_wait_for_ready(camera));

        CHECK (gp_port_usb_msg_write (camera->port, 0x54,
                           response, 0x0001,
                          NULL, 0x0000));
        /* Wait until cam is ready to send the T.O.C */
	usleep(ENIGMA13_WAIT_TOC_DELAY_MS * 1000);

        CHECK (gp_port_usb_msg_read (camera->port, 0x21,
                           0x0000, 0x0000,
                          buf, 0x01));
        if (buf[0]!=0x41) return GP_ERROR;

        CHECK (gp_port_usb_msg_read (camera->port, 0x21,
                           0x0000, 0x0002,
                          buf, 0x01));
        if (buf[0]!=0x01) return GP_ERROR;


       flash_toc = (char*)malloc(toc_size);
       if (!flash_toc)
                return GP_ERROR_NO_MEMORY;

        ret=gp_port_read (camera->port, flash_toc, toc_size);
        *toc= flash_toc;
				gp_log(GP_LOG_DEBUG, "enigma13","Byte transferred :%d ", ret);
        return ret;

}


/*
 * Function that downloads image
 *
 * Camera camera  : camera structure
 * int    index   : image index ( in gphoto point of view) -1 for real index
 * char*  toc     : pointer to toc
 * char** img_buf : returned image data
 * int* img_size      : returned image data size  
 *
 */
static int enigma13_download_img(Camera *camera, char *toc, int index, char **img_data, int *img_size)
{
	uint8_t *p;
	uint32_t file_size = 0, aligned_size = 0;
	char* buf=NULL; 
	int align=0;
	char  retbuf[2];

	gp_log(GP_LOG_DEBUG, "enigma13","DOWNLOADING IMAGE NO %d",index);

	/* Offset for image informations .
	Each image has 16 bytes for name and 16 for size, and others*/
	p = (uint8_t *)toc + (index*2)*32;

	/* real size from toc*/
	aligned_size = file_size =
		(p[0x1c] & 0xff)
		+ (p[0x1d] & 0xff) * 0x100
		+ (p[0x1e] & 0xff) * 0x10000;

	CHECK (gp_port_usb_msg_read (camera->port, 0x23,
		0x0000, 0x64,
		retbuf, 0x01));
	if (retbuf[0]==0x20){
		align=ENIGMA13_BLK_CARD_ALIGN;
		gp_log(GP_LOG_DEBUG, "enigma13"," Image from card, alignement is set to %d bytes ",align);
	} else if (retbuf[0]==0x10){
		align=ENIGMA13_BLK_FLASH_ALIGN;
		gp_log(GP_LOG_DEBUG, "enigma13"," Image from flash, alignement is set to %d bytes",align);
	} else {
		return GP_ERROR;
	}

	if (file_size % align != 0)
		aligned_size = ((file_size / align) + 1) * align;

	buf = (char*) malloc (aligned_size);
	if (!buf)
		return GP_ERROR_NO_MEMORY;

	CHECK_AND_FREE (gp_port_usb_msg_write (camera->port,
	0x54, index+1, 2, NULL, 0x00), buf);
	usleep(ENIGMA13_WAIT_IMAGE_READY_MS * 1000);

	CHECK_AND_FREE (gp_port_usb_msg_read (camera->port, 0x21,
		0x0000, 0x0000,
		buf, 0x01), buf);
	if (buf[0]!=0x41) {
		free (buf);
		return GP_ERROR;
	}

	CHECK_AND_FREE (gp_port_usb_msg_read (camera->port, 0x21,
		0x0000, 0x0002,
		buf, 0x01), buf);
	if (buf[0]!=0x01) {
		free (buf);
		return GP_ERROR;
	}

	CHECK_AND_FREE (gp_port_usb_msg_read (camera->port, 0x21,
		0x0000, 0x0002,
		buf, 0x01), buf);
	if (buf[0]!=0x01) {
		free (buf);
		return GP_ERROR;
	}
	gp_log(GP_LOG_DEBUG, "enigma13","READY FOR TRANSFER");
	CHECK_AND_FREE (gp_port_read (camera->port, buf, aligned_size), buf);
	*img_data=buf;
	*img_size=file_size;
	return GP_OK;
}


/*
 * GPHOTO list files
 */

static int file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int	i, numpics;
        char tmp[20];

        CHECK(enigma13_get_toc(camera,&numpics,&enigma13_static_toc));

	for ( i=0; i < numpics; i=i+2 ) {
                sprintf(tmp,"sunp%04d.jpg",(i/2));
		gp_list_append( list, tmp, NULL);
	}
	return (GP_OK);
}



/*
 * File operations 
 */
static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{


	Camera *camera = data;
        int image_no, result;

	char* img_data=NULL;
        int   img_size=-1;

        image_no = gp_filesystem_number(fs, folder, filename, context);
        gp_log(GP_LOG_DEBUG, "enigma13","Index of image %d is %s",image_no, filename);
        switch (type) {
        case GP_FILE_TYPE_NORMAL:
        {
                gp_log(GP_LOG_DEBUG, "enigma13","Downloading raw image");

                CHECK(enigma13_download_img(camera, enigma13_static_toc, image_no, &img_data, &img_size));
                result = gp_file_append( file, img_data, img_size);
 
		break;
	}
	default:
                result = GP_ERROR_NOT_SUPPORTED;
		break;
        }
        if (result < 0)
                return result;
        return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model,	"DigitalDream:Enigma1.3");
	a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port     		= GP_PORT_USB;
	a.speed[0] 		= 0;
	a.usb_vendor		= 0x05da;
	a.usb_product		= 0x1018;
	a.operations		=  GP_OPERATION_NONE;
	a.file_operations	=  GP_FILE_OPERATION_NONE;
	a.folder_operations	=  GP_FOLDER_OPERATION_DELETE_ALL;
	gp_abilities_list_append(list, a);
	return (GP_OK);
}

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "Digital Dream Enigma 1.3");
	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.delete_all_func = enigma13_flash_delete_all
};

int
camera_init (Camera *camera, GPContext *context) 
{
	GPPortSettings settings;

        camera->functions->about        = enigma13_about;

	CHECK(gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera));
	CHECK(gp_port_get_settings( camera->port, &settings));
        settings.usb.inep = 0x82;
        settings.usb.outep = 0x03;
        settings.usb.config = 1;
        settings.usb.interface = 0;
        settings.usb.altsetting = 0;
        CHECK(gp_port_set_timeout (camera->port, ENIGMA13_USB_TIMEOUT_MS));
	CHECK(gp_port_set_settings( camera->port, settings));
	return GP_OK;


}





