/* gphoto2-file.h
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

#ifndef __GPHOTO2_FILE_H__
#define __GPHOTO2_FILE_H__

typedef enum {
	GP_FILE_TYPE_PREVIEW,
	GP_FILE_TYPE_NORMAL,
	GP_FILE_TYPE_RAW
} CameraFileType;

typedef struct _CameraFile CameraFile;

/* Don't use this - it'll disappear soon... */
struct _CameraFile {
	CameraFileType type;
	char mime_type [64];
	char name [64];
	long int size;
	char *data;
	int bytes_read;
	int session;
	int ref_count;
}; 

int gp_file_new            (CameraFile **file);
int gp_file_ref            (CameraFile *file);
int gp_file_unref          (CameraFile *file);

int gp_file_session        (CameraFile *file);

int gp_file_open           (CameraFile *file, char *filename);
int gp_file_save           (CameraFile *file, char *filename);

int gp_file_clean          (CameraFile *file);
int gp_file_free           (CameraFile *file);

int gp_file_set_name       (CameraFile *file, const char *name);
int gp_file_set_mime_type  (CameraFile *file, const char *mime_type);
int gp_file_set_type       (CameraFile *file, CameraFileType type);

int gp_file_get_name       (CameraFile *file, const char **name);
int gp_file_get_mime_type  (CameraFile *file, const char **mime_type);
int gp_file_get_type       (CameraFile *file, CameraFileType *type);

int gp_file_append            (CameraFile*, const char *data,  int size);
int gp_file_get_last_chunk    (CameraFile*, char **data, long int *size);
int gp_file_set_data_and_size (CameraFile*, char *data, long int size);
int gp_file_get_data_and_size (CameraFile*, const char **data, long int *size);

int gp_file_copy           (CameraFile *destination, CameraFile *source);

#endif /* __GPHOTO2_FILE_H__ */
