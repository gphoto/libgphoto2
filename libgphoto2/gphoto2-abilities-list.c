/* gphoto2-abilities-list.c
 *
 * Copyright (C) 2000 Scott Fritzinger
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
#include "gphoto2-abilities-list.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gphoto2-result.h"
#include "gphoto2-port-log.h"
#include "gphoto2-library.h"

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

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_MEM(m)         {if (!(m)) return (GP_ERROR_NO_MEMORY);}

/**
 * CameraAbilitiesList:
 *
 * A list of supported camera models including their abilities. The internals
 * of this list are hidden - please use functions to access the list.
 **/
struct _CameraAbilitiesList {
	int count;
	CameraAbilities *abilities;
};

static int gp_abilities_list_lookup_id (CameraAbilitiesList *, const char *);
static int gp_abilities_list_sort      (CameraAbilitiesList *);

/**
 * gp_abilities_list_new:
 * @list:
 *
 * Allocates the memory for a new abilities list. You would then call
 * #gp_abilities_list_load in order to populate it.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_abilities_list_new (CameraAbilitiesList **list)
{
	CHECK_NULL (list);

	CHECK_MEM (*list = malloc (sizeof (CameraAbilitiesList)));

	(*list)->count = 0;
	(*list)->abilities = NULL;

	return (GP_OK);
}

/**
 * gp_abilities_list_free:
 * @list: a #CameraAbilitiesList
 *
 * Frees the @list.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_abilities_list_free (CameraAbilitiesList *list)
{
	CHECK_NULL (list);

	if (list->abilities) {
		free (list->abilities);
		list->abilities = NULL;
	}

	free (list);

	return (GP_OK);
}

static int
gp_abilities_list_load_dir (CameraAbilitiesList *list, const char *dir)
{
	GP_SYSTEM_DIR d;
	GP_SYSTEM_DIRENT de;
	char buf[1024];
	void *lh;
	CameraLibraryIdFunc id;
	CameraLibraryAbilitiesFunc ab;
	CameraText text;
	int x, old_count, new_count;

	CHECK_NULL (list && dir);

	gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
		"Loading camera libraries in '%s'...", dir);

	d = GP_SYSTEM_OPENDIR (dir);
	if (!d) {
		gp_log (GP_LOG_ERROR, "gphoto2-abilities-list",
			_("Could not open '%s'"), dir);
		return GP_ERROR_LIBRARY;
	}

	do {

		/* Read each entry */
		de = GP_SYSTEM_READDIR (d);
		if (de) {
			const char *filename = GP_SYSTEM_FILENAME (de);

			sprintf (buf, "%s%c%s", dir, GP_SYSTEM_DIR_DELIM,
				 filename);

			/* Don't try to open ".*" */
			if (filename[0] == '.')
				continue;

			/* Try to open the library */
			lh = GP_SYSTEM_DLOPEN (buf);
			if (!lh) {
				gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
					"Failed to load '%s': %s.", buf,
					GP_SYSTEM_DLERROR ());
				continue;
			}

			/* camera_id */
			id = GP_SYSTEM_DLSYM (lh, "camera_id");
			if (!id) {
				gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
					"Library '%s' does not seem to "
					"contain a camera_id function: %s",
					buf, GP_SYSTEM_DLERROR ());
				GP_SYSTEM_DLCLOSE (lh);
				continue;
			}

			/*
			 * Make sure the camera driver hasn't been
			 * loaded yet
			 */
			if (id (&text) != GP_OK) {
				GP_SYSTEM_DLCLOSE (lh);
				continue;
			}
			if (gp_abilities_list_lookup_id (list, text.text) >= 0){
				GP_SYSTEM_DLCLOSE (lh);
				continue;
			}

			/* camera_abilities */
			ab = GP_SYSTEM_DLSYM (lh, "camera_abilities");
			if (!ab) {
				gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
					"Library '%s' does not seem to "
					"contain a camera_abilities function: "
					"%s", buf, GP_SYSTEM_DLERROR ());
				GP_SYSTEM_DLCLOSE (lh);
				continue;
			}

			old_count = gp_abilities_list_count (list);
			if (old_count < 0) {
				GP_SYSTEM_DLCLOSE (lh);
				continue;
			}

			if (ab (list) != GP_OK) {
				GP_SYSTEM_DLCLOSE (lh);
				continue;
			}

			GP_SYSTEM_DLCLOSE (lh);

			new_count = gp_abilities_list_count (list);
			if (new_count < 0)
				continue;

			/* Copy in the core-specific information */
			for (x = old_count; x < new_count; x++) {
				strcpy (list->abilities[x].id, text.text);
				strcpy (list->abilities[x].library, buf);
			}
		}
	} while (de);

	return (GP_OK);
}

/**
 * gp_abilities_list_load:
 * @list: a #CameraAbilitiesList
 *
 * Scans the system for camera drivers. All supported camera models will then
 * be added to the @list.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_abilities_list_load (CameraAbilitiesList *list)
{
	CHECK_NULL (list);

	CHECK_RESULT (gp_abilities_list_load_dir (list, CAMLIBS));
	CHECK_RESULT (gp_abilities_list_sort (list));

	return (GP_OK);
}

/**
 * gp_abilities_list_detect:
 * @list: a #CameraAbilitiesList
 * @l: a #CameraList
 *
 * Tries to detect any camera connected to the computer using the supplied
 * @list of supported cameras.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_abilities_list_detect (CameraAbilitiesList *list, CameraList *l)
{
	GPPort *dev;
	int x, count, v, p;
	const char *m;

	CHECK_NULL (list && l);

	l->count = 0;

	CHECK_RESULT (count = gp_abilities_list_count (list));

	CHECK_RESULT (gp_port_new (&dev, GP_PORT_USB));

	gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
		"Auto-detecting cameras...");
	for (x = 0; x < count; x++) {
		v = list->abilities[x].usb_vendor;
		p = list->abilities[x].usb_product;
		if ((gp_port_usb_find_device (dev, v, p)       == GP_OK) &&
		    (gp_abilities_list_get_model (list, x, &m) == GP_OK)) {
			gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
				"Found '%s' (%i,%i)", m, v, p);
			gp_list_append (l, m, "usb:");
		}
	}
	gp_port_free (dev);

	return (GP_OK);
}

int
gp_abilities_list_dump_libs (CameraAbilitiesList *list)
{
	int x;

	CHECK_NULL (list);

	for (x = 0; x < list->count; x++)
		gp_log (GP_LOG_DEBUG, "gphoto2-abilities-list",
			"\t\"%s\" uses %s", list->abilities[x].model,
			list->abilities[x].library);
	return (GP_OK);
}

/**
 * gp_abilities_list_append:
 * @list: a #CameraAbilitiesList
 * @abilities: #CameraAbilities
 *
 * Appends the @abilities to the @list. This function is called by a camera
 * library on #camera_abilities in order to inform gphoto2 about a supported
 * camera model.
 *
 * Return value: a gphoto2 error code
 **/
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
	list->count++;

	return (GP_OK);
}

/**
 * gp_abilities_list_count:
 * @list: a #CameraAbilitiesList
 *
 * Counts the entries in the supplied @list.
 *
 * Return value: The number of entries or a gphoto2 error code
 **/
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
 * gp_abilities_list_get_model:
 * @list: a #CameraAbilitiesList
 * @index: index
 * @model:
 *
 * Retreives the model of entry with given @index.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_abilities_list_get_model (CameraAbilitiesList *list, int index,
			     const char **model)
{
	CHECK_NULL (list && model);

	if ((index < 0) || (index >= list->count))
		return (GP_ERROR_BAD_PARAMETERS);

	*model = list->abilities[index].model;

	return (GP_OK);
}

/**
 * gp_abilities_list_lookup_model
 * @list: a #CameraAbilitiesList
 * @model: a camera model
 *
 * Searches the @list for an entry of given @model.
 *
 * Return value: Index of entry or gphoto2 error code
 **/
int
gp_abilities_list_lookup_model (CameraAbilitiesList *list, const char *model)
{
	int x;

	CHECK_NULL (list && model);

	for (x = 0; x < list->count; x++)
		if (!strcasecmp (list->abilities[x].model, model))
			return (x);

	gp_log (GP_LOG_ERROR, "gphoto2-abilities-list", _("Could not find "
		"any driver for '%s'"), model);
	return (GP_ERROR_MODEL_NOT_FOUND);
}

/**
 * gp_abilities_list_get_abilities:
 * @list: a #CameraAbilitiesList
 * @index: index
 * @abilities: #CameraAbilities
 *
 * Retreives the camera @abilities of entry with supplied @index. Typically,
 * you would call #gp_camera_set_abilities afterwards in order to prepare the
 * initialization of a camera.
 *
 * Return value: a gphoto2 error code
 **/
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
