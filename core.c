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
#include "cache.h"
#include "library.h"
#include "settings.h"

#define DEBUG

/* Some local globals
   ---------------------------------------------------------------- */

	CameraPortSettings glob_port_settings;

	int glob_folder_number;	/* currently selected folder number */
	int glob_camera_number; /* currently selected camera number */

	/* Camera List */
	int		glob_camera_count; /* total cameras found */
	CameraChoice	glob_camera[512];  /* camera/library list */

	/* Currently loaded settings */
	int	glob_setting_count; /* number of settings found */
	Setting	glob_setting[512];  /* setting key/value list   */


	/* Current loaded library's handle */
	void 	*glob_library_handle; /* current library handle    */
	Camera   glob_c; /* pointer to current camera function set */

/* Core functions (for front-ends)
   ---------------------------------------------------------------- */

int gp_init () {

	DIR *d;
	struct dirent *de;
	char buf[1024];

	/* Initialize the globals */
	glob_camera_number = 0;
	glob_camera_count  = 0;
	glob_setting_count = 0;
	glob_library_handle = NULL;
	glob_folder_number = 0;

#ifdef DEBUG
	printf(" > Debug Mode On < \n");
	printf("core: Creating $HOME/.gphoto and $HOME/.gphoto/cache\n");
#endif
	/* Make sure the directories are created */
	sprintf(buf, "%s/.gphoto", getenv("HOME"));
	(void)mkdir(buf, 0700);
	sprintf(buf, "%s/.gphoto/cache", getenv("HOME"));
	(void)mkdir(buf, 0700);

#ifdef DEBUG
	printf("core: Camera library dir: %s\n", CAMLIBS);
	printf("core: Trying to load libraries:\n");
#endif
	/* Load settings */
	load_settings();

	/* Look for available camera libraries */
	d = opendir(CAMLIBS); 
	do {
	   /* Read each entry */
	   de = readdir(d);
	   if (de) {
#ifdef DEBUG
	   printf("core:\tis %s a library? ", de->d_name); 
#endif
		/* try to open the library */
		if (is_library(de->d_name) == GP_OK) {
#ifdef DEBUG
			printf("yes\n");
#endif
			glob_camera_count += load_camera_list(de->d_name, glob_camera, glob_camera_count);
#ifdef DEBUG
		   } else {
			printf("no\n");
#endif
		}
	   }
	} while (de);

#ifdef DEBUG
	{
		int x;
		printf("core: List of cameras found:\n");
		for (x=0; x<glob_camera_count; x++)
		printf("core:\t\"%s\" uses %s\n", 
			glob_camera[x].name, glob_camera[x].library);
		if (glob_camera_count == 0)
			printf("core:\tNone\n");
	}
#endif
	cache_delete(-1, -1, -1); /* wipe all the cache clean */
	if (gp_get_setting("camera", buf) == GP_OK)
		return (load_library(buf));
	return (GP_OK);
}

int gp_exit () {

	cache_delete(-1, -1, -1); /* wipe all the cache clean */
	
	gp_camera_exit();
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

#ifdef DEBUG
	printf("core: Initializing \"%s\" (%s)...\n", 
		glob_camera[camera_number].name,
		glob_camera[camera_number].library);
#endif
	strcpy(ci.model, glob_camera[camera_number].name);
	memcpy(&ci.port_settings, &glob_port_settings, sizeof(glob_port_settings));
	gp_camera_init(&ci);
	return(GP_OK);
}

int gp_camera_abilities (CameraAbilities *abilities) {

	if (glob_c.abilities == NULL)
		return(GP_ERROR);

	return(glob_c.abilities(abilities));
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

int gp_folder_count () {

	if (glob_c.folder_count == NULL)
		return(GP_ERROR);

	return(glob_c.folder_count());
}

int gp_folder_name (int folder_number, char *folder_name) {

	if (glob_c.folder_name == NULL)
		return (GP_ERROR);

	return(glob_c.folder_name(folder_number, folder_name));
}

int gp_folder_set (int folder_number) {

	if (glob_c.folder_set == NULL)
		return (GP_ERROR);

	if (glob_c.folder_set(folder_number) == GP_ERROR)
		return (GP_ERROR);
	glob_folder_number = folder_number;
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

	if (cache_exists(glob_camera_number, glob_folder_number, file_number) == GP_OK)
		return (cache_get(glob_camera_number, glob_folder_number, file_number, file));

	gp_file_clean(file);
	glob_c.file_get(file_number, file);
	cache_put(glob_camera_number, glob_folder_number, file_number, file);

	return (GP_OK);
}

int gp_file_get_preview (int file_number, CameraFile *preview) {

	if (glob_c.file_get_preview == NULL)
		return (GP_ERROR);
	if (cache_exists(glob_camera_number, glob_folder_number, file_number)==GP_OK)
		return (cache_get(glob_camera_number, glob_folder_number, file_number, preview));

	gp_file_clean(preview);
	glob_c.file_get_preview(file_number, preview);

	cache_put(glob_camera_number, glob_folder_number, file_number, preview);

	return(GP_OK);
}

int gp_file_delete (int file_number) {

	/*
	if (glob_c.file_delete == NULL)
		return (GP_ERROR);
	glob_c.file_delete(file_number)
	Remove from the cache
	update cache itself (picture number shifting)
	*/

	return(GP_OK);
}

int gp_file_lock (int file_number) {

	return (GP_ERROR);
}

int gp_file_unlock (int file_number) {

	return (GP_ERROR);
}

int gp_file_save_to_disk (CameraFile *file, char *filename) {
	
	FILE *fp;

	if ((fp = fopen(filename, "w"))==NULL)
		return (GP_ERROR);
	fwrite(file->data, (size_t)sizeof(char), (size_t)file->size, fp);
	fclose(fp);

	return (GP_OK);
}

int gp_file_save (int file_number, char *filename) {

	/*
	write image data to disk as "filename"
	*/

	CameraFile *f;

	f = gp_file_new();
	if (cache_exists(glob_camera_number,glob_folder_number,file_number)
	    == GP_ERROR)
		return (GP_ERROR);
	cache_get(glob_camera_number,glob_folder_number,file_number,f);

	return (gp_file_save_to_disk(f, filename));
}


CameraFile* gp_file_new () {

	/*
	Allocates a new CameraFile
	*/

	CameraFile *f;

	f = (CameraFile*)malloc(sizeof(CameraFile));
	f->type = GP_FILE_UNKNOWN;
	f->name = NULL;
	f->data = NULL;
	f->size = 0;

	return(f);
}

int gp_file_clean (CameraFile *file) {

	/*
	frees a CameraFile's components
	*/

	if (file->name) 
		free(file->name);
	if (file->data)
		free(file->data);
	file->name = NULL;
	file->data = NULL;
	file->size = 0;
	return(GP_OK);
}


int gp_file_free (CameraFile *file) {

	/* 
	frees a CameraFile from memory
	*/

	gp_file_clean(file);
	free(file);
	return(GP_OK);
}


int gp_config_get (char *config_dialog_filename) {

	/*
	strcpy(config_dialog_name, LIB/library_name.config);
	*/

	return(GP_OK);
}

int gp_config_set (char *config_settings) {

	if (glob_c.config_set == NULL)
		return (GP_ERROR);

	return(glob_c.config_set(config_settings));
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

/* Utility (helper) functions for Front-ends
   ---------------------------------------------------------------------------- */

/* Image previews */
int gp_image_preview_rotate (int file_number, int degrees, CameraFile *new_file) {

	return(GP_OK);
}

int gp_image_preview_scale (int file_number, int width, int height, CameraFile *new_file) {

	return(GP_OK);
}

int gp_image_preview_flip_h (int file_number, CameraFile *new_file) {

	return(GP_OK);
}

int gp_image_preview_flip_v (int file_number, CameraFile *new_file) {

	return(GP_OK);
}

/* Full-size images */
int gp_image_rotate (int file_number, int degrees, CameraFile *new_file) {

	return(GP_OK);
}

int gp_image_scale (int file_number, int width, int height, CameraFile *new_file) {

	return(GP_OK);
}

int gp_image_flip_h (int file_number, CameraFile *new_file) {

	return(GP_OK);
}

int gp_image_flip_v (int file_number, CameraFile *new_file) {

	return(GP_OK);
}


/* Utility (helper) functions for libraries
   ---------------------------------------------------------------------------- */

int gp_file_image_rotate (CameraFile *old_file, int degrees, CameraFile *new_file) {

	return(GP_OK);
}

int gp_file_image_scale (CameraFile *old_file, int width, int height, CameraFile *new_file) {

	return(GP_OK);
}

int gp_file_image_flip_h (CameraFile *old_file, CameraFile *new_file) {

	return(GP_OK);
}

int gp_file_image_flip_v (CameraFile *old_file, CameraFile *new_file) {

	return(GP_OK);
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

#ifdef DEBUG
	printf("core: Setting key \"%s\" to value \"%s\"\n",key,value);
#endif

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


/* Front-end interaction functions
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
