/*
 * agfa.c
 *
 * Copyright (c) 2001 Vince Weaver <vince@deater.net>
 * 
 * based on the digita driver 
 * Copyright (c) 1999-2000 Johannes Erdfelt
 * Copyright (c) 1999-2000 VA Linux Systems
 */

#include <config.h>

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
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "agfa.h"

#include <gphoto2-port.h>

struct {
   char *name;
   unsigned short idVendor;
   unsigned short idProduct;
   char serial;
} models[] = {
        {"Agfa CL18",0x06bd,0x0403,0},
        {NULL,0,0,0}
};

int camera_id(CameraText *id) {
        
    strcpy(id->text, "agfa");
        
    return GP_OK;
}


int camera_abilities(CameraAbilitiesList *list) {
        
    int i;
    CameraAbilities a;

    for(i=0; models[i].name; i++) {
       memset(&a, 0, sizeof(a));
       strcpy(a.model, models[i].name);
       a.status = GP_DRIVER_STATUS_PRODUCTION;
       a.port       = GP_PORT_USB;
       a.speed[0] = 0;
       a.usb_vendor = models[i].idVendor;
       a.usb_product= models[i].idProduct;
       a.operations        = 	GP_OPERATION_CAPTURE_IMAGE;
       a.folder_operations = 	GP_FOLDER_OPERATION_NONE;
       a.file_operations   = 	GP_FILE_OPERATION_PREVIEW | 
				GP_FILE_OPERATION_DELETE;

       gp_abilities_list_append(list, a);
    }
    return GP_OK;
}

static int camera_exit (Camera *camera) {
   
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
			   CameraList *list, void *data) {
 
    Camera *camera=data;
    int i;
    char temp_file[14];

    gp_debug_printf (GP_DEBUG_HIGH, "agfa", "camera_file_list %s\n", 
		     folder);
   
    if (agfa_get_file_list(camera->pl) < 0) {
       gp_debug_printf (GP_DEBUG_HIGH, "agfa", "Could not agfa_file_list!");
       return GP_ERROR;
    }
       
    for(i=0; i < camera->pl->num_pictures; i++) {
       strncpy(temp_file,camera->pl->file_list+(13*i),12);
       temp_file[12]=0;
       gp_list_append (list, temp_file, NULL);
    }
    return GP_OK;
}


static int agfa_file_get (Camera *camera, const char *filename, int thumbnail, 
			    unsigned char **data, int *size) {

    int buflen,throwaway,result;

    gp_debug_printf(GP_DEBUG_LOW, "agfa", "Getting file '%s'...",filename);

    agfa_reset(camera->pl);
        /* Always have to check num photos,
	 * then pic size no matter what.  Otherwise
	 * the camera will stop responding 
	 */
   
    throwaway=agfa_photos_taken(camera->pl);
    if (throwaway<0) return throwaway;
   
       /* The below two lines might look wrong, but they aren't! */
    buflen = agfa_get_pic_size(camera->pl,filename);
    if (thumbnail) buflen = agfa_get_thumb_size(camera->pl,filename);
   
    *data = malloc(buflen+1);
        
    if (!*data) return (GP_ERROR_NO_MEMORY);
   
    memset(*data, 0, buflen);
 
    if (thumbnail) {
       result=agfa_get_thumb(camera->pl, filename, *data, buflen);
       if (result < 0) {
	  free (*data);
	  gp_debug_printf(GP_DEBUG_LOW,"agfa","agfa_get_thumb_failed!");
	  return result;
       }
    }
    else {
       result=agfa_get_pic(camera->pl, filename, *data, buflen);
       if (result < 0) {
	  free(*data);
	  gp_debug_printf(GP_DEBUG_LOW,"agfa","agfa_get_pic_failed!");
          return result;
       }
    }
    if (size)
       *size = buflen;

    return GP_OK;

}

static int get_file_func (CameraFilesystem *fs, const char *folder, 
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data) {

    Camera *camera = user_data;
    unsigned char *data = NULL;
    int size,ret;

    switch (type) {
       case GP_FILE_TYPE_NORMAL:
               ret=agfa_file_get(camera, filename, 0, &data, &size);
               if (ret<0) return ret;
               break;
       case GP_FILE_TYPE_PREVIEW:
               ret=agfa_file_get(camera, filename, 1, &data, &size);
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

static int camera_summary(Camera *camera, CameraText *summary) {

    int taken;

    taken=agfa_photos_taken(camera->pl);
    if (taken< 0) return taken;

    sprintf(summary->text, _("Number of pictures: %d"), taken);

    return GP_OK;
}

static int camera_manual(Camera *camera, CameraText *manual) {

    strcpy(manual->text, _("Manual Not Implemented Yet"));

    return GP_OK;

}

static int camera_about(Camera *camera, CameraText *about) {
        
    strcpy(about->text, _("Agfa CL18\nVince Weaver <vince@deater.net>\n"));

    return GP_OK;
}


    /* Below contributed by Ben Hague <benhague@btinternet.com> */
static int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path)
{
    /* this is broken.  We capture image to the camera, but
     * we don't detect the new filename.  We should detect
     * it and gp_filesystem_append and return 
     */
       
    return (agfa_capture(camera->pl,path));
}


static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data) {
  
    Camera *camera = data;

    gp_debug_printf(GP_DEBUG_LOW,"agfa","Deleting '%s' in '%s'...",filename,folder); 
   
    agfa_delete_picture(camera->pl,filename);
   
       /* Update our file list */
    if (agfa_get_file_list(camera->pl)<0) return GP_ERROR;
   
    return GP_OK;
}


int camera_init(Camera *camera) {
   
    GPPortSettings settings;
    int ret = 0;

       /* First, set up all the function pointers */
    camera->functions->exit         = camera_exit;
    camera->functions->summary      = camera_summary;
    camera->functions->manual       = camera_manual;
    camera->functions->about        = camera_about;
    camera->functions->capture      = camera_capture;
   
    gp_debug_printf (GP_DEBUG_LOW, "agfa", "Initializing the camera\n");

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
  
    ret = agfa_reset (camera->pl);
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
