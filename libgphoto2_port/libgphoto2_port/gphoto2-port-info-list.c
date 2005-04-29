/* gphoto2-port-info-list.c:
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
#include "gphoto2-port-info-list.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_REGEX
#include <regex.h>
#else
#error We need regex.h, but it has not been detected.
#endif

#include <ltdl.h>

#include <gphoto2-port-result.h>
#include <gphoto2-port-library.h>
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

/**
 * GPPortInfoList:
 *
 * The internals of this list are private.
 **/
struct _GPPortInfoList {
	GPPortInfo *info;
	unsigned int count;
};

#define CHECK_NULL(x) {if (!(x)) return (GP_ERROR_BAD_PARAMETERS);}
#define CR(x)         {int r=(x);if (r<0) return (r);}

/** IOLIBDIR_ENV:
 *
 * Name of the environment variable which may contain the path where
 * to look for the IO libs. If this environment variable is not defined,
 * use the compiled-in default constant.
 **/
#define IOLIBDIR_ENV "IOLIBS"

/**
 * gp_port_info_list_new:
 * @list:
 *
 * Creates a new list which can later be filled with port infos (#GPPortInfo)
 * using #gp_port_info_list_load.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_info_list_new (GPPortInfoList **list)
{
	CHECK_NULL (list);

	/*
	 * We put this in here because everybody needs to call this function
	 * before accessing ports...
	 */
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);

	*list = malloc (sizeof (GPPortInfoList));
	if (!*list)
		return (GP_ERROR_NO_MEMORY);
	memset (*list, 0, sizeof (GPPortInfoList));

	return (GP_OK);
}

/**
 * gp_port_info_list_free:
 * @list: a #GPPortInfoList
 *
 * Frees a @list.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_info_list_free (GPPortInfoList *list)
{
	CHECK_NULL (list);

	if (list->info) {
		free (list->info);
		list->info = NULL;
	}
	list->count = 0;

	free (list);

	return (GP_OK);
}

/**
 * gp_port_info_list_append:
 * @list: a #GPPortInfoList
 * @info: the info to append
 *
 * Appends an entry to the @list. This function is typically called by
 * an io-driver during #gp_port_library_list. If you leave info.name blank,
 * #gp_port_info_list_lookup_path will try to match non-existent paths
 * against info.path and - if successfull - will append this entry to the 
 * list.
 *
 * Return value: The index of the new entry or a gphoto2 error code
 **/
int
gp_port_info_list_append (GPPortInfoList *list, GPPortInfo info)
{
	int generic, i;
	GPPortInfo *new_info;

	CHECK_NULL (list);

	if (!list->info)
		new_info = malloc (sizeof (GPPortInfo));
	else
		new_info = realloc (list->info, sizeof (GPPortInfo) *
							(list->count + 1));
	if (!new_info)
		return (GP_ERROR_NO_MEMORY);

	list->info = new_info;
	list->count++;

	memcpy (&(list->info[list->count - 1]), &info, sizeof (GPPortInfo));

	/* Ignore generic entries */
	for (generic = i = 0; i < list->count; i++)
		if (!strlen (list->info[i].name))
			generic++;
	return (list->count - 1 - generic);
}


static int
foreach_func (const char *filename, lt_ptr data)
{
	GPPortInfoList *list = data;
	lt_dlhandle lh;
	GPPortLibraryType lib_type;
	GPPortLibraryList lib_list;
	GPPortType type;
	unsigned int j, old_size = list->count;
	int result;

	gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
		_("Called for filename '%s'."), filename );

	lh = lt_dlopenext (filename);
	if (!lh) {
		gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
			_("Could not load '%s': '%s'."), filename, lt_dlerror ());
		return (0);
	}

	lib_type = lt_dlsym (lh, "gp_port_library_type");
	lib_list = lt_dlsym (lh, "gp_port_library_list");
	if (!lib_type || !lib_list) {
		gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
			_("Could not find some functions in '%s': '%s'."),
			filename, lt_dlerror ());
		lt_dlclose (lh);
		return (0);
	}

	type = lib_type ();
	for (j = 0; j < list->count; j++)
		if (list->info[j].type == type)
			break;
	if (j != list->count) {
		gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
			_("'%s' already loaded"), filename);
		lt_dlclose (lh);
		return (0);
	}

	result = lib_list (list);
	lt_dlclose (lh);
	if (result < 0) {
		gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
			_("Could not load port driver list: '%s'."),
			gp_port_result_as_string (result));
		return (0);
	}

	for (j = old_size; j < list->count; j++){
		gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
			_("Loaded '%s' ('%s') from '%s'."),
			list->info[j].name, list->info[j].path,
			filename);
		strcpy (list->info[j].library_filename, filename);
	}

	return (0);
}


/**
 * gp_port_info_list_load:
 * @list: a #GPPortInfoList
 *
 * Searches the system for io-drivers and appends them to the list. You would
 * normally call this function once after #gp_port_info_list_new and then
 * use this list in order to supply #gp_port_set_info with parameters.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_info_list_load (GPPortInfoList *list)
{
	const char *iolibs_env = getenv(IOLIBDIR_ENV);
	const char *iolibs = (iolibs_env != NULL)?iolibs_env:IOLIBS;
	int result;

	CHECK_NULL (list);

	gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list", _("Using ltdl to load io-drivers "
		"from '%s'..."),iolibs);
	lt_dlinit ();
	lt_dladdsearchdir (iolibs);
	result = lt_dlforeachfile (iolibs, foreach_func, list);
	lt_dlexit ();
	if (result < 0)
		return (result);

        return (GP_OK);
}

/**
 * gp_port_info_list_count:
 * @list: a #GPPortInfoList
 *
 * Returns the number of entries in the @list.
 *
 * Return value: The number of entries or a gphoto2 error code
 **/
int
gp_port_info_list_count (GPPortInfoList *list)
{
	int count, i;

	CHECK_NULL (list);

	gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list", _("Counting entries "
		"(%i available)..."), list->count);

	/* Ignore generic entries */
	count = list->count;
	for (i = 0; i < list->count; i++)
		if (!strlen (list->info[i].name))
			count--;

	gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list", _("%i regular entries "
		"available."), count);
	return (count);
}

/**
 * gp_port_info_list_lookup_path:
 * @list: a #GPPortInfoList
 * @path: a path
 *
 * Looks for an entry in the list with the supplied @path. If no exact match
 * can be found, a regex search will be performed in the hope some driver
 * claimed ports like "serial:*".
 *
 * Return value: The index of the entry or a gphoto2 error code
 **/
int
gp_port_info_list_lookup_path (GPPortInfoList *list, const char *path)
{
	int i, result, generic;
	regex_t pattern;
#ifdef HAVE_GNU_REGEX
	const char *rv;
#else
	regmatch_t match;
#endif

	CHECK_NULL (list && path);

	gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list", _("Looking for "
		"path '%s' (%i entries available)..."), path, list->count);

	/* Exact match? */
	for (generic = i = 0; i < list->count; i++)
		if (!strlen (list->info[i].name))
			generic++;
		else if (!strcmp (list->info[i].path, path))
			return (i - generic);

	/* Regex match? */
	gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
		_("Starting regex search for '%s'..."), path);
	for (i = 0; i < list->count; i++) {
		if (strlen (list->info[i].name))
			continue;

		gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
			_("Trying '%s'..."), list->info[i].path);

		/* Compile the pattern */
#ifdef HAVE_GNU_REGEX
		memset (&pattern, 0, sizeof (pattern));
		rv = re_compile_pattern (list->info[i].path,
					 strlen (list->info[i].path), &pattern);
		if (rv) {
			gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
				"%s", rv);
			continue;
		}
#else
		result = regcomp (&pattern, list->info[i].path, REG_ICASE);
		if (result) {
			char buf[1024];
			if (regerror (result, &pattern, buf, sizeof (buf)))
				gp_log (GP_LOG_ERROR, "gphoto2-port-info-list",
					"%s", buf);
			else
				gp_log (GP_LOG_ERROR, "gphoto2-port-info-list",
					_("regcomp failed"));
			return (GP_ERROR_UNKNOWN_PORT);
		}
#endif

		/* Try to match */
#ifdef HAVE_GNU_REGEX
		result = re_match (&pattern, path, strlen (path), 0, NULL);
		regfree (&pattern);
		if (result < 0) {
			gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
				_("re_match failed (%i)"), result);
			continue;
		}
#else
		result = regexec (&pattern, path, 1, &match, 0);
		regfree (&pattern);
		if (result) {
			gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
				_("regexec failed"));
			continue;
		}
#endif
		CR (result = gp_port_info_list_append (list, list->info[i]));
		strncpy (list->info[result].path, path,
			 sizeof (list->info[result].path));
		strncpy (list->info[result].name, _("Generic Port"),
			 sizeof (list->info[result].name));
		return (result);
	}

	return (GP_ERROR_UNKNOWN_PORT);
}

/**
 * gp_port_info_list_lookup_name:
 * @list: a #GPPortInfoList
 * @name: a name
 *
 * Looks for an entry in the @list with the given @name.
 *
 * Return value: The index of the entry or a gphoto2 error code
 **/
int
gp_port_info_list_lookup_name (GPPortInfoList *list, const char *name)
{
	int i, generic;

	CHECK_NULL (list && name);

	gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list", _("Looking up entry "
		"'%s'..."), name);

	/* Ignore generic entries */
	for (generic = i = 0; i < list->count; i++)
		if (!strlen (list->info[i].name))
			generic++;
		else if (!strcmp (list->info[i].name, name))
			return (i - generic);

	return (GP_ERROR_UNKNOWN_PORT);
}

/**
 * gp_port_info_list_get_info:
 * @list: a #GPPortInfoList
 * @n: the index of the entry
 * @info:
 *
 * Retreives an entry from the @list.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_port_info_list_get_info (GPPortInfoList *list, int n, GPPortInfo *info)
{
	int i;

	CHECK_NULL (list && info);

	gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list", _("Getting info of "
		"entry %i (%i available)..."), n, list->count);

	if (n < 0 || n >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	/* Ignore generic entries */
	for (i = 0; i <= n; i++)
		if (!strlen (list->info[i].name))
			n++;

	if (n >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	memcpy (info, &(list->info[n]), sizeof (GPPortInfo));

	return (GP_OK);
}
