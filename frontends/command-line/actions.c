#include <stdio.h>

#include "main.h"
#include "actions.h"
#include "globals.h"

/* Folder actions 						*/
/* ------------------------------------------------------------ */

int print_folder(char *subfolder, image_action action, int reverse) {
	/* print paths relative to glob_folder */
	char *c;
	
	c = subfolder + strlen(glob_folder);
	if (*c == '/')
		c++;

	printf("\"%s\"\n", c);
		
	return (GP_OK);
}

int print_files(char *subfolder, image_action iaction, int reverse) {

	CameraList filelist;
	CameraListEntry *entry;
	int x;
	char buf[64];
	gp_camera_file_list(glob_camera, &filelist, subfolder);
	if (glob_quiet)
		printf("%i\n", gp_list_count(&filelist));
	   else
		printf("Files in %s:\n", subfolder);
	for (x=0; x<gp_list_count(&filelist); x++) {
		entry = gp_list_entry(&filelist, x);
		if (glob_quiet)
			printf("\"%s\"\n", entry->name);
		   else {
			sprintf(buf, "%i", x+1);
			printf("#%-5s %s\n", buf, entry->name);
		}
	}
	return (GP_OK);
}

/* File actions 						*/
/* ------------------------------------------------------------ */

int save_picture_action(char *folder, char *filename) {
		
	if (save_picture_to_file(folder, filename, 0) != GP_OK)
		return (GP_ERROR);		
	return (GP_OK);
}

int save_thumbnail_action(char *folder, char *filename) {
	
	if (save_picture_to_file(folder, filename, 1) != GP_OK)
		return (GP_ERROR);
	return (GP_OK);
}

int delete_picture_action(char *folder, char *filename) {
	
	if (gp_camera_file_delete(glob_camera, folder, filename) != GP_OK)
		return (GP_ERROR);		
	return (GP_OK);
}
