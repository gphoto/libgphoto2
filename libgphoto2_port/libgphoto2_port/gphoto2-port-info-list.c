/** \file
 *
 * \author Copyright 2001 Lutz Müller <lutz@users.sf.net>
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

#include "config.h"
#include <gphoto2/gphoto2-port-info-list.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_REGEX
#include <regex.h>
#else
#error We need regex.h, but it has not been detected.
#endif

#include <ltdl.h>

#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-library.h>
#include <gphoto2/gphoto2-port-log.h>

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
#  define bind_textdomain_codeset(Domain,codeset) (codeset)
#  define _(String) (String)
#  define N_(String) (String)
#endif

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

#define CHECK_NULL(x) {if (!(x)) return (GP_ERROR_BAD_PARAMETERS);}
#define CR(x)         {int r=(x);if (r<0) return (r);}


/**
 * \brief Specify codeset for translations
 *
 * This function specifies the codeset that are used for the translated
 * strings that are passed back by the libgphoto2_port functions.
 *
 * This function is called by the gp_message_codeset() function, there is
 * no need to call it yourself.
 * 
 * \param codeset new codeset to use
 * \return the previous codeset
 */
const char*
gp_port_message_codeset (const char *codeset) {
	return bind_textdomain_codeset (GETTEXT_PACKAGE, codeset);
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
 * \brief Append a portinfo to the port information list
 *
 * \param list a #GPPortInfoList
 * \param info the info to append
 *
 * Appends an entry to the list. This function is typically called by
 * an io-driver during #gp_port_library_list. If you leave info.name blank,
 * #gp_port_info_list_lookup_path will try to match non-existent paths
 * against info.path and - if successfull - will append this entry to the 
 * list.
 *
 * \note This returns index - number of generic entries, not the correct index.
 *
 * \return A non-negative number or a gphoto2 error code
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

	list->iolib_count++;

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

	CHECK_NULL (list);

	gp_log (GP_LOG_DEBUG, "gphoto2-port-info-list",
		_("Using ltdl to load io-drivers from '%s'..."),
		iolibs);
	lt_dlinit ();
	lt_dladdsearchdir (iolibs);
	result = lt_dlforeachfile (iolibs, foreach_func, list);
	lt_dlexit ();
	if (result < 0)
		return (result);
	if (list->iolib_count == 0) {
		gp_log (GP_LOG_ERROR, "No iolibs found in '%s'", iolibs);
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
		GPPortInfo newinfo;

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
		memcpy (&newinfo, &list->info[i], sizeof(newinfo));
		strncpy (newinfo.path, path, sizeof (newinfo.path));
		strncpy (newinfo.name, _("Generic Port"), sizeof (newinfo.name));
		CR (result = gp_port_info_list_append (list, newinfo));
		return result;
	}

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
 * \brief Get port information of specific entry
 * \param list a #GPPortInfoList
 * \param n the index of the entry
 * \param info the returned information
 *
 * Retreives an entry from the list and stores it into info.
 *
 * \return a gphoto2 error code
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


#ifdef _GPHOTO2_INTERNAL_CODE
/**
 * \brief Internal map between GP_PORT and name
 * 
 * Internal map between the GP_PORT_xxx enumeration and
 * string names (usb,serial,disk,ptpip,...).
 */
const StringFlagItem gpi_gphoto_port_type_map[] = {
	{ "none",   GP_PORT_NONE },
	{ "serial", GP_PORT_SERIAL },
	{ "usb",    GP_PORT_USB },
	{ "disk",   GP_PORT_DISK },
	{ "ptpip",  GP_PORT_PTPIP },
	{ NULL, 0 },
};
#endif /* _GPHOTO2_INTERNAL_CODE */
