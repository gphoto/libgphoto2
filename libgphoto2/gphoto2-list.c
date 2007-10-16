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
 * Internal _CameraList data structure
 **/

struct _entry {
	char *name;
	char *value;
};

struct _CameraList {
	int	used;	/* used entries */
	int	max;	/* allocated entries */
	struct _entry *entry;
	int	ref_count;
};

/* check that the given index is in the valid range for this list */
#define CHECK_INDEX_RANGE(list, index)			\
  do {							\
    if (index < 0 || index >= list->used) {		\
      return (GP_ERROR_BAD_PARAMETERS);			\
    }							\
  } while (0)


/**
 * \brief Creates a new #CameraList.
 *
 * \param list
 * \return a gphoto2 error code
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
 * \brief Increments the reference count of the list.
 *
 * \param list a #CameraList
 * \return a gphoto2 error code.
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
 * \brief Decrements the reference count of the \c list.
 *
 * \param list a #CameraList
 * \return a gphoto2 error code
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
 * \param list the #CameraList to be freed
 * \return a gphoto2 error code
 *
 **/
int
gp_list_free (CameraList *list) 
{
	int	i;
	CHECK_LIST (list);

	for (i=0;i<list->used;i++) {
		if (list->entry[i].name) free (list->entry[i].name);
		list->entry[i].name = NULL;
		if (list->entry[i].value) free (list->entry[i].value);
		list->entry[i].value = NULL;
	}
	free (list->entry);
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
 * \param list a #CameraList
 * \return a gphoto2 error code
 *
 **/
int
gp_list_reset (CameraList *list)
{
	int i;
	CHECK_LIST (list);

	for (i=0;i<list->used;i++) {
		if (list->entry[i].name) free (list->entry[i].name);
		list->entry[i].name = NULL;
		if (list->entry[i].value) free (list->entry[i].value);
		list->entry[i].value = NULL;
	}
	/* keeps -> entry allocated for reuse. */
	list->used = 0;
	return (GP_OK);
}

/**
 * Appends \c name and \c value to the \c list.
 *
 * \param list a #CameraList
 * \param name the name of the entry to append
 * \param value the value of the entry to append
 * \return a gphoto2 error code
 *
 **/
int
gp_list_append (CameraList *list, const char *name, const char *value)
{
	CHECK_LIST (list);

	if (list->used == list->max) {
		struct _entry *new;

		new = realloc(list->entry,(list->max+100)*sizeof(struct _entry));
		if (!new)
			return GP_ERROR_NO_MEMORY;
		list->entry = new;
		list->max += 100;
	}

	if (name) {
		list->entry[list->used].name = strdup (name);
		if (!list->entry[list->used].name)
			return GP_ERROR_NO_MEMORY;
	} else {
		list->entry[list->used].name = NULL;
	}
	if (value) {
		list->entry[list->used].value = strdup (value);
		if (!list->entry[list->used].value)
			return GP_ERROR_NO_MEMORY;
	} else {
		list->entry[list->used].value = NULL;
	}
        list->used++;
        return (GP_OK);
}

/**
  * \internal
  */
static void
exchange (CameraList *list, int x, int y)
{
	char *tmp;

	tmp = list->entry[x].name;
	list->entry[x].name = list->entry[y].name;
	list->entry[y].name = tmp;

	tmp = list->entry[x].value;
	list->entry[x].value = list->entry[y].value;
	list->entry[y].value = tmp;
}

/**
 * Sorts the \c list entries with respect to the names.
 *
 * \param list a #CameraList
 * \return a gphoto2 error code
 *
 **/
int
gp_list_sort (CameraList *list)
{
	int x, y, z;
	CHECK_LIST (list);

	for (x = 0; x < list->used - 1; x++)
		for (y = x + 1; y < list->used; y++) {
			z = strcmp (list->entry[x].name, list->entry[y].name);
			if (z > 0)
				exchange (list, x, y);
		}

	return (GP_OK);
}

/**
 * Counts the entries in the \c list.
 * 
 * \param list a #CameraList
 * \return a gphoto2 error code
 *
 **/
int
gp_list_count (CameraList *list)
{
	CHECK_LIST (list);

        return (list->used);
}

/**
 * Retrieves the \c index of an arbitrary entry with \c name.
 *
 * \param list a #CameraList
 * \param index pointer to the result index (may be NULL, only set if found)
 * \param name name of the entry
 * \return a gphoto2 error code: GP_OK if found.
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
	for (i=list->used-1; i >= 0; i--) {
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
 * \param list a #CameraList
 * \param index index of the entry
 * \param name
 * \return a gphoto2 error code.
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
 * \param list a #CameraList
 * \param index index of the entry
 * \param value
 * \return a gphoto2 error code
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
 * \param list a #CameraList
 * \param index index of the entry
 * \param value the value to be set
 * \return a gphoto2 error code
 *
 **/
int
gp_list_set_value (CameraList *list, int index, const char *value)
{
	char *newval;
	CHECK_LIST (list);
	CHECK_NULL (value);
	CHECK_INDEX_RANGE (list, index);

	newval = strdup(value);
	if (!newval)
		return GP_ERROR_NO_MEMORY;
	if (list->entry[index].value)
		free (list->entry[index].value);
	list->entry[index].value = newval;
	return (GP_OK);
}

/**
 * Sets the name of an entry.
 *
 * \param list a #CameraList
 * \param index index of entry
 * \param name name to be set
 * \return a gphoto2 error code
 *
 **/
int
gp_list_set_name (CameraList *list, int index, const char *name)
{
	char *newname;
	CHECK_LIST (list);
	CHECK_NULL (name);
	CHECK_INDEX_RANGE (list, index);

	newname = strdup(name);
	if (!newname)
		return GP_ERROR_NO_MEMORY;
	if (list->entry[index].name)
		free (list->entry[index].name);
	list->entry[index].name = newname;
	return (GP_OK);
}

/**
 * Adds \c count entries to the list.
 *
 * \param list a #CameraList
 * \param format the format
 * \param count number of entries to be added to the list
 * return a gphoto2 error code
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
