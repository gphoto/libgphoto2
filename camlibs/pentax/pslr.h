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

#ifndef PSLR_H
#define PSLR_H

#include "pslr_enum.h"
#include "pslr_scsi.h"
#include "pslr_model.h"

#define PSLR_LIGHT_METER_AE_LOCK 0x8

typedef enum {
    PSLR_BUF_PEF,
    PSLR_BUF_DNG,
    PSLR_BUF_JPEG_MAX,
    PSLR_BUF_JPEG_MAX_M1,
    PSLR_BUF_JPEG_MAX_M2,
    PSLR_BUF_JPEG_MAX_M3,
    PSLR_BUF_PREVIEW = 8,
    PSLR_BUF_THUMBNAIL = 9 // 7 works also
} pslr_buffer_type;

typedef enum {
    USER_FILE_FORMAT_PEF,
    USER_FILE_FORMAT_DNG,
    USER_FILE_FORMAT_JPEG,
    USER_FILE_FORMAT_MAX
} user_file_format;

typedef struct {
    user_file_format uff;
    const char *file_format_name;
    const char *extension;
} user_file_format_t;

extern user_file_format_t file_formats[3];

user_file_format_t *get_file_format_t( user_file_format uff );

// OFF-AUTO: Off-Auto-Aperture
typedef enum {
    PSLR_EXPOSURE_MODE_P = 0,
    PSLR_EXPOSURE_MODE_GREEN = 1,
//    PSLR_EXPOSURE_MODE_HYP = 2,
//    PSLR_EXPOSURE_MODE_AUTO_PICT = 1,
//    PSLR_EXPOSURE_MODE_GREEN = 3, maybe 1 is AUTO_PICT
    PSLR_EXPOSURE_MODE_TV = 4,
    PSLR_EXPOSURE_MODE_AV = 5,
//    PSLR_EXPOSURE_MODE_TV_SHIFT = 6, //?
//    PSLR_EXPOSURE_MODE_AV_SHIFT = 7, //?
    PSLR_EXPOSURE_MODE_M = 8,
    PSLR_EXPOSURE_MODE_B = 9,
    PSLR_EXPOSURE_MODE_AV_OFFAUTO = 10,
    PSLR_EXPOSURE_MODE_M_OFFAUTO = 11,
    PSLR_EXPOSURE_MODE_B_OFFAUTO = 12,
    PSLR_EXPOSURE_MODE_TAV = 13, // ?
    PSLR_EXPOSURE_MODE_SV = 15,
    PSLR_EXPOSURE_MODE_X = 16, // ?
    PSLR_EXPOSURE_MODE_MAX = 17
} pslr_exposure_mode_t;

typedef enum {
    PSLR_GUI_EXPOSURE_MODE_GREEN,
    PSLR_GUI_EXPOSURE_MODE_P,
    PSLR_GUI_EXPOSURE_MODE_SV,
    PSLR_GUI_EXPOSURE_MODE_TV,
    PSLR_GUI_EXPOSURE_MODE_AV,
    PSLR_GUI_EXPOSURE_MODE_TAV,
    PSLR_GUI_EXPOSURE_MODE_M,
    PSLR_GUI_EXPOSURE_MODE_B,
    PSLR_GUI_EXPOSURE_MODE_X,
    PSLR_GUI_EXPOSURE_MODE_MAX
} pslr_gui_exposure_mode_t;

typedef void *pslr_handle_t;

typedef struct {
    uint32_t a;
    uint32_t b;
    uint32_t addr;
    uint32_t length;
} pslr_buffer_segment_info;

typedef void (*pslr_progress_callback_t)(uint32_t current, uint32_t total);

void sleep_sec(double sec);

pslr_handle_t pslr_init(char *model, char *device);
int pslr_connect(pslr_handle_t h);
int pslr_disconnect(pslr_handle_t h);
int pslr_shutdown(pslr_handle_t h);
const char *pslr_model(uint32_t id);

int pslr_shutter(pslr_handle_t h);
int pslr_focus(pslr_handle_t h);

int pslr_get_status(pslr_handle_t h, pslr_status *sbuf);
int pslr_get_status_buffer(pslr_handle_t h, uint8_t *st_buf);
int pslr_get_settings(pslr_handle_t h, pslr_settings *ps);
int pslr_get_settings_json(pslr_handle_t h, pslr_settings *ps);
int pslr_get_settings_buffer(pslr_handle_t h, uint8_t *st_buf);

char *collect_status_info( pslr_handle_t h, pslr_status status );
char *collect_settings_info( pslr_handle_t h, pslr_settings settings );

int pslr_get_buffer(pslr_handle_t h, int bufno, pslr_buffer_type type, int resolution,
                    uint8_t **pdata, uint32_t *pdatalen);

int pslr_set_progress_callback(pslr_handle_t h, pslr_progress_callback_t cb,
                               uintptr_t user_data);

int pslr_set_shutter(pslr_handle_t h, pslr_rational_t value);
int pslr_set_aperture(pslr_handle_t h, pslr_rational_t value);
int pslr_set_iso(pslr_handle_t h, uint32_t value, uint32_t auto_min_value, uint32_t auto_max_value);
int pslr_set_ec(pslr_handle_t h, pslr_rational_t value);

int pslr_set_white_balance(pslr_handle_t h, pslr_white_balance_mode_t wb_mode);
int pslr_set_white_balance_adjustment(pslr_handle_t h, pslr_white_balance_mode_t wb_mode, uint32_t wbadj_mg, uint32_t wbadj_ba);
int pslr_set_flash_mode(pslr_handle_t h, pslr_flash_mode_t value);
int pslr_set_flash_exposure_compensation(pslr_handle_t h, pslr_rational_t value);
int pslr_set_drive_mode(pslr_handle_t h, pslr_drive_mode_t drive_mode);
int pslr_set_af_mode(pslr_handle_t h, pslr_af_mode_t af_mode);
int pslr_set_af_point_sel(pslr_handle_t h, pslr_af_point_sel_t af_point_sel);
int pslr_set_ae_metering_mode(pslr_handle_t h, pslr_ae_metering_t ae_metering_mode);
int pslr_set_color_space(pslr_handle_t h, pslr_color_space_t color_space);

int pslr_set_jpeg_stars(pslr_handle_t h, int jpeg_stars);
int pslr_set_jpeg_resolution(pslr_handle_t h, int megapixel);
int pslr_set_jpeg_image_tone(pslr_handle_t h, pslr_jpeg_image_tone_t image_mode);

int pslr_set_jpeg_sharpness(pslr_handle_t h, int32_t sharpness);
int pslr_set_jpeg_contrast(pslr_handle_t h, int32_t contrast);
int pslr_set_jpeg_saturation(pslr_handle_t h, int32_t saturation);
int pslr_set_jpeg_hue(pslr_handle_t h, int32_t hue);

int pslr_set_image_format(pslr_handle_t h, pslr_image_format_t format);
int pslr_set_raw_format(pslr_handle_t h, pslr_raw_format_t format);
int pslr_set_user_file_format(pslr_handle_t h, user_file_format uff);
user_file_format get_user_file_format( pslr_status *st );

int pslr_delete_buffer(pslr_handle_t h, int bufno);

int pslr_green_button(pslr_handle_t h);

int pslr_button_test(pslr_handle_t h, int bno, int arg);

int pslr_ae_lock(pslr_handle_t h, bool lock);

int pslr_dust_removal(pslr_handle_t h);

int pslr_bulb(pslr_handle_t h, bool on );

int pslr_buffer_open(pslr_handle_t h, int bufno, pslr_buffer_type type, int resolution);
uint32_t pslr_buffer_read(pslr_handle_t h, uint8_t *buf, uint32_t size);
uint32_t pslr_fullmemory_read(pslr_handle_t h, uint8_t *buf, uint32_t offset, uint32_t size);
void pslr_buffer_close(pslr_handle_t h);
uint32_t pslr_buffer_get_size(pslr_handle_t h);

int pslr_set_exposure_mode(pslr_handle_t h, pslr_exposure_mode_t mode);
int pslr_select_af_point(pslr_handle_t h, uint32_t point);

const char *pslr_camera_name(pslr_handle_t h);
int pslr_get_model_max_jpeg_stars(pslr_handle_t h);
int pslr_get_model_jpeg_property_levels(pslr_handle_t h);
int pslr_get_model_status_buffer_size(pslr_handle_t h);
int pslr_get_model_fastest_shutter_speed(pslr_handle_t h);
int pslr_get_model_base_iso_min(pslr_handle_t h);
int pslr_get_model_base_iso_max(pslr_handle_t h);
int pslr_get_model_extended_iso_min(pslr_handle_t h);
int pslr_get_model_extended_iso_max(pslr_handle_t h);
int *pslr_get_model_jpeg_resolutions(pslr_handle_t h);
bool pslr_get_model_only_limited(pslr_handle_t h);
bool pslr_get_model_has_jpeg_hue(pslr_handle_t h);
bool pslr_get_model_need_exposure_conversion(pslr_handle_t h);
pslr_jpeg_image_tone_t pslr_get_model_max_supported_image_tone(pslr_handle_t h);
bool pslr_get_model_has_settings_parser(pslr_handle_t h);
int pslr_get_model_af_point_num(pslr_handle_t h);
bool pslr_get_model_old_bulb_mode(pslr_handle_t h);
bool pslr_get_model_bufmask_single(pslr_handle_t h);

pslr_buffer_type pslr_get_jpeg_buffer_type(pslr_handle_t h, int quality);
int pslr_get_jpeg_resolution(pslr_handle_t h, int hwres);

int pslr_read_datetime(pslr_handle_t *h, int *year, int *month, int *day, int *hour, int *min, int *sec);

int pslr_read_dspinfo(pslr_handle_t *h, char *firmware);

int pslr_read_setting(pslr_handle_t *h, int offset, uint32_t *value);
int pslr_write_setting(pslr_handle_t *h, int offset, uint32_t value);
int pslr_write_setting_by_name(pslr_handle_t *h, char *name, uint32_t value);
bool pslr_has_setting_by_name(pslr_handle_t *h, char *name);
int pslr_read_settings(pslr_handle_t *h);

pslr_gui_exposure_mode_t exposure_mode_conversion( pslr_exposure_mode_t exp );
char *format_rational( pslr_rational_t rational, char * fmt );

int pslr_test( pslr_handle_t h, bool cmd9_wrap, int subcommand, int argnum,  int arg1, int arg2, int arg3, int arg4);

char *copyright(void);

void write_debug( const char* message, ... );

int debug_onoff(ipslr_handle_t *p, char debug_mode);
#endif
