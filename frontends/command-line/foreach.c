#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main.h"
#include "actions.h"
#include "globals.h"
#include "range.h"
#include "foreach.h"

int
for_each_subfolder (char *folder, folder_action faction, 
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

	res = gp_camera_folder_list_folders (glob_camera, prefix, &folderlist);

	/* We don't print any information here when faction is
	 * for_each_image(). We let for_each_image() do the output
	 * in this case.
	 */
	if (gp_list_count(&folderlist) && (faction != for_each_image)) {
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
	for (i = 0; i < gp_list_count(&folderlist); i++) {
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

		if (recurse) 
			for_each_subfolder (subfolder, faction, iaction, 
					    recurse);
	}

	return (GP_OK);
}

int
for_each_image (char *folder, image_action iaction, int reverse)
{
	CameraList filelist;
	int i, res;
	const char *name;

	res = gp_camera_folder_list_files (glob_camera, folder, &filelist);
	if (res != GP_OK)
		return (res);

	if (glob_quiet)
		printf ("%i\n", gp_list_count (&filelist));
	else
		printf ("Files in %s:\n", folder);

	if (reverse) {
		for (i = gp_list_count (&filelist) - 1; 0 <= i; i--) {
			gp_list_get_name (&filelist, i, &name);
			res = iaction (folder, (char*)name);
			if (res != GP_OK)
				return (res);
		}
	} else {
		for (i = 0; i < gp_list_count (&filelist); i++) {
			gp_list_get_name (&filelist, i, &name);
			res = iaction (folder, (char*)name);
			if (res != GP_OK)
				return (res);
		}
	}

	return (GP_OK);
}


int
for_each_image_in_range (char *folder, char *range, image_action action,
			 int reverse)
{
	char	index[MAX_IMAGE_NUMBER];
	int 	i, max = 0;
	int	res;
	const char *name;
	CameraList 	filelist;

	memset(index, 0, MAX_IMAGE_NUMBER);
	
	res = parse_range (range, index);
	if (res != GP_OK) {
		cli_error_print("Invalid range");
		return (res);
	}

	res = gp_camera_folder_list_files (glob_camera, folder, &filelist);
	if (res != GP_OK)
		return (res);

	for (max = MAX_IMAGE_NUMBER - 1; !index[max]; max--) {}
	
	if ((res = gp_list_count(&filelist)) < 0)
		return (res);
		
	if (res < max + 1) {
		cli_error_print ("Picture number %i is too large. "
				 "Available %i picture(s).", max + 1, res);
		return (GP_ERROR_FILE_NOT_FOUND);
	}
	
	if (reverse) {
		for (i = max; 0 <= i; i--)
			if (index[i]) {
				gp_list_get_name (&filelist, i, &name);
				res = action (folder, (char*)name);
				if (res != GP_OK)
					return (res);
			}
	} else 
		for (i = 0; i <= max; i++)
			if (index[i]) {
				gp_list_get_name (&filelist, i, &name);
				res = action (folder, (char*)name);
				if (res != GP_OK)
					return (res);
			}
		
	return (GP_OK);
}
