/****************************************************************/
/* konica.c - 	Library giving access to all features of the 	*/
/*		camera.						*/
/*								*/
/* Copyright (C) 2000 The Free Software Foundation		*/
/*								*/
/* Author: Lutz Müller <Lutz.Mueller@stud.uni-karlsruhe.de>	*/
/*								*/
/* This program is free software; you can redistribute it 	*/
/* and/or modify it under the terms of the GNU General Public	*/ 
/* License as published by the Free Software Foundation; either */
/* version 2 of the License, or (at your option) any later 	*/
/* version.							*/
/*								*/
/* This program is distributed in the hope that it will be 	*/
/* useful, but WITHOUT ANY WARRANTY; without even the implied 	*/
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 	*/
/* PURPOSE.  See the GNU General Public License for more 	*/
/* details.							*/
/*								*/
/* You should have received a copy of the GNU General Public 	*/
/* License along with this program; if not, write to the Free 	*/
/* Software Foundation, Inc., 59 Temple Place, Suite 330, 	*/
/* Boston, MA 02111-1307, USA.					*/
/****************************************************************/


/****************************************************************/
/* Type Definitions						*/
/****************************************************************/


/****************************************************************/
/* The following are the commands that are supported by the 	*/
/* camera. All other commands will be rejected as unsupported.	*/
/*								*/
/* 0x9200: 	On 'HP PhotoSmart C20', this command seems to 	*/
/*		set the language by transferring huge amounts 	*/
/*		of data. For 'Q-M100', this command is not 	*/
/*		defined according to Konica.			*/
/* 0x9e10:	Used for development and testing of the camera.	*/
/* 0x9e20:	Used for development and testing of the camera.	*/
/*								*/
/* For those commands, no documentation is available. They are	*/
/* therefore not implemented.					*/
/****************************************************************/
typedef enum {
	K_ERASE_IMAGE			= 0x8000,
	K_FORMAT_MEMORY_CARD	 	= 0x8010,
	K_ERASE_ALL			= 0x8020,
	K_SET_PROTECT_STATUS		= 0x8030,
	K_GET_THUMBNAIL			= 0x8800,
	K_GET_IMAGE_JPEG		= 0x8810,
	K_GET_IMAGE_INFORMATION		= 0x8820,
	K_GET_IMAGE_EXIF		= 0x8830,
	K_GET_PREVIEW			= 0x8840,
	K_GET_IO_CAPABILITY		= 0x9000,
	K_GET_INFORMATION		= 0x9010,
	K_GET_STATUS			= 0x9020,
	K_GET_DATE_AND_TIME		= 0x9030,
	K_GET_PREFERENCES		= 0x9040,
	K_SET_IO_CAPABILITY		= 0x9080,
	K_SET_DATE_AND_TIME		= 0x90b0,
	K_SET_PREFERENCE		= 0x90c0,
	K_RESET_PREFERENCES		= 0x90c1,
	K_TAKE_PICTURE			= 0x9100,
	K_PUT_LOCALIZATION_FILE		= 0x9200, 
	K_CANCEL			= 0x9e00
	// ?				= 0x9e10,
	// ?				= 0x9e20,
} k_command_t;


/****************************************************************/
/* The following are the preferences that can be set. The 	*/
/* sixteen bits that identify the preference are as follows.  	*/
/* Low order byte:	0000	0aa0			 	*/
/* High order byte:	1x0b	xxxx				*/
/* x can be 1 or 0 with no effect on the command. The a and b	*/
/* bits identify the preference and are as follows.		*/
/*								*/
/*	b:	0			1			*/
/* aa:	----------------------------------------------------	*/
/* 00	|	resolution		flash			*/
/* 01	|	exposure		focus / self timer	*/
/* 10	|	self timer time		auto off time		*/
/* 11	|	slide show interval	beep			*/
/*								*/
/*								*/
/* A short explanation of the values of the preferences: 	*/
/*								*/
/*								*/
/* RESOLUTION (default: 2)					*/
/* Only the low order byte is processed.			*/
/*  - 0: medium resolution (same as 2)				*/
/*  - 1: high resolution (1152 x 872)				*/
/*  - 2: medium resolution (1152 x 872)				*/
/*  - 3: low resolution	( 576 x 436)				*/
/* Other values are accepted as well, produce however strange	*/
/* results on the display, as the camera will calculate the 	*/
/* remaining pictures on the basis of this value. Those values	*/
/* are at some point changed to 'normal' ones.			*/
/* Each pixel has an area of 4.6um x 4.6 um.			*/
/*								*/
/*								*/
/* EXPOSURE (default: 0)					*/
/* Only the low order byte is processed.			*/
/*  - 0 to 255. 						*/
/* The behaviour is somewhat strange. Sending a value will	*/
/* not necessarily set the value to exactly the sent one.	*/
/* The following table gives an overview.			*/
/* 	sent	received	sent	received		*/
/*	----------------	----------------		*/
/*	  0  -	  0		128	129			*/
/*	  1	  0		129	130			*/
/*	  2	  1		131  -	131			*/
/*	  3	  2		132	133			*/
/*	  4	  3		133	134			*/
/*	  5  -	  5		134	135			*/
/*	  6	  5		135	136			*/
/*	  7	  6		136  -	136			*/
/*	  8	  7		   (...)			*/
/*	  9	  8		246  -	246			*/
/*	 10  -	 10		247	248			*/
/*	   (...)		248	249			*/
/*	120  -	120		249	250			*/
/*	121	120		250	251			*/
/*	122	121		251  -	251			*/
/*	123	122		252	253			*/
/*	124	123		253	254			*/
/*	125  -	125		254	255			*/
/*	126	125		255	  0			*/
/*	127	126						*/
/*								*/
/* Additional information from HP:				*/
/*  - Range: EV 6 to EV 16 (ISO 100). 				*/
/*  - Exposure times: 1/500th to 2 (sec)			*/
/*								*/
/*								*/
/* SELF_TIMER_TIME (default: 10)				*/
/*  - 3 to 40. Measured in seconds.				*/
/* All other values are rejected.				*/
/*								*/
/*								*/
/* SLIDE_SHOW_INTERVAL (default: 3)				*/
/*  - 1 to 30. Measured in seconds.				*/
/* All other values are rejected.				*/
/*								*/
/*								*/
/* FLASH (default: 2)						*/
/* Only the low order byte is processed.			*/
/*  - 0: off							*/
/*  - 1: on							*/
/*  - 2: auto							*/
/*  - 5: on, redeye						*/
/*  - 6: auto, redeye						*/
/* Other values are accepted as well, but will be changed to 2	*/
/* at some point.						*/
/*								*/
/*								*/
/* FOCUS_SELF_TIMER (default: 0)				*/
/* Only the low order byte is processed.			*/
/*  - 0: fixed, next picture in normal mode			*/
/*  - 1: fixed, next picture in self timer mode			*/
/*  - 2: auto, next picture in normal mode			*/
/*  - 3: auto, next picture in self timer mode			*/
/* After the next picture has been taken in self timer mode,  	*/
/* the value is automatically changed to the next lower even	*/
/* value (normal mode).						*/
/*								*/
/* Additional information from HP:				*/
/*  - Fixed: 2.6 feet to infinity				*/
/*  - Auto: 8 inches to infinity				*/
/*								*/
/*								*/
/* AUTO_OFF_TIME (default: 5)					*/
/* Only the low order byte is processed.			*/
/*  - 0: Will be changed by the camera to 255.			*/
/*  - 1 to 255. Measured in minutes.				*/
/*								*/
/*								*/
/* BEEP (default: 1)						*/
/* Only the low order byte is processed.			*/
/*  - 0: off							*/
/*  - 1 to 255: on						*/
/****************************************************************/
typedef enum {
	K_PREFERENCE_RESOLUTION,
	K_PREFERENCE_EXPOSURE,
	K_PREFERENCE_SELF_TIMER_TIME,
	K_PREFERENCE_SLIDE_SHOW_INTERVAL,
	K_PREFERENCE_FLASH,
	K_PREFERENCE_FOCUS_SELF_TIMER,
	K_PREFERENCE_AUTO_OFF_TIME,
	K_PREFERENCE_BEEP
} k_preference_t;


/****************************************************************/
/* The following is a list of return states that the camera	*/
/* will send (third and fourth byte, see above). The ones 	*/
/* before K_SUCCESS have been added to make programming         */
/* easier - they are _not_ sent by the camera.			*/
/*								*/
/* The list is by no means complete - I added an item each time	*/
/* I discovered a new return state.				*/
/****************************************************************/
typedef enum {
	K_L_IO_ERROR,
	K_L_TRANSMISSION_ERROR,
	K_L_SUCCESS,
	K_UNSUPPORTED_BITRATE,
	K_PROGRAM_ERROR,
	K_SUCCESS,
	K_ERROR_FOCUSING_ERROR,
	K_ERROR_IRIS_ERROR,
	K_ERROR_STROBE_ERROR,
	K_ERROR_EEPROM_CHECKSUM_ERROR,
	K_ERROR_INTERNAL_ERROR1,
	K_ERROR_INTERNAL_ERROR2,
	K_ERROR_NO_CARD_PRESENT,
	K_ERROR_CARD_NOT_SUPPORTED,
	K_ERROR_CARD_REMOVED_DURING_ACCESS,
	K_ERROR_IMAGE_NUMBER_NOT_VALID,
	K_ERROR_CARD_CAN_NOT_BE_WRITTEN,
	K_ERROR_CARD_IS_WRITE_PROTECTED,
	K_ERROR_NO_SPACE_LEFT_ON_CARD,
	K_ERROR_NO_PICTURE_ERASED_AS_IMAGE_PROTECTED,
	K_ERROR_LIGHT_TOO_DARK,
	K_ERROR_AUTOFOCUS_ERROR,
	K_ERROR_SYSTEM_ERROR,
	K_ERROR_ILLEGAL_PARAMETER,
	K_ERROR_COMMAND_CANNOT_BE_CANCELLED,
	K_ERROR_LOCALIZATION_DATA_EXCESS,
	K_ERROR_LOCALIZATION_DATA_CORRUPT,
	K_ERROR_UNSUPPORTED_COMMAND,
	K_ERROR_OTHER_COMMAND_EXECUTING,
	K_ERROR_COMMAND_ORDER_ERROR,
	K_ERROR_UNKNOWN_ERROR
} k_return_status_t;


typedef enum {
	K_POWER_LEVEL_LOW, 
	K_POWER_LEVEL_NORMAL,
	K_POWER_LEVEL_HIGH
} k_power_level_t;


typedef enum {
	K_POWER_SOURCE_BATTERY,
	K_POWER_SOURCE_AC
} k_power_source_t;


typedef enum {
	K_THUMBNAIL,
	K_IMAGE_JPEG,
	K_IMAGE_EXIF
} k_image_type_t;


typedef enum {
	K_DISPLAY_BUILT_IN,
	K_DISPLAY_TV
} k_display_t;


typedef enum {
	K_CARD_STATUS_CARD,
	K_CARD_STATUS_NO_CARD
} k_card_status_t;


/****************************************************************/
/* Functions							*/
/****************************************************************/


/****************************************************************/
/* Two flavours of Konica's protocol exist: One uses two bytes  */
/* for image IDs (qm100, unsigned int), the other one uses four */
/* (qm200, unsigned long).                                      */
/****************************************************************/
k_return_status_t k_init (konica_data_t *konica_data);


k_return_status_t k_exit (konica_data_t *konica_data);


k_return_status_t k_get_io_capability (
	konica_data_t *konica_data,
	gboolean *bit_rate_300,
	gboolean *bit_rate_600,
	gboolean *bit_rate_1200,
	gboolean *bit_rate_2400,
	gboolean *bit_rate_4800,
	gboolean *bit_rate_9600,
	gboolean *bit_rate_19200,
	gboolean *bit_rate_38400,
	gboolean *bit_rate_57600,
	gboolean *bit_rate_115200, 
	gboolean *bit_flag_8_bits,
	gboolean *bit_flag_stop_2_bits,
	gboolean *bit_flag_parity_on,
	gboolean *bit_flag_parity_odd,
	gboolean *bit_flag_hw_flow_control);


k_return_status_t k_set_io_capability (
	konica_data_t *konica_data,
	guint bit_rate,
	gboolean bit_flag_7_or_8_bits,
	gboolean bit_flag_stop_2_bits,
	gboolean bit_flag_parity_on,
	gboolean bit_flag_parity_odd,
	gboolean bit_flag_use_hw_flow_control);


k_return_status_t k_erase_all (
        konica_data_t *konica_data,
	guint *number_of_images_not_erased);


k_return_status_t k_format_memory_card (konica_data_t *konica_data);


k_return_status_t k_take_picture (
        konica_data_t *konica_data,
	gulong *image_id, 
	guint *exif_size,
	guchar **information_buffer, 
	guint *information_buffer_size,
	gboolean *protected);


k_return_status_t k_get_preview (
        konica_data_t *konica_data,
	gboolean thumbnail, 
	guchar **image_buffer, 
	guint *image_buffer_size);


k_return_status_t k_set_preference (
        konica_data_t *konica_data,
	k_preference_t preference, 
	guint value);


k_return_status_t k_set_protect_status (
        konica_data_t *konica_data,
	gulong image_id, 
	gboolean protected);


k_return_status_t k_erase_image (
        konica_data_t *konica_data,
	gulong image_id);


k_return_status_t k_reset_preferences (konica_data_t *konica_data);


k_return_status_t k_get_date_and_time (
        konica_data_t *konica_data,
	guchar *year, 
	guchar *month, 
	guchar *day, 
	guchar *hour, 
	guchar *minute, 
	guchar *second);


k_return_status_t k_set_date_and_time (
        konica_data_t *konica_data,
	guchar year, 
	guchar month, 
	guchar day, 
	guchar hour, 
	guchar minute, 
	guchar second);


k_return_status_t k_get_preferences (
        konica_data_t *konica_data,
	guint *shutoff_time, 
	guint *self_timer_time, 
	guint *beep, 
	guint *slide_show_interval);


k_return_status_t k_get_status (
        konica_data_t *konica_data,
	guint *self_test_result, 
	k_power_level_t	*power_level,
	k_power_source_t *power_source,
	k_card_status_t	*card_status,
	k_display_t *display, 
	guint *card_size,
	guint *pictures, 
	guint *pictures_left, 
	guchar *year, 
	guchar *month, 
	guchar *day,
	guchar *hour,
	guchar *minute,
	guchar *second,
	guint *io_setting_bit_rate,
	guint *io_setting_flags,
	guchar *flash,
	guchar *resolution,
	guchar *focus,
	guchar *exposure,
	guint *total_pictures,
	guint *total_strobes);


k_return_status_t k_get_information (
        konica_data_t *konica_data,
	gchar **model,
	gchar **serial_number,
	guchar *hardware_version_major,
	guchar *hardware_version_minor,
	guchar *software_version_major, 
	guchar *software_version_minor,
	guchar *testing_software_version_major,
	guchar *testing_software_version_minor,
	gchar **name,
	gchar **manufacturer);


k_return_status_t k_get_image_information (
        konica_data_t *konica_data,
	gulong image_number,
	gulong *image_id, 
	guint *exif_size, 
	gboolean *protected, 
	guchar **information_buffer,
	guint *information_buffer_size);


k_return_status_t k_get_image (
        konica_data_t *konica_data,
	gulong image_id, 
	k_image_type_t image_type, 
	guchar **image_buffer, 
	guint *image_buffer_size);


k_return_status_t k_set_protect_status (
        konica_data_t *konica_data,
	gulong image_id, 
	gboolean protected);


k_return_status_t k_put_localization_data (
        konica_data_t *konica_data,
	guchar *data,
	gulong data_size);


k_return_status_t k_cancel (
        konica_data_t *konica_data,
	k_command_t *command);
