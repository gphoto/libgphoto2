/* gphoto2-port-core.h
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

#ifndef __GPHOTO2_PORT_CORE_H__
#define __GPHOTO2_PORT_CORE_H__

#include <gphoto2-port.h>

typedef struct {
        GPPortType type;
        char name[64];
        char path[64];

	/* Don't use this */
	unsigned int speed;

        char library_filename[1024];
} GPPortInfo;

int gp_port_core_count       (void);
int gp_port_core_get_info    (unsigned int x, GPPortInfo *info);
int gp_port_core_get_library (GPPortType type, const char **library);

/* Everything below is DEPRECATED! */
typedef GPPortInfo gp_port_info;

#endif /* __GPHOTO2_PORT_CORE_H__ */
