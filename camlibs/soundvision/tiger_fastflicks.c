/*
 * tiger_fastflicks.c
 *
 *  Commands specific to Tiger FastFlicks cameras
 *  Fast Flicks camera, made by Tiger Electronics.                
 *  Initial Support added by Dr. William Bland <wjb@abstractnonsense.com> 
 *  Other help from  Adrien Hernot <amh@BSD-DK.dk>
 * 
 * Copyright 2002 Vince Weaver <vince@deater.net>
 */


/* the fastflicks seems to use version 5.x of the soundvision protocol */
/* it will work properly with version 2.16b that Agfa Cl18 uses, but   */
/* the native updated commands might work better.  They can be added   */
/* here and be pointed to in the soundvision.c file.  You just have to */
/* track down the traces and test them out.                            */

#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#ifdef OS2
#include <db.h>
#endif

#include <gphoto2.h>
#include <gphoto2-endian.h>

#include "soundvision.h"
#include "commands.h"

#define GP_MODULE "soundvision"

       /* This is possible, traces exist.  If someone adds support     */
       /* e-mail the maintainer.                                       */
       /* be sure to put                                               */
       /* PUT_FILE for i==1 eventually under f_ops in camera_abilities */
       /* in soundvision.c                                             */
int tiger_upload_file(CameraPrivateLibrary *dev, const char *filename) {
 
    return 0;
}

       /* This is just a start, from the traces I have          */
       /* Hopefully someone who has the camera will finish this */
       /* as it is impossible to test w/o the camera            */
int tiger_delete_picture(CameraPrivateLibrary *dev, const char *filename) {
   
    int32_t ret,temp,taken,ascii_status[2]; 
   
    ret = soundvision_send_command(SOUNDVISION_START_TRANSACTION,0,dev);
    if (ret<0) return ret;
   
    ret = soundvision_send_command(SOUNDVISION_GET_VERSION,0,dev);
    if (ret<0) return ret;   
        
    ret = soundvision_read(dev, &ascii_status, sizeof(ascii_status));        
    if (ret<0) return ret;
   
    ret = soundvision_send_command(SOUNDVISION_START_TRANSACTION,0,dev);
    if (ret<0) return ret;
    
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;

    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    taken=soundvision_photos_taken(dev);

    return GP_OK;

}

