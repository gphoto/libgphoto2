/*
 * soundvision.c
 *
 * Copyright © 2002 Vince Weaver <vince@deater.net>
 * 
 * based on the digita driver 
 * Copyright © 1999-2000 Johannes Erdfelt
 * Copyright © 1999-2000 VA Linux Systems
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
#include <netinet/in.h>

#include <gphoto2.h>

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

#include <gphoto2-port.h>

#define GP_MODULE "soundvision"

struct {
   char *name;
   unsigned short idVendor;
   unsigned short idProduct;
   char serial;
} models[] = {
        {"Agfa:ePhoto CL18",0x06bd,0x0403,0},
        {"Mustek:gSmart 350",0x055f,0xa350,0},
        {"RCA:CDS1005",0x0784,0x0100,0},
   
        {"Generic SoundVision:Clarity2",0x0919,0x100,0},
        {"CoolCam:CP086",0x0919,0x100,0},
        {"FujiFilm:@xia ix-100",0x0919,0x0100,0},
        {"Media-Tech:mt-406",0x0919,0x0100,0},
        {"Oregon Scientific:DShot II",0x0919,0x0100,0},
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
       if (i==1)
          a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
       else
          a.status = GP_DRIVER_STATUS_PRODUCTION;
       a.port       = GP_PORT_USB;
       a.speed[0] = 0;
       a.usb_vendor = models[i].idVendor;
       a.usb_product= models[i].idProduct;
       if (i==1)
	  a.operations     =    GP_OPERATION_NONE;
       else
          a.operations     = 	GP_OPERATION_CAPTURE_IMAGE;
       a.folder_operations = 	GP_FOLDER_OPERATION_NONE;
       /* f_ops will (hopefully) be PUT_FILE for i==1 eventually */
       a.file_operations   = 	GP_FILE_OPERATION_PREVIEW | 
				GP_FILE_OPERATION_DELETE;

       gp_abilities_list_append(list, a);
    }
    return GP_OK;
}

static int camera_exit (Camera *camera, GPContext *context) {
   
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

    GP_DEBUG( "Getting file '%s'...",filename);

    soundvision_reset(camera->pl);
        /* Always have to check num photos,
	 * then pic size no matter what.  Otherwise
	 * the camera will stop responding 
	 */
   
    throwaway=soundvision_photos_taken(camera->pl);
    if (throwaway<0) return throwaway;
   
       /* The below two lines might look wrong, but they aren't! */
    buflen = soundvision_get_pic_size(camera->pl,filename);
    if (thumbnail) buflen = soundvision_get_thumb_size(camera->pl,filename);
   
       /* Don't try to download if size equals zero! */
    if (buflen) {
   
       *data = malloc(buflen+1);
        
       if (!*data) return (GP_ERROR_NO_MEMORY);
   
       memset(*data, 0, buflen);
 
       if (thumbnail) {
          result=soundvision_get_thumb(camera->pl, filename, *data, buflen);
          if (result < 0) {
	     free (*data);
	     GP_DEBUG("soundvision_get_thumb_failed!");
	     return result;
          }
       }
       else {
          result=soundvision_get_pic(camera->pl, filename, *data, buflen);
          if (result < 0) {
	     free(*data);
	     GP_DEBUG("soundvision_get_pic_failed!");
             return result;
          }
       }
    }
    if (size)
       *size = buflen;

    return GP_OK;

}

static int get_file_func (CameraFilesystem *fs, const char *folder, 
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data,
			  GPContext *context) {

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

    gp_file_set_data_and_size (file, data, size);
    gp_file_set_name (file, filename);
    gp_file_set_mime_type (file, GP_MIME_JPEG);

    return GP_OK;
}

static int camera_summary(Camera *camera, CameraText *summary,
			  GPContext *context) {

    char revision[9];
   
    soundvision_get_revision(camera->pl,revision);
   
    sprintf(summary->text, _("Revision: %8s"), revision);

    return GP_OK;
}

static int camera_about(Camera *camera, CameraText *about, GPContext *context) {
        
    strcpy(about->text, _("Soundvision Driver\nVince Weaver <vince@deater.net>\n"));

    return GP_OK;
}


    /* Below contributed by Ben Hague <benhague@btinternet.com> */
static int camera_capture (Camera *camera, CameraCaptureType type,
			   CameraFilePath *path, GPContext *context)
{
    /* this is broken.  We capture image to the camera, but
     * we don't detect the new filename.  We should detect
     * it and gp_filesystem_append and return 
     */
    if (camera->pl->device_type==SOUNDVISION_AGFACL18)     
       return (agfa_capture(camera->pl,path));
    return GP_ERROR_NOT_SUPPORTED;
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

				       
   
    camera->pl = malloc (sizeof (CameraPrivateLibrary));
    if (!camera->pl) return (GP_ERROR_NO_MEMORY);
    memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
    camera->pl->gpdev = camera->port;

    camera->pl->device_type=SOUNDVISION_AGFACL18;

    gp_camera_get_abilities (camera, &a);
    if ((a.usb_vendor==0x919) && (a.usb_product==0x0100)) {
       camera->pl->device_type=SOUNDVISION_TIGERFASTFLICKS;	
    }
     
    ret = soundvision_reset (camera->pl);
    if (ret < 0) {
	free (camera->pl);
	camera->pl = NULL;
	return (ret);
    }
    
       /* Tell the CameraFilesystem where to get lists from */
    gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
    gp_filesystem_set_file_funcs (camera->fs, get_file_func, delete_file_func,
		    		  camera);

    return GP_OK;
}
