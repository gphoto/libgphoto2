/* frontend.c
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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
#include <config.h>
#include "foreach.h"

#include <string.h>
#include <stdio.h>

#include <gphoto2-port-log.h>

#include "main.h"
#include "actions.h"
#include "globals.h"
#include "range.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define GP_ERROR_FRONTEND_BAD_ID -10000
#define CR(result) {int r=(result); if(r<0) return(r);}

#define GP_MODULE "frontend"

int
for_each_folder (const char *folder, FolderAction action, ForEachFlags flags)
{
	CameraList list;
	int	i, count;
	const char *name;
	char path[1024];

	CR (action (folder));

	/* If recursion is requested, descend into subfolders. */
	if (flags & FOR_EACH_FLAGS_RECURSE) {
		CR (gp_camera_folder_list_folders (glob_camera, folder, &list,
						   glob_context));
		CR (count = gp_list_count (&list));

		if (flags & FOR_EACH_FLAGS_REVERSE) {
			for (i = count - 1; i >= 0; i++) {
				CR (gp_list_get_name (&list, i, &name));
				strncpy (path, folder, sizeof (path));
				if (path[strlen (path) - 1] != '/')
					strncat (path, "/", sizeof (path));
				strncat (path, name, sizeof (path));
				CR (action (path));
			}
		} else {
			for (i = 0; i < count; i++) {
				CR (gp_list_get_name (&list, i, &name));
				strncpy (path, folder, sizeof (path));
				if (path[strlen (path) - 1] != '/')
					strncat (path, "/", sizeof (path));
				strncat (path, name, sizeof (path));
				CR (action (path));
			}
		}
	}

	return (GP_OK);
}

int
for_each_file (const char *folder, FileAction action, ForEachFlags flags)
{
	CameraList list;
	int i, count;
	const char *name;
	char path[1024];

	/* Iterate on all files */
	CR (gp_camera_folder_list_files (glob_camera, folder, &list,
					 glob_context));
	CR (count = gp_list_count (&list));
	if (flags & FOR_EACH_FLAGS_REVERSE) {
		for (i = count - 1; 0 <= i; i--) {
			CR (gp_list_get_name (&list, i, &name));
			CR (action (folder, name));
		}
	} else {
		for (i = 0; i < count; i++) {
			CR (gp_list_get_name (&list, i, &name));
			CR (action (folder, name));
		}
	}

	/* If recursion is requested, descend into subfolders. */
	if (flags & FOR_EACH_FLAGS_RECURSE) {
		CR (gp_camera_folder_list_folders (glob_camera, folder,
							&list, glob_context));
		CR (count = gp_list_count (&list));
		for (i = 0; i < count; i++) {
			CR (gp_list_get_name (&list, i, &name));
			strncpy (path, folder, sizeof (path));
			if (path[strlen (path) - 1] != '/')
				strncat (path, "/", sizeof (path));
			strncat (path, name, sizeof (path));
			CR (for_each_file (path, action, flags));
		}
	}

	return (GP_OK);
}

#define MAX_FOLDER_LEN 1024
#define MAX_FILE_LEN   1024

static int
get_path_for_id_rec (const char *base_folder, unsigned int id,
		     unsigned int *base_id, char *folder,
		     char *filename)
{
	char subfolder[1024];
	int n_folders, n_files, r;
	unsigned int i;
	const char *name;
	CameraList list;

	strncpy (folder, base_folder, MAX_FOLDER_LEN);
	CR (gp_camera_folder_list_files (glob_camera, base_folder, &list,
					 glob_context));
	CR (n_files = gp_list_count (&list));
	if (id - *base_id < n_files) {

		/* ID is in this folder */
		GP_DEBUG ("ID %i is in folder '%s'.", id, base_folder);
		CR (gp_list_get_name (&list, id - *base_id, &name));
		strncpy (filename, name, MAX_FILE_LEN);
		return (GP_OK);
	} else {

		/* Look for IDs in subfolders */
		GP_DEBUG ("ID %i is not in folder '%s'.", id, base_folder);
		*base_id += n_files;
		CR (gp_camera_folder_list_folders (glob_camera, base_folder,
						   &list, glob_context));
		CR (n_folders = gp_list_count (&list));
		for (i = 0; i < n_folders; i++) {
			CR (gp_list_get_name (&list, i, &name));
			strncpy (subfolder, base_folder, sizeof (subfolder));
			if (strlen (base_folder) > 1)
				strncat (subfolder, "/", sizeof (subfolder));
			strncat (subfolder, name, sizeof (subfolder));
			r = get_path_for_id_rec (subfolder, id, base_id,
						 folder, filename);
			switch (r) {
			case GP_ERROR_FRONTEND_BAD_ID:
				break;
			default:
				return (r);
			}
		}
		return (GP_ERROR_FRONTEND_BAD_ID);
	}
}

static int
get_path_for_id (const char *base_folder, ForEachFlags flags,
		 unsigned int id, char *folder, char *filename)
{
	int r;
	unsigned int base_id;
	CameraList list;
	const char *name;

	strncpy (folder, base_folder, MAX_FOLDER_LEN);
	if (flags & FOR_EACH_FLAGS_RECURSE) {
                base_id = 0;
                r = get_path_for_id_rec (base_folder, id, &base_id, folder,
                                         filename);
                switch (r) {
                case GP_ERROR_FRONTEND_BAD_ID:
                        gp_context_error (glob_context, _("Bad file number. "
                                "You specified %i, but there are only %i "
                                "files available in '%s' or its subfolders. "
                                "Please obtain a valid file number from "
                                "a file listing first."), id + 1, base_id,
                                base_folder);
                        return (GP_ERROR_BAD_PARAMETERS);
                default:
                        return (r);
                }
	} else {

		/* If we have no recursion, things are easy. */
		GP_DEBUG ("No recursion. Taking file %i from folder '%s'.",
			  id, base_folder);
		CR (gp_camera_folder_list_files (glob_camera, base_folder,
						 &list, glob_context));
		if (id >= gp_list_count (&list)) {
			switch (gp_list_count (&list)) {
			case 0:
				gp_context_error (glob_context,
					_("There are no files in "
					"folder '%s'."), base_folder);
				return (GP_ERROR_BAD_PARAMETERS);
			case 1:
				gp_context_error (glob_context, 
					_("Bad file number. "
					"You specified %i, but there is only "
					"1 file available in '%s'."), id + 1,
					base_folder);
				return (GP_ERROR_BAD_PARAMETERS);
			default:
				gp_context_error (glob_context,
					_("Bad file number. "
					"You specified %i, but there are only "
					"%i files available in '%s'."
					"Please obtain a valid file number "
					"from a file listing first."), id + 1,
					gp_list_count (&list), base_folder);
				return (GP_ERROR_BAD_PARAMETERS);
			}
		}
		CR (gp_list_get_name (&list, id, &name));
		strncpy (filename, name, MAX_FILE_LEN);
		return (GP_OK);
	}
}

int
for_each_file_in_range (const char *folder, FileAction action,
			 ForEachFlags flags, char *range)
{
	char	index[MAX_IMAGE_NUMBER];
	int 	i, max = 0;
	char ffolder[MAX_FOLDER_LEN], ffile[MAX_FILE_LEN];

	memset(index, 0, MAX_IMAGE_NUMBER);
	
	CR (parse_range (range, index, glob_context));

	for (max = MAX_IMAGE_NUMBER - 1; !index[max]; max--);
	
	if (flags & FOR_EACH_FLAGS_REVERSE) {
		for (i = max; 0 <= i; i--)
			if (index[i]) {
				CR (get_path_for_id (folder, flags,
					(unsigned int) i, ffolder, ffile));
				CR (action (ffolder, ffile));
			}
	} else 
		for (i = 0; i <= max; i++)
			if (index[i]) {
				GP_DEBUG ("Now processing ID %i...", i);
				CR (get_path_for_id (folder, flags,
					(unsigned int) i, ffolder, ffile));
				CR (action (ffolder, ffile));
			}
		
	return (GP_OK);
}
