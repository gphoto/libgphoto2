#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <sys/types.h>
#include <gphoto2.h>

#include "core.h"
#include "library.h"
#include "util.h"
#include "globals.h"

int is_library(char *library_filename) {

        char buf[1024];
        void *lh;


#if defined(OS2) || defined(WINDOWS)
        sprintf(buf, "%s\\%s", CAMLIBS, library_filename);
#else
        sprintf(buf, "%s/%s", CAMLIBS, library_filename);
#endif
        if ((lh = dlopen(buf, RTLD_LAZY))==NULL) {
                if (glob_debug)
                        perror("core:\tis_library");
                return (GP_ERROR);
        }

        dlclose(lh);

        return (GP_OK);
}

int load_library (Camera *camera, char *camera_name) {

        char buf[1024];
        void *lh;
        int x;
        for (x=0; x<glob_camera_count; x++) {
           if (strcmp(glob_camera[x].name, camera_name)==0) {
                sprintf(buf, "%s/%s", CAMLIBS, glob_camera[x].library);
                if ((lh = dlopen(buf, RTLD_LAZY))==NULL) {
                        if (glob_debug)
                                perror("core:\tload_library");
                        return (GP_ERROR);
                }
                camera->library_handle = lh;
                camera->functions->id = dlsym(lh, "camera_id");
                camera->functions->abilities = dlsym(lh, "camera_abilities");
                camera->functions->init = dlsym(lh, "camera_init");
/* replaced with camera_init, which initializes the ->functions
                camera->functions->exit = dlsym(lh, "camera_exit");
                camera->functions->folder_list = dlsym(lh, "camera_folder_list");
                camera->functions->folder_set = dlsym(lh, "camera_folder_set");
                camera->functions->file_count = dlsym(lh, "camera_file_count");
                camera->functions->file_get = dlsym(lh, "camera_file_get");
                camera->functions->file_get_preview = dlsym(lh, "camera_file_get_preview");
                camera->functions->file_put = dlsym(lh, "camera_file_put");
                camera->functions->file_delete = dlsym(lh, "camera_file_delete");
                camera->functions->file_lock = dlsym(lh, "camera_file_lock");
                camera->functions->file_unlock = dlsym(lh, "camera_file_unlock");
                camera->functions->config_get = dlsym(lh, "camera_config_get");
                camera->functions->config_set = dlsym(lh, "camera_config_set");
                camera->functions->capture = dlsym(lh, "camera_capture");
                camera->functions->summary = dlsym(lh, "camera_summary");
                camera->functions->manual = dlsym(lh, "camera_manual");
                camera->functions->about = dlsym(lh, "camera_about");
*/

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
                if (glob_debug)
                        perror("core:\tload_camera_list");
                return 0;
        }

        /* check to see if this library has been loaded */
        load_camera_id = dlsym(lh, "camera_id");
        load_camera_id(id);

        if (glob_debug)
                printf("core:\t library id: %s\n", id);

        for (x=0; x<glob_camera_id_count; x++) {
                if (strcmp(glob_camera_id[x], id)==0) {
                        dlclose(lh);
                        return (GP_ERROR);
                }
        }

        strcpy(glob_camera_id[glob_camera_id_count], id);
        glob_camera_id_count++;

        /* load in the camera_abilities function */
        gp_abilities_clear(&glob_camera_abilities[glob_camera_count]);
        load_camera_abilities = dlsym(lh, "camera_abilities");
        if (load_camera_abilities(&glob_camera_abilities[glob_camera_count],
                                  &count) == GP_ERROR) {
                dlclose(lh);
                return 0;
        }

        if (glob_debug)
                printf("core:\t camera model count: %i\n", count);

        /* Copy over the camera name */
        for (x=glob_camera_count; x<glob_camera_count+count; x++) {
                strcpy(glob_camera[x].name, glob_camera_abilities[x].model);
                strcpy(glob_camera[x].library, library_filename);
                if (glob_debug)
                        printf("core:\t camera model: %s\n", glob_camera[x].name);
        }

        glob_camera_count += count;

        dlclose(lh);


        return (x);
}

int load_cameras() {

        DIR *d;
        CameraChoice t;
        CameraAbilities a;

        int x, y, z;
        struct dirent *de;

        glob_camera_id_count = 0;

        /* Look for available camera libraries */
        d = opendir(CAMLIBS);
        if (!d) {
                fprintf(stderr, "core: couldn't open %s\n", CAMLIBS);
                return GP_ERROR;
        }

        do {
           /* Read each entry */
           de = readdir(d);
           if (de) {
                if (glob_debug)
                   printf("core:\tis %s a library? ", de->d_name);
                /* try to open the library */
                if (is_library(de->d_name) == GP_OK) {
                        if (glob_debug)
                                printf("yes\n");
                        load_camera_list(de->d_name);
                   } else {
                        if (glob_debug)
                                printf("no\n");
                }
           }
        } while (de);


        /* Sort the camera list */
        for (x=0; x<glob_camera_count-1; x++) {
                for (y=x+1; y<glob_camera_count; y++) {
                        z = strcmp(glob_camera[x].name, glob_camera[y].name);
                        if (z > 0) {
                                memcpy(&t, &glob_camera[x], sizeof(t));
                                memcpy(&glob_camera[x], &glob_camera[y], sizeof(t));
                                memcpy(&glob_camera[y], &t, sizeof(t));

                                memcpy(&a, &glob_camera_abilities[x], sizeof(t));
                                memcpy(&glob_camera_abilities[x], &glob_camera_abilities[y],
                                        sizeof(t));
                                memcpy(&glob_camera_abilities[y], &a, sizeof(t));
                        }
                }
        }
        return (GP_OK);
}

int close_library (Camera *camera) {

        dlclose(camera->library_handle);

        return (GP_OK);
}
