#include <gpio.h>
#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "globals.h"
#include "library.h"
#include "settings.h"
#include "util.h"

int gp_camera_init (Camera *camera, CameraInit *init);
int gp_camera_exit (Camera *camera);

int gp_camera_count ()
{
	return(glob_abilities_list->count);
} 

int gp_camera_name (int camera_number, char *camera_name)
{
        if (camera_number > glob_abilities_list->count)
                return (GP_ERROR);

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

        return (GP_ERROR);

}

int gp_camera_new (Camera **camera, int camera_number, CameraPortInfo *settings)
{

        CameraInit ci;

        if (camera_number >= glob_abilities_list->count)
                return (GP_ERROR);

        *camera = (Camera*)malloc(sizeof(Camera));

        /* Initialize the members */
        strcpy((*camera)->model, glob_abilities_list->abilities[camera_number]->model);
        (*camera)->debug      = glob_debug;
        (*camera)->port       = (CameraPortInfo*)malloc(sizeof(CameraPortInfo));
        (*camera)->abilities  = (CameraAbilities*)malloc(sizeof(CameraAbilities));
        (*camera)->functions  = (CameraFunctions*)malloc(sizeof(CameraFunctions));
        (*camera)->library_handle  = NULL;
        (*camera)->camlib_data     = NULL;
        (*camera)->frontend_data   = NULL;
	(*camera)->session    = glob_session_camera++;

        memcpy((*camera)->port, settings, sizeof(CameraPortInfo));


        if (load_library(*camera, glob_abilities_list->abilities[camera_number]->model)==GP_ERROR) {
                gp_camera_free(*camera);
                return (GP_ERROR);
        }

        /* Initialize the camera library */
	gp_debug_printf(GP_DEBUG_LOW, "core", "Initializing \"%s\" (%s)...",
                        glob_abilities_list->abilities[camera_number]->model,
                        glob_abilities_list->abilities[camera_number]->library);
        strcpy(ci.model, glob_abilities_list->abilities[camera_number]->model);
        memcpy(&ci.port_settings, settings, sizeof(CameraPortInfo));

        if (gp_camera_init(*camera, &ci)==GP_ERROR) {
                gp_camera_free(*camera);
                *camera=NULL;
                return (GP_ERROR);
        }

        return(GP_OK);
}

int gp_camera_free(Camera *camera)
{
        gp_camera_exit(camera);

        if (camera->port)
                free(camera->port);
        if (camera->abilities)
                free(camera->abilities);
        if (camera->functions)
                free(camera->functions);

        return (GP_OK);
}

int gp_camera_session (Camera *camera)
{

	return (camera->session);

}

int gp_camera_new_by_name (Camera **camera, char *camera_name, CameraPortInfo *settings)
{
        int x=0;

        while (x < glob_abilities_list->count) {
                if (strcmp(glob_abilities_list->abilities[x]->model, camera_name)==0)
                        return (gp_camera_new(camera, x, settings));
                x++;
        }

        return (GP_ERROR);
}

int gp_camera_init (Camera *camera, CameraInit *init)
{
        int x;
        CameraPortInfo info;

        if (camera->functions->init == NULL)
                return(GP_ERROR);

        for (x=0; x<gp_port_count(); x++) {
                gp_port_info(x, &info);
                if (strcmp(info.path, init->port_settings.path)==0)
                        init->port_settings.type = info.type;
        }

        return(camera->functions->init(camera, init));
}

int gp_camera_exit (Camera *camera)
{
        int ret;

        if (camera->functions->exit == NULL)
                return(GP_ERROR);

        ret = camera->functions->exit(camera);
        close_library(camera);

        return (ret);
}

int gp_camera_folder_list(Camera *camera, CameraList *list, char *folder)
{
        CameraListEntry t;
        int x, ret, y, z;

        /* Initialize the folder list to a known state */
        list->count = 0;

        if (camera->functions->folder_list == NULL)
                return (GP_ERROR);

        ret = camera->functions->folder_list(camera, list, folder);

        if (ret == GP_ERROR)
                return (GP_ERROR);

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

        /* Initialize the folder list to a known state */
        list->count = 0;

        if (camera->functions->file_list == NULL)
                return (GP_ERROR);
        ret = camera->functions->file_list(camera, list, folder);
        if (ret == GP_ERROR)
                return (GP_ERROR);

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
        if (camera->functions->file_get == NULL)
                return (GP_ERROR);

        gp_file_clean(file);

        return (camera->functions->file_get(camera, file, folder, filename));
}

int gp_camera_file_get_preview (Camera *camera, CameraFile *file,
                        char *folder, char *filename)
{
        if (camera->functions->file_get_preview == NULL)
                return (GP_ERROR);

        gp_file_clean(file);

        return (camera->functions->file_get_preview(camera, file, folder, filename));
}

int gp_camera_file_put (Camera *camera, CameraFile *file, char *folder)
{
        if (camera->functions->file_put == NULL)
                return (GP_ERROR);

        return (camera->functions->file_put(camera, file, folder));
}

int gp_camera_file_delete (Camera *camera, char *folder, char *filename)
{

        if (camera->functions->file_delete == NULL)
                return (GP_ERROR);
        return (camera->functions->file_delete(camera, folder, filename));
}

int gp_camera_config_get (Camera *camera, CameraWidget *window)
{
        int ret;

        if (camera->functions->config_get == NULL)
                return (GP_ERROR);

        ret = camera->functions->config_get(camera, window);

        if (glob_debug)
                gp_widget_dump(window);

        return (ret);
}

int gp_camera_config_set (Camera *camera, CameraSetting *setting, int count)
{
        int x;

        if (glob_debug) {
                gp_debug_printf(GP_DEBUG_LOW, "core", "Dumping CameraSetting");
                for (x=0; x<count; x++)
                        gp_debug_printf(GP_DEBUG_LOW, "core", "\"%s\" = \"%s\"", setting[x].name, setting[x].value);
        }

        if (camera->functions->config_set == NULL)
                return (GP_ERROR);

        return(camera->functions->config_set(camera, setting, count));
}

int gp_camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info)
{
        if (camera->functions->capture == NULL)
                return (GP_ERROR);

        gp_file_clean(file);

        return(camera->functions->capture(camera, file, info));
}

int gp_camera_summary (Camera *camera, CameraText *summary)
{
        if (camera->functions->summary == NULL)
                return (GP_ERROR);

        return(camera->functions->summary(camera, summary));
}

int gp_camera_manual (Camera *camera, CameraText *manual)
{
        if (camera->functions->manual == NULL)
                return (GP_ERROR);

        return(camera->functions->manual(camera, manual));
}

int gp_camera_about (Camera *camera, CameraText *about)
{
        if (camera->functions->about == NULL)
                return (GP_ERROR);

        return(camera->functions->about(camera, about));
}

int gp_camera_status (Camera *camera, char *status)
{
	if (gp_fe_status)
		gp_fe_status(camera, status);
        return(GP_OK);
}

int gp_camera_progress (Camera *camera, CameraFile *file, float percentage)
{
	if (gp_fe_progress)
		gp_fe_progress(camera, file, percentage);

        return(GP_OK);
}

int gp_camera_message (Camera *camera, char *message)
{
	if (gp_fe_message)
		gp_fe_message(camera, message);
        return(GP_OK);
}

int gp_camera_confirm (Camera *camera, char *message)
{
	if (!gp_fe_confirm)
		/* return YES. dangerous? */
		return 1;
        return(gp_fe_confirm(camera, message));
}
