/* gphoto2-list.h: Lists of files, folders, cameras, etc.
 *
 * Copyright (C) 2001 Scott Fritzinger
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

#ifndef __GPHOTO2_LIST_H__
#define __GPHOTO2_LIST_H__

typedef struct {
	char name [128];
	char value [128];
} CameraListEntry;

typedef struct {
	int  count;
	CameraListEntry entry [1024];
} CameraList;

CameraList*		gp_list_new	(void);
int			gp_list_free	(CameraList *list);
int			gp_list_count	(CameraList *list);
int			gp_list_append	(CameraList *list, char *name);
CameraListEntry*	gp_list_entry	(CameraList *list, int entry_number);

#endif /* __GPHOTO2_LIST_H__ */
