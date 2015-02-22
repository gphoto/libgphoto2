/* pmp.h
 *
 * Copyright (C) M. Adam Kendall <joker@penguinpub.com>
 * Copyright (C) 2002 Bart van Leeuwen <bart@netage.nl>
 *
 * Based on the chotplay CLI interface from Ken-ichi Hayashi 1996,1997
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

#define PMP_RESOLUTION 0x1d
#define PMP_FIN 0x33
#define PMP_STD 0x17
#define PMP_ECM 0x08

#define PMP_COMMENT 0x34

#define PMP_TAKE_YEAR 0x4c
#define PMP_TAKE_MONTH 0x4d
#define PMP_TAKE_DATE 0x4e
#define PMP_TAKE_HOUR 0x4f
#define PMP_TAKE_MINUTE 0x50
#define PMP_TAKE_SECOND 0x51

#define PMP_EDIT_YEAR 0x54
#define PMP_EDIT_MONTH 0x55
#define PMP_EDIT_DATE 0x56
#define PMP_EDIT_HOUR 0x57
#define PMP_EDIT_MINUTE 0x58
#define PMP_EDIT_SECOND 0x59

#define PMP_SPEED 0x66

#define PMP_FLASH 0x76
