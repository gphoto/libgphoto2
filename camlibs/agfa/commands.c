/*
 * commands.c
 *
 *  Command set for the agfa cameras
 *
 * Copyright 2001 Vince Weaver <vince@deater.net>
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
#include <netinet/in.h>
#include <gphoto2.h>

#include "agfa.h"

#define GP_MODULE "agfa"

    /* Regular commands always 8 bytes long */
static int agfa_send_command(int command, int argument, CameraPrivateLibrary *dev) {

    struct agfa_command cmd;
    int result;
   
    cmd.length = 8;
    cmd.command = command;
    cmd.argument = argument;
    result=gp_port_write(dev->gpdev,(char *)&cmd,sizeof(cmd));
    if (result<0) return result;
    return GP_OK;
}

    /* Filenames are always 12 bytes long */
static int agfa_send_file_command(const char *filename, CameraPrivateLibrary *dev) {
    
    struct agfa_file_command file_cmd;
    int result;
   
    file_cmd.length=0x0c;
    strncpy(file_cmd.filename,filename,12);
    result=gp_port_write(dev->gpdev,(char *)&file_cmd,sizeof(file_cmd));
    if (result<0) return result;
    return GP_OK;
}

    /* USB-only */
static int agfa_read(CameraPrivateLibrary *dev, void *buffer, int len) {

    return gp_port_read(dev->gpdev, buffer, len);
}


int agfa_reset(CameraPrivateLibrary *dev) {
    int ret;
   
    ret=agfa_send_command(AGFA_RESET,0,dev);
    if (ret<0) return ret;
   
    return (GP_OK);
}

    /* Status is a 60 byte array.  I have no clue what it does */
int agfa_get_status(CameraPrivateLibrary *dev, int *taken,
        int *available, int *rawcount) {

    unsigned char ss[0x60];
    int ret;

   
    ret=agfa_send_command(AGFA_STATUS, 0, dev);
   
    if (ret < 0) {
       fprintf(stderr, "agfa_get_storage_status: error sending command\n");
       return ret;
    }

    ret = agfa_read(dev, (unsigned char *)&ss, sizeof(ss));
    if (ret < 0) {
       fprintf(stderr, "agfa_get_storage_status: error getting count\n");
       return ret;
    }
       
    agfa_reset(dev); 
   
    return (GP_OK);
}

    /* Below contributed by Ben Hague <benhague@btinternet.com> */
int agfa_capture(CameraPrivateLibrary *dev, CameraFilePath *path) {
    /*FIXME: Not fully implemented according to the gphoto2 spec.*/
    /*Should also save taken picture, and then delete it from the camera*/
    /*but when I try to do that it just hangs*/
        
    int ret,taken;
      
    printf("Agfa take a picture\n");
    ret=agfa_send_command(AGFA_TAKEPIC2,0,dev);
    ret=agfa_send_command(AGFA_TAKEPIC1,0,dev);
    ret=agfa_send_command(AGFA_TAKEPIC3,0,dev);
    ret=agfa_send_command(AGFA_TAKEPIC2,0,dev);
    
    /*Not sure if this delay is necessary, but it was used in the windows driver*/
    /*delay(20); */
    sleep(20);
    /*Again, three times in windows driver*/
    taken = agfa_photos_taken(dev);
    taken = agfa_photos_taken(dev);
    taken = agfa_photos_taken(dev);
    /*This seems to do some kind of reset, but does cause the camera to start responding again*/
    ret=agfa_send_command(AGFA_GET_NAMES, 0, dev);

    return (GP_OK);
}


int agfa_photos_taken(CameraPrivateLibrary *dev) {
   
    int ret,numpics;

    ret=agfa_send_command(AGFA_GET_NUM_PICS, 0, dev);

    if (ret < 0) {
       fprintf(stderr, "agfa_get_storage_status: error sending command\n");
       return ret;
    }

    ret = agfa_read(dev, &numpics, sizeof(numpics));
    if (ret < 0) {
       fprintf(stderr, "agfa_get_storage_status: error getting count\n");
       return ret;
    }
    return numpics;

}


int agfa_get_file_list(CameraPrivateLibrary *dev) {

    char *buffer;
    int ret, taken, buflen;

    
    /* It seems we need to do a "reset" packet before reading names?? */
   
    agfa_reset(dev);
   
    if ( (taken=agfa_photos_taken(dev)) < 0)
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

    ret=agfa_send_command(AGFA_GET_NAMES, buflen, dev);
    if (ret < 0) {
       free(buffer);
       return ret;
    }

    ret = agfa_read(dev, (void *)buffer, buflen);
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
    taken=agfa_photos_taken(dev);
    agfa_get_thumb_size(dev,dev->file_list);
#endif 
    return GP_OK;
}

int agfa_get_thumb_size(CameraPrivateLibrary *dev, const char *filename) {
 
    int ret,temp,size; 
   
    ret=agfa_send_command(AGFA_GET_THUMB_SIZE,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
       
    agfa_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = agfa_read(dev, &size, sizeof(size));        
    if (ret<0) return ret;
    
    return size;
   
}

int agfa_get_thumb(CameraPrivateLibrary *dev, const char *filename,
		   unsigned char *data,int size) {

    int ret,temp; 
   
    ret = agfa_send_command(AGFA_GET_THUMB,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
       
    ret=agfa_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = agfa_read(dev, data, size);        
    if (ret<0) return ret;
#if 0
           /* Is this needed? */
        agfa_photos_taken(dev,&ret);
   
        ret=agfa_send_command(AGFA_END_OF_THUMB,0,dev);
        if (ret<0) return ret;
   
        ret = agfa_read(dev, temp_string, 8);        
        if (ret<0) return ret;   
#endif   
   
    return GP_OK;
   
}

int agfa_get_pic_size(CameraPrivateLibrary *dev, const char *filename) {
 
    int ret,temp,size; 
   
    ret=agfa_send_command(AGFA_GET_PIC_SIZE,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
    
    ret=agfa_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = agfa_read(dev, &size, sizeof(size));        
    if (ret<0) return ret;
    
    return size;
   
}

int agfa_get_pic(CameraPrivateLibrary *dev, const char *filename,
		   unsigned char *data,int size) {
   
    int ret,temp; 
   
    ret = agfa_send_command(AGFA_GET_PIC,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
    
    ret=agfa_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = agfa_read(dev, data, size);        
    if (ret<0) return ret;

#if 0
       /* Have to do this after getting pic ? */
    ret=agfa_send_command(AGFA_DONE_PIC,0,dev);
    if (ret<0) return ret;
#endif
   
    return GP_OK;
   
}

   /* thanks to heathhey3@hotmail.com for sending me the trace */
   /* to implement this */
int agfa_delete_picture(CameraPrivateLibrary *dev, const char *filename) {
   
    int ret,temp,taken; 
    char data[4],*buffer;
    int size=4,buflen;
   
       /* yes, we do this twice?? */
    taken=agfa_photos_taken(dev);
    taken=agfa_photos_taken(dev);
    
    ret = agfa_send_command(AGFA_GET_PIC_SIZE,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
       
      /* Some traces show sending other than the file we want deleted? */
    ret=agfa_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = agfa_read(dev, data, size);        
    if (ret<0) return ret;
   
          /* Check num taken AGAIN */
    taken=agfa_photos_taken(dev);
  
    ret = agfa_send_command(AGFA_GET_PIC_SIZE,0,dev);
    if (ret<0) return ret;
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
    
    ret=agfa_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = agfa_read(dev, data, size);        
    if (ret<0) return ret;
   
        /* Check num taken AGAIN */
    taken=agfa_photos_taken(dev);
       
    ret=agfa_send_command(AGFA_DELETE,0,dev);
    if (ret<0) return ret;
      
        /* read ff 0f ??? */
    ret = agfa_read(dev, data, size);        
    if (ret<0) return ret;
   
    ret = agfa_send_file_command(filename,dev);
    if (ret<0) return ret;
   
        /* This is the point we notices that in fact a pic is missing */
        /* Why do it 4 times??? Timing?? Who knows */
    taken=agfa_photos_taken(dev);
    taken=agfa_photos_taken(dev);
    taken=agfa_photos_taken(dev);
    taken=agfa_photos_taken(dev);
    
       /* Why +1 ? */
    buflen = ((taken+1) * 13)+1;  /* 12 char filenames and space for each */
                                  /* plus trailing NULL */
    buffer = malloc(buflen);
        
    if (!buffer) {
       GP_DEBUG("Could not allocate %i bytes!",
		       buflen);
       return (GP_ERROR_NO_MEMORY);
    }

    ret=agfa_send_command(AGFA_GET_NAMES, buflen,dev);
    if (ret < 0) {
       free(buffer);
       return ret;
    }

    ret = agfa_read(dev, (void *)buffer, buflen);
    if (ret < 0) {
       free(buffer);
       return ret;
    }

    if (dev->file_list) free(dev->file_list);
    dev->file_list = buffer;
   
    ret=agfa_send_command(AGFA_GET_PIC_SIZE,0,dev);
    if (ret<0) return ret;
    
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) return ret;
   
    ret=agfa_send_file_command(filename,dev);
    if (ret<0) return ret;
   
    ret = agfa_read(dev, data, size);        
    if (ret<0) return ret; 
   
    return 0;

}

