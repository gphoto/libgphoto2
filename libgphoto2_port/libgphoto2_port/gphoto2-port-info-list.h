/* gphoto2-port-info-list.h:
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

#ifndef __GPHOTO2_PORT_INFO_LIST_H__
#define __GPHOTO2_PORT_INFO_LIST_H__

#include <gphoto2-port.h>

typedef struct {
	GPPortType type;
	char name[64];
	char path[64];

	/* Private */
	char library_filename[1024];
} GPPortInfo;

/* Internals are private */
typedef struct _GPPortInfoList GPPortInfoList;

int gp_port_info_list_new  (GPPortInfoList **list);
int gp_port_info_list_free (GPPortInfoList *list);

int gp_port_info_list_append (GPPortInfoList *list, GPPortInfo info);

int gp_port_info_list_load (GPPortInfoList *list);

int gp_port_info_list_count (GPPortInfoList *list);

int gp_port_info_list_lookup_path (GPPortInfoList *list, const char *path);
int gp_port_info_list_lookup_name (GPPortInfoList *list, const char *name);

int gp_port_info_list_get_info (GPPortInfoList *list, int n, GPPortInfo *info);

/* DEPRECATED */
typedef GPPortInfo gp_port_info;

#endif /* __GPHOTO2_PORT_INFO_LIST_H__ */
