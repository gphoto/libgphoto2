/* gphoto2-debug.h
 *
 * Copyright (C) 2001 Lutz Müller
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

#ifndef __GPHOTO2_DEBUG_H__
#define __GPHOTO2_DEBUG_H__

/* The debug levels (GP_DEBUG_*) are defined here */
#include <gphoto2-port-debug.h>

void gp_debug_printf    (int level, const char *id, const char *format, ...);

void gp_debug_set_level (int level);
int  gp_debug_get_level (void);

/* Custom debugging function */
typedef void (* GPDebugFunc) (const char *id, const char *msg, void *data);
void gp_debug_set_func  (GPDebugFunc func, void *data);

/* History */
int         gp_debug_history_set_size (unsigned int size);
const char *gp_debug_history_get      (void);

#endif /* __GPHOTO2_DEBUG_H__ */
