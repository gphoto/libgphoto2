/* list.c
 *
 * Copyright (C) 2000 Scott Fritzinger
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
#include "gphoto2-list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gphoto2-result.h"

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

/**
 * gp_list_new:
 * @list:
 *
 * Creates a new #CameraList.
 *
 * Return value: a gphoto2 error code
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
 * gp_list_ref:
 * @list: a #CameraList
 *
 * Increments the reference count of the @list.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_list_ref (CameraList *list)
{
	CHECK_NULL (list);

	list->ref_count++;

	return (GP_OK);
}

/**
 * gp_list_unref:
 * @list: a #CameraList
 *
 * Decrements the reference count of the @list. If there are no references
 * left, the @list will be freed.
 *
 * Return value: a gphoto2 error code
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
 * gp_list_free:
 * @list: the #CameraList to be freed
 *
 * Frees the @list. It is recommended to use #gp_list_unref instead.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_list_free (CameraList *list) 
{
	CHECK_NULL (list);
	
	free (list);

        return (GP_OK);
}

/**
 * gp_list_reset:
 * @list: a #CameraList
 *
 * Resets the @list and removes all entries.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_list_reset (CameraList *list)
{
	CHECK_NULL (list);

	list->count = 0;

	return (GP_OK);
}

/**
 * gp_list_append:
 * @list: a #CameraList
 * @name: the name of the entry to append
 * @value: the value of the entry to append
 *
 * Appends @name and @value to the @list.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_list_append (CameraList *list, const char *name, const char *value)
{
	CHECK_NULL (list);

	if (list->count == MAX_ENTRIES)
		return (GP_ERROR_NO_MEMORY);

	if (name)
		strncpy (list->entry[list->count].name, name,
			 sizeof (list->entry[list->count].name));
	if (value)
		strncpy (list->entry[list->count].value, value,
			 sizeof (list->entry[list->count].value));

        list->count++;

        return (GP_OK);
}

static void
exchange (CameraList *list, int x, int y)
{
	char name  [128];
	char value [128];

	strcpy (name,  list->entry[x].name);
	strcpy (value, list->entry[x].value);
	strcpy (list->entry[x].name,  list->entry[y].name);
	strcpy (list->entry[x].value, list->entry[y].value);
	strcpy (list->entry[y].name,  name);
	strcpy (list->entry[y].value, value);
}

/**
 * gp_list_sort:
 * @list: a #CameraList
 *
 * Sorts the @list entries with respect to the names.
 *
 * Return value: a gphoto2 error code
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
 * gp_list_count:
 * @list: a #CameraList
 *
 * Counts the entries in the @list.
 * 
 * Return value: a gphoto2 error code
 **/
int
gp_list_count (CameraList *list)
{
	CHECK_NULL (list);

        return (list->count);
}

/**
 * gp_list_get_name:
 * @list: a #CameraList
 * @index: index of the entry
 * @name:
 *
 * Retrieves the @name of entry with @index.
 *
 * Return value: a gphoto2 error code.
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
 * gp_list_get_value:
 * @list: a #CameraList
 * @index: index of the entry
 * @value:
 *
 * Retrieves the value of entry with @index.
 *
 * Return value: a gphoto2 error code
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
 * gp_list_set_value:
 * @list: a #CameraList
 * @index: index of the entry
 * @value: the value to be set
 *
 * Sets the @value of an entry.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_list_set_value (CameraList *list, int index, const char *value)
{
	CHECK_NULL (list && value);

	if (index < 0 || index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	strcpy (list->entry[index].value, value);

	return (GP_OK);
}

/**
 * gp_list_set_name:
 * @list: a #CameraList
 * @index: index of entry
 * @name: name to be set
 *
 * Sets the name of an entry.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_list_set_name (CameraList *list, int index, const char *name)
{
	CHECK_NULL (list && name);

	if (index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	strcpy (list->entry[index].name, name);

	return (GP_OK);
}

/**
 * gp_list_populate:
 * @list: a #CameraList
 * @format: the format
 * @count: number of entries to be added to the @list
 *
 * Adds @count entries to the list. Typically, this function is called by
 * a camera driver when there is no way of retrieving the real name of a
 * picture. In this case, when asked for a file list
 * (see #CameraFilesystemListFunc), the list is populated with dummy
 * names generated by this function.
 *
 * Return value: a gphoto2 error code
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
