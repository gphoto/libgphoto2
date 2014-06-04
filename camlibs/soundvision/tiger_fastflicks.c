/*
 * tiger_fastflicks.c
 *
 *  Commands specific to soundvision camears with usbid
 *  0x0919:0x0100 and possibly others.
 * 
 *  Tiger FastFlicks was first camera of this type
 *  to be supported.
 * 
 *  Initial Support added by Dr. William Bland <wjb@abstractnonsense.com> 
 *  Other help from  Adrien Hernot <amh@BSD-DK.dk>
 * 
 * Vince Weaver got such a camera in Jan 2003 and massively
 * re-wrote things.
 * 
 * Copyright 2002-2003 Vince Weaver <vince@deater.net>
 */

/* Routines for the 0x919:0x100 cameras that aren't compatible */
/* with the others go in this file                             */

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


    /* Used at start of most tiger commands.  Also */
    /* Seems to set the camera into "PC" mode      */
int tiger_set_pc_mode(CameraPrivateLibrary *dev) {
   
    int result;

    GP_DEBUG("tiger_set_pc_mode");
      
    result=soundvision_send_command(SOUNDVISION_START_TRANSACTION,0,dev);
    if (result<0) goto set_pc_error;
   
    result=soundvision_get_revision(dev,NULL);
    if (result<0) goto set_pc_error;
   
    result=soundvision_send_command(SOUNDVISION_SETPC1,0,dev);
    if (result<0) goto set_pc_error;
   
    result=soundvision_send_command(SOUNDVISION_SETPC2,0,dev);
    if (result<0) goto set_pc_error;
   
    return GP_OK;
   
set_pc_error:   
    return result;
}

   
   

    /* This is packet-for-packet what windows does */
    /* The camera takes it all in stride.          */
    /* Yet uploaded file never appears.  Why??     */
int tiger_upload_file(CameraPrivateLibrary *dev, 
		      const char *filename,
		      const char *data,
		      long size) {
    int result=0;
    char return_value[4];
    
    uint32_t our_size;
    char *our_data=NULL;

       /* When we upload, the first 3 bytes are little-endian */
       /* File-size followed by the actual file               */
    our_size=size+4;
    our_data=calloc(our_size,sizeof(char));
    if (our_data==NULL) {
       goto upload_error;
    }
   
    htole32a(&our_data[0],size);
    memcpy(our_data+4,data,size);
   
   
    GP_DEBUG("File: %s Size=%ld\n",filename,size);
/*  for(result=0;result<our_size;result++) {
       printf("%x ",our_data[result]);
    }
*/  

    result=tiger_set_pc_mode(dev);
    if (result<0) goto upload_error;
   
    result=soundvision_get_revision(dev,NULL);
    if (result<0) goto upload_error;
   
    result=soundvision_send_command(SOUNDVISION_GET_MEM_FREE,0,dev);
    if (result<0) goto upload_error;
   
    result=soundvision_read(dev, &return_value, sizeof(return_value));        
    if (result<0) goto upload_error;
   
    result=soundvision_send_command(SOUNDVISION_PUT_FILE,size,dev);
    if (result<0) goto upload_error;
    
    result=soundvision_read(dev, &return_value, sizeof(return_value));        
    if (result<0) goto upload_error;
   
    result=gp_port_write(dev->gpdev,our_data,our_size);
    if (result<0) goto upload_error;
   
    free(our_data);
    our_data=NULL;

#if 0 
    /* Some traces show the following, though most likely */
    /* this is just the windows driver updating the file list */
   
    result=soundvision_photos_taken(dev);
    result=soundvision_get_file_list(dev);
    
    result=soundvision_send_command(SOUNDVISION_GET_PIC_SIZE,0,dev);
    if (result<0) goto upload_error;
    
    result=soundvision_read(dev, &return_value, sizeof(return_value));        
    if (result<0) goto upload_error;
   
    result=soundvision_send_file_command("000VINCE.JPG",dev);
    
   result=soundvision_read(dev, &return_value, sizeof(return_value));        
    if (result<0) goto upload_error;
   
    result=soundvision_send_command(SOUNDVISION_DONE_TRANSACTION,0,dev);
    if (result<0) goto upload_error;
#endif      
    return GP_OK;
   
upload_error:   
    if (our_data!=NULL) free(our_data);
    GP_DEBUG("Error in tiger_upload_file");
    return result;
}

int tiger_delete_picture(CameraPrivateLibrary *dev, const char *filename) {
   
    int32_t ret,temp; 
   
    ret = tiger_set_pc_mode(dev);
    if (ret<0) goto delete_pic_error;
   
    ret = soundvision_send_command(SOUNDVISION_DELETE,0,dev);
    if (ret<0) goto delete_pic_error;
   
       /* should get fff if all is well */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) goto delete_pic_error;

    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) goto delete_pic_error;
   
    ret = soundvision_send_command(SOUNDVISION_DONE_TRANSACTION,0,dev);
    if (ret<0) goto delete_pic_error;

    return GP_OK;

delete_pic_error:   
    return ret;
   
}

int tiger_get_mem(CameraPrivateLibrary *dev, int *num_pics, int *mem_total, int *mem_free) {

    int result=0;
    int temp_result;

    *num_pics=soundvision_photos_taken(dev);
    if (*num_pics<0) goto get_mem_error;
   
    result=soundvision_get_revision(dev,NULL);
    if (result<0) goto get_mem_error;

    result=soundvision_send_command(SOUNDVISION_GET_MEM_TOTAL,0,dev);
    if (result<0) goto get_mem_error;
    
    result=soundvision_read(dev, &temp_result, sizeof(temp_result));        
    if (result<0) goto get_mem_error;
   
    *mem_total=le32toh(temp_result);
   
    result=soundvision_send_command(SOUNDVISION_GET_MEM_FREE,0,dev);
    if (result<0) goto get_mem_error;
    
    result=soundvision_read(dev, &temp_result, sizeof(temp_result));        
    if (result<0) goto get_mem_error;
   
    *mem_free=le32toh(temp_result);    
   
    return GP_OK;
   
get_mem_error:
    GP_DEBUG("Error in tiger_get_mem");
    return result;

}

   
int tiger_capture(CameraPrivateLibrary *dev, CameraFilePath *path) {
   
    int result=0,start_pics,num_pics,mem_total,mem_free;
       
    result=soundvision_send_command(SOUNDVISION_START_TRANSACTION,0,dev);
    if (result<0) goto tiger_capture_error;
   
    result=soundvision_get_revision(dev,NULL);
    if (result<0) goto tiger_capture_error;
   
    result=tiger_get_mem(dev,&start_pics,&mem_total,&mem_free);
    if (result<0) goto tiger_capture_error;
    
    result=soundvision_send_command(SOUNDVISION_SETPC2,0,dev);
    if (result<0) goto tiger_capture_error;
   
    result=soundvision_send_command(SOUNDVISION_TAKEPIC3,0,dev);
    if (result<0) goto tiger_capture_error;

    result=soundvision_send_command(SOUNDVISION_SETPC1,0,dev);
    if (result<0) goto tiger_capture_error;
    
    result=tiger_get_mem(dev,&num_pics,&mem_total,&mem_free);
    if (result<0) goto tiger_capture_error;
   
    while(num_pics==start_pics) {
       sleep(4);
       result=tiger_get_mem(dev,&num_pics,&mem_total,&mem_free);
       if (result<0) goto tiger_capture_error;
    }
   
    result=tiger_get_mem(dev,&num_pics,&mem_total,&mem_free);
    if (result<0) goto tiger_capture_error;
     
    return GP_OK;
   
tiger_capture_error:
    GP_DEBUG("ERROR with tiger_capture");
    return result;
   
}


	 
int tiger_get_file_list(CameraPrivateLibrary *dev) {

    char *buffer=NULL;
    int32_t ret, taken, buflen,i;

    ret=tiger_set_pc_mode(dev);
    if (ret<0) goto list_files_error;
   
    if ( (taken=soundvision_photos_taken(dev)) < 0) {
       ret=taken;
       goto list_files_error;
    }
   
    dev->num_pictures = taken;

    if (taken>0) {
	
       buflen = (taken * 13)+1;  /* 12 char filenames and space for each */
                                 /* plus trailing NULL */
    
       buffer = malloc(buflen);
        
       if (!buffer) {
          GP_DEBUG("Could not allocate %i bytes!",buflen);
	  ret=GP_ERROR_NO_MEMORY;
	  goto list_files_error;
       }
       
       ret=soundvision_send_command(SOUNDVISION_GET_NAMES, buflen, dev);
       if (ret < 0) {
          goto list_files_error;
       }
       

       ret = soundvision_read(dev, (void *)buffer, buflen);
       if (ret < 0) {
          goto list_files_error;
       }
       
 
       if (dev->file_list) free(dev->file_list);

       dev->file_list = malloc(taken * 13);
    
       if (!dev->file_list) {
          GP_DEBUG("Could not allocate %i bytes!",taken*13);
          ret=GP_ERROR_NO_MEMORY;
	  goto list_files_error;
       }

       for(i=0;i<taken*13;i++) if (buffer[i]==' ') buffer[i]='\0';
       memcpy(dev->file_list, buffer, taken * 13);
       free(buffer);
       buffer=NULL;
       
       

    }
   
   
    ret=soundvision_send_command(SOUNDVISION_DONE_TRANSACTION, 0, dev);   
    if (ret<0) goto list_files_error;    

          /* If we have >1 pics we should stat a file?*/
          /* Some traces do, some don't...            */
/*    if (taken>0)
       soundvision_get_pic_size(dev,dev->file_list);
  */
   
    return GP_OK;
   
list_files_error:
       
    if (buffer!=NULL) free(buffer);
    return ret;
}

int tiger_get_pic(CameraPrivateLibrary *dev, const char *filename,
		   unsigned char *data,int size) {
   
    int32_t ret,temp; 

    GP_DEBUG("tiger_get_pic");
   
    dev->odd_command=1;
   
    ret=soundvision_get_revision(dev,NULL);
   
    ret = soundvision_send_command(SOUNDVISION_GET_PIC,0,dev);
    if (ret<0) goto get_pic_error;
   
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) goto get_pic_error;
    
    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) goto get_pic_error;
   
    ret = soundvision_read(dev, data, size);        
    if (ret<0) goto get_pic_error;

    ret=soundvision_send_command(SOUNDVISION_DONE_TRANSACTION,0,dev);
    if (ret<0) goto get_pic_error;
   
    return GP_OK;

get_pic_error:
    return ret;
   
   
}

int tiger_get_pic_size(CameraPrivateLibrary *dev, const char *filename) {
 
    int32_t ret,temp;
    uint32_t size; 

    GP_DEBUG("tiger_get_pic_size");
   
    ret=soundvision_send_command(SOUNDVISION_GET_PIC_SIZE,0,dev);
    if (ret<0) goto pic_size_error;

       /* should check that we get 0x0fff */
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) goto pic_size_error;
    
    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) goto pic_size_error;
   
    ret = soundvision_read(dev, &size, sizeof(size));        
    if (ret<0) goto pic_size_error;
    
    return le32toh(size);
   
pic_size_error:
    return ret;
   
}


int tiger_get_thumb_size(CameraPrivateLibrary *dev, const char *filename) {
 
    int32_t ret,temp;
    uint32_t size; 

   
    GP_DEBUG("tiger_get_thumb_size");
   
    ret=soundvision_send_command(SOUNDVISION_GET_THUMB_SIZE,0,dev);
    if (ret<0) goto thumb_size_error;

    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) goto thumb_size_error;
       
    ret = soundvision_send_file_command(filename,dev);
    if (ret<0) goto thumb_size_error;
   
    ret = soundvision_read(dev, &size, sizeof(size));        
    if (ret<0) goto thumb_size_error;
    
    return le32toh(size);

thumb_size_error:
    return ret;   
}

int tiger_get_thumb(CameraPrivateLibrary *dev, const char *filename,
		   unsigned char *data,int size) {

    int32_t ret,temp; 
   
    ret=soundvision_get_revision(dev,NULL);
   
    ret = soundvision_send_command(SOUNDVISION_GET_THUMB,0,dev);
    if (ret<0) goto get_thumb_error;
   
    ret = soundvision_read(dev, &temp, sizeof(temp));        
    if (ret<0) goto get_thumb_error;
    
    ret=soundvision_send_file_command(filename,dev);
    if (ret<0) goto get_thumb_error;
   
    ret = soundvision_read(dev, data, size);        
    if (ret<0) goto get_thumb_error;

    ret=soundvision_send_command(SOUNDVISION_DONE_TRANSACTION,0,dev);
    if (ret<0) goto get_thumb_error;
   
    return GP_OK;
   
get_thumb_error:

    return ret;
   
}
