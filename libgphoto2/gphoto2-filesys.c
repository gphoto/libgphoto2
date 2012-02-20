/** \file
 *
 * \author Copyright 2000 Scott Fritzinger
 * \author Contributions Lutz Müller <lutz@users.sf.net> (2001)
 * \author Copyright 2009 Marcus Meissner
 *
 * \par License
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \par
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define _BSD_SOURCE

#include "config.h"
#include <gphoto2/gphoto2-filesys.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

#include <limits.h>

#ifdef HAVE_LIBEXIF
#  include <libexif/exif-data.h>
#endif

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

#define GP_MODULE "libgphoto2"

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

typedef struct _CameraFilesystemFile {
	char *name;

	int info_dirty;

	CameraFileInfo info;

	struct _CameraFilesystemFile *lru_prev;
	struct _CameraFilesystemFile *lru_next;
	CameraFile *preview;
	CameraFile *normal;
	CameraFile *raw;
	CameraFile *audio;
	CameraFile *exif;
	CameraFile *metadata;

	struct _CameraFilesystemFile *next; /* in folder */
} CameraFilesystemFile;

typedef struct _CameraFilesystemFolder {
	char *name;

	int files_dirty;
	int folders_dirty;

	struct _CameraFilesystemFolder *next; /* chain in same folder */
	struct _CameraFilesystemFolder *folders; /* childchain of this folder */
	struct _CameraFilesystemFile *files; /* of this folder */
} CameraFilesystemFolder;

/**
 * The default number of pictures to keep in the internal cache,
 * can be overriden by settings.
 */
#define PICTURES_TO_KEEP	2
/**
 * The current number of pictures to keep in the internal cache,
 * either from #PICTURES_TO_KEEP or from the settings.
 */
static int pictures_to_keep = -1;

static int gp_filesystem_lru_clear (CameraFilesystem *fs);
static int gp_filesystem_lru_remove_one (CameraFilesystem *fs, CameraFilesystemFile *item);

#ifdef HAVE_LIBEXIF

static int gp_filesystem_get_file_impl (CameraFilesystem *, const char *,
		const char *, CameraFileType, CameraFile *, GPContext *);

static time_t
get_time_from_exif_tag(ExifEntry *e) {
	struct tm ts;

	e->data[4] = e->data[ 7] = e->data[10] = e->data[13] = e->data[16] = 0;
	ts.tm_year = atoi ((char*)e->data) - 1900;
	ts.tm_mon  = atoi ((char*)(e->data + 5)) - 1;
	ts.tm_mday = atoi ((char*)(e->data + 8));
	ts.tm_hour = atoi ((char*)(e->data + 11));
	ts.tm_min  = atoi ((char*)(e->data + 14));
	ts.tm_sec  = atoi ((char*)(e->data + 17));

	return mktime (&ts);
}

static time_t
get_exif_mtime (const unsigned char *data, unsigned long size)
{
	ExifData *ed;
	ExifEntry *e;
	time_t t, t1 = 0, t2 = 0, t3 = 0;

	ed = exif_data_new_from_data (data, size);
	if (!ed) {
		GP_DEBUG ("Could not parse data for EXIF information.");
		return 0;
	}

	/*
	 * HP PhotoSmart C30 has the date and time in ifd_exif.
	 */
#ifdef HAVE_LIBEXIF_IFD
	e = exif_content_get_entry (ed->ifd[EXIF_IFD_0], EXIF_TAG_DATE_TIME);
	if (e)
		t1 = get_time_from_exif_tag(e);
	e = exif_content_get_entry (ed->ifd[EXIF_IFD_EXIF],
				    EXIF_TAG_DATE_TIME_ORIGINAL);
	if (e)
		t2 = get_time_from_exif_tag(e);
	e = exif_content_get_entry (ed->ifd[EXIF_IFD_EXIF],
				    EXIF_TAG_DATE_TIME_DIGITIZED);
	if (e)
		t3 = get_time_from_exif_tag(e);
#else
	e = exif_content_get_entry (ed->ifd0, EXIF_TAG_DATE_TIME);
	if (e) {
		t1 = get_time_from_exif_tag(e);
		exif_data_unref (e);
	}
	e = exif_content_get_entry (ed->ifd_exif,
				    EXIF_TAG_DATE_TIME_ORIGINAL);
	if (e) {
		t2 = get_time_from_exif_tag(e);
		exif_data_unref (e);
	}
	e = exif_content_get_entry (ed->ifd_exif,
				    EXIF_TAG_DATE_TIME_DIGITIZED);
	if (e) {
		t3 = get_time_from_exif_tag(e);
		exif_data_unref (e);
	}
#endif
	exif_data_unref (ed);
	if (!t1 && !t2 && !t3) {
		GP_DEBUG ("EXIF data has not date/time tags.");
		return 0;
	}

	/* Perform some sanity checking on those tags */
	t = t1; /* "last modified" */

	if (t2 > t)	/* "image taken" > "last modified" ? can not be */
		t = t2;
	if (t3 > t)	/* "image digitized" > max(last two) ? can not be */
		t = t3;

	GP_DEBUG ("Found time in EXIF data: '%s'.", asctime (localtime (&t)));
	return (t);
}

static time_t
gp_filesystem_get_exif_mtime (CameraFilesystem *fs, const char *folder,
			      const char *filename)
{
	CameraFile *file;
	const char *data = NULL;
	unsigned long int size = 0;
	time_t t;

	if (!fs)
		return 0;

	/* This is only useful for JPEGs. Avoid querying it for other types. */
	if (	!strstr(filename,"jpg")  && !strstr(filename,"JPG") &&
		!strstr(filename,"jpeg") && !strstr(filename,"JPEG")
	)
		return 0;

	gp_file_new (&file);
	if (gp_filesystem_get_file (fs, folder, filename,
				GP_FILE_TYPE_EXIF, file, NULL) != GP_OK) {
		GP_DEBUG ("Could not get EXIF data of '%s' in folder '%s'.",
			  filename, folder);
		gp_file_unref (file);
		return 0;
	}

	gp_file_get_data_and_size (file, &data, &size);
	t = get_exif_mtime ((unsigned char*)data, size);
	gp_file_unref (file);

	return (t);
}
#endif

/**
 * \brief The internal camera filesystem structure
 *
 * The internals of the #CameraFilesystem are only visible to gphoto2. You
 * can only access them using the functions provided by gphoto2.
 **/
struct _CameraFilesystem {
	CameraFilesystemFolder *rootfolder;

	CameraFilesystemFile *lru_first;
	CameraFilesystemFile *lru_last;
	unsigned long int lru_size;

	CameraFilesystemGetInfoFunc get_info_func;
	CameraFilesystemSetInfoFunc set_info_func;
	void *info_data;

	CameraFilesystemListFunc file_list_func;
	CameraFilesystemListFunc folder_list_func;
	void *list_data;

	CameraFilesystemGetFileFunc get_file_func;
	CameraFilesystemDeleteFileFunc delete_file_func;
	void *file_data;

	CameraFilesystemPutFileFunc put_file_func;
	CameraFilesystemDeleteAllFunc delete_all_func;
	CameraFilesystemDirFunc make_dir_func;
	CameraFilesystemDirFunc remove_dir_func;
	void *folder_data;

	CameraFilesystemStorageInfoFunc	storage_info_func;
};

#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CR(result)           {int r = (result); if (r < 0) return (r);}
#define CHECK_MEM(m)         {if (!(m)) return (GP_ERROR_NO_MEMORY);}

#define CL(result,list)			\
{					\
	int r = (result);		\
					\
	 if (r < 0) {			\
		gp_list_free (list);	\
		return (r);		\
	}				\
}

#define CU(result,file)			\
{					\
	int r = (result);		\
					\
	if (r < 0) {			\
		gp_file_unref (file);	\
		return (r);		\
	}				\
}

#define CC(context)							\
{									\
	if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)	\
		return GP_ERROR_CANCEL;					\
}

#define CA(f,c)							\
{									\
	if ((f)[0] != '/') {						\
		gp_context_error ((c),					\
			_("The path '%s' is not absolute."), (f));	\
		return (GP_ERROR_PATH_NOT_ABSOLUTE);			\
	}								\
}

static int
delete_all_files (CameraFilesystem *fs, CameraFilesystemFolder *folder)
{
	CameraFilesystemFile	*file;

	CHECK_NULL (folder);
	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Delete all files in folder %p/%s", folder, folder->name);

	file = folder->files;
	while (file) {
		CameraFilesystemFile	*next;
		/* Get rid of cached files */
		gp_filesystem_lru_remove_one (fs, file);
		if (file->preview) {
			gp_file_unref (file->preview);
			file->preview = NULL;
		}
		if (file->normal) {
			gp_file_unref (file->normal);
			file->normal = NULL;
		}
		if (file->raw) {
			gp_file_unref (file->raw);
			file->raw = NULL;
		}
		if (file->audio) {
			gp_file_unref (file->audio);
			file->audio = NULL;
		}
		if (file->exif) {
			gp_file_unref (file->exif);
			file->exif = NULL;
		}
		if (file->metadata) {
			gp_file_unref (file->metadata);
			file->metadata = NULL;
		}
		next = file->next;
		free (file->name);
		free (file);
		file = next;
	}
	folder->files = NULL;
	return (GP_OK);
}

static int
delete_folder (CameraFilesystem *fs, CameraFilesystemFolder **folder)
{
	CameraFilesystemFolder *next;
	CHECK_NULL (folder);

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Delete one folder %p/%s", *folder, (*folder)->name);
	next = (*folder)->next;
	delete_all_files (fs, *folder);
	free ((*folder)->name);
	free (*folder);
	*folder = next;
	return (GP_OK);
}

static CameraFilesystemFolder*
lookup_folder (
	CameraFilesystem *fs,
	CameraFilesystemFolder *folder, const char *foldername,
	GPContext *context
) {
	CameraFilesystemFolder	*f;
	const char	*curpt = foldername;
	const char	*s;

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Lookup folder '%s'...", foldername);
	while (folder) {
		/* handle multiple slashes, and slashes at the end */
		while (curpt[0]=='/')
			curpt++;
		if (!curpt[0]) {
			gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Found! %s is %p", foldername, folder);
			return folder;
		}

		s = strchr(curpt,'/');
		/* Check if we need to load the folder ... */
		if (folder->folders_dirty) {
			CameraList	*list;
			char		*copy = strdup (foldername);
			int		ret;
			/*
			 * The parent folder is dirty. List the folders in the parent 
			 * folder to make it clean.
			 */
			/* the character _before_ curpt is a /, overwrite it temporary with \0 */
			copy[curpt-foldername] = '\0';
			gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Folder %s is dirty. "
				"Listing folders in there to make folder clean...", copy);
			ret = gp_list_new (&list);
			if (ret == GP_OK) {
				ret = gp_filesystem_list_folders (fs, copy, list, context);
				gp_list_free (list);
				gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Done making folder %s clean...", copy);
			} else {
				gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Making folder %s clean failed: %d", copy, ret);
			}
			free (copy);
		}
		f = folder->folders;
		while (f) {
			if (s) {
				if (!strncmp(f->name,curpt, (s-curpt)) &&
				    (strlen(f->name) == (s-curpt))
				) {
					folder = f;
					curpt = s;
					break;
				}
			} else {
				if (!strcmp(f->name,curpt))
					return f;
			}
			f = f->next;
		}
		folder = f;
	}
	return NULL;
}

static int
lookup_folder_file (
	CameraFilesystem *fs,
	const char *folder, const char *filename,
	CameraFilesystemFolder **xfolder, CameraFilesystemFile **xfile,
	GPContext *context
) {
	CameraFilesystemFolder*	xf;
	CameraFilesystemFile*	f;

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Lookup folder %s file %s", folder, filename);
	xf = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!xf) return GP_ERROR_DIRECTORY_NOT_FOUND;
	/* Check if we need to load the filelist of the folder ... */
	if (xf->files_dirty) {
		CameraList	*list;
		int		ret;
		/*
                 * The folder is dirty. List the files in it to make it clean.
		 */
		gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Folder %s is dirty. "
			"Listing files in there to make folder clean...", folder);
		ret = gp_list_new (&list);
		if (ret == GP_OK) {
			ret = gp_filesystem_list_files (fs, folder, list, context);
			gp_list_free (list);
			gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Done making folder %s clean...", folder);
		}
		if (ret != GP_OK)
			gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Making folder %s clean failed: %d", folder, ret);
	}

	f = xf->files;
	while (f) {
		if (!strcmp (f->name, filename)) {
			*xfile = f;
			*xfolder = xf;
			return GP_OK;
		}
		f = f->next;
	}
	return GP_ERROR_FILE_NOT_FOUND;
}

/* delete all folder content */
static int
recurse_delete_folder (CameraFilesystem *fs, CameraFilesystemFolder *folder) {
	CameraFilesystemFolder  **f;

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Recurse delete folder %p/%s", folder, folder->name);
	f = &folder->folders;
	while (*f) {
		recurse_delete_folder (fs, *f);
		delete_folder (fs, f); /* will also advance to next */
	}
	return (GP_OK);
}

static int
delete_all_folders (CameraFilesystem *fs, const char *foldername,
		    GPContext *context)
{
	CameraFilesystemFolder	*folder;

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Internally deleting "
		"all folders from '%s'...", foldername);

	CHECK_NULL (fs && foldername);
	CC (context);
	CA (foldername, context);

	folder = lookup_folder (fs, fs->rootfolder, foldername, context);
	return recurse_delete_folder (fs, folder);
}

/* create and append 1 new folder entry to the current folder */
static int
append_folder_one (
	CameraFilesystemFolder *folder,
	const char *name,
	CameraFilesystemFolder **newfolder
) {
	CameraFilesystemFolder *f;

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Append one folder %s", name);
	CHECK_MEM (f = calloc(sizeof(CameraFilesystemFolder),1));
	CHECK_MEM (f->name = strdup (name));
	f->files_dirty = 1;
	f->folders_dirty = 1;

	/* Link into the current chain...  perhaps later alphabetically? */
	f->next = folder->folders;
	folder->folders = f;
	if (newfolder) *newfolder = f;
	return (GP_OK);
}

/* This is a mix between lookup and folder creator */
static int
append_to_folder (CameraFilesystemFolder *folder,
	const char *foldername,
	CameraFilesystemFolder **newfolder
) {
	CameraFilesystemFolder	*f;
	char	*s;

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Append to folder %p/%s - %s", folder, folder->name, foldername);
	/* Handle multiple slashes, and slashes at the end */
	while (foldername[0]=='/')
		foldername++;
	if (!foldername[0]) {
		if (newfolder) *newfolder = folder;
		return (GP_OK);
	}

	s = strchr(foldername,'/');
	f = folder->folders;
	while (f) {
		if (s) {
			if (!strncmp(f->name,foldername, (s-foldername)) &&
			    (strlen(f->name) == (s-foldername))
			)
				return append_to_folder (f, s+1, newfolder);
		} else {
			if (!strcmp(f->name,foldername)) {
				if (newfolder) *newfolder = f;
				return (GP_OK);
			}
		}
		f = f->next;
	}
	/* Not found ... create new folder */
	if (s) {
		char *x;
		CHECK_MEM (x = calloc ((s-foldername)+1,1));
		memcpy (x, foldername, (s-foldername));
		x[s-foldername] = 0;
		CR (append_folder_one (folder, x, newfolder));
		free (x);
	} else {
		CR (append_folder_one (folder, foldername, newfolder));
	}
	return (GP_OK);
}

static int
append_folder (CameraFilesystem *fs,
	const char *folder,
	CameraFilesystemFolder **newfolder,
	GPContext *context
) {
	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Appending folder %s...", folder);
	CHECK_NULL (fs);
	CHECK_NULL (folder);
	CC (context);
	CA (folder, context);
	return append_to_folder (fs->rootfolder, folder, newfolder);
}

static int
append_file (CameraFilesystem *fs, CameraFilesystemFolder *folder, CameraFile *file, GPContext *context)
{
	CameraFilesystemFile **new;
	const char *name;

	CHECK_NULL (fs && file);

	CR (gp_file_get_name (file, &name));
	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Appending file %s...", name);


	new = &folder->files;
	while (*new) {
		if (!strcmp((*new)->name, name)) {
			gp_log (GP_LOG_ERROR, "gphoto2-filesystem", "File %s already exists!", name);
			return (GP_ERROR);
		}
		new = &((*new)->next);
	}
	/* new now points to the location of the last ->next pointer,
	 * if we write to it, we set last->next */
	CHECK_MEM ((*new) = calloc (sizeof (CameraFilesystemFile), 1));
	(*new)->name = strdup (name);
	(*new)->info_dirty = 1;
	(*new)->normal = file;
	gp_file_ref (file);
	return (GP_OK);
}

/**
 * \brief Clear the filesystem
 * \param fs the filesystem to be cleared
 *
 * Resets the filesystem. All cached information including the folder tree
 * will get lost and will be queried again on demand.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_reset (CameraFilesystem *fs)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "resetting filesystem");
	CR (gp_filesystem_lru_clear (fs));
	CR (delete_all_folders (fs, "/", NULL));
	if (fs->rootfolder) {
		fs->rootfolder->files_dirty = 1;
		fs->rootfolder->folders_dirty = 1;
	} else {
		gp_log (GP_LOG_ERROR,"gphoto2-filesys", "root folder is gone?");
	}
	return (GP_OK);
}

/**
 * \brief Create a new filesystem struct
 *
 * \param fs a pointer to a #CameraFilesystem
 *
 * Creates a new empty #CameraFilesystem
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_new (CameraFilesystem **fs)
{
	CHECK_NULL (fs);

	CHECK_MEM (*fs = malloc (sizeof (CameraFilesystem)));

	memset(*fs,0,sizeof(CameraFilesystem));

	(*fs)->rootfolder = calloc (sizeof (CameraFilesystemFolder), 1);
	if (!(*fs)->rootfolder) {
		free (*fs);
		return (GP_ERROR_NO_MEMORY);
	}
	(*fs)->rootfolder->name = strdup("/");
	if (!(*fs)->rootfolder->name) {
		free ((*fs)->rootfolder);
		free (*fs);
		return (GP_ERROR_NO_MEMORY);
	}
	(*fs)->rootfolder->files_dirty = 1;
	(*fs)->rootfolder->folders_dirty = 1;
	return (GP_OK);
}

/**
 * \brief Free filesystem struct
 * \param fs a #CameraFilesystem
 *
 * Frees the #CameraFilesystem
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_free (CameraFilesystem *fs)
{
	/* We don't care for success or failure */
	gp_filesystem_reset (fs);

	/* Now, we've only got left over the root folder. Free that and
	 * the filesystem. */
	free (fs->rootfolder->name);
	free (fs->rootfolder);
	free (fs);
	return (GP_OK);
}

/**
 * \brief Append a file to a folder in a filesystem
 * \param fs a #CameraFilesystem
 * \param folder the folder where to put the file in
 * \param filename filename of the file
 * \param context a #GPContext
 *
 * Tells the fs that there is a file called filename in folder
 * called folder. Usually camera drivers will call this function after
 * capturing an image in order to tell the fs about the new file.
 * A front-end should not use this function.
 *
 * \return a gphoto2 error code.
 **/
static int
internal_append (CameraFilesystem *fs, CameraFilesystemFolder *f,
		      const char *filename, GPContext *context)
{
	CameraFilesystemFile **new;

	CHECK_NULL (fs && f);

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Internal append %s to folder %s", filename, f->name);
	/* Check folder for existence, if not, create it. */
	new = &f->files;
	while (*new) {
		if (!strcmp((*new)->name, filename)) break;
		new = &((*new)->next);
	}
	if (*new)
		return (GP_ERROR_FILE_EXISTS);

	CHECK_MEM ((*new) = calloc (sizeof (CameraFilesystemFile), 1))
	(*new)->name = strdup (filename);
	if (!(*new)->name) {
		free (*new);
		*new = NULL;
		return (GP_ERROR_NO_MEMORY);
	}
	(*new)->info_dirty = 1;
	return (GP_OK);
}

int
gp_filesystem_append (CameraFilesystem *fs, const char *folder,
		      const char *filename, GPContext *context)
{
	CameraFilesystemFolder *f;
	int ret;

	CHECK_NULL (fs && folder);
	CC (context);
	CA (folder, context);

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Append %s/%s to filesystem", folder, filename);
	/* Check folder for existence, if not, create it. */
	f = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!f)
		CR (append_folder (fs, folder, &f, context));
	if (f->files_dirty) { /* Need to load folder from driver first ... capture case */
		CameraList	*xlist;
		int ret;

		ret = gp_list_new (&xlist);
		if (ret != GP_OK) return ret;
		ret = gp_filesystem_list_files (fs, folder, xlist, context);
		gp_list_free (xlist);
		if (ret != GP_OK) return ret;
	}
	ret = internal_append (fs, f, filename, context);
	if (ret == GP_ERROR_FILE_EXISTS) /* not an error here ... just in case we add files twice to the list */
		ret = GP_OK;
	return ret;
}


static void
recursive_fs_dump (CameraFilesystemFolder *folder, int depth) {
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*xfile;

	gp_log (GP_LOG_DEBUG, "gphoto2-filesys", "%*sFolder %s", depth, " ", folder->name);

	xfile = folder->files;
	while (xfile) {
		gp_log (GP_LOG_DEBUG, "gphoto2-filesys", "%*s    %s", depth, " ", xfile->name);
		xfile = xfile->next;
	}
	
	f = folder->folders;
	while (f) {
		recursive_fs_dump (f, depth+4);
		f = f->next;
	}
}
/**
 * \brief Dump the current filesystem.
 * \param fs the #CameraFilesystem
 * \return a gphoto error code
 *
 * Internal function to dump the current filesystem.
 */
int
gp_filesystem_dump (CameraFilesystem *fs)
{
	GP_DEBUG("Dumping Filesystem:");
	recursive_fs_dump (fs->rootfolder, 0);
	return (GP_OK);
}

static int
delete_file (CameraFilesystem *fs, CameraFilesystemFolder *folder, CameraFilesystemFile *file)
{
	CameraFilesystemFile **prev;

	gp_filesystem_lru_remove_one (fs, file);
	/* Get rid of cached files */
	if (file->preview) {
		gp_file_unref (file->preview);
		file->preview = NULL;
	}
	if (file->normal) {
		gp_file_unref (file->normal);
		file->normal = NULL;
	}
	if (file->raw) {
		gp_file_unref (file->raw);
		file->raw = NULL;
	}
	if (file->audio) {
		gp_file_unref (file->audio);
		file->audio = NULL;
	}
	if (file->exif) {
		gp_file_unref (file->exif);
		file->exif = NULL;
	}
	if (file->metadata) {
		gp_file_unref (file->metadata);
		file->metadata = NULL;
	}

	prev = &(folder->files);
	while ((*prev) && ((*prev) != file))
		prev = &((*prev)->next);
	if (!*prev)
		return GP_ERROR;
	*prev = file->next;
	file->next = NULL;
	free (file->name);
	free (file);
	return (GP_OK);
}

static int
gp_filesystem_delete_all_one_by_one (CameraFilesystem *fs, const char *folder,
				     GPContext *context)
{
	CameraList *list;
	int count, x;
	const char *name;

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Deleting all 1 by 1 from %s", folder);
	CR (gp_list_new (&list));
	CL (gp_filesystem_list_files (fs, folder, list, context), list);
	CL (count = gp_list_count (list), list);
	for (x = count ; x--; ) {
		CL (gp_list_get_name (list, x, &name), list);
		CL (gp_filesystem_delete_file (fs, folder, name, context),list);
	}
	gp_list_free(list);
	return (GP_OK);
}

/**
 * \brief Delete all files in specified folder.
 *
 * \param fs a #CameraFilesystem
 * \param folder the folder in which to delete all files
 * \param context a #GPContext
 *
 * Deletes all files in the given folder from the fs. If the fs has not
 * been supplied with a delete_all_func, it tries to delete the files
 * one by one using the delete_file_func. If that function has not been
 * supplied neither, an error is returned.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_delete_all (CameraFilesystem *fs, const char *folder,
			  GPContext *context)
{
	int r;
	CameraFilesystemFolder *f;

	CHECK_NULL (fs && folder);
	CC (context);
	CA (folder, context);

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Deleting all from %s", folder);
	/* Make sure this folder exists */
	f = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!f) return (GP_ERROR_DIRECTORY_NOT_FOUND);

	if (!fs->delete_all_func)
		return gp_filesystem_delete_all_one_by_one (fs, folder, context);
	/*
	 * Mark the folder dirty - it could be that an error
	 * happens, and then we don't know which files have been
	 * deleted and which not.
	 */
	f->files_dirty = 1;
	/*
	 * First try to use the delete_all function. If that fails,
	 * fall back to deletion one-by-one.
	 */
	r = fs->delete_all_func (fs, folder, fs->folder_data, context);
	if (r < 0) {
		gp_log (GP_LOG_DEBUG, "gphoto2-filesystem",
			"delete_all failed (%s). Falling back to "
			"deletion one-by-one.",
			gp_result_as_string (r));
		CR (gp_filesystem_delete_all_one_by_one (fs, folder,
							 context));
	} else {
		/* delete from filesystem view too now */
		CR (delete_all_files (fs, f));
	}
	/*
	 * No error happened. We can be sure that all files have been
	 * deleted.
	 */
	f->files_dirty = 0;
	return (GP_OK);
}

/**
 * \brief Get the list of files in a folder
 * \param fs a #CameraFilesystem
 * \param folder a folder of which a file list should be generated
 * \param list a #CameraList where to put the list of files into
 * \param context a #GPContext
 *
 * Lists the files in folder using either cached values or (if there
 * aren't any) the file_list_func which (hopefully) has been previously
 * supplied.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_list_files (CameraFilesystem *fs, const char *folder,
			  CameraList *list, GPContext *context)
{
	int count, y;
	const char *name;
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*file;

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Listing files in %s", folder);

	CHECK_NULL (fs && list && folder);
	CC (context);
	CA (folder, context);

	gp_list_reset (list);

	/* Search the folder */
	f = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!f) return (GP_ERROR_DIRECTORY_NOT_FOUND);

	/* If the folder is dirty, delete the contents and query the camera */
	if (f->files_dirty && fs->file_list_func) {
		gp_log (GP_LOG_DEBUG, "gphoto2-filesystem",
			"Querying folder %s...", folder);
		CR (delete_all_files (fs, f));
		CR (fs->file_list_func (fs, folder, list,
					fs->list_data, context));

		CR (count = gp_list_count (list));
		for (y = 0; y < count; y++) {
			CR (gp_list_get_name (list, y, &name));
			gp_log (GP_LOG_DEBUG, "gphoto2-filesystem",
					 "Added '%s'", name);
			CR (internal_append (fs, f, name, context));
		}
		gp_list_reset (list);
	}

	/* The folder is clean now */
	f->files_dirty = 0;

	file = f->files;
	while (file) {
		gp_log (GP_LOG_DEBUG, "filesys",
			"Listed '%s'", file->name);
		CR (gp_list_append (list, file->name, NULL));
		file = file->next;
	}
	return (GP_OK);
}

/**
 * \brief List all subfolders within a filesystem folder
 * \param fs a #CameraFilesystem
 * \param folder a folder
 * \param list a #CameraList where subfolders should be listed
 * \param context a #GPContext
 *
 * Generates a list of subfolders of the supplied folder either using
 * cached values (if there are any) or the folder_list_func if it has been
 * supplied previously. If not, it is assumed that only a root folder
 * exists (which is the case for many cameras).
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_list_folders (CameraFilesystem *fs, const char *folder,
			    CameraList *list, GPContext *context)
{
	int y, count;
	const char *name;
	CameraFilesystemFolder	*f, *new;

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Listing folders in %s", folder);

	CHECK_NULL (fs && folder && list);
	CC (context);
	CA (folder, context);

	gp_list_reset (list);

	/* Search the folder */
	f = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!f) return (GP_ERROR_DIRECTORY_NOT_FOUND);


	/* If the folder is dirty, query the contents. */
	if (f->folders_dirty && fs->folder_list_func) {
		gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "... is dirty, getting from camera");
		CR (fs->folder_list_func (fs, folder, list,
					  fs->list_data, context));
		CR (delete_all_folders (fs, folder, context));

		CR (count = gp_list_count (list));
		for (y = 0; y < count; y++) {
			CR (gp_list_get_name (list, y, &name));
			CR (append_folder_one (f, name, NULL));
		}
		/* FIXME: why not just return (GP_OK); ? the list should be fine */
		gp_list_reset (list);
	}

	new = f->folders;
	while (new) {
		CR (gp_list_append (list, new->name, NULL));
		new = new->next;
	}
	/* The folder is clean now */
	f->folders_dirty = 0;
	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Folder %s contains %i "
		"subfolders.", folder, gp_list_count (list));
	return (GP_OK);
}

/**
 * \brief Count files a folder of a filesystem.
 * \param fs a #CameraFilesystem
 * \param folder a folder in which to count the files
 * \param context a #GPContext
 *
 * Counts the files in the folder.
 *
 * \return The number of files in the folder or a gphoto2 error code.
 **/
int
gp_filesystem_count (CameraFilesystem *fs, const char *folder,
		     GPContext *context)
{
	int x;
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*file;

	CHECK_NULL (fs && folder);
	CC (context);
	CA (folder, context);

	f = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!f) return (GP_ERROR_DIRECTORY_NOT_FOUND);

	x = 0;
	file = f->files;
	while (file) {
		x++;
		file = file->next;
	}
	return x;
}

/**
 * \brief Delete a file from a folder.
 * \param fs a #CameraFilesystem
 * \param folder a folder in which to delete the file
 * \param filename the name of the file to delete
 * \param context a #GPContext
 *
 * If a delete_file_func has been supplied to the fs, this function will
 * be called and, if this function returns without error, the file will be
 * removed from the fs.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_delete_file (CameraFilesystem *fs, const char *folder,
			   const char *filename, GPContext *context)
{
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*file;

	CHECK_NULL (fs && folder && filename);
	CC (context);
	CA (folder, context);

	/* First of all, do we support file deletion? */
	if (!fs->delete_file_func) {
		gp_context_error (context, _("You have been trying to delete "
			"'%s' from folder '%s', but the filesystem does not "
			"support deletion of files."), filename, folder);
		return (GP_ERROR_NOT_SUPPORTED);
	}

	/* Search the folder and the file */
	CR (lookup_folder_file (fs, folder, filename, &f, &file, context));

	gp_context_status (context, _("Deleting '%s' from folder '%s'..."),
			   filename, folder);
	/* Delete the file */
	CR (fs->delete_file_func (fs, folder, filename,
				  fs->file_data, context));
	CR (delete_file (fs, f, file));
	return (GP_OK);
}

/**
 * \brief Delete a virtal file from a folder in the filesystem
 * \param fs a #CameraFilesystem
 * \param folder a folder in which to delete the file
 * \param filename the name of the file to delete
 * \param context a #GPContext
 *
 * Remove a file from the filesystem. Compared to gp_filesystem_delete_file()
 * this just removes the file from the libgphoto2 view of the filesystem, but
 * does not call the camera driver to delete it from the physical device.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_delete_file_noop (CameraFilesystem *fs, const char *folder,
				const char *filename, GPContext *context)
{
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*file;

	CHECK_NULL (fs && folder && filename);
	CC (context);
	CA (folder, context);
	/* Search the folder and the file */
	CR (lookup_folder_file (fs, folder, filename, &f, &file, context));
	return delete_file (fs, f, file);
}

/**
 * \brief Create a subfolder within a folder
 * \param fs a #CameraFilesystem
 * \param folder the folder in which the directory should be created
 * \param name the name of the directory to be created
 * \param context a #GPContext
 *
 * Creates a new directory called name in given folder.
 *
 * \return a gphoto2 error code
 **/
int
gp_filesystem_make_dir (CameraFilesystem *fs, const char *folder,
			const char *name, GPContext *context)
{
	CameraFilesystemFolder	*f;

	CHECK_NULL (fs && folder && name);
	CC (context);
	CA (folder, context);

	if (!fs->make_dir_func)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Search the folder */
	f = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!f) return (GP_ERROR_DIRECTORY_NOT_FOUND);

	/* Create the directory */
	CR (fs->make_dir_func (fs, folder, name, fs->folder_data, context));
	/* and append to internal fs */
	return append_folder_one (f, name, NULL);
}

/**
 * \brief Remove a subfolder from within a folder
 * \param fs a #CameraFilesystem
 * \param folder the folder in which the directory should be created
 * \param name the name of the directory to be created
 * \param context a #GPContext
 *
 * Removes a directory called name from the given folder.
 *
 * \return a gphoto2 error code
 **/
int
gp_filesystem_remove_dir (CameraFilesystem *fs, const char *folder,
			  const char *name, GPContext *context)
{
	CameraFilesystemFolder *f;
	CameraFilesystemFolder **prev;

	CHECK_NULL (fs && folder && name);
	CC (context);
	CA (folder, context);

	if (!fs->remove_dir_func)
		return (GP_ERROR_NOT_SUPPORTED);

	/*
	 * Make sure there are neither files nor folders in the folder
	 * that is to be removed.
	 */
	f = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!f) return (GP_ERROR_DIRECTORY_NOT_FOUND);
	/* Check if we need to load the folder ... */
	if (f->folders_dirty) {
		CameraList	*list;
		int		ret;
		/*
		 * The owning folder is dirty. List the folders in it 
		 * to make it clean.
		 */
		gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Folder %s is dirty. "
			"Listing folders in there to make folder clean...", folder);
		ret = gp_list_new (&list);
		if (ret == GP_OK) {
			ret = gp_filesystem_list_folders (fs, folder, list, context);
			gp_list_free (list);
			gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Done making folder %s clean...", folder);
		}
	}
	prev = &(f->folders);
	while (*prev) {
		if (!strcmp (name, (*prev)->name))
			break;
		prev = &((*prev)->next);
	}
	if (!*prev) return (GP_ERROR_DIRECTORY_NOT_FOUND);

	if ((*prev)->folders) {
		gp_context_error (context, _("There are still subfolders in "
			"folder '%s/%s' that you are trying to remove."), folder, name);
		return (GP_ERROR_DIRECTORY_EXISTS);
	}
	if ((*prev)->files) {
		gp_context_error (context, _("There are still files in "
			"folder '%s/%s' that you are trying to remove."), folder,name);
		return (GP_ERROR_FILE_EXISTS);
	}

	/* Remove the directory */
	CR (fs->remove_dir_func (fs, folder, name, fs->folder_data, context));
	CR (delete_folder (fs, prev));
	return (GP_OK);
}

/**
 * \brief Upload a file to a folder on the device filesystem
 * \param fs a #CameraFilesystem
 * \param folder the folder where to put the file into
 * \param file the file
 * \param context a #GPContext
 *
 * Uploads a file to the camera if a put_file_func has been previously
 * supplied to the fs. If the upload is successful, the file will get
 * cached in the fs.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_put_file (CameraFilesystem *fs, const char *folder,
			CameraFile *file, GPContext *context)
{
	CameraFilesystemFolder	*f;

	CHECK_NULL (fs && folder && file);
	CC (context);
	CA (folder, context);

	/* Do we support file upload? */
	if (!fs->put_file_func) {
		gp_context_error (context, _("The filesystem does not support "
			"upload of files."));
		return (GP_ERROR_NOT_SUPPORTED);
	}

	/* Search the folder */
	f = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!f) return (GP_ERROR_DIRECTORY_NOT_FOUND);

	/* Upload the file */
	CR (fs->put_file_func (fs, folder, file, fs->folder_data, context));
	/* And upload it to internal structure too */
	return append_file (fs, f, file, context);
}

/**
 * \brief Lookup the filename of an indexed file within a folder.
 * \param fs a #CameraFilesystem
 * \param folder the folder where to look up the file with the filenumber
 * \param filenumber the number of the file
 * \param filename pointer to a filename where the result is stored
 * \param context a #GPContext
 *
 * Looks up the filename of file with given filenumber in given folder.
 * See gp_filesystem_number for exactly the opposite functionality.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_name (CameraFilesystem *fs, const char *folder, int filenumber,
		    const char **filename, GPContext *context)
{
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*file;
	int count;
	CHECK_NULL (fs && folder);
	CC (context);
	CA (folder, context);

	f = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!f) return (GP_ERROR_DIRECTORY_NOT_FOUND);

	file = f->files;
	count = 0;
	while (file) {
		if (filenumber == 0)
			break;
		filenumber--;
		count++;
		file = file->next;
	}

	if (!file) {
		gp_context_error (context, _("Folder '%s' only contains "
			"%i files, but you requested a file with number %i."),
			folder, count, filenumber);
		return (GP_ERROR_FILE_NOT_FOUND);
	}
	*filename = file->name;
	return (GP_OK);
}

/**
 * \brief Get the index of a file in specified folder
 * \param fs a #CameraFilesystem
 * \param folder the folder where to look for file called filename
 * \param filename the file to look for
 * \param context a #GPContext
 *
 * Looks for a file called filename in the given folder. See
 * gp_filesystem_name for exactly the opposite functionality.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_number (CameraFilesystem *fs, const char *folder,
		      const char *filename, GPContext *context)
{
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*file;
	CameraList *list;
	int num;

	CHECK_NULL (fs && folder && filename);
	CC (context);
	CA (folder, context);

	f = lookup_folder (fs, fs->rootfolder, folder, context);
	if (!f) return (GP_ERROR_DIRECTORY_NOT_FOUND);

	file = f->files;
	num = 0;
	while (file) {
		if (!strcmp (file->name, filename))
			return num;
		num++;
		file = file->next;
	}

	/* Ok, we didn't find the file. Is the folder dirty? */
	if (!f->files_dirty) {
		gp_context_error (context, _("File '%s' could not be found "
			"in folder '%s'."), filename, folder);
		return (GP_ERROR_FILE_NOT_FOUND);
	}
	/* The folder is dirty. List all files to make it clean */
	CR (gp_list_new(&list));
	CL (gp_filesystem_list_files (fs, folder, list, context), list);
	gp_list_free(list);
	return (gp_filesystem_number (fs, folder, filename, context));
}

static int
gp_filesystem_scan (CameraFilesystem *fs, const char *folder,
		    const char *filename, GPContext *context)
{
	int count, x;
	CameraList *list;
	const char *name;
	char path[128];

	gp_log (GP_LOG_DEBUG, "gphoto2-filesystem", "Scanning %s for %s...",
		folder, filename);

	CHECK_NULL (fs && folder && filename);
	CC (context);
	CA (folder, context);

	CR (gp_list_new (&list));
	CL (gp_filesystem_list_files (fs, folder, list, context), list);
	CL (count = gp_list_count (list), list);
	for (x = 0; x < count; x++) {
		CL (gp_list_get_name (list, x, &name), list);
		if (!strcmp (filename, name)) {
			gp_list_free (list);
			return (GP_OK);
		}
	}

	CL (gp_filesystem_list_folders (fs, folder, list, context), list);
	CL (count = gp_list_count (list), list);
	for (x = 0; x < count; x++) {
		CL (gp_list_get_name (list, x, &name), list);
		strncpy (path, folder, sizeof (path));
		if (path[strlen (path) - 1] != '/')
			strncat (path, "/", sizeof (path) - strlen (path) - 1);
		strncat (path, name, sizeof (path) - strlen (path) - 1);
		CL (gp_filesystem_scan (fs, path, filename, context), list);
	}
	gp_list_free (list);
	return (GP_OK);
}

static int
recursive_folder_scan (
	CameraFilesystemFolder *folder, const char *lookforfile,
	char **foldername
) {
	CameraFilesystemFile	*file;
	CameraFilesystemFolder	*f;
	int ret;

	file = folder->files;
	while (file) {
		if (!strcmp(file->name, lookforfile)) {
			*foldername = strdup (folder->name);
			return GP_OK;
		}
		file = file->next;
	}
	f = folder->folders;
	while (f) {
		char *xfolder;
		ret = recursive_folder_scan (f, lookforfile, &xfolder);
		if (ret == GP_OK) {
			(*foldername) = malloc (strlen (folder->name) + 1 + strlen (xfolder) + 1);
			strcpy ((*foldername),folder->name);
			strcat ((*foldername),"/");
			strcat ((*foldername),xfolder);
			free (xfolder);
			return GP_OK;
		}
		f = f->next;
	}
	/* thorugh all subfoilders */
	return GP_ERROR_FILE_NOT_FOUND;
}
/**
 * \brief Search a folder that contains a given filename
 * \param fs a #CameraFilesystem
 * \param filename the name of the file to search in the fs
 * \param folder pointer to value where the string is stored in
 * \param context a #GPContext
 *
 * Searches a file called filename in the fs and returns the first
 * occurrency. This functionality is needed for camera drivers that cannot
 * figure out where a file gets created after capturing an image although the
 * name of the image is known. Usually, those drivers will call
 * gp_filesystem_reset in order to tell the fs that something has
 * changed and then gp_filesystem_get_folder in order to find the file.
 *
 * Note that you get a reference to the string stored in the filesystem structure,
 * so do not free it yourself.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_get_folder (CameraFilesystem *fs, const char *filename,
			  char **folder, GPContext *context)
{
	int ret;
	CHECK_NULL (fs && filename && folder);
	CC (context);

	CR (gp_filesystem_scan (fs, "/", filename, context));

	ret = recursive_folder_scan ( fs->rootfolder, filename, folder);
	if (ret == GP_OK) return ret;
	gp_context_error (context, _("Could not find file '%s'."), filename);
	return (GP_ERROR_FILE_NOT_FOUND);
}

/**
 * \brief Set the functions to list folders and files
 * \param fs a #CameraFilesystem
 * \param file_list_func the function that will return listings of files
 * \param folder_list_func the function that will return listings of folders
 * \param data private data structure
 *
 * Tells the fs which functions to use to retrieve listings of folders
 * and/or files. Typically, a camera driver would call this function
 * on initialization. Each function can be NULL indicating that this
 * functionality is not supported. For example, many cameras don't support
 * folders. In this case, you would supply NULL for folder_list_func. Then,
 * the fs assumes that there is only a root folder.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_set_list_funcs (CameraFilesystem *fs,
			      CameraFilesystemListFunc file_list_func,
			      CameraFilesystemListFunc folder_list_func,
			      void *data)
{
	CHECK_NULL (fs);

	fs->file_list_func = file_list_func;
	fs->folder_list_func = folder_list_func;
	fs->list_data = data;

	return (GP_OK);
}

/**
 * \brief Set camera filesystem file related functions
 * \param fs a #CameraFilesystem
 * \param get_file_func the function downloading files
 * \param del_file_func the function deleting files
 * \param data private data structure
 *
 * Tells the fs which functions to use for file download or file deletion.
 * Typically, a camera driver would call this function on initialization.
 * A function can be NULL indicating that this functionality is not supported.
 * For example, if a camera does not support file deletion, you would supply
 * NULL for del_file_func.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_set_file_funcs (CameraFilesystem *fs,
			      CameraFilesystemGetFileFunc get_file_func,
			      CameraFilesystemDeleteFileFunc del_file_func,
			      void *data)
{
	CHECK_NULL (fs);

	fs->delete_file_func = del_file_func;
	fs->get_file_func = get_file_func;
	fs->file_data = data;

	return (GP_OK);
}

/**
 * \brief Set folder related functions of the filesystem
 * \param fs a #CameraFilesystem
 * \param put_file_func function used to upload files
 * \param delete_all_func function used to delete all files in a folder
 * \param make_dir_func function used to create a new directory
 * \param remove_dir_func function used to remove an existing directory
 * \param data a data object that will passed to all called functions
 *
 * Tells the filesystem which functions to call for file upload, deletion
 * of all files in a given folder, creation or removal of a folder.
 * Typically, a camera driver would call this function on initialization.
 * If one functionality is not supported, NULL can be supplied.
 * If you don't call this function, the fs will assume that neither
 * of these features is supported.
 *
 * The fs will try to compensate missing delete_all_func
 * functionality with the delete_file_func if such a function has been
 * supplied.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_set_folder_funcs (CameraFilesystem *fs,
				CameraFilesystemPutFileFunc put_file_func,
				CameraFilesystemDeleteAllFunc delete_all_func,
				CameraFilesystemDirFunc make_dir_func,
				CameraFilesystemDirFunc remove_dir_func,
				void *data)
{
	CHECK_NULL (fs);

	fs->put_file_func = put_file_func;
	fs->delete_all_func = delete_all_func;
	fs->make_dir_func = make_dir_func;
	fs->remove_dir_func = remove_dir_func;
	fs->folder_data = data;

	return (GP_OK);
}

static int
gp_filesystem_get_file_impl (CameraFilesystem *fs, const char *folder,
			     const char *filename, CameraFileType type,
			     CameraFile *file, GPContext *context)
{
	CameraFilesystemFolder	*xfolder;
	CameraFilesystemFile	*xfile;

	CHECK_NULL (fs && folder && file && filename);
	CC (context);
	CA (folder, context);

	GP_DEBUG ("Getting file '%s' from folder '%s' (type %i)...",
		  filename, folder, type);

	CR (gp_file_set_type (file, type));
	CR (gp_file_set_name (file, filename));

	if (!fs->get_file_func) {
		gp_context_error (context,
			_("The filesystem doesn't support getting files"));
		return (GP_ERROR_NOT_SUPPORTED);
	}

	/* Search folder and file */
	CR( lookup_folder_file (fs, folder, filename, &xfolder, &xfile, context));

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		if (xfile->preview)
			return (gp_file_copy (file, xfile->preview));
		break;
	case GP_FILE_TYPE_NORMAL:
		if (xfile->normal)
			return (gp_file_copy (file, xfile->normal));
		break;
	case GP_FILE_TYPE_RAW:
		if (xfile->raw)
			return (gp_file_copy (file, xfile->raw));
		break;
	case GP_FILE_TYPE_AUDIO:
		if (xfile->audio)
			return (gp_file_copy (file, xfile->audio));
		break;
	case GP_FILE_TYPE_EXIF:
		if (xfile->exif)
			return (gp_file_copy (file, xfile->exif));
		break;
	case GP_FILE_TYPE_METADATA:
		if (xfile->metadata)
			return (gp_file_copy (file, xfile->metadata));
		break;
	default:
		gp_context_error (context, _("Unknown file type %i."), type);
		return (GP_ERROR);
	}

	gp_context_status (context, _("Downloading '%s' from folder '%s'..."),
			   filename, folder);
	CR (fs->get_file_func (fs, folder, filename, type, file,
			       fs->file_data, context));

	/* We don't trust the camera drivers */
	CR (gp_file_set_type (file, type));
	CR (gp_file_set_name (file, filename));

#if 0
	/* this disables LRU completely. */
	/* Cache this file */
	CR (gp_filesystem_set_file_noop (fs, folder, file, context));
#endif

	/*
	 * Often, thumbnails are of a different mime type than the normal
	 * picture. In this case, we should rename the file.
	 */
	if (type != GP_FILE_TYPE_NORMAL)
		CR (gp_file_adjust_name_for_mime_type (file));

	return (GP_OK);
}

/**
 * \brief Get file data from the filesystem
 * \param fs a #CameraFilesystem
 * \param folder the folder in which the file can be found
 * \param filename the name of the file to download
 * \param type the type of the file
 * \param file the file that receives the data
 * \param context a #GPContext
 *
 * Downloads the file called filename from the folder using the
 * get_file_func if such a function has been previously supplied. If the
 * file has been previously downloaded, the file is retrieved from cache.
 * The result is stored in the passed file structure.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_get_file (CameraFilesystem *fs, const char *folder,
			const char *filename, CameraFileType type,
			CameraFile *file, GPContext *context)
{
	int r;
#ifdef HAVE_LIBEXIF
	CameraFile *efile;
	const char *data = NULL;
	unsigned char *buf;
	unsigned int buf_size;
	unsigned long int size = 0;
	ExifData *ed;
#endif

	r = gp_filesystem_get_file_impl (fs, folder, filename, type,
					 file, context);

	if ((r == GP_ERROR_NOT_SUPPORTED) &&
	    (type == GP_FILE_TYPE_PREVIEW)) {

		/*
		 * Could not get preview (unsupported operation). Some
		 * cameras hide the thumbnail in EXIF data. Check it out.
		 */
#ifdef HAVE_LIBEXIF
		GP_DEBUG ("Getting previews is not supported. Trying "
			  "EXIF data...");
		CR (gp_file_new (&efile));
		CU (gp_filesystem_get_file_impl (fs, folder, filename,
				GP_FILE_TYPE_EXIF, efile, context), efile);
		CU (gp_file_get_data_and_size (efile, &data, &size), efile);
		ed = exif_data_new_from_data ((unsigned char*)data, size);
		gp_file_unref (efile);
		if (!ed) {
			GP_DEBUG ("Could not parse EXIF data of "
				"'%s' in folder '%s'.", filename, folder);
			return (GP_ERROR_CORRUPTED_DATA);
		}
		if (!ed->data) {
			GP_DEBUG ("EXIF data does not contain a thumbnail.");
			exif_data_unref (ed);
			return (r);
		}

		/*
		 * We found a thumbnail in EXIF data! Those
		 * thumbnails are always JPEG. Set up the file.
		 */
		r = gp_file_set_data_and_size (file, (char*)ed->data, ed->size);
		if (r < 0) {
			exif_data_unref (ed);
			return (r);
		}
		ed->data = NULL;
		ed->size = 0;
		exif_data_unref (ed);
		CR (gp_file_set_type (file, GP_FILE_TYPE_PREVIEW));
		CR (gp_file_set_name (file, filename));
		CR (gp_file_set_mime_type (file, GP_MIME_JPEG));
		CR (gp_filesystem_set_file_noop (fs, folder, file, context));
		CR (gp_file_adjust_name_for_mime_type (file));
#else
		GP_DEBUG ("Getting previews is not supported and "
			"libgphoto2 has been compiled without exif "
			"support. ");
		return (r);
#endif
	} else if ((r == GP_ERROR_NOT_SUPPORTED) &&
		   (type == GP_FILE_TYPE_EXIF)) {

		/*
		 * Some cameras hide EXIF data in thumbnails (!). Check it
		 * out.
		 */
#ifdef HAVE_LIBEXIF
		GP_DEBUG ("Getting EXIF data is not supported. Trying "
			  "thumbnail...");
		CR (gp_file_new (&efile));
		CU (gp_filesystem_get_file_impl (fs, folder, filename,
				GP_FILE_TYPE_PREVIEW, efile, context), efile);
		CU (gp_file_get_data_and_size (efile, &data, &size), efile);
		ed = exif_data_new_from_data ((unsigned char*)data, size);
		gp_file_unref (efile);
		if (!ed) {
			GP_DEBUG ("Could not parse EXIF data of thumbnail of "
				"'%s' in folder '%s'.", filename, folder);
			return (GP_ERROR_CORRUPTED_DATA);
		}
		exif_data_save_data (ed, &buf, &buf_size);
		exif_data_unref (ed);
		r = gp_file_set_data_and_size (file, (char*)buf, buf_size);
		if (r < 0) {
			free (buf);
			return (r);
		}
		CR (gp_file_set_type (file, GP_FILE_TYPE_EXIF));
		CR (gp_file_set_name (file, filename));
		CR (gp_file_set_mime_type (file, GP_MIME_EXIF));
		CR (gp_filesystem_set_file_noop (fs, folder, file, context));
		CR (gp_file_adjust_name_for_mime_type (file));
#else
		GP_DEBUG ("Getting EXIF data is not supported and libgphoto2 "
			"has been compiled without EXIF support.");
		return (r);
#endif
	} else if (r < 0) {
		GP_DEBUG ("Download of '%s' from '%s' (type %i) failed. "
			"Reason: '%s'", filename, folder, type,
			gp_result_as_string (r));
		return (r);
	}

	return (GP_OK);
}

/**
 * \brief Set file information functions
 * \param fs a #CameraFilesystem
 * \param get_info_func the function to retrieve file information
 * \param set_info_func the function to set file information
 * \param data private data
 *
 * Tells the filesystem which functions to call when file information
 * about a file should be retrieved or set. Typically, this function will
 * get called by the camera driver on initialization.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_set_info_funcs (CameraFilesystem *fs,
			      CameraFilesystemGetInfoFunc get_info_func,
			      CameraFilesystemSetInfoFunc set_info_func,
			      void *data)
{
	CHECK_NULL (fs);

	fs->get_info_func = get_info_func;
	fs->set_info_func = set_info_func;
	fs->info_data = data;

	return (GP_OK);
}

/**
 * \brief Set all filesystem related function pointers
 * \param fs a #CameraFilesystem
 * \param funcs pointer to a struct of filesystem functions
 * \param data private data
 *
 * Tells the filesystem which functions to call for camera/filesystem specific
 * functions, like listing, retrieving, uploading files and so on.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_set_funcs	(CameraFilesystem *fs,
			 CameraFilesystemFuncs *funcs,
			 void *data)
{
	CHECK_NULL (fs);

	fs->get_info_func	= funcs->get_info_func;
	fs->set_info_func	= funcs->set_info_func;
	fs->info_data = data;

	fs->put_file_func	= funcs->put_file_func;
	fs->delete_all_func	= funcs->delete_all_func;
	fs->make_dir_func	= funcs->make_dir_func;
	fs->remove_dir_func	= funcs->remove_dir_func;
	fs->folder_data = data;

	fs->file_list_func	= funcs->file_list_func;
	fs->folder_list_func	= funcs->folder_list_func;
	fs->list_data = data;

	fs->delete_file_func	= funcs->del_file_func;
	fs->get_file_func	= funcs->get_file_func;
	fs->file_data = data;

	fs->storage_info_func	= funcs->storage_info_func;
	return (GP_OK);
}

/**
 * \brief Get information about the specified file
 * \param fs a #CameraFilesystem
 * \param folder the folder that contains the file
 * \param filename the filename
 * \param info pointer to #CameraFileInfo that receives the information
 * \param context a #GPContext
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_get_info (CameraFilesystem *fs, const char *folder,
			const char *filename, CameraFileInfo *info,
			GPContext *context)
{
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*file;
#ifdef HAVE_LIBEXIF
	time_t t;
#endif

	CHECK_NULL (fs && folder && filename && info);
	CC (context);
	CA (folder, context);

	GP_DEBUG ("Getting information about '%s' in '%s'...", filename,
		  folder);

	if (!fs->get_info_func) {
		gp_context_error (context,
			_("The filesystem doesn't support getting file "
			  "information"));
		return (GP_ERROR_NOT_SUPPORTED);
	}

	/* Search folder and file and get info if needed */
	CR ( lookup_folder_file (fs, folder, filename, &f, &file, context));

	if (file->info_dirty) {
		CR (fs->get_info_func (fs, folder, filename,
						&file->info,
						fs->info_data, context));
		file->info_dirty = 0;
	}

	/*
	 * If we didn't get GP_FILE_INFO_MTIME, we'll have a look if we
	 * can get it from EXIF data.
	 */
#ifdef HAVE_LIBEXIF
	if (!(file->info.file.fields & GP_FILE_INFO_MTIME)) {
		GP_DEBUG ("Did not get mtime. Trying EXIF information...");
		t = gp_filesystem_get_exif_mtime (fs, folder, filename);
		if (t) {
			file->info.file.mtime = t;
			file->info.file.fields |= GP_FILE_INFO_MTIME;
		}
	}
#endif
	memcpy (info, &file->info, sizeof (CameraFileInfo));
	return (GP_OK);
}

static int
gp_filesystem_lru_clear (CameraFilesystem *fs)
{
	int n = 0;
	CameraFilesystemFile *ptr, *prev;

	GP_DEBUG ("Clearing fscache LRU list...");

	if (fs->lru_first == NULL) {
		GP_DEBUG ("fscache LRU list already empty");
		return (GP_OK);
	}

	ptr = prev = fs->lru_first;
	while (ptr != NULL) {
		n++;
		if (ptr->lru_prev != prev) {
			GP_DEBUG ("fscache LRU list corrupted (%i)", n);
			return (GP_ERROR);
		}
		prev = ptr;
		ptr = ptr->lru_next;

		prev->lru_prev = NULL;
		prev->lru_next = NULL;
	}

	fs->lru_first = NULL;
	fs->lru_last = NULL;
	fs->lru_size = 0;

	GP_DEBUG ("fscache LRU list cleared (removed %i items)", n);

	return (GP_OK);
}

static int
gp_filesystem_lru_remove_one (CameraFilesystem *fs, CameraFilesystemFile *item)
{
	if (item->lru_prev == NULL)
		return (GP_ERROR);

	/* Update the prev and next pointers. */
	if (item->lru_prev) item->lru_prev->lru_next = item->lru_next;
	if (item->lru_next) item->lru_next->lru_prev = item->lru_prev;
	if (fs->lru_last == item) {
		if (fs->lru_first == item) {

			/*
			 * Case 1: ITEM is the only one in the list. We'll
			 * remove it, and the list will be empty afterwards.
			 */
			fs->lru_last  = NULL;
			fs->lru_first = NULL;
		} else {

			/* Case 2: ITEM is the last in the list. */
			fs->lru_last = item->lru_prev;
		}
	} else if (fs->lru_first == item) {

		/* Case 3: ITEM is the first in the list. */
		fs->lru_first = item->lru_next;
		/* the first item prev links back to itself */
		fs->lru_first->lru_prev = fs->lru_first;
	}

	/* Clear the pointers */
	item->lru_prev = NULL;
	item->lru_next = NULL;

	return (GP_OK);
}

static int
gp_filesystem_lru_free (CameraFilesystem *fs)
{
	CameraFilesystemFile *ptr;
	unsigned long int size;

	CHECK_NULL (fs && fs->lru_first);

	ptr = fs->lru_first;

	GP_DEBUG ("Freeing cached data for file '%s'...", ptr->name);

	/* Remove it from the list. */
	fs->lru_first = ptr->lru_next;
	if (fs->lru_first)
		fs->lru_first->lru_prev = fs->lru_first;
	else
		fs->lru_last = NULL;

	/* Free its content. */
	if (ptr->normal) {
		CR( gp_file_get_data_and_size (ptr->normal, NULL, &size));
		fs->lru_size -= size;
		gp_file_unref (ptr->normal);
		ptr->normal = NULL;
	}
	if (ptr->raw) {
		CR( gp_file_get_data_and_size (ptr->raw, NULL, &size));
		fs->lru_size -= size;
		gp_file_unref (ptr->raw);
		ptr->raw = NULL;
	}
	if (ptr->audio) {
		CR( gp_file_get_data_and_size (ptr->audio, NULL, &size));
		fs->lru_size -= size;
		gp_file_unref (ptr->audio);
		ptr->audio = NULL;
	}
	ptr->lru_next = ptr->lru_prev = NULL;
	return (GP_OK);
}

static int
gp_filesystem_lru_count (CameraFilesystem *fs)
{
	CameraFilesystemFile *ptr;
	int count = 0;

	if (!fs) return 0;
	ptr = fs->lru_first;
	while (ptr) {
		if (ptr->normal || ptr->raw || ptr->audio)
			count++;
		ptr = ptr->lru_next;
	}
	return count;
}

static int
gp_filesystem_lru_update (CameraFilesystem *fs, const char *folder,
			  CameraFile *file, GPContext *context)
{
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*xfile;
	CameraFileType type;
	CameraFile *oldfile = NULL;
	const char *filename;
	unsigned long int size;
	int x;
	char cached_images[1024];

	CHECK_NULL (fs && folder && file);

	CR (gp_file_get_name (file, &filename));
	CR (gp_file_get_type (file, &type));
	CR (gp_file_get_data_and_size (file, NULL, &size));

	/*
	 * The following is a very simple case which is used to prune
	 * the LRU. We keep PICTURES_TO_KEEP pictures in the LRU.
	 *
	 * We have 2 main scenarios:
	 *	- query all thumbnails (repeatedly) ... they are cached and
	 *	  are not pruned by lru free.
	 *	- download all images, linear.	no real need for caching.
	 *	- skip back 1 image (in viewers) (really? I don't know.)
	 *
	 * So lets just keep 2 pictures in memory.
	 */
	if (pictures_to_keep == -1) {
		if (gp_setting_get ("libgphoto", "cached-images", cached_images) == GP_OK) {
			pictures_to_keep = atoi(cached_images);
		} else {
			/* store a default setting */
			sprintf (cached_images, "%d", PICTURES_TO_KEEP);
			gp_setting_set ("libgphoto", "cached-images", cached_images);
		}
	}

	if (pictures_to_keep < 0) /* also sanity check, but no upper limit. */
		pictures_to_keep = PICTURES_TO_KEEP;

	x = gp_filesystem_lru_count (fs);
	while (x > pictures_to_keep) {
		CR (gp_filesystem_lru_free (fs));
		x = gp_filesystem_lru_count (fs);
	}

	GP_DEBUG ("Adding file '%s' from folder '%s' to the fscache LRU list "
		  "(type %i)...", filename, folder, type);

	/* Search folder and file */
	CR (lookup_folder_file (fs, folder, filename, &f, &xfile, context));

	/*
	 * If the file is already in the lru, we first remove it. Note that
	 * we will only remove 'normal', 'raw' and 'audio' from cache.
	 * See gp_filesystem_lru_free.
	 */
	if (xfile->lru_prev != NULL) {
		switch (type) {
		case GP_FILE_TYPE_NORMAL:
			oldfile = xfile->normal;
			break;
		case GP_FILE_TYPE_RAW:
			oldfile = xfile->raw;
			break;
		case GP_FILE_TYPE_AUDIO:
			oldfile = xfile->audio;
			break;
		case GP_FILE_TYPE_PREVIEW:
		case GP_FILE_TYPE_EXIF:
		case GP_FILE_TYPE_METADATA:
			break;
		default:
			gp_context_error (context, _("Unknown file type %i."),
					  type);
			return (GP_ERROR);
		}
		if (oldfile) {
			CR( gp_file_get_data_and_size (oldfile, NULL, &size));
			fs->lru_size -= size;
		}

		CR (gp_filesystem_lru_remove_one (fs, xfile));
	}

	/* Then add the file at the end of the LRU. */
	if (fs->lru_first == NULL) {
		fs->lru_first = xfile;
		fs->lru_last = xfile;

		/*
		 * For the first item, prev point it itself to show that the
		 * item is in the list.
		 */
		xfile->lru_prev = xfile;

	} else {
		xfile->lru_next = NULL;
		xfile->lru_prev = fs->lru_last;
		fs->lru_last->lru_next = xfile;
		fs->lru_last = xfile;
	}

	CR( gp_file_get_data_and_size (file, NULL, &size));
	fs->lru_size += size;

	GP_DEBUG ("File '%s' from folder '%s' added in fscache LRU list.",
		  filename, folder);

	return (GP_OK);
}

static int
gp_filesystem_lru_check (CameraFilesystem *fs)
{
	int n = 0;
	CameraFilesystemFile *ptr, *prev;

	GP_DEBUG ("Checking fscache LRU list integrity...");

	if (fs->lru_first == NULL) {
		GP_DEBUG ("fscache LRU list empty");
		return (GP_OK);
	}

	ptr = prev = fs->lru_first;
	while (ptr != NULL) {
		n++;
		if (ptr->lru_prev != prev) {
			GP_DEBUG ("fscache LRU list corrupted (%i)", n);
			return (GP_ERROR);
		}
		prev = ptr;
		ptr = ptr->lru_next;
	}

	GP_DEBUG ("fscache LRU list ok with %i items (%ld bytes)", n,
		  fs->lru_size);

	return (GP_OK);
}

/**
 * \brief Attach file content to a specified file.
 *
 * \param fs a #CameraFilesystem
 * \param folder a folder in the filesystem
 * \param file a #CameraFile
 * \param context: a #GPContext
 *
 * Tells the fs about a file. Typically, camera drivers will call this
 * function in case they get information about a file (i.e. preview) "for free"
 * on gp_camera_capture() or gp_camera_folder_list_files().
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_set_file_noop (CameraFilesystem *fs, const char *folder,
			     CameraFile *file, GPContext *context)
{
	CameraFileType type;
	CameraFileInfo info;
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*xfile;
	const char *filename;
	int r;
	time_t t;

	CHECK_NULL (fs && folder && file);
	CC (context);
	CA (folder, context);

	CR (gp_file_get_name (file, &filename));
	CR (gp_file_get_type (file, &type));
	GP_DEBUG ("Adding file '%s' to folder '%s' (type %i)...",
		  filename, folder, type);

	/* Search folder and file */
	CR (lookup_folder_file (fs, folder, filename, &f, &xfile, context));

	/*
	 * If we add a significant amount of data in the cache, we put (or
	 * move) a reference to this file in the LRU linked list. We
	 * assume that only GP_FILE_TYPE_[RAW,NORMAL,EXIF] will contain a
	 * significant amount of data.
	 */
	if ((type == GP_FILE_TYPE_RAW) || (type == GP_FILE_TYPE_NORMAL) ||
	    (type == GP_FILE_TYPE_AUDIO))
		CR (gp_filesystem_lru_update (fs, folder, file, context));

	/* Redundant sanity check. */
	CR (gp_filesystem_lru_check (fs));

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		if (xfile->preview)
			gp_file_unref (xfile->preview);
		xfile->preview = file;
		gp_file_ref (file);
		break;
	case GP_FILE_TYPE_NORMAL:
		if (xfile->normal)
			gp_file_unref (xfile->normal);
		xfile->normal = file;
		gp_file_ref (file);
		break;
	case GP_FILE_TYPE_RAW:
		if (xfile->raw)
			gp_file_unref (xfile->raw);
		xfile->raw = file;
		gp_file_ref (file);
		break;
	case GP_FILE_TYPE_AUDIO:
		if (xfile->audio)
			gp_file_unref (xfile->audio);
		xfile->audio = file;
		gp_file_ref (file);
		break;
	case GP_FILE_TYPE_EXIF:
		if (xfile->exif)
			gp_file_unref (xfile->exif);
		xfile->exif = file;
		gp_file_ref (file);
		break;
	case GP_FILE_TYPE_METADATA:
		if (xfile->metadata)
			gp_file_unref (xfile->metadata);
		xfile->metadata = file;
		gp_file_ref (file);
		break;
	default:
		gp_context_error (context, _("Unknown file type %i."), type);
		return (GP_ERROR);
	}

	/*
	 * If we didn't get a mtime, try to get it from the CameraFileInfo.
	 */
	CR (gp_file_get_mtime (file, &t));
	if (!t) {
		GP_DEBUG ("File does not contain mtime. Trying "
			  "information on the file...");
		r = gp_filesystem_get_info (fs, folder, filename, &info, NULL);
		if ((r == GP_OK) && (info.file.fields & GP_FILE_INFO_MTIME))
			t = info.file.mtime;
	}

	/*
	 * If we still don't have the mtime and this is a normal
	 * file, check if there is EXIF data in the file that contains
	 * information on the mtime.
	 */
#ifdef HAVE_LIBEXIF
	if (!t && (type == GP_FILE_TYPE_NORMAL)) {
		unsigned long int size;
		const char *data;

		GP_DEBUG ("Searching data for mtime...");
		CR (gp_file_get_data_and_size (file, NULL, &size));
		if (size < 32*1024*1024) { /* just assume stuff above 32MB is not EXIF capable */
			/* FIXME: Will leak on "fd" retrieval */
			CR (gp_file_get_data_and_size (file, &data, &size));
			t = get_exif_mtime ((unsigned char*)data, size);
		}
	}
	/*
	 * Still no mtime? Let's see if the camera offers us data of type
	 * GP_FILE_TYPE_EXIF that includes information on the mtime.
	 */
	if (!t) {
		GP_DEBUG ("Trying EXIF information...");
		t = gp_filesystem_get_exif_mtime (fs, folder, filename);
	}
#endif

	if (t)
		CR (gp_file_set_mtime (file, t));

	return (GP_OK);
}

/**
 * \brief Store the file information in the virtual fs
 * \param fs a #CameraFilesystem
 * \param folder the foldername
 * \param info the #CameraFileInfo to store
 * \param context a #GPContext
 *
 * In contrast to #gp_filesystem_set_info, #gp_filesystem_set_info_noop
 * will only change the file information in the fs. Typically, camera
 * drivers will use this function in case they get file information "for free"
 * on #gp_camera_capture or #gp_camera_folder_list_files.
 *
 * \return a gphoto2 error code
 **/
int
gp_filesystem_set_info_noop (CameraFilesystem *fs, const char *folder,
			     CameraFileInfo info, GPContext *context)
{
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*xfile;

	CHECK_NULL (fs && folder);
	CC (context);
	CA (folder, context);

	/* Search folder and file */
	CR (lookup_folder_file (fs, folder, info.file.name, &f, &xfile, context));

	memcpy (&xfile->info, &info, sizeof (CameraFileInfo));
	xfile->info_dirty = 0;
	return (GP_OK);
}

/**
 * \brief Set information about a file
 * \param fs a #CameraFilesystem
 * \param folder foldername where the file resides
 * \param filename the files name
 * \param info the #CameraFileInfo to set
 * \param context a #GPContext
 *
 * Sets information about a file in the camera.
 *
 * \return a gphoto2 error code.
 **/
int
gp_filesystem_set_info (CameraFilesystem *fs, const char *folder,
			const char *filename, CameraFileInfo info,
			GPContext *context)
{
	int result, name, e;
	CameraFilesystemFolder	*f;
	CameraFilesystemFile	*xfile;

	CHECK_NULL (fs && folder && filename);
	CC (context);
	CA (folder, context);

	if (!fs->set_info_func) {
		gp_context_error (context,
			_("The filesystem doesn't support setting file "
			  "information"));
		return (GP_ERROR_NOT_SUPPORTED);
	}

	/* Search folder and file */
	CR (lookup_folder_file (fs, folder, filename, &f, &xfile, context));

	/* Check if people want to set read-only attributes */
	if ((info.file.fields    & GP_FILE_INFO_TYPE)   ||
	    (info.file.fields    & GP_FILE_INFO_SIZE)   ||
	    (info.file.fields    & GP_FILE_INFO_WIDTH)  ||
	    (info.file.fields    & GP_FILE_INFO_HEIGHT) ||
	    (info.file.fields    & GP_FILE_INFO_STATUS) ||
	    (info.preview.fields & GP_FILE_INFO_TYPE)   ||
	    (info.preview.fields & GP_FILE_INFO_SIZE)   ||
	    (info.preview.fields & GP_FILE_INFO_WIDTH)  ||
	    (info.preview.fields & GP_FILE_INFO_HEIGHT) ||
	    (info.preview.fields & GP_FILE_INFO_STATUS) ||
	    (info.audio.fields   & GP_FILE_INFO_TYPE)   ||
	    (info.audio.fields   & GP_FILE_INFO_SIZE)   ||
	    (info.audio.fields   & GP_FILE_INFO_STATUS)) {
		gp_context_error (context, _("Read-only file attributes "
			"like width and height can not be changed."));
		return (GP_ERROR_BAD_PARAMETERS);
	}

	/*
	 * Set the info. If anything goes wrong, mark info as dirty,
	 * because the operation could have been partially successful.
	 *
	 * Handle name changes in a separate round.
	 */
	name = (info.file.fields & GP_FILE_INFO_NAME);
	info.file.fields &= ~GP_FILE_INFO_NAME;
	result = fs->set_info_func (fs, folder, filename, info, fs->info_data,
				    context);
	if (result < 0) {
		xfile->info_dirty = 1;
		return (result);
	}
	if (info.file.fields & GP_FILE_INFO_PERMISSIONS)
		xfile->info.file.permissions = info.file.permissions;

	/* Handle name change */
	if (name) {
		char *xname;
		/* Make sure the file does not exist */
		e = gp_filesystem_number (fs, folder, info.file.name, context);
		if (e != GP_ERROR_FILE_NOT_FOUND)
			return (e);

		info.preview.fields = GP_FILE_INFO_NONE;
		info.file.fields = GP_FILE_INFO_NAME;
		info.audio.fields = GP_FILE_INFO_NONE;
		CR (fs->set_info_func (fs, folder, filename, info,
				       fs->info_data, context));
		strncpy (xfile->info.file.name, info.file.name,
			 sizeof (xfile->info.file.name));
		xname = strdup(info.file.name);
		if (xname) {
			free (xfile->name);
			xfile->name = xname;
		}
	}

	return (GP_OK);
}

/**
 * \brief Get the storage information about this filesystem
 * \param fs the filesystem
 * \param storageinfo Pointer to receive a pointer to/array of storage info items
 * \param nrofstorageinfos Pointer to receive number of array entries
 * \param context a #GPContext
 *
 * This function is only called from gp_camera_get_storageinfo(). You may
 * want to make sure this information is consistent with the information on
 * gp_camera_get_storageinfo().
 *
 * Retrieves the storage information, like maximum and free space, for
 * the specified filesystem, if supported by the device. The storage
 * information is returned in an newly allocated array of
 * #CameraStorageInformation objects, to which the pointer pointed to
 * by #storageinfo will be set.
 *
 * The variable pointed to by #nrofstorageinfos will be set to the
 * number of elements in that array.
 *
 * It is the caller's responsibility to free the memory of the array.
 *
 * \return a gphoto error code
 **/
int
gp_filesystem_get_storageinfo (
	CameraFilesystem *fs,
	CameraStorageInformation **storageinfo,
	int *nrofstorageinfos,
	GPContext *context
) {
	CHECK_NULL (fs && storageinfo && nrofstorageinfos);
	CC (context);

	if (!fs->storage_info_func) {
		gp_context_error (context,
			_("The filesystem doesn't support getting storage "
			  "information"));
		return (GP_ERROR_NOT_SUPPORTED);
	}
	return fs->storage_info_func (fs,
		storageinfo, nrofstorageinfos,
		fs->info_data, context);
}
