/** \file gphoto2-list.c
 *
 * \author Copyright 2000 Scott Fritzinger
 *
 * \note
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \note
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * \note
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <gphoto2/gphoto2-list.h>
#include <gphoto2/gphoto2-port-log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-result.h>

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

/** check that the list is valid */
#define CHECK_LIST(list)				\
  do {							\
    if (NULL == list) {					\
      return (GP_ERROR_BAD_PARAMETERS);			\
    } else if (list->ref_count == 0) {			\
      /* catch calls to already freed list */		\
      return (GP_ERROR_BAD_PARAMETERS);			\
    }							\
  } while (0)


/**
 * Whether to preserve binary compatibility for structure internals.
 *
 * Question: Find out whether any libgphoto2 frontend relies on those
 *           internals.
 * Answer: They do, at least by instantiation a "struct CameraList foo;".
 *
 * Binary compatibility and problems:
 *   * fixed string size for name and value
 *   * fixed max number of entries
 *   * some apps instantiate a "struct CameraList foo;" directly and
 *     thus reserve a specific amount of memory
 *   * FIXME: Do frontends rely on the memory layout and size of their 
 *     "struct CameraList" or "struct CameraList *" when accessing
 *     its members? They *SHOULD* use access functions.
 */
#undef CAMERALIST_STRUCT_COMPATIBILITY
#define CAMERALIST_STRUCT_COMPATIBILITY


/**
 * Internal _CameraList data structure
 **/

#ifdef CAMERALIST_STRUCT_COMPATIBILITY

#define MAX_ENTRIES 1024
#define MAX_LIST_STRING_LENGTH 128
struct _CameraList {
	int  count;
	struct {
		char name  [MAX_LIST_STRING_LENGTH];
		char value [MAX_LIST_STRING_LENGTH];
	} entry [MAX_ENTRIES];
	int ref_count;
};

/* check that the given index is in the valid range for this list */
#define CHECK_INDEX_RANGE(list, index)			\
  do {							\
    if (index < 0 || index >= list->count) {		\
      return (GP_ERROR_BAD_PARAMETERS);			\
    }							\
  } while (0)

#else /* !CAMERALIST_STRUCT_COMPATIBILITY */

#error The !CAMERALIST_STRUCT_COMPATIBILITY part has not been implemented yet!

struct _CamaraSubList {
	int count;
  foo bar;
};

struct _CameraList {
	int ref_count;
};

#endif /* !CAMERALIST_STRUCT_COMPATIBILITY */


/**
 * Creates a new #CameraList.
 *
 * @param list
 * @return a gphoto2 error code
 *
 **/
int
gp_list_new (CameraList **list)
{
	CHECK_NULL (list);

	(*list) = malloc (sizeof (CameraList));
        if (!(*list))
		return (GP_ERROR_NO_MEMORY);
	memset ((*list), 0, sizeof (CameraList));

	(*list)->ref_count = 1;

	return (GP_OK);
}

/**
 * Increments the reference count of the @list.
 *
 * @param list a #CameraList
 * @return a gphoto2 error code.
 *
 **/
int
gp_list_ref (CameraList *list)
{
	CHECK_LIST (list);

	list->ref_count++;

	return (GP_OK);
}

/**
 * Decrements the reference count of the \c list.
 *
 * @param list a #CameraList
 * @return a gphoto2 error code
 *
 * If there are no references left, the \c list will be freed.
 *
 **/
int
gp_list_unref (CameraList *list)
{
	CHECK_LIST (list);

	if (list->ref_count == 1) /* time to free */
		gp_list_free (list);
	else
		list->ref_count--;
	return (GP_OK);
}

/**
 * Frees the \c list. It is recommended to use #gp_list_unref instead.
 *
 * @param list the #CameraList to be freed
 * @return a gphoto2 error code
 *
 **/
int
gp_list_free (CameraList *list) 
{
	CHECK_LIST (list);

	/* Mark this list as having been freed. That may help us
	 * prevent access to already freed lists.
	 */
	list->ref_count = 0;
	
	free (list);

        return (GP_OK);
}

/**
 * Resets the \c list and removes all entries.
 *
 * @param list a #CameraList
 * @return a gphoto2 error code
 *
 **/
int
gp_list_reset (CameraList *list)
{
	CHECK_LIST (list);

	list->count = 0;

	return (GP_OK);
}

/**
 * Appends \c name and \c value to the \c list.
 *
 * @param list a #CameraList
 * @param name the name of the entry to append
 * @param value the value of the entry to append
 * @return a gphoto2 error code
 *
 **/
int
gp_list_append (CameraList *list, const char *name, const char *value)
{
	CHECK_LIST (list);

	if (list->count == MAX_ENTRIES) {
		gp_log (GP_LOG_ERROR, "gphoto2-list", "gp_list_append: "
		"Tried to add more than %d entries to the list, reporting error.",
		MAX_ENTRIES);
		return (GP_ERROR_FIXED_LIMIT_EXCEEDED);
	}

	if (name) {
		/* check that the 'name' value fits */
		const size_t buf_len = sizeof (list->entry[list->count].name);
		const size_t str_len = strlen (name);
		if (str_len >= buf_len) {
			gp_log (GP_LOG_ERROR, "gphoto2-list", 
				"gp_list_append: "
				"'name' value too long (%d >= %d)",
				str_len, buf_len);
			return (GP_ERROR_FIXED_LIMIT_EXCEEDED);
		}
		/* set the value */
		strncpy (list->entry[list->count].name, name, buf_len);
		list->entry[list->count].name[buf_len-1] = '\0';
	}
	if (value) {
		/* check that the 'value' value fits */
		const size_t buf_len = sizeof (list->entry[list->count].value);
		const size_t str_len = strlen (value);
		if (str_len >= buf_len) {
			gp_log (GP_LOG_ERROR, "gphoto2-list", 
				"gp_list_append: "
				"'value' value too long (%d >= %d)",
				str_len, buf_len);
			return (GP_ERROR_FIXED_LIMIT_EXCEEDED);
		}
		/* set the value */
		strncpy (list->entry[list->count].value, value, buf_len);
		list->entry[list->count].value[buf_len-1] = '\0';
	}

        list->count++;

        return (GP_OK);
}

/**
  * @internal
  */
static void
exchange (CameraList *list, int x, int y)
{
	char name  [128];
	char value [128];

	/* here we use memcpy to avoid unterminated strings *
	 * stored in the list. 128 is hardcoded. use a constant *
	 * for cleaness */
	memcpy (name,  list->entry[x].name, 128);
	memcpy (value, list->entry[x].value, 128);
	memcpy (list->entry[x].name,  list->entry[y].name, 128);
	memcpy (list->entry[x].value, list->entry[y].value, 128);
	memcpy (list->entry[y].name,  name, 128);
	memcpy (list->entry[y].value, value, 128);
}

/**
 * Sorts the \c list entries with respect to the names.
 *
 * @param list a #CameraList
 * @return a gphoto2 error code
 *
 **/
int
gp_list_sort (CameraList *list)
{
	int x, y, z;

	CHECK_LIST (list);

	for (x = 0; x < list->count - 1; x++)
		for (y = x + 1; y < list->count; y++) {
			z = strcmp (list->entry[x].name, list->entry[y].name);
			if (z > 0)
				exchange (list, x, y);
		}

	return (GP_OK);
}

/**
 * Counts the entries in the \c list.
 * 
 * @param list a #CameraList
 * @return a gphoto2 error code
 *
 **/
int
gp_list_count (CameraList *list)
{
	CHECK_LIST (list);

        return (list->count);
}

/**
 * Retrieves the \c index of an arbitrary entry with \c name.
 *
 * @param list a #CameraList
 * @param index pointer to the result index (may be NULL, only set if found)
 * @param name name of the entry
 * @return a gphoto2 error code: GP_OK if found.
 *
 * No guarantees as to the speed of the search, or in what sequence
 * the list is searched.
 *
 **/
int
gp_list_find_by_name (CameraList *list, int *index, const char *name)
{
	int i;
	CHECK_LIST (list);
	CHECK_NULL (name);

	/* We search backwards because our only known user
	 * camlibs/ptp2/library.c thinks this is faster
	 */
	for (i=list->count-1; i >= 0; i--) {
	  if (0==strcmp(list->entry[i].name, name)) {
	    if (index) {
	      (*index) = i;
	    }
	    return (GP_OK);
	  }
	}

	return (GP_ERROR);
}

/**
 * Retrieves the \c name of entry with \c index.
 *
 * @param list a #CameraList
 * @param index index of the entry
 * @param name
 * @return a gphoto2 error code.
 *
 **/
int
gp_list_get_name (CameraList *list, int index, const char **name)
{
	CHECK_LIST (list);
	CHECK_NULL (name);
	CHECK_INDEX_RANGE (list, index);

	*name = list->entry[index].name;

	return (GP_OK);
}

/**
 * Retrieves the value of entry with \c index.
 *
 * @param list a #CameraList
 * @param index index of the entry
 * @param value
 * @return a gphoto2 error code
 *
 **/
int
gp_list_get_value (CameraList *list, int index, const char **value)
{
	CHECK_LIST (list);
	CHECK_NULL (value);
	CHECK_INDEX_RANGE (list, index);

	*value = list->entry[index].value;

	return (GP_OK);
}

/**
 * Sets the \c value of an entry.
 *
 * @param list a #CameraList
 * @param index index of the entry
 * @param value the value to be set
 * @return a gphoto2 error code
 *
 **/
int
gp_list_set_value (CameraList *list, int index, const char *value)
{
	CHECK_LIST (list);
	CHECK_NULL (value);
	CHECK_INDEX_RANGE (list, index);

	do {
		/* check that the value fits */
		const size_t buf_len = sizeof (list->entry[index].value);
		const size_t str_len = strlen (value);
		if (str_len >= buf_len) {
			gp_log (GP_LOG_ERROR, "gphoto2-list", 
				"gp_list_append: "
				"'value' value too long (%d >= %d)",
				str_len, buf_len);
			return (GP_ERROR_FIXED_LIMIT_EXCEEDED);
		}
	} while (0);

	/* set the value */
	strcpy (list->entry[index].value, value);

	return (GP_OK);
}

/**
 * Sets the name of an entry.
 *
 * @param list a #CameraList
 * @param index index of entry
 * @param name name to be set
 * @return a gphoto2 error code
 *
 **/
int
gp_list_set_name (CameraList *list, int index, const char *name)
{
	CHECK_LIST (list);
	CHECK_NULL (name);
	CHECK_INDEX_RANGE (list, index);

	do {
		/* check that the value fits */
		const size_t buf_len = sizeof (list->entry[index].name);
		const size_t str_len = strlen (name);
		if (str_len >= buf_len) {
			gp_log (GP_LOG_ERROR, "gphoto2-list", 
				"gp_list_append: "
				"'name' value too long (%d >= %d)",
				str_len, buf_len);
			return (GP_ERROR_FIXED_LIMIT_EXCEEDED);
		}
	} while (0);

	/* set the value */
	strcpy (list->entry[index].name, name);

	return (GP_OK);
}

/**
 * Adds \c count entries to the list.
 *
 * @param list a #CameraList
 * @param format the format
 * @param count number of entries to be added to the @list
 * @return a gphoto2 error code
 *
 * Typically, this function is called by a camera driver when there is no way
 * of retrieving the real name of a picture. In this case, when asked for a
 * file list (see #CameraFilesystemListFunc), the list is populated with dummy
 * names generated by this function.
 *
 **/
int
gp_list_populate  (CameraList *list, const char *format, int count)
{
	int x;
	char buf[1024];

	CHECK_LIST (list);
	CHECK_NULL (format);

	gp_list_reset (list);
	for (x = 0; x < count; x++) {
		snprintf (buf, sizeof (buf), format, x + 1);
		CHECK_RESULT (gp_list_append (list, buf, NULL));
	}

	return (GP_OK);
}



/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:nil
 * End:
 */
