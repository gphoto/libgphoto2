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


typedef enum {
	K_DATE_FORMAT_MONTH_DAY_YEAR,
	K_DATE_FORMAT_DAY_MONTH_YEAR,
	K_DATE_FORMAT_YEAR_MONTH_DAY
} k_date_format_t;


typedef enum {
	K_TV_OUTPUT_FORMAT_NTSC,
	K_TV_OUTPUT_FORMAT_PAL,
	K_TV_OUTPUT_FORMAT_HIDE
} k_tv_output_format_t;


/****************************************************************/
/* Functions							*/
/****************************************************************/


/****************************************************************/
/* Two flavours of Konica's protocol exist: One uses two bytes  */
/* for image IDs (qm100, unsigned int), the other one uses four */
/* (qm200, unsigned long).                                      */
/****************************************************************/
gint k_init (gpio_device *device);


gint k_exit (gpio_device *device);


gint k_get_io_capability (
	gpio_device *device,
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


gint k_set_io_capability (
	gpio_device *device,
	guint bit_rate,
	gboolean bit_flag_7_or_8_bits,
	gboolean bit_flag_stop_2_bits,
	gboolean bit_flag_parity_on,
	gboolean bit_flag_parity_odd,
	gboolean bit_flag_use_hw_flow_control);


gint k_erase_all (gpio_device *device, guint *number_of_images_not_erased);


gint k_format_memory_card (gpio_device *device);


gint k_take_picture (
	gpio_device *device,
	gboolean image_id_long,
	gulong *image_id, 
	guint *exif_size,
	guchar **information_buffer, 
	guint *information_buffer_size,
	gboolean *protected);


gint k_get_preview (gpio_device *device, gboolean thumbnail, guchar **image_buffer, guint *image_buffer_size);


gint k_set_preference (gpio_device *device, k_preference_t preference, guint value);


gint k_set_protect_status (gpio_device *device, gboolean image_id_long, gulong image_id, gboolean protected);


gint k_erase_image (gpio_device *device, gboolean image_id_long, gulong image_id);


gint k_reset_preferences (gpio_device *device);


gint k_get_date_and_time (
        gpio_device *device,
	guchar *year, 
	guchar *month, 
	guchar *day, 
	guchar *hour, 
	guchar *minute, 
	guchar *second);


gint k_set_date_and_time (
	gpio_device *device,
	guchar year, 
	guchar month, 
	guchar day, 
	guchar hour, 
	guchar minute, 
	guchar second);


gint k_get_preferences (
	gpio_device *device,
	guint *shutoff_time, 
	guint *self_timer_time, 
	guint *beep, 
	guint *slide_show_interval);


gint k_get_status (
	gpio_device *device,
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


gint k_get_information (
	gpio_device *device,
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


gint k_get_image_information (
	gpio_device *device,
	gboolean image_id_long,
	gulong image_number,
	gulong *image_id, 
	guint *exif_size, 
	gboolean *protected, 
	guchar **information_buffer,
	guint *information_buffer_size);


gint k_get_image (
	gpio_device *device,
	gboolean image_id_long,
	gulong image_id, 
	k_image_type_t image_type, 
	guchar **image_buffer, 
	guint *image_buffer_size);


gint k_set_protect_status (gpio_device *device, gboolean image_id_long, gulong image_id, gboolean protected);


gint k_localization_tv_output_format_set (gpio_device *device, k_tv_output_format_t tv_output_format);


gint k_localization_date_format_set (gpio_device *device, k_date_format_t date_format);


gint k_localization_data_put (gpio_device *device, guchar *data, gulong data_size);


gint k_cancel (gpio_device *device, k_command_t *command);

