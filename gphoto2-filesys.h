/* gphoto2-filesys.h
 *
 * Copyright (C) 2000 Scott Fritzinger
 *
 * Contributions:
 * 	Lutz Müller <urc8@rz.uni-karlsruhe.de> (2001)
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
#include <gphoto2-file.h>

typedef enum {
	GP_FILE_INFO_NONE            = 0,
	GP_FILE_INFO_TYPE            = 1 << 0,
	GP_FILE_INFO_NAME            = 1 << 1,
	GP_FILE_INFO_SIZE            = 1 << 2,
	GP_FILE_INFO_WIDTH           = 1 << 3,
	GP_FILE_INFO_HEIGHT          = 1 << 4,
	GP_FILE_INFO_PERMISSIONS     = 1 << 5,
	GP_FILE_INFO_STATUS	     = 1 << 6,
	GP_FILE_INFO_ALL             = 0xFF
} CameraFileInfoFields;

typedef enum {
	GP_FILE_PERM_NONE       = 0,
	GP_FILE_PERM_READ       = 1 << 0,
	GP_FILE_PERM_DELETE     = 1 << 1,
	GP_FILE_PERM_ALL        = 0xFF
} CameraFilePermissions;

typedef enum {
	GP_FILE_STATUS_UNKNOWN,
	GP_FILE_STATUS_NOT_DOWNLOADED,
	GP_FILE_STATUS_DOWNLOADED
} CameraFileStatus;

typedef struct {
	CameraFileInfoFields fields;
	char type[64];
	CameraFilePermissions permissions;
	char name[64];
	unsigned int size;
	unsigned int width, height;
	CameraFileStatus status;
} CameraFileInfoFile;

typedef struct {
	CameraFileInfoFields fields;
	char type[64];
	unsigned int size;
	unsigned int width, height;
	CameraFileStatus status;
} CameraFileInfoPreview;

typedef struct {
	CameraFileInfoFields fields;
	char type[64];
	unsigned int size;
	CameraFileStatus status;
} CameraFileInfoAudio;

typedef struct {
	CameraFileInfoPreview preview;
	CameraFileInfoFile    file;
	CameraFileInfoAudio   audio;
} CameraFileInfo;

/* You don't really want to know what's inside, do you? */
typedef struct _CameraFilesystem CameraFilesystem;

int gp_filesystem_new	 (CameraFilesystem **fs);
int gp_filesystem_free	 (CameraFilesystem *fs);

/* Manual editing */
int gp_filesystem_append        (CameraFilesystem *fs, const char *folder,
			         const char *filename);
int gp_filesystem_set_info_noop (CameraFilesystem *fs, const char *folder,
				 CameraFileInfo *info);
int gp_filesystem_set_file_noop (CameraFilesystem *fs, const char *folder,
				 CameraFile *file);
int gp_filesystem_reset         (CameraFilesystem *fs);

/* Information retrieval */
int gp_filesystem_count	       (CameraFilesystem *fs, const char *folder);
int gp_filesystem_name         (CameraFilesystem *fs, const char *folder,
			        int filenumber, const char **filename);
int gp_filesystem_get_folder   (CameraFilesystem *fs, const char *filename,
			        const char **folder);
int gp_filesystem_number       (CameraFilesystem *fs, const char *folder,
				const char *filename);

/* Listings */
typedef int (*CameraFilesystemListFunc) (CameraFilesystem *fs,
					 const char *folder, CameraList *list,
					 void *data);
int gp_filesystem_set_list_funcs (CameraFilesystem *fs,
				  CameraFilesystemListFunc file_list_func,
				  CameraFilesystemListFunc folder_list_func,
				  void *data);
int gp_filesystem_list_files     (CameraFilesystem *fs, const char *folder,
				  CameraList *list);
int gp_filesystem_list_folders   (CameraFilesystem *fs, const char *folder,
				  CameraList *list);

/* File information */
typedef int (*CameraFilesystemInfoFunc) (CameraFilesystem *fs,
					 const char *folder,
					 const char *filename,
					 CameraFileInfo *info, void *data);
int gp_filesystem_set_info_funcs (CameraFilesystem *fs,
				  CameraFilesystemInfoFunc get_info_func,
				  CameraFilesystemInfoFunc set_info_func,
				  void *data);
int gp_filesystem_get_info       (CameraFilesystem *fs, const char *folder,
				  const char *filename, CameraFileInfo *info);
int gp_filesystem_set_info       (CameraFilesystem *fs, const char *folder,
				  const char *filename, CameraFileInfo *info);

/* Files */
typedef int (*CameraFilesystemGetFileFunc)    (CameraFilesystem *fs,
					       const char *folder,
					       const char *filename,
					       CameraFileType type,
					       CameraFile *file, void *data);
typedef int (*CameraFilesystemDeleteFileFunc) (CameraFilesystem *fs,
					       const char *folder,
					       const char *filename,
					       void *data);
int gp_filesystem_set_file_funcs (CameraFilesystem *fs,
				  CameraFilesystemGetFileFunc get_file_func,
				  CameraFilesystemDeleteFileFunc del_file_func,
				  void *data);
int gp_filesystem_get_file       (CameraFilesystem *fs, const char *folder,
				  const char *filename, CameraFileType type,
				  CameraFile *file);
int gp_filesystem_delete_file    (CameraFilesystem *fs, const char *folder,
				  const char *filename);

/* Folders */
typedef int (*CameraFilesystemPutFileFunc)   (CameraFilesystem *fs,
					      const char *folder,
					      CameraFile *file, void *data);
typedef int (*CameraFilesystemDeleteAllFunc) (CameraFilesystem *fs,
					      const char *folder, void *data);
typedef int (*CameraFilesystemDirFunc)       (CameraFilesystem *fs,
					      const char *folder,
					      const char *name, void *data);
int gp_filesystem_set_folder_funcs (CameraFilesystem *fs,
				  CameraFilesystemPutFileFunc put_file_func,
				  CameraFilesystemDeleteAllFunc delete_all_func,
				  CameraFilesystemDirFunc make_dir_func,
				  CameraFilesystemDirFunc remove_dir_func,
				  void *data);
int gp_filesystem_put_file   (CameraFilesystem *fs, const char *folder,
			      CameraFile *file);
int gp_filesystem_delete_all (CameraFilesystem *fs, const char *folder);
int gp_filesystem_make_dir   (CameraFilesystem *fs, const char *folder,
			      const char *name);
int gp_filesystem_remove_dir (CameraFilesystem *fs, const char *folder,
			      const char *name);

/* For debugging */
int gp_filesystem_dump         (CameraFilesystem *fs);

#endif /* __GPHOTO2_FILESYS_H__ */
