#include <string.h>
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
	int x, res;

	res = gp_camera_folder_list_files (glob_camera, subfolder, &filelist);
	if (res != GP_OK)
		return (res);

	if (glob_quiet)
		printf ("%i\n", gp_list_count (&filelist));
	   else
		printf ("Files in %s:\n", subfolder);

	for (x = 0; x < gp_list_count (&filelist); x++) {
		entry = gp_list_entry (&filelist, x);
		if (glob_quiet)
			printf ("\"%s\"\n", entry->name);
                else {
			CameraFileInfo info;
			if (gp_camera_file_get_info(glob_camera, subfolder, entry->name, &info) == GP_OK) {
			    printf("#%-5i %-27s", x+1, entry->name);
			    if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
			    	printf("%s%s",
			    		(info.file.permissions & GP_FILE_PERM_READ) ? "r" : "-",
			    		(info.file.permissions & GP_FILE_PERM_DELETE) ? "d" : "-");
			    }
			    if (info.file.fields & GP_FILE_INFO_SIZE)
			    	printf(" %5d KB", (info.file.size+1023) / 1024);
			    if ((info.file.fields & GP_FILE_INFO_WIDTH) && +			    	(info.file.fields & GP_FILE_INFO_HEIGHT))
			    	printf(" %4dx%-4d", info.file.width, info.file.height);
			    if (info.file.fields & GP_FILE_INFO_TYPE)
			    	printf(" %s", info.file.type);
				printf("\n");
			} else {
			    printf("#%-5i %s\n", x+1, entry->name);
			}
                }
	}
	return (GP_OK);
}

/* File actions 						*/
/* ------------------------------------------------------------ */

int save_picture_action(char *folder, char *filename) {

	return (save_picture_to_file(folder, filename, 0));
}

int save_thumbnail_action(char *folder, char *filename) {
	
	return (save_picture_to_file(folder, filename, 1));
}

int delete_picture_action(char *folder, char *filename) {

	return (gp_camera_file_delete(glob_camera, folder, filename));
}
