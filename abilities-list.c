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

CameraAbilitiesList *gp_abilities_list_new ()
{
        CameraAbilitiesList *list;
	
	list = malloc (sizeof (CameraAbilitiesList));
	if (!list)
		return (NULL);
	
	list->count = 0;
	list->abilities = NULL;

	return (list);
}

int gp_abilities_list_free (CameraAbilitiesList *list)
{
	int x;

	/* TODO: OS/2 Port crashes here... maybe compiler setting */

	for (x = 0; x < list->count; x++)
		free (list->abilities [x]);
	free (list);

	return (GP_OK);
}

int gp_abilities_list_dump (CameraAbilitiesList *list)
{
	int x;
	
	for (x = 0; x < list->count; x++) {
		gp_debug_printf (GP_DEBUG_LOW, "core", "Camera #%i:", x);
		gp_abilities_dump (list->abilities[x]);
	}
	
	return (GP_OK);
}

int gp_abilities_list_append (CameraAbilitiesList *list,
			      CameraAbilities *abilities)
{
	if (!list->abilities)
		list->abilities = malloc (sizeof (CameraAbilities*));
	else
		list->abilities = realloc (list->abilities,
				sizeof (CameraAbilities*) * (list->count + 1));

	if (!list->abilities)
		return (GP_ERROR_NO_MEMORY);
	
	list->abilities [list->count] = abilities;
	list->count++;
	
	return (GP_OK);
}
