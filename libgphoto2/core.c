#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <gpio.h>
#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "core.h"
#include "library.h"
#include "settings.h"
#include "util.h"

/* Some globals
   ---------------------------------------------------------------- */

	int		glob_debug=0;

	/* Camera List */
	/* ------------------------------------------------ */
	int		glob_camera_count = 0;
	CameraChoice	glob_camera_list[512];
	CameraAbilities glob_camera_abilities[512];
	char		*glob_camera_id[512];
	int		glob_camera_id_count = 0;

	/* Currently loaded settings */
	/* ------------------------------------------------ */
	int		glob_setting_count = 0; 
	Setting		glob_setting[512];  

/* Core functions (for front-ends)
   ---------------------------------------------------------------- */

int gp_camera_init (Camera *camera, CameraInit *init);
int gp_camera_exit (Camera *camera);

int gp_init (int debug) {

	int x;
	char buf[1024];

	glob_debug = debug;

	/* Initialize the globals */
	glob_camera_count    = 0;
	glob_camera_id_count = 0;
	glob_setting_count   = 0;

	for(x=0; x<512; x++)
		glob_camera_id[x] = (char *)malloc(sizeof(char)*64);

	if (glob_debug) {
		printf("core: >> Debug Mode On << \n");
		printf("core: Creating $HOME/.gphoto\n");
	}

	/* Make sure the directories are created */
	sprintf(buf, "%s/.gphoto", getenv("HOME"));
	(void)mkdir(buf, 0700);

	if (glob_debug)
		printf("core: Initializing gpio:\n");

	if (gpio_init() == GPIO_ERROR) {
		gp_camera_message(NULL, "ERROR: Can not initialize libgpio");
		return (GP_ERROR);
	}

	if (glob_debug)
		printf("core: Trying to load settings:\n");
	/* Load settings */
	load_settings();

	if (glob_debug) {
		printf("core: Camera library dir: %s\n", CAMLIBS);
		printf("core: Trying to load libraries:\n");
	}
	load_cameras();

	if (glob_debug) {
		int x;
		printf("core: List of cameras found:\n");
		for (x=0; x<glob_camera_count; x++)
		printf("core:\t\"%s\" uses %s\n", 
			glob_camera_list[x].name, glob_camera_list[x].library);
		if (glob_camera_count == 0)
			printf("core:\tNone\n");
		printf("core: Initializing the gPhoto IO library (libgpio)\n");
	}	

	return (GP_OK);
}

int gp_exit () {
	
	int x;
	for(x=0; x<512; x++)
                free(glob_camera_id[x]);

	return (GP_OK);
}

int gp_port_count() {

	return (gpio_get_device_count());
}

int gp_port_info(int port_number, CameraPortInfo *info) {

	gpio_device_info i;

	if (gpio_get_device_info(port_number, &i)==GPIO_ERROR)
		return (GP_ERROR);

	/* Translate to gPhoto types */
	switch (i.type) {
		case GPIO_DEVICE_SERIAL:
			info->type = GP_PORT_SERIAL;
			break;
		case GPIO_DEVICE_PARALLEL:
			info->type = GP_PORT_PARALLEL;
			break;
	        case GPIO_DEVICE_USB:
			info->type = GP_PORT_USB;
			break;
	        case GPIO_DEVICE_IEEE1394:
			info->type = GP_PORT_IEEE1394;
			break;
	        case GPIO_DEVICE_NETWORK:
			info->type = GP_PORT_NETWORK;
			break;
		default:
			info->type = GP_PORT_NONE;
	}	
	strcpy(info->name, i.name);
	strcpy(info->path, i.path);

	return (GP_OK);
}

int gp_camera_count () {

	return(glob_camera_count);
}

int gp_camera_name (int camera_number, char *camera_name) {

	if (camera_number > glob_camera_count)
		return (GP_ERROR);

	strcpy(camera_name, glob_camera_list[camera_number].name);
	return (GP_OK);
}

int gp_camera_abilities (int camera_number, CameraAbilities *abilities) {

	memcpy(abilities, &glob_camera_abilities[camera_number],
	       sizeof(CameraAbilities));

	if (glob_debug)
		gp_abilities_dump(abilities);

	return (GP_OK);
}

int gp_camera_abilities_by_name (char *camera_name, CameraAbilities *abilities) {

	int x=0;
	while (x < glob_camera_count) {
		if (strcmp(glob_camera_list[x].name, camera_name)==0)
			return (gp_camera_abilities(x, abilities));
		x++;
	}

	return (GP_ERROR);

}

int gp_camera_new (Camera **camera, int camera_number, CameraPortInfo *settings) {

	CameraInit ci;

	if (camera_number >= glob_camera_count)
		return (GP_ERROR);

	*camera = (Camera*)malloc(sizeof(Camera));

	/* Initialize the members */
	strcpy((*camera)->model, glob_camera_list[camera_number].name);
	(*camera)->debug      = glob_debug;
	(*camera)->port	      = (CameraPortInfo*)malloc(sizeof(CameraPortInfo));
	(*camera)->abilities  = (CameraAbilities*)malloc(sizeof(CameraAbilities));
	(*camera)->functions  = (CameraFunctions*)malloc(sizeof(CameraFunctions));
	(*camera)->library_handle  = NULL;
	(*camera)->camlib_data     = NULL;
	(*camera)->frontend_data   = NULL;

	memcpy((*camera)->port, settings, sizeof(CameraPortInfo));

	if (load_library(*camera, glob_camera_list[camera_number].name)==GP_ERROR) {
		gp_camera_free(*camera);
		return (GP_ERROR);
	}

	/* Initialize the camera library */
	if (glob_debug)
		printf("core: Initializing \"%s\" (%s)...\n", 
			glob_camera_list[camera_number].name,
			glob_camera_list[camera_number].library);
	strcpy(ci.model, glob_camera_list[camera_number].name);
	memcpy(&ci.port_settings, settings, sizeof(CameraPortInfo));

	if (gp_camera_init(*camera, &ci)==GP_ERROR) {
		gp_camera_free(*camera);
		*camera=NULL;
		return (GP_ERROR);
	}

	return(GP_OK);
}

int gp_camera_free(Camera *camera) {

	gp_camera_exit(camera);

	if (camera->port)
		free(camera->port);
	if (camera->abilities)
		free(camera->abilities);
	if (camera->functions)
		free(camera->functions);

	return (GP_OK);	
}

int gp_camera_new_by_name (Camera **camera, char *camera_name, CameraPortInfo *settings) {

	int x=0;

	while (x < glob_camera_count) {
		if (strcmp(glob_camera_list[x].name, camera_name)==0)
			return (gp_camera_new(camera, x, settings));
		x++;
	}

	return (GP_ERROR);
}

int gp_camera_init (Camera *camera, CameraInit *init) {

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

int gp_camera_exit (Camera *camera) {

	int ret;

	if (camera->functions->exit == NULL)
		return(GP_ERROR);

	ret = camera->functions->exit(camera);
	close_library(camera);

	return (ret);
}

int gp_camera_folder_list(Camera *camera, char *folder_path, CameraFolderInfo *list) {

	CameraFolderInfo t;
	int x, y, z, count;

	if (camera->functions->folder_list == NULL)
		return (GP_ERROR);

	count = camera->functions->folder_list(camera, folder_path, list);

	if (count == GP_ERROR)
		return (GP_ERROR);

	/* Sort the folder list */
	for (x=0; x<count-1; x++) {
		for (y=x+1; y<count; y++) {
			z = strcmp(list[x].name, list[y].name);
			if (z > 0) {
				memcpy(&t, &list[x], sizeof(t));
				memcpy(&list[x], &list[y], sizeof(t));
				memcpy(&list[y], &t, sizeof(t));
                        }
                }
        }

	return(count);
}

int gp_camera_folder_set (Camera *camera, char *folder_path) {

	if (camera->functions->folder_set == NULL)
		return (GP_ERROR);

	if (camera->functions->folder_set(camera, folder_path) == GP_ERROR)
		return (GP_ERROR);

	return(GP_OK);
}

int gp_camera_file_count (Camera *camera) {

	if (camera->functions->file_count == NULL)
		return (GP_ERROR);

	return(camera->functions->file_count(camera));
}

int gp_camera_file_get (Camera *camera, CameraFile *file, int file_number) {

	if (camera->functions->file_get == NULL)
		return (GP_ERROR);

	gp_file_clean(file);

	return (camera->functions->file_get(camera, file, file_number));
}

int gp_camera_file_get_preview (Camera *camera, CameraFile *file, int file_number) {

	if (camera->functions->file_get_preview == NULL)
		return (GP_ERROR);

	gp_file_clean(file);

	return (camera->functions->file_get_preview(camera, file, file_number));
}

int gp_camera_file_put (Camera *camera, CameraFile *file) {

	if (camera->functions->file_put == NULL)
		return (GP_ERROR);

	return (camera->functions->file_put(camera, file));
}

int gp_camera_file_delete (Camera *camera, int file_number) {

	if (camera->functions->file_delete == NULL)
		return (GP_ERROR);
	return (camera->functions->file_delete(camera, file_number));
}

int gp_camera_config_get (Camera *camera, CameraWidget *window) {

	int ret;

	if (camera->functions->config_get == NULL)
		return (GP_ERROR);

	ret = camera->functions->config_get(camera, window);

	if (glob_debug)
		gp_widget_dump(window);

	return (ret);
}

int gp_camera_config_set (Camera *camera, CameraSetting *setting, int count) {

	int x;

	if (glob_debug) {
		printf("core: Dumping CameraSetting:\n");
		for (x=0; x<count; x++)
			printf("core: \"%s\" = \"%s\"\n", setting[x].name, setting[x].value);
	}

	if (camera->functions->config_set == NULL)
		return (GP_ERROR);

	return(camera->functions->config_set(camera, setting, count));
}

int gp_camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	if (camera->functions->capture == NULL)
		return (GP_ERROR);

	gp_file_clean(file);

	return(camera->functions->capture(camera, file, info));
}

int gp_camera_summary (Camera *camera, CameraText *summary) {

	if (camera->functions->summary == NULL)
		return (GP_ERROR);

	return(camera->functions->summary(camera, summary));
}

int gp_camera_manual (Camera *camera, CameraText *manual) {

	if (camera->functions->manual == NULL)
		return (GP_ERROR);

	return(camera->functions->manual(camera, manual));
}

int gp_camera_about (Camera *camera, CameraText *about) {

	if (camera->functions->about == NULL)
		return (GP_ERROR);

	return(camera->functions->about(camera, about));
}

/* Configuration file functions (for front-ends and libraries)
   ---------------------------------------------------------------------------- */

int gp_setting_get (char *id, char *key, char *value) {

	int x;

	for (x=0; x<glob_setting_count; x++) {
		if (strcmp(glob_setting[x].key, key)==0) {
			strcpy(value, glob_setting[x].value);
			return (GP_OK);
		}
	}
	strcpy(value, "");
	return(GP_ERROR);
}

int gp_setting_set (char *id, char *key, char *value) {

	int x;

	if (glob_debug)
		printf("core: Setting key \"%s\" to value \"%s\"\n",
			key,value);

	for (x=0; x<glob_setting_count; x++) {
		if (strcmp(glob_setting[x].key, key)==0) {
			strcpy(glob_setting[x].value, value);
			save_settings(glob_setting, glob_setting_count);
			return (GP_OK);
		}
	}
	strcpy(glob_setting[glob_setting_count].key, key);
	strcpy(glob_setting[glob_setting_count++].value, value);
	save_settings(glob_setting, glob_setting_count);

	return(GP_OK);
}


/* Front-end interaction functions (libraries calls these!)
   ---------------------------------------------------------------------------- */

int gp_camera_status (Camera *camera, char *status) {

	gp_interface_status(camera, status);
	return(GP_OK);
}

int gp_camera_progress (Camera *camera, CameraFile *file, float percentage) {

	gp_interface_progress(camera, file, percentage);

	return(GP_OK);
}

int gp_camera_message (Camera *camera, char *message) {

	gp_interface_message(camera, message);
	return(GP_OK);
}

int gp_camera_confirm (Camera *camera, char *message) {

	return(gp_interface_confirm(camera, message));
}
