/* foreach.h
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

#ifndef __FOREACH_H__
#define __FOREACH_H__

#include <gphoto2-list.h>

#include "actions.h"

int for_each_subfolder      (const char *folder, folder_action faction,
			     image_action action, int reverse);
int for_each_image          (const char *folder, image_action iaction,
			     int reverse);
int for_each_image_in_range (const char *folder, unsigned char recurse,
			     char *range, image_action action, int reverse);

int get_path_for_id (const char *base_folder, unsigned char recurse,
		     unsigned int id, const char **folder,
		     const char **filename, CameraList *list);

#endif /* __FOREACH_H__ */
