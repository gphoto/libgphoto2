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
	CameraList		folderlist;
	CameraListEntry		*entry;

	char	prefix[1024], subfolder[1024];
	int	i, res;
	
	prefix[0] = 0;
	subfolder[0] = 0;

	if (folder != NULL && *folder != '\0') 
		strcat(prefix, folder);
	else
		strcat(prefix, glob_folder);

	res = gp_camera_folder_list_folders (glob_camera, prefix, &folderlist);
	if (res != GP_OK)
		return (res);

	if ((glob_quiet)&&(faction == print_folder))
		printf ("%i\n", gp_list_count(&folderlist));
	
	for (i = 0; i < gp_list_count(&folderlist); i++) {
		entry = gp_list_entry(&folderlist, i);
		sprintf(subfolder, "%s/%s", prefix, entry->name);

		res = faction (subfolder, iaction, recurse);
		if (res != GP_OK)
			return (res);

		if (recurse) 
			for_each_subfolder (subfolder, faction, iaction, 
					    recurse);
	}

	return (GP_OK);
}

int
for_each_image (char *folder, image_action iaction, int reverse)
{
	CameraList 	filelist;
	CameraListEntry *entry;
	int i, res;

	res = gp_camera_folder_list_files (glob_camera, folder, &filelist);
	if (res != GP_OK)
		return (res);

	if (reverse) {
		for (i = gp_list_count (&filelist) - 1; 0 <= i; i--) {
			entry = gp_list_entry (&filelist, i);
			res = iaction (folder, entry->name);
			if (res != GP_OK)
				return (res);
		}
	} else {
		for (i = 0; i < gp_list_count (&filelist); i++) {
			entry = gp_list_entry (&filelist, i);
			res = iaction (folder, entry->name);
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

	CameraList 	filelist;
	CameraListEntry *entry;

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
				entry = gp_list_entry (&filelist, i);
				res = action (folder, entry->name);
				if (res != GP_OK)
					return (res);
			}
	} else 
		for (i = 0; i <= max; i++)
			if (index[i]) {
				entry = gp_list_entry (&filelist, i);
				res = action (folder, entry->name);
				if (res != GP_OK)
					return (res);
			}
		
	return (GP_OK);
}
