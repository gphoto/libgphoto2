/* Notes:
	* uses the setting value of "dir_directory" to open as the directory
	  (interfaces will want to set this when they choose to "open" a 
	  directory.)
	* adding filetypes: search for "jpg", copy line and change extension
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gphoto2.h>

#include "directory.h"

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
	abilities[0].port     = GP_PORT_NONE;
	abilities[0].speed[0] = 0;

	abilities[0].capture   = 0;
	abilities[0].config    = 1;
	abilities[0].file_delete  = 0;
	abilities[0].file_preview = 1;
	abilities[0].file_put  = 0;

	return (GP_OK);
}

int camera_init (Camera *camera, CameraInit *init) {

	int i=0;
	DirectoryStruct *d;

	/* First, set up all the function pointers */
	camera->functions->id 		= camera_id;
	camera->functions->abilities 	= camera_abilities;
	camera->functions->init 	= camera_init;
	camera->functions->exit 	= camera_exit;
	camera->functions->folder_list  = camera_folder_list;
	camera->functions->folder_set 	= camera_folder_set;
	camera->functions->file_count 	= camera_file_count;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->file_put 	= camera_file_put;
	camera->functions->file_delete 	= camera_file_delete;
	camera->functions->config_get   = camera_config_get;
	camera->functions->config_set   = camera_config_set;
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	d = (DirectoryStruct*)malloc(sizeof(DirectoryStruct));
	camera->camlib_data = d;

	d->num_images = 0;

	d->debug = init->debug;

	for (i=0; i<1024; i++)
		strcpy(d->images[i], "");

	strcpy(d->directory, init->port_settings.path);
	if (strlen(d->directory)==0) {
		strcpy(d->directory, "/");
	}

	return (GP_OK);
}

int camera_exit (Camera *camera) {

	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	free(d);

	return (GP_OK);
}

int camera_folder_list(Camera *camera, char *folder_name, CameraFolderInfo *list) {

	DIR *d;
	struct dirent *de;
	struct stat st;
	char *ch;
	char buf[1024];
	int count=0;

	if ((d = opendir(folder_name))==NULL)
		return (GP_ERROR);

	while (de = readdir(d)) {
		if ((strcmp(de->d_name, ".") !=0) &&
		    (strcmp(de->d_name, "..")!=0)) {
			sprintf(buf, "%s/%s", folder_name, de->d_name);
			if (stat(buf, &st)==0) {
				if (S_ISDIR(st.st_mode)) {
					strcpy(list[count++].name, de->d_name);
				}
			}
		}
	}

	return (count);
}

int folder_index(Camera *camera) {

	DIR *dir;
	struct dirent *de;
	struct stat s;
	char *dot, fname[1024];
	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	d->num_images = 0;

	dir = opendir(d->directory);
	if (!dir)
		return (GP_ERROR);
	de = readdir(dir);
	while (de) {
		sprintf(fname, "%s/%s", d->directory, de->d_name);
		stat(fname, &s);
                /* If it's a file ...*/
                if (S_ISREG(s.st_mode)) {
			dot = strrchr(de->d_name, '.');
			if (dot) {
			   if (
			    (strcasecmp(dot, ".gif")==0)||
			    (strcasecmp(dot, ".tif")==0)||
			    (strcasecmp(dot, ".tiff")==0)||
			    (strcasecmp(dot, ".jpg")==0)||
			    (strcasecmp(dot, ".xpm")==0)||
			    (strcasecmp(dot, ".png")==0)||
			    (strcasecmp(dot, ".pbm")==0)||
			    (strcasecmp(dot, ".pgm")==0)||
			    (strcasecmp(dot, ".jpeg")==0)) {
				strcpy(d->images[d->num_images++],
					de->d_name);
				if (d->debug)
					printf("directory: found \"%s\"\n", de->d_name);
			   }
			}
		}
		de = readdir(dir);
	}
	closedir(dir);

	return (GP_OK);
}

int camera_folder_set(Camera *camera, char *folder_name) {

	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	strcpy(d->directory, folder_name);

	return (folder_index(camera));
}

int camera_file_count (Camera *camera) {

	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	return (d->num_images);
}

int camera_file_get (Camera *camera, CameraFile *file, int file_number) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/

	char filename[1024];
	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	sprintf(filename, "%s/%s", d->directory, d->images[file_number]);
	if (gp_file_open(file, filename)==GP_ERROR)
		return (GP_ERROR);

	return (GP_OK);
}

int camera_file_get_preview (Camera *camera, CameraFile *file, int file_number) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/

	char filename[1024];
	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	sprintf(filename, "%s/%s", d->directory, d->images[file_number]);
	if (gp_file_open(file, filename)==GP_ERROR)
		return (GP_ERROR);

	return (GP_OK);
}

int camera_file_put (Camera *camera, CameraFile *file) {

	return (GP_ERROR);
}


int camera_file_delete (Camera *camera, int file_number) {

	return (GP_ERROR);
}

int camera_config_get (Camera *camera, CameraWidget *window) {

	CameraWidget *t, *section;
	int num;
	char buf[1024];

	debug_print("Building configuration window");

	/* Create a new section for "Quality" */
	section = gp_widget_new(GP_WIDGET_SECTION, "Quality");
	gp_widget_append(window, section);

		/* Add the Resolution setting radio buttons */
		t = gp_widget_new(GP_WIDGET_RADIO, "Resolution");
		gp_widget_append(section, t);
		gp_widget_choice_add(t, "Normal", 0);
		gp_widget_choice_add(t, "High", 0);
		gp_widget_choice_add(t, "Best", 1);

		t = gp_widget_new(GP_WIDGET_TEXT, "Camera Name");
		gp_widget_append(section, t);
		gp_widget_text_set(t, "hey there!");

		t = gp_widget_new(GP_WIDGET_RANGE, "LCD Brightness");
		gp_widget_append(section, t);
		gp_widget_range_set(t, 1, 7, 1, 4);
		

	/* Create a new section for "Flash/Lens" and append to window */
	section = gp_widget_new(GP_WIDGET_SECTION, "Flash/Lens");
	gp_widget_append(window, section);

		t = gp_widget_new(GP_WIDGET_MENU, "Flash Setting");
		gp_widget_append(section, t);
		gp_widget_choice_add(t, "Auto", 0);
		gp_widget_choice_add(t, "Red-eye", 0);
		gp_widget_choice_add(t, "Force", 1);
		gp_widget_choice_add(t, "None", 0);

		t = gp_widget_new(GP_WIDGET_RADIO, "Lens Mode");
		gp_widget_append(section, t);
		gp_widget_choice_add(t, "Normal", 0);
		gp_widget_choice_add(t, "Macro", 1);

	return (GP_OK);
}

int camera_config_set (Camera *camera, CameraSetting *setting, int count) {

	int x;

	printf("Directory library got the following config values:\n");
	for (x=0; x<count; x++)
		printf("\"%s\" = \"%s\"\n", setting[x].name, setting[x].value);

	return (GP_OK);
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	return (GP_ERROR);
}

int camera_summary (Camera *camera, CameraText *summary) {

	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	sprintf(summary->text, "Current directory:\n%s", d->directory);

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) {

	strcpy(manual->text, 
"The Directory Browse \"camera\" lets you index
photos on your hard drive. The folder list on the
left contains the folders on your hard drive,
beginning at the root directory (\"/\").
");

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

	strcpy(about->text,
"Directory Browse Mode
Scott Fritzinger <scottf@unr.edu>");

	return (GP_OK);
}
