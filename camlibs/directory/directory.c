/* directory.c
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>

#ifdef HAVE_EXIF
#include <libexif/exif-data.h>
#endif

#include <gphoto2-setting.h>
#include <gphoto2-library.h>
#include <gphoto2-port.h>
#include <gphoto2-port-log.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String) 
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

static const struct {
	const char *extension;
	const char *mime_type;
} mime_table[] = {
	{"jpeg", GP_MIME_JPEG},
	{"jpg",  GP_MIME_JPEG}, 
	{"tif", "image/tiff"},
	{"ppm", "image/x-portable-pixmap"},
	{"pgm", "image/x-portable-graymap"},
	{"pbm", "image/x-portable-bitmap"},
	{"png",  GP_MIME_PNG},
	{NULL, NULL}
};

#define DEBUG
#define GP_MODULE "directory"

static const char *
get_mime_type (const char *filename)
{

        char *dot;
        int x=0;

        dot = strrchr(filename, '.');
        if (dot) {
		for (x = 0; mime_table[x].extension; x++) {
#ifdef OS2
			if (!stricmp(mime_table[x].extension, dot+1))
#else
			if (!strcasecmp (mime_table[x].extension, dot+1))
#endif
				return (mime_table[x].mime_type);
                }
	}

        return (NULL);
}

int camera_id (CameraText *id)
{
        strcpy(id->text, "directory");

        return (GP_OK);
}


int camera_abilities (CameraAbilitiesList *list)
{
        CameraAbilities a;

	memset(&a, 0, sizeof(a));
        strcpy(a.model, "Directory Browse");
	a.status = GP_DRIVER_STATUS_PRODUCTION;
        a.port     = GP_PORT_NONE;
        a.speed[0] = 0;

        a.operations = GP_OPERATION_CONFIG;
#ifdef DEBUG
        a.file_operations = GP_FILE_OPERATION_PREVIEW |
			    GP_FILE_OPERATION_DELETE;
#else
	a.file_operations = GP_FILE_OPERATION_DELETE;
#endif
        a.folder_operations = GP_FOLDER_OPERATION_MAKE_DIR |
			      GP_FOLDER_OPERATION_REMOVE_DIR |
			      GP_FOLDER_OPERATION_PUT_FILE;

        gp_abilities_list_append(list, a);

        return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	GP_SYSTEM_DIR dir;
	GP_SYSTEM_DIRENT de;
	char buf[1024], f[1024];
	unsigned int id, n;

	dir = GP_SYSTEM_OPENDIR ((char*) folder);
	if (!dir)
		return (GP_ERROR);
	
	/* Make sure we have 1 delimiter */
	if (folder[strlen(folder)-1] != '/')
		sprintf (f, "%s%c", folder, '/');
	else
		strcpy (f, folder);

	/* Count the files */
	n = 0;
	while (GP_SYSTEM_READDIR (dir))
		n++;

	GP_SYSTEM_CLOSEDIR (dir);
	dir = GP_SYSTEM_OPENDIR (folder);
	if (!dir)
		return (GP_ERROR);
	id = gp_context_progress_start (context, n, _("Listing files in "
				"'%s'..."), folder);
	n = 0;
	while ((de = GP_SYSTEM_READDIR(dir))) {
		gp_context_progress_update (context, id, n + 1);
		if (strcmp(GP_SYSTEM_FILENAME(de), "." ) &&
		    strcmp(GP_SYSTEM_FILENAME(de), "..")) {
			sprintf (buf, "%s%s", f, GP_SYSTEM_FILENAME (de));
			if (GP_SYSTEM_IS_FILE (buf) && (get_mime_type (buf)))
				gp_list_append (list, GP_SYSTEM_FILENAME (de),
						NULL);
		}
		n++;
	}
	gp_context_progress_stop (context, id);

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	GP_SYSTEM_DIR dir;
	GP_SYSTEM_DIRENT de;
	char buf[1024], f[1024], link[1024];
	const char *dirname;
	int view_hidden=1;
	unsigned int id, n;
	struct stat st;

	if (gp_setting_get ("directory", "hidden", buf) == GP_OK)
		view_hidden = atoi (buf);

	/* Check if this is a link */
	if (lstat (folder, &st) != 0) {
		gp_context_error (context, _("Could not get information "
				  "about '%s' (%m)."), folder);
		return (GP_ERROR);
	}
	if (S_ISLNK (st.st_mode)) {
		if (readlink (folder, link, sizeof (link) < 0)) {
			gp_context_error (context, _("Could not follow the "
				"link '%s' (%m)."), folder);
			return (GP_ERROR);
		}
		GP_DEBUG ("Following link '%s' -> '%s'...", folder, link);
		return (folder_list_func (fs, link, list, data, context));
	}

	dir = GP_SYSTEM_OPENDIR ((char*) folder);
	if (!dir)
		return (GP_ERROR);

	/* Make sure we have 1 delimiter */
	if (folder[strlen(folder)-1] != '/')
		sprintf (f, "%s%c", folder, '/');
	else
		strcpy (f, folder);

	/* Count the files */
	n = 0;
	while (GP_SYSTEM_READDIR (dir))
		n++;

	GP_SYSTEM_CLOSEDIR (dir);
	dir = GP_SYSTEM_OPENDIR (folder);
	if (!dir)
		return (GP_ERROR);
	id = gp_context_progress_start (context, n, _("Listing folders in "
					"'%s'..."), folder);
	n = 0;
	while ((de = GP_SYSTEM_READDIR (dir))) {
		gp_context_progress_update (context, id, n + 1);
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
		n++;
	}
	gp_context_progress_stop (context, id);

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
		      CameraFileInfo *info, void *data, GPContext *context)
{
	char path[1024], link[1024];
	char *name;
	const char *mime_type;
	struct stat st;

	if (strlen (folder) == 1)
		snprintf (path, sizeof (path), "/%s", file);
	else
		snprintf (path, sizeof (path), "%s/%s", folder, file);
	if (lstat (path, &st) != 0) {
		gp_context_error (context, _("Could not get information "
			"about '%s' in '%s' (%m)."), file, folder);
		return (GP_ERROR);
	}
	if (S_ISLNK (st.st_mode)) {
		if (readlink (path, link, sizeof (link) < 0)) {
			gp_context_error (context, _("Could not follow the "
				"link '%s' in '%s' (%m)."), file, folder);
			return (GP_ERROR);
		}
		name = strrchr (link, '/');
		if (!name)
			return (GP_ERROR);
		else {
			*name = '\0';
			name++;
			return (get_info_func (fs, link, name, info, data,
					       context));
		}
	}

#ifdef DEBUG
	info->preview.fields = GP_FILE_INFO_SIZE;
#else
        info->preview.fields = GP_FILE_INFO_NONE;
#endif
        info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_NAME |
                            GP_FILE_INFO_TYPE | GP_FILE_INFO_PERMISSIONS |
			    GP_FILE_INFO_MTIME;

	info->file.mtime = st.st_mtime;
	info->file.permissions = GP_FILE_PERM_NONE;
	if (st.st_mode & S_IRUSR)
		info->file.permissions |= GP_FILE_PERM_READ;
	if (st.st_mode & S_IWUSR)
		info->file.permissions |= GP_FILE_PERM_DELETE;
        strcpy (info->file.name, file);
        info->file.size = st.st_size;
#ifdef DEBUG
	info->preview.size = st.st_size;
#endif

	mime_type = get_mime_type (file);
	if (!mime_type)
		mime_type = "application/octet-stream";
	strcpy (info->file.type, mime_type);

        return (GP_OK);
}

static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context)
{
	int retval;
	char path_old[1024], path_new[1024], path[1024];

	/* We don't support updating permissions (yet) */
	if (info.file.fields & GP_FILE_INFO_PERMISSIONS)
		return (GP_ERROR_NOT_SUPPORTED);

	if (info.file.fields & GP_FILE_INFO_MTIME) {
		struct utimbuf utimbuf;

		utimbuf.actime  = info.file.mtime;
		utimbuf.modtime = info.file.mtime;
		if (strlen (folder) == 1)
			snprintf (path, sizeof (path), "/%s", file);
		else
			snprintf (path, sizeof (path), "%s/%s", folder, file);
		if (utime (path, &utimbuf) != 0) {
			gp_context_error (context, _("Could not change "
				"time of file '%s' in '%s' (%m)."),
				file, folder);
			return (GP_ERROR);
		}
	}

	if (info.file.fields & GP_FILE_INFO_NAME) {
        	if (!strcmp (info.file.name, file))
        	        return (GP_OK);

	        /* We really have to rename the poor file... */
		if (strlen (folder) == 1) {
			snprintf (path_old, sizeof (path_old), "/%s",
				  file);
			snprintf (path_new, sizeof (path_new), "/%s",
				  info.file.name);
		} else {
			snprintf (path_old, sizeof (path_old), "%s/%s",
				  folder, file);
			snprintf (path_new, sizeof (path_new), "%s/%s",
				  folder, info.file.name);
		}
                retval = rename (path_old, path_new);
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
	}

        return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
        char path[1024];
	int result = GP_OK;
#ifdef DEBUG
	unsigned int i, id;
#endif
#ifdef HAVE_EXIF
	ExifData *data;
	unsigned char *buf;
	unsigned int buf_len;
#endif /* HAVE_EXIF */

	if (strlen (folder) == 1)
		snprintf (path, sizeof (path), "/%s", filename);
	else
		snprintf (path, sizeof (path), "%s/%s", folder, filename);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		result = gp_file_open (file, path);
		break;
#ifdef DEBUG
	case GP_FILE_TYPE_PREVIEW:
		result = gp_file_open (file, path);
		break;
#endif
#ifdef HAVE_EXIF
	case GP_FILE_TYPE_EXIF:
		data = exif_data_new_from_file (path);
		if (!data) {
			gp_context_error (context, _("Could not open '%s'."),
					  path);
			return (GP_ERROR);
		}
		exif_data_save_data (data, &buf, &buf_len);
		exif_data_unref (data);
		gp_file_set_data_and_size (file, buf, buf_len);
		return (GP_OK);
#endif /* HAVE_EXIF */
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
	if (result < 0)
		return (result);

#ifdef DEBUG
	id = gp_context_progress_start (context, 500., "Getting file...");
	GP_DEBUG ("Progress id: %i", id);
	for (i = 0; i < 500; i++) {
		gp_context_progress_update (context, id, i + 1);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
		usleep (10);
	}
	gp_context_progress_stop (context, id);
#endif

	return (GP_OK);
}

static int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *widget;
#ifdef DEBUG
	CameraWidget *section;
#endif
        char buf[256];
        int val;

        gp_widget_new (GP_WIDGET_WINDOW, _("Directory Browse"), window);
        gp_widget_new (GP_WIDGET_TOGGLE, _("View hidden directories"),
                       &widget);
        gp_setting_get ("directory", "hidden", buf);
        val = atoi (buf);
        gp_widget_set_value (widget, &val);
        gp_widget_append (*window, widget);

#ifdef DEBUG
	gp_widget_new (GP_WIDGET_SECTION, "Testing", &section);
	gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_TEXT, "Text", &widget);
	gp_widget_set_value (widget, "This is some text.");
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_DATE, "Date & Time", &widget);
	val = time (NULL) - 100000;
	gp_widget_set_value (widget, &val);
	gp_widget_append (section, widget);
#endif

        return (GP_OK);
}

static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
        CameraWidget *widget;
        char buf[256];
        int val;

        gp_widget_get_child_by_label (window, _("View hidden directories"),
				      &widget);
        if (gp_widget_changed (widget)) {
                gp_widget_get_value (widget, &val);
                sprintf (buf, "%i", val);
                gp_setting_set ("directory", "hidden", buf);
        }
        return (GP_OK);
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
        strcpy (manual->text, _("The Directory Browse \"camera\" lets "
		"you index photos on your hard drive."));

        return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
        strcpy (about->text, _("Directory Browse Mode - written "
		"by Scott Fritzinger <scottf@unr.edu>."));

        return (GP_OK);
}

static int
make_dir_func (CameraFilesystem *fs, const char *folder, const char *name,
	       void *data, GPContext *context)
{
	char path[2048];

	strncpy (path, folder, sizeof (path));
	if (strlen (folder) > 1)
		strncat (path, "/", sizeof (path));
	strncat (path, name, sizeof (path));

	return (GP_SYSTEM_MKDIR (path));
}

static int
remove_dir_func (CameraFilesystem *fs, const char *folder, const char *name,
		 void *data, GPContext *context)
{
	char path[2048];

	strncpy (path, folder, sizeof (path));
	if (strlen (folder) > 1)
		strncat (path, "/", sizeof (path));
	strncat (path, name, sizeof (path));

	return (GP_SYSTEM_RMDIR (path));
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *file, void *data, GPContext *context)
{
	char path[2048];

	if (strlen (folder) > 1)
		snprintf (path, sizeof (path), "%s/%s", folder, file);
	else
		snprintf (path, sizeof (path), "/%s", file);
	if (unlink (path))
		return (GP_ERROR);

	return (GP_OK);
}

static int
put_file_func (CameraFilesystem *fs, const char *folder,
	       CameraFile *file, void *data, GPContext *context)
{
	char path[2048];
	const char *name;
	int result;
#ifdef DEBUG
	unsigned int i, id;
#endif

	gp_file_get_name (file, &name);

	strncpy (path, folder, sizeof (path));
	if (strlen (folder) > 1)
		strncat (path, "/", sizeof (path));
	strncat (path, name, sizeof (path));

	result = gp_file_save (file, path);
	if (result < 0)
		return (result);

#ifdef DEBUG
	id = gp_context_progress_start (context, 500., "Uploading file...");
	for (i = 0; i < 500; i++) {
		gp_context_progress_update (context, id, i + 1);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (result);
		usleep (10);
	}
	gp_context_progress_stop (context, id);
#endif

	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context)
{
        char buf[256];

        /* First, set up all the function pointers */
        camera->functions->get_config           = camera_get_config;
        camera->functions->set_config           = camera_set_config;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

        if (gp_setting_get("directory", "hidden", buf) != GP_OK)
                gp_setting_set("directory", "hidden", "1");

        gp_filesystem_set_list_funcs (camera->fs, file_list_func,
                                      folder_list_func, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func,
				      set_info_func, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      delete_file_func, camera);
	gp_filesystem_set_folder_funcs (camera->fs, put_file_func, NULL,
					make_dir_func, remove_dir_func, camera);

        return (GP_OK);
}

