/* actions.h
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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

#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include <gphoto2-camera.h>
#include <gphoto2-context.h>

typedef struct _ActionParams ActionParams;
struct _ActionParams {
	Camera *camera;
	GPContext *context;
	const char *folder;
};

/* Image actions */
typedef int FileAction    (ActionParams *params, const char *filename);
int print_file_action     (ActionParams *params, const char *filename);
int print_exif_action     (ActionParams *params, const char *filename);
int print_info_action     (ActionParams *params, const char *filename);
int save_file_action      (ActionParams *params, const char *filename);
int save_thumbnail_action (ActionParams *params, const char *filename);
int save_raw_action       (ActionParams *params, const char *filename);
int save_audio_action     (ActionParams *params, const char *filename);
int save_exif_action      (ActionParams *params, const char *filename);
int delete_file_action    (ActionParams *params, const char *filename);

/* Folder actions */
typedef int FolderAction  (ActionParams *params);
int delete_all_action     (ActionParams *params);
int list_files_action     (ActionParams *params);
int list_folders_action   (ActionParams *params);

#endif /* __ACTIONS_H__ */
