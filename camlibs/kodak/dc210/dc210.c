/*
  DC210 driver
  Copyright (c) 2001 Hubert Figuiere
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
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
    int ret;
    DC210Data * dd = (DC210Data *)camera->camlib_data;

    ret = kodak_dc210_close_camera (dd);

    if (dd->dev) {
	gp_port_close(dd->dev);
        if (ret == GP_ERROR) { 
	    gp_debug_printf (GP_DEBUG_LOW, "dc210", 
			     "%s,%d: gp_port_close = %d", 
			     __FILE__, __LINE__, ret);
	    
	}
        gp_port_free(dd->dev);
    }
    free(dd);

    return ret;
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list) 
{
    /* there is only one folder */
    /* don't list */
    gp_debug_printf (GP_DEBUG_LOW, "dc210", 
		     "%s,%d: camera_folder_list_folders: unimplemented", 
		     __FILE__, __LINE__);
    
    return (GP_ERROR);
}

int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list)
{
    int ret;
    DC210Data * dd = (DC210Data *)camera->camlib_data;

    ret = kodak_dc210_get_directory_listing (dd, list);
/*    gp_debug_printf (GP_DEBUG_LOW, "dc210", 
			     "kodak_dc210_get_picture_info failed %d", ret);*/
    return ret;
}


/*
  Possible ops are:
  -getPic 0;
  -getThumbnail 1;
  -delete 2;
 */
static int _camera_file_op (Camera *camera, const char *folder, 
                            const char *filename, CameraFile *file, int op)
{
    int ret = GP_OK;
    int count = 0;
    int i;
    CameraList * list;
    const char * current;
    DC210Data * dd = (DC210Data *)camera->camlib_data;

    if (!dd) {
	gp_debug_printf (GP_DEBUG_LOW, "dc210", "%s,%d: unable to get dd", 
			 __FILE__, __LINE__);
   	return GP_ERROR;
    }

    gp_list_new (&list);
    ret = kodak_dc210_get_directory_listing (dd, list);
    count = gp_list_count (list);

    for (i = 0; i < count; i++) {
        gp_list_get_name (list, i, &current);
        if (current == NULL) {
            /* ERROR */
            ret = GP_ERROR;
            break;
        }

        if (strcmp (current, filename) == 0) {
            /* picture found */
            switch (op) {
            case 0:
                ret = kodak_dc210_get_picture (dd, i, file);
                break;
            case 1:
                ret = kodak_dc210_get_thumbnail (dd, i, file);                
                break;
            case 2:
                ret = kodak_dc210_delete_picture (dd, i);
                break;
            default:
		gp_debug_printf (GP_DEBUG_LOW, "dc210", 
				 "%s,%d: unknown file op = %d", 
				 __FILE__, __LINE__, op);
                /* DOH ! */
                break;
            }
	    /* we found our file */
            break;
        }
    }
    gp_list_free (list);
    gp_debug_printf (GP_DEBUG_LOW, "dc210", "%s,%d: file op completed = %d", 
		     __FILE__, __LINE__, ret);
    return ret;
}


/* Get the file */
int camera_file_get (Camera *camera, const char *folder, const char *filename, 
		     CameraFileType type, CameraFile *file)
{ 
    switch (type) 
    {
    case GP_FILE_TYPE_NORMAL:
	return _camera_file_op (camera, folder, filename, file, 0);
	break;
    case GP_FILE_TYPE_PREVIEW:
	return _camera_file_op (camera, folder, filename, file, 1);
	break;
    default:
	return GP_ERROR_NOT_SUPPORTED;
    }
}


int camera_folder_put_file (Camera *camera, const char *folder, 
			    CameraFile *file)
{
    /* not supported */
    return GP_ERROR;
}

int camera_file_delete (Camera *camera, const char *folder, 
			const char *filename) 
{
    return _camera_file_op (camera, folder, filename, NULL, 2);
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
    int ret = GP_OK;
    int numPicBefore;
    int numPicAfter;
    struct kodak_dc210_picture_info picInfo;    
    DC210Data * dd = (DC210Data *)camera->camlib_data;
    
    /* find out how many pictures are in the camera so we can
       make sure a picture was taken later */
    numPicBefore = kodak_dc210_number_of_pictures(dd);
    
    /* take a picture -- it returns the picture number taken */
    numPicAfter = kodak_dc210_capture (dd);
    
    /* if a picture was taken then get the picture from the camera and
       then delete it */
    if (numPicBefore + 1 == numPicAfter)
    {
	strcpy (path->folder, "/");
	ret = kodak_dc210_get_picture_info (dd, numPicAfter, &picInfo);
	strncpy (path->name, picInfo.fileName, 12);
	path->name[12] = 0;
    }
    
    return ret;
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
