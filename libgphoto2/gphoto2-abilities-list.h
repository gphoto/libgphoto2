/* gphoto2-abilities-list.h
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

#ifndef __GPHOTO2_ABILITIES_LIST_H__
#define __GPHOTO2_ABILITIES_LIST_H__

#include <gphoto2-abilities.h>
#include <gphoto2-list.h>

typedef struct _CameraAbilitiesList CameraAbilitiesList;

int gp_abilities_list_new    (CameraAbilitiesList **list);
int gp_abilities_list_free   (CameraAbilitiesList *list);

int gp_abilities_list_load   (CameraAbilitiesList *list);
int gp_abilities_list_detect (CameraAbilitiesList *list, CameraList *l);

int gp_abilities_list_dump_libs (CameraAbilitiesList *list);

int gp_abilities_list_append (CameraAbilitiesList *list,
			      CameraAbilities abilities);

int gp_abilities_list_count  (CameraAbilitiesList *list);

int gp_abilities_list_lookup_model (CameraAbilitiesList *list,
				    const char *model);
int gp_abilities_list_get_model    (CameraAbilitiesList *list, int index,
				    const char **model);

int gp_abilities_list_get_abilities (CameraAbilitiesList *list, int index,
				     CameraAbilities *abilities);

#endif /* __GPHOTO2_ABILITIES_LIST_H__ */
