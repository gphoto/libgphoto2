/* gphoto2-port-debug.h
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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

#ifndef __GPHOTO2_PORT_DEBUG_H__
#define __GPHOTO2_PORT_DEBUG_H__

void gp_port_debug_set_level (int level);
void gp_port_debug_printf (int target_debug_level, int debug_level,
			   char *format, ...);

/* Custom debugging function */
typedef void (* GPPortDebugFunc) (const char *msg, void *data);
void gp_port_debug_set_func (GPPortDebugFunc func, void *data);

/* History */
void        gp_port_debug_history_append (const char *msg);
int         gp_port_debug_history_set_size (unsigned int size);
const char *gp_port_debug_history_get (void);

#endif /* __GPHOTO2_PORT_DEBUG_H__ */
