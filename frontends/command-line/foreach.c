#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main.h"
#include "actions.h"
#include "globals.h"
#include "range.h"

int for_each_subfolder (char *folder, folder_action faction, 
			image_action iaction, int recurse) {
	
	CameraList		folderlist;
	CameraListEntry		*entry;

	char	prefix[1024], subfolder[1024];
	int	i;
	
	prefix[0] = 0;
	subfolder[0] = 0;

	if (folder != NULL && *folder != '\0') 
		strcat(prefix, folder);
	else
		strcat(prefix, glob_folder);

	gp_camera_folder_list(glob_camera, &folderlist, prefix);

	if ((glob_quiet)&&(faction == print_folder))
		printf ("%i\n", gp_list_count(&folderlist));
	
	for (i = 0; i < gp_list_count(&folderlist); i++) {
		entry = gp_list_entry(&folderlist, i);
		sprintf(subfolder, "%s/%s", prefix, entry->name);
		if (faction(subfolder, iaction, recurse) != GP_OK) 
			return (GP_ERROR);
		if (recurse) 
			for_each_subfolder(subfolder, faction, iaction, recurse);
	}

	return (GP_OK);
}

int for_each_image(char *folder, image_action iaction, int reverse) {
	
	CameraList 	filelist;
	CameraListEntry *entry;
	int		i;

	gp_camera_file_list(glob_camera, &filelist, folder);

	if (reverse) {
		for (i = gp_list_count(&filelist) - 1; 0 <= i; i--) {
			entry = gp_list_entry(&filelist, i);
			if (iaction(folder, entry->name) != GP_OK)
				return (GP_ERROR);
		}
	} else {
		for (i = 0; i < gp_list_count(&filelist); i++) {
			entry = gp_list_entry(&filelist, i);
			if (iaction(folder, entry->name) != GP_OK)
				return (GP_ERROR);
		}
	}

	return (GP_OK);
}


int for_each_image_in_range(char *folder, char *range, image_action action, int reverse) {
	
	char	index[MAX_IMAGE_NUMBER];
	int 	i, max = 0;

	CameraList 	filelist;
	CameraListEntry *entry;

	memset(index, 0, MAX_IMAGE_NUMBER);
	
	if (parse_range(range, index) != GP_OK) {
		cli_error_print("Invalid range");
		return (GP_ERROR);
	}

	if (gp_camera_file_list(glob_camera, &filelist, folder) != GP_OK)
		return (GP_ERROR);

	for (max = MAX_IMAGE_NUMBER - 1; !index[max]; max--) {}
	
	if (gp_list_count(&filelist) < max + 1) {
		cli_error_print("Picture number %i is too large. Available %i picture(s).", max + 1, gp_list_count(&filelist));
		return (GP_ERROR);
	}
	
	if (reverse) {
		for (i = max; 0 <= i; i--)
			if (index[i]) {
				entry = gp_list_entry(&filelist, i);
				if (action(folder, entry->name) != GP_OK)
					return (GP_ERROR);
			}
	} else 
		for (i = 0; i <= max; i++)
			if (index[i]) {
				entry = gp_list_entry(&filelist, i);
				if (action(folder, entry->name) != GP_OK)
					return (GP_ERROR);
			}
		
	return (GP_OK);
}
