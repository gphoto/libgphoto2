/** \file
 * 
 * \author Copyright 2001 Lutz Mueller <lutz@users.sf.net>
 * \author Copyright 1999 Scott Fritzinger <scottf@unr.edu>
 *
 * \par License
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \par
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * \par
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * This file contains various portability functions that
 * make non UNIX (Windows) ports easier.
 */

#include "config.h"
#include <stdio.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-portability.h>

/* Windows Portability
   ------------------------------------------------------------------ */
#ifdef WIN32


void gp_port_win_convert_path (char *path) {

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

int gp_system_rmdir (const char *dirname) {

        if (_rmdir(dirname) < 0)
                return (GP_ERROR);
        return (GP_OK);
}


gp_system_dir gp_system_opendir (const char *dirname) {

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

gp_system_dirent gp_system_readdir (gp_system_dir d) {

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

const char *gp_system_filename (gp_system_dirent de) {

        return (de->cFileName);
}

int gp_system_closedir (gp_system_dir d) {
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

/** 
 * \brief mkdir UNIX functionality
 * \param dirname directory to create
 * 
 * Creates a new directory.
 *
 * \return a gphoto error code
 */
int gp_system_mkdir (const char *dirname) {

        if (mkdir(dirname, 0700)<0)
                return (GP_ERROR);
        return (GP_OK);
}

/**
 * \brief rmdir UNIX functionality
 * \param dirname directory to remove
 * 
 * Removes a empty directory.
 *
 * \return a gphoto error code
 */
int gp_system_rmdir (const char *dirname) {

	if (rmdir (dirname) < 0)
		return (GP_ERROR);

	return (GP_OK);
}

/**
 * \brief opendir UNIX functionality
 * \param dirname directory to open
 * 
 * Opens a directory for readdir and later closedir operations, 
 * to enumerate its contents.
 *
 * \return a directory handle for use in gp_system_readdir() and gp_system_closedir()
 */
gp_system_dir gp_system_opendir (const char *dirname) {
        return (opendir(dirname));
}

/**
 * \brief readdir UNIX functionality
 * \param d directory to enumerate
 * 
 * Reads one directory entry from the specified directory handle
 * as returned from gp_system_opendir(). Use gp_system_filename()
 * to extract the filename from the returned value.
 *
 * \return a new gp_system_dirent or NULL
 */
gp_system_dirent gp_system_readdir (gp_system_dir d) {
        return (readdir(d));
}

/**
 * \brief retrieve UNIX filename out of a directory entry
 * \param de directory entry as returned from gp_system_readdir()
 * 
 * Extracts a filename out of the passed directory entry.
 *
 * \return the filename of the directory entry
 */
const char *gp_system_filename (gp_system_dirent de) {
        return (de->d_name);
}

/**
 * \brief closedir UNIX functionality
 * \param dir directory to close
 * 
 * Closes a directory after readdir operations. 
 *
 * \return a gphoto error code
 */
int gp_system_closedir (gp_system_dir dir) {
        closedir(dir);
        return (GP_OK);
}

/**
 * \brief check if passed filename is a file
 * \param filename file name to check
 * 
 * Checks whether the passed in filename is
 * a file and returns this as boolean.
 *
 * \return boolean flag whether passed filename is a file.
 */
int gp_system_is_file (const char *filename) {
        struct stat st;

        if (stat(filename, &st)!=0)
                return 0;
        return (!S_ISDIR(st.st_mode));
}

/**
 * \brief check if passed filename is a directory
 * \param dirname file name to check
 * 
 * Checks whether the passed in dirname is
 * a directory and returns this as boolean.
 *
 * \return boolean flag whether passed filename is a directory.
 */
int gp_system_is_dir (const char *dirname) {
        struct stat st;

        if (stat(dirname, &st)!=0)
                return 0;
        return (S_ISDIR(st.st_mode));
}
#endif
