/* gphoto2-port-info-list.h:
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
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

typedef enum { 
	GP_PORT_NONE        =      0,
	GP_PORT_SERIAL      = 1 << 0,
	GP_PORT_USB         = 1 << 2
} GPPortType;

typedef struct _GPPortInfo GPPortInfo;
struct _GPPortInfo {
	GPPortType type;
	char name[64];
	char path[64];

	/* Private */
	char library_filename[1024];
};

#include <gphoto2-port.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GPHOTO2_PORT_INFO_LIST_H__ */
