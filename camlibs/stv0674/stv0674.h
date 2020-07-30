/*
 * STV0674 Camera Chipset Driver
 * Copyright 2000 Vince Sanders <vince@kyllikki.org>
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

#ifndef STV0674_H
#define STV0674_H

#define CMDID_SET_IMAGE 0x3
#define CMDID_READ_IMAGE 0x5
#define READ_IMAGE_VALUE_RESET 0xff
#define READ_IMAGE_VALUE_READ 0x08

#define CMDID_FINISH_READ 0x9

#define CMDID_PING 0x80

#define CMDID_ENUMERATE_IMAGES 0x83

#define CMDID_IHAVENOIDEA 0x86

#endif
