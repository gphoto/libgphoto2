/* ricoh.h
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

#ifndef __RICOH_H__
#define __RICOH_H__

#include <time.h>

#include <gphoto2-camera.h>
#include <gphoto2-context.h>

int ricoh_get_num  (Camera *camera, GPContext *context, unsigned int *n);
int ricoh_get_size (Camera *camera, GPContext *context, unsigned int  n,
		    unsigned long *size);
int ricoh_get_date (Camera *camera, GPContext *context, unsigned int  n,
		    time_t *date);
int ricoh_del_pic  (Camera *camera, GPContext *context, unsigned int  n);


#endif /* __RICOH_H__ */
