/*
 * commands.c
 *
 *  Command set for the agfa cameras
 *
 * Copyright 2001 Vince Weaver <vince@deater.net>
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#ifdef OS2
#include <db.h>
#endif
#include <netinet/in.h>

#include "agfa.h"

#include <gphoto2-port.h>

    /* Regular commands always 8 bytes long */
static void build_command(struct agfa_command *cmd, int command, int argument) {

    cmd->length = 8;
    cmd->command = command;
    cmd->argument = argument;
}

    /* Filenames are always 12 bytes long */
static void build_file_command(struct agfa_file_command *cmd, const char *filename) {
   
    cmd->length=0x0c;
    strncpy(cmd->filename,filename,12);
}

void agfa_reset(struct agfa_device *dev) {
    int ret;
    struct agfa_command cmd;
   
    build_command(&cmd,AGFA_RESET,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));   
}

    /* Status is a 60 byte array.  I have no clue what it does */
int agfa_get_status(struct agfa_device *dev, int *taken,
        int *available, int *rawcount) {

    struct agfa_command cmd;
    unsigned char ss[0x60];
    int ret;

    build_command(&cmd, AGFA_STATUS, 0);

    ret = agfa_send(dev, &cmd, sizeof(cmd));
   
    if (ret < 0) {
       fprintf(stderr, "agfa_get_storage_status: error sending command\n");
       return -1;
    }

    ret = agfa_read(dev, (unsigned char *)&ss, sizeof(ss));
    if (ret < 0) {
       fprintf(stderr, "agfa_get_storage_status: error getting count\n");
       return -1;
    }
       
    agfa_reset(dev); 
   
    return 0;
}

    /* Below contributed by Ben Hague <benhague@btinternet.com> */

int agfa_capture(struct agfa_device *dev, const char *path) {
    /*FIXME: Not fully implemented according to the gphoto2 spec.*/
    /*Should also save taken picture, and then delete it from the camera*/
    /*but when I try to do that it just hangs*/
        
    struct agfa_command cmd;
    int ret,taken;
      
    printf("Agfa take a picture\n");
    build_command(&cmd, AGFA_TAKEPIC2,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));
    build_command(&cmd, AGFA_TAKEPIC1,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));
    build_command(&cmd, AGFA_TAKEPIC3,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));
    build_command(&cmd, AGFA_TAKEPIC2,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));
    
    /*Not sure if this delay is necessary, but it was used in the windows driver*/
    /*delay(20); */
    sleep(20);
    /*Again, three times in windows driver*/
    ret = agfa_photos_taken(dev,&taken);
    ret = agfa_photos_taken(dev,&taken);
    ret = agfa_photos_taken(dev,&taken);
    /*This seems to do some kind of reset, but does cause the camera to start responding again*/
    build_command(&cmd,AGFA_GET_NAMES, 0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));
    return 0;
}


int agfa_photos_taken(struct agfa_device *dev, int *taken) {
   
    struct agfa_command cmd;
    int ret,numpics;

    build_command(&cmd, AGFA_GET_NUM_PICS, 0);

    ret = agfa_send(dev, &cmd, sizeof(cmd));
   
    if (ret < 0) {
       fprintf(stderr, "agfa_get_storage_status: error sending command\n");
       return -1;
    }

    ret = agfa_read(dev, &numpics, sizeof(numpics));
    if (ret < 0) {
       fprintf(stderr, "agfa_get_storage_status: error getting count\n");
       return -1;
    }
    *taken=numpics;

    return 0;
}


int agfa_get_file_list(struct agfa_device *dev) {

    struct agfa_command cmd;
    char *buffer;
    int ret, taken, buflen;

    /* It seems we need to do a "reset" packet before reading names?? */
   
    agfa_reset(dev);
   
    if (agfa_photos_taken(dev, &taken) < 0)
       return -1;
   
    dev->num_pictures = taken;

    
    buflen = (taken * 13)+1;  /* 12 char filenames and space for each */
                              /* plus trailing NULL */
    
    buffer = malloc(buflen);
        
    if (!buffer) {
       fprintf(stderr, "agfa_get_file_list: error allocating %d bytes\n", buflen);
       return -1;
    }

    build_command(&cmd,AGFA_GET_NAMES, buflen);

    ret = agfa_send(dev, &cmd, sizeof(cmd));
    if (ret < 0) {
       fprintf(stderr, "agfa_get_file_list: error sending command\n");
       return -1;
    }

    ret = agfa_read(dev, (void *)buffer, buflen);
    if (ret < 0) {
       fprintf(stderr, "agfa_get_file_list: error receiving data\n");
       return -1;
    }

    if (dev->file_list) free(dev->file_list);

    dev->file_list = malloc(taken * 13);
    if (!dev->file_list) {
       fprintf(stderr, "agfa_get_file_list: error allocating file_list memory\n");
       return -1;
    }

    memcpy(dev->file_list, buffer, taken * 13);

    free(buffer);
#if 0
    agfa_photos_taken(dev, &taken);
    agfa_get_thumb_size(dev,dev->file_list);
#endif 
    return 0;
}

int agfa_get_thumb_size(struct agfa_device *dev, const char *filename) {
 
    struct agfa_command cmd;    
    struct agfa_file_command file_cmd;
    int ret,temp,size; 
   
    build_command(&cmd,AGFA_GET_THUMB_SIZE,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));  
   
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
     }
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
    
    build_file_command(&file_cmd,filename);
    ret = agfa_send(dev,&file_cmd, sizeof(file_cmd));
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
   
    ret = agfa_read(dev, &size, sizeof(size));        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
    
    return size;
   
}

int agfa_get_thumb(struct agfa_device *dev, const char *filename,
		   unsigned char *data,int size) {
   
    struct agfa_command cmd;   
    struct agfa_file_command file_cmd;
    int ret,temp; 
/*    unsigned char temp_string[8]; */
    
    build_command(&cmd,AGFA_GET_THUMB,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));  
   
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
    
    build_file_command(&file_cmd,filename);
    ret = agfa_send(dev,&file_cmd, sizeof(file_cmd));
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
   
    ret = agfa_read(dev, data, size);        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
#if 0
           /* Is this needed? */
        agfa_photos_taken(dev,&ret);
   
        build_command(&cmd,AGFA_END_OF_THUMB,0);
        ret = agfa_send(dev,&cmd, sizeof(cmd));
        if (ret<0) {
	   printf("Error sending command!\n");
	   return -1;
	}
   
        ret = agfa_read(dev, temp_string, 8);        
        if (ret<0) {
	   printf("Error sending command!\n");
	   return -1;
	}   

#endif   
    return 0;
   
}

int agfa_get_pic_size(struct agfa_device *dev, const char *filename) {
 
    struct agfa_command cmd;    
    struct agfa_file_command file_cmd;
    int ret,temp,size; 
   
    build_command(&cmd,AGFA_GET_PIC_SIZE,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));  
   
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
    
    build_file_command(&file_cmd,filename);
    ret = agfa_send(dev,&file_cmd, sizeof(file_cmd));
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
   
    ret = agfa_read(dev, &size, sizeof(size));        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
    
    return size;
   
}

int agfa_get_pic(struct agfa_device *dev, const char *filename,
		   unsigned char *data,int size) {
   
    struct agfa_command cmd;   
    struct agfa_file_command file_cmd;
    int ret,temp; 
//  int taken;
   
//    agfa_photos_taken(dev,&taken);
//  agfa_get_pic_size(dev,filename);
   
    build_command(&cmd,AGFA_GET_PIC,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));  
   
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
    
    build_file_command(&file_cmd,filename);
    ret = agfa_send(dev,&file_cmd, sizeof(file_cmd));
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
   
    ret = agfa_read(dev, data, size);        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   

#if 0
       /* Have to do this after getting pic */
    build_command(&cmd,AGFA_DONE_PIC,0);
    ret=agfa_send(dev,&cmd,sizeof(cmd));
#endif
   
    return 0;
   
}

   /* thanks to heathhey3@hotmail.com for sending me the trace */
   /* to implement this */
int agfa_delete_picture(struct agfa_device *dev, const char *filename)
{
   
    struct agfa_command cmd;   
    struct agfa_file_command file_cmd;
    int ret,temp,taken; 
    char data[4],*buffer;
    int size=4,buflen;
   
       /* yes, we do this twice?? */
    agfa_photos_taken(dev,&taken);
    agfa_photos_taken(dev,&taken);
    
    build_command(&cmd,AGFA_GET_PIC_SIZE,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));  
   
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
    
      /* Some traces show sending other than the file we want deleted? */
    build_file_command(&file_cmd,filename);
    ret = agfa_send(dev,&file_cmd, sizeof(file_cmd));
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
   
    ret = agfa_read(dev, data, size);        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
          /* Check num taken AGAIN */
    agfa_photos_taken(dev,&taken);
  
    build_command(&cmd,AGFA_GET_PIC_SIZE,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));  
   
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
    
    build_file_command(&file_cmd,filename);
    ret = agfa_send(dev,&file_cmd, sizeof(file_cmd));
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
   
    ret = agfa_read(dev, data, size);        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   

        /* Check num taken AGAIN */
    agfa_photos_taken(dev,&taken);
       
    build_command(&cmd,AGFA_DELETE,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));   
   
   
        /* read ff 0f ??? */
    ret = agfa_read(dev, data, size);        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   

    build_file_command(&file_cmd,filename);
    ret = agfa_send(dev,&file_cmd, sizeof(file_cmd));
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
        /* This is the point we notices that in fact a pic is missing */
        /* Why do it 4 times??? Timing?? Who knows */
    agfa_photos_taken(dev,&taken);
    agfa_photos_taken(dev,&taken);
    agfa_photos_taken(dev,&taken);
    agfa_photos_taken(dev,&taken);
    
       /* Why +1 ? */
    buflen = ((taken+1) * 13)+1;  /* 12 char filenames and space for each */
                              /* plus trailing NULL */
    buffer = malloc(buflen);
        
    if (!buffer) {
       fprintf(stderr, "agfa_get_file_list: error allocating %d bytes\n", buflen);
       return -1;
    }

    build_command(&cmd,AGFA_GET_NAMES, buflen);

    ret = agfa_send(dev, &cmd, sizeof(cmd));
    if (ret < 0) {
       fprintf(stderr, "agfa_get_file_list: error sending command\n");
       return -1;
    }

    ret = agfa_read(dev, (void *)buffer, buflen);
    if (ret < 0) {
       fprintf(stderr, "agfa_get_file_list: error receiving data\n");
       return -1;
    }

    build_command(&cmd,AGFA_GET_PIC_SIZE,0);
    ret = agfa_send(dev, &cmd, sizeof(cmd));  
   
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
     
       /* always returns ff 0f 00 00 ??? */
    ret = agfa_read(dev, &temp, sizeof(temp));        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   

    build_file_command(&file_cmd,filename);
    ret = agfa_send(dev,&file_cmd, sizeof(file_cmd));
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }
   
    ret = agfa_read(dev, data, size);        
    if (ret<0) {
       printf("Error sending command!\n");
       return -1;
    }   
   
   
    return 0;

}

