#include <stdlib.h>
#include <string.h>
#include <gpio.h>
#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#  else
#      define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "globals.h"
#include "library.h"
#include "settings.h"
#include "util.h"

int gp_camera_exit (Camera *camera);

int gp_camera_count ()
{
	return(glob_abilities_list->count);
} 

int gp_camera_name (int camera_number, char *camera_name)
{
        if (camera_number > glob_abilities_list->count)
                return (GP_ERROR_MODEL_NOT_FOUND);

        strcpy(camera_name, glob_abilities_list->abilities[camera_number]->model);
        return (GP_OK);
}

int gp_camera_abilities (int camera_number, CameraAbilities *abilities)
{
        memcpy(abilities, glob_abilities_list->abilities[camera_number],
               sizeof(CameraAbilities));

        if (glob_debug)
                gp_abilities_dump(abilities);

        return (GP_OK);
}


int gp_camera_abilities_by_name (char *camera_name, CameraAbilities *abilities)
{

        int x=0;
        while (x < glob_abilities_list->count) {
                if (strcmp(glob_abilities_list->abilities[x]->model, camera_name)==0)
                        return (gp_camera_abilities(x, abilities));
                x++;
        }

        return (GP_ERROR_MODEL_NOT_FOUND);

}

int gp_camera_new (Camera **camera, int camera_number)
{
	int result;

	if (camera == NULL) return (GP_ERROR_BAD_PARAMETERS);

        if (camera_number >= glob_abilities_list->count) {
		gp_debug_printf(GP_DEBUG_LOW, "core", "camera_number too big");
                return (GP_ERROR_MODEL_NOT_FOUND);
	}

        *camera = (Camera*)malloc(sizeof(Camera));
	if (!*camera) 
		return (GP_ERROR_NO_MEMORY);

        /* Initialize the members */
        strcpy((*camera)->model, glob_abilities_list->abilities[camera_number]->model);
        (*camera)->port       = (CameraPortInfo*)malloc(sizeof(CameraPortInfo));
	if (!(*camera)->port) 
		return (GP_ERROR_NO_MEMORY);
	(*camera)->port->type = GP_PORT_NONE;
        (*camera)->abilities  = (CameraAbilities*)malloc(sizeof(CameraAbilities));
        (*camera)->functions  = (CameraFunctions*)malloc(sizeof(CameraFunctions));
        (*camera)->library_handle  = NULL;
        (*camera)->camlib_data     = NULL;
        (*camera)->frontend_data   = NULL;
	(*camera)->session    = glob_session_camera++;
        (*camera)->ref_count  = 1;
	if (!(*camera)->abilities || !(*camera)->functions) 
		return (GP_ERROR_NO_MEMORY);

	/* Initialize function pointers to NULL.*/
	(*camera)->functions->id		= NULL;
	(*camera)->functions->abilities		= NULL;
	(*camera)->functions->init		= NULL;
	(*camera)->functions->exit		= NULL;
	(*camera)->functions->folder_list	= NULL;
	(*camera)->functions->file_list		= NULL;
	(*camera)->functions->file_get_preview	= NULL;
	(*camera)->functions->file_config_get	= NULL;
	(*camera)->functions->file_config_set	= NULL;
	(*camera)->functions->folder_config_get	= NULL;
	(*camera)->functions->folder_config_set	= NULL;
	(*camera)->functions->config_get	= NULL;
	(*camera)->functions->config_set	= NULL;
	(*camera)->functions->file_put		= NULL;
	(*camera)->functions->file_delete	= NULL;
	(*camera)->functions->capture		= NULL;
	(*camera)->functions->summary		= NULL;
	(*camera)->functions->manual		= NULL;
	(*camera)->functions->about		= NULL;
	(*camera)->functions->result_as_string	= NULL;
	(*camera)->functions->config		= NULL;

	/* Fill the abilities. */
	(*camera)->abilities->file_delete	= glob_abilities_list->abilities[camera_number]->file_delete;
	(*camera)->abilities->file_preview	= glob_abilities_list->abilities[camera_number]->file_preview;
	(*camera)->abilities->file_put		= glob_abilities_list->abilities[camera_number]->file_put;
	(*camera)->abilities->capture		= glob_abilities_list->abilities[camera_number]->capture;
	(*camera)->abilities->config		= glob_abilities_list->abilities[camera_number]->config;

        if ((result = load_library(*camera, glob_abilities_list->abilities[camera_number]->model)) != GP_OK) {
                gp_camera_free(*camera);
                return (result);
        }

        /* Initialize the camera library */
	gp_debug_printf(GP_DEBUG_LOW, "core", "Initializing \"%s\" (%s)...",
                        glob_abilities_list->abilities[camera_number]->model,
                        glob_abilities_list->abilities[camera_number]->library);

        return(GP_OK);
}

int gp_camera_new_by_name (Camera **camera, char *camera_name)
{
        int x=0;

        while (x < glob_abilities_list->count) {
                if (strcmp(glob_abilities_list->abilities[x]->model, camera_name)==0)
                        return (gp_camera_new(camera, x));
                x++;
        }

        return (GP_ERROR_MODEL_NOT_FOUND);
}

int gp_camera_free(Camera *camera)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
        gp_camera_exit(camera);

        if (camera->port)
                free(camera->port);
        if (camera->abilities)
                free(camera->abilities);
        if (camera->functions)
                free(camera->functions);

        return (GP_OK);
}

int gp_camera_ref (Camera *camera)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);

	camera->ref_count += 1;

	return (GP_OK);
}

int gp_camera_unref (Camera *camera)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
		
	camera->ref_count -= 1;

	if (camera->ref_count == 0)
		gp_camera_free(camera);

	return (GP_OK);
}


int gp_camera_session (Camera *camera)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);

	return (camera->session);

}

int gp_camera_init (Camera *camera)
{
	int x;
	CameraPortInfo info;

	if (camera == NULL) 
		return (GP_ERROR_BAD_PARAMETERS);
		
        if (camera->functions->init == NULL) {
                return(GP_ERROR_NOT_SUPPORTED);
	}

	/* Set the port type from the path in case the frontend didn't. */
	if (camera->port->type == GP_PORT_NONE) {
	        for (x=0; x<gp_port_count(); x++) {
        	        gp_port_info(x, &info);
                	if (strcmp(info.path, camera->port->path)==0)
                        	camera->port->type = info.type;
	        }
	}


        return (camera->functions->init (camera));
}

int gp_camera_exit (Camera *camera)
{
        int ret;

	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);

        if (camera->functions->exit == NULL)
                return(GP_ERROR_NOT_SUPPORTED);

        ret = camera->functions->exit(camera);
        close_library(camera);

        return (ret);
}

int gp_camera_folder_list(Camera *camera, CameraList *list, char *folder)
{
        CameraListEntry t;
        int x, ret, y, z;

	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
	
	/* Initialize the folder list to a known state */
        list->count = 0;

        if (camera->functions->folder_list == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        ret = camera->functions->folder_list(camera, list, folder);

        if (ret != GP_OK)
                return (ret);

        /* Sort the folder list */
        for (x=0; x<list->count-1; x++) {
                for (y=x+1; y<list->count; y++) {
                        z = strcmp(list->entry[x].name, list->entry[y].name);
                        if (z > 0) {
                                memcpy(&t, &list->entry[x], sizeof(t));
                                memcpy(&list->entry[x], &list->entry[y], sizeof(t));
                                memcpy(&list->entry[y], &t, sizeof(t));
                        }
                }
        }

        return (GP_OK);
}

int gp_camera_file_list(Camera *camera, CameraList *list, char *folder)
{
        CameraListEntry t;
        int x, ret, y, z;

	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);

        /* Initialize the folder list to a known state */
        list->count = 0;

        if (camera->functions->file_list == NULL)
                return (GP_ERROR_NOT_SUPPORTED);
        ret = camera->functions->file_list(camera, list, folder);
        if (ret != GP_OK)
                return (ret);

        /* Sort the file list */
        for (x=0; x<list->count-1; x++) {
                for (y=x+1; y<list->count; y++) {
                        z = strcmp(list->entry[x].name, list->entry[y].name);
                        if (z > 0) {
                                memcpy(&t, &list->entry[x], sizeof(t));
                                memcpy(&list->entry[x], &list->entry[y], sizeof(t));
                                memcpy(&list->entry[y], &t, sizeof(t));
                        }
                }
        }

        return (GP_OK);
}

int gp_camera_file_get (Camera *camera, CameraFile *file,
                        char *folder, char *filename)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
        if (camera->functions->file_get == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        gp_file_clean(file);

        return (camera->functions->file_get(camera, file, folder, filename));
}

int gp_camera_file_get_preview (Camera *camera, CameraFile *file,
                        char *folder, char *filename)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
        if (camera->functions->file_get_preview == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        gp_file_clean(file);

        return (camera->functions->file_get_preview(camera, file, folder, filename));
}

int gp_camera_file_config_get (Camera *camera, CameraWidget **window,
			char *folder, char *filename)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
	if (camera->functions->file_config_get == NULL)
		return (GP_ERROR_NOT_SUPPORTED);
		
	return (camera->functions->file_config_get (camera, window, folder, filename));
}

int gp_camera_file_config_set (Camera *camera, CameraWidget *window, 
			char *folder, char *filename)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
	if (camera->functions->file_config_set == NULL)
		return (GP_ERROR_NOT_SUPPORTED);
	
	return (camera->functions->file_config_set (camera, window, folder, filename));
}

int gp_camera_folder_config_get (Camera *camera, CameraWidget **window,
			char *folder)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
	if (camera->functions->folder_config_get == NULL)
		return (GP_ERROR_NOT_SUPPORTED);

	return (camera->functions->folder_config_get (camera, window, folder));
}

int gp_camera_folder_config_set (Camera *camera, CameraWidget *window,
			char *folder)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
	if (camera->functions->folder_config_set == NULL)
		return (GP_ERROR_NOT_SUPPORTED);

	return (camera->functions->folder_config_set (camera, window, folder));
}

int gp_camera_config_get (Camera *camera, CameraWidget **window)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
	if (camera->functions->config_get == NULL)
		return (GP_ERROR_NOT_SUPPORTED);

	return (camera->functions->config_get (camera, window));
}

int gp_camera_config_set (Camera *camera, CameraWidget *window)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
	if (camera->functions->config_set == NULL)
		return (GP_ERROR_NOT_SUPPORTED);

	return (camera->functions->config_set (camera, window));
}

int gp_camera_file_put (Camera *camera, CameraFile *file, char *folder)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
        if (camera->functions->file_put == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->file_put(camera, file, folder));
}

int gp_camera_file_delete (Camera *camera, char *folder, char *filename)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
        if (camera->functions->file_delete == NULL)
                return (GP_ERROR_NOT_SUPPORTED);
		
        return (camera->functions->file_delete(camera, folder, filename));
}

int gp_camera_config (Camera *camera)
{

        if (camera->functions->config == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->config(camera));
}

int gp_camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
        if (camera->functions->capture == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        gp_file_clean(file);

        return (camera->functions->capture(camera, file, info));
}

int gp_camera_summary (Camera *camera, CameraText *summary)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
        if (camera->functions->summary == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->summary(camera, summary));
}

int gp_camera_manual (Camera *camera, CameraText *manual)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
        if (camera->functions->manual == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->manual(camera, manual));
}

int gp_camera_about (Camera *camera, CameraText *about)
{
	if (camera == NULL)
		return (GP_ERROR_BAD_PARAMETERS);
        if (camera->functions->about == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->about(camera, about));
}

char *gp_camera_result_as_string (Camera *camera, int result)
{
	/* Camlib error? */
	if (-result >= 100 && (camera != NULL)) {
		if (camera->functions->result_as_string == NULL)
			return _("Error description not available");
		return (camera->functions->result_as_string (camera, result));
	}

	return (gp_result_as_string (result));
}

