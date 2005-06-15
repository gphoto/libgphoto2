/** \file gphoto2-abilities-list.h
 * \brief List of supported camera models including their abilities.
 *
 * \author Copyright © 2000 Scott Fritzinger
 *
 * \par
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \par
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * \par
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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
        GP_DRIVER_STATUS_PRODUCTION,
        GP_DRIVER_STATUS_TESTING,
	GP_DRIVER_STATUS_EXPERIMENTAL,
	GP_DRIVER_STATUS_DEPRECATED
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
        GP_FILE_OPERATION_AUDIO         = 1 << 5,
        GP_FILE_OPERATION_EXIF          = 1 << 6
} CameraFileOperation;

typedef enum {
        GP_FOLDER_OPERATION_NONE        = 0, 
        GP_FOLDER_OPERATION_DELETE_ALL  = 1 << 0,
        GP_FOLDER_OPERATION_PUT_FILE    = 1 << 1,
        GP_FOLDER_OPERATION_MAKE_DIR    = 1 << 2,
        GP_FOLDER_OPERATION_REMOVE_DIR  = 1 << 3
} CameraFolderOperation;


/** 
 * \brief Describes the properties of a specific camera.
 *
 * The internals of this structures are used extensively by the
 * camlibs, but the status regarding use by frontends is questionable.
 */
typedef struct {
        char model [128];			/**< name of camera model */
        CameraDriverStatus status;		/**< driver quality */

	/** Supported port. */
	GPPortType port;
	/** Supported port speeds (terminated with a value of 0). */
        int speed [64];

        /* Supported operations */
        CameraOperation       operations;	/**< camera operation funcs */
        CameraFileOperation   file_operations;  /**< camera file op funcs */
        CameraFolderOperation folder_operations;/**< camera folder op funcs */

	int usb_vendor;		/**< USB Vendor D */
	int usb_product;	/**< USB Product ID */
	int usb_class;          /**< USB device class */
	int usb_subclass;	/**< USB device subclass */
	int usb_protocol;	/**< USB device protocol */

        /* For core use */
        char library [1024];	/**< \internal */
        char id [1024];		/**< \internal */

	/* Reserved space to use in the future w/out changing the 
	 * struct size */
	int reserved1;		/**< reserved space \internal */
        int reserved2;		/**< reserved space \internal */
        int reserved3;		/**< reserved space \internal */
        int reserved4;		/**< reserved space \internal */
        int reserved5;		/**< reserved space \internal */
        int reserved6;		/**< reserved space \internal */
        int reserved7;		/**< reserved space \internal */
        int reserved8;		/**< reserved space \internal */
} CameraAbilities;


/**
 * \brief List of supported camera models including their abilities
 *
 * The internals of this list are hidden - use the access functions.
 */
typedef struct _CameraAbilitiesList CameraAbilitiesList;


int gp_abilities_list_new    (CameraAbilitiesList **list);
int gp_abilities_list_free   (CameraAbilitiesList *list);

int gp_abilities_list_load   (CameraAbilitiesList *list, GPContext *context);
int gp_abilities_list_reset  (CameraAbilitiesList *list);

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GPHOTO2_ABILITIES_LIST_H__ */
