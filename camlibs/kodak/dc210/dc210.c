/*
  DC210 driver
  Copyright (c) 2001 Hubert Figuiere
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <time.h>

#include <gphoto2.h>
#include <gphoto2-port.h>

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

#include "dc210.h"
#include "library.h"

int camera_id (CameraText *id) 
{
	strcpy(id->text, "REPLACE WITH UNIQUE LIBRARY ID");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) 
{
    CameraAbilities a;
    
    memset(&a, 0, sizeof(a));
    strcpy(a.model, "Kodak DC210");
    a.port     = GP_PORT_SERIAL;
    a.speed[0] = 9600;
    a.speed[1] = 19200;
    a.speed[2] = 38400;
    a.speed[3] = 57600;
    a.speed[4] = 115200;
    a.speed[5] = 0;
    a.operations        = 	GP_OPERATION_CAPTURE_IMAGE;
    a.file_operations   = 	GP_FILE_OPERATION_DELETE | 
                                GP_FILE_OPERATION_PREVIEW;
    a.folder_operations = 	GP_FOLDER_OPERATION_NONE;
    
    gp_abilities_list_append(list, a);
    
    return (GP_OK);
}

int camera_init (Camera *camera) 
{
    gp_port_settings settings;

    DC210Data *dd;
    int ret;
    
    if (!camera) {
        return (GP_ERROR);
    }
    
    dd = (DC210Data *)malloc (sizeof (DC210Data));
    if (dd == NULL) {
        return GP_ERROR;
    }

    /* First, set up all the function pointers */
    camera->functions->id 			= camera_id;
    camera->functions->abilities 		= camera_abilities;
    camera->functions->init 		= camera_init;
    camera->functions->exit 		= camera_exit;
    camera->functions->folder_list_folders 	= camera_folder_list_folders;
    camera->functions->folder_list_files	= camera_folder_list_files;
    camera->functions->file_get 		= camera_file_get;
    camera->functions->file_delete 		= camera_file_delete;
#if 0
    camera->functions->config_get   	= camera_config_get;
    camera->functions->config_set   	= camera_config_set;
    camera->functions->folder_config_get 	= camera_folder_config_get;
    camera->functions->folder_config_set 	= camera_folder_config_set;
    camera->functions->file_config_get 	= camera_file_config_get;
    camera->functions->file_config_set 	= camera_file_config_set;
#endif
    camera->functions->capture 		= camera_capture;
    camera->functions->summary		= camera_summary;
    camera->functions->manual 		= camera_manual;
    camera->functions->about 		= camera_about;
    camera->functions->result_as_string 	= camera_result_as_string;

    switch (camera->port->type) {
    case GP_PORT_SERIAL:
        if ((ret = gp_port_new(&(dd->dev), GP_PORT_SERIAL)) < 0) {
            free(dd);
            return (GP_ERROR);
        }
        strcpy(settings.serial.port, camera->port->path);
        settings.serial.speed    = 9600;
        settings.serial.bits     = 8;
        settings.serial.parity   = 0;
        settings.serial.stopbits = 1;
        break;
    default:
        return GP_ERROR;
        break;
    }

    if (gp_port_settings_set(dd->dev, settings) == GP_ERROR) {
        gp_port_free(dd->dev);
        free(dd);
        return (GP_ERROR);
    }

    if (gp_port_open(dd->dev) == GP_ERROR) {
        gp_port_free(dd->dev);
        free(dd);
        return (GP_ERROR);
    }

    /* timeout is set to 1500 */
    gp_port_timeout_set (dd->dev, 1500);

    /* Reset the camera to 9600 by sending a break. */
    gp_port_send_break(dd->dev, 1);
    
    /* Wait for it to reset */
    GP_SYSTEM_SLEEP(1500);

    if (kodak_dc210_open_camera(dd) == GP_ERROR) {
        gp_port_close(dd->dev);
        gp_port_free(dd->dev);
        free(dd);
        return (GP_ERROR);
    }

    camera->camlib_data = dd;

    

    return (GP_OK);
}

int camera_exit (Camera *camera) 
{
    DC210Data * dd = (DC210Data *)camera->camlib_data;

    kodak_dc210_close_camera (dd);

    if (dd->dev) {
        if (gp_port_close(dd->dev) == GP_ERROR)
                { /* camera did a bad, bad thing */ }
        gp_port_free(dd->dev);
    }
    free(dd);

    return (GP_OK);
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list) 
{
	return (GP_OK);
}

int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list)
{
	return (GP_OK);
}

int camera_file_get (Camera *camera, const char *folder, const char *filename,
		     CameraFileType type, CameraFile *file) 
{
    int numPicBefore;
    int numPicAfter;
    struct Image *im = NULL;

    DC210Data * dd = (DC210Data *)camera->camlib_data;

    if (type != GP_FILE_TYPE_PREVIEW)
	    return (GP_ERROR_NOT_SUPPORTED);

    /*
     * 2001/08/25: Lutz Müller <urc8@rz.uni-karlsruhe.de>
     * What's happening here? We are supposed to get the file or the preview,
     * not capturing an image?!? That's totally screwed up... Please fix.
     */

    /* find out how many pictures are in the camera so we can
       make sure a picture was taken later */
    numPicBefore = kodak_dc210_number_of_pictures(dd);
    
    /* take a picture -- it returns the picture number taken */
    numPicAfter = kodak_dc210_capture (dd);
    
    /* if a picture was taken then get the picture from the camera and
       then delete it */
    if (numPicBefore + 1 == numPicAfter)
    {
        im = kodak_dc210_get_picture (dd, numPicAfter, file);
        kodak_dc210_delete_picture (dd, numPicAfter);
    }
    
    return (GP_OK);
}

int camera_folder_put_file (Camera *camera, const char *folder, 
			    CameraFile *file)
{
	return (GP_OK);
}

int camera_file_delete (Camera *camera, const char *folder, 
			const char *filename) 
{
    /* TODO */
    int picNum = 0;
    DC210Data * dd = (DC210Data *)camera->camlib_data;
    
    kodak_dc210_delete_picture (dd, picNum);
    return (GP_OK);
}

#if 0
int camera_config_get (Camera *camera, CameraWidget **window) 
{

	*window = gp_widget_new (GP_WIDGET_WINDOW, "Camera Configuration");

	// Append your sections and widgets here.

	return (GP_OK);
}

int camera_config_set (Camera *camera, CameraWidget *window) 
{
	// Check, if the widget's value have changed.

	return (GP_OK);
}
#endif

int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) 
{
	// Capture image/preview/video and return it in 'file'. Don't store it
	// anywhere on your camera! If your camera does store the image, 
	// delete it manually here.

	return (GP_OK);
}

int camera_summary (Camera *camera, CameraText *summary) 
{
    static char summary_string[2048] = "";
    char buff[1024];
    struct kodak_dc210_status status;

    DC210Data * dd = (DC210Data *)camera->camlib_data;
    
    if (kodak_dc210_get_camera_status (dd, &status))
    {
        strcpy(summary_string,"Kodak DC210\n");
        
        snprintf(buff,1024,"Camera Identification: %s\n",status.camera_ident);
        strcat(summary_string,buff);
        
        snprintf(buff,1024,"Camera Type: %d\n",status.camera_type_id);
        strcat(summary_string,buff);
        
        snprintf(buff,1024,"Firmware: %d.%d\n",status.firmware_major,status.firmware_minor);
        strcat(summary_string,buff);
        
        snprintf(buff,1024,"Battery Status: %d\n",status.batteryStatusId);
        strcat(summary_string,buff);
        
        snprintf(buff,1024,"AC Status: %d\n",status.acStatusId);
        strcat(summary_string,buff);
        
        strftime(buff,1024,"Time: %a, %d %b %y %T\n",localtime((time_t *)&status.time));
        strcat(summary_string,buff);
        
        fprintf(stderr,"step 4\n");
        snprintf(buff,1024,"Total Pictures Taken: %d\n",status.totalPicturesTaken);
        strcat(summary_string,buff);
        
        snprintf(buff,1024,"Total Flashes Fired: %d\n",status.totalFlashesFired);
        strcat(summary_string,buff);
        
        snprintf(buff,1024,"Pictures in Camera: %d\n",status.num_pictures);
        strcat(summary_string,buff);
        
    }
    
    strcpy(summary->text, buff);
    
    return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) 
{
    strcpy(manual->text, _("No Manual Available"));
    
    return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) 
{
	strcpy(about->text, 
               "Kodak DC210 Camera Library\n
by Brian Hirt <bhirt@mobygames.com> http://www.mobygames.com\n
gphoto2 port by Hubert Figuiere <hfiguiere@teaser.fr>");
        
	return (GP_OK);
}

char* camera_result_as_string (Camera *camera, int result) 
{
	if (result >= 0) return ("This is not an error...");
	if (-result < 100) return gp_result_as_string (result);
	return ("This is a template specific error.");
}
