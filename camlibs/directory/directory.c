/* Notes:
        * uses the setting value of "dir_directory" to open as the directory
          (interfaces will want to set this when they choose to "open" a
          directory.)
        * adding filetypes: search for "jpg", copy line and change extension
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>
#include <gphoto2-port.h>

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

int is_image (char *filename) 
{

        char *dot;
        int x=0;

        dot = strrchr(filename, '.');
        if (dot) {
                while (extension[x]) {
                        if (strcmp(extension[x], dot+1)==0)
                                return (1);
                        x++;
                }
        }
        return (0);
}

int camera_id (CameraText *id) 
{
        strcpy(id->text, "directory");

        return (GP_OK);
}


int camera_abilities (CameraAbilitiesList *list) 
{
        CameraAbilities *a;

        a = gp_abilities_new();

        strcpy(a->model, "Directory Browse");
        a->port     = GP_PORT_NONE;
        a->speed[0] = 0;

        a->operations = GP_OPERATION_CONFIG;
        a->file_operations = GP_FILE_OPERATION_NONE;
	a->folder_operations = GP_FOLDER_OPERATION_NONE;

        gp_abilities_list_append(list, a);

        return (GP_OK);
}

int camera_init (Camera *camera) 
{
        int i = 0;
        DirectoryStruct *d;
        char buf[256];

        /* First, set up all the function pointers */
        camera->functions->id           	= camera_id;
        camera->functions->abilities    	= camera_abilities;
        camera->functions->init         	= camera_init;
        camera->functions->exit         	= camera_exit;
        camera->functions->folder_list_folders 	= camera_folder_list_folders;
        camera->functions->folder_list_files    = camera_folder_list_files;
        camera->functions->file_get     	= camera_file_get;
	camera->functions->file_set_info	= camera_file_set_info;
	camera->functions->get_config		= camera_get_config;
	camera->functions->set_config		= camera_set_config;
        camera->functions->summary      	= camera_summary;
        camera->functions->manual       	= camera_manual;
        camera->functions->about        	= camera_about;

        d = (DirectoryStruct*)malloc(sizeof(DirectoryStruct));
        camera->camlib_data = d;

        d->num_images = 0;

        for (i=0; i<1024; i++)
                strcpy(d->images[i], "");

        strcpy(d->directory, camera->port->path);
        if (strlen(d->directory)==0) {
                strcpy(d->directory, "/");
        }

        if (gp_setting_get("directory", "hidden", buf) != GP_OK)
                gp_setting_set("directory", "hidden", "1");

        return (GP_OK);
}

int camera_exit (Camera *camera) 
{
        DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

        free(d);

        return (GP_OK);
}

int camera_file_set_info (Camera *camera, const char *folder, const char *file, 
			  CameraFileInfo *info)
{
	int retval;
	char *path_old;
	char *path_new;

	if ((info->file.fields == GP_FILE_INFO_NONE) && 
	    (info->preview.fields == GP_FILE_INFO_NONE))
		return (GP_OK);

	/* We only support basic renaming in the same folder. */
	if (!((info->file.fields == GP_FILE_INFO_NAME) && 
	      (info->preview.fields == GP_FILE_INFO_NONE))) 
		return (GP_ERROR_NOT_SUPPORTED);
	
	if (!strcmp (info->file.name, file))
		return (GP_OK);

	/* We really have to rename the poor file... */
	path_old = malloc (strlen (folder) + 1 + strlen (file) + 1);
	path_new = malloc (strlen (folder) + 1 + strlen (info->file.name) + 1);
	strcpy (path_old, folder);
	strcpy (path_new, folder);
	path_old [strlen (folder)] = '/';
	path_new [strlen (folder)] = '/';
	strcpy (path_old + strlen (folder) + 1, file);
	strcpy (path_new + strlen (folder) + 1, info->file.name);

	if (*(path_old + 1) == '/')
		retval = rename (path_old + 1, path_new + 1);
	else 
		retval = rename (path_old, path_new);
	free (path_old);
	free (path_new);

	if (retval != 0) {
		switch (errno) {
		case EISDIR:
			return (GP_ERROR_DIRECTORY_EXISTS);
		case EEXIST:
			return (GP_ERROR_FILE_EXISTS);
		case EINVAL:
			return (GP_ERROR_BAD_PARAMETERS);
		case EIO:
			return (GP_ERROR_IO);
		case ENOMEM:
			return (GP_ERROR_NO_MEMORY);
		case ENOENT:
			return (GP_ERROR_FILE_NOT_FOUND);
		default:
			return (GP_ERROR);
		}
	}
	
	return (GP_OK);
}

int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list)
{
        GP_SYSTEM_DIR d;
        GP_SYSTEM_DIRENT de;
        char buf[1024], f[1024];

        if ((d = GP_SYSTEM_OPENDIR ((char*) folder))==NULL)
                return (GP_ERROR);

        /* Make sure we have 1 delimiter */
        if (folder[strlen(folder)-1] != '/')
                sprintf(f, "%s%c", folder, '/');
         else
                strcpy(f, folder);

        while ((de = GP_SYSTEM_READDIR(d))) {
                if ((strcmp(GP_SYSTEM_FILENAME(de), "." )!=0) &&
                    (strcmp(GP_SYSTEM_FILENAME(de), "..")!=0)) {
                        sprintf(buf, "%s%s", f, GP_SYSTEM_FILENAME(de));
                        if (GP_SYSTEM_IS_FILE(buf) && (is_image(buf)))
                                gp_list_append(list, GP_SYSTEM_FILENAME(de), GP_LIST_FILE);
                }
        }

        return (GP_OK);
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list)
{
        GP_SYSTEM_DIR d;
        GP_SYSTEM_DIRENT de;
        char buf[1024], f[1024];
        char *dirname;
        int view_hidden=1;

        if (gp_setting_get ("directory", "hidden", buf) == GP_OK)
                view_hidden = atoi(buf);
        if ((d = GP_SYSTEM_OPENDIR ((char*) folder))==NULL)
                return (GP_ERROR);

        /* Make sure we have 1 delimiter */
        if (folder[strlen(folder)-1] != '/')
                sprintf (f, "%s%c", folder, '/');
         else
                strcpy (f, folder);

        while ((de = GP_SYSTEM_READDIR(d))) {
                if ((strcmp (GP_SYSTEM_FILENAME (de), "." ) != 0) &&
                    (strcmp (GP_SYSTEM_FILENAME (de), "..") != 0)) {
                        sprintf (buf, "%s%s", f, GP_SYSTEM_FILENAME (de));
                        dirname = GP_SYSTEM_FILENAME(de);
                        if (GP_SYSTEM_IS_DIR (buf)) {
                           if (dirname[0] != '.')
                                gp_list_append (list, GP_SYSTEM_FILENAME (de), 
						GP_LIST_FOLDER);
                             else
                               if (view_hidden)
                                gp_list_append (list, GP_SYSTEM_FILENAME (de), 
						GP_LIST_FOLDER);
                        }
                }
        }

        return (GP_OK);
}

int folder_index(Camera *camera) 
{
        GP_SYSTEM_DIR dir;
        GP_SYSTEM_DIRENT de;
        char fname[1024];
        DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

        d->num_images = 0;

        dir = GP_SYSTEM_OPENDIR(d->directory);
        if (!dir)
                return (GP_ERROR);
        de = GP_SYSTEM_READDIR(dir);
        while (de) {
                sprintf(fname, "%s/%s", d->directory, GP_SYSTEM_FILENAME(de));
                if (GP_SYSTEM_IS_FILE(fname)) {
                        strcpy(d->images[d->num_images++], GP_SYSTEM_FILENAME(de));
                        gp_debug_printf(GP_DEBUG_LOW, "directory", "found \"%s\"\n", GP_SYSTEM_FILENAME(de));
                }
                de = GP_SYSTEM_READDIR(dir);
        }
        GP_SYSTEM_CLOSEDIR(dir);

        return (GP_OK);
}

int directory_folder_set (Camera *camera, const char *folder_name) 
{
        DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

        strcpy (d->directory, folder_name);

        return (folder_index (camera));
}

int camera_file_get (Camera *camera, const char *folder, const char *filename, 
		     CameraFile *file)
{
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

int camera_get_config (Camera *camera, CameraWidget **window) 
{
	CameraWidget *widget;
	char buf[256];
	int val;

	gp_widget_new (GP_WIDGET_WINDOW, "Dummy", window);
	gp_widget_new (GP_WIDGET_TOGGLE, "View hidden (dot) directories", 
		       &widget);
	gp_setting_get ("directory", "hidden", buf);
	val = atoi (buf);
	gp_widget_set_value (widget, &val);
	gp_widget_append (*window, widget);

	return (GP_OK);
}

int camera_set_config (Camera *camera, CameraWidget *window) 
{
	CameraWidget *widget;
	char buf[256];
	int val;
	
	gp_widget_get_child_by_label (window, "View hidden (dot) directories", 
				  &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &val);
		sprintf (buf, "%i", val);
		gp_setting_set ("directory", "hidden", buf);
	}
	return (GP_OK);
}

int camera_summary (Camera *camera, CameraText *summary) 
{
        DirectoryStruct *d = (DirectoryStruct*)camera->camlib_data;

        sprintf(summary->text, "Current directory:\n%s", d->directory);

        return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) 
{
        strcpy(manual->text, "The Directory Browse \"camera\" lets you index\nphotos on your hard drive. The folder list on the\nleft contains the folders on your hard drive,\nbeginning at the root directory (\"/\").");

        return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) 
{
        strcpy(about->text, "Directory Browse Mode\nScott Fritzinger <scottf@unr.edu>");

        return (GP_OK);
}
