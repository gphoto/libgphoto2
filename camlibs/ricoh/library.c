/* library.c
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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

#include <string.h>

#include <gphoto2-library.h>

#include "ricoh.h"

#define CR(result) {int r=(result); if (r<0) return r;}

static struct {
	const char *model;
} models[] = {
	{"Ricoh RDC-300"},
	{"Ricoh RDC-300Z"},
	{NULL}
};

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	memset (&a, 0, sizeof (CameraAbilities));
	for (i = 0; models[i].model; i++) {
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.operations = GP_OPERATION_NONE;
		a.file_operations = GP_FILE_OPERATION_NONE;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		CR (gp_abilities_list_append (list, a));
	}

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned int n;

	CR (ricoh_get_num (camera, context, &n));
	CR (gp_list_populate (list, "RDC%04i.jpg", n));

	return (GP_OK);
}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "Ricoh");

	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context)
{
	CR (gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL,
					  camera));

	return (GP_OK);
}
