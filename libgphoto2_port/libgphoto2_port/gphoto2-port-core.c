/* gphoto2-port-core.c
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
#include "gphoto2-port-core.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gphoto2-port-result.h"
#include "gphoto2-port-library.h"
#include "gphoto2-port-log.h"

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

GPPortInfo  *port_info = NULL;
unsigned int port_info_size = 0;

#define CHECK_RESULT(result) {int r=(result); if (r<0) return (r);}

static int
gp_port_core_init (void)
{
	const char *filename;
	GP_SYSTEM_DIR d;
	GP_SYSTEM_DIRENT de;
	char path[1024];
	void *lh;
	GPPortLibraryType lib_type;
	GPPortLibraryList lib_list;
	GPPortType type;
	int i, result;
	unsigned int old_size = port_info_size;

	if (!port_info)
		port_info = malloc (sizeof (GPPortInfo) * 256);
	if (!port_info)
		return (GP_ERROR_NO_MEMORY);

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
		for (i = 0; i < port_info_size; i++)
			if (port_info[i].type == type)
				break;
		if (i != port_info_size) {
			gp_log (GP_LOG_DEBUG, "gphoto2-port-core",
				"'%s' already loaded", path);
			GP_SYSTEM_DLCLOSE (lh);
			continue;
		}

		result = lib_list (port_info, &port_info_size);
		if (result < 0) {
			gp_log (GP_LOG_DEBUG, "gphoto2-port-core",
				"Could not load list (%s)",
				gp_port_result_as_string (result));
			GP_SYSTEM_DLCLOSE (lh);
			continue;
		}

		for (i = old_size; i < port_info_size; i++){
			gp_log (GP_LOG_DEBUG, "gphoto2-port-core",
				"Loaded '%s' (%s) from '%s'",
				port_info[i].name, port_info[i].path,
				filename);
			strcpy (port_info[i].library_filename, path);
		}
		old_size = port_info_size;

		GP_SYSTEM_DLCLOSE (lh);
	} while (1);

	GP_SYSTEM_CLOSEDIR (d);

	return (GP_OK);
}

/**
 * gp_port_core_count:
 *
 * Counts the devices gphoto2-port can address. You can then retreive
 * some information about each device using #gp_port_core_get_info.
 *
 * Return value: The number of devices or a gphoto2-port error code
 **/
int
gp_port_core_count (void)
{
	if (!port_info_size)
		CHECK_RESULT (gp_port_core_init ());

	return (port_info_size);
}

/**
 * gp_port_core_get_info:
 * @x: index
 * @info:
 *
 * Retreives information about a device. You can get the total number of
 * available devices with #gp_port_core_count.
 *
 * Return value: a gphoto2-port error code
 **/
int
gp_port_core_get_info (unsigned int x, GPPortInfo *info)
{
	if (!port_info_size)
		CHECK_RESULT (gp_port_core_init ());

	if (!info || (x > port_info_size - 1))
		return (GP_ERROR_BAD_PARAMETERS);

	memcpy (info, &port_info [x], sizeof (GPPortInfo));

	return (GP_OK);
}

/**
 * gp_port_core_get_library:
 * @type: the type of the library
 * @library: 
 *
 * Tries to find the filename of an io library for devices of given @type.
 *
 * Return value: a gphoto2-port error code.
 **/
int
gp_port_core_get_library (GPPortType type, const char **library)
{
	int i;

	if (!port_info_size)
		CHECK_RESULT (gp_port_core_init ());

	for (i = 0; i < port_info_size; i++)
		if (port_info[i].type == type) {
			*library = port_info[i].library_filename;
			return (GP_OK);
		}

	return (GP_ERROR_UNKNOWN_PORT);
}

/*
 * Everything below is deprecated!
 */
int gp_port_init (void);
int
gp_port_init (void)
{
	return (gp_port_core_init ());
}

int gp_port_info_get (int n, GPPortInfo *info);
int
gp_port_info_get (int n, GPPortInfo *info)
{
	return (gp_port_core_get_info (n, info));
}

int gp_port_count_get (void);
int
gp_port_count_get (void)
{
	return (gp_port_core_count ());
}
