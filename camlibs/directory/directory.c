/* Notes:
	* only supports jpegs/gifs right now
	* uses the setting value of "dir_directory" to open as the directory
	  (interfaces will want to set this when they choose to "open" a 
	  directory.)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gphoto2.h>

char  dir_directory[1024];
char  dir_images[1024][1024];
int   dir_num_images;
int   dir_get_index;

int camera_id (char *id) {

        strcpy(id, "directory-scottf");

        return (GP_OK);
}

int camera_debug_set (int onoff) {

	return (GP_OK);
}

int camera_abilities (CameraAbilities *abilities, int *count) {

	*count = 1;

	strcpy(abilities[0].model, "Directory Browse");
	abilities[0].serial    = 0;
	abilities[0].usb       = 0;
	abilities[0].parallel  = 0;
	abilities[0].ieee1394  = 0;

	abilities[0].serial_baud[0] = 0;

	abilities[0].cancel    = 0;
	abilities[0].capture   = 0;
	abilities[0].config    = 1;
	abilities[0].file_delete  = 0;
	abilities[0].file_preview = 1;
	abilities[0].file_put  = 0;
	abilities[0].lock      = 0;

	return (GP_OK);
}

int camera_init (CameraInit *init) {

	int i=0;

	dir_num_images = 0;

	for (i=0; i<1024; i++)
		strcpy(dir_images[i], "");

	strcpy(dir_directory, init->port_settings.directory_path);
	if (strlen(dir_directory)==0) {
		strcpy(dir_directory, "/");
	}

	return (GP_OK);
}

int camera_exit () {

	return (GP_OK);
}

int camera_folder_list(char *folder_name, CameraFolderList *list) {

	return (GP_ERROR);
}

int camera_folder_set(char *folder_name) {

	DIR *dir;
	struct dirent *de;
	struct stat s;
	char *dot, fname[1024];

	strcpy(dir_directory, folder_name);
	dir = opendir(dir_directory);
	if (!dir) {
		perror("directory: folder_set");
		return (GP_ERROR);
	}
	de = readdir(dir);
	while (de) {
		gp_update_progress(-1);
		sprintf(fname, "%s/%s", dir_directory, de->d_name);
		stat(fname, &s);
                /* If it's a file ...*/
                if (S_ISREG(s.st_mode)) {
			dot = strrchr(de->d_name, '.');
			if (
			    (strcasecmp(dot, ".gif")==0)||
			    (strcasecmp(dot, ".jpg")==0)||
			    (strcasecmp(dot, ".jpeg")==0)) {
				strcpy(dir_images[dir_num_images++],
					de->d_name);
#ifdef DEBUG
				printf("directory: found \"%s\"\n", de->d_name);
#endif
			}
		}
		de = readdir(dir);
	}
	closedir(dir);

	return (GP_OK);
}

int camera_file_count () {

	return (dir_num_images);
}

int camera_file_get (int file_number, CameraFile *file) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/

	FILE *fp;
	long imagesize;
	char filename[1024];

	sprintf(filename, "%s%s", dir_directory,
		dir_images[file_number]);
	fp = fopen(filename, "r");
	fseek(fp, 0, SEEK_END);
	imagesize = ftell(fp);
	rewind(fp);
	file->data = (char *)malloc(sizeof(char)*imagesize);
	fread(file->data, (size_t)imagesize, (size_t)sizeof(char), fp);
	fclose(fp);

	strcpy(file->name, dir_images[file_number]);
	file->type = GP_FILE_JPEG;
	file->size = imagesize;
	return (GP_OK);
}

int camera_file_get_preview (int file_number, CameraFile *preview) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/

	char filename[1024];
	FILE *fp;
	long int imagesize;
	sprintf(filename, "%s/%s", dir_directory,
		dir_images[file_number]);	
	fp = fopen(filename, "r");
	fseek(fp, 0, SEEK_END);
	imagesize = ftell(fp);
	rewind(fp);
	preview->data = (char *)malloc(sizeof(char)*imagesize);
	fread(preview->data, (size_t)imagesize, (size_t)sizeof(char),fp);
	fclose(fp);
	preview->type = GP_FILE_JPEG;
	preview->size = imagesize;

//	gp_file_image_scale(preview, 80, 60, preview);

	return (GP_OK);
}

int camera_file_put (CameraFile *file) {

	return (GP_ERROR);
}


int camera_file_delete (int file_number) {

	return (GP_ERROR);
}

int camera_file_lock (int file_number) {

	return (GP_ERROR);
}

int camera_file_unlock (int file_number) {

	return (GP_ERROR);
}

int camera_config (CameraConfig *config) {

	return (GP_ERROR);
}

int camera_capture (int type) {

	return (GP_ERROR);
}

int camera_summary (char *summary) {

	strcpy(summary, "Summary Not Available");

	return (GP_OK);
}

int camera_manual (char *manual) {

	strcpy(manual, "Manual Not Available");

	return (GP_OK);
}

int camera_about (char *about) {

	strcpy(about,
"Directory Browse Mode
Scott Fritzinger <scottf@unr.edu>");

	return (GP_OK);
}
