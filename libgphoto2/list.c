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

CameraList *gp_list_new ()
{
        CameraList *list;

        list = (CameraList*)malloc (sizeof (CameraList));
        if (!list)
                return NULL;

        memset (list, 0, sizeof (CameraList));
        return (list);
}

int gp_list_free (CameraList *list) 
{
        if (list)
                free (list);
        return (GP_OK);
}

int gp_list_append (CameraList *list, char *name)
{
        strcpy (list->entry[list->count].name, name);

        list->count += 1;

        return (GP_OK);
}

int gp_list_count (CameraList *list)
{
        return (list->count);
}

CameraListEntry *gp_list_entry (CameraList *list, int entrynum)
{
        return (&list->entry [entrynum]);
}
