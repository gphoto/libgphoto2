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
for_each_subfolder (const char *folder, folder_action faction, 
		    image_action iaction, int recurse)
{
	CameraList folderlist;

	char	prefix[1024], subfolder[1024];
	int	i, l, res;

	prefix[0] = 0;
	subfolder[0] = 0;

	if (folder != NULL && *folder != '\0') 
		strcat(prefix, folder);
	else
		strcat(prefix, glob_folder);

	res = gp_camera_folder_list_folders (glob_camera, prefix, &folderlist,
					     glob_context);

	/* We don't print any information here when faction is
	 * for_each_image(). We let for_each_image() do the output
	 * in this case.
	 */
	if (gp_list_count (&folderlist) && (faction != for_each_image)) {
		if (glob_quiet)
			printf ("%i\n", gp_list_count (&folderlist));
		else
			printf ("Folders in %s:\n", prefix);
	}

	if (res != GP_OK)
		return (res);

	/* This loop is splitted is two parts. The first one
	 * only applies faction() and the second one do the
	 * recursion. This way, we obtain a cleaner display
	 * when file names are displayed. 
	 */
	for (i = 0; i < gp_list_count (&folderlist); i++) {
		const char *name;

		gp_list_get_name (&folderlist, i, &name);
		while (*prefix && prefix[l=strlen(prefix)-1] == '/')
			prefix[l] = '\0';
		sprintf(subfolder, "%s/%s", prefix, name);

		res = faction (subfolder, iaction, 0);
		if (res != GP_OK)
			return (res);
	}

	for (i = 0; i < gp_list_count(&folderlist); i++) {
		const char *name;

		gp_list_get_name (&folderlist, i, &name);
		while (*prefix && prefix[l=strlen(prefix)-1] == '/')
			prefix[l] = '\0';
		sprintf(subfolder, "%s/%s", prefix, name);

		if (recurse) {
			res = for_each_subfolder (subfolder, faction, iaction, 
					    recurse);
			if (res != GP_OK)
				return (res);
		}
	}

	return (GP_OK);
}

int
for_each_image (const char *folder, image_action iaction, int reverse)
{
	CameraList filelist;
	int i;
	const char *name;

	CR (gp_camera_folder_list_files (glob_camera, folder, &filelist,
					 glob_context));

	if (glob_quiet)
		printf ("%i\n", gp_list_count (&filelist));
	else
		printf ("Files in %s:\n", folder);

	if (reverse) {
		for (i = gp_list_count (&filelist) - 1; 0 <= i; i--) {
			gp_list_get_name (&filelist, i, &name);
			CR (iaction (folder, name));
		}
	} else {
		for (i = 0; i < gp_list_count (&filelist); i++) {
			gp_list_get_name (&filelist, i, &name);
			CR (iaction (folder, name));
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
get_path_for_id (const char *base_folder, unsigned char recurse,
		 unsigned int id, char *folder, char *filename)
{
	int r;
	unsigned int base_id;
	CameraList list;
	const char *name;

	strncpy (folder, base_folder, MAX_FOLDER_LEN);
	if (!recurse) {

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
					_("Bad image number. "
					"You specified %i, but there is only "
					"1 file available in '%s'."), id + 1,
					base_folder);
				return (GP_ERROR_BAD_PARAMETERS);
			default:
				gp_context_error (glob_context,
					_("Bad image number. "
					"You specified %i, but there are only "
					"%i files available in '%s'."
					"Please obtain a valid image number "
					"from a file listing first."), id + 1,
					gp_list_count (&list), base_folder);
				return (GP_ERROR_BAD_PARAMETERS);
			}
		}
		CR (gp_list_get_name (&list, id, &name));
		strncpy (filename, name, MAX_FILE_LEN);
		return (GP_OK);
	} else {
		base_id = 0;
		r = get_path_for_id_rec (base_folder, id, &base_id, folder,
					 filename);
		switch (r) {
		case GP_ERROR_FRONTEND_BAD_ID:
			gp_context_error (glob_context, _("Bad image number. "
				"You specified %i, but there are only %i "
				"files available in '%s' or its subfolders. "
				"Please obtain a valid image number from "
				"a file listing first."), id + 1, base_id,
				base_folder);
			return (GP_ERROR_BAD_PARAMETERS);
		default:
			return (r);
		}
	}
}

int
for_each_image_in_range (const char *folder, unsigned char recurse,
			 char *range, image_action action, int reverse)
{
	char	index[MAX_IMAGE_NUMBER];
	int 	i, max = 0;
	char ffolder[MAX_FOLDER_LEN], ffile[MAX_FILE_LEN];

	memset(index, 0, MAX_IMAGE_NUMBER);
	
	CR (parse_range (range, index, glob_context));

	for (max = MAX_IMAGE_NUMBER - 1; !index[max]; max--);
	
	if (reverse) {
		for (i = max; 0 <= i; i--)
			if (index[i]) {
				CR (get_path_for_id (folder, recurse,
					(unsigned int) i, ffolder, ffile));
				CR (action (ffolder, ffile));
			}
	} else 
		for (i = 0; i <= max; i++)
			if (index[i]) {
				GP_DEBUG ("Now processing ID %i...", i);
				CR (get_path_for_id (folder, recurse,
					(unsigned int) i, ffolder, ffile));
				CR (action (ffolder, ffile));
			}
		
	return (GP_OK);
}
