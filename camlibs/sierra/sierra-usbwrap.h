/* sierra_usbwrap.h
 *
 * Copyright 2002 Lutz Mueller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __SIERRA_USBWRAP_H__
#define __SIERRA_USBWRAP_H__

#include <gphoto2/gphoto2-port.h>

int usb_wrap_write_packet (GPPort *dev, unsigned int type, char *sierra_msg,      int sierra_len);
int usb_wrap_read_packet  (GPPort *dev, unsigned int type, char *sierra_response, int sierra_len);

#endif /* __SIERRA_USBWRAP_H__ */
