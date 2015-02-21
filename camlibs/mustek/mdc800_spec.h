/*
 * Copyright 1999/2000 by Henning Zabel <henning@uni-paderborn.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Definition of Commands and usefull structs.
 */
 
#ifndef DEFINE_MDC800_SPEC_H
#define DEFINE_MDC800_SPEC_H


#define ANSWER_COMMIT 			 0xbb
/* Commands */

#define COMMAND_BEGIN			 0x55
#define COMMAND_END			 0xAA

#define COMMAND_INIT_CONNECT 		 0x00
#define COMMAND_GET_SYSTEM_STATUS	 0x01
#define COMMAND_TAKE_PICTURE		 0x02
#define COMMAND_SET_TARGET		 0x03
#define COMMAND_DELETE_IMAGE		 0x04

#define COMMAND_GET_IMAGE		 0x05
#define COMMAND_GET_IMAGE_SIZE		 0x07
#define COMMAND_GET_THUMBNAIL		 0x09

#define COMMAND_CHANGE_RS232_BAUD_RATE 	 0x0b
#define COMMAND_GET_NUM_IMAGES		 0x0d

#define COMMAND_SET_PLAYBACK_MODE	 0x12
#define COMMAND_SET_CAMERA_MODE		 0x16
#define COMMAND_PLAYBACK_IMAGE		 0x17

#define COMMAND_SET_FLASHMODE_AUTO	 0x18
#define COMMAND_SET_FLASHMODE_ON    	 0x19
#define COMMAND_SET_FLASHMODE_OFF    	 0x1A

#define COMMAND_GET_WB_AND_EXPOSURE	 0x20
#define COMMAND_SET_EXPOSURE		 0x21
#define COMMAND_SET_WB			 0x22

#define COMMAND_GET_REMAIN_FREE_IMAGE_COUNT	 0x25
#define COMMAND_SET_LCD_ON		 0x2a
#define COMMAND_SET_LCD_OFF		 0x2b
#define COMMAND_SET_IMAGE_QUALITY	 0x2d
#define COMMAND_SET_MENU_ON		 0x2f
#define COMMAND_SET_MENU_OFF		 0x30

#define COMMAND_SET_STORAGE_SOURCE	 0x32
#define COMMAND_DISCONNECT		 0x34

#define COMMAND_GET_IMAGE_QUALITY	 0x49
#define COMMAND_SET_EXPOSURE_MODE	 0x50
#define COMMAND_GET_EXPOSURE_MODE	 0x51

#endif
