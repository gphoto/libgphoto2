/** \file
 *
 * \author Copyright 2001 Lutz Mueller <lutz@users.sf.net>
 *
 * \par License
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

#ifndef __GPHOTO2_PORT_INFO_LIST_H__
#define __GPHOTO2_PORT_INFO_LIST_H__

/**
 * \brief The gphoto port type.
 *
 * Enumeration specifying the port type.
 * The enum is providing bitmasks, but most code uses it as
 * just the one specific values.
 */
typedef enum { 
	GP_PORT_NONE        =      0,	/**< \brief No specific type associated. */
	GP_PORT_SERIAL      = 1 << 0,	/**< \brief Serial port. */
	GP_PORT_USB         = 1 << 2,	/**< \brief USB port. */
	GP_PORT_DISK        = 1 << 3,	/**< \brief Disk / local mountpoint port. */
	GP_PORT_PTPIP       = 1 << 4,	/**< \brief PTP/IP port. */
	GP_PORT_USB_DISK_DIRECT = 1 << 5, /**< \brief Direct IO to an usb mass storage device. */
	GP_PORT_USB_SCSI    = 1 << 6	/**< \brief USB Mass Storage raw SCSI port. */
} GPPortType;

/**
 * \brief Information about the current port.
 * 
 * Specific information about the current port. Usually taken from the
 * "--port=XXXX" setting from the frontend.
 *
 * This is not to be confused with the driver configurable port settings
 * in \ref GPPortSettings.
 */
typedef struct _GPPortInfo {
	GPPortType type;	/**< \brief The type of this port. */
	char name[64];		/**< \brief The name of this port (usb:) */
	char path[64];		/**< \brief The path of this port (whatever is after the :) */

	/* Private */
	char library_filename[1024];	/**< \brief Internal pathname of the port driver. Do not use outside of the port library. */
} GPPortInfo;

#include <gphoto2/gphoto2-port.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef _GPHOTO2_INTERNAL_CODE
#include <gphoto2/gphoto2-port-log.h>
extern const StringFlagItem gpi_gphoto_port_type_map[];
#endif

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

const char *gp_port_message_codeset (const char*);

/* DEPRECATED */
typedef GPPortInfo gp_port_info;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GPHOTO2_PORT_INFO_LIST_H__ */
