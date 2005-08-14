/* directory.c
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
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
#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>

#ifdef HAVE_LIBEXIF
#include <libexif/exif-data.h>
#endif

#include <gphoto2-setting.h>
#include <gphoto2-library.h>
#include <gphoto2-port.h>
#include <gphoto2-port-log.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
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
	{"tif",  GP_MIME_TIFF},
	{"ppm",  GP_MIME_PPM},
	{"pgm",  GP_MIME_PGM},
	{"pbm", "image/x-portable-bitmap"},
	{"png",  GP_MIME_PNG},
	{NULL, NULL}
};

/* #define DEBUG */
/* #define FOLLOW_LINKS */

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

#ifdef DEBUG
	a.operations = GP_OPERATION_CONFIG | GP_OPERATION_CAPTURE_PREVIEW |
		       GP_OPERATION_CAPTURE_IMAGE;
#else
        a.operations = GP_OPERATION_CONFIG;
#endif
#ifdef DEBUG
        a.file_operations = GP_FILE_OPERATION_PREVIEW |
			    GP_FILE_OPERATION_DELETE | 
			    GP_FILE_OPERATION_EXIF;
#else
	a.file_operations = GP_FILE_OPERATION_DELETE |
			    GP_FILE_OPERATION_EXIF;
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
	gp_system_dir dir;
	gp_system_dirent de;
	char buf[1024], f[1024];
	unsigned int id, n;
	Camera *camera = (Camera*)data;

	if (camera->port->type == GP_PORT_DISK) {
		GPPortSettings settings;

		gp_port_get_settings (camera->port, &settings);
		snprintf (f, sizeof(f), "%s/%s/",
			settings.disk.mountpoint, 
			folder
		);
	} else {
		/* old style access */
		if (folder[strlen(folder)-1] != '/')
			sprintf (f, "%s%c", folder, '/');
		else
			strcpy (f, folder);
	}
	dir = gp_system_opendir ((char*) f);
	if (!dir)
		return (GP_ERROR);
	
	/* Make sure we have 1 delimiter */

	/* Count the files */
	n = 0;
	while (gp_system_readdir (dir))
		n++;

	gp_system_closedir (dir);
	dir = gp_system_opendir (f);
	if (!dir)
		return (GP_ERROR);
	id = gp_context_progress_start (context, n, _("Listing files in "
				"'%s'..."), f);
	n = 0;
	while ((de = gp_system_readdir(dir))) {

		/* Give some feedback */
		gp_context_progress_update (context, id, n + 1);
		gp_context_idle (context);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			gp_system_closedir (dir);
			return (GP_ERROR_CANCEL);
		}

		if (strcmp(gp_system_filename(de), "." ) &&
		    strcmp(gp_system_filename(de), "..")) {
			sprintf (buf, "%s%s", f, gp_system_filename (de));
			if (gp_system_is_file (buf) && (get_mime_type (buf)))
				gp_list_append (list, gp_system_filename (de),
						NULL);
		}
		n++;
	}
	gp_system_closedir (dir);
	gp_context_progress_stop (context, id);

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	gp_system_dir dir;
	gp_system_dirent de;
	char buf[1024], f[1024];
	unsigned int id, n;
	struct stat st;
	Camera *camera = (Camera*)data;

	if (camera->port->type == GP_PORT_DISK) {
		GPPortSettings settings;

		gp_port_get_settings (camera->port, &settings);
		snprintf (f, sizeof(f), "%s/%s/",
			settings.disk.mountpoint, 
			folder
		);
	} else {
		/* old style access */
		/* Make sure we have 1 delimiter */
		if (folder[strlen(folder)-1] != '/')
			sprintf (f, "%s%c", folder, '/');
		else
			strcpy (f, folder);
	}
	dir = gp_system_opendir ((char*) f);
	if (!dir)
		return (GP_ERROR);
	/* Count the files */
	n = 0;
	while (gp_system_readdir (dir))
		n++;
	gp_system_closedir (dir);

	dir = gp_system_opendir (f);
	if (!dir)
		return (GP_ERROR);
	id = gp_context_progress_start (context, n, _("Listing folders in "
					"'%s'..."), folder);
	n = 0;
	while ((de = gp_system_readdir (dir))) {
		/* Give some feedback */
		gp_context_progress_update (context, id, n + 1);
		gp_context_idle (context);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			gp_system_closedir (dir);
			return (GP_ERROR_CANCEL);
		}
		if ((strcmp (gp_system_filename (de), "." ) != 0) &&
		    (strcmp (gp_system_filename (de), "..") != 0)) {
			sprintf (buf, "%s%s", f, gp_system_filename (de));

			/* lstat ... do not follow symlinks */
			if (lstat (buf, &st) != 0) {
				gp_context_error (context, _("Could not get information "
						  "about '%s' (%m)."), buf);
				return (GP_ERROR);
			}
			if (S_ISDIR (st.st_mode))
				gp_list_append (list,
						gp_system_filename (de),
						NULL);
		}
		n++;
	}
	gp_system_closedir (dir);
	gp_context_progress_stop (context, id);
	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
		      CameraFileInfo *info, void *data, GPContext *context)
{
	char path[1024];
	const char *mime_type;
	struct stat st;
	Camera *camera = (Camera*)data;

	if (camera->port->type == GP_PORT_DISK) {
		GPPortSettings settings;

		gp_port_get_settings (camera->port, &settings);
		snprintf (path, sizeof(path), "%s/%s/%s",
			settings.disk.mountpoint, 
			folder,
			file
		);
	} else {
		/* old style access */
		snprintf (path, sizeof (path), "%s/%s", folder, file);
	}

	if (lstat (path, &st) != 0) {
		gp_context_error (context, _("Could not get information "
			"about '%s' in '%s' (%m)."), file, folder);
		return (GP_ERROR);
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
	Camera *camera = (Camera*)data;

	if (camera->port->type == GP_PORT_DISK) {
		GPPortSettings settings;

		gp_port_get_settings (camera->port, &settings);
		snprintf (path, sizeof(path), "%s/%s/%s",
			settings.disk.mountpoint, 
			folder,
			file
		);
	} else {
		/* old style access */
		snprintf (path, sizeof (path), "%s/%s", folder, file);
	}

	/* We don't support updating permissions (yet) */
	if (info.file.fields & GP_FILE_INFO_PERMISSIONS)
		return (GP_ERROR_NOT_SUPPORTED);

	if (info.file.fields & GP_FILE_INFO_MTIME) {
		struct utimbuf utimbuf;

		utimbuf.actime  = info.file.mtime;
		utimbuf.modtime = info.file.mtime;
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
#ifdef HAVE_LIBEXIF
	ExifData *data;
	unsigned char *buf;
	unsigned int buf_len;
#endif /* HAVE_LIBEXIF */
	Camera *camera = (Camera*)user_data;

	if (camera->port->type == GP_PORT_DISK) {
		GPPortSettings settings;

		gp_port_get_settings (camera->port, &settings);
		snprintf (path, sizeof(path), "%s/%s/%s",
			settings.disk.mountpoint, 
			folder,
			filename
		);
	} else {
		/* old style access */
		snprintf (path, sizeof (path), "%s/%s", folder, filename);
	}

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		result = gp_file_open (file, path);
		break;
#ifdef DEBUG
	case GP_FILE_TYPE_PREVIEW:
		result = gp_file_open (file, path);
		break;
#endif
#ifdef HAVE_LIBEXIF
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
#endif /* HAVE_LIBEXIF */
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
		gp_context_idle (context);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
		usleep (10);
	}
	gp_context_progress_stop (context, id);
#endif

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
	Camera *camera = (Camera*)data;

	if (camera->port->type == GP_PORT_DISK) {
		GPPortSettings settings;

		gp_port_get_settings (camera->port, &settings);
		snprintf (path, sizeof(path), "%s/%s/%s",
			settings.disk.mountpoint, 
			folder,
			name
		);
	} else {
		/* old style access */
		snprintf (path, sizeof (path), "%s/%s", folder, name);
	}
	return (gp_system_mkdir (path));
}

static int
remove_dir_func (CameraFilesystem *fs, const char *folder, const char *name,
		 void *data, GPContext *context)
{
	char path[2048];
	Camera *camera = (Camera*)data;

	if (camera->port->type == GP_PORT_DISK) {
		GPPortSettings settings;

		gp_port_get_settings (camera->port, &settings);
		snprintf (path, sizeof(path), "%s/%s/%s",
			settings.disk.mountpoint, 
			folder,
			name
		);
	} else {
		/* old style access */
		snprintf (path, sizeof (path), "%s/%s", folder, name);
	}
	return (gp_system_rmdir (path));
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *file, void *data, GPContext *context)
{
	char path[2048];
	int result;
	Camera *camera = (Camera*)data;

	if (camera->port->type == GP_PORT_DISK) {
		GPPortSettings settings;

		gp_port_get_settings (camera->port, &settings);
		snprintf (path, sizeof(path), "%s/%s/%s",
			settings.disk.mountpoint, 
			folder,
			file
		);
	} else {
		/* old style access */
		snprintf (path, sizeof (path), "%s/%s", folder, file);
	}
	result = unlink (path);
	if (result) {
		gp_context_error (context, _("Could not delete file '%s' "
			"in folder '%s' (error code %i: %m)."),
			file, folder, result);
		return (GP_ERROR);
	}

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
	Camera *camera = (Camera*)data;

	gp_file_get_name (file, &name);

	if (camera->port->type == GP_PORT_DISK) {
		GPPortSettings settings;

		gp_port_get_settings (camera->port, &settings);
		snprintf (path, sizeof(path), "%s/%s/%s",
			settings.disk.mountpoint, 
			folder,
			name
		);
	} else {
		/* old style access */
		snprintf (path, sizeof (path), "%s/%s", folder, name);
	}

	result = gp_file_save (file, path);
	if (result < 0)
		return (result);

#ifdef DEBUG
	id = gp_context_progress_start (context, 500., "Uploading file...");
	for (i = 0; i < 500; i++) {
		gp_context_progress_update (context, id, i + 1);
		gp_context_idle (context);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (result);
		usleep (10);
	}
	gp_context_progress_stop (context, id);
#endif

	return (GP_OK);
}

#ifdef DEBUG
static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	int r;

	r = gp_file_open (file, "/usr/share/pixmaps/gnome-logo-large.png");
	if (r < 0)
		return (r);
	
	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	if (path) {
		strcpy (path->folder, "/usr/share/pixmaps");
		strcpy (path->name, "gnome-logo-large.png");
	}

	return (GP_OK);
}
#endif

int
camera_init (Camera *camera, GPContext *context)
{
        /* First, set up all the function pointers */
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

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

