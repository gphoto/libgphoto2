#include <dlfcn.h>
#include <stdio.h>
#include <gphoto2.h>

#include "core.h"
#include "library.h"
#include "util.h"

extern CameraChoice glob_camera[];
extern int     glob_camera_count;
extern void   *glob_library_handle;
extern Camera  glob_c;

int is_library(char *library_filename) {

	char buf[1024];
	void *lh;

	sprintf(buf, "%s/%s", CAMLIBS, library_filename);

	if ((lh = dlopen(buf, RTLD_LAZY))==NULL) {
		perror("load_library");
		return (GP_ERROR);
	}
	
	dlclose(lh);

	return (GP_OK);
}

int load_library (char *camera_name) {

	char buf[1024];
	void *lh;
	int x;
	for (x=0; x<glob_camera_count; x++) {
	   if (strcmp(glob_camera[x].name, camera_name)==0) {
		sprintf(buf, "%s/%s", CAMLIBS, glob_camera[x].library);
		if ((lh = dlopen(buf, RTLD_LAZY))==NULL) {
			perror("load_library");
			return (GP_ERROR);
		}
		glob_c.abilities = dlsym(lh, "camera_abilities");
		glob_c.init = dlsym(lh, "camera_init");
		glob_c.exit = dlsym(lh, "camera_exit");
		glob_c.open = dlsym(lh, "camera_open");
		glob_c.close = dlsym(lh, "camera_close");
		glob_c.folder_count = dlsym(lh, "camera_folder_count");
		glob_c.folder_name = dlsym(lh, "camera_folder_name");
		glob_c.folder_set = dlsym(lh, "camera_folder_set");
		glob_c.file_count = dlsym(lh, "camera_file_count");
		glob_c.file_get = dlsym(lh, "camera_file_get");
		glob_c.file_get_preview = dlsym(lh, "camera_file_get_preview");
		glob_c.file_delete = dlsym(lh, "camera_file_delete");
		glob_c.file_lock = dlsym(lh, "camera_file_lock");
		glob_c.file_unlock = dlsym(lh, "camera_file_unlock");
		glob_c.config_set = dlsym(lh, "camera_config_set");
		glob_c.capture = dlsym(lh, "camera_capture");
		glob_c.summary = dlsym(lh, "camera_summary");
		glob_c.manual = dlsym(lh, "camera_manual");
		glob_c.about = dlsym(lh, "camera_about");
		glob_library_handle = lh;

		return (GP_OK);
	   }
	}

	return (GP_ERROR);
}

int load_camera_list (char *library_filename, CameraChoice *camera, int index) {
	/* Returns the number of cameras added to the CameraChoice list */
	/* 0 implies an error occurred */

	char buf[1024];
	void *lh;
	c_abilities load_camera_abilities;
	CameraAbilities a;
	int x;

	sprintf(buf, "%s/%s", CAMLIBS, library_filename);

	/* try to open the library */
	if ((lh = dlopen(buf, RTLD_LAZY))==NULL) {
		perror("load_library");
		return 0;
	}

	/* load in the camera_abilities function */
	load_camera_abilities = dlsym(lh, "camera_abilities");
	if (load_camera_abilities(&a) == GP_ERROR) {
		dlclose(lh);
		return 0;
	}

	/* check to see if this library has been loaded */
	for (x=0; x<index; x++) {
		if (strcmp(camera[x].name, a.model[0])==0) {
			dlclose(lh);
			return (0);
		}
	}
	
	/* read in the supported camera list */
	x=0;
	while (a.model[x]) {
		printf("camera model: %s\n", a.model[x]);
		strcpy(camera[x+index].name, a.model[x]);
		strcpy(camera[x+index].library, library_filename);
		x++;
	}

	/* clean up */
	gp_free_abilities(&a);
	
	dlclose(lh);

	return (x);
}
