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

#include "gphoto2-list.h"

#include <stdlib.h>
#include <string.h>

#include "gphoto2-result.h"

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

int
gp_list_new (CameraList **list)
{
	CHECK_NULL (list);

	*list = malloc (sizeof (CameraList));
        if (!*list)
		return (GP_ERROR_NO_MEMORY);

	memset (*list, 0, sizeof (CameraList));

	return (GP_OK);
}

int
gp_list_free (CameraList *list) 
{
	CHECK_NULL (list);
	
	free (list);

        return (GP_OK);
}

int
gp_list_remove_all (CameraList *list)
{
	CHECK_NULL (list);

	list->count = 0;

	return (GP_OK);
}

int
gp_list_append (CameraList *list, const char *name, const char *value)
{
	CHECK_NULL (list);

	if (list->count == MAX_ENTRIES)
		return (GP_ERROR_NO_MEMORY);

	if (name)
	        strcpy (list->entry[list->count].name, name);
	if (value)
		strcpy (list->entry[list->count].value, value);

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

int
gp_list_count (CameraList *list)
{
	CHECK_NULL (list);

        return (list->count);
}

int
gp_list_get_name (CameraList *list, int index, const char **name)
{
	CHECK_NULL (list && name);

	if (index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	*name = list->entry[index].name;

	return (GP_OK);
}

int
gp_list_get_value (CameraList *list, int index, const char **value)
{
	CHECK_NULL (list && value);

	if (index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	*value = list->entry[index].value;

	return (GP_OK);
}

int
gp_list_set_value (CameraList *list, int index, const char *value)
{
	CHECK_NULL (list && value);

	if (index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	strcpy (list->entry[index].value, value);

	return (GP_OK);
}

int
gp_list_set_name (CameraList *list, int index, const char *name)
{
	CHECK_NULL (list && name);

	if (index >= list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	strcpy (list->entry[index].name, name);

	return (GP_OK);
}
