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

	return (GP_OK);
}

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

int
gp_port_info_list_count (GPPortInfoList *list)
{
	CHECK_NULL (list);

	return (list->count);
}

int
gp_port_info_list_lookup_path (GPPortInfoList *list, const char *path)
{
	int i;

	CHECK_NULL (list && path);

	for (i = 0; i < list->count; i++)
		if (!strcmp (list->info[i].path, path))
			return (i);
	
	return (GP_ERROR_UNKNOWN_PORT);
}

int
gp_port_info_list_get_info (GPPortInfoList *list, int n, GPPortInfo *info)
{
	CHECK_NULL (list && info);

	if (n < 0 || n >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	memcpy (info, &(list->info[n]), sizeof (GPPortInfo));

	return (GP_OK);
}
