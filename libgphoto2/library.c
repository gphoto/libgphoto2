#include <stdio.h>
#include <sys/types.h>

#include <gphoto2.h>

#include "core.h"
#include "library.h"
#include "util.h"
#include "globals.h"

int is_library(char *library_filename) {

		int ret = GP_OK;
        char buf[1024];
        void *lh;
		c_id id;

#if defined(OS2) || defined(WIN32)
        sprintf(buf, "%s\\%s", CAMLIBS, library_filename);
#else
        sprintf(buf, "%s/%s", CAMLIBS, library_filename);
#endif
        
		if ((lh = GPIO_DLOPEN(buf)) == NULL)
                return (GP_ERROR);
		id = (c_id)GPIO_DLSYM(lh, "camera_id");
		if (!id)
			ret = GP_ERROR;
		GPIO_DLCLOSE(lh);

        return (ret);
}

int load_library (Camera *camera, char *camera_name) {

        char buf[1024];
        void *lh;
        int x;
        for (x=0; x<glob_abilities_list->count; x++) {
           if (strcmp(glob_abilities_list->abilities[x]->model, camera_name)==0) {
                sprintf(buf, "%s/%s", CAMLIBS, glob_abilities_list->abilities[x]->library);
                if ((lh = GPIO_DLOPEN(buf))==NULL) {
                        if (glob_debug)
                                perror("core:\tload_library");
                        return (GP_ERROR);
                }
                camera->library_handle = lh;
				camera->functions->id = (c_id)GPIO_DLSYM(lh, "camera_id");
				camera->functions->abilities = (c_abilities)GPIO_DLSYM(lh, "camera_abilities");
				camera->functions->init = (c_init)GPIO_DLSYM(lh, "camera_init");

                return (GP_OK);
           }
        }

        return (GP_ERROR);
}

int load_camera_list (char *library_filename) {
        /* Returns the number of cameras added to the CameraChoice list */
        /* 0 implies an error occurred */

        char buf[1024];
		CameraText id;
        void *lh;
        c_abilities load_camera_abilities;
        c_id load_camera_id;
        int x, old_count;

#if defined(OS2) || defined(WIN32)
		sprintf(buf, "%s\\%s", CAMLIBS, library_filename);
#else
        sprintf(buf, "%s/%s", CAMLIBS, library_filename);
#endif

        /* try to open the library */
        if ((lh = GPIO_DLOPEN(buf))==NULL) {
                if (glob_debug)
                        perror("core:\tload_camera_list");
                return 0;
        }

        /* check to see if this library has been loaded */
	load_camera_id = (c_id)GPIO_DLSYM(lh, "camera_id");
	load_camera_id(&id);

        if (glob_debug)
                printf("core:\t library id: %s\n", id.text);

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

        return (x);
}

int load_cameras() {

        GPIO_DIR d;
        GPIO_DIRENT de;
		CameraAbilities *t;

        int x, y, z;
        

        /* Look for available camera libraries */
        d = GPIO_OPENDIR(CAMLIBS);
        if (!d) {
                fprintf(stderr, "core: couldn't open %s\n", CAMLIBS);
                return GP_ERROR;
        }

        do {
           /* Read each entry */
           de = GPIO_READDIR(d);
           if (de) {
                if (glob_debug)
                   printf("core:\tis %s a library? ", GPIO_FILENAME(de));
                /* try to open the library */
                if (is_library(GPIO_FILENAME(de)) == GP_OK) {
                        if (glob_debug)
                                printf("yes\n");
                        load_camera_list(GPIO_FILENAME(de));
                   } else {
                        if (glob_debug) {
                                printf("no\n");
				printf("core: reason: %s\n", GPIO_DLERROR());
			}
                }
           }
        } while (de);

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

	if (glob_debug) {
		printf("core: Loaded camera list:");
//		gp_abilities_list_dump(glob_abilities_list);
	}

        return (GP_OK);
}

int close_library (Camera *camera) {

        GPIO_DLCLOSE(camera->library_handle);

        return (GP_OK);
}
