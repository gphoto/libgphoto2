/*
 * agfa.c
 *
 * Copyright (c) 2001 Vince Weaver <vince@deater.net>
 * 
 * based on the digita driver 
 * Copyright (c) 1999-2000 Johannes Erdfelt
 * Copyright (c) 1999-2000 VA Linux Systems
 */

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
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "agfa.h"

#include <gphoto2-port.h>

int (*agfa_send)(struct agfa_device *dev, void *buffer, int buflen) = NULL;
int (*agfa_read)(struct agfa_device *dev, void *buffer, int buflen) = NULL;

int camera_id(CameraText *id) {
        
    strcpy(id->text, "agfa");
        
    return GP_OK;
}

struct model models[] = {
        {"Agfa CL18",0x06bd,0x0403,0},
        {NULL,0,0,0}
};

int camera_abilities(CameraAbilitiesList *list) {
        
    int i;
    CameraAbilities *a;

    for(i=0; models[i].name; i++) {
       a=gp_abilities_new();
       strcpy(a->model, models[i].name);
       a->port       = GP_PORT_USB;
       a->usb_vendor = models[i].idVendor;
       a->usb_product= models[i].idProduct;
       a->operations        = 	GP_OPERATION_CAPTURE_IMAGE;
       a->folder_operations = 	GP_FOLDER_OPERATION_NONE;
       a->file_operations   = 	GP_FILE_OPERATION_PREVIEW | 
				GP_FILE_OPERATION_DELETE;

       gp_abilities_list_append(list, a);
    }
    return GP_OK;
}

int camera_init(Camera *camera) {
   
    struct agfa_device *dev;
    int ret = 0;

    if (!camera)
       return GP_ERROR;

       /* First, set up all the function pointers */
    camera->functions->id           = camera_id;
    camera->functions->abilities    = camera_abilities;
    camera->functions->init         = camera_init;
    camera->functions->exit         = camera_exit;
    camera->functions->folder_list_folders  = camera_folder_list_folders;
    camera->functions->folder_list_files    = camera_folder_list_files;
    camera->functions->file_get     = camera_file_get;
    camera->functions->file_get_preview =  camera_file_get_preview;
    camera->functions->summary      = camera_summary;
    camera->functions->manual       = camera_manual;
    camera->functions->about        = camera_about;
    camera->functions->file_delete  = camera_file_delete;
    camera->functions->capture      = camera_capture;
   
    gp_debug_printf (GP_DEBUG_LOW, "agfa", "Initializing the camera\n");

    dev = malloc(sizeof(*dev));
    if (!dev) {
       fprintf(stderr, "Couldn't allocate agfa device\n");
       return GP_ERROR;
    }
    memset((void *)dev, 0, sizeof(*dev));

    switch (camera->port->type) {
       
       case GP_PORT_USB: 
          ret = agfa_usb_open(dev, camera);
          break;
       
       default: 
          fprintf(stderr, "Unknown port type %d\n", camera->port->type);
          return GP_ERROR;
    }

    if (ret < 0) {
       fprintf(stderr, "Couldn't open agfa device\n");
       return GP_ERROR;
    }

    camera->camlib_data = dev;

    agfa_reset(dev); 
    return GP_OK;
}

int camera_exit (Camera *camera)
{
        return GP_OK;
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list)
{
        /* As far as I can tell the CL18 doesn't allow subfolders */
   
        return GP_OK;
}

int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list) {

    struct agfa_device *dev = camera->camlib_data;
    int i;
    char temp_file[14];

    gp_debug_printf (GP_DEBUG_HIGH, "agfa", "camera_file_list %s\n", 
		     folder);

    if (!dev) {
       printf("ERROR 1\n");
       return GP_ERROR;
    }
   
    if (agfa_get_file_list(dev) < 0) {
       printf("ERROR 2\n");
       return GP_ERROR;
    }
   
    if (folder[0] == '/')
                /* FIXME: (from digita driver) 
		   Why does the frontend return a path with multiple */
                /*  leading slashes? */
                while (folder[0] == '/')
                        folder++;

       
    for(i=0; i < dev->num_pictures; i++) {
       strncpy(temp_file,dev->file_list+(13*i),12);
       temp_file[12]=0;
       gp_list_append (list, temp_file, NULL);
    }
    return GP_OK;
}


static char *agfa_file_get (Camera *camera, const char *folder, 
			      const char *filename, int thumbnail, int *size) {

    struct agfa_device *dev = camera->camlib_data;
    unsigned char *data;
    int buflen,throwaway;

    printf("agfa_file_get\n");

    printf("agfa: getting %s%s\n", folder, filename);

   
        /* Always have to check num photos,
	 * then pic size no matter what.  Otherwise
	 * the camera will stop responding */
    
/*    if (thumbnail)  {*/
       agfa_photos_taken(dev,&throwaway);
/*    } */
   
    buflen = agfa_get_pic_size(dev,filename);
    
    if (thumbnail) buflen = agfa_get_thumb_size(dev,filename);
/*    else throwaway = agfa_get_thumb_size(dev,filename);  */
   
    data = malloc(buflen+1);
        
    if (!data) {
       fprintf(stderr, "allocating memory\n");
       return NULL;
    }
    memset(data, 0, buflen);
 
    gp_frontend_progress(NULL, NULL, 0.00);

    if (thumbnail) {
       if (agfa_get_thumb(dev, filename, data, buflen) < 0) {
          printf("agfa_get_picture: agfa_get_file_data failed\n");
          return NULL;
       }
    }
    else {
       if (agfa_get_pic(dev, filename, data, buflen) < 0) {
          printf("agfa_get_picture: agfa_get_file_data failed\n");
          return NULL;
       }
    }
    gp_frontend_progress(NULL, NULL, 1.00);

    if (size)
       *size = buflen;

    return data;

}

int camera_file_get (Camera *camera, const char *folder, const char *filename,
                     CameraFile *file) {

    struct agfa_device *dev = camera->camlib_data;
    unsigned char *data;
    int size;

    if (!dev) return GP_ERROR;

    if (folder[0] == '/') folder++;

    data = agfa_file_get(camera, folder, filename, 0, &size);
    
    if (!data) return GP_ERROR;

    gp_file_set_data_and_size (file, data, size);
    gp_file_set_name (file, filename);
    gp_file_set_type (file, "image/jpeg");

    return GP_OK;
}

   /* FIXME can easily share code with the preceding procedure */
int camera_file_get_preview (Camera *camera, const char *folder, 
			     const char *filename, CameraFile *file) {

    struct agfa_device *dev = camera->camlib_data;
    unsigned char *data;
    int size;
   
    if (!dev) return GP_ERROR;

    if (folder[0] == '/') folder++;

    data = agfa_file_get(camera, folder, filename, 1, &size);
        
    if (!data) return GP_ERROR;

    gp_file_set_data_and_size (file, data, size);
    gp_file_set_type (file, "image/jpeg");
    gp_file_set_name (file, filename);

    return GP_OK;
}

int camera_summary(Camera *camera, CameraText *summary) {

    struct agfa_device *dev = camera->camlib_data;
    int taken;

    if (!dev) return GP_ERROR;

    if (agfa_photos_taken(dev, &taken) < 0) return 0;

    sprintf(summary->text, _("Number of pictures: %d"), taken);

    return GP_OK;
}

int camera_manual(Camera *camera, CameraText *manual) {

    strcpy(manual->text, _("Manual Not Implemented Yet"));

    return GP_ERROR;

}

int camera_about(Camera *camera, CameraText *about) {
        
    strcpy(about->text, _("Agfa CL18\nVince Weaver <vince@deater.net>\n"));

    return GP_OK;
}


    /* Below contributed by Ben Hague <benhague@btinternet.com> */
int camera_capture (Camera *camera, int capture_type, CameraFilePath *path)
{
    struct agfa_device *dev=camera->camlib_data;

    /* Capture image/preview/video and return it in 'file'. Don't store it
       anywhere on your camera! If your camera does store the image,
       delete it manually here. */
       
    return (agfa_capture(dev,path));
}


int camera_file_delete (Camera *camera, const char *folder,
			const char *filename) {
  
   struct agfa_device *dev=camera->camlib_data;
   
printf("agfa_delete_picture\n");
#if 0
        if (index > dev->num_pictures)
                return 0;

        index--;
#endif
   
printf("deleting  %s%s\n", folder,filename);

        if (agfa_delete_picture(dev, filename) < 0)
                return 0;

        if (agfa_get_file_list(dev) < 0)
                return 0;

        return 0;

}

