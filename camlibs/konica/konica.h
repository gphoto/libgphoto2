/* konica.h
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sourceforge.net>
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

#ifndef __KONICA_H__
#define __KONICA_H__

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-context.h>

/*
 * The following are the commands that are supported by the
 * camera. All other commands will be rejected as unsupported.
 *
 * 0x9200: 	On 'HP PhotoSmart C20', this command seems to
 *		set the language by transferring huge amounts
 *		of data. For 'Q-M100', this command is not
 *		defined according to Konica.
 * 0x9e10:	Used for development and testing of the camera.
 * 0x9e20:	Used for development and testing of the camera.
 *
 * For those commands, no documentation is available. They are
 * therefore not implemented.
 */
typedef enum
{
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
	/* ?				= 0x9e10, */
	/* ?				= 0x9e20, */
} KCommand;

/*
 * The following are the preferences that can be set. The
 * sixteen bits that identify the preference are as follows.
 * Low order byte:	0000	0aa0
 * High order byte:	1x0b	xxxx
 * x can be 1 or 0 with no effect on the command. The a and b
 * bits identify the preference and are as follows.
 *
 *	b:	0			1
 * aa:	----------------------------------------------------
 * 00	|	resolution		flash
 * 01	|	exposure		focus / self timer
 * 10	|	self timer time		auto off time
 * 11	|	slide show interval	beep
 *
 * A short explanation of the values of the preferences:
 *
 * RESOLUTION (default: 2)
 * Only the low order byte is processed.
 *  - 0: medium resolution (same as 2)
 *  - 1: high resolution (1152 x 872)
 *  - 2: medium resolution (1152 x 872)
 *  - 3: low resolution	( 576 x 436)
 * Other values are accepted as well, produce however strange
 * results on the display, as the camera will calculate the
 * remaining pictures on the basis of this value. Those values
 * are at some point changed to 'normal' ones.
 * Each pixel has an area of 4.6um x 4.6 um.
 *
 * EXPOSURE (default: 0)
 * Only the low order byte is processed.
 *  - 0 to 255.
 * The behaviour is somewhat strange. Sending a value will
 * not necessarily set the value to exactly the sent one.
 * The following table gives an overview.
 * 	sent	received	sent	received
 *	----------------	----------------
 *	  0  -	  0		128	129
 *	  1	  0		129	130
 *	  2	  1		131  -	131
 *	  3	  2		132	133
 *	  4	  3		133	134
 *	  5  -	  5		134	135
 *	  6	  5		135	136
 *	  7	  6		136  -	136
 *	  8	  7		   (...)
 *	  9	  8		246  -	246
 *	 10  -	 10		247	248
 *	   (...)		248	249
 *	120  -	120		249	250
 *	121	120		250	251
 *	122	121		251  -	251
 *	123	122		252	253
 *	124	123		253	254
 *	125  -	125		254	255
 *	126	125		255	  0
 *	127	126
 *
 * Additional information from HP:
 *  - Range: EV 6 to EV 16 (ISO 100).
 *  - Exposure times: 1/500th to 2 (sec)
 *
 * SELF_TIMER_TIME (default: 10)
 *  - 3 to 40. Measured in seconds.
 * All other values are rejected.
 *
 * SLIDE_SHOW_INTERVAL (default: 3)
 *  - 1 to 30. Measured in seconds.
 * All other values are rejected.
 *
 * FLASH (default: 2)
 * Only the low order byte is processed.
 *  - 0: off
 *  - 1: on
 *  - 2: auto
 *  - 5: on, redeye
 *  - 6: auto, redeye
 * Other values are accepted as well, but will be changed to 2
 * at some point.
 *
 * FOCUS_SELF_TIMER (default: 0)
 * Only the low order byte is processed.
 *  - 0: fixed, next picture in normal mode
 *  - 1: fixed, next picture in self timer mode
 *  - 2: auto, next picture in normal mode
 *  - 3: auto, next picture in self timer mode
 * After the next picture has been taken in self timer mode,
 * the value is automatically changed to the next lower even
 * value (normal mode).
 *
 * Additional information from HP:
 *  - Fixed: 2.6 feet to infinity
 *  - Auto: 8 inches to infinity
 *
 * AUTO_OFF_TIME (default: 5)
 * Only the low order byte is processed.
 *  - 0: Will be changed by the camera to 255.
 *  - 1 to 255. Measured in minutes.
 *
 * BEEP (default: 1)
 * Only the low order byte is processed.
 *  - 0: off
 *  - 1 to 255: on
 */
typedef enum
{
	K_PREFERENCE_RESOLUTION          = 0xc000,
	K_PREFERENCE_EXPOSURE            = 0xc002,
	K_PREFERENCE_SELF_TIMER_TIME     = 0xc004,
	K_PREFERENCE_SLIDE_SHOW_INTERVAL = 0xc006,
	K_PREFERENCE_FLASH               = 0xd000,
	K_PREFERENCE_FOCUS_SELF_TIMER    = 0xd002,
	K_PREFERENCE_AUTO_OFF_TIME       = 0xd004,
	K_PREFERENCE_BEEP                = 0xd006
} KPreference;

typedef enum
{
	K_POWER_LEVEL_LOW    = 0x00,
	K_POWER_LEVEL_NORMAL = 0x01,
	K_POWER_LEVEL_HIGH   = 0x02
} KPowerLevel;

typedef enum
{
	K_POWER_SOURCE_BATTERY = 0x00,
	K_POWER_SOURCE_AC      = 0x01
} KPowerSource;

typedef enum
{
	K_THUMBNAIL  = 0x00,
	K_IMAGE_JPEG = 0x10,
	K_IMAGE_EXIF = 0x30
} KImageType;

typedef enum
{
	K_DISPLAY_BUILT_IN = 0x00,
	K_DISPLAY_TV       = 0x02
} KDisplay;

typedef enum
{
	K_CARD_STATUS_CARD    = 0x07,
	K_CARD_STATUS_NO_CARD = 0x12
} KCardStatus;

typedef enum
{
	K_DATE_FORMAT_MONTH_DAY_YEAR = 0x00,
	K_DATE_FORMAT_DAY_MONTH_YEAR = 0x01,
	K_DATE_FORMAT_YEAR_MONTH_DAY = 0x02
} KDateFormat;

typedef enum
{
	K_TV_OUTPUT_FORMAT_NTSC = 0x00,
	K_TV_OUTPUT_FORMAT_PAL  = 0x01,
	K_TV_OUTPUT_FORMAT_HIDE = 0x02
} KTVOutputFormat;

/*
 * Two flavours of Konica's protocol exist: One uses two bytes
 * for image IDs (qm100, unsigned int), the other one uses four
 * (qm200, unsigned long).
 */
int k_init (GPPort *, GPContext *);
int k_ping (GPPort *, GPContext *);

typedef enum
{
	K_BIT_RATE_300    = 1 << 0,
	K_BIT_RATE_600    = 1 << 1,
	K_BIT_RATE_1200   = 1 << 2,
	K_BIT_RATE_2400   = 1 << 3,
	K_BIT_RATE_4800   = 1 << 4,
	K_BIT_RATE_9600   = 1 << 5,
	K_BIT_RATE_19200  = 1 << 6,
	K_BIT_RATE_38400  = 1 << 7,
	K_BIT_RATE_57600  = 1 << 8,
	K_BIT_RATE_115200 = 1 << 9
} KBitRate;

typedef enum
{
	K_BIT_FLAG_8_BITS       = 1 << 0,
	K_BIT_FLAG_STOP_2_BITS  = 1 << 1,
	K_BIT_FLAG_PARITY_ON    = 1 << 2,
	K_BIT_FLAG_PARITY_ODD   = 1 << 3,
	K_BIT_FLAG_HW_FLOW_CTRL = 1 << 4
} KBitFlag;

int k_get_io_capability (GPPort *, GPContext *,
			 KBitRate *bit_rates, KBitFlag *bit_flags);
int k_set_io_capability (GPPort *, GPContext *,
			 KBitRate  bit_rate,  KBitFlag  bit_flags);

int k_erase_all (GPPort *device, GPContext *,
		 unsigned int *number_of_images_not_erased);

int k_format_memory_card (GPPort *, GPContext *);

int k_take_picture (
	GPPort *device, GPContext *,
	int image_id_long,
	unsigned long *image_id,
	unsigned int *exif_size,
	unsigned char **information_buffer,
	unsigned int *information_buffer_size,
	int *protected);


int k_get_preview (GPPort *device, GPContext *, int thumbnail,
		   unsigned char **image_buffer,
		   unsigned int *image_buffer_size);


int k_set_protect_status (GPPort *device, GPContext *, int image_id_long,
			  unsigned long image_id, int protected);


int k_erase_image (GPPort *device, GPContext *, int image_id_long,
		   unsigned long image_id);


int k_reset_preferences (GPPort *device, GPContext *);

typedef struct {
	unsigned char year, month, day;
	unsigned char hour, minute, second;
} KDate;

int k_get_date_and_time (GPPort *device, GPContext *, KDate *date);
int k_set_date_and_time (GPPort *device, GPContext *, KDate  date);

typedef struct {
	unsigned int shutoff_time;
	unsigned int self_timer_time;
	unsigned int beep;
	unsigned int slide_show_interval;
} KPreferences;

int k_get_preferences (GPPort *device, GPContext *, KPreferences *preferences);
int k_set_preference  (GPPort *device, GPContext *, KPreference   preference,
		       unsigned int value);

typedef struct {
	KPowerLevel  power_level;
	KPowerSource power_source;
	KCardStatus  card_status;
	KDisplay     display;
	unsigned int self_test_result;
	unsigned int card_size;
	unsigned int pictures, pictures_left;
	KDate date;
	unsigned int bit_rate;
	KBitFlag bit_flags;
	unsigned char flash, resolution, focus, exposure;
	unsigned char total_pictures, total_strobes;
} KStatus;

int k_get_status (GPPort *device, GPContext *, KStatus *status);

typedef struct _KVersion KVersion;
struct _KVersion {
	unsigned char major;
	unsigned char minor;
};

typedef struct _KInformation KInformation;
struct _KInformation {
	char model[5];
	char serial_number[11];
	KVersion hardware;
	KVersion software;
	KVersion testing;
	char name[23];
	char manufacturer[31];
};

int k_get_information (GPPort *device, GPContext *, KInformation *info);

int k_get_image_information (
	GPPort *device, GPContext *,
	int image_id_long,
	unsigned long image_number,
	unsigned long *image_id,
	unsigned int *exif_size,
	int *protected,
	unsigned char **information_buffer,
	unsigned int *information_buffer_size);

int k_get_image (
	GPPort *device, GPContext *,
	int image_id_long,
	unsigned long image_id,
	KImageType image_type,
	unsigned char **image_buffer,
	unsigned int *image_buffer_size);

/* Localization */
int k_localization_tv_output_format_set (GPPort *device, GPContext *,
					 KTVOutputFormat tv_output_format);
int k_localization_date_format_set      (GPPort *device, GPContext *,
					 KDateFormat date_format);
int k_localization_data_put             (GPPort *device, GPContext *,
					 unsigned char *data,
					 unsigned long data_size);

int k_cancel (GPPort *device, GPContext *, KCommand *command);

#endif /* __KONICA_H__ */
