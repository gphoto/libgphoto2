/* gphoto2-filesys.c
 *
 * Copyright (C) 2000 Scott Fritzinger
 *
 * Contributions:
 * 	Lutz Müller <urc8@rz.uni-karlsruhe.de> (2001)
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
#include "gphoto2-file.h"
#include "gphoto2-debug.h"

typedef struct {
	char name [128];
	int info_dirty;
	CameraFileInfo info;
	CameraFile *preview;
	CameraFile *normal;
	CameraFile *raw;
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

	CameraFilesystemListFunc file_list_func;
	CameraFilesystemListFunc folder_list_func;
	void *list_data;

	CameraFilesystemGetFileFunc get_file_func;
	CameraFilesystemDeleteFileFunc delete_file_func;
	void *file_data;

	CameraFilesystemPutFileFunc put_file_func;
	CameraFilesystemDeleteAllFunc delete_all_func;
	void *folder_data;
};

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_MEM(m)         {if (!(m)) return (GP_ERROR_NO_MEMORY);}
#define CHECK_ABS(folder)    {if ((folder)[0] != '/') return (GP_ERROR_PATH_NOT_ABSOLUTE);}

static int
delete_all_files (CameraFilesystem *fs, int x)
{
        int y;

        CHECK_NULL (fs);

        if (fs->folder[x].count) {

		/* Get rid of cached files */
                for (y = 0; y < fs->folder[x].count; y++) {
                        gp_file_unref (fs->folder[x].file[y].preview);
                        gp_file_unref (fs->folder[x].file[y].normal);
                        gp_file_unref (fs->folder[x].file[y].raw);
                }

                free (fs->folder[x].file);
                fs->folder[x].file = NULL;
                fs->folder[x].count = 0;
        }

        return (GP_OK);
}

static int
delete_folder (CameraFilesystem *fs, int x)
{
        CameraFilesystemFolder *new_fop;

        CHECK_NULL (fs);

        delete_all_files (fs, x);

        /* Move all folders behind one position up */
        if (x < fs->count - 1)
                memmove (&fs->folder[x], &fs->folder[x + 1],
                         sizeof (CameraFilesystemFolder) * (fs->count - x - 1));
        fs->count--;

        /* Get rid of the last one */
        new_fop = realloc (fs->folder,
                           sizeof (CameraFilesystemFolder) * (fs->count));
        if (!fs->count || (fs->count && new_fop))
                fs->folder = new_fop;

        return (GP_OK);
}

static int
delete_all_folders (CameraFilesystem *fs, const char *folder)
{
        int x;

	gp_debug_printf (GP_DEBUG_HIGH, "filesys", "Internally deleting "
			 "all folders from '%s'...", folder);

        CHECK_NULL (fs && folder);
        CHECK_ABS (folder);

        for (x = 0; x < fs->count; x++)
                if (!strncmp (fs->folder[x].name, folder, strlen (folder))) {

                        /*
                         * Is this really a subfolder (and not the folder
                         * itself)?
                         */
                        if (strlen (fs->folder[x].name) <= strlen (folder))
                                continue;

                        CHECK_RESULT (delete_all_files (fs, x));
                        CHECK_RESULT (delete_folder (fs, x));
                        x--;
                }

        return (GP_OK);
}

static int
append_folder (CameraFilesystem *fs, const char *folder)
{
        CameraFilesystemFolder *new;
        int x;
        char buf[128];

	gp_debug_printf (GP_DEBUG_HIGH, "filesys",
			 "Internally appending folder %s...", folder);

        CHECK_NULL (fs && folder);
        CHECK_ABS (folder);

        /* Make sure the directory doesn't exist */
	for (x = 0; x < fs->count; x++)
		if (!strncmp (fs->folder[x].name, folder, strlen (folder)) &&
		    (strlen (fs->folder[x].name) == strlen (folder)))
			break;
	if (x < fs->count) {
                gp_debug_printf (GP_DEBUG_HIGH, "core", "Could not append "
                                 "folder '%s' as this folder already exists.",
                                 folder);
                return (GP_ERROR_DIRECTORY_EXISTS);
        }

        /* Make sure the parent exist. If not, create it. */
        strcpy (buf, folder);
        for (x = strlen (buf) - 1; x >= 0; x--)
                if (buf[x] == '/')
                        break;
        if (x > 0) {
                buf[x] = '\0';
		for (x = 0; x < fs->count; x++)
			if (!strncmp (fs->folder[x].name, buf, strlen (buf)))
				break;
		if (x == fs->count)
			CHECK_RESULT (append_folder (fs, buf))
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

        return (GP_OK);
}

static int
append_file (CameraFilesystem *fs, int x, CameraFile *file)
{
	CameraFilesystemFile *new;
	const char *name;

	CHECK_NULL (fs && file);

	CHECK_RESULT (gp_file_get_name (file, &name));

	if (!fs->folder[x].count)
		CHECK_MEM (new = malloc (sizeof (CameraFilesystemFile)))
	else
		CHECK_MEM (new = realloc (fs->folder[x].file,
					sizeof (CameraFilesystemFile) *
						(fs->folder[x].count + 1)));
	fs->folder[x].file = new;
	fs->folder[x].count++;
	strcpy (fs->folder[x].file[fs->folder[x].count - 1].name, name);
	fs->folder[x].file[fs->folder[x].count - 1].info_dirty = 1;
	fs->folder[x].file[fs->folder[x].count - 1].raw = NULL;
	fs->folder[x].file[fs->folder[x].count - 1].preview = NULL;
	fs->folder[x].file[fs->folder[x].count - 1].normal = file;
	gp_file_ref (file);

	return (GP_OK);
}

/**
 * gp_filesystem_reset:
 * @fs: a #CameraFilesystem
 *
 * Resets the filesystem. All cached information including the folder tree
 * will get lost and will be queried again on demand. 
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_reset (CameraFilesystem *fs)
{
	CHECK_RESULT (delete_all_folders (fs, "/"));
	CHECK_RESULT (delete_all_files (fs, 0));

	fs->folder[0].folders_dirty = 1;
	fs->folder[0].files_dirty = 1;

	return (GP_OK);
}

/**
 * gp_filesystem_new:
 * @fs: a pointer to a #CameraFilesystem
 *
 * Creates a new empty #CameraFilesystem
 *
 * Return value: a gphoto2 error code.
 **/
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

	(*fs)->get_file_func = NULL;
	(*fs)->file_data = NULL;

	result = append_folder (*fs, "/");
	if (result != GP_OK) {
		free (*fs);
		return (result);
	}

        return (GP_OK);
}

/**
 * gp_filesystem_free:
 * @fs: a #CameraFilesystem
 *
 * Frees the #CameraFilesystem
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_free (CameraFilesystem *fs)
{
	CHECK_RESULT (gp_filesystem_reset (fs));

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

	gp_debug_printf (GP_DEBUG_HIGH, "filesys",
			 "Looking for folder '%s'...", folder);

	CHECK_NULL (fs && folder);
	CHECK_ABS (folder);

	/*
	 * We are nice to front-end/camera-driver writers - we'll ignore
	 * trailing slashes (if any).
	 */
	len = strlen (folder);
	if ((len > 1) && (folder[len - 1] == '/'))
		len--;

	for (x = 0; x < fs->count; x++)
		if (!strncmp (fs->folder[x].name, folder, len) &&
		    (len == strlen (fs->folder[x].name)))
			return (x);

	/* Ok, we didn't find the folder. Do we have a parent? */
	if (!strcmp (folder, "/")) {
		gp_debug_printf (GP_DEBUG_HIGH, "core", "Could not find "
				 "folder '%s'.", folder);
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	}

	/* If the parent folder is not dirty, return. */
	strncpy (buf, folder, len);
	buf[len] = '\0';
	for (y = strlen (buf) - 1; y >= 0; y--)
		if (buf[y] == '/')
			break;
	if (y)
		buf[y] = '\0';
	else
		buf[y + 1] = '\0'; /* Parent is root */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, buf));
	if (!fs->folder[x].folders_dirty) {
		gp_debug_printf (GP_DEBUG_HIGH, "core", "Folder '%s' is up "
				 "to date but does not contain a folder "
				 "'%s'.", buf, folder);
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	}

	/*
	 * The parent folder is dirty. List the folders in the parent 
	 * folder to make it clean.
	 */
	gp_debug_printf (GP_DEBUG_HIGH, "filesys", "Folder %s is dirty. "
			 "Listing file in there to make folder clean...",
			 buf);
	CHECK_RESULT (gp_filesystem_list_folders (fs, buf, &list));

	return (gp_filesystem_folder_number (fs, folder));
}

/**
 * gp_filesystem_append:
 * @fs: a #CameraFilesystem
 * @folder: the folder where to put the file in
 * @filename: filename of the file
 *
 * Tells the @fs that there is a file called @filename in folder 
 * called @folder. Usually, camera drivers will call this function after
 * capturing an image in order to tell the @fs about the new file.
 * A front-end should not use this function.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_append (CameraFilesystem *fs, const char *folder, 
		      const char *filename) 
{
	CameraFilesystemFile *new;
        int x, y;

	CHECK_NULL (fs && folder);
	CHECK_ABS (folder);

	/* Check for existence */
	x = gp_filesystem_folder_number (fs, folder);
	if (x == GP_ERROR_DIRECTORY_NOT_FOUND)
		CHECK_RESULT (append_folder (fs, folder))
	else if (x < 0) {
		gp_debug_printf (GP_DEBUG_HIGH, "core", "Could not append "
				 "'%s' to folder '%s'.", filename, folder);
		return (x);
	}
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	if (!filename)
		return (GP_OK);

	/* If file exists, return error */
	for (y = 0; y < fs->folder[x].count; y++)
		if (!strncmp (fs->folder[x].file[y].name, filename,
			     strlen (filename)) && (
		    (strlen (filename) == strlen (fs->folder[x].file[y].name))))
			break;
	if (y < fs->folder[x].count) {
		gp_debug_printf (GP_DEBUG_HIGH, "core", "Could not append "
				 "'%s' to folder '%s'. That file already "
				 "exists.", filename, folder);
		return (GP_ERROR_FILE_EXISTS);
	}

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
	fs->folder[x].file[fs->folder[x].count - 1].preview = NULL;
	fs->folder[x].file[fs->folder[x].count - 1].normal = NULL;
	fs->folder[x].file[fs->folder[x].count - 1].raw = NULL;

	/*
	 * If people manually add files, they probably know the contents of
	 * this folder.
	 */
	fs->folder[x].files_dirty = 0;

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
delete_file (CameraFilesystem *fs, int x, int y)
{
	CameraFilesystemFile *new_fip;

	/* Get rid of cached files */
	gp_file_unref (fs->folder[x].file[y].preview);
	gp_file_unref (fs->folder[x].file[y].normal);
	gp_file_unref (fs->folder[x].file[y].raw);

	/* Move all files behind one position up */
	if (y < fs->folder[x].count - 1)
		memmove (&fs->folder[x].file[y], &fs->folder[x].file[y + 1],
			 sizeof (CameraFilesystemFile) *
			 	(fs->folder[x].count - y - 1));
	fs->folder[x].count--;

	/* Get rid of the last one */
	new_fip = realloc (fs->folder[x].file,
		sizeof (CameraFilesystemFile) * (fs->folder[x].count));
	if (!fs->folder[x].count || (fs->folder[x].count && new_fip))
		fs->folder[x].file = new_fip;

	return (GP_OK);
}

static int
gp_filesystem_delete_all_one_by_one (CameraFilesystem *fs, const char *folder)
{
	CameraList list;
	int count, x;
	const char *name;

	CHECK_RESULT (gp_filesystem_list_files (fs, folder, &list));
	CHECK_RESULT (count = gp_list_count (&list));
	for (x = count - 1; x >= 0; x--) {
		CHECK_RESULT (gp_list_get_name (&list, x, &name));
		CHECK_RESULT (gp_filesystem_delete_file (fs, folder, name));
	}

	return (GP_OK);
}

/**
 * gp_filesystem_delete_all
 * @fs: a #CameraFilesystem
 * @folder: the folder in which to delete all files
 *
 * Deletes all files in the given @folder from the @fs. If the @fs has not
 * been supplied with a delete_all_func, it tries to delete the files
 * one by one using the delete_file_func. If that function has not been
 * supplied neither, an error is returned.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_delete_all (CameraFilesystem *fs, const char *folder)
{
	int x;
//	CameraList list;

	CHECK_NULL (fs && folder);
	CHECK_ABS (folder);

	/* Make sure this folder exists */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	if (!fs->delete_all_func)
		CHECK_RESULT (gp_filesystem_delete_all_one_by_one (fs, folder))
	else {
		CHECK_RESULT (fs->delete_all_func (fs, folder,fs->folder_data));
		CHECK_RESULT (delete_all_files (fs, x));

//FIXME: Some cameras don't delete all pictures?
//		fs->folder[x].files_dirty = 1;
//		CHECK_RESULT (gp_filesystem_list_files (fs, folder, &list));
//		if (gp_list_count (&list) > 0)
//			CHECK_RESULT (gp_filesystem_delete_all_one_by_one (fs,
//								folder));
	}

	fs->folder[x].files_dirty = 0;
	return (GP_OK);
}

/**
 * gp_filesystem_list_files:
 * @fs: a #CameraFilesystem
 * @folder: a folder of which a file list should be generated
 * @list: a #CameraList where to put the list of files into
 *
 * Lists the files in @folder using either cached values or (if there
 * aren't any) the file_list_func which (hopefully) has been previously
 * supplied.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_list_files (CameraFilesystem *fs, const char *folder, 
		          CameraList *list)
{
	int x, y, count;
	const char *name;

	CHECK_NULL (fs && list && folder);
	CHECK_ABS (folder);

	list->count = 0;

	/* Search the folder */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	/* If the folder is dirty, delete the contents and query the camera */
	if (fs->folder[x].files_dirty && fs->file_list_func) {

		gp_debug_printf (GP_DEBUG_HIGH, "filesys",
				 "Querying folder %s...", folder);
		CHECK_RESULT (fs->file_list_func (fs, folder, list,
						  fs->list_data));
		CHECK_RESULT (delete_all_files (fs, x));

		gp_debug_printf (GP_DEBUG_HIGH, "filesys",
				 "Adding the files:");
		CHECK_RESULT (count = gp_list_count (list));
		for (y = 0; y < count; y++) {
			CHECK_RESULT (gp_list_get_name (list, y, &name));
			gp_debug_printf (GP_DEBUG_HIGH, "filesys",
					 " - %s", name);
			CHECK_RESULT (gp_filesystem_append (fs, folder, name));
		}
		list->count = 0;
	}

	/* The folder is clean now */
	fs->folder[x].files_dirty = 0;

	gp_debug_printf (GP_DEBUG_HIGH, "filesys", "Folder %s:", folder);
	for (y = 0; y < fs->folder[x].count; y++) {
		gp_debug_printf (GP_DEBUG_HIGH, "filesys",
				 "  - %s", fs->folder[x].file[y].name);
		CHECK_RESULT (gp_list_append (list,
					      fs->folder[x].file[y].name,
					      NULL));
	}

	return (GP_OK);
}

/**
 * gp_filesystem_list_folders:
 * @fs: a #CameraFilesystem
 * @folder: a folder
 * @list: a #CameraList where subfolders should be listed
 *
 * Generates a list of subfolders of the supplied @folder either using 
 * cached values (if there are any) or the folder_list_func if it has been 
 * supplied previously. If not, it is assumed that only a root folder 
 * exists (which is the case for many cameras).
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_list_folders (CameraFilesystem *fs, const char *folder,
			    CameraList *list)
{
	int x, y, j, offset, count;
	char buf[128];
	const char *name;

	gp_debug_printf (GP_DEBUG_HIGH, "filesys",
			 "Listing folders in '%s'...", folder);

	CHECK_NULL (fs && folder && list);
	CHECK_ABS (folder);

	list->count = 0;

	/* Search the folder */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	/* If the folder is dirty, query the contents. */
	if (fs->folder[x].folders_dirty && fs->folder_list_func) {
		CHECK_RESULT (fs->folder_list_func (fs, folder, list,
						    fs->list_data));
		CHECK_RESULT (delete_all_folders (fs, folder));
		CHECK_RESULT (count = gp_list_count (list));
		for (y = 0; y < count; y++) {
			CHECK_RESULT (gp_list_get_name (list, y, &name));
			strcpy (buf, folder);
			if (strlen (folder) != 1)
				strcat (buf, "/");
			strcat (buf, name);
			CHECK_RESULT (append_folder (fs, buf));
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

	gp_debug_printf (GP_DEBUG_HIGH, "filesys", "Folder %s contains %i "
			 "files.", folder, fs->folder[x].count);

	return (GP_OK);
}

/**
 * gp_filesystem_count:
 * @fs: a #CameraFilesystem
 * @folder: a folder in which to count the files
 *
 * Counts the files in the @folder.
 *
 * Return value: The number of files in the @folder or a gphoto2 error code.
 **/
int
gp_filesystem_count (CameraFilesystem *fs, const char *folder)
{
        int x;

	CHECK_NULL (fs && folder);
	CHECK_ABS (folder);

	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	return (fs->folder[x].count);
}

/**
 * gp_filesystem_delete_file:
 * @fs: a #CameraFilesystem
 * @folder: a folder in which to delete the file
 * @filename: the name of the file to delete
 *
 * If a delete_file_func has been supplied to the @fs, this function will
 * be called and, if this function returns without error, the file will be 
 * removed from the @fs.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_delete_file (CameraFilesystem *fs, const char *folder, 
			   const char *filename)
{
        int x, y;

	CHECK_NULL (fs && folder && filename);
	CHECK_ABS (folder);

	/* First of all, do we support file deletion? */
	if (!fs->delete_file_func)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Search the folder and the file */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));
	CHECK_RESULT (y = gp_filesystem_number (fs, folder, filename));

	/* Delete the file */
	CHECK_RESULT (fs->delete_file_func (fs, folder, filename,
					    fs->file_data));
	CHECK_RESULT (delete_file (fs, x, y));

	return (GP_OK);
}

/**
 * gp_filesystem_put_file:
 * @fs: a #CameraFilesystem
 * @folder: the folder where to put the @file into
 * @file: the file
 *
 * Uploads a file to the camera if a put_file_func has been previously 
 * supplied to the @fs. If the upload is successful, the file will get
 * cached in the @fs.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_put_file (CameraFilesystem *fs, const char *folder,
			CameraFile *file)
{
	const char *name;
	int x;

	CHECK_NULL (fs && folder && file);

	/* Do we support file upload? */
	if (!fs->put_file_func)
		return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT (gp_file_get_name (file, &name));

	/* Search the folder */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	/* Upload the file */
	CHECK_RESULT (fs->put_file_func (fs, folder, file, fs->folder_data));
	CHECK_RESULT (append_file (fs, x, file));

	return (GP_OK);
}

/**
 * gp_filesystem_name:
 * @fs: a #CameraFilesystem
 * @folder: the folder where to look up the file with the @filenumber
 * @filenumber: the number of the file
 * @filename:
 *
 * Looks up the @filename of file with given @filenumber in given @folder.
 * See gp_filesystem_number for exactly the opposite functionality.
 * 
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_name (CameraFilesystem *fs, const char *folder, int filenumber,
		    const char **filename)
{
        int x;

	CHECK_NULL (fs && folder);
	CHECK_ABS (folder);

	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));
	
	if (filenumber > fs->folder[x].count)
		return (GP_ERROR_FILE_NOT_FOUND);
	
	*filename = fs->folder[x].file[filenumber].name;
	return (GP_OK);
}

/**
 * gp_filesystem_number:
 * @fs: a #CameraFilesystem
 * @folder: the folder where to look for file called @filename
 * @filename: the file to look for
 *
 * Looks for a file called @filename in the given @folder. See
 * gp_filesystem_name for exactly the opposite functionality.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_number (CameraFilesystem *fs, const char *folder, 
		      const char *filename)
{
	CameraList list;
        int x, y;

	CHECK_NULL (fs && folder && filename);
	CHECK_ABS (folder);

	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));

	for (y = 0; y < fs->folder[x].count; y++)
		if (!strcmp (fs->folder[x].file[y].name, filename))
			return (y);

	/* Ok, we didn't find the file. Is the folder dirty? */
	if (!fs->folder[x].files_dirty)
		return (GP_ERROR_FILE_NOT_FOUND);

	/* The folder is dirty. List all files to make it clean */
	CHECK_RESULT (gp_filesystem_list_files (fs, folder, &list));

        return (gp_filesystem_number (fs, folder, filename));
}

static int
gp_filesystem_scan (CameraFilesystem *fs, const char *folder,
		    const char *filename)
{
	int count, x;
	CameraList list;
	const char *name;
	char path[128];

	gp_debug_printf (GP_DEBUG_HIGH, "filesys", "Scanning %s for %s...",
			 folder, filename);
	CHECK_NULL (fs && folder && filename);
	CHECK_ABS (folder);

	CHECK_RESULT (gp_filesystem_list_files (fs, folder, &list));
	CHECK_RESULT (count = gp_list_count (&list));
	for (x = 0; x < count; x++) {
		CHECK_RESULT (gp_list_get_name (&list, x, &name));
		if (filename && !strcmp (filename, name))
			return (GP_OK);
	}

	CHECK_RESULT (gp_filesystem_list_folders (fs, folder, &list));
	CHECK_RESULT (count = gp_list_count (&list));
	for (x = 0; x < count; x++) {
		CHECK_RESULT (gp_list_get_name (&list, x, &name));
		strcpy (path, folder);
		if (strcmp (path, "/"))
			strcat (path, "/");
		strcat (path, name);
		CHECK_RESULT (gp_filesystem_scan (fs, path, filename));
	}

	return (GP_OK);
}

/**
 * gp_filesystem_get_folder:
 * @fs: a #CameraFilesystem
 * @filename: the name of the file to search in the @fs
 * @folder:
 *
 * Searches a file called @filename in the @fs and returns the first 
 * occurrency. This functionality is needed for camera drivers that cannot
 * figure out where a file gets created after capturing an image although the
 * name of the image is known. Usually, those drivers will call
 * gp_filesystem_reset in order to tell the @fs that something has 
 * changed and then gp_filesystem_get_folder in order to find the file.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_get_folder (CameraFilesystem *fs, const char *filename, 
			  const char **folder)
{
	int x, y;

	CHECK_NULL (fs && filename && folder);

	CHECK_RESULT (gp_filesystem_scan (fs, "/", filename));

	for (x = 0; x < fs->count; x++)
		for (y = 0; y < fs->folder[x].count; y++)
			if (!strcmp (fs->folder[x].file[y].name, filename)) {
				*folder = fs->folder[x].name;
				return (GP_OK);
			}

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Could not find file '%s'.",
			 filename);
	return (GP_ERROR_FILE_NOT_FOUND);
}

/**
 * gp_filesystem_set_list_funcs:
 * @fs: a #CameraFilesystem
 * @file_list_func: the function that will return listings of files
 * @folder_list_func: the function that will return listings of folders
 * @data:
 *
 * Tells the @fs which functions to use to retreive listings of folders 
 * and/or files. Typically, a camera driver would call this function
 * on initialization. Each function can be NULL indicating that this 
 * functionality is not supported. For example, many cameras don't support
 * folders. In this case, you would supply NULL for folder_list_func. Then,
 * the @fs assumes that there is only a root folder.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_set_list_funcs (CameraFilesystem *fs,
			      CameraFilesystemListFunc file_list_func,
			      CameraFilesystemListFunc folder_list_func,
			      void *data)
{
	CHECK_NULL (fs);

	fs->file_list_func = file_list_func;
	fs->folder_list_func = folder_list_func;
	fs->list_data = data;

	return (GP_OK);
}

/**
 * gp_filesystem_set_file_funcs:
 * @fs: a #CameraFilesystem
 * @get_file_func: the function downloading files
 * @del_file_func: the function deleting files
 * @data:
 *
 * Tells the @fs which functions to use for file download or file deletion.
 * Typically, a camera driver would call this function on initialization.
 * A function can be NULL indicating that this functionality is not supported.
 * For example, if a camera does not support file deletion, you would supply 
 * NULL for del_file_func.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_set_file_funcs (CameraFilesystem *fs,
			      CameraFilesystemGetFileFunc get_file_func,
			      CameraFilesystemDeleteFileFunc del_file_func,
			      void *data)
{
	CHECK_NULL (fs);

	fs->delete_file_func = del_file_func;
	fs->get_file_func = get_file_func;
	fs->file_data = data;

	return (GP_OK);
}

/**
 * gp_filesystem_set_folder_funcs:
 * @fs: a #CameraFilesystem
 * @put_file_func: function used to upload files
 * @delete_all_func: function used to delete all files in a folder
 * @data:
 *
 * Tells the filesystem which functions to call for file upload or deletion
 * of all files in a given folder. Typically, a camera driver would call
 * this function on initialization. If one functionality is not 
 * supported, NULL can be supplied. If you don't call this function, the @fs
 * will assume that neither upload nor deletion of all files in a folder
 * at once is supported. The @fs will try to compensate latter with the
 * delete_file_func if such a function has been supplied.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_set_folder_funcs (CameraFilesystem *fs,
				CameraFilesystemPutFileFunc put_file_func,
				CameraFilesystemDeleteAllFunc del_all_func,
				void *data)
{
	CHECK_NULL (fs);

	fs->put_file_func = put_file_func;
	fs->delete_all_func = del_all_func;
	fs->folder_data = data;

	return (GP_OK);
}

/**
 * gp_filesystem_get_file:
 * @fs: a #CameraFilesystem
 * @folder: the folder in which the file can be found
 * @filename: the name of the file to download
 * @type: the type of the file
 * @file:
 *
 * Downloads the file called @filename from the @folder using the 
 * get_file_func if such a function has been previously supplied. If the 
 * file has been previously downloaded, the file is retreived from cache.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_get_file (CameraFilesystem *fs, const char *folder,
			const char *filename, CameraFileType type,
			CameraFile *file)
{
	int x, y, result;
	CameraFile *tmp;

	CHECK_NULL (fs && folder && file && filename);
	CHECK_ABS (folder);

	if (!fs->get_file_func) {
		gp_debug_printf (GP_DEBUG_HIGH, "core", "No get_file_func "
				 "available.");
		return (GP_ERROR_NOT_SUPPORTED);
	}

	/* Search folder and file */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));
	CHECK_RESULT (y = gp_filesystem_number (fs, folder, filename));

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		if (fs->folder[x].file[y].preview)
			return (gp_file_copy (file,
					fs->folder[x].file[y].preview));
		break;
	case GP_FILE_TYPE_NORMAL:
		if (fs->folder[x].file[y].normal)
			return (gp_file_copy (file,
					fs->folder[x].file[y].normal));
		break;
	case GP_FILE_TYPE_RAW:
		if (fs->folder[x].file[y].raw)
			return (gp_file_copy (file,
					fs->folder[x].file[y].raw));
		break;
	default:
		gp_debug_printf (GP_DEBUG_HIGH, "core", "Unknown file type.");
		return (GP_ERROR);
	}

	CHECK_RESULT (gp_file_new (&tmp));
	result = fs->get_file_func (fs, folder, filename, type, tmp,
				    fs->file_data);
	if (result != GP_OK) {
		gp_file_unref (tmp);
		return (result);
	}

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		fs->folder[x].file[y].preview = tmp;
		break;
	case GP_FILE_TYPE_NORMAL:
		fs->folder[x].file[y].normal = tmp;
		break;
	case GP_FILE_TYPE_RAW:
		fs->folder[x].file[y].raw = tmp;
		break;
	default:
		gp_file_unref (tmp);
		gp_debug_printf (GP_DEBUG_HIGH, "core", "Unknown file type.");
		return (GP_ERROR);
	}

	CHECK_RESULT (gp_file_set_type (tmp, type));
	CHECK_RESULT (gp_file_set_name (tmp, filename)); 
	CHECK_RESULT (gp_file_copy (file, tmp));

	return (GP_OK);
}

/**
 * gp_filesystem_set_info-funcs:
 * @fs: a #CameraFilesystem
 * @get_info_func: the function to retreive file information
 * @set_info_func: the function to set file information
 * @data:
 *
 * Tells the filesystem which functions to call when file information 
 * about a file should be retreived or set. Typically, this function will
 * get called by the camera driver on initialization. 
 *
 * Return value: a gphoto2 error code.
 **/
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

/**
 * gp_filesystem_get_info:
 * @fs: a #CameraFilesystem
 * @folder:
 * @filename:
 * @info:
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_get_info (CameraFilesystem *fs, const char *folder,
			const char *filename, CameraFileInfo *info)
{
	int x, y;

	CHECK_NULL (fs && folder && filename && info);
	CHECK_ABS (folder);

	if (!fs->get_info_func) {
		gp_debug_printf (GP_DEBUG_HIGH, "core", "No get_info_func "
				 "available.");
		return (GP_ERROR_NOT_SUPPORTED);
	}

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

/**
 * gp_filesystem_set_info:
 * @fs: a #CameraFilesystem
 * @folder:
 * @filename:
 * @info:
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_filesystem_set_info (CameraFilesystem *fs, const char *folder,
			const char *filename, CameraFileInfo *info)
{
	int x, y;

	CHECK_NULL (fs && folder && filename && info);
	CHECK_ABS (folder);

	if (!fs->set_info_func) {
		gp_debug_printf (GP_DEBUG_HIGH, "core", "No set_info_func "
				 "available.");
		return (GP_ERROR_NOT_SUPPORTED);
	}

	/* Search folder and file and set info */
	CHECK_RESULT (x = gp_filesystem_folder_number (fs, folder));
	CHECK_RESULT (y = gp_filesystem_number (fs, folder, filename)); 
	CHECK_RESULT (fs->set_info_func (fs, folder, filename, info,
					 fs->info_data));

	memcpy (&fs->folder[x].file[y].info, info, sizeof (CameraFileInfo));
	fs->folder[x].file[y].info_dirty = 0;

	return (GP_OK);
}
