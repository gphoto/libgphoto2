/*
    pkTriggerCord
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    PK-Remote for Windows
    Copyright (C) 2010 Tomasz Kos

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License
    and GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef PSLR_ENUM_H
#define PSLR_ENUM_H

#include <stdint.h>

typedef enum {
    PSLR_COLOR_SPACE_SRGB,
    PSLR_COLOR_SPACE_ADOBERGB,
    PSLR_COLOR_SPACE_MAX
} pslr_color_space_t;

typedef enum {
    PSLR_AF_MODE_MF,
    PSLR_AF_MODE_AF_S,
    PSLR_AF_MODE_AF_C,
    PSLR_AF_MODE_AF_A,
    PSLR_AF_MODE_MAX
} pslr_af_mode_t;

typedef enum {
    PSLR_AE_METERING_MULTI,
    PSLR_AE_METERING_CENTER,
    PSLR_AE_METERING_SPOT,
    PSLR_AE_METERING_MAX
} pslr_ae_metering_t;

typedef enum {
    PSLR_FLASH_MODE_MANUAL = 0,
    PSLR_FLASH_MODE_MANUAL_REDEYE = 1,
    PSLR_FLASH_MODE_SLOW = 2,
    PSLR_FLASH_MODE_SLOW_REDEYE = 3,
    PSLR_FLASH_MODE_TRAILING_CURTAIN = 4,
    PSLR_FLASH_MODE_AUTO = 5,
    PSLR_FLASH_MODE_AUTO_REDEYE = 6,
    /* 7 not used */
    PSLR_FLASH_MODE_WIRELESS = 8,
    PSLR_FLASH_MODE_MAX = 9
} pslr_flash_mode_t;

typedef enum {
    PSLR_DRIVE_MODE_SINGLE,
    PSLR_DRIVE_MODE_CONTINUOUS_HI,
    PSLR_DRIVE_MODE_SELF_TIMER_12,
    PSLR_DRIVE_MODE_SELF_TIMER_2,
    PSLR_DRIVE_MODE_REMOTE,
    PSLR_DRIVE_MODE_REMOTE_3,
    PSLR_DRIVE_MODE_CONTINUOUS_LO,
    PSLR_DRIVE_MODE_MAX
} pslr_drive_mode_t;

typedef enum {
    PSLR_AF_POINT_SEL_AUTO_5,
    PSLR_AF_POINT_SEL_SELECT,
    PSLR_AF_POINT_SEL_SPOT,
    PSLR_AF_POINT_SEL_AUTO_11, /* maybe not for all cameras */
    PSLR_AF_POINT_SEL_EXPANDED, /* only for newer */
    PSLR_AF_POINT_SEL_MAX
} pslr_af_point_sel_t;

typedef enum {
    PSLR_AF11_POINT_TOP_LEFT  = 0x01,
    PSLR_AF11_POINT_TOP_MID   = 0x2,
    PSLR_AF11_POINT_TOP_RIGHT = 0x4,
    PSLR_AF11_POINT_FAR_LEFT  = 0x8,
    PSLR_AF11_POINT_MID_LEFT  = 0x10,
    PSLR_AF11_POINT_MID_MID   = 0x20,
    PSLR_AF11_POINT_MID_RIGHT = 0x40,
    PSLR_AF11_POINT_FAR_RIGHT = 0x80,
    PSLR_AF11_POINT_BOT_LEFT  = 0x100,
    PSLR_AF11_POINT_BOT_MID   = 0x200,
    PSLR_AF11_POINT_BOT_RIGHT = 0x400
} pslr_af11_point_t;

typedef enum {
    PSLR_JPEG_IMAGE_TONE_NONE = -1,
    PSLR_JPEG_IMAGE_TONE_NATURAL,
    PSLR_JPEG_IMAGE_TONE_BRIGHT,
    PSLR_JPEG_IMAGE_TONE_PORTRAIT,
    PSLR_JPEG_IMAGE_TONE_LANDSCAPE,
    PSLR_JPEG_IMAGE_TONE_VIBRANT,
    PSLR_JPEG_IMAGE_TONE_MONOCHROME,
    PSLR_JPEG_IMAGE_TONE_MUTED,
    PSLR_JPEG_IMAGE_TONE_REVERSAL_FILM,
    PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,
    PSLR_JPEG_IMAGE_TONE_RADIANT,
    PSLR_JPEG_IMAGE_TONE_CROSS_PROCESSING,
    PSLR_JPEG_IMAGE_TONE_FLAT,
    PSLR_JPEG_IMAGE_TONE_AUTO,
    PSLR_JPEG_IMAGE_TONE_MAX
} pslr_jpeg_image_tone_t;

typedef enum {
    PSLR_WHITE_BALANCE_MODE_AUTO,
    PSLR_WHITE_BALANCE_MODE_DAYLIGHT,
    PSLR_WHITE_BALANCE_MODE_SHADE,
    PSLR_WHITE_BALANCE_MODE_CLOUDY,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_DAYLIGHT_COLOR,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_DAYLIGHT_WHITE,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_COOL_WHITE,
    PSLR_WHITE_BALANCE_MODE_TUNGSTEN,
    PSLR_WHITE_BALANCE_MODE_FLASH,
    PSLR_WHITE_BALANCE_MODE_MANUAL,
    PSLR_WHITE_BALANCE_MODE_MANUAL_2,
    PSLR_WHITE_BALANCE_MODE_MANUAL_3,
    PSLR_WHITE_BALANCE_MODE_KELVIN_1,
    PSLR_WHITE_BALANCE_MODE_KELVIN_2,
    PSLR_WHITE_BALANCE_MODE_KELVIN_3,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_WARM_WHITE,
    PSLR_WHITE_BALANCE_MODE_CTE,
    PSLR_WHITE_BALANCE_MODE_MULTI_AUTO,
    PSLR_WHITE_BALANCE_MODE_MAX
} pslr_white_balance_mode_t;

typedef enum {
    PSLR_CUSTOM_EV_STEPS_1_2,
    PSLR_CUSTOM_EV_STEPS_1_3,
    PSLR_CUSTOM_EV_STEPS_MAX
} pslr_custom_ev_steps_t;

typedef enum {
    PSLR_CUSTOM_SENSITIVITY_STEPS_1EV,
    PSLR_CUSTOM_SENSITIVITY_STEPS_AS_EV,
    PSLR_CUSTOM_SENSITIVITY_STEPS_MAX
} pslr_custom_sensitivity_steps_t;

typedef enum {
    PSLR_IMAGE_FORMAT_JPEG,
    PSLR_IMAGE_FORMAT_RAW,
    PSLR_IMAGE_FORMAT_RAW_PLUS,
    PSLR_IMAGE_FORMAT_MAX
} pslr_image_format_t;

typedef enum {
    PSLR_RAW_FORMAT_PEF,
    PSLR_RAW_FORMAT_DNG,
    PSLR_RAW_FORMAT_MAX
} pslr_raw_format_t;

typedef enum {
    PSLR_SCENE_MODE_NONE,
    PSLR_SCENE_MODE_HISPEED,
    PSLR_SCENE_MODE_DOF,
    PSLR_SCENE_MODE_MTF,
    PSLR_SCENE_MODE_STANDARD,
    PSLR_SCENE_MODE_PORTRAIT,
    PSLR_SCENE_MODE_LANDSCAPE,
    PSLR_SCENE_MODE_MACRO,
    PSLR_SCENE_MODE_SPORT,
    PSLR_SCENE_MODE_NIGHTSCENEPORTRAIT,
    PSLR_SCENE_MODE_NOFLASH,
    PSLR_SCENE_MODE_NIGHTSCENE,
    PSLR_SCENE_MODE_SURFANDSNOW,
    PSLR_SCENE_MODE_TEXT,
    PSLR_SCENE_MODE_SUNSET,
    PSLR_SCENE_MODE_KIDS,
    PSLR_SCENE_MODE_PET,
    PSLR_SCENE_MODE_CANDLELIGHT,
    PSLR_SCENE_MODE_MUSEUM,
    PSLR_SCENE_MODE_19,
    PSLR_SCENE_MODE_FOOD,
    PSLR_SCENE_MODE_STAGE,
    PSLR_SCENE_MODE_NIGHTSNAP,
    PSLR_SCENE_MODE_SWALLOWDOF,
    PSLR_SCENE_MODE_24,
    PSLR_SCENE_MODE_NIGHTSCENEHDR,
    PSLR_SCENE_MODE_BLUESKY,
    PSLR_SCENE_MODE_FOREST,
    PSLR_SCENE_MODE_28,
    PSLR_SCENE_MODE_BLACKLIGHTSILHOUETTE,
    PSLR_SCENE_MODE_MAX
} pslr_scene_mode_t;

int str_comparison_i (const char *s1, const char *s2, int n);
int find_in_array( const char** array, int length, char* str );

pslr_color_space_t get_pslr_color_space( char *str );
const char *get_pslr_color_space_str( pslr_color_space_t value );

pslr_af_mode_t get_pslr_af_mode( char *str );
const char *get_pslr_af_mode_str( pslr_af_mode_t value );

pslr_ae_metering_t get_pslr_ae_metering( char *str );
const char *get_pslr_ae_metering_str( pslr_ae_metering_t value );

pslr_flash_mode_t get_pslr_flash_mode( char *str );
const char *get_pslr_flash_mode_str( pslr_flash_mode_t value );

pslr_drive_mode_t get_pslr_drive_mode( char *str );
const char *get_pslr_drive_mode_str( pslr_drive_mode_t value );

pslr_af_point_sel_t get_pslr_af_point_sel( char *str );
const char *get_pslr_af_point_sel_str( pslr_af_point_sel_t value );

char *get_pslr_af11_point_str( uint32_t value );

pslr_jpeg_image_tone_t get_pslr_jpeg_image_tone( char *str );
const char *get_pslr_jpeg_image_tone_str( pslr_jpeg_image_tone_t value );

pslr_white_balance_mode_t get_pslr_white_balance_mode( char *str );
const char *get_pslr_white_balance_mode_str( pslr_white_balance_mode_t value );

/* pslr_custom_ev_steps_t get_pslr_custom_ev_steps( char *str ); */
const char *get_pslr_custom_ev_steps_str( pslr_custom_ev_steps_t value );

const char *get_pslr_custom_sensitivity_steps_str( pslr_custom_sensitivity_steps_t value );

const char *get_pslr_image_format_str( pslr_image_format_t value );

const char *get_pslr_raw_format_str( pslr_raw_format_t value );

const char *get_pslr_scene_mode_str( pslr_scene_mode_t value );

#endif
