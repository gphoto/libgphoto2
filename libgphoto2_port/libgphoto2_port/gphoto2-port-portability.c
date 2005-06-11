/* gphoto2-port-portability.c
 */

#include "config.h"
#include <stdio.h>
#include <gphoto2-port.h>
#include <gphoto2-port-result.h>

/* Windows Portability
   ------------------------------------------------------------------ */
#ifdef WIN32

void gp_port_win_convert_path (const char *path) {

        int x;

        if (strchr(path, '\\'))
                /* already converted */
                return;

        if (path[0] != '.') {
                path[0] = path[1];
                path[1] = ':';
                path[2] = '\\';
        }

        for (x=0; x<strlen(path); x++)
                if (path[x] == '/')
                        path[x] = '\\';
}

int gp_system_mkdir (const char *dirname) {

        if (_mkdir(dirname) < 0)
                return (GP_ERROR);
        return (GP_OK);
}

GP_SYSTEM_DIR gp_system_opendir (const char *dirname) {

        GPPORTWINDIR *d;
        DWORD dr;
        int x;
        printf("blah2\n");
        d = (GPPORTWINDIR*)malloc(sizeof(GPPORTWINDIR));
        d->handle = INVALID_HANDLE_VALUE;
        d->got_first = 0;
        strcpy(d->dir, dirname);
        d->drive_count = 0;
        d->drive_index = 0;

        dr = GetLogicalDrives();

        for (x=0; x<32; x++) {
                if ((dr >> x) & 0x0001) {
                        sprintf(d->drive[d->drive_count], "%c", 'A' + x);
                        d->drive_count += 1;
                }
        }

        return (d);
}

GP_SYSTEM_DIRENT gp_system_readdir (GP_SYSTEM_DIR d) {

        char dirn[1024];

        if (strcmp(d->dir, "/")==0) {
                if (d->drive_index == d->drive_count)
                        return (NULL);
                strcpy(d->search.cFileName, d->drive[d->drive_index]);
                d->drive_index += 1;
                return (&(d->search));
        }


        /* Append the wildcard */

        strcpy(dirn, d->dir);
        gp_port_win_convert_path(dirn);

        if (dirn[strlen(dirn)-1] != '\\')
                strcat(dirn, "\\");
        strcat(dirn, "*");


        if (d->handle == INVALID_HANDLE_VALUE) {
                d->handle = FindFirstFile(dirn, &(d->search));
                if (d->handle == INVALID_HANDLE_VALUE)
                        return NULL;
        } else {
                if (!FindNextFile(d->handle, &(d->search)))
                        return NULL;
        }

        return (&(d->search));
}

const char *gp_system_filename (GP_SYSTEM_DIRENT de) {

        return (de->cFileName);
}

int gp_system_closedir (GP_SYSTEM_DIR d) {
        FindClose(d->handle);
        free(d);
        return (1);
}

int gp_system_is_file (const char *filename) {

        struct stat st;

        gp_port_win_convert_path(filename);

        if (stat(filename, &st)!=0)
                return 0;
        return (st.st_mode & _S_IFREG);
}

int gp_system_is_dir (const char *dirname) {

        struct stat st;

        if (strlen(dirname) <= 3)
                return 1;

        gp_port_win_convert_path(dirname);

        if (stat(dirname, &st)!=0)
                return 0;
        return (st.st_mode & _S_IFDIR);
}


#else

int gp_system_mkdir (const char *dirname) {

        if (mkdir(dirname, 0700)<0)
                return (GP_ERROR);
        return (GP_OK);
}

int gp_system_rmdir (const char *dirname) {

	if (rmdir (dirname) < 0)
		return (GP_ERROR);

	return (GP_OK);
}

gp_system_dir gp_system_opendir (const char *dirname) {
        return (opendir(dirname));
}

gp_system_dirent gp_system_readdir (gp_system_dir d) {
        return (readdir(d));
}

const char *gp_system_filename (gp_system_dirent de) {
        return (de->d_name);
}

int gp_system_closedir (gp_system_dir dir) {
        closedir(dir);
        return (GP_OK);
}

int gp_system_is_file (const char *filename) {
        struct stat st;

        if (stat(filename, &st)!=0)
                return 0;
        return (!S_ISDIR(st.st_mode));
}

int gp_system_is_dir (const char *dirname) {
        struct stat st;

        if (stat(dirname, &st)!=0)
                return 0;
        return (S_ISDIR(st.st_mode));
}
#endif
