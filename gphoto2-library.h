/* gphoto2-library.h:
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

#ifndef __GPHOTO2_LIBRARY_H__
#define __GPHOTO2_LIBRARY_H__

#include <gphoto2-abilities-list.h>
#include <gphoto2-camera.h>

typedef int (* CameraLibraryIdFunc)        (CameraText *);
typedef int (* CameraLibraryAbilitiesFunc) (CameraAbilitiesList *);
typedef int (* CameraLibraryInitFunc)      (Camera *);

/*
 * If you want to write a camera library, you need to implement 
 * the following three functions. Everything else should be declared
 * as static.
 */
int camera_id		(CameraText *id);
int camera_abilities 	(CameraAbilitiesList *list);
int camera_init 	(Camera *camera);

#endif /* __GPHOTO2_LIBRARY_H__ */
