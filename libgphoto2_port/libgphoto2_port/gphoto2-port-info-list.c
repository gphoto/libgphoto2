/* gphoto2-port-info-list.c:
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
#include "gphoto2-port-info-list.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>

#include <gphoto2-port-result.h>
#include <gphoto2-port-library.h>
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

	return (GP_OK);
}

/**
 * gp_port_info_list_append:
 * @list: a #GPPortInfoList
 * @info: the info to append
 *
 * Appends an entry to the @list. This function is typically called by
 * an io-driver on #gp_port_library_list.
 *
 * Return value: The index of the new entry or a gphoto2 error code
 **/
int
gp_port_info_list_append (GPPortInfoList *list, GPPortInfo info)
{
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

	return (list->count - 1);
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
	GP_SYSTEM_DIR d;
	GP_SYSTEM_DIRENT de;
	char path[1024];
	void *lh;
	GPPortLibraryType lib_type;
	GPPortLibraryList lib_list;
	GPPortType type;
	int i, result;
	unsigned int old_size = list->count;
	const char *filename;

	CHECK_NULL (list);

	d = GP_SYSTEM_OPENDIR (IOLIBS);
        if (!d) {
                gp_log (GP_LOG_ERROR, "gphoto2-port-core",
                        _("Could not load any io-library because '%s' could "
                        "not be opened (%m)"), IOLIBS);
                return (GP_ERROR_LIBRARY);
        }

        do {
                de = GP_SYSTEM_READDIR (d);
                if (!de)
                        break;

                filename = GP_SYSTEM_FILENAME (de);
                if (filename[0] == '.')
                        continue;

#if defined(OS2) || defined(WIN32)
                snprintf (path, sizeof (path), "%s\\%s", IOLIBS, filename);
#else
                snprintf (path, sizeof (path), "%s/%s", IOLIBS, filename);
#endif

                lh = GP_SYSTEM_DLOPEN (path);
                if (!lh) {
                        gp_log (GP_LOG_DEBUG, "gphoto2-port-core",
                                "'%s' is not a library (%s)", path,
                                GP_SYSTEM_DLERROR ());
                        continue;
                }

                lib_type = GP_SYSTEM_DLSYM (lh, "gp_port_library_type");
                lib_list = GP_SYSTEM_DLSYM (lh, "gp_port_library_list");
                if (!lib_type || !lib_list) {
                        gp_log (GP_LOG_DEBUG, "gphoto2-port-core",
                                "Could not find some functions in '%s' (%s)",
                                path, GP_SYSTEM_DLERROR ());
                        GP_SYSTEM_DLCLOSE (lh);
                        continue;
                }

                type = lib_type ();
                for (i = 0; i < list->count; i++)
                        if (list->info[i].type == type)
                                break;
                if (i != list->count) {
                        gp_log (GP_LOG_DEBUG, "gphoto2-port-core",
                                "'%s' already loaded", path);
                        GP_SYSTEM_DLCLOSE (lh);
                        continue;
                }

                result = lib_list (list);
                if (result < 0) {
                        gp_log (GP_LOG_DEBUG, "gphoto2-port-core",
                                "Could not load list (%s)",
                                gp_port_result_as_string (result));
                        GP_SYSTEM_DLCLOSE (lh);
                        continue;
                }

                for (i = old_size; i < list->count; i++){
                        gp_log (GP_LOG_DEBUG, "gphoto2-port-core",
                                "Loaded '%s' (%s) from '%s'",
                                list->info[i].name, list->info[i].path,
                                filename);
                        strcpy (list->info[i].library_filename, path);
                }
                old_size = list->count;

                GP_SYSTEM_DLCLOSE (lh);
        } while (1);

        GP_SYSTEM_CLOSEDIR (d);

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
	CHECK_NULL (list);

	return (list->count);
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
	int i, result;
	regex_t reb;
#if HAVE_GNU_REGEX
	const char *rv;
#else
	regmatch_t match;
#endif

	CHECK_NULL (list && path);

	/* Exact match? */
	for (i = 0; i < list->count; i++)
		if (!strcmp (list->info[i].path, path))
			return (i);

	/* Regex match? */
	gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
		"Starting regex search for '%s'...", path);
	for (i = 0; i < list->count; i++) {
		gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
			"Trying '%s'...", list->info[i].path);

		/* Compile the pattern */
#if HAVE_GNU_REGEX
		re_syntax_options = RE_SYNTAX_POSIX_EXTENDED;
		reb.allocated = 0;
		reb.buffer = NULL;
		reb.fastmap = NULL;
		reb.translate = 0;
		reb.no_sub = 0;
		rv = re_compile_pattern (list->info[i].path,
					 sizeof (list->info[i].path), &reb);
		if (rv) {
			gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
				"%s", rv);
			continue;
		}
#else
		result = regcomp (&reb, list->info[i].path, REG_ICASE);
		if (result) {
			char buf[1024];
			if (regerror (result, &reb, buf, sizeof (buf)))
				gp_log (GP_LOG_ERROR, "gphoto2-port-info-list",
					"%s", buf);
			else
				gp_log (GP_LOG_ERROR, "gphoto2-port-info-list",
					"regcomp failed");
			return (GP_ERROR_UNKNOWN_PORT);
		}
#endif

		/* Try to match */
#if HAVE_GNU_REGEX
		result = re_match (&reb, path, strlen (path), 0, NULL);
		if (result < 0) {
			gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
				"re_match failed (%i)", result);
			continue;
		}
#else
		if (!regexec (&reb, path, 1, &match, 0)) {
			gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
				"regexec failed");
			continue;
		}
#endif
		CR (result = gp_port_info_list_append (list, list->info[i]));
		strncpy (list->info[result].path, path,
			 sizeof (list->info[result].path));
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
	int i;

	CHECK_NULL (list && name);

	for (i = 0; i < list->count; i++)
		if (!strcmp (list->info[i].name, name))
			return (i);

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
	CHECK_NULL (list && info);

	if (n < 0 || n >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	memcpy (info, &(list->info[n]), sizeof (GPPortInfo));

	return (GP_OK);
}
