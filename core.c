#include <dlfcn.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "core.h"
#include "library.h"
#include "settings.h"

/* Some globals
   ---------------------------------------------------------------- */

	int		glob_debug=0;

	CameraPortSettings glob_port_settings;

	/* Currently Selected camera/folder */
	/* ------------------------------------------------ */
			/* currently selected camera number */
	int 		glob_camera_number;
			/* currently selected folder path */
	char 		glob_folder_path[512];

	/* Camera List */
	/* ------------------------------------------------ */
			/* total cameras found 		*/
	int		glob_camera_count;
			/* camera/library list 		*/
	CameraChoice	glob_camera[512];
			/* camera abilities 		*/
	CameraAbilities glob_camera_abilities[512];
			/* camera library id's 		*/
	char		*glob_camera_id[512];
			/* number of camera library id's*/
	int		glob_camera_id_count;

	/* Currently loaded settings */
	/* ------------------------------------------------ */
			/* number of settings found 	*/
	int		glob_setting_count; 
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

int gp_init () {

	int x;
	char buf[1024];

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
	}

	if (gp_get_setting("camera", buf) == GP_OK)
		return (load_library(buf));
	return (GP_OK);
}

int gp_exit () {
	
	int x;
	for(x=0; x<512; x++)
                free(glob_camera_id[x]);

	gp_camera_exit();
	return (GP_OK);
}

int gp_debug_set (int value) {

	glob_debug = value;

	return (GP_OK);
}

int gp_port_set (CameraPortSettings port_settings) {

	memcpy(&glob_port_settings, &port_settings, sizeof(glob_port_settings));
	
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

int gp_camera_set (int camera_number) {

	CameraInit ci;

	if (camera_number >= glob_camera_count)
		return (GP_ERROR);

	if (glob_library_handle != NULL)
		dlclose(glob_library_handle);
	glob_library_handle = NULL;
	if (load_library(glob_camera[camera_number].name)==GP_ERROR)
		return (GP_ERROR);
	glob_camera_number = camera_number;

	if (glob_debug)
		printf("core: Initializing \"%s\" (%s)...\n", 
			glob_camera[camera_number].name,
			glob_camera[camera_number].library);

	strcpy(ci.model, glob_camera[camera_number].name);
	memcpy(&ci.port_settings, &glob_port_settings, sizeof(glob_port_settings));
	gp_camera_init(&ci);
	return(GP_OK);
}

int gp_camera_abilities (CameraAbilities *abilities) {

	memcpy(abilities, &glob_camera_abilities[glob_camera_number],
	       sizeof(glob_camera_abilities[glob_camera_number]));

	return (GP_OK);
}

int gp_camera_init (CameraInit *init) {

	if (glob_c.init == NULL)
		return(GP_ERROR);

	return(glob_c.init(init));
}

int gp_camera_exit () {

	if (glob_c.exit == NULL)
		return(GP_ERROR);

	return(glob_c.exit());
}

int gp_camera_open () {

	if (glob_c.open == NULL)
		return(GP_ERROR);

	return(glob_c.open());
}

int gp_camera_close () {

	if (glob_c.close == NULL)
		return(GP_ERROR);

	return(glob_c.close());
}

int gp_folder_list(char *folder_path, CameraFolderList *list) {

	return (GP_OK);
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


int gp_config_get (char *config_dialog_filename) {

	/*
	strcpy(config_dialog_name, LIB/library_name.config);
	*/

	return(GP_OK);
}

int gp_config_set (CameraConfig *config, int config_count) {

	if (glob_c.config == NULL)
		return (GP_ERROR);

	return(glob_c.config(config, config_count));
}

int gp_capture (int type) {

	if (glob_c.capture == NULL)
		return (GP_ERROR);

	return(glob_c.capture(type));
}

int gp_summary (char *summary) {

	if (glob_c.summary == NULL)
		return (GP_ERROR);

	return(glob_c.summary(summary));
}

int gp_manual (char *manual) {

	if (glob_c.manual == NULL)
		return (GP_ERROR);

	return(glob_c.manual(manual));
}

int gp_about (char *about) {

	if (glob_c.about == NULL)
		return (GP_ERROR);

	return(glob_c.about(about));
}

/* Configuration file functions (for front-ends and libraries)
   ---------------------------------------------------------------------------- */

int gp_get_setting (char *key, char *value) {

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

int gp_set_setting (char *key, char *value) {

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

int gp_update_status (char *status) {

	interface_update_status(status);
	return(GP_OK);
}

int gp_update_progress (float percentage) {

	interface_update_progress(percentage);
	return(GP_OK);
}

int gp_message (char *message) {

	interface_message(message);
	return(GP_OK);
}

int gp_confirm (char *message) {

	return(interface_confirm(message));
}
