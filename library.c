#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <sys/types.h>
#include <gphoto2.h>

#include "core.h"
#include "library.h"
#include "util.h"

extern CameraChoice glob_camera[];
extern CameraAbilities glob_camera_abilities[];
extern int     glob_camera_count;
extern void   *glob_library_handle;
extern Camera  glob_c;
extern char   *glob_camera_id[];
extern int     glob_camera_id_count;

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
		glob_c.id = dlsym(lh, "camera_id");
		glob_c.abilities = dlsym(lh, "camera_abilities");
		glob_c.init = dlsym(lh, "camera_init");
		glob_c.exit = dlsym(lh, "camera_exit");
		glob_c.open = dlsym(lh, "camera_open");
		glob_c.close = dlsym(lh, "camera_close");
		glob_c.folder_list = dlsym(lh, "camera_folder_list");
		glob_c.folder_set = dlsym(lh, "camera_folder_set");
		glob_c.file_count = dlsym(lh, "camera_file_count");
		glob_c.file_get = dlsym(lh, "camera_file_get");
		glob_c.file_get_preview = dlsym(lh, "camera_file_get_preview");
		glob_c.file_put = dlsym(lh, "camera_file_put");
		glob_c.file_delete = dlsym(lh, "camera_file_delete");
		glob_c.file_lock = dlsym(lh, "camera_file_lock");
		glob_c.file_unlock = dlsym(lh, "camera_file_unlock");
		glob_c.config = dlsym(lh, "camera_config");
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

int load_camera_list (char *library_filename) {
	/* Returns the number of cameras added to the CameraChoice list */
	/* 0 implies an error occurred */

	char buf[1024], id[64];
	void *lh;
	c_abilities load_camera_abilities;
	c_id load_camera_id;
	int x, count;

	sprintf(buf, "%s/%s", CAMLIBS, library_filename);

	/* try to open the library */
	if ((lh = dlopen(buf, RTLD_LAZY))==NULL) {
		perror("load_library");
		return 0;
	}

	/* check to see if this library has been loaded */
	load_camera_id = dlsym(lh, "camera_id");
	load_camera_id(id);
#ifdef DEBUG 
	printf("camera id: %s\n", id);
#endif
	for (x=0; x<glob_camera_id_count; x++) {
		if (strcmp(glob_camera_id[x], id)==0) {
			dlclose(lh);
			return (GP_ERROR);
		}
	}

	strcpy(glob_camera_id[glob_camera_id_count], id);
	glob_camera_id_count++;

	/* load in the camera_abilities function */
	load_camera_abilities = dlsym(lh, "camera_abilities");
	if (load_camera_abilities(&glob_camera_abilities[glob_camera_count], 
				  &count) == GP_ERROR) {
		dlclose(lh);
		return 0;
	}

#ifdef DEBUG
	printf("camera model count: %i\n", count);
#endif

	/* Copy over the camera name */
	for (x=glob_camera_count; x<glob_camera_count+count; x++) {
		strcpy(glob_camera[x].name, glob_camera_abilities[x].model);
		strcpy(glob_camera[x].library, library_filename);		
#ifdef DEBUG
		printf("camera model: %s\n", glob_camera[x].name);
#endif
	}

	glob_camera_count += count;
	
	dlclose(lh);

	return (x);
}

int load_cameras() {

        DIR *d;
        struct dirent *de;

	glob_camera_id_count = 0;

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
                        load_camera_list(de->d_name);
#ifdef DEBUG
                   } else {
                        printf("no\n");
#endif
                }
           }
        } while (de);
	return (GP_OK);
}
