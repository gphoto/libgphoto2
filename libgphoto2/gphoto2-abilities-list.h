/* gphoto2-abilities-list.h
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

#ifndef __GPHOTO2_ABILITIES_LIST_H__
#define __GPHOTO2_ABILITIES_LIST_H__

#include <gphoto2-context.h>
#include <gphoto2-list.h>
#include <gphoto2-port-info-list.h>

typedef enum {
        GP_DRIVER_STATUS_PRODUCTION,
        GP_DRIVER_STATUS_TESTING,
        GP_DRIVER_STATUS_EXPERIMENTAL
} CameraDriverStatus;

typedef enum {
        GP_OPERATION_NONE               = 0,
        GP_OPERATION_CAPTURE_IMAGE      = 1 << 0,
        GP_OPERATION_CAPTURE_VIDEO      = 1 << 1,
        GP_OPERATION_CAPTURE_AUDIO      = 1 << 2,
        GP_OPERATION_CAPTURE_PREVIEW    = 1 << 3,
        GP_OPERATION_CONFIG             = 1 << 4
} CameraOperation;

typedef enum {
        GP_FILE_OPERATION_NONE          = 0,
        GP_FILE_OPERATION_DELETE        = 1 << 1,
        GP_FILE_OPERATION_PREVIEW       = 1 << 3,
        GP_FILE_OPERATION_RAW           = 1 << 4,
	GP_FILE_OPERATION_AUDIO		= 1 << 5,
	GP_FILE_OPERATION_EXIF		= 1 << 6
} CameraFileOperation;

typedef enum {
        GP_FOLDER_OPERATION_NONE        = 0, 
        GP_FOLDER_OPERATION_DELETE_ALL  = 1 << 0,
        GP_FOLDER_OPERATION_PUT_FILE    = 1 << 1,
	GP_FOLDER_OPERATION_MAKE_DIR    = 1 << 2,
	GP_FOLDER_OPERATION_REMOVE_DIR  = 1 << 3
} CameraFolderOperation;

typedef struct {
        char model [128];
        CameraDriverStatus status;

        /* Supported ports and speeds (latter terminated with a value of 0) */
        GPPortType port;
        int speed [64];

        /* Supported operations */
        CameraOperation       operations;
        CameraFileOperation   file_operations;
        CameraFolderOperation folder_operations;

        int usb_vendor, usb_product;
	int usb_class, usb_subclass, usb_protocol;

        /* For core use */
        char library [1024];
        char id [1024];
} CameraAbilities;

typedef struct _CameraAbilitiesList CameraAbilitiesList;

int gp_abilities_list_new    (CameraAbilitiesList **list);
int gp_abilities_list_free   (CameraAbilitiesList *list);

int gp_abilities_list_load     (CameraAbilitiesList *list, GPContext *context);

int gp_abilities_list_detect (CameraAbilitiesList *list,
			      GPPortInfoList *info_list, CameraList *l,
			      GPContext *context);

int gp_abilities_list_append (CameraAbilitiesList *list,
			      CameraAbilities abilities);

int gp_abilities_list_count  (CameraAbilitiesList *list);

int gp_abilities_list_lookup_model (CameraAbilitiesList *list,
				    const char *model);

int gp_abilities_list_get_abilities (CameraAbilitiesList *list, int index,
				     CameraAbilities *abilities);

#endif /* __GPHOTO2_ABILITIES_LIST_H__ */
