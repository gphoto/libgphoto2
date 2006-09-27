/** \file
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
#include "gphoto2-list.h"
#include "gphoto2-port-log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gphoto2-result.h"

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}


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

	*list = malloc (sizeof (CameraList));
        if (!*list)
		return (GP_ERROR_NO_MEMORY);
	memset (*list, 0, sizeof (CameraList));

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
	CHECK_NULL (list);

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
	CHECK_NULL (list);

	list->ref_count--;

	if (!list->ref_count)
		gp_list_free (list);

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
	CHECK_NULL (list);
	
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
	CHECK_NULL (list);

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
	CHECK_NULL (list);

	if (list->count == MAX_ENTRIES) {
		gp_log (GP_LOG_ERROR, "gphoto2-list", "gp_list_append: "
		"Tried to add more than %d entries to the list, reporting out of memory",
		MAX_ENTRIES);
		return (GP_ERROR_NO_MEMORY);
	}

	if (name) {
		/* check that the value fits */
		const size_t buf_len = sizeof (list->entry[list->count].name);
		const size_t str_len = strlen (name);
		if (str_len >= buf_len) {
			gp_log (GP_LOG_ERROR, "gphoto2-list", 
				"gp_list_append: "
				"'name' value too long (%d >= %d)",
				str_len, buf_len);
			return (GP_ERROR_NO_MEMORY);		
		}
		/* set the value */
		strncpy (list->entry[list->count].name, name, buf_len);
		list->entry[list->count].name[buf_len-1] = '\0';
	}
	if (value) {
		/* check that the value fits */
		const size_t buf_len = sizeof (list->entry[list->count].value);
		const size_t str_len = strlen (value);
		if (str_len >= buf_len) {
			gp_log (GP_LOG_ERROR, "gphoto2-list", 
				"gp_list_append: "
				"'value' value too long (%d >= %d)",
				str_len, buf_len);
			return (GP_ERROR_NO_MEMORY);		
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

	CHECK_NULL (list);

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
	CHECK_NULL (list);

        return (list->count);
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
	CHECK_NULL (list && name);

	if (index < 0 || index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

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
	CHECK_NULL (list && value);

	if (index < 0 || index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

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
	CHECK_NULL (list && value);

	if (index < 0 || index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	do {
		/* check that the value fits */
		const size_t buf_len = sizeof (list->entry[index].value);
		const size_t str_len = strlen (value);
		if (str_len >= buf_len) {
			gp_log (GP_LOG_ERROR, "gphoto2-list", 
				"gp_list_append: "
				"'value' value too long (%d >= %d)",
				str_len, buf_len);
			return (GP_ERROR_NO_MEMORY);		
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
	CHECK_NULL (list && name);

	if (index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	do {
		/* check that the value fits */
		const size_t buf_len = sizeof (list->entry[index].name);
		const size_t str_len = strlen (name);
		if (str_len >= buf_len) {
			gp_log (GP_LOG_ERROR, "gphoto2-list", 
				"gp_list_append: "
				"'name' value too long (%d >= %d)",
				str_len, buf_len);
			return (GP_ERROR_NO_MEMORY);		
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

	CHECK_NULL (list && format);

	gp_list_reset (list);
	for (x = 0; x < count; x++) {
		snprintf (buf, sizeof (buf), format, x + 1);
		CHECK_RESULT (gp_list_append (list, buf, NULL));
	}

	return (GP_OK);
}
