/* gphoto2-core.h
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

#ifndef __GPHOTO2_CORE_H__
#define __GPHOTO2_CORE_H__

#include <gphoto2-list.h>
#include <gphoto2-abilities.h>

int gp_init           (int debug);
int gp_is_initialized (void);
int gp_exit           (void);

void gp_debug_printf (int level, char *id, char *format, ...);

int  gp_autodetect (CameraList *list);

/* Retrieve the number of available cameras */
int gp_camera_count (void);

/* Retrieve the name of a particular camera */
int gp_camera_name  (int camera_number, const char **camera_name);

/* Retreive abilities for a given camera */
int gp_camera_abilities         (int camera_number, CameraAbilities *abilities);
int gp_camera_abilities_by_name (char *camera_name, CameraAbilities *abilities);

#endif /* __GPHOTO2_CORE_H__ */
