/* gphoto2-port-result.h
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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
#define GP_ERROR_TIMEOUT                -2
#define GP_ERROR_IO_SUPPORTED_SERIAL    -3
#define GP_ERROR_IO_SUPPORTED_USB       -4
#define GP_ERROR_IO_SUPPORTED_PARALLEL  -5
#define GP_ERROR_IO_SUPPORTED_NETWORK   -6
#define GP_ERROR_IO_SUPPORTED_IEEE1394  -7
#define GP_ERROR_IO_UNKNOWN_PORT        -8

#define GP_ERROR_IO_MEMORY              -9
#define GP_ERROR_IO_LIBRARY             -10

#define GP_ERROR_IO_INIT                -11
#define GP_ERROR_IO_OPEN                -12
#define GP_ERROR_IO_TIMEOUT             -13
#define GP_ERROR_IO_READ                -14
#define GP_ERROR_IO_WRITE               -15
#define GP_ERROR_IO_CLOSE               -16
#define GP_ERROR_IO_UPDATE              -17
#define GP_ERROR_IO_PIN                 -18

#define GP_ERROR_IO_SERIAL_SPEED        -19
#define GP_ERROR_IO_SERIAL_BREAK        -20
#define GP_ERROR_IO_SERIAL_FLUSH        -21

#define GP_ERROR_IO_USB_CLEAR_HALT      -22
#define GP_ERROR_IO_USB_FIND            -23
#define GP_ERROR_IO_USB_CLAIM           -24

#define GP_ERROR_IO_LOCK                -25

const char *gp_port_result_as_string (int result);

#endif /* __GPHOTO2_PORT_RESULT_H__ */

