/* directory.c
 *
 * Copyright (c) 2001 Lutz Mueller <lutz@users.sf.net>
 * Copyright (c) 2005 Marcus Meissner <marcus@jet.franken.de>
 * Copyright (c) 2007 Hubert Figuiere <hub@figuiere.net>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _BSD_SOURCE

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>
#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_VFS_H
# include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATFS_H
# include <sys/statfs.h>
#endif
#include <fcntl.h>

/* will happen only on Win32 */
#ifndef HAVE_LSTAT
#define lstat stat
#endif

#ifdef HAVE_LIBEXIF
#include <libexif/exif-data.h>
#endif

#include <gphoto2/gphoto2-setting.h>
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-portability.h>

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
	{"thm",  GP_MIME_JPEG}, 
	{"tif",  GP_MIME_TIFF},
	{"ppm",  GP_MIME_PPM},
	{"pgm",  GP_MIME_PGM},
	{"avi",  GP_MIME_AVI},
	{"mov",  GP_MIME_QUICKTIME},
	{"moov", GP_MIME_QUICKTIME},
	{"qt",   GP_MIME_QUICKTIME},
	{"qtvr", GP_MIME_QUICKTIME},
	{"mp2",  GP_MIME_MPEG},
	{"mp3",  GP_MIME_MPEG},
	{"mp4",  GP_MIME_MPEG},
	{"mpg",  GP_MIME_MPEG},
	{"mpeg", GP_MIME_MPEG},
	{"mpe",  GP_MIME_MPEG},
	{"ogg",  GP_MIME_OGG},
	{"mts",  GP_MIME_AVCHD},
	{"m2ts", GP_MIME_AVCHD},
	{"pbm", "image/x-portable-bitmap"},
	{"crw",  GP_MIME_CRW},
	{"rw2",  GP_MIME_RW2},
	{"cr2",  GP_MIME_RAW},
	{"nef",  GP_MIME_RAW},
	{"mrw",  GP_MIME_RAW},
	{"dng",  GP_MIME_RAW},
	{"sr2",  GP_MIME_RAW},
	{"raf",  GP_MIME_RAW},
	{"erf",  GP_MIME_RAW},
	{"orf",  GP_MIME_RAW},
	{"arw",  GP_MIME_RAW},
	{"pef",  GP_MIME_RAW},
	{"raw",  GP_MIME_RAW},
	{"dcr",  GP_MIME_RAW},
	{"x3f",  GP_MIME_RAW},
	{"srw",  GP_MIME_RAW},
	{"gf1",  GP_MIME_RAW},
	{"srw",  GP_MIME_RAW},
	{"nrw",  GP_MIME_RAW},
	{"png",  GP_MIME_PNG},
	{"wav",  GP_MIME_WAV},
	{"3gp",  "video/3gpp"},
	{"3g2",  "video/x-3gpp2"},
	{"dif",  "video/dv"},
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
        a.port     = GP_PORT_DISK;
        a.speed[0] = 0;

        a.operations = GP_OPERATION_NONE;

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

	/* Since "Directory Browse" is hardcoded in clients,
	 * better also add a new name here.
	 */
        strcpy(a.model, "Mass Storage Camera");
        gp_abilities_list_append(list, a);
        return (GP_OK);
}

static int
_get_mountpoint(GPPort *port, char **path) {
	GPPortInfo	info;
	int ret;
	char *p;

	ret = gp_port_get_info (port, &info);
	if (ret < GP_OK)
		return ret;
	ret = gp_port_info_get_path (info, path);
	if (ret < GP_OK)
		return ret;
	p = strchr (*path, ':');
	if (p) *path = p+1;
	return GP_OK;
}

static int
_get_path (GPPort *port, const char *folder, const char *file, char *path, unsigned int size) {
	if (port->type == GP_PORT_DISK) {
		int ret;
		char *xpath;

		ret = _get_mountpoint (port, &xpath);
		if (ret < GP_OK)
			return ret;
		snprintf (path, size, "%s/%s/%s", xpath, folder, file);
	} else {
		/* old style access */
		snprintf (path, size, "%s/%s", folder, file);
	}
	return GP_OK;
}


static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	gp_system_dir dir;
	gp_system_dirent de;
	char buf[1024], f[1024];
	unsigned int id, n;
	int ret;
	Camera *camera = (Camera*)data;

	if (camera->port->type == GP_PORT_DISK) {
		char *path;

		ret = _get_mountpoint (camera->port, &path);
		if (ret < GP_OK)
			return ret;
		snprintf (f, sizeof(f), "%s/%s/", path, folder);
		/* UNIX / is empty, or we recurse through the whole fs */
		if (	(!strcmp(path, "") || !strcmp(path, "/")) &&
			!strcmp(folder,"/")
		)
			return GP_OK;
	} else {
		/* old style access */
		if (folder[strlen(folder)-1] != '/') {
			snprintf (f, sizeof(f), "%s%c", folder, '/');
		} else {
			strncpy (f, folder, sizeof(f));
		}
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
		const char * filename = NULL;
		/* Give some feedback */
		gp_context_progress_update (context, id, n + 1);
		gp_context_idle (context);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			gp_system_closedir (dir);
			return (GP_ERROR_CANCEL);
		}

		filename = gp_system_filename(de);
		if (*filename != '.') {
			snprintf (buf, sizeof(buf), "%s%s", f, filename);
			if (gp_system_is_file (buf) && (get_mime_type (buf))) {
				gp_list_append (list, filename,	NULL);
			}
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
		char *path;
		int ret;

		ret = _get_mountpoint (camera->port, &path);
		if (ret < GP_OK)
			return ret;

		snprintf (f, sizeof(f), "%s/%s/", path, folder);
		/* UNIX / is empty, or we recurse through the whole fs */
		if (	(!strcmp(path, "") || !strcmp(path, "/")) &&
			!strcmp(folder,"/")
		)
			return GP_OK;
	} else {
		/* old style access */
		/* Make sure we have 1 delimiter */
		if (folder[strlen(folder)-1] != '/') {
			snprintf (f, sizeof(f), "%s%c", folder, '/');
		} else {
			strncpy (f, folder, sizeof(f));
		}
	}
	dir = gp_system_opendir ((char*) f);
	if (!dir)
		return GP_ERROR;
	/* Count the files */
	n = 0;
	while (gp_system_readdir (dir))
		n++;
	gp_system_closedir (dir);

	dir = gp_system_opendir (f);
	if (!dir)
		return GP_ERROR;
	id = gp_context_progress_start (context, n, _("Listing folders in "
					"'%s'..."), folder);
	n = 0;
	while ((de = gp_system_readdir (dir))) {
		const char * filename = NULL;

		/* Give some feedback */
		gp_context_progress_update (context, id, n + 1);
		gp_context_idle (context);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			gp_system_closedir (dir);
			return GP_ERROR_CANCEL;
		}
		filename = gp_system_filename (de);
		if (*filename != '.') {
			snprintf (buf, sizeof(buf), "%s%s", f, filename);
			
			/* lstat ... do not follow symlinks */
			if (lstat (buf, &st) != 0) {
				int saved_errno = errno;
				gp_context_error (context, _("Could not get information "
							     "about '%s' (%s)."),
						  buf, strerror(saved_errno));
				gp_system_closedir (dir);
				return GP_ERROR;
			}
			if (S_ISDIR (st.st_mode)) {
				gp_list_append(list,	filename,	NULL);
			}
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
	int result;

	result = _get_path (camera->port, folder, file, path, sizeof(path));
	if (result < GP_OK)
		return result;

	if (lstat (path, &st) != 0) {
		int saved_errno = errno;
		gp_context_error (context, _("Could not get information "
					     "about '%s' in '%s' (%s)."),
				  file, folder, strerror(saved_errno));
		return (GP_ERROR);
	}

        info->preview.fields = GP_FILE_INFO_NONE;
        info->file.fields = GP_FILE_INFO_SIZE | 
                            GP_FILE_INFO_TYPE | GP_FILE_INFO_PERMISSIONS |
			    GP_FILE_INFO_MTIME;

	info->file.mtime = st.st_mtime;
	info->file.permissions = GP_FILE_PERM_NONE;
	if (st.st_mode & S_IRUSR)
		info->file.permissions |= GP_FILE_PERM_READ;
	if (st.st_mode & S_IWUSR)
		info->file.permissions |= GP_FILE_PERM_DELETE;
        info->file.size = st.st_size;
	mime_type = get_mime_type (file);
	if (!mime_type)
		mime_type = GP_MIME_UNKNOWN;
	strcpy (info->file.type, mime_type);
        return (GP_OK);
}

static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context)
{
	int retval;
	char path[1024];
	Camera *camera = (Camera*)data;

	retval = _get_path (camera->port, folder, file, path, sizeof(path));
	if (retval < GP_OK)
		return retval;

	/* We don't support updating permissions (yet) */
	if (info.file.fields & GP_FILE_INFO_PERMISSIONS)
		return (GP_ERROR_NOT_SUPPORTED);

	if (info.file.fields & GP_FILE_INFO_MTIME) {
		struct utimbuf utimbuf;

		utimbuf.actime  = info.file.mtime;
		utimbuf.modtime = info.file.mtime;
		if (utime (path, &utimbuf) != 0) {
			int saved_errno = errno;
			gp_context_error (context, _("Could not change "
						     "time of file '%s' in '%s' (%s)."),
					  file, folder, strerror(saved_errno));
			return (GP_ERROR);
		}
	}

#if 0 /* implement this using new api -Marcus */
	if (info.file.fields & GP_FILE_INFO_NAME) {
        	if (!strcasecmp (info.file.name, file))
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
#endif

        return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
        char path[1024];
	int result = GP_OK;
	struct stat stbuf;
	int fd, id;
	unsigned int curread, toread;
	unsigned char *buf;
#ifdef HAVE_LIBEXIF
	ExifData *data;
	unsigned int buf_len;
#endif /* HAVE_LIBEXIF */
	Camera *camera = (Camera*)user_data;

	result = _get_path (camera->port, folder, filename, path, sizeof(path));
	if (result < GP_OK)
		return result;

	if (-1 == lstat(path,&stbuf))
		return GP_ERROR_IO_READ;

	gp_file_set_mtime (file, stbuf.st_mtime);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
#ifdef DEBUG
	case GP_FILE_TYPE_PREVIEW:
#endif
		fd = open (path,O_RDONLY);
		if (fd == -1)
			return GP_ERROR_IO_READ;
		break;
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
		gp_file_set_data_and_size (file, (char *)buf, buf_len);
		return (GP_OK);
#endif /* HAVE_LIBEXIF */
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
#define BLOCKSIZE 65536
	/* do it in 64kb blocks */
	buf = malloc(BLOCKSIZE);
	if (!buf) {
		close (fd);
		return GP_ERROR_NO_MEMORY;
	}
	if (-1 == fstat(fd,&stbuf)) {
		free (buf);
		close (fd);
		return GP_ERROR_IO_READ;
	}

	curread = 0;
	id = gp_context_progress_start (context, (1.0*stbuf.st_size/BLOCKSIZE), _("Getting file..."));
	GP_DEBUG ("Progress id: %i", id);
	result = GP_OK;
	while (curread < stbuf.st_size) {
		int ret;

		toread = stbuf.st_size-curread;
		if (toread>BLOCKSIZE) toread = BLOCKSIZE;
		ret = read(fd,buf,toread);
		if (ret == -1) {
			result = GP_ERROR_IO_READ;
			break;
		}
		curread += ret;
		gp_file_append (file, (char *)buf, ret);
		gp_context_progress_update (context, id, (1.0*curread/BLOCKSIZE));
		gp_context_idle (context);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			result = GP_ERROR_CANCEL;
			break;
		}
#if 0
		/* We could take 2 seconds to download this image. everytime. */
		/* But actually this driver is used in production by some frontends,
		 * so do not delay at all
		 */
		usleep(2000000/(stbuf.st_size/BLOCKSIZE));
#endif
	}
	gp_context_progress_stop (context, id);
	free (buf);
	close (fd);
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
	int result;

	result = _get_path (camera->port, folder, name, path, sizeof(path));
	if (result < GP_OK)
		return result;
	return gp_system_mkdir (path);
}

static int
remove_dir_func (CameraFilesystem *fs, const char *folder, const char *name,
		 void *data, GPContext *context)
{
	char path[2048];
	Camera *camera = (Camera*)data;
	int result;

	result = _get_path (camera->port, folder, name, path, sizeof(path));
	if (result < GP_OK)
		return result;
	return gp_system_rmdir (path);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *file, void *data, GPContext *context)
{
	char path[2048];
	int result;
	Camera *camera = (Camera*)data;

	result = _get_path (camera->port, folder, file, path, sizeof(path));
	if (result < GP_OK)
		return result;
	result = unlink (path);
	if (result) {
		int saved_errno = errno;
		gp_context_error (context, _("Could not delete file '%s' "
					     "in folder '%s' (error code %i: %s)."),
				  file, folder, result, strerror(saved_errno));
		return (GP_ERROR);
	}

	return (GP_OK);
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, const char *name,
	       CameraFileType type, CameraFile *file, void *data, GPContext *context)
{
	char path[2048];
	int result;
#ifdef DEBUG
	unsigned int i, id;
#endif
	Camera *camera = (Camera*)data;

	result = _get_path (camera->port, folder, name, path, sizeof(path));
	if (result < GP_OK)
		return result;

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

static int
storage_info_func (CameraFilesystem *fs,
                CameraStorageInformation **sinfos,
                int *nrofsinfos,
                void *data, GPContext *context
) {
	Camera 				*camera = data;
	CameraStorageInformation	*sfs;

#if defined(linux) && defined(HAVE_STATFS)
	struct	statfs		stfs;
	char *xpath;
	int ret;

	ret = _get_mountpoint (camera->port, &xpath);
	if (ret < GP_OK)
		return ret;
	if (-1 == statfs (xpath, &stfs))
		return GP_ERROR_NOT_SUPPORTED;

	sfs = malloc (sizeof (CameraStorageInformation));
	if (!sfs)
		return GP_ERROR_NO_MEMORY;
	*sinfos = sfs;
	*nrofsinfos = 1;

	sfs->fields =	GP_STORAGEINFO_BASE		|
			GP_STORAGEINFO_DESCRIPTION	|
			GP_STORAGEINFO_STORAGETYPE	|
			GP_STORAGEINFO_FILESYSTEMTYPE	|
			GP_STORAGEINFO_ACCESS		|
			GP_STORAGEINFO_MAXCAPACITY	|
			GP_STORAGEINFO_FREESPACEKBYTES;
	;
	strcpy (sfs->basedir, "/");
	strcpy (sfs->description, "Directory Driver");
	sfs->type		= GP_STORAGEINFO_ST_REMOVABLE_RAM;
	sfs->fstype		= GP_STORAGEINFO_FST_GENERICHIERARCHICAL;
	sfs->access		= GP_STORAGEINFO_AC_READWRITE;
	if (stfs.f_bsize >= 1024) {
		sfs->capacitykbytes	= stfs.f_blocks*(stfs.f_bsize/1024);
		sfs->freekbytes		= stfs.f_bavail*(stfs.f_bsize/1024);
	} else {
		sfs->capacitykbytes	= stfs.f_blocks/(1024/stfs.f_bsize);
		sfs->freekbytes		= stfs.f_bavail/(1024/stfs.f_bsize);
	}
	return GP_OK;
#endif

	return GP_ERROR_NOT_SUPPORTED;
} 

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
	.set_info_func = set_info_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.put_file_func = put_file_func,
	.del_file_func = delete_file_func,
	.make_dir_func = make_dir_func,
	.remove_dir_func = remove_dir_func,
	.storage_info_func = storage_info_func,
};

int
camera_init (Camera *camera, GPContext *context)
{
        /* First, set up all the function pointers */
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;
        return gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
}
