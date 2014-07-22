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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _BSD_SOURCE

#include "config.h"
#include <gphoto2/gphoto2-list.h>
#include <gphoto2/gphoto2-port-log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-result.h>

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

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
	C_PARAMS (list);

	C_MEM (*list = calloc (1, sizeof (CameraList)));

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
	C_PARAMS (list && list->ref_count);

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
	C_PARAMS (list && list->ref_count);

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
	C_PARAMS (list && list->ref_count);

	for (i=0;i<list->used;i++) {
		free (list->entry[i].name);
		list->entry[i].name = NULL;
		free (list->entry[i].value);
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
	C_PARAMS (list && list->ref_count);

	for (i=0;i<list->used;i++) {
		free (list->entry[i].name);
		list->entry[i].name = NULL;
		free (list->entry[i].value);
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
	C_PARAMS (list && list->ref_count);

	if (list->used == list->max) {
		C_MEM (list->entry = realloc(list->entry,(list->max+100)*sizeof(struct _entry)));
		list->max += 100;
	}

	if (name) {
		C_MEM (list->entry[list->used].name = strdup (name));
	} else {
		list->entry[list->used].name = NULL;
	}
	if (value) {
		C_MEM (list->entry[list->used].value = strdup (value));
	} else {
		list->entry[list->used].value = NULL;
	}
        list->used++;
        return (GP_OK);
}

static int
cmp_list (const void *a, const void *b) {
        const struct _entry *ca = a;
        const struct _entry *cb = b;

        return strcmp (ca->name, cb->name);
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
	C_PARAMS (list && list->ref_count);

	qsort (list->entry, list->used, sizeof(list->entry[0]), cmp_list);
	return GP_OK;
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
	C_PARAMS (list && list->ref_count);

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
	C_PARAMS (list && list->ref_count);
	C_PARAMS (name);

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
	C_PARAMS (list && list->ref_count);
	C_PARAMS (name);
	C_PARAMS (0 <= index && index < list->used);

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
	C_PARAMS (list && list->ref_count);
	C_PARAMS (value);
	C_PARAMS (0 <= index && index < list->used);

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
	C_PARAMS (list && list->ref_count);
	C_PARAMS (value);
	C_PARAMS (0 <= index && index < list->used);

	C_MEM (newval = strdup(value));
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
	C_PARAMS (list && list->ref_count);
	C_PARAMS (name);
	C_PARAMS (0 <= index && index < list->used);

	C_MEM (newname = strdup(name));
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

	C_PARAMS (list && list->ref_count);
	C_PARAMS (format);

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
