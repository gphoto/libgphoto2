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
	int info_dirty;
	CameraFileInfo info;
} CameraFilesystemFile;

typedef struct {
	int count;
	char name [128];
	int files_dirty;
	int folders_dirty;
	CameraFilesystemFile *file;
} CameraFilesystemFolder;

struct _CameraFilesystem {
	int count;
	CameraFilesystemFolder *folder;

	CameraFilesystemInfoFunc get_info_func;
	CameraFilesystemInfoFunc set_info_func;
	void *info_data;

	CameraFilesystemFileListFunc file_list_func;
	CameraFilesystemFolderListFunc folder_list_func;
	void *list_data;
};

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_MEM(m)         {if (!(m)) return (GP_ERROR_NO_MEMORY);}

static int gp_filesystem_append_folder (CameraFilesystem *, const char *);

int
gp_filesystem_new (CameraFilesystem **fs)
{
	int result;

	CHECK_NULL (fs);

	CHECK_MEM (*fs = malloc (sizeof (CameraFilesystem)));

        (*fs)->folder = NULL;
        (*fs)->count = 0;

	(*fs)->set_info_func = NULL;
	(*fs)->get_info_func = NULL;
	(*fs)->info_data = NULL;

	(*fs)->file_list_func = NULL;
	(*fs)->folder_list_func = NULL;
	(*fs)->list_data = NULL;

	result = gp_filesystem_append_folder (*fs, "/");
	if (result != GP_OK) {
		free (*fs);
		return (result);
	}

        return (GP_OK);
}

int
gp_filesystem_free (CameraFilesystem *fs)
{
        gp_filesystem_format (fs);

	/* Now, we've only got left over the root folder. Free that and
	 * the filesystem. */
	free (fs->folder);
	free (fs);

        return (GP_OK);
}

static int
gp_filesystem_folder_number (CameraFilesystem *fs, const char *folder)
{
	int x, y, len;
	char buf[128];
	CameraList list;

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

	/* Ok, we didn't find the folder. Do we have a parent? */
	if (!strcmp (folder, "/"))
		return (GP_ERROR_DIRECTORY_NOT_FOUND);

	/* If the parent folder is not dirty, return. */
	strncpy (buf, folder, len);
	for (y = strlen (buf) - 1; y >= 0; y--)
		if (buf[y] == '/')
			break;
	if (y)
		buf[y] = '\0';
	else
		buf[y + 1] = '\0'; /* Parent is root */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, buf));
	if (!fs->folder[x].folders_dirty)
		return (GP_ERROR_DIRECTORY_NOT_FOUND);

	/*
	 * The parent folder is dirty. List the folders in the parent 
	 * folder to make it clean.
	 */
	CHECK_RESULT (gp_filesystem_list_folders (fs, buf, &list));

	return (gp_filesystem_folder_number (fs, folder));
}

static int
gp_filesystem_append_folder (CameraFilesystem *fs, const char *folder)
{
	CameraFilesystemFolder *new;
	int x;
	char buf[128];

	CHECK_NULL (fs && folder);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Appending folder '%s'...",
			 folder);

	/* Make sure the directory doesn't exist */
	x = gp_filesystem_folder_number (fs, folder);
	if (x >= 0)
		return (GP_ERROR_DIRECTORY_EXISTS);
	else if (x != GP_ERROR_DIRECTORY_NOT_FOUND)
		return (x);

	/* Make sure the parent exist. If not, create it. */
	strcpy (buf, folder);
	for (x = strlen (buf) - 1; x >= 0; x--)
		if (buf[x] == '/')
			break;
	if (x > 0) {
		buf[x] = '\0';
		gp_debug_printf (GP_DEBUG_HIGH, "core", "Making sure that "
				 "parent '%s' exists...", buf);
		x = gp_filesystem_folder_number (fs, buf);
		if (x == GP_ERROR_DIRECTORY_NOT_FOUND)
			CHECK_RESULT (gp_filesystem_append_folder (fs, buf))
		else if (x < 0)
			return (x);
	}

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
	fs->folder[fs->count - 1].files_dirty = 1;
	fs->folder[fs->count - 1].folders_dirty = 1;

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Added folder '%s'...", folder);

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
	fs->folder[x].file[fs->folder[x].count - 1].info_dirty = 1;

	/*
	 * If people manually add files, they probably know the contents of
	 * this folder.
	 */
	fs->folder[x].files_dirty = 0;
	fs->folder[x].folders_dirty = 0;

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

static int
gp_filesystem_delete_all_files (CameraFilesystem *fs, const char *folder)
{
	int x, count;
	CameraList list;
	const char *name;

	CHECK_NULL (fs && folder);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Deleting all files in "
			 "'%s'...", folder);

	CHECK_RESULT (gp_filesystem_list_files (fs, folder, &list));
	CHECK_RESULT (count = gp_list_count (&list));
	for (x = 0; x < count; x++) {
		CHECK_RESULT (gp_list_get_name (&list, x, &name));
		CHECK_RESULT (gp_filesystem_delete (fs, folder, name));
	}
	fs->folder[x].files_dirty = 0;

	return (GP_OK);
}

static int
gp_filesystem_delete_all_folders (CameraFilesystem *fs, const char *folder)
{
	int x, count;
	CameraList list;
	const char *name;
	char buf[128];

	CHECK_NULL (fs && folder);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Deleting all folders in "
			 "'%s'...", folder);

	CHECK_RESULT (gp_filesystem_list_folders (fs, folder, &list));
	CHECK_RESULT (count = gp_list_count (&list));
	for (x = 0; x < count; x++) {
		CHECK_RESULT (gp_list_get_name (&list, x, &name));
		strcpy (buf, folder);
		if (strcmp (folder, "/"))
			strcat (buf, "/");
		strcat (buf, name);
		CHECK_RESULT (gp_filesystem_delete (fs, buf, NULL));
	}
	fs->folder[x].folders_dirty = 0;

	return (GP_OK);
}

int
gp_filesystem_list_files (CameraFilesystem *fs, const char *folder, 
		          CameraList *list)
{
	int x, y, count;
	const char *name;

	CHECK_NULL (fs && list && folder);

	list->count = 0;

	/* Search the folder */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	/* If the folder is dirty, delete the contents and query the camera */
	if (fs->folder[x].files_dirty && fs->file_list_func) {
		/*
		 * Declare the older clean so that we don't get called again
		 * and delete all files in it.
		 */
		fs->folder[x].files_dirty = 0; 
		CHECK_RESULT (gp_filesystem_delete_all_files (fs, folder));
		CHECK_RESULT (fs->file_list_func (fs, folder, list,
						  fs->list_data));
		CHECK_RESULT (count = gp_list_count (list));
		for (y = 0; y < count; y++) {
			CHECK_RESULT (gp_list_get_name (list, y, &name));
			CHECK_RESULT (gp_filesystem_append (fs, folder, name));
		}
		CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));
		list->count = 0;
	}

	for (y = 0; y < fs->folder[x].count; y++)
		CHECK_RESULT (gp_list_append (list,
					      fs->folder[x].file[y].name,
					      NULL));

	/* The folder is clean now */
	fs->folder[x].files_dirty = 0;

	return (GP_OK);
}

int
gp_filesystem_list_folders (CameraFilesystem *fs, const char *folder,
			    CameraList *list)
{
	int x, y, j, offset, count;
	char buf[128];
	const char *name;

	CHECK_NULL (fs && folder && list);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Listing folders in '%s'...",
			 folder);

	list->count = 0;

	/* Search the folder */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	/* If the folder is dirty, query the contents. */
	if (fs->folder[x].folders_dirty && fs->folder_list_func) {
		/*
		 * Declare the folder clean so that we don't get called again
		 * and delete all folders in it.
		 */
		fs->folder[x].folders_dirty = 0;
		CHECK_RESULT (gp_filesystem_delete_all_folders (fs, folder));
		CHECK_RESULT (fs->folder_list_func (fs, folder, list,
						    fs->list_data));
		CHECK_RESULT (count = gp_list_count (list));
		for (y = 0; y < count; y++) {
			CHECK_RESULT (gp_list_get_name (list, y, &name));
			strcpy (buf, folder);
			if (strlen (folder) != 1)
				strcat (buf, "/");
			strcat (buf, name);
			CHECK_RESULT (gp_filesystem_append_folder (fs, buf));
		}
		list->count = 0;
	}

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

	/* The folder is clean now */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));
	fs->folder[x].folders_dirty = 0;

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
		/* If this is root, just ignore the request */
		if (!strcmp (folder, "/"))
			return (GP_OK);

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
	CHECK_NULL (fs && folder);

	gp_debug_printf (GP_DEBUG_HIGH, "core",
			 "Deleting everything in '%s'...", folder);

	/* Delete subfolders and files */
	CHECK_RESULT (gp_filesystem_delete_all_folders (fs, folder));
	CHECK_RESULT (gp_filesystem_delete_all_files (fs, folder));

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

	/* 
	 * Append the root folder so that list_[folders,files] will stop
	 * there when recursively looking up parent folders.
	 */
	CHECK_RESULT (gp_filesystem_append_folder (fs, "/"));

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

int
gp_filesystem_set_list_funcs (CameraFilesystem *fs,
			      CameraFilesystemFileListFunc file_list_func,
			      CameraFilesystemFolderListFunc folder_list_func,
			      void *data)
{
	CHECK_NULL (fs);

	fs->file_list_func = file_list_func;
	fs->folder_list_func = folder_list_func;
	fs->list_data = data;

	return (GP_OK);
}

int
gp_filesystem_set_info_funcs (CameraFilesystem *fs,
			      CameraFilesystemInfoFunc get_info_func,
			      CameraFilesystemInfoFunc set_info_func,
			      void *data)
{
	CHECK_NULL (fs);

	fs->get_info_func = get_info_func;
	fs->set_info_func = set_info_func;
	fs->info_data = data;

	return (GP_OK);
}

int
gp_filesystem_get_info (CameraFilesystem *fs, const char *folder,
			const char *filename, CameraFileInfo *info)
{
	int x, y;

	CHECK_NULL (fs && folder && filename && info);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Getting info for '%s' in "
			 "folder '%s'...", filename, folder);

	if (!fs->get_info_func)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Search folder and file and get info if needed */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));
	CHECK_RESULT (y = gp_filesystem_number (fs, folder, filename));
	if (fs->folder[x].file[y].info_dirty) {
		CHECK_RESULT (fs->get_info_func (fs, folder, filename, 
						&fs->folder[x].file[y].info,
						fs->info_data));
		fs->folder[x].file[y].info_dirty = 0;
	}

	memcpy (info, &fs->folder[x].file[y].info, sizeof (CameraFileInfo));

	return (GP_OK);
}

int
gp_filesystem_set_info (CameraFilesystem *fs, const char *folder,
			const char *filename, CameraFileInfo *info)
{
	int x, y;

	CHECK_NULL (fs && folder && filename && info);

	if (!fs->set_info_func)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Search folder and file and set info */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));
	CHECK_RESULT (y = gp_filesystem_number (fs, folder, filename)); 
	CHECK_RESULT (fs->set_info_func (fs, folder, filename, info,
					 fs->info_data));

	memcpy (&fs->folder[x].file[y].info, info, sizeof (CameraFileInfo));
	fs->folder[x].file[y].info_dirty = 0;

	return (GP_OK);
}
