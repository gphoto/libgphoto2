/** \file
 *
 * \author Copyright 2001 Lutz Mueller <lutz@users.sf.net>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE

#include "config.h"

#include <gphoto2/gphoto2-port-info-list.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_REGEX
#include <regex.h>
#elif defined(_MSC_VER)
#pragma message("We need regex.h, but it has not been detected.")
#else
#warning We need regex.h, but it has not been detected.
#endif

#include <ltdl.h>

#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-locking.h>

#include "libgphoto2_port/gphoto2-port-info.h"
#include "libgphoto2_port/i18n.h"


/**
 * \internal GPPortInfoList:
 *
 * The internals of this list are private.
 **/
struct _GPPortInfoList {
	GPPortInfo *info;
	unsigned int count;
	unsigned int iolib_count;
};

#define CR(x)         {int r=(x);if (r<0) return (r);}


/**
 * \brief Initialize the localedir directory for the libgphoto2_port gettext domain
 *
 * Override the localedir directory libgphoto2_port uses for its message
 * translations.
 *
 * This function is called by the gp_init_localedir() function, so if
 * you are calling that already, there is no need to call
 * gp_port_init_localedir() yourself.
 *
 * You only need to call this if you have a non-standard installation
 * where the locale files are at a location which differs from the
 * compiled in default location.
 *
 * If you need to call this function, call it before calling any
 * non-initialization function.
 *
 * Internally, this will make sure bindtextdomain() is called for the
 * relevant gettext text domain(s).
 *
 * \param localedir Root directory of libgphoto2_port's localization files.
 *                  If NULL, use the compiled in default value, which
 *                  will be something like "/usr/share/locale".
 * \return gphoto2 error code.
 */
int
gp_port_init_localedir (const char *localedir)
{
	static int locale_initialized = 0;
	if (locale_initialized) {
		gp_log(GP_LOG_DEBUG, "gp_port_init_localedir",
		       "ignoring late call (localedir value %s)",
		       localedir?localedir:"NULL");
		return GP_OK;
	}
	const char *const actual_localedir = (localedir?localedir:LOCALEDIR);
	const char *const gettext_domain = GETTEXT_PACKAGE_LIBGPHOTO2_PORT;
	if (bindtextdomain (gettext_domain, actual_localedir) == NULL) {
		if (errno == ENOMEM)
			return GP_ERROR_NO_MEMORY;
		return GP_ERROR;
	}
	gp_log(GP_LOG_DEBUG, "gp_port_init_localedir",
	       "localedir has been set to %s%s",
	       actual_localedir,
	       localedir?"":" (compile-time default)");
	locale_initialized = 1;
	return GP_OK;
}


/**
 * \brief Specify codeset for translations
 *
 * This function specifies the codeset that is used for the translated
 * strings that are passed back by the libgphoto2_port functions.
 *
 * This function is called by the gp_message_codeset() function, so
 * there is no need to call it yourself.
 *
 * \param codeset new codeset to use
 * \return the previous codeset
 */
const char*
gp_port_message_codeset (const char *codeset) {
	return bind_textdomain_codeset (GETTEXT_PACKAGE_LIBGPHOTO2_PORT,
					codeset);
}

/**
 * \brief Create a new GPPortInfoList
 *
 * \param list pointer to a GPPortInfoList* which is allocated
 *
 * Creates a new list which can later be filled with port infos (#GPPortInfo)
 * using #gp_port_info_list_load.
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_list_new (GPPortInfoList **list)
{
	C_PARAMS (list);

	/*
	 * We put this in here because everybody needs to call this function
	 * before accessing ports...
	 */
	gp_port_init_localedir (NULL);

	C_MEM (*list = calloc (1, sizeof (GPPortInfoList)));

	return (GP_OK);
}

/**
 * \brief Free a GPPortInfo list
 * \param list a #GPPortInfoList
 *
 * Frees a GPPortInfoList structure and its internal data structures.
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_list_free (GPPortInfoList *list)
{
	C_PARAMS (list);

	if (list->info) {
		unsigned int i;

		for (i=0;i<list->count;i++) {
			free (list->info[i]->name);
			list->info[i]->name = NULL;
			free (list->info[i]->path);
			list->info[i]->path = NULL;
			free (list->info[i]->library_filename);
			list->info[i]->library_filename = NULL;
			free (list->info[i]);
		}
		free (list->info);
		list->info = NULL;
	}
	list->count = 0;

	free (list);

	return (GP_OK);
}

/**
 * \brief Append a portinfo to the port information list
 *
 * \param list a #GPPortInfoList
 * \param info the info to append
 *
 * Appends an entry to the list. This function is typically called by
 * an io-driver during #gp_port_library_list. If you leave info.name blank,
 * #gp_port_info_list_lookup_path will try to match non-existent paths
 * against info.path and - if successful - will append this entry to the
 * list.
 *
 * \return A gphoto2 error code, or an index into the port list (excluding generic entries).
 *         which can be used for gp_port_info_list_get_info.
 **/
int
gp_port_info_list_append (GPPortInfoList *list, GPPortInfo info)
{
	unsigned int generic, i;

	C_PARAMS (list);

	C_MEM (list->info = realloc (list->info, sizeof (GPPortInfo) * (list->count + 1)));
	list->count++;
	list->info[list->count - 1] = info;

	/* Ignore generic entries */
	for (generic = i = 0; i < list->count; i++)
		if (!strlen (list->info[i]->name))
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

	GP_LOG_D ("Called for filename '%s'.", filename );

	lh = lt_dlopenext (filename);
	if (!lh) {
		GP_LOG_D ("Could not load '%s': '%s'.", filename, lt_dlerror ());
		return (0);
	}

	lib_type = lt_dlsym (lh, "gp_port_library_type");
	lib_list = lt_dlsym (lh, "gp_port_library_list");
	if (!lib_type || !lib_list) {
		GP_LOG_D ("Could not find some functions in '%s': '%s'.",
			filename, lt_dlerror ());
		lt_dlclose (lh);
		return (0);
	}

	type = lib_type ();
	for (j = 0; j < list->count; j++)
		if (list->info[j]->type == type)
			break;
	if (j != list->count) {
		GP_LOG_D ("'%s' already loaded", filename);
		lt_dlclose (lh);
		return (0);
	}

	result = lib_list (list);
#if !defined(VALGRIND)
	lt_dlclose (lh);
#endif
	if (result < 0) {
		GP_LOG_E ("Error during assembling of port list: '%s' (%d).",
			gp_port_result_as_string (result), result);
	}

	if (old_size != list->count) {
		/*
		 * It doesn't matter if lib_list returned a failure code,
		 * at least some entries were added
		 */
		list->iolib_count++;

		for (j = old_size; j < list->count; j++){
			GP_LOG_D ("Loaded '%s' ('%s') from '%s'.",
				list->info[j]->name, list->info[j]->path,
				filename);
			list->info[j]->library_filename = strdup (filename);
		}
	}

	return (0);
}


/**
 * \brief Load system ports
 *
 * \param list a #GPPortInfoList
 *
 * Searches the system for io-drivers and appends them to the list. You would
 * normally call this function once after #gp_port_info_list_new and then
 * use this list in order to supply #gp_port_set_info with parameters or to do
 * autodetection.
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_list_load (GPPortInfoList *list)
{
	const char *iolibs_env = getenv(IOLIBDIR_ENV);
	const char *iolibs = (iolibs_env != NULL)?iolibs_env:IOLIBS;
	int result;

	C_PARAMS (list);

	GP_LOG_D ("Using ltdl to load io-drivers from '%s'...", iolibs);
	gpi_libltdl_lock();
	lt_dlinit ();
	lt_dladdsearchdir (iolibs);
	result = lt_dlforeachfile (iolibs, foreach_func, list);
	lt_dlexit ();
	gpi_libltdl_unlock();
	if (result < 0)
		return (result);
	if (list->iolib_count == 0) {
		GP_LOG_E ("No iolibs found in '%s'", iolibs);
		return GP_ERROR_LIBRARY;
	}
        return (GP_OK);
}

/**
 * \brief Number of ports in the list
 * \param list a #GPPortInfoList
 *
 * Returns the number of entries in the passed list.
 *
 * \return The number of entries or a gphoto2 error code
 **/
int
gp_port_info_list_count (GPPortInfoList *list)
{
	unsigned int count, i;

	C_PARAMS (list);

	GP_LOG_D ("Counting entries (%i available)...", list->count);

	/* Ignore generic entries */
	count = list->count;
	for (i = 0; i < list->count; i++)
		if (!strlen (list->info[i]->name))
			count--;

	GP_LOG_D ("%i regular entries available.", count);
	return count;
}

/**
 * \brief Lookup a specific path in the list
 *
 * \param list a #GPPortInfoList
 * \param path a path
 *
 * Looks for an entry in the list with the supplied path. If no exact match
 * can be found, a regex search will be performed in the hope some driver
 * claimed ports like "serial:*".
 *
 * \return The index of the entry or a gphoto2 error code
 **/
int
gp_port_info_list_lookup_path (GPPortInfoList *list, const char *path)
{
	unsigned int i;
	int result, generic;
#ifdef HAVE_REGEX
	regex_t pattern;
#ifdef HAVE_GNU_REGEX
	const char *rv;
#else
	regmatch_t match;
#endif
#endif

	C_PARAMS (list && path);

	GP_LOG_D ("Looking for path '%s' (%i entries available)...", path, list->count);

	/* Exact match? */
	for (generic = i = 0; i < list->count; i++)
		if (!strlen (list->info[i]->name))
			generic++;
		else if (!strcmp (list->info[i]->path, path))
			return (i - generic);

#ifdef HAVE_REGEX
	/* Regex match? */
	GP_LOG_D ("Starting regex search for '%s'...", path);
	for (i = 0; i < list->count; i++) {
		GPPortInfo newinfo;

		if (strlen (list->info[i]->name))
			continue;

		GP_LOG_D ("Trying '%s'...", list->info[i]->path);

		/* Compile the pattern */
#ifdef HAVE_GNU_REGEX
		memset (&pattern, 0, sizeof (pattern));
		rv = re_compile_pattern (list->info[i]->path,
					 strlen (list->info[i]->path), &pattern);
		if (rv) {
			GP_LOG_D ("%s", rv);
			continue;
		}
#else
		result = regcomp (&pattern, list->info[i]->path, REG_ICASE);
		if (result) {
			char buf[1024];
			if (regerror (result, &pattern, buf, sizeof (buf)))
				GP_LOG_E ("%s", buf);
			else
				GP_LOG_E ("regcomp failed");
			return (GP_ERROR_UNKNOWN_PORT);
		}
#endif

		/* Try to match */
#ifdef HAVE_GNU_REGEX
		result = re_match (&pattern, path, strlen (path), 0, NULL);
		regfree (&pattern);
		if (result < 0) {
			GP_LOG_D ("re_match failed (%i)", result);
			continue;
		}
#else
		result = regexec (&pattern, path, 1, &match, 0);
		regfree (&pattern);
		if (result) {
			GP_LOG_D ("regexec failed");
			continue;
		}
#endif
		gp_port_info_new (&newinfo);
		gp_port_info_set_type (newinfo, list->info[i]->type);
		newinfo->library_filename = strdup(list->info[i]->library_filename);
		gp_port_info_set_name (newinfo, _("Generic Port"));
		gp_port_info_set_path (newinfo, path);
		CR (result = gp_port_info_list_append (list, newinfo));
		return result;
	}
#endif /* HAVE_REGEX */

	return (GP_ERROR_UNKNOWN_PORT);
}

/**
 * \brief Look up a name in the list
 * \param list a #GPPortInfoList
 * \param name a name
 *
 * Looks for an entry in the list with the exact given name.
 *
 * \return The index of the entry or a gphoto2 error code
 **/
int
gp_port_info_list_lookup_name (GPPortInfoList *list, const char *name)
{
	unsigned int i, generic;

	C_PARAMS (list && name);

	GP_LOG_D ("Looking up entry '%s'...", name);

	/* Ignore generic entries */
	for (generic = i = 0; i < list->count; i++)
		if (!strlen (list->info[i]->name))
			generic++;
		else if (!strcmp (list->info[i]->name, name))
			return (i - generic);

	return (GP_ERROR_UNKNOWN_PORT);
}

/**
 * \brief Get port information of specific entry
 * \param list a #GPPortInfoList
 * \param n the index of the entry
 * \param info the returned information
 *
 * Returns a pointer to the current port entry.
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_list_get_info (GPPortInfoList *list, int n, GPPortInfo *info)
{
	int i;

	C_PARAMS (list && info);

	GP_LOG_D ("Getting info of entry %i (%i available)...", n, list->count);

	C_PARAMS ((n >= 0) && (unsigned int)n < list->count);

	/* Ignore generic entries */
	for (i = 0; i <= n; i++)
		if (!strlen (list->info[i]->name)) {
			n++;
			C_PARAMS ((unsigned int)n < list->count);
		}

	*info = list->info[n];
	return GP_OK;
}


/**
 * \brief Get name of a specific port entry
 * \param info a #GPPortInfo
 * \param name a pointer to a char* which will receive the name
 *
 * Retrieves the name of the passed in GPPortInfo, by reference.
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_get_name (GPPortInfo info, char **name) {
	*name = info->name;
	return GP_OK;
}

/**
 * \brief Set name of a specific port entry
 * \param info a #GPPortInfo
 * \param name a char* pointer which will receive the name
 *
 * Sets the name of the passed in GPPortInfo
 * This is a libgphoto2_port internal function.
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_set_name (GPPortInfo info, const char *name) {
	C_MEM (info->name = strdup (name));
	return GP_OK;
}

/**
 * \brief Get path of a specific port entry
 * \param info a #GPPortInfo
 * \param path a pointer to a char* which will receive the path
 *
 * Retrieves the path of the passed in GPPortInfo, by reference.
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_get_path (GPPortInfo info, char **path) {
	*path = info->path;
	return GP_OK;
}

/**
 * \brief Set path of a specific port entry
 * \param info a #GPPortInfo
 * \param path a char* pointer which will receive the path
 *
 * Sets the path of the passed in GPPortInfo
 * This is a libgphoto2_port internal function.
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_set_path (GPPortInfo info, const char *path) {
	C_MEM (info->path = strdup (path));
	return GP_OK;
}

/**
 * \brief Get type of a specific port entry
 * \param info a #GPPortInfo
 * \param type a pointer to a GPPortType variable which will receive the type
 *
 * Retrieves the type of the passed in GPPortInfo
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_get_type (GPPortInfo info, GPPortType *type) {
	*type = info->type;
	return GP_OK;
}

/**
 * \brief Set type of a specific port entry
 * \param info a #GPPortInfo
 * \param type a GPPortType variable which will has the type
 *
 * Sets the type of the passed in GPPortInfo
 * This is a libgphoto2_port internal function.
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_set_type (GPPortInfo info, GPPortType type) {
	info->type = type;
	return GP_OK;
}

/**
 * \brief Create a new portinfo
 * \param info pointer to a #GPPortInfo
 *
 * Allocates and initializes a GPPortInfo structure.
 * This is a libgphoto2_port internal function.
 *
 * \return a gphoto2 error code
 **/
int
gp_port_info_new (GPPortInfo *info) {
	C_MEM (*info = calloc (1, sizeof(struct _GPPortInfo)));
	return GP_OK;
}


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
