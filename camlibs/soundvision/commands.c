/*
 * commands.c
 *
 *  Command set for the soundvision cameras
 *
 * Copyright 2001-2002 Vince Weaver <vince@deater.net>
 */
#include <config.h>

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

#define GP_MODULE "soundvision"

    /* Regular commands always 8 bytes long */
uint32_t soundvision_send_command(uint32_t command, uint32_t argument, 
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
uint32_t soundvision_send_file_command(const char *filename, 
				       CameraPrivateLibrary *dev) {
    
    uint8_t file_cmd[16];
    int result;
   
    htole32a(&file_cmd[0],0xc);       /* Length is "C" little-endian 32 bits */
    strncpy(&file_cmd[4],filename,12);/* Filename is 12 bytes at the end */
					  
    result=gp_port_write(dev->gpdev,(char *)&file_cmd,sizeof(file_cmd));
    if (result<0) return result;
    return GP_OK;
}

    /* USB-only */
uint32_t soundvision_read(CameraPrivateLibrary *dev, void *buffer, int len) {

    return gp_port_read(dev->gpdev, buffer, len);
}


int soundvision_reset(CameraPrivateLibrary *dev) {
    int ret;
   
    ret=soundvision_send_command(SOUNDVISION_RESET,0,dev);
    if (ret<0) return ret;
   
    return (GP_OK);
}

int soundvision_get_revision(CameraPrivateLibrary *dev, char *revision) {
 
    int ret;
    char version[8];
    uint32_t temp;
   
    ret = soundvision_send_command(SOUNDVISION_DONE_TRANSACTION,0,dev);
   
    ret = soundvision_send_command(SOUNDVISION_GET_VERSION,0,dev);
    if (ret<0) return ret;
   
    ret = soundvision_read(dev, &version, sizeof(version));
    if (ret<0) return ret;
   
    strncpy(revision,version,8);
   
    ret=soundvision_reset(dev);
    if (ret<0) return ret;
   
//    ret = soundvision_read(dev, &temp, sizeof(temp));        
//    if (ret<0) return ret;    
    
    
   
   
    return 0;
}


    /* Status is a 60 byte array.  I have no clue what it does */
int soundvision_get_status(CameraPrivateLibrary *dev, int *taken,
        int *available, int *rawcount) {

    uint8_t ss[0x60];
    uint32_t ret;

   
    ret=soundvision_send_command(SOUNDVISION_STATUS, 0, dev);
   
    if (ret < 0) {
       fprintf(stderr, "soundvision_get_storage_status: error sending command\n");
       return ret;
    }

    ret = soundvision_read(dev, (unsigned char *)&ss, sizeof(ss));
    if (ret < 0) {
       fprintf(stderr, "soundvision_get_storage_status: error getting count\n");
       return ret;
    }
       
    soundvision_reset(dev); 
   
    return (GP_OK);
}

int soundvision_photos_taken(CameraPrivateLibrary *dev) {
   
    uint32_t ret,numpics;

    ret=soundvision_send_command(SOUNDVISION_GET_NUM_PICS, 0, dev);

    if (ret < 0) {
       fprintf(stderr, "soundvision_get_storage_status: error sending command\n");
       return ret;
    }

    ret = soundvision_read(dev, &numpics, sizeof(numpics));
    if (ret < 0) {
       fprintf(stderr, "soundvision_get_storage_status: error getting count\n");
       return ret;
    }
    return le32toh(numpics);

}


int soundvision_get_file_list(CameraPrivateLibrary *dev) {

    char *buffer;
    uint32_t ret, taken, buflen;

    
    /* It seems we need to do a "reset" packet before reading names?? */
   
    soundvision_reset(dev);
   
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

    memcpy(dev->file_list, buffer, taken * 13);
    free(buffer);

#if 0
    taken=soundvision_photos_taken(dev);
    soundvision_get_thumb_size(dev,dev->file_list);
#endif 
    return GP_OK;
}

int soundvision_get_thumb_size(CameraPrivateLibrary *dev, const char *filename) {
 
    uint32_t ret,temp,size; 
   
    ret=soundvision_send_command(SOUNDVISION_GET_THUMB_SIZE,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
       
    soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = soundvision_read(dev, &size, sizeof(size));        
    if (ret<0) return ret;
    
    return le32toh(size);
   
}

int soundvision_get_thumb(CameraPrivateLibrary *dev, const char *filename,
		   unsigned char *data,int size) {

    uint32_t ret,temp; 
   
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

int soundvision_get_pic_size(CameraPrivateLibrary *dev, const char *filename) {
 
    uint32_t ret,temp,size; 
   
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

int soundvision_get_pic(CameraPrivateLibrary *dev, const char *filename,
		   unsigned char *data,int size) {
   
    uint32_t ret,temp; 
   
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

   /* thanks to heathhey3@hotmail.com for sending me the trace */
   /* to implement this */
int soundvision_delete_picture(CameraPrivateLibrary *dev, const char *filename) {
   
    uint32_t ret,temp,taken; 
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
    dev->file_list = buffer;
   
    ret=soundvision_send_command(SOUNDVISION_GET_PIC_SIZE,0,dev);
    if (ret<0) return ret;
    
       /* always returns ff 0f 00 00 ??? */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
   
    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = soundvision_read(dev, data, size);        
    if (ret<0) return ret; 
   
    return 0;

}
