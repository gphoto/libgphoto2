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

char  dir_directory[1024];
char  dir_images[1024][1024];
int   dir_num_images;
int   dir_get_index;

int glob_debug;

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

int camera_init (CameraInit *init) {

	int i=0;

	dir_num_images = 0;

	glob_debug = init->debug;

	for (i=0; i<1024; i++)
		strcpy(dir_images[i], "");

	strcpy(dir_directory, init->port_settings.path);
	if (strlen(dir_directory)==0) {
		strcpy(dir_directory, "/");
	}

	return (GP_OK);
}

int camera_exit () {

	return (GP_OK);
}

int camera_folder_list(char *folder_name, CameraFolderInfo *list) {

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

int folder_index() {

	DIR *dir;
	struct dirent *de;
	struct stat s;
	char *dot, fname[1024];

	dir_num_images = 0;

	dir = opendir(dir_directory);
	if (!dir)
		return (GP_ERROR);
	de = readdir(dir);
	while (de) {
		sprintf(fname, "%s/%s", dir_directory, de->d_name);
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
				strcpy(dir_images[dir_num_images++],
					de->d_name);
				if (glob_debug)
					printf("directory: found \"%s\"\n", de->d_name);
			   }
			}
		}
		de = readdir(dir);
	}
	closedir(dir);

	return (GP_OK);
}

int camera_folder_set(char *folder_name) {

	strcpy(dir_directory, folder_name);

	return (folder_index());
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
	char *slash;

#if defined(OS2) || defined(WINDOWS)
	sprintf(filename, "%s\\%s", dir_directory,
		dir_images[file_number]);
#else
	sprintf(filename, "%s/%s", dir_directory,
		dir_images[file_number]);
#endif
	if ((fp = fopen(filename, "r"))==NULL)
		return (GP_ERROR);
	fseek(fp, 0, SEEK_END);
	imagesize = ftell(fp);
	rewind(fp);
	file->data = (char *)malloc(sizeof(char)*imagesize);
	fread(file->data, (size_t)imagesize, (size_t)sizeof(char), fp);
	fclose(fp);

	strcpy(file->name, dir_images[file_number]);
	slash = strrchr(dir_images[file_number], '/');
	if (slash)
		sprintf(file->type, "image/%s", &slash[1]);
	   else
		sprintf(file->type, "image/unknown", &slash[1]);

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
	char *slash;

	sprintf(filename, "%s/%s", dir_directory,
		dir_images[file_number]);	
	fp = fopen(filename, "r");
	fseek(fp, 0, SEEK_END);
	imagesize = ftell(fp);
	rewind(fp);
	preview->data = (char *)malloc(sizeof(char)*imagesize);
	fread(preview->data, (size_t)imagesize, (size_t)sizeof(char),fp);
	fclose(fp);

	strcpy(preview->name, dir_images[file_number]);
	slash = strrchr(dir_images[file_number], '/');
	if (slash)
		sprintf(preview->type, "image/%s", &slash[1]);
	   else
		sprintf(preview->type, "image/unknown", &slash[1]);

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

int camera_config_get (CameraWidget *window) {

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

int camera_config_set (CameraSetting *setting, int count) {

	int x;

	printf("Directory library got the following config values:\n");
	for (x=0; x<count; x++)
		printf("\"%s\" = \"%s\"\n", setting[x].name, setting[x].value);

	return (GP_OK);
}

int camera_capture (CameraFile *file, CameraCaptureInfo *info) {

	return (GP_ERROR);
}

int camera_summary (CameraText *summary) {

	sprintf(summary->text, "Current directory:\n%s", dir_directory);

	return (GP_OK);
}

int camera_manual (CameraText *manual) {

	strcpy(manual->text, 
"The Directory Browse \"camera\" lets you index
photos on your hard drive. The folder list on the
left contains the folders on your hard drive,
beginning at the root directory (\"/\").
");

	return (GP_OK);
}

int camera_about (CameraText *about) {

	strcpy(about->text,
"Directory Browse Mode
Scott Fritzinger <scottf@unr.edu>");

	return (GP_OK);
}
