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

#include "gphoto2-core.h"
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

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

int
gp_filesystem_new (CameraFilesystem **fs)
{
	CHECK_NULL (fs);

        *fs = malloc (sizeof (CameraFilesystem));
	if (!*fs)
		return (GP_ERROR_NO_MEMORY);

        (*fs)->folder = NULL;
        (*fs)->count = 0;

        return (GP_OK);
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
        int x, y;

	CHECK_NULL (fs && folder);
	if (*folder != '/')
		return (GP_ERROR_BAD_PARAMETERS);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Appending '%s' to folder "
			 "'%s'...", filename, folder);

	/* Check for existence */
        for (x = 0; x < fs->count; x++)
                if (!strcmp (fs->folder[x]->name, folder))
			break;
	if (x == fs->count) {

		/* Allocate the folder pointer */
		if (fs->count)
			fs->folder = realloc (fs->folder,
				sizeof (CameraFilesystemFolder*) *
							(fs->count + 1));
		else
			fs->folder = malloc (sizeof (CameraFilesystemFolder*));
		if (!fs->folder) {
			fs->count = 0;
			return (GP_ERROR_NO_MEMORY);
		}
		fs->count++;

		/* Allocate the actual folder */
		fs->folder[x] = malloc (sizeof (CameraFilesystemFolder));
		if (!fs->folder[x])
			return (GP_ERROR_NO_MEMORY); 
		
		/* Initialize the folder */
		strcpy (fs->folder[x]->name, folder);
		fs->folder[x]->count = 0;
	}

	if (!filename)
		return (GP_OK);

	/* If file exists, return error */
	for (y = 0; y < fs->folder[x]->count; y++)
		if (!strcmp (fs->folder[x]->file[y]->name, filename))
			return (GP_ERROR_FILE_EXISTS);

	/* Allocate a new file in that folder */
	if (!fs->folder[x]->count)
		fs->folder[x]->file = malloc (sizeof (CameraFilesystemFile*));
	else
		fs->folder[x]->file = realloc (fs->folder[x]->file,
			sizeof(CameraFilesystemFile*) *
				(fs->folder[x]->count + 1));
	if (!fs->folder[x]->file) {
		fs->folder[x]->count = 0;
		return (GP_ERROR_NO_MEMORY);
	}
	fs->folder[x]->count++;

        /* Append the file */
        fs->folder[x]->file[y] = malloc (sizeof (CameraFilesystemFile));
	if (!fs->folder[x]->file[y])
		return (GP_ERROR_NO_MEMORY);
	strcpy (fs->folder[x]->file [y]->name, filename);

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

	CHECK_NULL (fs && list && folder);

	list->count = 0;
	for (folder_num = 0; folder_num < fs->count; folder_num++)
		if (!strcmp (fs->folder[folder_num]->name, folder))
			break;
	if (folder_num == fs->count)
		return (GP_ERROR_DIRECTORY_NOT_FOUND);

	for (file_num = 0; file_num < fs->folder[folder_num]->count; file_num++)
		CHECK_RESULT (gp_list_append (list,
			fs->folder[folder_num]->file[file_num]->name, NULL));

	return (GP_OK);
}

int
gp_filesystem_list_folders (CameraFilesystem *fs, const char *folder,
			    CameraList *list)
{
	int i, j, offset;

	CHECK_NULL (fs && folder && list);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Listing folders in '%s'...",
			 folder);

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
				CHECK_RESULT (gp_list_append (list,
						fs->folder[i]->name + offset,
						NULL));
			}
		}

	return (GP_OK);
}

int
gp_filesystem_populate (CameraFilesystem *fs, const char *folder, 
		        char *format, int count)
{
        int x, y;
        char buf[1024];

	CHECK_NULL (fs && folder && format);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Populating '%s'...", folder);

	/* Delete all files and folders in the folder (if any exist) */
	gp_filesystem_delete_all (fs, folder);

	/* Search for the folder and create one if it doesn't exist */
        for (x = 0; x < fs->count; x++)
		if (!strcmp (fs->folder[x]->name, folder))
			break;
	if (x == fs->count) {

                /* Allocate the folder pointer */
                fs->folder = realloc (fs->folder,
			sizeof (CameraFilesystemFolder*) * (fs->count + 1));
                if (!fs->folder) {
			fs->count = 0;
                        return (GP_ERROR_NO_MEMORY);
		}
		fs->count++;

		/* Allocate the actual folder */
		fs->folder[x] = malloc (sizeof (CameraFilesystemFolder));
		if (!fs->folder[x])
			return (GP_ERROR_NO_MEMORY);

		strcpy (fs->folder[x]->name, folder);
		fs->folder[x]->count = 0;
        }

        /* Allocate the files in that (empty) folder */
        fs->folder[x]->file = malloc (sizeof (CameraFilesystemFile*) * count);
        if (!fs->folder[x]->file)
                return (GP_ERROR_NO_MEMORY);
	fs->folder[x]->count = count;

        /* Populate the folder with files */
        for (y = 0; y < count; y++) {
		sprintf (buf, format, y + 1);
		fs->folder[x]->file[y] = malloc (sizeof (CameraFilesystemFile));
		if (!fs->folder[x]->file[y])
			return (GP_ERROR_NO_MEMORY);
		strcpy (fs->folder[x]->file[y]->name, buf);
        }

        return (GP_OK);
}

int
gp_filesystem_count (CameraFilesystem *fs, const char *folder)
{
        int x;

	CHECK_NULL (fs && folder);

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
        int x, y;

	CHECK_NULL (fs && folder);

	gp_debug_printf (GP_DEBUG_HIGH, "core",
			 "Deleting file '%s' in folder '%s'...",
			 filename, folder);

	/* If no file is given, first delete the contents of the folder */
	if (!filename)
		CHECK_RESULT (gp_filesystem_delete_all (fs, folder));

	/* Search the folder */
	for (x = 0; x < fs->count; x++)
		if (!strcmp (fs->folder[x]->name, folder))
			break;
	if (x == fs->count)
		return (GP_ERROR_DIRECTORY_NOT_FOUND);

	if (filename) {

		/* Search the file */
		for (y = 0; y < fs->folder[x]->count; y++)
			if (!strcmp (fs->folder[x]->file[y]->name, filename))
				break;
		if (y == fs->folder[x]->count)
			return (GP_ERROR_FILE_NOT_FOUND);

		/* Move all files behind one position up */
		for (; y < fs->folder[x]->count - 1; y++)
			memcpy (&fs->folder[x]->file[y],
				&fs->folder[x]->file[y + 1],
				sizeof (fs->folder[x]->file[y]));

		/* Get rid of the last one */
		fs->folder[x]->count--;
		fs->folder[x]->file = realloc (fs->folder[x]->file,
			sizeof (CameraFilesystemFile) * fs->folder[x]->count);
	} else {

		/* Move all folders behind one position up */
		for (; x < fs->count - 1; x++)
			memcpy (&fs->folder[x], &fs->folder[x + 1],
				sizeof (fs->folder[x]));

		/* Free the last one (files have already been deleted) */
		fs->count--;
		fs->folder = realloc (fs->folder,
				sizeof (CameraFilesystemFolder) * fs->count);
	}

	return (GP_OK);
}

int
gp_filesystem_delete_all (CameraFilesystem *fs, const char *folder)
{
	CameraList list;
	int y;
	char path [1024];
	const char *name;

	CHECK_NULL (fs && folder);

	gp_debug_printf (GP_DEBUG_HIGH, "core",
			 "Deleting all files in '%s'...", folder);

	/* Check for subfolders and delete those */
	CHECK_RESULT (gp_filesystem_list_folders (fs, folder, &list));
	for (y = 0; y < gp_list_count (&list); y++) {
		CHECK_RESULT (gp_list_get_name (&list, y, &name));
		if (strlen (folder) == 1)
			sprintf (path, "/%s", name);
		else
			sprintf (path, "%s/%s", folder, name);
		CHECK_RESULT (gp_filesystem_delete (fs, path, NULL));
	}

	/* Delete the files in this folder */
	CHECK_RESULT (gp_filesystem_list_files (fs, folder, &list));
	for (y = 0; y < gp_list_count (&list); y++) {
		CHECK_RESULT (gp_list_get_name (&list, y, &name));
		CHECK_RESULT (gp_filesystem_delete (fs, folder, name));
	}

	return (GP_OK);
}

int
gp_filesystem_format (CameraFilesystem *fs)
{
        int x, y;

	CHECK_NULL (fs);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Formatting file system...");

        for (x = 0; x < fs->count; x++) {
		for (y = 0; y < fs->folder[x]->count; y++)
			free (fs->folder[x]->file[y]);
		free (fs->folder[x]);
	}
	free (fs->folder);
	fs->count = 0;

        return (GP_OK);
}

int
gp_filesystem_name (CameraFilesystem *fs, const char *folder, int filenumber,
		    const char **filename)
{
        int x;

	CHECK_NULL (fs && folder);

        for (x = 0; x < fs->count; x++) {
                if (!strcmp(fs->folder[x]->name, folder)) {
                        if (filenumber > fs->folder[x]->count)
				return (GP_ERROR_FILE_NOT_FOUND);
			*filename = fs->folder[x]->file[filenumber]->name;
			return (GP_OK);
                }
        }

	return (GP_ERROR_DIRECTORY_NOT_FOUND);
}

int
gp_filesystem_number (CameraFilesystem *fs, const char *folder, 
		      const char *filename)
{
        int x, y;

	CHECK_NULL (fs && folder && filename);

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

	CHECK_NULL (fs && filename && folder);

	for (x = 0; x < fs->count; x++)
		for (y = 0; y < fs->folder[x]->count; y++)
			if (!strcmp (fs->folder[x]->file[y]->name, filename)) {
				*folder = fs->folder[x]->name;
				return (GP_OK);
			}

	return (GP_ERROR_FILE_NOT_FOUND);
}
