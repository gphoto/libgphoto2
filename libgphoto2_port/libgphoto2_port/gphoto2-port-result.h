/* gphoto2-port-result.h
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
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

#ifndef __GPHOTO2_PORT_RESULT_H__
#define __GPHOTO2_PORT_RESULT_H__

/* Return values. gphoto2-port should only return values from 0 to -99 */
#define GP_OK                            0
#define GP_ERROR                        -1
#define GP_ERROR_BAD_PARAMETERS		-2
#define GP_ERROR_NO_MEMORY		-3
#define GP_ERROR_LIBRARY		-4
#define GP_ERROR_UNKNOWN_PORT		-5
#define GP_ERROR_NOT_SUPPORTED		-6
#define GP_ERROR_IO			-7

#define GP_ERROR_TIMEOUT                -10

#define GP_ERROR_IO_SUPPORTED_SERIAL    -20
#define GP_ERROR_IO_SUPPORTED_USB       -21

#define GP_ERROR_IO_INIT                -31
#define GP_ERROR_IO_READ                -34
#define GP_ERROR_IO_WRITE               -35
#define GP_ERROR_IO_UPDATE              -37

#define GP_ERROR_IO_SERIAL_SPEED        -41

#define GP_ERROR_IO_USB_CLEAR_HALT      -51
#define GP_ERROR_IO_USB_FIND            -52
#define GP_ERROR_IO_USB_CLAIM           -53

#define GP_ERROR_IO_LOCK                -60

const char *gp_port_result_as_string (int result);

#endif /* __GPHOTO2_PORT_RESULT_H__ */

