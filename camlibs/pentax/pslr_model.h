/*
    pkTriggerCord
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    Support for K200D added by Jens Dreyer <jens.dreyer@udo.edu> 04/2011
    Support for K-r added by Vincenc Podobnik <vincenc.podobnik@gmail.com> 06/2011
    Support for K-30 added by Camilo Polymeris <cpolymeris@gmail.com> 09/2012
    Support for K-01 added by Ethan Queen <ethanqueen@gmail.com> 01/2013
    Support for K-3 added by Tao Wang <twang2218@gmail.com> 01/2016

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
#ifndef PSLR_MODEL_H
#define PSLR_MODEL_H

#include "pslr_enum.h"
#include "pslr_scsi.h"

#define MAX_RESOLUTION_SIZE 4
#define MAX_STATUS_BUF_SIZE 456
#define SETTINGS_BUFFER_SIZE 1024
#define MAX_SEGMENTS 4

typedef struct ipslr_handle ipslr_handle_t;

typedef struct {
    int32_t nom;
    int32_t denom;
} pslr_rational_t;

typedef struct {
    uint16_t bufmask;
    uint32_t current_iso;
    pslr_rational_t current_shutter_speed;
    pslr_rational_t current_aperture;
    pslr_rational_t lens_max_aperture;
    pslr_rational_t lens_min_aperture;
    pslr_rational_t set_shutter_speed;
    pslr_rational_t set_aperture;
    pslr_rational_t max_shutter_speed;
    uint32_t auto_bracket_mode;
    pslr_rational_t auto_bracket_ev;
    uint32_t auto_bracket_picture_count;
    uint32_t auto_bracket_picture_counter;
    uint32_t fixed_iso;
    uint32_t jpeg_resolution;
    uint32_t jpeg_saturation;
    uint32_t jpeg_quality;
    uint32_t jpeg_contrast;
    uint32_t jpeg_sharpness;
    uint32_t jpeg_image_tone;
    uint32_t jpeg_hue;
    pslr_rational_t zoom;
    int32_t focus;
    uint32_t image_format;
    uint32_t raw_format;
    uint32_t light_meter_flags;
    pslr_rational_t ec;
    uint32_t custom_ev_steps;
    uint32_t custom_sensitivity_steps;
    uint32_t exposure_mode;
    uint32_t scene_mode;
    uint32_t user_mode_flag;
    uint32_t ae_metering_mode;
    uint32_t af_mode;
    uint32_t af_point_select;
    uint32_t selected_af_point;
    uint32_t focused_af_point;
    uint32_t auto_iso_min;
    uint32_t auto_iso_max;
    uint32_t drive_mode;
    uint32_t shake_reduction;
    uint32_t white_balance_mode;
    uint32_t white_balance_adjust_mg;
    uint32_t white_balance_adjust_ba;
    uint32_t flash_mode;
    int32_t flash_exposure_compensation; /* 1/256 */
    int32_t manual_mode_ev; /* 1/10 */
    uint32_t color_space;
    uint32_t lens_id1;
    uint32_t lens_id2;
    uint32_t battery_1;
    uint32_t battery_2;
    uint32_t battery_3;
    uint32_t battery_4;
} pslr_status;

typedef enum {
    PSLR_SETTING_STATUS_UNKNOWN,
    PSLR_SETTING_STATUS_READ,
    PSLR_SETTING_STATUS_HARDWIRED,
    PSLR_SETTING_STATUS_NA
} pslr_setting_status_t;

typedef struct {
    pslr_setting_status_t pslr_setting_status;
    bool value;
} pslr_bool_setting;

typedef struct {
    pslr_setting_status_t pslr_setting_status;
    uint16_t value;
} pslr_uint16_setting;

typedef struct {
    pslr_bool_setting one_push_bracketing;
    pslr_bool_setting bulb_mode_press_press;
    pslr_bool_setting bulb_timer;
    pslr_uint16_setting bulb_timer_sec;
    pslr_bool_setting using_aperture_ring;
    pslr_bool_setting shake_reduction;
    pslr_bool_setting astrotracer;
    pslr_uint16_setting astrotracer_timer_sec;
    pslr_bool_setting horizon_correction;
    pslr_bool_setting remote_bulb_mode_press_press;
} pslr_settings;

typedef struct {
    const char *name;
    unsigned long address;
    const char *value;
    const char *type;
} pslr_setting_def_t;

typedef void (*ipslr_status_parse_t)(ipslr_handle_t *p, pslr_status *status);
pslr_setting_def_t *find_setting_by_name (pslr_setting_def_t *array, int array_length, char *name);
void ipslr_settings_parser_json(const char *cameraid, ipslr_handle_t *p, pslr_settings *settings);
pslr_setting_def_t *setting_file_process(const char *cameraid, int *def_num);

typedef struct {
    uint32_t id;                                     // Pentax model ID
    const char *name;                                // name
    bool old_scsi_command;                           // true for *ist cameras, false for the newer cameras
    bool old_bulb_mode;                              // true for older cameras
    bool need_exposure_mode_conversion;              // is exposure_mode_conversion required
    bool bufmask_command;                            // true if bufmask determined by calling command 0x02 0x00
    bool bufmask_single;                             // true if buffer cannot handle multiple images
    bool is_little_endian;                           // whether the return value should be parsed as little-endian
    int status_buffer_size;                          // status buffer size in bytes
    int max_jpeg_stars;                              // maximum jpeg stars
    int jpeg_resolutions[MAX_RESOLUTION_SIZE];       // jpeg resolution table
    int jpeg_property_levels;                        // 5 [-2, 2] or 7 [-3,3] or 9 [-4,4]
    int fastest_shutter_speed;                       // fastest shutter speed denominator
    int base_iso_min;                                // base iso minimum
    int base_iso_max;                                // base iso maximum
    int extended_iso_min;                            // extended iso minimum
    int extended_iso_max;                            // extended iso maximum
    pslr_jpeg_image_tone_t max_supported_image_tone; // last supported jpeg image tone
    bool has_jpeg_hue;                               // camera has jpeg hue setting
    int af_point_num;                                // number of AF points
    ipslr_status_parse_t status_parser_function;     // parse function for status buffer
} ipslr_model_info_t;

typedef struct {
    uint32_t offset;
    uint32_t addr;
    uint32_t length;
} ipslr_segment_t;

struct ipslr_handle {
    FDTYPE fd;
    pslr_status status;
    pslr_settings settings;
    uint32_t id;
    ipslr_model_info_t *model;
    ipslr_segment_t segments[MAX_SEGMENTS];
    uint32_t segment_count;
    uint32_t offset;
    uint8_t status_buffer[MAX_STATUS_BUF_SIZE];
    uint8_t settings_buffer[SETTINGS_BUFFER_SIZE];
};

ipslr_model_info_t *find_model_by_id( uint32_t id );

int get_hw_jpeg_quality( ipslr_model_info_t *model, int user_jpeg_stars);

uint32_t get_uint32_be(uint8_t *buf);
uint32_t get_uint32_le(uint8_t *buf);
void set_uint32_be(uint32_t v, uint8_t *buf);
void set_uint32_le(uint32_t v, uint8_t *buf);

typedef uint32_t (*get_uint32_func)(uint8_t *buf);
typedef uint16_t (*get_uint16_func)(const uint8_t *buf);
typedef int32_t (*get_int32_func)(uint8_t *buf);

char *shexdump(uint8_t *buf, uint32_t bufLen);
void hexdump(uint8_t *buf, uint32_t bufLen);
void hexdump_debug(uint8_t *buf, uint32_t bufLen);
const char* int_to_binary( uint16_t x );

#endif
