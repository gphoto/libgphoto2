#include <string.h>
#include <stdio.h>

#include "main.h"
#include "actions.h"
#include "globals.h"

/* Folder actions 						*/
/* ------------------------------------------------------------ */

int
print_folder (const char *subfolder, image_action action, int reverse)
{
	char *c;
	
	/* remove the basename for clarity purposes */
	c = strrchr(subfolder, '/');
	if (*c == '/')
		c++;
	printf("\"%s\"\n", c);
		
	return (GP_OK);
}

int
delete_folder_files (const char *subfolder, image_action action, int reverse)
{
	cli_debug_print("Deleting all files in '%s'", subfolder);
	
	return gp_camera_folder_delete_all (glob_camera, subfolder, glob_context);
}

/* File actions 						*/
/* ------------------------------------------------------------ */

int
print_picture_action (const char *folder, const char *filename)
{
	static int x=0;

	if (glob_quiet)
		printf ("\"%s\"\n", filename);
	else {
		CameraFileInfo info;
		if (gp_camera_file_get_info(glob_camera, folder, filename, &info, NULL) == GP_OK) {
		    printf("#%-5i %-27s", x+1, filename);
		    if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
			printf("%s%s",
				(info.file.permissions & GP_FILE_PERM_READ) ? "r" : "-",
				(info.file.permissions & GP_FILE_PERM_DELETE) ? "d" : "-");
		    }
		    if (info.file.fields & GP_FILE_INFO_SIZE)
			printf(" %5d KB", (info.file.size+1023) / 1024);
		    if ((info.file.fields & GP_FILE_INFO_WIDTH) && +
			    (info.file.fields & GP_FILE_INFO_HEIGHT))
			printf(" %4dx%-4d", info.file.width, info.file.height);
		    if (info.file.fields & GP_FILE_INFO_TYPE)
			printf(" %s", info.file.type);
			printf("\n");
		} else {
		    printf("#%-5i %s\n", x+1, filename);
		}
	}
	x++;
	return (GP_OK);
}

int
save_picture_action (const char *folder, const char *filename)
{
	return (save_picture_to_file(folder, filename, GP_FILE_TYPE_NORMAL));
}

int
save_exif_action (const char *folder, const char *filename)
{
	return (save_picture_to_file (folder, filename, GP_FILE_TYPE_EXIF));
}

int
save_thumbnail_action (const char *folder, const char *filename)
{
	return (save_picture_to_file (folder, filename, GP_FILE_TYPE_PREVIEW));
}

int
save_raw_action (const char *folder, const char *filename)
{
	return (save_picture_to_file (folder, filename, GP_FILE_TYPE_RAW));
}

int
save_audio_action (const char *folder, const char *filename)
{
	return (save_picture_to_file (folder, filename, GP_FILE_TYPE_AUDIO));
}

int delete_picture_action (const char *folder, const char *filename)
{
	return (gp_camera_file_delete(glob_camera, folder, filename, glob_context));
}
