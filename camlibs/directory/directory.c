/* Notes:
	* uses the setting value of "dir_directory" to open as the directory
	  (interfaces will want to set this when they choose to "open" a 
	  directory.)
	* adding filetypes: search for "jpg", copy line and change extension
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>
#include <gpio.h>

#include "directory.h"

char *extension[] = {
	"gif",
	"tif",
	"jpg",
	"xpm",
	"png",
	"pbm",
	"pgm",
	"jpeg",
	"tiff",
	NULL
};

int is_image (char *filename) {

	char *dot;
	int x=0;

	dot = strrchr(filename, '.');
	if (dot) {
		while (extension[x]) {
			if (strcasecmp(extension[x], dot+1)==0)
				return (1);
			x++;
		}
	}
	return (0);
}

int camera_id (CameraText *id) {

        strcpy(id->text, "directory");

        return (GP_OK);
}


int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities *a;

	a = gp_abilities_new();

	strcpy(a->model, "Directory Browse");
	a->port     = GP_PORT_NONE;
	a->speed[0] = 0;

	a->capture   = 0;
	a->config    = 1;
	a->file_delete  = 0;
	a->file_preview = 1;
	a->file_put  = 0;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

int camera_init (Camera *camera, CameraInit *init) {

	int i=0;
	DirectoryStruct *d;
	char buf[256];

	/* First, set up all the function pointers */
	camera->functions->id 		= camera_id;
	camera->functions->abilities 	= camera_abilities;
	camera->functions->init 	= camera_init;
	camera->functions->exit 	= camera_exit;
	camera->functions->folder_list  = camera_folder_list;
	camera->functions->file_list    = camera_file_list;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->file_put 	= camera_file_put;
	camera->functions->file_delete 	= camera_file_delete;
	camera->functions->config       = camera_config;
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	d = (DirectoryStruct*)malloc(sizeof(DirectoryStruct));
	camera->camlib_data = d;

	d->num_images = 0;

	for (i=0; i<1024; i++)
		strcpy(d->images[i], "");

	strcpy(d->directory, init->port.path);
	if (strlen(d->directory)==0) {
		strcpy(d->directory, "/");
	}

	if (gp_setting_get("directory", "hidden", buf) != GP_OK)
		gp_setting_set("directory", "hidden", "1");

	return (GP_OK);
}

int camera_exit (Camera *camera) {

	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	free(d);

	return (GP_OK);
}

int camera_file_list(Camera *camera, CameraList *list, char *folder) {

	GPIO_DIR d;
	GPIO_DIRENT de;
	char buf[1024], f[1024];

	if ((d = GPIO_OPENDIR(folder))==NULL)
		return (GP_ERROR);
	
	/* Make sure we have 1 delimiter */
	if (folder[strlen(folder)-1] != '/')
		sprintf(f, "%s%c", folder, '/');
	 else
		strcpy(f, folder);

	while ((de = GPIO_READDIR(d))) {
		if ((strcmp(GPIO_FILENAME(de), "." )!=0) &&
		    (strcmp(GPIO_FILENAME(de), "..")!=0)) {
			sprintf(buf, "%s%s", f, GPIO_FILENAME(de));
			if (GPIO_IS_FILE(buf) && (is_image(buf)))
				gp_list_append(list, GPIO_FILENAME(de), GP_LIST_FILE);
		}
	}

	return (GP_OK);
}

int camera_folder_list(Camera *camera, CameraList *list, char *folder) {

	GPIO_DIR d;
	GPIO_DIRENT de;
	char buf[1024], f[1024];
	char *dirname;
	int view_hidden=1;

	if (gp_setting_get("directory", "hidden", buf)==GP_OK)
		view_hidden = atoi(buf);
	if ((d = GPIO_OPENDIR(folder))==NULL)
		return (GP_ERROR);

	/* Make sure we have 1 delimiter */
	if (folder[strlen(folder)-1] != '/')
		sprintf(f, "%s%c", folder, '/');
	 else
		strcpy(f, folder);

	while ((de = GPIO_READDIR(d))) {
		if ((strcmp(GPIO_FILENAME(de), "." )!=0) &&
		    (strcmp(GPIO_FILENAME(de), "..")!=0)) {
			sprintf(buf, "%s%s", f, GPIO_FILENAME(de));
			dirname = GPIO_FILENAME(de);
			if (GPIO_IS_DIR(buf)) {
			   if (dirname[0] != '.')
				gp_list_append(list, GPIO_FILENAME(de), GP_LIST_FOLDER);
			     else
			       if (view_hidden)
				gp_list_append(list, GPIO_FILENAME(de), GP_LIST_FOLDER);
			}
		}
	}

	return (GP_OK);
}

int folder_index(Camera *camera) {

	GPIO_DIR dir;
	GPIO_DIRENT de;
	char fname[1024];
	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	d->num_images = 0;

	dir = GPIO_OPENDIR(d->directory);
	if (!dir)
		return (GP_ERROR);
	de = GPIO_READDIR(dir);
	while (de) {
		sprintf(fname, "%s/%s", d->directory, GPIO_FILENAME(de));
		if (GPIO_IS_FILE(fname)) {
			strcpy(d->images[d->num_images++], GPIO_FILENAME(de));
			if (camera->debug)
				printf("directory: found \"%s\"\n", GPIO_FILENAME(de));
		}
		de = GPIO_READDIR(dir);
	}
	GPIO_CLOSEDIR(dir);

	return (GP_OK);
}

int directory_folder_set(Camera *camera, char *folder_name) {

	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	strcpy(d->directory, folder_name);

	return (folder_index(camera));
}

int camera_file_get (Camera *camera, CameraFile *file, char *folder, char *filename) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/

	char buf[1024];
	int result;
	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	directory_folder_set(camera, folder);

	sprintf(buf, "%s/%s", d->directory, filename);
	if ((result = gp_file_open(file, buf)) != GP_OK)
		return (result);

	return (GP_OK);
}

int camera_file_get_preview (Camera *camera, CameraFile *file, char *folder, char *filename) {

	/**********************************/
	/* file_number now starts at 0!!! */
	/**********************************/

	char buf[1024];
	int result;
	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	directory_folder_set(camera, folder);

	sprintf(buf, "%s/%s", d->directory, filename);
	if ((result = gp_file_open(file, buf)) != GP_OK)
		return (result);

	return (GP_OK);
}

int camera_file_put (Camera *camera, CameraFile *file, char *folder) {

	return (GP_ERROR_NOT_SUPPORTED);
}


int camera_file_delete (Camera *camera, char *folder, char *filename) {

	return (GP_ERROR_NOT_SUPPORTED);
}

int camera_config (Camera *camera) {

	CameraWidget *window, *t;
	char buf[256];
	int val;

	window = gp_widget_new(GP_WIDGET_WINDOW, "Directory Configuration");

	t = gp_widget_new(GP_WIDGET_TOGGLE, "View hidden (dot) directories");
	gp_setting_get("directory", "hidden", buf);
	val = atoi (buf);
	gp_widget_value_set(t, &val);
	gp_widget_append(window, t);

        /* Prompt the user with the config window */
        if (gp_frontend_prompt (camera, window) == GP_PROMPT_CANCEL) {
                gp_widget_unref(window);
                return GP_OK;
        }
	t = gp_widget_child_by_label(window, "View hidden (dot) directories");
        if (gp_widget_changed(t)) {
                gp_widget_value_get (t, &val);
		sprintf(buf, "%i", val);
                gp_setting_set("directory", "hidden", buf);
        }

	return (GP_OK);
#if 0
	/* Example. shows when debugging is on */
//FIXME: Syntax of gp_widget_value_get/set has changed!

	/* Create a new section for "Quality" */
	section = gp_widget_new(GP_WIDGET_SECTION, "Quality");
	gp_widget_append(window, section);

		/* Add the Resolution setting radio buttons */
		t = gp_widget_new(GP_WIDGET_RADIO, "Resolution");
		gp_widget_append(section, t);
		gp_widget_choice_add(t, "Normal");
		gp_widget_choice_add(t, "High");
		gp_widget_choice_add(t, "Best");
		gp_widget_value_set(t, "High");

		t = gp_widget_new(GP_WIDGET_TEXT, "Camera Name");
		gp_widget_append(section, t);
		gp_widget_value_set(t, "hey there!");

		t = gp_widget_new(GP_WIDGET_RANGE, "LCD Brightness");
		gp_widget_append(section, t);
		gp_widget_range_set(t, 1.0, 7.0, 1.0);
		gp_widget_value_set(t, "4.0");

	/* Create a new section for "Flash/Lens" and append to window */
	section = gp_widget_new(GP_WIDGET_SECTION, "Flash/Lens");
	gp_widget_append(window, section);

		t = gp_widget_new(GP_WIDGET_MENU, "Flash Setting");
		gp_widget_append(section, t);
		gp_widget_choice_add(t, "Auto");
		gp_widget_choice_add(t, "Red-eye");
		gp_widget_choice_add(t, "Force");
		gp_widget_choice_add(t, "None");
		gp_widget_value_set(t, "Red-eye");

		t = gp_widget_new(GP_WIDGET_RADIO, "Lens Mode");
		gp_widget_append(section, t);
		gp_widget_choice_add(t, "Normal");
		gp_widget_choice_add(t, "Macro");
		gp_widget_value_set(t, "Macro");
#endif 
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	return (GP_ERROR_NOT_SUPPORTED);
}

int camera_summary (Camera *camera, CameraText *summary) {

	DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

	sprintf(summary->text, "Current directory:\n%s", d->directory);

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) {

	strcpy(manual->text, "The Directory Browse \"camera\" lets you index\nphotos on your hard drive. The folder list on the\nleft contains the folders on your hard drive,\nbeginning at the root directory (\"/\").");

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

	strcpy(about->text, "Directory Browse Mode\nScott Fritzinger <scottf@unr.edu>");

	return (GP_OK);
}
