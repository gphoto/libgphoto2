/* abilities-list.c
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

#include "gphoto2-abilities-list.h"

#include <stdlib.h>

#include "gphoto2-core.h"
#include "gphoto2-result.h"

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_MEM(m)         {if (!(m)) return (GP_ERROR_NO_MEMORY);}

int
gp_abilities_list_new (CameraAbilitiesList **list)
{
	CHECK_NULL (list);

	CHECK_MEM (*list = malloc (sizeof (CameraAbilitiesList)));

	(*list)->count = 0;
	(*list)->abilities = NULL;

	return (GP_OK);
}

int
gp_abilities_list_free (CameraAbilitiesList *list)
{
	int x;

	CHECK_NULL (list);

	/* TODO: OS/2 Port crashes here... maybe compiler setting */

	for (x = 0; x < list->count; x++)
		free (list->abilities [x]);
	free (list);

	return (GP_OK);
}

int
gp_abilities_list_dump (CameraAbilitiesList *list)
{
	int x;

	CHECK_NULL (list);
	
	for (x = 0; x < list->count; x++) {
		gp_debug_printf (GP_DEBUG_LOW, "core", "Camera #%i:", x);
		gp_abilities_dump (list->abilities[x]);
	}
	
	return (GP_OK);
}

int
gp_abilities_list_append (CameraAbilitiesList *list, CameraAbilities *abilities)
{
	CHECK_NULL (list && abilities);

	if (!list->abilities)
		list->abilities = malloc (sizeof (CameraAbilities*));
	else
		list->abilities = realloc (list->abilities,
				sizeof (CameraAbilities*) * (list->count + 1));
	CHECK_MEM (list->abilities);

	list->abilities [list->count] = abilities;
	list->count++;

	return (GP_OK);
}

int
gp_abilities_list_count (CameraAbilitiesList *list)
{
	CHECK_NULL (list);

	return (list->count);
}

int
gp_abilities_list_sort (CameraAbilitiesList *list)
{
	CameraAbilities *t;
	int x, y;

	CHECK_NULL (list);

	for (x = 0; x < list->count - 1; x++)
		for (y = x + 1; y < list->count; y++)
			if (strcmp (list->abilities[x]->model,
				    list->abilities[y]->model) > 0) {
				t = list->abilities[x];
				list->abilities[x] = list->abilities[y];
				list->abilities[y] = t;
			}

	return (GP_OK);
}
