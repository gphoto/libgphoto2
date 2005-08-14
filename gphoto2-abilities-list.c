/** \file gphoto2-abilities-list.c
 * \brief List of supported camera models including their abilities.
 *
 * \author Copyright © 2000 Scott Fritzinger
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gphoto2-abilities-list.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ltdl.h>

#include "gphoto2-result.h"
#include "gphoto2-port-log.h"
#include "gphoto2-library.h"

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

/** \internal */
#define GP_MODULE "gphoto2-abilities-list"

/** \internal */
#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
/** \internal */
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
/** \internal */
#define CHECK_MEM(m)         {if (!(m)) return (GP_ERROR_NO_MEMORY);}

/**
 * Name of the environment variable which may contain the path where
 * to look for the camlibs. If this environment variable is not defined,
 * use the compiled-in default constant.
 *
 * \internal Internal use only.
 */
#define CAMLIBDIR_ENV "CAMLIBS"

/** \internal */
struct _CameraAbilitiesList {
	int count;
	CameraAbilities *abilities;
};

static int gp_abilities_list_lookup_id (CameraAbilitiesList *, const char *);
static int gp_abilities_list_sort      (CameraAbilitiesList *);

/**
 * Allocate the memory for a new abilities list.
 * \param list CameraAbilitiesList object to initialize
 * \return gphoto2 error code
 *
 * You would then call gp_abilities_list_load() in order to 
 * populate it.
 */
int
gp_abilities_list_new (CameraAbilitiesList **list)
{
	CHECK_NULL (list);

	/*
	 * We do this here because everybody needs to call this function
	 * first before accessing a camera. Pretty ugly, but I don't know
	 * an other way without introducing a global initialization
	 * function...
	 */
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);

	CHECK_MEM (*list = malloc (sizeof (CameraAbilitiesList)));
	memset (*list, 0, sizeof (CameraAbilitiesList));

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
	CHECK_NULL (list);

	CHECK_RESULT (gp_abilities_list_reset (list));

	free (list);

	return (GP_OK);
}


static int
foreach_func (const char *filename, lt_ptr data)
{
	CameraList *list = data;
	int result;

	gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
		"Found '%s'.", filename);
	result = gp_list_append (list, filename, NULL);

	return ((result == GP_OK)?0:1);
}


static int
gp_abilities_list_load_dir (CameraAbilitiesList *list, const char *dir,
			    GPContext *context)
{
	CameraLibraryIdFunc id;
	CameraLibraryAbilitiesFunc ab;
	CameraText text;
	int x, old_count, new_count;
	unsigned int i, p;
	const char *filename;
	CameraList *flist;
	int count;
	int ret;
	lt_dlhandle lh;

	CHECK_NULL (list && dir);

	gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
		"Using ltdl to load camera libraries from '%s'...", dir);
	CHECK_RESULT (gp_list_new (&flist));
	CHECK_RESULT (gp_list_reset (flist));
	lt_dlinit ();
	lt_dladdsearchdir (dir);
	ret = lt_dlforeachfile (dir, foreach_func, flist);
	lt_dlexit ();
	if (ret != 0) {
		gp_log (GP_LOG_ERROR, "gp-abilities-list", 
			"Internal error looking for camlibs (%d)", ret);
		gp_context_error (context,
				  _("Internal error looking for camlibs. "
				    "(path names too long?)"));
		return (GP_ERROR);
	}
	CHECK_RESULT (count = gp_list_count (flist));
	gp_log (GP_LOG_DEBUG, "gp-abilities-list", "Found %i "
		"camera drivers.", count);
	lt_dlinit ();
	p = gp_context_progress_start (context, count,
		_("Loading camera drivers from '%s'..."), dir);
	for (i = 0; i < count; i++) {
		CHECK_RESULT (gp_list_get_name (flist, i, &filename));
		lh = lt_dlopenext (filename);
		if (!lh) {
			gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
				"Failed to load '%s': %s.", filename,
				lt_dlerror ());
			continue;
		}

		/* camera_id */
		id = lt_dlsym (lh, "camera_id");
		if (!id) {
			gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
				"Library '%s' does not seem to "
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
			gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
				"Library '%s' does not seem to "
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

		lt_dlclose (lh);

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
			return (GP_ERROR_CANCEL); 
		}
	}
	gp_context_progress_stop (context, p);
	lt_dlexit ();
	gp_list_free (flist);

	return (GP_OK);
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
	CHECK_NULL (list);

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
	gp_log (GP_LOG_VERBOSE, __FILE__,
		"Auto-detecting USB cameras...");
	*ability = -1;
	for (i = 0; i < count; i++) {
		int v, p, c, s;

		v = list->abilities[i].usb_vendor;
		p = list->abilities[i].usb_product;
		if (v) {
			res = gp_port_usb_find_device(port, v, p);
			if (res == GP_OK) {
				gp_log(GP_LOG_DEBUG, __FILE__,
					"Found '%s' (0x%x,0x%x)",
					list->abilities[i].model,
					v, p);
				*ability = i;
			} else if (res < 0 && res != GP_ERROR_IO_USB_FIND) {
				/* another error occured. 
				 * perhaps we should better
				 * report this to the calling
				 * method?
				 */
				gp_log(GP_LOG_DEBUG, __FILE__,
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
				gp_log(GP_LOG_DEBUG, __FILE__,
					"Found '%s' (0x%x,0x%x,0x%x)",
					list->abilities[i].model,
					c, s, p);
				*ability = i;
			} else if (res < 0 && res != GP_ERROR_IO_USB_FIND) {
				/* another error occured. 
				 * perhaps we should better
				 * report this to the calling
				 * method?
				 */
				gp_log(GP_LOG_DEBUG, __FILE__,
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
 * \param info_list a GPPortInfoList
 * \param l a CameraList
 * \return a gphoto2 error code
 *
 * Tries to detect any camera connected to the computer using the supplied
 * list of supported cameras and the supplied info_list of ports.
 *
 */
int
gp_abilities_list_detect (CameraAbilitiesList *list,
			  GPPortInfoList *info_list, CameraList *l,
			  GPContext *context)
{
	GPPortInfo info;
	GPPort *port;
	int i, info_count;

	CHECK_NULL (list && info_list && l);

	gp_list_reset (l);

	CHECK_RESULT (info_count = gp_port_info_list_count (info_list));

	CHECK_RESULT (gp_port_new (&port));
	for (i = 0; i < info_count; i++) {
		int res;

		CHECK_RESULT (gp_port_info_list_get_info (info_list, i, &info));
		CHECK_RESULT (gp_port_set_info (port, info));
		switch (info.type) {
		case GP_PORT_USB: {
			int ability;
			res = gp_abilities_list_detect_usb (list, &ability, port);
			if (res == GP_OK) {
				gp_list_append(l,
					list->abilities[ability].model,
					info.path);
			} else if (res < 0)
				gp_port_set_error (port, NULL);

			break;
		}
		case GP_PORT_DISK: {
			char	*s, path[1024];
			struct stat stbuf;
		
			s = strchr (info.path, ':');
			if (!s)
				break;
			s++;
			snprintf (path, sizeof(path), "%s/DCIM", s);
			if (-1 == stat(path, &stbuf)) {
				snprintf (path, sizeof(path), "%s/dcim", s);
				if (-1 == stat(path, &stbuf))
					continue;
			}
			gp_list_append (l, "Directory Browse", info.path);
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
	CameraAbilities *new_abilities;

	CHECK_NULL (list);

	if (!list->count)
		new_abilities = malloc (sizeof (CameraAbilities));
	else
		new_abilities = realloc (list->abilities,
				sizeof (CameraAbilities) * (list->count + 1));
	CHECK_MEM (new_abilities);
	list->abilities = new_abilities;
	
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
	CHECK_NULL (list);

	if (list->abilities) {
		free (list->abilities);
		list->abilities = NULL;
	}
	list->count = 0;

	return (GP_OK);
}


/**
 * \brief Count the entries in the supplied list.
 * \param a CameraAbilitiesList
 * \return The number of entries or a gphoto2 error code
 */
int
gp_abilities_list_count (CameraAbilitiesList *list)
{
	CHECK_NULL (list);

	return (list->count);
}


static int
gp_abilities_list_sort (CameraAbilitiesList *list)
{
	CameraAbilities t;
	int x, y;

	CHECK_NULL (list);

	for (x = 0; x < list->count - 1; x++)
		for (y = x + 1; y < list->count; y++)
			if (strcasecmp (list->abilities[x].model,
					list->abilities[y].model) > 0) {
				memcpy (&t, &list->abilities[x],
					sizeof (CameraAbilities));
				memcpy (&list->abilities[x],
					&list->abilities[y],
					sizeof (CameraAbilities));
				memcpy (&list->abilities[y], &t,
					sizeof (CameraAbilities));
			}

	return (GP_OK);
}


static int
gp_abilities_list_lookup_id (CameraAbilitiesList *list, const char *id)
{
	int x;

	CHECK_NULL (list && id);

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

	CHECK_NULL (list && model);

	for (x = 0; x < list->count; x++) {
		if (!strcasecmp (list->abilities[x].model, model))
			return (x);
	}

	gp_log (GP_LOG_ERROR, "gphoto2-abilities-list", _("Could not find "
		"any driver for '%s'"), model);
	return (GP_ERROR_MODEL_NOT_FOUND);
}


/**
 * \brief Retrieve the camera abilities of entry with supplied index number.
 * \param list a CameraAbilitiesList
 * \param index index
 * \param abilities CameraAbilities
 * \return a gphoto2 error code
 *
 * Retrieves the camera @abilities of entry with supplied
 * index number. Typically, you would call gp_camera_set_abilities()
 * afterwards in order to prepare the initialization of a camera.
 */
int
gp_abilities_list_get_abilities (CameraAbilitiesList *list, int index,
				 CameraAbilities *abilities)
{
	CHECK_NULL (list && abilities);

	if (index < 0 || index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	memcpy (abilities, &list->abilities[index], sizeof (CameraAbilities));

	return (GP_OK);
}


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
