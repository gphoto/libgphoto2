#include <stdio.h>
#include <sys/types.h>

#include <gphoto2.h>

#include "globals.h"
#include "library.h"
#include "util.h"

int is_library(char *library_filename)
{
	int ret = GP_OK;
        void *lh;
	c_id id;
       
	if ((lh = GPIO_DLOPEN(library_filename)) == NULL)
                return (GP_ERROR);
	id = (c_id)GPIO_DLSYM(lh, "camera_id");
	if (!id)
		ret = GP_ERROR;
	GPIO_DLCLOSE(lh);

        return (ret);
}

int load_library(Camera *camera, char *camera_name)
{
	void *lh;
	int x;

	for (x=0; x<glob_abilities_list->count; x++) {
		if (strcmp(glob_abilities_list->abilities[x]->model, camera_name)==0) {
			if ((lh = GPIO_DLOPEN(glob_abilities_list->abilities[x]->library))==NULL) {
				if (glob_debug)
					perror("core:\tload_library");
				return (GP_ERROR);
			}

			camera->library_handle = lh;
			camera->functions->id = (c_id)GPIO_DLSYM(lh, "camera_id");
			camera->functions->abilities = (c_abilities)GPIO_DLSYM(lh, "camera_abilities");
			camera->functions->init = (c_init)GPIO_DLSYM(lh, "camera_init");

			return GP_OK;
		}
	}

	return GP_ERROR;
}

/* Returns the number of cameras added to the CameraChoice list */
/* 0 implies an error occurred */
int load_camera_list(char *library_filename)
{
	CameraText id;
	void *lh;
        c_abilities load_camera_abilities;
        c_id load_camera_id;
        int x, old_count;

        /* try to open the library */
        if ((lh = GPIO_DLOPEN(library_filename))==NULL) {
                if (glob_debug)
                        perror("core:\tload_camera_list");
                return 0;
        }

        /* check to see if this library has been loaded */
	load_camera_id = (c_id)GPIO_DLSYM(lh, "camera_id");
	load_camera_id(&id);
	gp_debug_printf(GP_DEBUG_LOW, "core", "\t library id: %s", id.text);

	for (x=0; x<glob_abilities_list->count; x++) {
		if (strcmp(glob_abilities_list->abilities[x]->id, id.text)==0) {
			GPIO_DLCLOSE(lh);
			return (GP_ERROR);
		}
	}

	/* load in the camera_abilities function */
	load_camera_abilities = (c_abilities)GPIO_DLSYM(lh, "camera_abilities");
	old_count = glob_abilities_list->count;

        if (load_camera_abilities(glob_abilities_list) == GP_ERROR) {
                GPIO_DLCLOSE(lh);
                return 0;
        }

	/* Copy in the core-specific information */
	for (x=old_count; x<glob_abilities_list->count; x++) {
		strcpy(glob_abilities_list->abilities[x]->id, id.text);
		strcpy(glob_abilities_list->abilities[x]->library, library_filename);
	}

        GPIO_DLCLOSE(lh);

	return x;
}

int load_cameras_search(char *directory)
{
        GPIO_DIR d;
        GPIO_DIRENT de;
	char buf[1024];

	gp_debug_printf(GP_DEBUG_LOW, "core","Trying to load camera libraries in:");
	gp_debug_printf(GP_DEBUG_LOW, "core","\t%s", directory);

        /* Look for available camera libraries */
        d = GPIO_OPENDIR(directory);
        if (!d) {
                gp_debug_printf(GP_DEBUG_LOW, "core", "couldn't open %s", directory);
                return GP_ERROR;
        }

        do {
           /* Read each entry */
           de = GPIO_READDIR(d);
           if (de) {
		sprintf(buf, "%s%c%s", directory, GPIO_DIR_DELIM, GPIO_FILENAME(de));
                gp_debug_printf(GP_DEBUG_LOW, "core", "\tis %s a library? ", buf);
                /* try to open the library */
                if (is_library(buf) == GP_OK) {
                        gp_debug_printf(GP_DEBUG_LOW, "core", "yes");
                        load_camera_list(buf);
                   } else {
			gp_debug_printf(GP_DEBUG_LOW, "core", "no. reason: %s", GPIO_DLERROR());
                }
           }
        } while (de);

        return (GP_OK);
}

int load_cameras() {

	int x, y, z;
	CameraAbilities *t;

	/* Where should we search for camera libraries? */

	/* The installed camera library directory */
	load_cameras_search(CAMLIBS);

	/* Current directory */
	/* load_cameras_search("."); */

        /* Sort the camera list */
        for (x=0; x<glob_abilities_list->count-1; x++) {
                for (y=x+1; y<glob_abilities_list->count; y++) {
                        z = strcmp(glob_abilities_list->abilities[x]->model, 
				   glob_abilities_list->abilities[y]->model);
                        if (z > 0) {
			  t = glob_abilities_list->abilities[x];
			  glob_abilities_list->abilities[x] = glob_abilities_list->abilities[y];
			  glob_abilities_list->abilities[y] = t;
                        }
                }
        }

/* MUCH too verbose. 
	if (glob_debug) {
		gp_debug_printf(GP_DEBUG_LOW, "core", "Loaded camera list");
		gp_abilities_list_dump(glob_abilities_list);
	}
*/ 

	return GP_OK;
}

int close_library(Camera *camera)
{
        GPIO_DLCLOSE(camera->library_handle);

	return GP_OK;
}

