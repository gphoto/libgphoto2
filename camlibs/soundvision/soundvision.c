/*
 * soundvision.c
 *
 * For cameras using the Clarity2 chip made by Soundvision Inc.
 * Made entirely w/o Soundvision's help.  They would not give
 * documentation.  Reverse-engineered from usb-traces.
 * 
 * Copyright 2002-2003 Vince Weaver <vince@deater.net>
 * 
 * Originally based on the digita driver 
 * Copyright 1999-2000 Johannes Erdfelt, VA Linux Systems
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#ifdef OS2
#include <db.h>
#endif

#include <gphoto2/gphoto2.h>

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

#include "soundvision.h"

#include <gphoto2/gphoto2-port.h>

#define GP_MODULE "soundvision"

static const struct {
   char *name;
   unsigned short idVendor;
   unsigned short idProduct;
   char serial;
} models[] = {
        {"Agfa:ePhoto CL18",0x06bd,0x0403,0},
   
        {"Mustek:gSmart 350",0x055f,0xa350,0},
   
        {"RCA:CDS1005",0x0784,0x0100,0},
        {"Ixla:DualCam 640",0x0784,0x0100,0},

        {"Pretec:dc530",0x0784,0x5300,0},
   
        {"Generic SoundVision:Clarity2",0x0919,0x100,0},
        {"Argus:DC-2200",0x0919,0x100,0},
        {"CoolCam:CP086",0x0919,0x100,0},
        {"DigitalDream:l'elite",0x0919,0x0100,0},
        {"FujiFilm:@xia ix-100",0x0919,0x0100,0},
        {"Media-Tech:mt-406",0x0919,0x0100,0},
        {"Oregon Scientific:DShot II",0x0919,0x0100,0},
        {"Oregon Scientific:DShot III",0x0919,0x0100,0},
        {"Scott:APX 30",0x0919,0x0100,0},
        {"StarCam:CP086",0x0919,0x100,0},
        {"Tiger:Fast Flicks",0x0919,0x0100,0},   
        {NULL,0,0,0}
};

int camera_id(CameraText *id) {
        
    strcpy(id->text, "soundvision");
        
    return GP_OK;
}


int camera_abilities(CameraAbilitiesList *list) {
        
    int i;
    CameraAbilities a;

    for(i=0; models[i].name; i++) {
       memset(&a, 0, sizeof(a));
       strcpy(a.model, models[i].name);
       
          /* Agfa and Dshot types owned by author and tested */
          /* Others the testing not quite certain            */
       if ( (models[i].idVendor==0x06bd) || (models[i].idVendor==0x0919))
          a.status = GP_DRIVER_STATUS_PRODUCTION;
       else
          a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
       
          /* For now all are USB.  Have the spec for Serial */
          /* If ever one shows up                           */
       a.port       = GP_PORT_USB;
       a.speed[0] = 0;
       a.usb_vendor = models[i].idVendor;
       a.usb_product= models[i].idProduct;
       
          /* All should support image capture */
       a.operations     = 	GP_OPERATION_CAPTURE_IMAGE;
       
          /* Folders not supported on any */
       a.folder_operations = 	GP_FOLDER_OPERATION_NONE;
       
          /* Dshot compat can upload files */
       if (models[i].idVendor==0x0919) {
          a.file_operations|=GP_FOLDER_OPERATION_PUT_FILE;	    
       }       
       
       a.file_operations   = 	GP_FILE_OPERATION_PREVIEW | 
				GP_FILE_OPERATION_DELETE;

       gp_abilities_list_append(list, a);
    }
    return GP_OK;
}

static int camera_exit (Camera *camera, GPContext *context) {

    GP_DEBUG("MAKE ME GP_DEBUG Reset: %i  pics: %i  odd_command: %i\n",
	                          camera->pl->reset_times,
	                          camera->pl->num_pictures,
	                          camera->pl->odd_command);
  
       /* We _must_ reset an even number of times ?? */
       /* It's more complicated than that.  *sigh*   */
    if (camera->pl->reset_times==1) {
       soundvision_reset (camera->pl,NULL,NULL);
#if 0       
          /* Odd number of pics */
       if (camera->pl->num_pictures%2==1) { 
	  if (!camera->pl->odd_command) 
	     soundvision_reset (camera->pl,NULL,NULL);
       }
       else {
	  if (camera->pl->odd_command) 
	     soundvision_reset (camera->pl,NULL,NULL);
       }
#endif       
    }
   
       
    if (camera->pl) {
	if (camera->pl->file_list) {
	    free(camera->pl->file_list);
	    camera->pl->file_list = NULL;
	}
	free(camera->pl);
	camera->pl = NULL;
    }

    return GP_OK;
}

static int file_list_func (CameraFilesystem *fs, const char *folder, 
			   CameraList *list, void *data, GPContext *context) {
 
    Camera *camera=data;
    int i;
    char temp_file[14];

    GP_DEBUG ("camera_file_list %s\n", folder);
   
    if (soundvision_get_file_list(camera->pl) < 0) {
       GP_DEBUG ("Could not soundvision_file_list!");
       return GP_ERROR;
    }
       
    for(i=0; i < camera->pl->num_pictures; i++) {
       strncpy(temp_file,camera->pl->file_list+(13*i),12);
       temp_file[12]=0;
       gp_list_append (list, temp_file, NULL);
    }
    return GP_OK;
}


static int soundvision_file_get (Camera *camera, const char *filename, int thumbnail, 
			    unsigned char **data, int *size) {

    int buflen,throwaway,result;
   
    if (thumbnail) GP_DEBUG( "Getting thumbnail '%s'...",filename);
    else GP_DEBUG( "Getting file '%s'...",filename);

    if (camera->pl->device_type==SOUNDVISION_TIGERFASTFLICKS) {
       result=tiger_set_pc_mode(camera->pl);
       if (thumbnail) buflen=soundvision_get_thumb_size(camera->pl,filename);
       else buflen=soundvision_get_pic_size(camera->pl,filename);
       if (buflen < 0) return buflen;
    } else {
       soundvision_reset(camera->pl,NULL,NULL);
          
          /* Always have to check num photos,
	   * then pic size no matter what.  Otherwise
	   * the camera will stop responding 
	   */
       throwaway=soundvision_photos_taken(camera->pl);
       if (throwaway<0) {
	  result=throwaway;
	  goto file_get_error;
       }
       
       
          /* The below two lines might look wrong, but they aren't! */
       buflen = soundvision_get_pic_size(camera->pl,filename);
       if (thumbnail) buflen=soundvision_get_thumb_size(camera->pl,filename);
    }
   
   

       
       /* Don't try to download if size equals zero! */
    if (buflen) {
   
       *data = malloc(buflen+1);
        
       if (!*data) {
	  result=GP_ERROR_NO_MEMORY;
	  goto file_get_error;
       }
       
       memset(*data, 0, buflen);

       if (thumbnail) {
          result=soundvision_get_thumb(camera->pl, filename, *data, buflen);
          if (result < 0) {
	     GP_DEBUG("soundvision_get_thumb_failed!");
	     goto file_get_error;
	  }	     
       }
       else {  
          result=soundvision_get_pic(camera->pl, filename, *data, buflen);
          if (result < 0) {
	     GP_DEBUG("soundvision_get_pic_failed!");
             goto file_get_error;
	  }	     
       }
	         
       if (size)
          *size = buflen;       
    }
      
    return GP_OK;

file_get_error:

    if (*data!=NULL) free(*data);
    return result;
}

static int get_file_func (CameraFilesystem *fs, const char *folder, 
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data,
			  GPContext *context) {
    char *pos;
   
    Camera *camera = user_data;
    unsigned char *data = NULL;
    int size,ret;

    switch (type) {
       case GP_FILE_TYPE_NORMAL:
               ret=soundvision_file_get(camera, filename, 0, &data, &size);
               if (ret<0) return ret;
               break;
       case GP_FILE_TYPE_PREVIEW:
               ret=soundvision_file_get(camera, filename, 1, &data, &size);
               if (ret<0) return ret;
               break;
       default:
               return GP_ERROR_NOT_SUPPORTED;
    }
   
    if (!data) return GP_ERROR;

    gp_file_set_data_and_size (file, (char *)data, size);
       /* Maybe skip below if EXIF data present? */
   
       /* As far as I know we only support JPG and MOV */
       /* Maybe some have MP3???                       */
    pos=strchr (filename, '.');   
    if (pos) {
       if ((!strcmp(pos,".JPG")) || ((!strcmp(pos,".jpg"))))
	  gp_file_set_mime_type (file, GP_MIME_JPEG);
       else if (!strcmp(pos,".MOV"))
	  gp_file_set_mime_type (file, GP_MIME_QUICKTIME);
       else
	  gp_file_set_mime_type (file, GP_MIME_UNKNOWN);
    }
   
    return GP_OK;
}

static int camera_summary(Camera *camera, CameraText *summary,
			  GPContext *context) {

    char revision[9];
   
    soundvision_reset(camera->pl,revision,NULL);
   
    if (camera->pl->device_type==SOUNDVISION_TIGERFASTFLICKS) {
       int mem_total,mem_free,num_pics, ret;

       ret = tiger_get_mem(camera->pl,&num_pics,&mem_total,&mem_free);
       if (ret < GP_OK)
           return ret;
       
       sprintf(summary->text, _("Firmware Revision: %8s\n"
				"Pictures:     %i\n"
				"Memory Total: %ikB\n"
				"Memory Free:  %ikB\n"),
				revision,num_pics,mem_total,mem_free);
    } else {
	sprintf(summary->text, _("Firmware Revision: %8s"), revision);
    }
    return GP_OK;
}

static int camera_about(Camera *camera, CameraText *about, GPContext *context) {
        
    strcpy(about->text, _("Soundvision Driver\nVince Weaver <vince@deater.net>\n"));

    return GP_OK;
}


    /* Below contributed by Ben Hague <benhague@btinternet.com> */
static int camera_capture (Camera *camera, CameraCaptureType type,
			   CameraFilePath *path, GPContext *context) {
   
    int result;
   
    if (camera->pl->device_type==SOUNDVISION_AGFACL18)     
       result=agfa_capture(camera->pl,path);
    else if (camera->pl->device_type==SOUNDVISION_TIGERFASTFLICKS) { 
       result=tiger_capture(camera->pl,path);
    }
    else return GP_ERROR_NOT_SUPPORTED;

    if (result < GP_OK)
       return result;
   
    soundvision_get_file_list(camera->pl);
   
       /* For some reason last taken picture is first on tiger? */
       /* Might be last on Agfa.  Who knows.  Craziness.        */
    if (camera->pl->num_pictures<1) return GP_ERROR;
      
    strcpy (path->name,camera->pl->file_list);
    strcpy (path->folder, "/");
/*    gp_filesystem_append (camera->fs, path->folder,
			  path->name, context);*/
    return GP_OK;
}


static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data,
			     GPContext *context) {
  
    Camera *camera = data;

    GP_DEBUG("Deleting '%s' in '%s'...",filename,folder); 
   
    soundvision_delete_picture(camera->pl,filename);
   
       /* Update our file list */
    if (soundvision_get_file_list(camera->pl)<0) return GP_ERROR;
   
    return GP_OK;
}


static int put_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
			  CameraFileType type, CameraFile *file, void *data, GPContext *context) {
   
    Camera *camera=data;
    const char *data_file;
    long unsigned int data_size;
   
    /*
     * Upload the file to the camera. Use gp_file_get_data_and_size, etc.
    */
    GP_DEBUG ("*** put_file_func");
    GP_DEBUG ("*** folder: %s", folder);
    GP_DEBUG ("*** filename: %s", filename);
   
    gp_file_get_data_and_size (file, &data_file, &data_size);
    if ( data_size == 0) {
       gp_context_error (context,
			 _("The file to be uploaded has a null length"));
       return GP_ERROR_BAD_PARAMETERS;
    }

    /* Should check memory here */
   
   /*        if (available_memory < data_size) {
       gp_context_error (context,
			 _("Not enough memory available on the memory card"));
       return GP_ERROR_NO_MEMORY;
     }
   */
   
    tiger_upload_file (camera->pl, filename,data_file,data_size);
   
    return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.put_file_func = put_file_func,
	.del_file_func = delete_file_func,
};

int camera_init(Camera *camera, GPContext *context) {
   
    GPPortSettings settings;
    CameraAbilities a;
    int ret = 0;

       /* First, set up all the function pointers */
    camera->functions->exit         = camera_exit;
    camera->functions->summary      = camera_summary;
    camera->functions->about        = camera_about;
    camera->functions->capture      = camera_capture;
   
    GP_DEBUG ("Initializing the camera\n");

    switch (camera->port->type) {
       case GP_PORT_USB:
            ret=gp_port_get_settings(camera->port,&settings);
            if (ret<0) return ret;
 
            /* Use the defaults the core parsed */

            ret=gp_port_set_settings(camera->port,settings);
            if (ret<0) return ret;       
            break;
       
       case GP_PORT_SERIAL:
            return GP_ERROR_IO_SUPPORTED_SERIAL;
       default: 
            return GP_ERROR_NOT_SUPPORTED;
    }

				       
   
       /* Set up camera private library */
    camera->pl = malloc (sizeof (CameraPrivateLibrary));
    if (!camera->pl) return (GP_ERROR_NO_MEMORY);
    memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
    camera->pl->gpdev = camera->port;

   
       /* Set up the sub-architectures */
       /* Default to Agfa.  Should we default otherwise? */
    camera->pl->device_type=SOUNDVISION_AGFACL18;
    gp_camera_get_abilities (camera, &a);
   
    if ((a.usb_vendor==0x0919) && (a.usb_product==0x0100)) {
       camera->pl->device_type=SOUNDVISION_TIGERFASTFLICKS;	
    }
   
    if ((a.usb_vendor==0x0784) && (a.usb_product==0x0100)) {
       camera->pl->device_type=SOUNDVISION_IXLA;
    }
   
	
	
   

       /* Keep track.  We _must_ reset an even number of times */
    camera->pl->reset_times=0;
    camera->pl->odd_command=0;
   
       /* Reset the camera */
    ret = soundvision_reset (camera->pl,NULL,NULL);
    if (ret < 0) {
	free (camera->pl);
	camera->pl = NULL;
	return (ret);
    }
    /* Tell the CameraFilesystem where to get lists from */
    return gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
}

