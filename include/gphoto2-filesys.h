/* gphoto2-filesys.h: Filesystem emulation for cameras that don't support 
 *                    filenames. In addition, it can be used to cache the
 *                    contents of the camera in order to avoid traffic.
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

#ifndef __GPHOTO2_FILESYS_H__
#define __GPHOTO2_FILESYS_H__

#include <gphoto2-list.h>

/* You don't really want to know what's inside, do you? */
typedef struct _CameraFilesystem CameraFilesystem;

CameraFilesystem* gp_filesystem_new	 ();
int		  gp_filesystem_free	 (CameraFilesystem *fs);
int 		  gp_filesystem_populate (CameraFilesystem *fs, 
					  const char *folder, char *format, 
					  int count);
int 		  gp_filesystem_append   (CameraFilesystem *fs, 
					  const char *folder, 
					  const char *filename);
int 		  gp_filesystem_format   (CameraFilesystem *fs);
int 		  gp_filesystem_delete   (CameraFilesystem *fs, 
					  const char *folder, 
					  const char *filename);
int 		  gp_filesystem_count	 (CameraFilesystem *fs, 
					  const char *folder);
char*		  gp_filesystem_name     (CameraFilesystem *fs, 
					  const char *folder, int filenumber);
int		  gp_filesystem_number   (CameraFilesystem *fs, 
					  const char *folder, 
					  const char *filename);

int               gp_filesystem_get_folder (CameraFilesystem *fs,
					    const char *filename,
					    const char **folder);

int gp_filesystem_list_files   (CameraFilesystem *fs, const char *folder,
				CameraList *list);
int gp_filesystem_list_folders (CameraFilesystem *fs, const char *folder,
				CameraList *list);
int gp_filesystem_dump         (CameraFilesystem *fs);

#endif /* __GPHOTO2_FILESYS_H__ */
