/* smal.h
 *
 * Copyright (C) 2003 Lee Benfield <lee@benf.org>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#ifndef __SMAL_H__

#define __SMAL_H__ 1

#define USB_VENDOR_ID_SMAL        0x0dca
#define USB_DEVICE_ID_ULTRAPOCKET 0x0002
#define USB_DEVICE_ID_FLATFOTO    0x0004

#define USB_VENDOR_ID_LOGITECH    0x046d
#define USB_DEVICE_ID_POCKETDIGITAL 0x0950

#define USB_VENDOR_ID_CREATIVE    0x041e
#define USB_DEVICE_ID_CARDCAM 0x4016

typedef enum ultrapocket_BADGE_TYPE {
    BADGE_UNKNOWN = 0,
    BADGE_GENERIC,    /* could be axia/ultrapocket */
    BADGE_ULTRAPOCKET,
    BADGE_AXIA,
    BADGE_FLATFOTO,
    BADGE_LOGITECH_PD, /* pocket digital */
    BADGE_CARDCAM
} up_badge_type;

struct _CameraPrivateLibrary {
    up_badge_type up_type;
};


#endif
