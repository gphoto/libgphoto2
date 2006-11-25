/*
 * Definition of Commands and usefull structs.
 */
 
#ifndef DEFINE_MDC800_SPEC_H
#define DEFINE_MDC800_SPEC_H


#define ANSWER_COMMIT 			(char) 0xbb
/* Commands */

#define COMMAND_BEGIN			(char) 0x55
#define COMMAND_END			(char) 0xAA

#define COMMAND_INIT_CONNECT 		(char) 0x00
#define COMMAND_GET_SYSTEM_STATUS	(char) 0x01
#define COMMAND_TAKE_PICTURE		(char) 0x02
#define COMMAND_SET_TARGET		(char) 0x03
#define COMMAND_DELETE_IMAGE		(char) 0x04

#define COMMAND_GET_IMAGE		(char) 0x05
#define COMMAND_GET_IMAGE_SIZE		(char) 0x07
#define COMMAND_GET_THUMBNAIL		(char) 0x09

#define COMMAND_CHANGE_RS232_BAUD_RATE 	(char) 0x0b
#define COMMAND_GET_NUM_IMAGES		(char) 0x0d

#define COMMAND_SET_PLAYBACK_MODE	(char) 0x12
#define COMMAND_SET_CAMERA_MODE		(char) 0x16
#define COMMAND_PLAYBACK_IMAGE		(char) 0x17

#define COMMAND_SET_FLASHMODE_AUTO	(char) 0x18
#define COMMAND_SET_FLASHMODE_ON    	(char) 0x19
#define COMMAND_SET_FLASHMODE_OFF    	(char) 0x1A

#define COMMAND_GET_WB_AND_EXPOSURE	(char) 0x20
#define COMMAND_SET_EXPOSURE		(char) 0x21
#define COMMAND_SET_WB			(char) 0x22

#define COMMAND_GET_REMAIN_FREE_IMAGE_COUNT	(char) 0x25
#define COMMAND_SET_LCD_ON		(char) 0x2a
#define COMMAND_SET_LCD_OFF		(char) 0x2b
#define COMMAND_SET_IMAGE_QUALITY	(char) 0x2d
#define COMMAND_SET_MENU_ON		(char) 0x2f
#define COMMAND_SET_MENU_OFF		(char) 0x30

#define COMMAND_SET_STORAGE_SOURCE	(char) 0x32
#define COMMAND_DISCONNECT		(char) 0x34

#define COMMAND_GET_IMAGE_QUALITY	(char) 0x49
#define COMMAND_SET_EXPOSURE_MODE	(char) 0x50
#define COMMAND_GET_EXPOSURE_MODE	(char) 0x51

#endif
