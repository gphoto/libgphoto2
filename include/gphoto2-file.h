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

typedef struct {
	char type [64];
	char name [64];
	long int size;
	char *data;
	int bytes_read;
	int session;
	int ref_count;
} CameraFile;

CameraFile *gp_file_new (void);

int gp_file_ref            (CameraFile *file);
int gp_file_unref          (CameraFile *file);

int gp_file_session        (CameraFile *file);

int gp_file_open           (CameraFile *file, char *filename);
int gp_file_save           (CameraFile *file, char *filename);

int gp_file_clean          (CameraFile *file);
int gp_file_free           (CameraFile *file);

int gp_file_append         (CameraFile *file, char *data,  int size);
int gp_file_get_last_chunk (CameraFile *file, char **data, int *size);

#endif /* __GPHOTO2_FILE_H__ */
