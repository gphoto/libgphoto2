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
	int dirty;
	CameraFilesystemFile *file;
} CameraFilesystemFolder;

struct _CameraFilesystem {
	int count;
	CameraFilesystemFolder *folder;
};

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_MEM(m)         {if (!(m)) return (GP_ERROR_NO_MEMORY);}

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

static int
gp_filesystem_folder_number (CameraFilesystem *fs, const char *folder)
{
	int x, len;

	CHECK_NULL (fs && folder);

	/*
	 * We are nice to front-end/camera-driver writers - we'll ignore
	 * trailing slashes (if any).
	 */
	len = strlen (folder);
	if ((len > 1) && (folder[len - 1] == '/'))
		len--;

	for (x = 0; x < fs->count; x++)
		if (!strncmp (fs->folder[x].name, folder, len))
			return (x);

	return (GP_ERROR_DIRECTORY_NOT_FOUND);
}

static int
gp_filesystem_append_folder (CameraFilesystem *fs, const char *folder)
{
	CameraFilesystemFolder *new;
	int x;

	CHECK_NULL (fs && folder);

	/* Make sure the directory doesn't exist */
	x = gp_filesystem_folder_number (fs, folder);
	if (x >= 0)
		return (GP_ERROR_DIRECTORY_EXISTS);
	else if (x != GP_ERROR_DIRECTORY_NOT_FOUND)
		return (x);

	/* Allocate the folder pointer and the actual folder */
	if (fs->count)
		CHECK_MEM (new = realloc (fs->folder,
			sizeof (CameraFilesystemFolder) * (fs->count + 1)))
	else
		CHECK_MEM (new = malloc (sizeof (CameraFilesystemFolder)));
	fs->folder = new;
	fs->count++;

	/* Initialize the folder (and remove trailing slashes if necessary). */
	strcpy (fs->folder[fs->count - 1].name, folder);
	if ((strlen (folder) > 1) &&
	    (fs->folder[fs->count - 1].name[strlen (folder) - 1] == '/'))
		fs->folder[fs->count - 1].name[strlen (folder) - 1] = '\0';
	fs->folder[fs->count - 1].count = 0;
	fs->folder[fs->count - 1].dirty = 0;

	return (GP_OK);
}

int
gp_filesystem_append (CameraFilesystem *fs, const char *folder, 
		      const char *filename) 
{
	CameraFilesystemFile *new;
        int x, y;

	CHECK_NULL (fs && folder);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Appending '%s' to folder "
			 "'%s'...", filename, folder);

	if (*folder != '/')
		return (GP_ERROR_BAD_PARAMETERS);

	/* Check for existence */
	x = gp_filesystem_folder_number (fs, folder);
	if (x == GP_ERROR_DIRECTORY_NOT_FOUND)
		CHECK_RESULT (gp_filesystem_append_folder (fs, folder))
	else if (x < 0)
		return (x);
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	if (!filename)
		return (GP_OK);

	/* If file exists, return error */
	y = gp_filesystem_number (fs, folder, filename);
	if (y >= 0)
		return (GP_ERROR_FILE_EXISTS);
	else if (y != GP_ERROR_FILE_NOT_FOUND)
		return (y);

	/* Allocate a new file in that folder and append the file */
	if (!fs->folder[x].count)
		CHECK_MEM (new = malloc (sizeof (CameraFilesystemFile)))
	else
		CHECK_MEM (new = realloc (fs->folder[x].file,
					sizeof (CameraFilesystemFile) *
					(fs->folder[x].count + 1)));
	fs->folder[x].file = new;
	fs->folder[x].count++;
	strcpy (fs->folder[x].file[fs->folder[x].count - 1].name, filename);

        return (GP_OK);
}

int
gp_filesystem_dump (CameraFilesystem *fs)
{
	int i, j;

	printf ("Dumping Filesystem:\n");
	for (i = 0; i < fs->count; i++) {
		printf ("  Folder: %s\n", fs->folder[i].name);
		for (j = 0; j < fs->folder[i].count; j++) {
			printf ("    %2i: %s\n", j, fs->folder[i].file[j].name);
		}
	}

	return (GP_OK);
}

int
gp_filesystem_list_files (CameraFilesystem *fs, const char *folder, 
		          CameraList *list)
{
	int x, y;

	CHECK_NULL (fs && list && folder);

	list->count = 0;

	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	for (y = 0; y < fs->folder[x].count; y++)
		CHECK_RESULT (gp_list_append (list,
					      fs->folder[x].file[y].name,
					      NULL));

	return (GP_OK);
}

int
gp_filesystem_folder_is_dirty (CameraFilesystem *fs, const char *folder)
{
	int x;

	CHECK_NULL (fs && folder);

	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	return (fs->folder[x].dirty);
}

int
gp_filesystem_folder_set_dirty (CameraFilesystem *fs, const char *folder, 
				int dirty)
{
	int x;

	CHECK_NULL (fs);

	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	fs->folder[x].dirty = dirty;

	return (GP_OK);
}

int
gp_filesystem_list_folders (CameraFilesystem *fs, const char *folder,
			    CameraList *list)
{
	int x, j, offset;

	CHECK_NULL (fs && folder && list);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Listing folders in '%s'...",
			 folder);

	list->count = 0;

	/* Make sure we've got this folder */
	CHECK_RESULT (gp_filesystem_folder_number (fs, folder));

	for (x = 0; x < fs->count; x++)
		if (!strncmp (fs->folder[x].name, folder, strlen (folder))) {
			
			/*
			 * Is this really a subfolder (and not the folder
			 * itself)?
			 */
			if (strlen (fs->folder[x].name) <= strlen (folder))
				continue;

			/*
			 * Is this really a direct subfolder (and not a 
			 * subsubfolder)?
			 */
			for (j = strlen (folder) + 1; 
			     fs->folder[x].name[j] != '\0'; j++)
				if (fs->folder[x].name[j] == '/')
					break;
			if (j == strlen (fs->folder[x].name)) {
				if (!strcmp (folder, "/"))
					offset = 1;
				else
					offset = strlen (folder) + 1;
				CHECK_RESULT (gp_list_append (list,
						fs->folder[x].name + offset,
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

	/*
	 * Search for the folder and create one if it doesn't exist. If it
	 * exists, delete the contents.
	 */
	x = gp_filesystem_folder_number (fs, folder);
	if (x >= 0)
		CHECK_RESULT (gp_filesystem_delete_all (fs, folder))
	else if (x == GP_ERROR_DIRECTORY_NOT_FOUND)
		CHECK_RESULT (gp_filesystem_append_folder (fs, folder))
	else if (x < 0)
		return (x);
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

        /* Allocate the files in that (empty) folder */
	CHECK_MEM (fs->folder[x].file = malloc (
				sizeof (CameraFilesystemFile) * count));
	fs->folder[x].count = count;

        /* Populate the folder with files */
        for (y = 0; y < count; y++) {
		sprintf (buf, format, y + 1);
		strcpy (fs->folder[x].file[y].name, buf);
        }

        return (GP_OK);
}

int
gp_filesystem_count (CameraFilesystem *fs, const char *folder)
{
        int x;

	CHECK_NULL (fs && folder);

	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	return (fs->folder[x].count);
}

int
gp_filesystem_delete (CameraFilesystem *fs, const char *folder, 
		      const char *filename)
{
	CameraFilesystemFolder *new_fop;
	CameraFilesystemFile   *new_fip;
        int x, y;

	CHECK_NULL (fs && folder);

	gp_debug_printf (GP_DEBUG_HIGH, "core",
			 "Deleting file '%s' in folder '%s'...",
			 filename, folder);

	/* If no file is given, first delete the contents of the folder */
	if (!filename)
		CHECK_RESULT (gp_filesystem_delete_all (fs, folder));

	/* Search the folder */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	if (filename) {

		/* Search the file */
		CHECK_RESULT (y = gp_filesystem_number (fs, folder, filename));

		/* Move all files behind one position up */
		for (; y < fs->folder[x].count - 1; y++)
			memcpy (&fs->folder[x].file[y],
				&fs->folder[x].file[y + 1],
				sizeof (CameraFilesystemFile));

		/* Get rid of the last one */
		new_fip = realloc (fs->folder[x].file, sizeof (
			CameraFilesystemFile) * (fs->folder[x].count - 1));
		if (fs->folder[x].count != 1)
			CHECK_MEM (new_fip);
		fs->folder[x].file = new_fip;
		fs->folder[x].count--;
	} else {

		/* Move all folders behind one position up */
		for (; x < fs->count - 1; x++)
			memcpy (&fs->folder[x], &fs->folder[x + 1],
				sizeof (fs->folder[x]));

		/* Free the last one (files have already been deleted) */
		new_fop = realloc (fs->folder,
			sizeof (CameraFilesystemFolder) * (fs->count - 1));
		if (fs->count != 1)
			CHECK_MEM (new_fop);
		fs->folder = new_fop;
		fs->count--;
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
        int x;

	CHECK_NULL (fs);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Formatting file system...");

        for (x = 0; x < fs->count; x++)
		if (fs->folder[x].count)
			free (fs->folder[x].file);

	if (fs->count)
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

	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));
	
	if (filenumber > fs->folder[x].count)
		return (GP_ERROR_FILE_NOT_FOUND);
	
	*filename = fs->folder[x].file[filenumber].name;
	return (GP_OK);
}

int
gp_filesystem_number (CameraFilesystem *fs, const char *folder, 
		      const char *filename)
{
        int x, y;

	CHECK_NULL (fs && folder && filename);

	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));
	
	for (y = 0; y < fs->folder[x].count; y++)
		if (!strcmp (fs->folder[x].file[y].name, filename))
			return (y);

        return (GP_ERROR_FILE_NOT_FOUND);
}

int
gp_filesystem_get_folder (CameraFilesystem *fs, const char *filename, 
			  const char **folder)
{
	int x, y;

	CHECK_NULL (fs && filename && folder);

	for (x = 0; x < fs->count; x++)
		for (y = 0; y < fs->folder[x].count; y++)
			if (!strcmp (fs->folder[x].file[y].name, filename)) {
				*folder = fs->folder[x].name;
				return (GP_OK);
			}

	return (GP_ERROR_FILE_NOT_FOUND);
}
