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

int gp_core_autodetect (CameraList *list);

/* Don't use - those are DEPRECATED! */
int gp_exit (void);
int gp_autodetect (CameraList *);
int gp_camera_count (void);
int gp_camera_name  (int, const char **);
int gp_camera_abilities (int, CameraAbilities *);
int gp_camera_abilities_by_name (const char *, CameraAbilities *);

#endif /* __GPHOTO2_CORE_H__ */
