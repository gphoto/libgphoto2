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

#include <gphoto2-list.h>
#include <gphoto2-port-info-list.h>

/**
 * CameraDriverStatus:
 *
 * This flag indicates the stability of the driver.
 * #GP_DRIVER_STATUS_EXPERIMENTAL is set initially. Once the driver is ready
 * for testing, the flag #GP_DRIVER_STATUS_TESTING is set. Finally, if testing
 * has shown that the driver is usable and no bugs can be found any longer, 
 * the flag is set to #GP_DRIVER_STATUS_PRODUCTION.
 **/
typedef enum {
        GP_DRIVER_STATUS_PRODUCTION,
        GP_DRIVER_STATUS_TESTING,
        GP_DRIVER_STATUS_EXPERIMENTAL
} CameraDriverStatus;

/**
 * CameraOperation:
 *
 * This flag indicates what operations a camera supports. It is useful in case
 * a frontend want to determine what operations a camera supports in order to
 * adjust the GUI for this model.
 **/
typedef enum {
        GP_OPERATION_NONE               = 0,
        GP_OPERATION_CAPTURE_IMAGE      = 1 << 0,
        GP_OPERATION_CAPTURE_VIDEO      = 1 << 1,
        GP_OPERATION_CAPTURE_AUDIO      = 1 << 2,
        GP_OPERATION_CAPTURE_PREVIEW    = 1 << 3,
        GP_OPERATION_CONFIG             = 1 << 4
} CameraOperation;

/**
 * CameraFileOperations:
 *
 * Similarly to #CameraOperations, this flag indicates what file operations
 * a specific camera model supports.
 **/
typedef enum {
        GP_FILE_OPERATION_NONE          = 0,
        GP_FILE_OPERATION_DELETE        = 1 << 1,
        GP_FILE_OPERATION_CONFIG        = 1 << 2,
        GP_FILE_OPERATION_PREVIEW       = 1 << 3,
        GP_FILE_OPERATION_RAW           = 1 << 4
} CameraFileOperation;

/**
 * CameraFolderOperations:
 *
 * Similarly to #CameraFolderOperations, this flag indicates what folder
 * operations a specific camera model supports.
 **/
typedef enum {
        GP_FOLDER_OPERATION_NONE        = 0, 
        GP_FOLDER_OPERATION_DELETE_ALL  = 1 << 0,
        GP_FOLDER_OPERATION_PUT_FILE    = 1 << 1,
        GP_FOLDER_OPERATION_CONFIG      = 1 << 2
} CameraFolderOperation;

/**
 * CameraAbilities:
 *
 * This structure is used to inform frontends about what abilities a 
 * specific camera model has. In addition, it is used by gphoto2 to 
 * determine what library to load on #gp_camera_set_abilities.
 * For retreiving abilities of a specific model, create a
 * #CameraAbilitiesList and use #gp_abilities_list_load in order to scan
 * the system for drivers. Then, retreive the #CameraAbilities from this list.
 **/
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

        /* For core use */
        char library [1024];
        char id [1024];
} CameraAbilities;

typedef struct _CameraAbilitiesList CameraAbilitiesList;

int gp_abilities_list_new    (CameraAbilitiesList **list);
int gp_abilities_list_free   (CameraAbilitiesList *list);

int gp_abilities_list_load   (CameraAbilitiesList *list);
int gp_abilities_list_detect (CameraAbilitiesList *list,
			      GPPortInfoList *info_list, CameraList *l);

int gp_abilities_list_append (CameraAbilitiesList *list,
			      CameraAbilities abilities);

int gp_abilities_list_count  (CameraAbilitiesList *list);

int gp_abilities_list_lookup_model (CameraAbilitiesList *list,
				    const char *model);

int gp_abilities_list_get_abilities (CameraAbilitiesList *list, int index,
				     CameraAbilities *abilities);

#endif /* __GPHOTO2_ABILITIES_LIST_H__ */
