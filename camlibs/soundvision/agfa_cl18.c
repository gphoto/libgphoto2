/*
 * agfa_cl18.c
 *
 *  Command set specific to agfa-ephoto-CL18
 *
 * Copyright 2001-2002 Vince Weaver <vince@deater.net>
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
#include <unistd.h>

#include <gphoto2/gphoto2.h>
#include "gphoto2-endian.h"

#include "soundvision.h"
#include "commands.h"

#define GP_MODULE "soundvision"

    /* Below contributed by Ben Hague <benhague@btinternet.com> */
int agfa_capture(CameraPrivateLibrary *dev, CameraFilePath *path) {
    /*FIXME: Not fully implemented according to the gphoto2 spec.*/
    /*Should also save taken picture, and then delete it from the camera*/
    /*but when I try to do that it just hangs*/

    int ret;

    ret=soundvision_send_command(SOUNDVISION_SETPC1,0,dev);
    if (ret < 0) return ret;
    ret=soundvision_send_command(SOUNDVISION_SETPC2,0,dev);
    if (ret < 0) return ret;
    ret=soundvision_send_command(SOUNDVISION_TAKEPIC3,0,dev);
    if (ret < 0) return ret;
    ret=soundvision_send_command(SOUNDVISION_SETPC2,0,dev);
    if (ret < 0) return ret;
    
    /*Not sure if this delay is necessary, but it was used in the windows driver*/
    /*delay(20); */
    sleep(20);
    /*Again, three times in windows driver*/
    soundvision_photos_taken(dev);
    soundvision_photos_taken(dev);
    soundvision_photos_taken(dev);
    /*This seems to do some kind of reset, but does cause the camera to start responding again*/
    ret=soundvision_send_command(SOUNDVISION_GET_NAMES, 0, dev);
    if (ret < 0) return ret;

    return GP_OK;
}

   /* Seems to work OK though sometimes camera is left in */
   /* unknown state after completing */
   /* thanks to heathhey3@hotmail.com for sending me the trace */
   /* to implement this */
int agfa_delete_picture(CameraPrivateLibrary *dev, const char *filename) {
   
    int32_t ret,temp,taken; 
    uint8_t data[4],*buffer;
    uint32_t size=4,buflen;
   
       /* yes, we do this twice?? */
    taken=soundvision_photos_taken(dev);
    taken=soundvision_photos_taken(dev);
    
    ret = soundvision_send_command(SOUNDVISION_GET_PIC_SIZE,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
       
      /* Some traces show sending other than the file we want deleted? */
    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = soundvision_read(dev, data, size);        
    if (ret<0) return ret;
   
          /* Check num taken AGAIN */
    taken=soundvision_photos_taken(dev);
  
    ret = soundvision_send_command(SOUNDVISION_GET_PIC_SIZE,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
    
    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = soundvision_read(dev, data, size);        
    if (ret<0) return ret;
   
        /* Check num taken AGAIN */
    taken=soundvision_photos_taken(dev);
       
    ret=soundvision_send_command(SOUNDVISION_DELETE,0,dev);
    if (ret<0) return ret;
      
        /* read ff 0f ??? */
    ret = soundvision_read(dev, data, size);        
    if (ret<0) return ret;
   
    ret = soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
        /* This is the point we notices that in fact a pic is missing */
        /* Why do it 4 times??? Timing?? Who knows */
    taken=soundvision_photos_taken(dev);
    taken=soundvision_photos_taken(dev);
    taken=soundvision_photos_taken(dev);
    taken=soundvision_photos_taken(dev);
    
    buflen = (taken * 13)+1;  /* 12 char filenames and space for each */
                              /* plus trailing NULL */
    buffer = malloc(buflen);
        
    if (!buffer) {
       GP_DEBUG("Could not allocate %i bytes!",
		       buflen);
       return (GP_ERROR_NO_MEMORY);
    }

    ret=soundvision_send_command(SOUNDVISION_GET_NAMES, buflen,dev);
    if (ret < 0) {
       free(buffer);
       return ret;
    }

    ret = soundvision_read(dev, (void *)buffer, buflen);
    if (ret < 0) {
       free(buffer);
       return ret;
    }

    if (dev->file_list) free(dev->file_list);
    dev->file_list = (char *)buffer;
   
    ret=soundvision_send_command(SOUNDVISION_GET_PIC_SIZE,0,dev);
    if (ret<0) return ret;
    
       /* always returns ff 0f 00 00 ??? */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
   
    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = soundvision_read(dev, data, size);        
    if (ret<0) return ret; 
   
    return GP_OK;

}



int agfa_get_file_list(CameraPrivateLibrary *dev) {

    char *buffer;
    int32_t ret, taken, buflen,i;

    
    /* It seems we need to do a "reset" packet before reading names?? */
   
    soundvision_reset(dev,NULL,NULL);
   
    if ( (taken=soundvision_photos_taken(dev)) < 0)
       return taken;
   
    dev->num_pictures = taken;

    
    buflen = (taken * 13)+1;  /* 12 char filenames and space for each */
                              /* plus trailing NULL */
    
    buffer = malloc(buflen);
        
    if (!buffer) {
       GP_DEBUG("Could not allocate %i bytes!",
		       buflen);
       return GP_ERROR_NO_MEMORY;
    }

    ret=soundvision_send_command(SOUNDVISION_GET_NAMES, buflen, dev);
    if (ret < 0) {
       free(buffer);
       return ret;
    }

    ret = soundvision_read(dev, (void *)buffer, buflen);
    if (ret < 0) {
       free(buffer);
       return ret;
    }

    if (dev->file_list) free(dev->file_list);

    dev->file_list = malloc(taken * 13);
    if (!dev->file_list) {
       GP_DEBUG("Could not allocate %i bytes!",
		       taken*13);
       free(buffer);
       return (GP_ERROR_NO_MEMORY);
    }
   
    for(i=0;i<taken*13;i++) if (buffer[i]==' ') buffer[i]='\0';
    memcpy(dev->file_list, buffer, taken * 13);
    free(buffer);

#if 0
    taken=soundvision_photos_taken(dev);
    soundvision_get_thumb_size(dev,dev->file_list);
#endif 
    return GP_OK;
}

int agfa_get_pic(CameraPrivateLibrary *dev, const char *filename,
		   unsigned char *data,int size) {
   
    int32_t ret,temp; 
   
    ret = soundvision_send_command(SOUNDVISION_GET_PIC,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
    
    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = soundvision_read(dev, data, size);        
    if (ret<0) return ret;

#if 0
       /* Have to do this after getting pic ? */
    ret=soundvision_send_command(SOUNDVISION_DONE_PIC,0,dev);
    if (ret<0) return ret;
#endif
   
    return GP_OK;
   
}

int agfa_get_pic_size(CameraPrivateLibrary *dev, const char *filename) {
 
    int32_t ret,temp;
    uint32_t size; 
   
    ret=soundvision_send_command(SOUNDVISION_GET_PIC_SIZE,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
    
    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = soundvision_read(dev, &size, sizeof(size));        
    if (ret<0) return ret;
    
    return le32toh(size);
   
}


int agfa_get_thumb_size(CameraPrivateLibrary *dev, const char *filename) {
 
    int32_t ret,temp;
    uint32_t size; 
   
    ret=soundvision_send_command(SOUNDVISION_GET_THUMB_SIZE,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
       
    ret = soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = soundvision_read(dev, &size, sizeof(size));        
    if (ret<0) return ret;
    
    return le32toh(size);
   
}

int agfa_get_thumb(CameraPrivateLibrary *dev, const char *filename,
		   unsigned char *data,int size) {

    int32_t ret,temp; 
   
    ret = soundvision_send_command(SOUNDVISION_GET_THUMB,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
       
    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = soundvision_read(dev, data, size);        
    if (ret<0) return ret;
#if 0
           /* Is this needed? */
        soundvision_photos_taken(dev,&ret);
   
        ret=soundvision_send_command(SOUNDVISION_END_OF_THUMB,0,dev);
        if (ret<0) return ret;
   
        ret = soundvision_read(dev, temp_string, 8);        
        if (ret<0) return ret;   
#endif   
   
    return GP_OK;
   
}
