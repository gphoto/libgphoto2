#include <dlfcn.h>
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

	CameraPortSettings glob_port_settings;

	/* Currently Selected camera/folder */
	/* ------------------------------------------------ */
			/* currently selected camera number */
	int 		glob_camera_number = -1;
			/* currently selected folder path */
	char 		glob_folder_path[512];

	/* Camera List */
	/* ------------------------------------------------ */
			/* total cameras found 		*/
	int		glob_camera_count = 0;
			/* camera/library list 		*/
	CameraChoice	glob_camera[512];
			/* camera abilities 		*/
	CameraAbilities glob_camera_abilities[512];
			/* camera library id's 		*/
	char		*glob_camera_id[512];
			/* number of camera library id's*/
	int		glob_camera_id_count = 0;

	/* Currently loaded settings */
	/* ------------------------------------------------ */
			/* number of settings found 	*/
	int		glob_setting_count = 0; 
			/* setting key/value list   	*/
	Setting		glob_setting[512];  


	/* Current loaded library's handle */
	/* ------------------------------------------------ */
			/* current library handle    	*/
	void 		*glob_library_handle; 
			/* pointer to current camera function set */
	Camera   	glob_c;

/* Core functions (for front-ends)
   ---------------------------------------------------------------- */

int gp_init (int debug) {

	int x;
	char buf[1024];

	glob_debug = debug;

	/* Initialize the globals */
	glob_camera_number   = 0;
	glob_camera_count    = 0;
	glob_setting_count   = 0;
	glob_library_handle  = NULL;
	strcpy(glob_folder_path, "/");
	glob_camera_id_count = 0;

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
		gp_message("ERROR: Can not initialize libgpio");
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
			glob_camera[x].name, glob_camera[x].library);
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

	gp_camera_exit();
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

	strcpy(camera_name, glob_camera[camera_number].name);
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
		if (strcmp(glob_camera[x].name, camera_name)==0)
			return (gp_camera_abilities(x, abilities));
		x++;
	}

	return (GP_ERROR);

}

int gp_camera_set (int camera_number, CameraPortSettings *settings) {

	CameraInit ci;

	if (camera_number >= glob_camera_count)
		return (GP_ERROR);

	if (glob_library_handle != NULL)
		gp_camera_exit();
	glob_library_handle = NULL;
	if (load_library(glob_camera[camera_number].name)==GP_ERROR)
		return (GP_ERROR);
	glob_camera_number = camera_number;

	if (glob_debug)
		printf("core: Initializing \"%s\" (%s)...\n", 
			glob_camera[camera_number].name,
			glob_camera[camera_number].library);
	strcpy(ci.model, glob_camera[camera_number].name);
	memcpy(&ci.port_settings, settings, sizeof(ci.port_settings));

	ci.debug = glob_debug;

	if (gp_camera_init(&ci)==GP_ERROR)
		return (GP_ERROR);



	return(GP_OK);
}

int gp_camera_set_by_name (char *camera_name, CameraPortSettings *settings) {

	int x=0;

	while (x < glob_camera_count) {
		if (strcmp(glob_camera[x].name, camera_name)==0)
			return (gp_camera_set(x, settings));
		x++;
	}

	return (GP_ERROR);
}

int gp_camera_init (CameraInit *init) {

	if (glob_c.init == NULL)
		return(GP_ERROR);

	return(glob_c.init(init));
}

int gp_camera_exit () {

	int ret;

	if (glob_c.exit == NULL)
		return(GP_ERROR);

	ret = glob_c.exit();
	close_library();

	return (ret);
}

int gp_folder_list(char *folder_path, CameraFolderInfo *list) {

	CameraFolderInfo t;
	int x, y, z, count;

	if (glob_c.folder_list == NULL)
		return (GP_ERROR);

	count = glob_c.folder_list(folder_path, list);

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

int gp_folder_set (char *folder_path) {

	if (glob_c.folder_set == NULL)
		return (GP_ERROR);

	if (glob_c.folder_set(folder_path) == GP_ERROR)
		return (GP_ERROR);

	strcpy(glob_folder_path, folder_path);
	return(GP_OK);
}

int gp_file_count () {

	if (glob_c.file_count == NULL)
		return (GP_ERROR);

	return(glob_c.file_count());
}

int gp_file_get (int file_number, CameraFile *file) {

	if (glob_c.file_get == NULL)
		return (GP_ERROR);

	gp_file_clean(file);
	return (glob_c.file_get(file_number, file));
}

int gp_file_get_preview (int file_number, CameraFile *preview) {

	if (glob_c.file_get_preview == NULL)
		return (GP_ERROR);

	gp_file_clean(preview);
	return (glob_c.file_get_preview(file_number, preview));
}

int gp_file_put (CameraFile *file) {

	if (glob_c.file_put == NULL)
		return (GP_ERROR);

	return (glob_c.file_put(file));
}


int gp_file_delete (int file_number) {

	if (glob_c.file_delete == NULL)
		return (GP_ERROR);
	return (glob_c.file_delete(file_number));
}

int gp_file_lock (int file_number) {

	return (GP_ERROR);
}

int gp_file_unlock (int file_number) {

	return (GP_ERROR);
}


int gp_config_get (CameraWidget *window) {

	/*
	strcpy(config_dialog_name, LIB/library_name.config);
	*/

	return(GP_OK);
}

int gp_config_set (CameraSetting *setting, int count) {

	if (glob_c.config_set == NULL)
		return (GP_ERROR);

	return(glob_c.config_set(setting, count));
}

int gp_capture (CameraFile *f, CameraCaptureInfo *info) {

	if (glob_c.capture == NULL)
		return (GP_ERROR);

	return(glob_c.capture(f, info));
}

int gp_summary (CameraText *summary) {

	if (glob_c.summary == NULL)
		return (GP_ERROR);

	return(glob_c.summary(summary));
}

int gp_manual (CameraText *manual) {

	if (glob_c.manual == NULL)
		return (GP_ERROR);

	return(glob_c.manual(manual));
}

int gp_about (CameraText *about) {

	if (glob_c.about == NULL)
		return (GP_ERROR);

	return(glob_c.about(about));
}

/* Configuration file functions (for front-ends and libraries)
   ---------------------------------------------------------------------------- */

int gp_setting_get (char *key, char *value) {

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

int gp_setting_set (char *key, char *value) {

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

int gp_status (char *status) {

	gp_interface_status(status);
	return(GP_OK);
}

int gp_progress (float percentage) {

	gp_interface_progress(percentage);
	return(GP_OK);
}

int gp_message (char *message) {

	gp_interface_message(message);
	return(GP_OK);
}

int gp_confirm (char *message) {

	return(gp_interface_confirm(message));
}
