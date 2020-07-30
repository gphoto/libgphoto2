/*
 * commands.c
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
 *
 *  Command set for the soundvision cameras
 *
 * Copyright 2001-2003 Vince Weaver <vince@deater.net>
 */

#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#ifdef OS2
#include <db.h>
#endif

#include <gphoto2/gphoto2.h>
#include "gphoto2-endian.h"

#include "soundvision.h"
#include "commands.h"

#define GP_MODULE "soundvision"

    /* Regular commands always 8 bytes long */
int32_t soundvision_send_command(uint32_t command, uint32_t argument,
				  CameraPrivateLibrary *dev) {

    uint8_t cmd[12];
    int result;

    htole32a(&cmd[0],8);        /* Length "8" in little-endian 32bits */
    htole32a(&cmd[4],command);  /* Command is a little-endian 32bits  */
    htole32a(&cmd[8],argument); /* Argument is a little-endian 32bits */

    result=gp_port_write(dev->gpdev,(char *)&cmd,sizeof(cmd));
    if (result<0) return result;
    return GP_OK;
}

    /* Filenames are always 12 bytes long */
int32_t soundvision_send_file_command(const char *filename,
				       CameraPrivateLibrary *dev) {
    char file_cmd[16];

    htole32a(&file_cmd[0],0xc);        /* Length is 12 little-endian 32 bits */
    strncpy(&file_cmd[4],filename,12); /* Filename is 12 bytes at the end */
    return gp_port_write (dev->gpdev, file_cmd, sizeof(file_cmd));
}

    /* USB-only */
int32_t soundvision_read(CameraPrivateLibrary *dev, void *buffer, int len) {

    return gp_port_read(dev->gpdev, buffer, len);
}


       /* Reset the camera */
int soundvision_reset(CameraPrivateLibrary *dev,char *revision, char *status) {

    int ret,attempts=0;

retry_reset:

        /* This prevents lockups on tiger !!!! */
    ret=soundvision_send_command(SOUNDVISION_START_TRANSACTION,0,dev);
    if (ret<0) goto reset_error;

       /* First get firmware revision */
    ret=soundvision_get_revision(dev,revision);

       /* If camera out of whack, this is where it will hang   */
       /* If we retry a few times usually after a few timeouts */
       /* It will get going again                              */
    if (ret<0) {
       if (attempts<2) {
          attempts++;
          goto retry_reset;
       }
       else goto reset_error;
    }


       /* Dshot 3 camera does 2 extra steps    */
       /* Seems to enable extra info in status */
       /* What does it do?  It works w/o it    */
       /* And other tiger cameras might not    */
       /* need it.                             */
#if 0
    if (dev->device_type==SOUNDVISION_TIGERFASTFLICKS) {

       char result[12];

          /* What does 0x405 signify? */
       ret=soundvision_send_command(SOUNDVISION_SETPC2,0x405,dev);
       if (ret<0) goto reset_error;

       ret=soundvision_send_command(SOUNDVISION_INIT2,0,dev);
       if (ret<0) goto reset_error;

          /* Read returned value.  Why 0x140,0xf,0x5 ? */
       ret = soundvision_read(dev, &result, sizeof(result));
       if (ret<0) goto reset_error;
    }
#endif

       /* Read the status registers */
    dev->reset_times++;

    if (dev->device_type!=SOUNDVISION_IXLA) {
       ret=soundvision_get_status(dev,status);
       if (ret<0) goto reset_error;
    }

    return GP_OK;

reset_error:

    GP_DEBUG("Error in soundvision_reset\n");
    return ret;

}

int soundvision_get_revision(CameraPrivateLibrary *dev, char *revision) {

    int ret;
    char version[9];

    ret = soundvision_send_command(SOUNDVISION_GET_VERSION,0,dev);
    if (ret<0) return ret;

    ret = soundvision_read(dev, &version, sizeof(version)-1);
    if (ret<0) return ret;

       /* If null we don't care */
    if (revision!=NULL) {
       strncpy(revision,version,8);
       revision[8]=0;
    }

    return GP_OK;
}


    /* Status is a 96 byte array.         */
    /* Haven't been able to decode it yet */
    /* It changes with every read  though */
int soundvision_get_status(CameraPrivateLibrary *dev, char *status) {

    uint8_t ss[0x60];
    int32_t ret;

    ret=soundvision_send_command(SOUNDVISION_STATUS, 0, dev);
    if (ret < 0) goto get_status_error;

    ret = soundvision_read(dev, (unsigned char *)&ss, sizeof(ss));
    if (ret < 0) goto get_status_error;

    if (status!=NULL) {
       memcpy(status,ss,0x60);
    }

    return GP_OK;

get_status_error:
    GP_DEBUG("Error getting camera status\n");
    return ret;

}

int soundvision_photos_taken(CameraPrivateLibrary *dev) {

    int32_t ret;
    uint32_t numpics;

    ret=soundvision_send_command(SOUNDVISION_GET_NUM_PICS, 0, dev);
    if (ret < 0) goto error_photos_taken;

    ret = soundvision_read(dev, &numpics, sizeof(numpics));
    if (ret < 0) goto error_photos_taken;

    return le32toh(numpics);

error_photos_taken:
    GP_DEBUG("Error getting number of photos taken.\n");
    return ret;
}

int soundvision_get_file_list(CameraPrivateLibrary *dev) {
    if (dev->device_type==SOUNDVISION_TIGERFASTFLICKS)
       return tiger_get_file_list(dev);
    else
       return agfa_get_file_list(dev);
}

int soundvision_get_thumb_size(CameraPrivateLibrary *dev, const char *filename) {
    if (dev->device_type==SOUNDVISION_TIGERFASTFLICKS)
       return tiger_get_thumb_size(dev,filename);
    else
       return agfa_get_thumb_size(dev,filename);
}

int soundvision_get_thumb(CameraPrivateLibrary *dev, const char *filename,
		   unsigned char *data,int size) {
    if (dev->device_type==SOUNDVISION_TIGERFASTFLICKS)
       return tiger_get_thumb(dev,filename,data,size);
    else
       return agfa_get_thumb(dev,filename,data,size);
}

int soundvision_get_pic_size(CameraPrivateLibrary *dev, const char *filename) {
    if (dev->device_type==SOUNDVISION_TIGERFASTFLICKS)
       return tiger_get_pic_size(dev,filename);
    else
       return agfa_get_pic_size(dev,filename);
}

int soundvision_get_pic(CameraPrivateLibrary *dev, const char *filename,
		   unsigned char *data,int size) {
    if (dev->device_type==SOUNDVISION_TIGERFASTFLICKS)
       return tiger_get_pic(dev,filename,data,size);
    else
       return agfa_get_pic(dev,filename,data,size);
}

int soundvision_delete_picture(CameraPrivateLibrary *dev, const char *filename) {
    if (dev->device_type==SOUNDVISION_TIGERFASTFLICKS)
       return tiger_delete_picture(dev,filename);
    else
       return agfa_delete_picture(dev,filename);
}
