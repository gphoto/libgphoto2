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

static int
is_image (char *filename)
{

        char *dot;
        int x=0;

        dot = strrchr(filename, '.');
        if (dot) {
                while (extension[x]) {
#ifdef OS2
                        if (stricmp(extension[x], dot+1)==0)
#else
                        if (strcasecmp(extension[x], dot+1)==0)
#endif
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
        CameraAbilities a;

        strcpy(a.model, "Directory Browse");
	a.status = GP_DRIVER_STATUS_PRODUCTION;
        a.port     = GP_PORT_NONE;
        a.speed[0] = 0;

        a.operations = GP_OPERATION_CONFIG;
        a.file_operations = GP_FILE_OPERATION_NONE;
        a.folder_operations = GP_FOLDER_OPERATION_NONE;

        gp_abilities_list_append(list, a);

        return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data)
{
	GP_SYSTEM_DIR dir;
	GP_SYSTEM_DIRENT de;
	char buf[1024], f[1024];

	dir = GP_SYSTEM_OPENDIR ((char*) folder);
	if (!dir)
		return (GP_ERROR);
	
	/* Make sure we have 1 delimiter */
	if (folder[strlen(folder)-1] != '/')
		sprintf (f, "%s%c", folder, '/');
	else
		strcpy (f, folder);

	while ((de = GP_SYSTEM_READDIR(dir))) {
		if (strcmp(GP_SYSTEM_FILENAME(de), "." ) &&
		    strcmp(GP_SYSTEM_FILENAME(de), "..")) {
			sprintf (buf, "%s%s", f, GP_SYSTEM_FILENAME (de));
			if (GP_SYSTEM_IS_FILE (buf) && (is_image (buf)))
				gp_list_append (list, GP_SYSTEM_FILENAME (de),
						NULL);
		}
	}

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data)
{
	GP_SYSTEM_DIR dir;
	GP_SYSTEM_DIRENT de;
	char buf[1024], f[1024];
	const char *dirname;
	int view_hidden=1;

	if (gp_setting_get ("directory", "hidden", buf) == GP_OK)
		view_hidden = atoi (buf);

	dir = GP_SYSTEM_OPENDIR ((char*) folder);
	if (!dir)
		return (GP_ERROR);

	/* Make sure we have 1 delimiter */
	if (folder[strlen(folder)-1] != '/')
		sprintf (f, "%s%c", folder, '/');
	else
		strcpy (f, folder);
	while ((de = GP_SYSTEM_READDIR (dir))) {
		if ((strcmp (GP_SYSTEM_FILENAME (de), "." ) != 0) &&
		    (strcmp (GP_SYSTEM_FILENAME (de), "..") != 0)) {
			sprintf (buf, "%s%s", f, GP_SYSTEM_FILENAME (de));
			dirname = GP_SYSTEM_FILENAME (de);
			if (GP_SYSTEM_IS_DIR (buf)) {
				if (dirname[0] != '.')
					gp_list_append (list,
							GP_SYSTEM_FILENAME (de),
							NULL);
				else if (view_hidden)
					gp_list_append (list,
							GP_SYSTEM_FILENAME (de),
							NULL);
			}
		}
	} 

	return (GP_OK);
}

static int
camera_file_get_info (Camera *camera, const char *folder, const char *file,
		      CameraFileInfo *info)
{
        int result;
        char buf [1024];
        CameraFile *cam_file;
	const char *name;
	const char *mime_type;
	const char *data;
	long int size;

        sprintf (buf, "%s/%s", folder, file);
        result = gp_file_new (&cam_file);
	if (result != GP_OK)
		return (result);
        result = gp_file_open (cam_file, buf);
        if (result != GP_OK) {
                gp_file_free (cam_file);
                return (result);
        }

        info->preview.fields = GP_FILE_INFO_NONE;
        info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_NAME |
                            GP_FILE_INFO_TYPE;

	gp_file_get_name (cam_file, &name);
	gp_file_get_mime_type (cam_file, &mime_type);
	gp_file_get_data_and_size (cam_file, &data, &size);

        strcpy (info->file.type, mime_type);
        strcpy (info->file.name, name);
        info->file.size = size;

        gp_file_free (cam_file);

        return (GP_OK);
}

static int
camera_file_set_info (Camera *camera, const char *folder, const char *file,
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

static int
camera_file_get (Camera *camera, const char *folder, const char *filename,
		 CameraFileType type, CameraFile *file)
{
        /**********************************/
        /* file_number now starts at 0!!! */
        /**********************************/

        char buf[1024];
        int result;

	if (type != GP_FILE_TYPE_NORMAL)
		return (GP_ERROR_NOT_SUPPORTED);

        sprintf(buf, "%s/%s", folder, filename);

        if ((result = gp_file_open(file, buf)) != GP_OK)
                return (result);

        return (GP_OK);
}

static int
camera_get_config (Camera *camera, CameraWidget **window)
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

static int
camera_set_config (Camera *camera, CameraWidget *window)
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

static int
camera_manual (Camera *camera, CameraText *manual)
{
        strcpy(manual->text, "The Directory Browse \"camera\" lets you index\nphotos on your hard drive. The folder list on the\nleft contains the folders on your hard drive,\nbeginning at the root directory (\"/\").");

        return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about)
{
        strcpy(about->text, "Directory Browse Mode\nScott Fritzinger <scottf@unr.edu>");

        return (GP_OK);
}

int
camera_init (Camera *camera)
{
        char buf[256];

        /* First, set up all the function pointers */
        camera->functions->file_get             = camera_file_get;
        camera->functions->file_get_info        = camera_file_get_info;
        camera->functions->file_set_info        = camera_file_set_info;
        camera->functions->get_config           = camera_get_config;
        camera->functions->set_config           = camera_set_config;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

        if (gp_setting_get("directory", "hidden", buf) != GP_OK)
                gp_setting_set("directory", "hidden", "1");

        gp_filesystem_set_list_funcs (camera->fs, file_list_func,
                                      folder_list_func, NULL);

        return (GP_OK);
}

