/** \file gphoto2-abilities-list.c
 * \brief List of supported camera models including their abilities.
 *
 * \author Copyright 2000 Scott Fritzinger
 *
 * \par
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#define _DARWIN_C_SOURCE

#include "config.h"
#include <gphoto2/gphoto2-abilities-list.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ltdl.h>

#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-locking.h>

#include "libgphoto2/i18n.h"


/** \internal */
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

/** \internal */
struct _CameraAbilitiesList {
	int count;
	int maxcount;
	CameraAbilities *abilities;
};

/** \internal */
static int gp_abilities_list_lookup_id (CameraAbilitiesList *, const char *);
/** \internal */
static int gp_abilities_list_sort      (CameraAbilitiesList *);

/**
 * \brief Set the current character codeset libgphoto2 is operating in.
 *
 * Set the codeset for all messages returned by libgphoto2.
 *
 * \param codeset New codeset for the messages. For instance "utf-8".
 * \return old codeset as returned from bind_textdomain_codeset().
 */
const char*
gp_message_codeset (const char *codeset)
{
	gp_port_message_codeset (codeset);
	return bind_textdomain_codeset (GETTEXT_PACKAGE_LIBGPHOTO2, codeset);
}

/**
 * \brief Initialize the localedir directory for the libgphoto2 gettext domain
 *
 * Override the localedir directory libgphoto2 uses for its message
 * translations.
 *
 * You only need to call this if you have a non-standard installation
 * where the locale files are at a location which differs from the
 * compiled in default location.
 *
 * If you do need to call this function, call it before calling any
 * non-initialization function.
 *
 * Internally, this will make sure bindtextdomain() is called for the
 * relevant gettext text domain(s).
 *
 * \param localedir Root directory of libgphoto2's localization files.
 *                  If NULL, use the compiled in default value, which
 *                  will be something like "/usr/share/locale".
 * \return gphoto2 error code.
 */
int
gp_init_localedir (const char *localedir)
{
	static int locale_initialized = 0;
	if (locale_initialized) {
		gp_log(GP_LOG_DEBUG, "gp_init_localedir",
		       "ignoring late call (localedir value %s)",
		       localedir?localedir:"NULL");
		return GP_OK;
	}
	const int gpp_result = gp_port_init_localedir (localedir);
	if (gpp_result != GP_OK) {
		return gpp_result;
	}
	const char *actual_localedir = (localedir?localedir:LOCALEDIR);
	const char *const gettext_domain = GETTEXT_PACKAGE_LIBGPHOTO2;
	if (bindtextdomain (gettext_domain, actual_localedir) == NULL) {
		if (errno == ENOMEM)
			return GP_ERROR_NO_MEMORY;
		return GP_ERROR;
	}
	gp_log(GP_LOG_DEBUG, "gp_init_localedir",
	       "localedir has been set to %s%s",
	       actual_localedir,
	       localedir?"":" (compile-time default)");
	locale_initialized = 1;
	return GP_OK;
}

/**
 * \brief Allocate the memory for a new abilities list.
 *
 * Function to allocate the memory for a new abilities list.
 * \param list CameraAbilitiesList object to initialize
 * \return gphoto2 error code
 *
 * You would then call gp_abilities_list_load() in order to
 * populate it.
 */
int
gp_abilities_list_new (CameraAbilitiesList **list)
{
	C_PARAMS (list);

	/*
	 * We do this here because everybody needs to call this function
	 * first before accessing a camera. Pretty ugly, but I don't know
	 * an other way without introducing a global initialization
	 * function...
	 */
	gp_init_localedir (NULL);

	C_MEM (*list = calloc (1, sizeof (CameraAbilitiesList)));

	return (GP_OK);
}

/**
 * \brief Free the given CameraAbilitiesList object.
 *
 * \param list a CameraAbilitiesList
 * \return a gphoto2 error code
 */
int
gp_abilities_list_free (CameraAbilitiesList *list)
{
	C_PARAMS (list);

	CHECK_RESULT (gp_abilities_list_reset (list));

	free (list);

	return (GP_OK);
}


typedef struct {
	CameraList *list;
	int result;
} foreach_data_t;


static int
foreach_func (const char *filename, lt_ptr data)
{
	foreach_data_t *fd = data;
	CameraList *list = fd->list;

	GP_LOG_D ("Found '%s'.", filename);
	fd->result = gp_list_append (list, filename, NULL);

	return ((fd->result == GP_OK)?0:1);
}

static int
unlocked_gp_abilities_list_load_dir (CameraAbilitiesList *list, const char *dir,
			    GPContext *context)
{
	CameraLibraryIdFunc id;
	CameraLibraryAbilitiesFunc ab;
	CameraText text;
	int ret, x, old_count, new_count;
	int i, p;
	const char *filename;
	CameraList *flist;
	int count;
	lt_dlhandle lh;

	C_PARAMS (list && dir);

	GP_LOG_D ("Using ltdl to load camera libraries from '%s'...", dir);
	CHECK_RESULT (gp_list_new (&flist));
	ret = gp_list_reset (flist);
	if (ret < GP_OK) {
		gp_list_free (flist);
		return ret;
	}
	if (1) { /* a new block in which we can define a temporary variable */
		foreach_data_t foreach_data = { NULL, GP_OK };
		foreach_data.list = flist;
		lt_dlinit ();
		lt_dladdsearchdir (dir);
		ret = lt_dlforeachfile (dir, foreach_func, &foreach_data);
		lt_dlexit ();
		if (ret != 0) {
			gp_list_free (flist);
			GP_LOG_E ("Internal error looking for camlibs (%d)", ret);
			gp_context_error (context,
					  _("Internal error looking for camlibs. "
					    "(path names too long?)"));
			return (foreach_data.result!=GP_OK)?foreach_data.result:GP_ERROR;
		}
	}
	count = gp_list_count (flist);
	if (count < GP_OK) {
		gp_list_free (flist);
		return ret;
	}
	GP_LOG_D ("Found %i camera drivers.", count);
	lt_dlinit ();
	p = gp_context_progress_start (context, count,
		_("Loading camera drivers from '%s'..."), dir);
	for (i = 0; i < count; i++) {
		ret = gp_list_get_name (flist, i, &filename);
		if (ret < GP_OK) {
			gp_list_free (flist);
			return ret;
		}
		lh = lt_dlopenext (filename);
		if (!lh) {
			GP_LOG_D ("Failed to load '%s': %s.", filename,
				lt_dlerror ());
			continue;
		}

		/* camera_id */
		id = lt_dlsym (lh, "camera_id");
		if (!id) {
			GP_LOG_D ("Library '%s' does not seem to "
				"contain a camera_id function: %s",
				filename, lt_dlerror ());
			lt_dlclose (lh);
			continue;
		}

		/*
		 * Make sure the camera driver hasn't been
		 * loaded yet.
		 */
		if (id (&text) != GP_OK) {
			lt_dlclose (lh);
			continue;
		}
		if (gp_abilities_list_lookup_id (list, text.text) >= 0) {
			lt_dlclose (lh);
			continue;
		}

		/* camera_abilities */
		ab = lt_dlsym (lh, "camera_abilities");
		if (!ab) {
			GP_LOG_D ("Library '%s' does not seem to "
				"contain a camera_abilities function: "
				"%s", filename, lt_dlerror ());
			lt_dlclose (lh);
			continue;
		}

		old_count = gp_abilities_list_count (list);
		if (old_count < 0) {
			lt_dlclose (lh);
			continue;
		}

		if (ab (list) != GP_OK) {
			lt_dlclose (lh);
			continue;
		}

		/* do not free the library in valgrind mode */
#if !defined(VALGRIND)
		lt_dlclose (lh);
#endif
		new_count = gp_abilities_list_count (list);
		if (new_count < 0)
			continue;

		/* Copy in the core-specific information */
		for (x = old_count; x < new_count; x++) {
			strcpy (list->abilities[x].id, text.text);
			strcpy (list->abilities[x].library, filename);
		}

		gp_context_progress_update (context, p, i);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
			lt_dlexit ();
			gp_list_free (flist);
			return GP_ERROR_CANCEL;
		}
	}
	gp_context_progress_stop (context, p);
	lt_dlexit ();
	gp_list_free (flist);

	return GP_OK;
}

int
gp_abilities_list_load_dir (CameraAbilitiesList *list, const char *dir,
			    GPContext *context)
{
	int ret;

	gpi_libltdl_lock();
	ret = unlocked_gp_abilities_list_load_dir (list, dir, context);
	gpi_libltdl_unlock();
	return ret;
}



/**
 * \brief Scans the system for camera drivers.
 *
 * \param list a CameraAbilitiesList
 * \param context a GPContext
 * \return a gphoto2 error code
 *
 * All supported camera models will then be added to the list.
 *
 */
int
gp_abilities_list_load (CameraAbilitiesList *list, GPContext *context)
{
	const char *camlib_env = getenv(CAMLIBDIR_ENV);
	const char *camlibs = (camlib_env != NULL)?camlib_env:CAMLIBS;
	C_PARAMS (list);

	CHECK_RESULT (gp_abilities_list_load_dir (list, camlibs, context));
	CHECK_RESULT (gp_abilities_list_sort (list));

	return (GP_OK);
}


static int
gp_abilities_list_detect_usb (CameraAbilitiesList *list,
			      int *ability, GPPort *port)
{
	int i, count, res = GP_ERROR_IO_USB_FIND;

	CHECK_RESULT (count = gp_abilities_list_count (list));

	/* Detect USB cameras */
	GP_LOG_D ("Auto-detecting USB cameras...");
	*ability = -1;
	for (i = 0; i < count; i++) {
		int v, p, c, s;

		if (!(list->abilities[i].port & port->type))
			continue;

		v = list->abilities[i].usb_vendor;
		p = list->abilities[i].usb_product;
		if (v) {
			res = gp_port_usb_find_device(port, v, p);
			if (res == GP_OK) {
				GP_LOG_D ("Found '%s' (0x%x,0x%x)",
					list->abilities[i].model, v, p);
				*ability = i;
			} else if (res < 0 && res != GP_ERROR_IO_USB_FIND) {
				/* another error occurred.
				 * perhaps we should better
				 * report this to the calling
				 * method?
				 */
				GP_LOG_D (
					"gp_port_usb_find_device(vendor=0x%x, "
					"product=0x%x) returned %i, clearing "
					"error message on port", v, p, res);
			}

			if (res != GP_ERROR_IO_USB_FIND)
				return res;
		}

		c = list->abilities[i].usb_class;
		s = list->abilities[i].usb_subclass;
		p = list->abilities[i].usb_protocol;
		if (c) {
			res = gp_port_usb_find_device_by_class(port, c, s, p);
			if (res == GP_OK) {
				GP_LOG_D ("Found '%s' (0x%x,0x%x,0x%x)",
					list->abilities[i].model, c, s, p);
				*ability = i;
			} else if (res < 0 && res != GP_ERROR_IO_USB_FIND) {
				/* another error occurred.
				 * perhaps we should better
				 * report this to the calling
				 * method?
				 */
				GP_LOG_D (
					"gp_port_usb_find_device_by_class("
					"class=0x%x, subclass=0x%x, "
					"protocol=0x%x) returned %i, "
					"clearing error message on port",
					c, s, p, res);
			}

			if (res != GP_ERROR_IO_USB_FIND)
				return res;
		}
	}

	return res;
}


/**
 * \param list a CameraAbilitiesList
 * \param info_list the GPPortInfoList of ports to use for detection
 * \param l a #CameraList that contains the autodetected cameras after the call
 * \param context a #GPContext
 *
 * Tries to detect any camera connected to the computer using the supplied
 * list of supported cameras and the supplied info_list of ports.
 *
 * \return a gphoto2 error code
 */
int
gp_abilities_list_detect (CameraAbilitiesList *list,
			  GPPortInfoList *info_list, CameraList *l,
			  GPContext *context)
{
	GPPortInfo info;
	GPPort *port;
	int i, info_count;

	C_PARAMS (list && info_list && l);

	gp_list_reset (l);

	CHECK_RESULT (info_count = gp_port_info_list_count (info_list));

	CHECK_RESULT (gp_port_new (&port));
	for (i = 0; i < info_count; i++) {
		int res;
		char *xpath;
		GPPortType	type;

		CHECK_RESULT (gp_port_info_list_get_info (info_list, i, &info));
		CHECK_RESULT (gp_port_set_info (port, info));
		gp_port_info_get_type (info, &type);
		res = gp_port_info_get_path (info, &xpath);
		if (res <GP_OK)
			continue;
		switch (type) {
		case GP_PORT_USB:
		case GP_PORT_USB_SCSI:
		case GP_PORT_USB_DISK_DIRECT: {
			int ability;

			res = gp_abilities_list_detect_usb (list, &ability, port);
			if (res == GP_OK) {
				gp_list_append(l,
					list->abilities[ability].model,
					xpath);
			} else if (res < 0)
				gp_port_set_error (port, NULL);

			break;
		}
		case GP_PORT_DISK: {
			char	*s, path[1024];
			struct stat stbuf;

			s = strchr (xpath, ':');
			if (!s)
				break;
			s++;
			snprintf (path, sizeof(path), "%s/DCIM", s);
			if (-1 == stat(path, &stbuf)) {
				snprintf (path, sizeof(path), "%s/dcim", s);
				if (-1 == stat(path, &stbuf))
					continue;
			}
			gp_list_append (l, "Mass Storage Camera", xpath);
			break;
		}
		case GP_PORT_PTPIP: {
			char	*s;

			s = strchr (xpath, ':');
			if (!s) break;
			s++;
			if (!strlen(s)) break;
			gp_list_append (l, "PTP/IP Camera", xpath);
			break;
		}
		default:
			/*
			 * We currently cannot detect any cameras on this
			 * port
			 */
			break;
		}
	}

	gp_port_free (port);

	return (GP_OK);
}


/**
 * \brief Remove first colon from string, if any. Replace it by a space.
 * \param str a char * string
 */
static void
remove_colon_from_string (char *str)
{
	char *ch;
	ch = strchr(str, ':');
	if (ch) {
		*ch = ' ';
	}
}


/**
 * \brief Append the abilities to the list.
 * \param list  CameraAbilitiesList
 * \param abilities  CameraAbilities
 * \return a gphoto2 error code
 *
 * This function is called by a camera library on camera_abilities()
 * in order to inform libgphoto2 about a supported camera model.
 *
 */
int
gp_abilities_list_append (CameraAbilitiesList *list, CameraAbilities abilities)
{
	C_PARAMS (list);

	if (list->count == list->maxcount) {
	    C_MEM (list->abilities = realloc (list->abilities,
				sizeof (CameraAbilities) * (list->maxcount + 100)));
	    list->maxcount += 100;
	}

	memcpy (&(list->abilities [list->count]), &abilities,
		sizeof (CameraAbilities));

	/* FIXME: We replace the colon by a space in the model string
	 *        This keeps backward compatibility until we have
	 *        thought of and implemented something better.
	 */
	remove_colon_from_string(list->abilities[list->count].model);

	list->count++;

	return (GP_OK);
}


/**
 * \brief Reset the list.
 * \param list a CameraAbilitiesList
 * \return a gphoto2 error code
 */
int
gp_abilities_list_reset (CameraAbilitiesList *list)
{
	C_PARAMS (list);

	free (list->abilities);
	list->abilities = NULL;
	list->count = 0;
	list->maxcount = 0;

	return (GP_OK);
}


/**
 * \brief Count the entries in the supplied list.
 * \param list a #CameraAbilitiesList
 * \returns The number of entries or a gphoto2 error code
 */
int
gp_abilities_list_count (CameraAbilitiesList *list)
{
	C_PARAMS (list);

	return (list->count);
}

static int
cmp_abilities (const void *a, const void *b) {
	const CameraAbilities *ca = a;
	const CameraAbilities *cb = b;

	return strcasecmp (ca->model, cb->model);
}

static int
gp_abilities_list_sort (CameraAbilitiesList *list)
{
	C_PARAMS (list);

	qsort (list->abilities, list->count, sizeof(CameraAbilities), cmp_abilities);
	return (GP_OK);
}


static int
gp_abilities_list_lookup_id (CameraAbilitiesList *list, const char *id)
{
	int x;

	C_PARAMS (list && id);

	for (x = 0; x < list->count; x++)
		if (!strcmp (list->abilities[x].id, id))
			return (x);

	return (GP_ERROR);
}


/**
 * \brief Search the list for an entry of given model name
 * \param list a #CameraAbilitiesList
 * \param model a camera model name
 * \return Index of entry or gphoto2 error code
 */
int
gp_abilities_list_lookup_model (CameraAbilitiesList *list, const char *model)
{
	int x;

	C_PARAMS (list && model);

	for (x = 0; x < list->count; x++) {
		if (!strcasecmp (list->abilities[x].model, model))
			return (x);
	}

	GP_LOG_E ("Could not find any driver for '%s'", model);
	return (GP_ERROR_MODEL_NOT_FOUND);
}


/**
 * \brief Retrieve the camera abilities of entry with supplied index number.
 *
 * \param list a CameraAbilitiesList
 * \param index index
 * \param abilities pointer to CameraAbilities for returned data.
 * \return a gphoto2 error code
 *
 * Retrieves the camera abilities of entry with supplied
 * index number. Typically, you would call gp_camera_set_abilities()
 * afterwards in order to prepare the initialization of a camera.
 */
int
gp_abilities_list_get_abilities (CameraAbilitiesList *list, int index,
				 CameraAbilities *abilities)
{
	C_PARAMS (list && abilities);
	C_PARAMS (0 <= index && index < list->count);

	memcpy (abilities, &list->abilities[index], sizeof (CameraAbilities));

	return (GP_OK);
}


#ifdef _GPHOTO2_INTERNAL_CODE

/* enum CameraOperation */
const StringFlagItem gpi_camera_operation_map[] = {
	{ "none",            GP_OPERATION_NONE },
	{ "capture_image",   GP_OPERATION_CAPTURE_IMAGE },
	{ "capture_video",   GP_OPERATION_CAPTURE_VIDEO },
	{ "capture_audio",   GP_OPERATION_CAPTURE_AUDIO },
	{ "capture_preview", GP_OPERATION_CAPTURE_PREVIEW },
	{ "config",          GP_OPERATION_CONFIG },
	{ NULL, 0 },
};

/* enum CameraFileOperation */
const StringFlagItem gpi_file_operation_map[] = {
	{ "none",    GP_FILE_OPERATION_NONE },
	{ "delete",  GP_FILE_OPERATION_DELETE },
	{ "preview", GP_FILE_OPERATION_PREVIEW },
	{ "raw",     GP_FILE_OPERATION_RAW },
	{ "audio",   GP_FILE_OPERATION_AUDIO },
	{ "exif",    GP_FILE_OPERATION_EXIF },
	{ NULL, 0 },
};

/* enum CameraFolderOperation */
const StringFlagItem gpi_folder_operation_map[] = {
	{ "none",       GP_FOLDER_OPERATION_NONE },
	{ "delete_all", GP_FOLDER_OPERATION_DELETE_ALL },
	{ "put_file",   GP_FOLDER_OPERATION_PUT_FILE },
	{ "make_dir",   GP_FOLDER_OPERATION_MAKE_DIR },
	{ "remove_dir", GP_FOLDER_OPERATION_REMOVE_DIR },
	{ NULL, 0 },
};

/* enum GphotoDeviceType */
const StringFlagItem gpi_gphoto_device_type_map[] = {
	{ "still_camera", GP_DEVICE_STILL_CAMERA },
	{ "audio_player", GP_DEVICE_AUDIO_PLAYER },
	{ NULL, 0 },
};

/* enum CameraDriverStatus */
const StringFlagItem gpi_camera_driver_status_map[] = {
	{ "production",   GP_DRIVER_STATUS_PRODUCTION },
	{ "testing",      GP_DRIVER_STATUS_TESTING },
	{ "experimental", GP_DRIVER_STATUS_EXPERIMENTAL },
	{ "deprecated",   GP_DRIVER_STATUS_DEPRECATED },
	{ NULL, 0 },
};

#endif /* _GPHOTO2_INTERNAL_CODE */


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
