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

struct _CameraAbilitiesList {
	int count;
	CameraAbilities **abilities;
};

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
gp_abilities_list_dump_libs (CameraAbilitiesList *list)
{
	int x;

	CHECK_NULL (list);

	for (x = 0; x < list->count; x++)
		gp_debug_printf (GP_DEBUG_LOW, "core", "\t\"%s\" uses %s",
				 list->abilities[x]->model,
				 list->abilities[x]->library);
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

int
gp_abilities_list_lookup_id (CameraAbilitiesList *list, const char *id)
{
	int x;

	CHECK_NULL (list && id);

	for (x = 0; x < list->count; x++)
		if (!strcmp (list->abilities[x]->id, id))
			return (x);

	return (GP_ERROR);
}

int
gp_abilities_list_set_id (CameraAbilitiesList *list, int index,
			  const char *id)
{
	CHECK_NULL (list && id);

	if (index > list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	strcpy (list->abilities[index]->id, id);

	return (GP_OK);
}

int
gp_abilities_list_set_library (CameraAbilitiesList *list, int index,
			       const char *library)
{
	CHECK_NULL (list && library);

	if (index > list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	strcpy (list->abilities[index]->library, library);

	return (GP_OK);
}

int
gp_abilities_list_get_model (CameraAbilitiesList *list, int index,
			     const char **model)
{
	CHECK_NULL (list && model);

	if (index > list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	*model = list->abilities[index]->model;

	return (GP_OK);
}

int
gp_abilities_list_lookup_model (CameraAbilitiesList *list, const char *model)
{
	int x;

	CHECK_NULL (list && model);

	for (x = 0; x < list->count; x++)
		if (!strcmp (list->abilities[x]->model, model))
			return (x);

	return (GP_ERROR_MODEL_NOT_FOUND);
}

int
gp_abilities_list_get_vendor_and_product (CameraAbilitiesList *list, int index,
					  int *usb_vendor,
					  int *usb_product)
{
	CHECK_NULL (list && usb_vendor && usb_product);

	if (index > list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	*usb_vendor  = list->abilities[index]->usb_vendor;
	*usb_product = list->abilities[index]->usb_product;

	return (GP_OK);
}

int
gp_abilities_list_get_abilities (CameraAbilitiesList *list, int index,
				 CameraAbilities *abilities)
{
	CHECK_NULL (list && abilities);

	if (index > list->count)
		return (GP_ERROR_BAD_PARAMETERS);

	memcpy (abilities, list->abilities[index], sizeof (CameraAbilities));

	return (GP_OK);
}

