/* filesys.c
 *
 * Copyright (C) 2000 Scott Fritzinger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gphoto2-filesys.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gphoto2-result.h"

typedef struct {
	char name [128];
} CameraFilesystemFile;

typedef struct {
	int count;
	char name [128];
	CameraFilesystemFile **file;
} CameraFilesystemFolder;

struct _CameraFilesystem {
	int count;
	CameraFilesystemFolder **folder;
};

static CameraFilesystemFile *
gp_filesystem_entry_new (const char *filename) {
        CameraFilesystemFile *fse;

        fse = malloc (sizeof (CameraFilesystemFile));

        if (!fse)
                return (NULL);

        strcpy (fse->name, filename);

        return (fse);
}

CameraFilesystem *
gp_filesystem_new (void)
{
        CameraFilesystem *fs;

        fs = malloc (sizeof (CameraFilesystem));

        if (!fs)
                return (NULL);

        fs->folder = NULL;
        fs->count = 0;

        return (fs);
}

int
gp_filesystem_free (CameraFilesystem *fs)
{
        gp_filesystem_format (fs);

        free (fs);

        return (GP_OK);
}

int
gp_filesystem_append (CameraFilesystem *fs, const char *folder, 
		      const char *filename) 
{
        int x, folder_num = -1, file_num;

	if (!fs || !folder || (*folder != '/'))
		return (GP_ERROR_BAD_PARAMETERS);

        for (x = 0; x < fs->count; x++)
                if (!strcmp (fs->folder[x]->name, folder)) {
                        folder_num = x;
			break;
		}

	if (folder_num < 0) {
		/* Allocate the folder pointer */
		fs->folder = realloc(fs->folder,
			sizeof (CameraFilesystemFolder*) * (fs->count + 1));
		if (!fs->folder)
			return (GP_ERROR_NO_MEMORY);

		/* Allocate the actual folder */
		fs->folder[fs->count] = malloc (sizeof(CameraFilesystemFolder));
		if (!fs->folder[fs->count])
			return (GP_ERROR_NO_MEMORY); 
		
		/* Initialize the folder */
		strcpy (fs->folder[fs->count]->name, folder);
		fs->folder[fs->count]->count = 0;

		folder_num = fs->count;
		fs->count += 1;
	}

	if (!filename)
		return (GP_OK);

	/* If file exists, return error */
	for (x = 0; x < fs->folder[folder_num]->count; x++)
		if (!strcmp (fs->folder[folder_num]->file[x]->name, filename))
			return (GP_ERROR_FILE_EXISTS);

	/* Allocate a new file in that folder */
	file_num = fs->folder[folder_num]->count;
	if (file_num == 0)
		fs->folder[folder_num]->file = malloc(
					sizeof (CameraFilesystemFile*));
	else
		fs->folder[folder_num]->file = realloc(
				fs->folder[folder_num]->file,
				sizeof(CameraFilesystemFile*)*(file_num + 1));
	if (!fs->folder[folder_num]->file)
		return (GP_ERROR_NO_MEMORY);

        /* Append the file */
        fs->folder[folder_num]->file [file_num] =
					gp_filesystem_entry_new (filename);
        fs->folder[folder_num]->count += 1;

        return (GP_OK);
}

int
gp_filesystem_dump (CameraFilesystem *fs)
{
	int i, j;

	printf ("Dumping Filesystem:\n");
	for (i = 0; i < fs->count; i++) {
		printf ("  Folder: %s\n", fs->folder[i]->name);
		for (j = 0; j < fs->folder[i]->count; j++) {
			printf ("    %2i: %s\n", j, fs->folder[i]->file[j]->name);
		}
	}

	return (GP_OK);
}

int
gp_filesystem_list_files (CameraFilesystem *fs, const char *folder, 
		          CameraList *list)
{
	int folder_num;
	int file_num;

	if (!fs || !list || !folder)
		return (GP_ERROR_BAD_PARAMETERS);

	list->count = 0;
	for (folder_num = 0; folder_num < fs->count; folder_num++)
		if (!strcmp (fs->folder[folder_num]->name, folder))
			break;
	if (folder_num == fs->count)
		return (GP_ERROR_DIRECTORY_NOT_FOUND);

	for (file_num = 0; file_num < fs->folder[folder_num]->count; file_num++)
		gp_list_append (list,
				fs->folder[folder_num]->file[file_num]->name);

	return (GP_OK);
}

int
gp_filesystem_list_folders (CameraFilesystem *fs, const char *folder,
			    CameraList *list)
{
	int i, j, offset;

	if (!fs || !folder || !list)
		return (GP_ERROR_BAD_PARAMETERS);

	list->count = 0;

	/* Make sure we've got this folder */
	for (i = 0; i < fs->count; i++)
		if (!strcmp (fs->folder[i]->name, folder))
			break;
	if (i == fs->count)
		return (GP_ERROR_DIRECTORY_NOT_FOUND);

	for (i = 0; i < fs->count; i++)
		if (!strncmp (fs->folder[i]->name, folder, strlen (folder))) {
			
			/*
			 * Is this really a subfolder (and not the folder
			 * itself)?
			 */
			if (strlen (fs->folder[i]->name) <= strlen (folder))
				continue;

			/*
			 * Is this really a direct subfolder (and not a 
			 * subsubfolder)?
			 */
			for (j = strlen (folder) + 1; 
			     fs->folder[i]->name[j] != '\0'; j++)
				if (fs->folder[i]->name[j] == '/')
					break;
			if (j == strlen (fs->folder[i]->name)) {
				if (!strcmp (folder, "/"))
					offset = 1;
				else
					offset = strlen (folder) + 1;
				gp_list_append (list,
						fs->folder[i]->name + offset);
			}
		}

	return (GP_OK);
}

int
gp_filesystem_populate (CameraFilesystem *fs, const char *folder, 
		        char *format, int count)
{
        int x, y, new_folder = -1;
        char buf[1024];

        for (x = 0; x < fs->count; x++) {
                if (strcmp (fs->folder[x]->name, folder) == 0) {
                        /* If folder already populated, free it */
                        for (y = 0; y < fs->folder[x]->count; y++)
                                free (fs->folder[x]->file[y]);
                        free(fs->folder[x]);
                        new_folder = x;
                }
        }

        if (new_folder == -1) {
                /* Allocate the folder pointer */
                fs->folder = realloc(fs->folder,
                        sizeof(CameraFilesystemFolder*)*(fs->count + count));
                if (!fs->folder)
                        return (GP_ERROR_NO_MEMORY);
                new_folder = fs->count;
                fs->count += 1;
        }

        /* Allocate the actual folder */
        fs->folder[new_folder] = malloc (sizeof (CameraFilesystemFolder));

        if (!fs->folder[new_folder])
                return (GP_ERROR_NO_MEMORY);

        strcpy (fs->folder[new_folder]->name, folder);

        /* Allocate the files in that folder */
        fs->folder[new_folder]->file = malloc(
				sizeof (CameraFilesystemFile*) * count);

        if (!fs->folder[new_folder]->file)
                return (GP_ERROR_NO_MEMORY);

        /* Populate the folder with files */
        for (x = 0; x < count; x++) {
                sprintf(buf, format, x+1);
                fs->folder[new_folder]->file[x] = gp_filesystem_entry_new(buf);
        }
        fs->folder[new_folder]->count = count;

        return (GP_OK);
}

int
gp_filesystem_count (CameraFilesystem *fs, const char *folder)
{
        int x;

        for (x = 0; x < fs->count; x++) {
           if (!strcmp (fs->folder[x]->name, folder))
                return (fs->folder[x]->count);
        }

        return (GP_ERROR_DIRECTORY_NOT_FOUND);
}

int
gp_filesystem_delete (CameraFilesystem *fs, const char *folder, 
		      const char *filename)
{
        int x,y, shift = 0;

        for (x = 0; x < fs->count; x++) {
           if (!strcmp(fs->folder[x]->name, folder)) {
                for (y = 0; y < fs->folder[x]->count; y++) {
                        if (!strcmp(fs->folder[x]->file[y]->name, filename))
                                shift = 1;
                        if ((shift)&&(y<fs->folder[x]->count-1))
                                memcpy( &fs->folder[x]->file[y],
                                        &fs->folder[x]->file[y+1],
                                        sizeof(fs->folder[x]->file[y]));
                }
                if (shift)
                        fs->folder[x]->count -= 1;
           }
        }

        if (!shift)
                return (GP_ERROR_FILE_NOT_FOUND);

        return (GP_OK);
}

int
gp_filesystem_format (CameraFilesystem *fs)
{
        int x, y;

	if (!fs)
		return (GP_ERROR_BAD_PARAMETERS);

        for (x = 0; x < fs->count; x++) {
		for (y = 0; y < fs->folder[x]->count; y++)
			free (fs->folder[x]->file[y]);
		free (fs->folder[x]);
	}
	free (fs->folder);
	fs->count = 0;

        return (GP_OK);
}

char *
gp_filesystem_name (CameraFilesystem *fs, const char *folder, int filenumber)
{
        int x;

        for (x = 0; x < fs->count; x++) {
                if (!strcmp(fs->folder[x]->name, folder)) {
                        if (filenumber > fs->folder[x]->count)
                                return (NULL);
                        return (fs->folder[x]->file[filenumber]->name);
                }
        }

        return (NULL);
}

int
gp_filesystem_number (CameraFilesystem *fs, const char *folder, 
		      const char *filename)
{
        int x, y;

        for (x = 0; x < fs->count; x++) {
                if (!strcmp(fs->folder[x]->name, folder)) {
                        for (y = 0; y < fs->folder[x]->count; y++) {
                                if (!strcmp (fs->folder[x]->file[y]->name, 
					    filename))
                                        return (y);
                        }
                }
        }

        return (GP_ERROR_FILE_NOT_FOUND);
}

int
gp_filesystem_get_folder (CameraFilesystem *fs, const char *filename, 
			  const char **folder)
{
	int x, y;

	if (!fs || !filename || !folder)
		return (GP_ERROR_BAD_PARAMETERS);

	for (x = 0; x < fs->count; x++)
		for (y = 0; y < fs->folder[x]->count; y++)
			if (!strcmp (fs->folder[x]->file[y]->name, filename)) {
				*folder = fs->folder[x]->name;
				break;
			}

	return (GP_ERROR_FILE_NOT_FOUND);
}
